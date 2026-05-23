# Sashiko 源码架构分析

> 基于 commit: 当前 HEAD，v0.1.8
> 生成时间: 2026-05-23

---

## 1. Sashiko 是什么？

**Sashiko**（刺し子，日语意为"小针脚"）是一个**面向 Linux 内核的自动化、智能化代码审查系统**。名称隐喻来自日本传统的刺绣工艺——用细密的针脚加固布料——这里指用 AI 对内核补丁进行细密、多角度的审查以加固代码质量。

### 核心信息

| 属性     | 值                                     |
| -------- | -------------------------------------- |
| 项目归属 | Linux Foundation                       |
| 语言     | Rust (edition 2024)                    |
| Web 框架 | Axum 0.8                               |
| 数据库   | libSQL / SQLite                        |
| 协议     | Apache 2.0                             |
| 仓库     | https://github.com/sashiko-dev/sashiko |

### 关键特点

- **完全自包含**：不依赖任何外部 agent 工具（如 Claude Code CLI 等），自带完整的 AI 对话循环
- **多 LLM 支持**：Gemini、Claude（含 prompt caching）、OpenAI-compatible、AWS Bedrock、GCP Vertex、以及多种外部 CLI 包装器
- **Linux 内核专精**：审查提示词（prompts）专门针对内核开发场景设计，包含子系统指南、模式文档等
- **10 阶段审查协议**：模拟一个由不同专长 reviewer 组成的团队，分阶段交叉审查
- **自动摄入**：通过 NNTP 协议从 `lore.kernel.org` 监听邮件列表，自动提取补丁
- **多方式输入**：邮件列表、本地 git、HTTP API 均可提交补丁
- **基准测试**：能发现 53.6% 的上游合入后才发现 bug 的补丁（Gemini 3.1 Pro）

---

## 2. 目录结构与文件解释

### 项目根目录

```
sashiko/
├── Cargo.toml                    # Rust 项目配置，依赖管理
├── Cargo.lock                    # 依赖锁定
├── Settings.toml                 # 默认配置文件
├── email_policy.toml             # 邮件发送策略配置
├── Makefile                      # 构建辅助
├── Dockerfile                    # Docker 构建
├── flake.nix / flake.lock        # Nix 包管理
├── rust-toolchain.toml           # Rust 工具链版本
│
├── src/                          # 主要源代码
├── third_party/                  # 第三方依赖
├── tests/                        # 集成测试
├── benchmarks/                   # 基准测试
├── docs/                         # 文档
├── designs/                      # 设计文档
├── scripts/                      # 辅助脚本
├── static/                       # Web UI 静态资源
├── skills/                       # Gemini CLI skills
└── sashiko.dev/                  # 项目网站源码
```

### `src/` 源代码目录详解

#### 二进制入口

| 文件                   | 说明                                                                                                                                           |
| ---------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------- |
| `main.rs`            | **守护进程入口**。解析 CLI 参数，初始化所有子系统（数据库、摄入器、审查器、API 服务器、邮件 worker），编排事件管道。是整个系统的"大脑"。 |
| `bin/review.rs`      | **审查子进程入口**。由守护进程 spawn，通过 stdin/stdout JSON 协议与父进程通信。负责实际的 AI 对话循环。                                  |
| `bin/sashiko-cli.rs` | **CLI 工具**。用于向运行中的守护进程提交补丁或执行本地审查。                                                                             |
| `bin/benchmark.rs`   | **基准测试工具**。评估审查质量。                                                                                                         |

#### 核心模块

| 文件                | 行数  | 职责                                                                                                                                                                                                                      |
| ------------------- | ----- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `lib.rs`          | ~110  | 模块声明 +`ReviewStatus` 枚举定义（Incomplete/Pending/InReview/Reviewed/Failed 等）                                                                                                                                     |
| `reviewer.rs`     | ~3170 | **审查引擎的核心**。`Reviewer` 结构体：后台循环调度 → baseline 解析 → git worktree 创建 → patch 应用 → spawn review binary → 结果处理 → 邮件通知。包含完整的并发控制（semaphore）、超时管理、token 预算等。 |
| `settings.rs`     | —    | 配置加载。支持 TOML 文件和环境变量覆盖（`SASHIKO_*` prefix）。                                                                                                                                                          |
| `db.rs`           | —    | **数据库层**。使用 libSQL（SQLite 兼容），包含 patchset、patch、review、finding、email_outbox 等全部表结构定义和 CRUD 操作。                                                                                        |
| `events.rs`       | —    | 事件类型定义（Event 枚举）：ArticleFetched、PatchSubmitted、RawMboxSubmitted、IngestionFailed 等。                                                                                                                        |
| `ingestor.rs`     | —    | **补丁摄入器**。通过 NNTP 协议连接 `lore.kernel.org`，下载邮件列表中的新补丁。支持跟踪模式和一次性下载模式。                                                                                                      |
| `fetcher.rs`      | —    | **Git Fetch 管理器**。管理自定义 remote 的 git fetch 操作，支持 webhook 触发。                                                                                                                                      |
| `nntp.rs`         | —    | **NNTP 客户端实现**。连接新闻组服务器（lore.kernel.org 使用 nntp 协议暴露邮件列表）。                                                                                                                               |
| `patch.rs`        | —    | **邮件/补丁解析器**。将原始 mbox 格式的邮件解析为结构化的 Patch + PatchsetMetadata。支持 `From:`、`Subject:`、`Date:`、`In-Reply-To:`、`diff --git` 等字段提取。                                          |
| `git_ops.rs`      | —    | **Git 操作封装**。`GitWorktree` 结构体：创建/删除 worktree、`git am` 应用补丁、`git apply` 回退、commit hash 解析等。                                                                                         |
| `baseline.rs`     | —    | **Baseline 解析器**。解析内核 MAINTAINERS 文件，根据补丁修改的文件找到对应的子系统 git 树，确定补丁应该基于哪个提交进行审查。`BaselineResolution` 枚举支持 Commit/LocalRef/RemoteTarget 三种基线。                |
| `api.rs`          | —    | **Web API 服务器**（Axum）。提供 RESTful API + Web UI 静态文件服务。支持补丁提交、状态查询、审查结果查看等。                                                                                                        |
| `email_router.rs` | —    | **邮件路由策略**。根据 email_policy.toml 配置决定审查结果是否发送、发送给谁。                                                                                                                                       |
| `email_policy.rs` | —    | 邮件策略配置加载。                                                                                                                                                                                                        |
| `patchwork.rs`    | —    | **Patchwork CI 集成**。将审查结果通过 API 发布到 Patchwork 实例，显示 CI check 状态。                                                                                                                               |
| `metrics.rs`      | —    | Prometheus 指标收集。跟踪 pending/reviewing patches、消息数、patchset 数。                                                                                                                                                |
| `inspector.rs`    | —    | 数据库检查工具。                                                                                                                                                                                                          |
| `logging.rs`      | —    | 日志配置（`IgnoreBrokenPipe` wrapper 等）。                                                                                                                                                                             |
| `utils.rs`        | —    | 工具函数。JSON 清理、secret 脱敏等。                                                                                                                                                                                      |

#### AI 提供者层 (`src/ai/`)

| 文件                | 说明                                                                                                                                             |
| ------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------ |
| `mod.rs`          | **核心抽象**：`AiProvider` trait 定义 + `AiRequest`/`AiResponse` 数据结构 + 工厂函数 `create_provider()`。所有 AI 后端的统一接口。 |
| `claude.rs`       | Anthropic Claude API 实现，支持 prompt caching（`/v1/messages` API）                                                                           |
| `gemini.rs`       | Google Gemini API 实现 + StdioGeminiClient（子进程通信模式）                                                                                     |
| `openai.rs`       | OpenAI-compatible API 实现（支持自定义 base_url，可对接 Ollama 等）                                                                              |
| `bedrock.rs`      | AWS Bedrock 实现                                                                                                                                 |
| `vertex.rs`       | GCP Vertex AI 实现                                                                                                                               |
| `cache.rs`        | **响应缓存**：用 SQLite 缓存 AI 响应，`CachingAiProvider` 装饰器模式                                                                     |
| `quota.rs`        | **速率限制管理**：追踪请求频率，在 rate limit 时自动等待                                                                                   |
| `claude_cli.rs`   | Claude Code CLI 包装器                                                                                                                           |
| `codex_cli.rs`    | Codex CLI 包装器                                                                                                                                 |
| `copilot_cli.rs`  | GitHub Copilot CLI 包装器                                                                                                                        |
| `kiro_cli.rs`     | Kiro CLI 包装器                                                                                                                                  |
| `proxy.rs`        | 代理转发                                                                                                                                         |
| `token_budget.rs` | Token 预算计算                                                                                                                                   |
| `truncator.rs`    | **输出截断**：智能截断大 diff 和代码块，保留关键内容                                                                                       |

#### 审查工作单元 (`src/worker/`)

| 文件              | 行数  | 说明                                                                                                                                                                                                                                                                              |
| ----------------- | ----- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `prompts.rs`    | ~1750 | **审查提示词系统 + Worker 执行器**。包含完整的 10 阶段审查提示词、`PromptRegistry`（读取和管理第三方提示词文件）、`Worker` 结构体（管理 AI 会话循环、工具调用、多阶段执行）。这是代码审查的"灵魂"。                                                                     |
| `tools.rs`      | ~1140 | **AI 可用工具集**（`ToolBox`）。13 个工具：read_files（smart/raw 模式）、git_blame/git_diff/git_show/git_log/git_status/git_checkout/git_branch/git_tag、search_file_content（基于 grep）、list_dir、find_files、TodoWrite、read_prompt。含白名单参数校验和路径遍历防护。 |
| `prefetch.rs`   | —    | **上下文预取**。基于 diff 中修改的行，用 tree-sitter 解析 C 代码 AST，自动提取被修改函数/结构体的完整源代码，减少 AI 交互轮数。                                                                                                                                             |
| `email.rs`      | —    | **邮件发送 worker**。从 email_outbox 表读取待发送邮件，通过 SMTP 发送。                                                                                                                                                                                                     |
| `tools_test.rs` | —    | ToolBox 的测试。                                                                                                                                                                                                                                                                  |

### `third_party/` 目录

```
third_party/
├── linux/                         # Linux 内核源码子模块（用于 local ref baseline 解析）
└── prompts/
    ├── kernel/                    # Linux 内核审查提示词（核心内容）
    │   ├── callstack.md           # 调用栈分析指南
    │   ├── technical-patterns.md  # 内核技术模式文档
    │   ├── false-positive-guide.md # 假阳性过滤指南
    │   ├── severity.md            # 严重性分级指南
    │   ├── inline-template.md     # 内联回复模板
    │   ├── locking.md             # 锁机制指南
    │   ├── subsystem/             # 子系统指南
    │   │   ├── mm.md, bpf.md, net.md, locking.md, ...
    │   └── patterns/              # 具体模式文档
    ├── systemd/                   # systemd 审查提示词
    └── iproute/                   # iproute2 审查提示词
```

---

## 3. 架构框架

### 3.1 总体架构

```
┌──────────────────────────────────────────────────────────────────────┐
│                        sashiko Daemon (main.rs)                      │
│                                                                      │
│  ┌────────────────┐     ┌────────────────┐     ┌─────────────────┐  │
│  │   Ingestor     │────▶│  Parser        │────▶│   DB Worker     │  │
│  │  (NNTP/Lore)   │     │  Dispatcher    │     │  (batch insert) │  │
│  └────────────────┘     └────────────────┘     └─────────────────┘  │
│         │                                                           │
│         │ raw_tx (mpsc channel, buffer 1000)                        │
│         ▼                                                           │
│  ┌────────────────┐                                                 │
│  │  FetchAgent    │── git fetch ──▶ Event::ArticleFetched            │
│  │  (git remote)  │                                                 │
│  └────────────────┘                                                 │
│                                                                      │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐              │
│  │  API Server  │  │  Reviewer    │  │  Email       │              │
│  │  (Axum)      │  │  (后台循环)  │  │  Worker      │              │
│  └──────────────┘  └──────┬───────┘  │  (SMTP)      │              │
│                           │           └──────────────┘              │
│                           ▼                                         │
│                    ┌──────────────┐                                 │
│                    │  review bin  │  ◀── spawn + stdio JSON 协议    │
│                    └──────┬───────┘                                 │
│                           │                                         │
│                    ┌──────▼───────┐                                 │
│                    │  Worker      │  AI 对话循环 + 10 阶段审查      │
│                    │  (prompts)   │  ToolBox 工具调用                │
│                    │  + ToolBox   │  ←→ AI Provider (Gemini/Claude) │
│                    └──────────────┘                                 │
└──────────────────────────────────────────────────────────────────────┘
```

### 3.2 事件管道（三阶段处理）

```
raw_tx (mpsc)          parsed_tx (mpsc)         DB (batch)
┌─────────┐    ┌──────────────────┐    ┌────────────────────┐
│ Event:  │───▶│ ParsedArticle    │───▶│ create_message     │
│ - NNTP  │    │                  │    │ create_patchset    │
│ - API   │    │ (解析后的结构化   │    │ create_patch       │
│ - Local │    │  文章+补丁数据)   │    │ link subsystems    │
│ - Fetch │    │                  │    │ link recipients    │
└─────────┘    └──────────────────┘    └────────────────────┘
```

关键设计：

- **异步解耦**：两个 `mpsc::channel(1000)` 将摄入、解析、存储三个阶段解耦
- **并发控制**：Parser Dispatcher 用 `tokio::sync::Semaphore(50)` 限制同时解析数量
- **批量写入**：DB Worker 用 `recv_many(&mut buffer, 100)` 批量入库，提高吞吐量
- **摄入截止**：通过 `cutoff_timestamp` 避免重启后重复摄入旧邮件
- **启动恢复**：`db.reset_reviewing_status()` 将中断的 InReview 重置为 Pending

### 3.3 审查调度器（Reviewer Loop）

```rust
// 伪代码：reviewer.rs 审查循环
loop {
    // 1. 取待审查的 patchset
    let patchsets = db.get_pending_patchsets(10);
    for patchset in patchsets {
        // 2. 每个 patchset 一个异步任务
        tokio::spawn(review_patchset_task(ctx, patchset));
    }

    // 3. 释放到期的 embargo
    release_embargoed_results();

    // 4. 每 10 秒轮询
    sleep(10s);
}
```

单个 patchset 审查生命周期（`review_patchset_task`）：

```
1. 更新状态: Pending → InReview
2. 从 DB 读取所有 patch 的 diff
3. 识别补丁修改的文件
4. BaselineRegistry 解析候选 baseline
5. 对每个候选 baseline:
   a. 创建 git worktree
   b. 尝试 git am / git apply / checkout commit
   c. 找到可用的 baseline 后 break
6. 跳过超过大小限制的 patch（max_lines_changed, max_files_touched）
7. 对有效 patch 进行审查:
   a. 对于 < 10 个 patch: 串行处理（复用 worktree）
   b. 对于 >= 10 个 patch: 自动 try_acquire 额外 semaphore permit 并行处理
8. 每个 patch 的处理:
   a. 创建 review 记录
   b. 启动 review 子进程
   c. 发送 JSON payload
   d. 循环处理 ai_request/ai_response 直到拿到最终结果
   e. 记录 findings、token 用量、tool usage
   f. 队列化邮件通知
9. 清理 worktree
10. 更新最终状态: Reviewed / Failed / FailedToApply
```

### 3.4 通信协议（Stdio JSON 协议）

Daemon 与 review binary 之间通过 stdin/stdout 的 JSON 行协议通信：

```
Daemon (main process)               Review Binary (child process)
    │                                       │
    │── stdin: JSON(patchset payload) ─────▶│
    │                                       │
    │                                       │── 分析补丁
    │                                       │── 需要 AI 时:
    │◀── stdout: {"type":"ai_request",      │
    │              "payload": AiRequest}────│
    │                                       │
    │  ┌── 调用实际 AI Provider ──┐         │
    │  │ Gemini / Claude / ...    │         │
    │  └──────────────────────────┘         │
    │                                       │
    │── stdin: {"type":"ai_response",       │
    │            "payload": AiResponse}────▶│
    │                                       │
    │         ... 多轮工具调用对话 ...       │
    │                                       │
    │◀── stdout: {"patchset_id":...,        │
    │              "patches":[...],          │
    │              "review":{...},           │
    │              "findings":[...]}─────────│
```

这个设计的巧妙之处：Daemon 作为"AI 代理中间人"——review binary 不需要直接知道任何 API key 或 provider 详情，它只管发出 `ai_request`，daemon 拦截后调用实际的 provider 并返回 `ai_response`。

### 3.5 AI 提供者抽象层

```rust
#[async_trait]
pub trait AiProvider: Send + Sync {
    async fn generate_content(&self, request: AiRequest) -> Result<AiResponse>;
    fn estimate_tokens(&self, request: &AiRequest) -> usize;
    fn get_capabilities(&self) -> ProviderCapabilities;
}
```

目前支持 10+ 种实现，均可通过配置文件切换：

| 配置 provider                       | 后端                   | 特殊能力                 |
| ----------------------------------- | ---------------------- | ------------------------ |
| `gemini`                          | Google Gemini API      | 原生实现                 |
| `claude`                          | Anthropic Messages API | **prompt caching** |
| `openai` / `openai-compatible`  | OpenAI / 兼容 API      | 可对接 Ollama 等         |
| `bedrock`                         | AWS Bedrock            | **prompt caching** |
| `vertex`                          | GCP Vertex AI          | **prompt caching** |
| `stdio-gemini` / `stdio-claude` | 子进程通信模式         | review binary 专用       |
| `claude-cli`                      | Claude Code CLI        | 包装外部 CLI             |
| `codex-cli`                       | Codex CLI              | 包装外部 CLI             |
| `copilot-cli`                     | GitHub Copilot CLI     | 包装外部 CLI             |
| `kiro-cli`                        | Kiro CLI               | 包装外部 CLI             |

★ *响应缓存装饰器*：`create_provider_cached()` 用 `CachingAiProvider` 包装任何 provider，用 SQLite 缓存 AI 响应，`cache_ttl_days` 控制缓存有效期。

### 3.6 工具系统（ToolBox）

AI 在审查过程中可以调用的工具，封装在 `ToolBox` 中：

```
┌─────────────────────────────────────────────────────┐
│                    ToolBox                           │
├─────────────────────────────────────────────────────┤
│  1. read_files(path, start_line, end_line, mode)     │
│     - raw mode: 返回原始内容                         │
│     - smart mode: 折叠无关代码，保留焦点区域           │
│  2. search_file_content(pattern, path, context_lines) │
│     - 基于 grep crate 的全文搜索                      │
│  3-10. git_blame / git_diff / git_show / git_log     │
│         git_status / git_checkout / git_branch        │
│         git_tag                                      │
│  11. list_dir(path)                                   │
│  12. find_files(pattern, path)                        │
│  13. TodoWrite(content)                               │
│  14. read_prompt(name) [条件启用]                      │
├─────────────────────────────────────────────────────┤
│                    安全机制                            │
│  • validate_path(): 路径遍历防护                       │
│    → canonicalize() 解析符号链接后检查是否在 worktree 内 │
│  • validate_git_args(): git 参数白名单                 │
│    → 拒绝 --output=malicious.txt 等危险参数            │
│  • 输出截断: Truncator 限制最大输出 10K tokens         │
└─────────────────────────────────────────────────────┘
```

### 3.7 Baseline 解析系统

```rust
enum BaselineResolution {
    Commit(String),       // 明确的 base-commit hash
    LocalRef(String),     // 如 "net-next/master" 或 "HEAD"
    RemoteTarget { url, name, branch }, // 如 torvalds/linux.git
}
```

`BaselineRegistry` 的工作流程：

1. 从补丁 diff 中提取修改的文件名
2. 解析内核 MAINTAINERS 文件，找到每个文件对应的子系统
3. 从子系统的 `T:` 条目中找到 git 树 URL
4. 创建候选 baseline 列表（按优先级排序）
5. 对每个候选尝试创建 worktree 并应用补丁
6. 返回第一个可用的 baseline

### 3.8 数据库架构（libSQL）

主要表结构：

```
messages          # 邮件消息（完整的邮件头 + 正文）
threads           # 邮件线程（patch series 分组）
patchsets         # 补丁集（对应一个 patch series）
patches           # 单个补丁（diff 内容）
reviews           # 审查记录（关联 provider、model、status）
findings          # 审查发现的问题（severity、preexisting 标记）
ai_interactions   # AI 请求/响应日志
tool_usage        # AI 工具调用记录
email_outbox      # 待发送的审查邮件队列
mailing_lists     # 邮件列表
subsystems        # 子系统
persons           # 人员
message_recipients # 消息收件人
```

---

## 4. 10 阶段审查协议（核心）

这是 Sashiko 最有特色的设计。它不是让一个 LLM"看一眼"补丁然后给出意见，而是**模拟一个由不同专长的 reviewer 组成的团队**，分阶段、多角度系统性地审查补丁。

### 4.1 整体流程

```
Patch Input
    │
    ▼
┌──────────────────────────────────────────────────────────────────────┐
│  Phase 0: Pre-screening（预筛选）                                     │
│  AI 根据补丁内容从 subsystem/ 中选择相关的子系统指南                    │
│  输出: ["mm.md", "locking.md", ...]                                 │
└──────────────────────────────────────────────────────────────────────┘
    │
    ▼
┌──────────────────────────────────────────────────────────────────────┐
│  Planning Phase（规划阶段）                                           │
│  AI 决定 Stages 4-7 中哪些需要运行                                    │
│  Stages 1-3 总是运行                                                  │
│  输出: [4, 5, 7]  （跳过 Stage 6 安全审计如果不相关）                  │
└──────────────────────────────────────────────────────────────────────┘
    │
    ▼
┌──────────────────────────────────────────────────────────────────────┐
│  Stages 1-7: 多点并行深入审查                                         │
│                                                                      │
│  Stage 1  Stage 2  Stage 3  Stage 4  Stage 5  Stage 6  Stage 7       │
│  架构审查  实现验证  执行流    资源管理  锁与同步  安全审计  硬件审查    │
│                                                                      │
│  每个 Stage:                                                         │
│  1. AI 收到对应的角色扮演 prompt                                      │
│  2. AI 可以通过 ToolBox 工具深入探索代码                                │
│  3. AI 输出 JSON 格式的 concerns[] 数组                               │
│  4. 最多 3 次重试（内层）+ 3 次外层重试                                │
│  5. 如果所有 concerns 为空 → 跳过后续阶段                              │
└──────────────────────────────────────────────────────────────────────┘
    │
    ▼
┌──────────────────────────────────────────────────────────────────────┐
│  Stage 8: Deduplication（去重与合并）                                 │
│  合并所有 stages 的重叠 concerns                                       │
│  输出: 去重后的 concerns[]                                            │
└──────────────────────────────────────────────────────────────────────┘
    │
    ▼
┌──────────────────────────────────────────────────────────────────────┐
│  Stage 9: Verification（验证与严重性评估）                             │
│  对每个 concern 进行验证                                              │
│  → 找到确凿证据证明是假阳性 → 丢弃                                    │
│  → 无法找到反证 → 确认为 finding                                      │
│  分配 severity: Low / Medium / High / Critical                        │
│  标记 preexisting: 补丁引入的问题 vs 代码库原有的问题                   │
│  输出: findings[] (含 severity 和 severity_explanation)               │
└──────────────────────────────────────────────────────────────────────┘
    │
    ▼
┌──────────────────────────────────────────────────────────────────────┐
│  Stage 10: Report Generation（LKML 报告生成）                         │
│  将 findings 转化为标准的 LKML 内联回复格式                             │
│  格式校验：必须有 commit header、author、> 引用、注释内容               │
│  如果格式校验失败 → 最多 3 次重试，附带格式错误提示                     │
│  如果 recitation 错误 → 切换到自由模式                                 │
│  输出: 格式化的 LKML 邮件回复文本                                     │
└──────────────────────────────────────────────────────────────────────┘
    │
    ▼
Review Result (findings + inline_review + tokens_used)
```

### 4.2 Phase 0: 预筛选

**目的**：从大量子系统指南中选出与当前补丁相关的部分，避免把所有指南都塞进 context。

```
输入: subsystem/ 目录下所有 .md 文件
任务: AI 判断每个指南是否与当前补丁相关
规则: 偏向包含——只有 100% 无关才排除
输出: ["mm.md", "locking.md", ...]
```

这些选中的提示词文件随后被加载到 system prompt 中，作为全局知识库的一部分。

### 4.3 Planning Phase: 规划阶段

**目的**：让 AI 自行判断哪些深度审查阶段需要运行，避免在不相关的方向上浪费 token。

```
输入: 补丁 diff + 全局上下文
任务: 判断 Stages 4-7 是否相关
规则: 倾向于运行更多 stage——"如果你不确定，就包含"
Stages 1-3 总是运行（架构、实现验证、执行流）
输出: [4, 5, 6, 7] 的子集
```

### 4.4 Stages 1-7: 多点深入审查

每个 stage 的核心结构相同但 prompt 不同：

```javascript
// 每个 stage 的 JSON 输出格式
{
  "concerns": [
    {
      "type": "Memory Leak",
      "description": "在函数 X 中，当条件 Y 满足时可能发生内存泄漏",
      "reasoning": "1. kmalloc() 分配了缓冲区\n2. 在错误路径上 goto cleanup\n3. cleanup 标签处没有 kfree()",
      "preexisting": false,  // 是否补丁引入前就存在的问题
      "locations": [
        {
          "file": "drivers/net/ethernet/xxx.c",
          "function/symbol": "xxx_probe",
          "code_snippet": "if (ret < 0)\n    goto err_free;"
        }
      ]
    }
  ]
}
```

#### Stage 1: 分析 commit 主要目标

> 角色：**资深内核维护者**评估 commit 的高层意图

- 架构缺陷
- UAPI 破坏
- 向后兼容性
- 长期可维护性
- 基本概念的合理性

#### Stage 2: 高级实现验证

> 角色：**验证者**检查代码是否实现了 commit message 声称的内容

- 代码是否真的做了 commit message 说的那些事？
- 缺少的调用者更新
- 修改 struct 时更新所有初始化器
- API 契约违反
- 数学/语义正确性（off-by-one、位运算、format specifier）
- "不要相信 commit message——假设它可能不正确甚至恶意"

#### Stage 3: 执行流验证

> 角色：**静态分析引擎**逐行跟踪执行流

- 逻辑错误、循环条件错误
- 未处理的错误路径
- 缺失的返回值检查
- NULL 指针解引用（注意：读取指针字段不是解引用）
- goto cleanup 路径的正确性
- 预处理器宏正确性（`CONFIG_` 前缀等）
- `static`/`inline` 声明可能导致的 LTO 符号丢失

#### Stage 4: 资源管理

> 角色：**C/Rust 资源管理专家**

- 内存泄漏
- Use-After-Free
- double free
- 未初始化变量
- 生命周期不匹配（alloc → init → use → cleanup → free）
- 引用计数逻辑（`kref_get()`/`kref_put()`）
- **异步交接和销毁对称性**：如果对象交给了后台任务（timer、workqueue、notifier），必须证明在释放前已显式 cancel

#### Stage 5: 锁与同步

> 角色：**世界级并发与锁专家**

- **原子上下文睡眠**：持有 spinlock 时调用 `mutex_lock`、`kzalloc(GFP_KERNEL)`、`msleep` 等
- **锁顺序和死锁**：AB-BA deadlock、IRQ disable 顺序
- **竞态条件和无锁访问**：共享变量未加锁访问、缺失 memory barrier
- **锁释放后使用**：在对象已释放后调用 `mutex_unlock`
- **RCU 规则**：`list_splice_init` 用在 RCU 链表上？
- **未保护的状态修改**：在加锁前检查状态？
- **序列计数器**：在 `u64_stats_fetch_retry` 内做累加导致重复计数？
- **锁重新初始化**
- **缺失的锁保护**：端口/文件暴露给用户空间时链接是否完成？

#### Stage 6: 安全审计

> 角色：**红队安全研究员**

- 缓冲区溢出
- 越界读写
- 整数溢出
- 权限提升
- TOCTOU 竞态
- 信息泄漏（将未初始化的内核内存 `copy_to_user`）
- 所有不受信任的用户输入到达敏感函数的路径

#### Stage 7: 硬件工程师审查

> 角色：**硬件工程师**审查驱动代码

- 寄存器访问正确性
- IRQ 处理
- DMA mapping/unmapping
- memory barrier（`dma_wmb()`、`dma_rmb()`）
- 大小端转换（`cpu_to_le32`）
- 硬件状态机正确性（suspend/resume、reset）
- 时钟/电源域在寄存器访问前已使能
- 如果 patch 是纯软件逻辑 → 输出空 concerns

### 4.5 Stage 8: 去重与合并

**目的**：不同 stage 可能发现同一个问题（如 Stage 3 和 Stage 4 都发现某个 NULL 解引用），需要合并。

```
输入: 多个 stages 的 concerns[] 合并后的数组
任务: 
  - 将引用相同根因或同一行代码的 concern 分组
  - 将重叠的 concern 合并为一个综合 concern
  - 保留 preexisting 标记
输出: 去重后的 concerns[]
```

### 4.6 Stage 9: 验证与严重性评估

**目的**：过滤假阳性，为每个 finding 分配严重性等级。

```
输入: 去重后的 concerns[]
规则:
  - 要丢弃 concern，必须找到确凿证据证明它无效
  - 如果找不到反证 → 必须报告
  - 检查后续 patch 是否修复了此问题
  - 引用同系列其他 patch 时用 subject 而非 hash
  - 低/中级严重性的 preexisting 问题 → 丢弃
  - 高/严重级 preexisting 问题 → 保留
输出: 
  - findings[] 每个包含:
    - problem: 问题描述
    - severity: Low / Medium / High / Critical
    - severity_explanation: 推理过程
    - preexisting: boolean
```

### 4.7 Stage 10: LKML 报告生成

**目的**：将结构化的 findings 转化为标准的 LKML 内联回复格式。

```
输入: findings[]
格式要求:
  - 必须以 commit 信息开头（Commit hash、Author、Subject）
  - 使用 > 引用源代码
  - 不使用 markdown 代码块（```
  - 语气礼貌、专业、建设性
  - preexisting 的 issue 需明确说明
格式校验:
  - 不能包含 markdown 代码块
  - 必须包含 > 引用
  - 前 20 行必须有 commit header
  - 前 20 行必须有 Author:
  - 必须有注释/总结文字
重试机制:
  - 格式校验失败 → 最多 3 次重试，附带具体错误提示
  - Recitation 错误 → 切换到自由模式
输出: 格式化 LKML 邮件回复文本
```

### 4.8 执行细节

#### 重试与容错

每个 stage 有两层重试机制：

```
外层尝试 (max 3):
  └── 内层尝试 (max 3):
       ├── run_ai_stage()
       │   ├── OK 且含 valid concerns → 成功
       │   ├── OK 但缺少 concerns 数组 → 增强 prompt 重试
       │   └── Error
       │       ├── ReviewError (LimitExceeded/BudgetExceeded/FormatRejection)
       │       │   → 不可重试，立即失败
       │       └── 其他错误 → 重试
       └── 全部失败 → 外层重试
```

关键设计：`ReviewError` 类型（LimitExceeded、BudgetExceeded、FormatRejection）被分类为 `AiErrorClass::Fatal`，不可重试——重试只会浪费 token。

#### Token 预算控制

```rust
pub struct WorkerConfig {
    max_input_tokens: usize,     // 单次请求最大输入
    max_interactions: usize,     // 每阶段最大交互轮数
    temperature: f32,            // AI 温度参数
}
```

在 `run_review_tool()` 中还有两层 token 保护：

- `max_total_tokens`：累计 uncached input + output 上限
- `max_total_output_tokens`：累计 output token 上限
- 超过任一上限 → `ReviewError::BudgetExceeded` → 立即终止

#### 预取上下文优化

在 AI 审查开始前，`prefetch::prefetch_context()` 基于 diff 的修改行，用 **tree-sitter** 解析 C 代码 AST，自动提取：

- 被修改函数的完整源代码
- 被修改结构体的完整定义

这些预取上下文作为 `<pre_fetched_context>` 标签插入 prompt。AI 可以直接看到相关代码的完整上下文，无需通过工具读取文件——这显著减少了交互轮数。

---

## 5. 关键设计决策与洞见

### 5.1 为什么用子进程通信而非直接调用 LLM？

daemon 不直接调用 AI provider 而是 spawn 子进程通过 stdio 协议通信的原因：

1. **隔离性**：review binary 崩溃不影响 daemon 稳定性
2. **职责分明**：daemon 管调度/存储/通知，review binary 管 AI 对话
3. **代理模式**：daemon 作为中间人，可以拦截 `ai_request`/`ai_response` 做额外处理（如记录 tool usage、check token budget）
4. **灵活性**：review binary 可以本地测试、独立运行

### 5.2 10 阶段协议的设计哲学

传统 prompt：一次性告诉 AI"审查这个补丁，找出 bug"——AI 往往给出泛泛而谈的反馈。

Sashiko 的方法：

1. **角色扮演**：每个 stage 扮演不同专家角色，激活不同的知识维度
2. **关注点分离**：先看架构再看细节，先找问题再验证
3. **交叉验证**：Stage 9 独立验证 Stage 1-7 的产出，减少幻觉和假阳性
4. **主动推理**：AI 不是被动接收信息，而是通过工具主动探索代码
5. **渐进式深度**：从宽泛到具体，从怀疑到验证

### 5.3 并发与性能

- daemon 内部大量使用 `tokio::spawn` 实现异步并发
- `Semaphore` 控制并发审查数量（默认配置）
- 大型 patchset（≥10 patches）自动启用并行审查
- `recv_many` 批量写入数据库
- worktree 复用（主 worker 串行处理，避免重复创建 worktree）

### 5.4 安全设计

- **路径遍历防护**：`validate_path()` 用 `canonicalize()` 解析符号链接后再检查
- **Git 参数白名单**：`validate_git_args()` 只允许白名单参数
- **输出截断**：`Truncator` 限制工具返回的输出大小，防止 AI 被大输出干扰
- **Env 清理**：spawn review binary 时 `env_clear()` 再选择性恢复关键环境变量
- **Embargo 机制**：敏感子系统的审查结果可延迟发布

---

`★ Insight ─────────────────────────────────────`

Sashiko 最令人印象深刻的不是它的 AI 能力本身，而是它**将软件工程方法论系统性地编码进了架构**：事件驱动管道解耦了摄入/解析/存储/审查/通知；10 阶段审查协议将"代码审查"这个人类活动拆解为可被 LLM 可靠执行的原子步骤；Stdio 协议实现了 daemon-review binary 的职责分离；ToolBox 的安全设计体现了对 LLM 作为潜在攻击面的清醒认知。这些工程决策使得一个基于概率性 LLM 的系统能够产生**可复现、可审计、高度专业化**的代码审查结果。

`─────────────────────────────────────────────────`
