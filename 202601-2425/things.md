## 1 BUG & BUG_ON

内核注释版：https://github.com/torvalds/linux/blob/master/include/asm-generic/bug.h#L62

```c
/*
 * Don't use BUG() or BUG_ON() unless there's really no way out; one
 * example might be detecting data structure corruption in the middle
 * of an operation that can't be backed out of.  If the (sub)system
 * can somehow continue operating, perhaps with reduced functionality,
 * it's probably not BUG-worthy.
 *
 * If you're tempted to BUG(), think again:  is completely giving up
 * really the *only* solution?  There are usually better options, where
 * users don't need to reboot ASAP and can mostly shut down cleanly.
 */
```

翻译版：

```c
/*
 * 除非真的已经没有任何退路，否则不要使用 BUG() 或 BUG_ON()；一个例子是：
 * 在一次无法回滚的操作过程中，检测到数据结构已经被破坏。
 * 如果这个（子）系统还能以某种方式继续运行，
 * 即使只能以功能受限的方式运行，那它大概就不值得用 BUG。
 *
 * 如果你正打算使用 BUG()，请再想一想：
 * “彻底放弃”真的就是唯一的解决办法吗？
 * 通常都会有更好的选择——让用户不必立刻重启，
 * 而是还能大体上有序、干净地关闭系统。
 */

```

LLM理解版：

> 这段话本质上是在给内核开发者“踩刹车”，劝你别动不动就把系统炸掉。
>
> 在 Linux 内核里，`BUG()` /`BUG_ON()` 的语义是： 这里发生了一个绝对不可恢复的错误，系统已经不可信了，直接崩给你看 。这通常会触发 oops 或 panic，逼用户重启机器。代价非常高。
>
> 作者想强调几层思想：
>
> 第一， BUG 只适用于“世界已经坏掉”的情况 。
> 比如：核心数据结构被破坏，而且你正处在一个无法回滚的中间状态，继续跑下去只会制造更大的灾难。这种情况下，停机反而是最“诚实”的行为。
>
> 第二， “还能勉强活着”就不配用 BUG 。
> 如果子系统还能继续运行，哪怕功能残缺、性能下降，或者只是拒绝当前操作、返回错误码，那就应该选择这些路径，而不是直接把整个内核带走。
> 换句话说： 局部失败 ≠ 系统级自杀 。
>
> 第三， BUG 往往是“懒惰但粗暴”的设计选择 。
> 写 `BUG_ON(x)` 很爽，逻辑也“干净”，但它把复杂性转嫁给了用户：数据没了，服务断了，机器得重启。作者在提醒你：再想一层，通常是有更优雅的退路的。
>
> 第四， 以内核为中心，还是以用户为中心 。
> 这段话明显站在用户一侧：
> 与其“立刻重启”，不如让系统还能 尽量有序地退出（shut down cleanly） 。这体现的是内核工程里的一个成熟价值观：
>
>> 内核宁可丑一点、复杂一点，也不要轻易把用户的系统炸掉。
>>
>
> 如果把它压缩成一句内核哲学，那就是：
>
> “BUG() 是最后的核按钮，而不是普通的错误处理手段。”
>
> `BUG()`
> 是一个无条件触发的“核爆按钮”。走到这行代码，内核立刻认为自己不可信，直接炸。
>
> `BUG_ON(cond)`
> 是一个 带条件的核爆按钮 。当 `cond` 为真时，等价于执行一次 `BUG()`。
>
> 如果只看效果：
>
>> `BUG_ON(x)` ≈`if (x) BUG();`
>>

## 2 WARN & WARN_ON

内核注释版：https://github.com/torvalds/linux/blob/master/include/asm-generic/bug.h#L85

```c
/*
 * WARN(), WARN_ON(), WARN_ON_ONCE(), and so on can be used to report
 * significant kernel issues that need prompt attention if they should ever
 * appear at runtime.
 *
 * Do not use these macros when checking for invalid external inputs
 * (e.g. invalid system call arguments, or invalid data coming from
 * network/devices), and on transient conditions like ENOMEM or EAGAIN.
 * These macros should be used for recoverable kernel issues only.
 * For invalid external inputs, transient conditions, etc use
 * pr_err[_once/_ratelimited]() followed by dump_stack(), if necessary.
 * Do not include "BUG"/"WARNING" in format strings manually to make these
 * conditions distinguishable from kernel issues.
 *
 * Use the versions with printk format strings to provide better diagnostics.
 */
```

翻译版：

```c
/*
 * WARN()、WARN_ON()、WARN_ON_ONCE() 等宏，
 * 可以用来报告那些一旦在运行时出现，就需要被尽快关注的
 * 重要内核问题。
 *
 * 在检查无效的外部输入时不要使用这些宏
 *（例如：非法的系统调用参数，或者来自网络 / 设备的非法数据），
 * 也不要用于像 ENOMEM 或 EAGAIN 这样的瞬态条件。
 * 这些宏只应该用于“可恢复的内核问题”。
 *
 * 对于无效的外部输入、瞬态条件等情况，
 * 应该使用 pr_err[_once/_ratelimited]()，
 * 并在必要时配合 dump_stack()。
 * 不要在格式字符串里手动包含 “BUG” 或 “WARNING”，
 * 以免把这些情况伪装成内核自身的问题。
 *
 * 使用带 printk 格式字符串的版本，
 * 以便提供更好的诊断信息。
 */

```

LLM理解版：

> 这段话是在给WARN 系列宏划清“使用边界” ，防止它们被滥用成“高级版 printk”。
>
> 核心思想只有一句话，但分量很重：
>
>> WARN 表示“内核自己出了问题”，不是“用户或外界做错了事”。
>>
>
> 把它拆开看。
>
> 第一层：WARN 是“需要人类注意的内核异常”
>
> `WARN()` /`WARN_ON()` 的定位不是调试噪音，也不是普通错误处理，而是：
>
> * 内核状态出现了不该出现、但还没到要死的问题
> * 开发者看到 warning + 栈回溯后，应该认真想：
>   “这里是不是有 bug？”
>
> 所以它天然带着一种“报警”语义，而不是“日志”。
>
> 第二层：为什么不能用于外部输入检查
>
> 非法 syscall 参数、畸形网络包、设备返回的垃圾数据——
> 这些东西在现实世界里 随时都会发生 ，尤其是在 fuzz、恶意环境、或者不可靠硬件下。
>
> 如果你对这些写：
>
> <pre class="overflow-visible! px-0!" data-start="856" data-end="888"><div class="contain-inline-size rounded-2xl corner-superellipse/1.1 relative bg-token-sidebar-surface-primary"><div class="sticky top-[calc(--spacing(9)+var(--header-height))] @w-xl/main:top-9"><div class="absolute end-0 bottom-0 flex h-9 items-center pe-2"><div class="bg-token-bg-elevated-secondary text-token-text-secondary flex items-center gap-4 rounded-sm px-2 font-sans text-xs"></div></div></div><div class="overflow-y-auto p-4" dir="ltr"><code class="whitespace-pre! language-c"><span><span>WARN_ON(len > MAX_LEN);
> </span></span></code></div></div></pre>
>
> 你等于在说：
>
>> “用户传了个坏参数 = 内核 bug”
>>
>
> 这是语义污染。
>
> 外部输入是 不可信的 ，必须被当成常态风险处理：返回 `-EINVAL`、丢包、重试，而不是报警。
>
> 第三层：为什么 ENOMEM / EAGAIN 也不配 WARN
>
> `ENOMEM`、`EAGAIN` 是 系统压力下的正常现象 ：
>
> * 内存吃紧
> * 资源暂时不可用
> * 并发竞争失败
>
> 这些不是“内核设计错误”，而是“环境条件变化”。
> 对它们 WARN，只会制造噪音，掩盖真正的问题。
>
> 第四层：什么叫“可恢复的内核问题”
>
> 这是 WARN 唯一该出现的地方：
>
> * 内核内部状态违反了某个预期
> * 但你有兜底逻辑，可以继续运行
> * 同时你希望：有人看到这个堆栈，回来修代码
>
> 典型例子：
>
> * refcount 不该为 0，但你还能跳过这条路径
> * 某个状态机进入了理论上不该出现的分支
> * 调度 / KVM / MM 里发现了“设计假设被打破”的情况
>
> 第五层：为什么强调 pr_err + dump_stack
>
> 这是在区分两种“红色日志”：
>
> * pr_err ：
>   世界很糟，但这是“外部/环境/用户”的锅
> * WARN ：
>   世界很糟，而且这是内核自己的锅
>
> 作者明确要求：
> 不要在 pr_err 里自己写 `"WARNING"`、`"BUG"`，否则人和工具都会被误导。
>
> 第六层：为什么要用带格式字符串的 WARN
>
> <pre class="overflow-visible! px-0!" data-start="1566" data-end="1586"><div class="contain-inline-size rounded-2xl corner-superellipse/1.1 relative bg-token-sidebar-surface-primary"><div class="sticky top-[calc(--spacing(9)+var(--header-height))] @w-xl/main:top-9"><div class="absolute end-0 bottom-0 flex h-9 items-center pe-2"><div class="bg-token-bg-elevated-secondary text-token-text-secondary flex items-center gap-4 rounded-sm px-2 font-sans text-xs"></div></div></div><div class="overflow-y-auto p-4" dir="ltr"><code class="whitespace-pre! language-c"><span><span>WARN_ON(x);
> </span></span></code></div></div></pre>
>
> 信息量极低，只告诉你“x 为真了”。
>
> 而：
>
> <pre class="overflow-visible! px-0!" data-start="1612" data-end="1667"><div class="contain-inline-size rounded-2xl corner-superellipse/1.1 relative bg-token-sidebar-surface-primary"><div class="sticky top-[calc(--spacing(9)+var(--header-height))] @w-xl/main:top-9"><div class="absolute end-0 bottom-0 flex h-9 items-center pe-2"><div class="bg-token-bg-elevated-secondary text-token-text-secondary flex items-center gap-4 rounded-sm px-2 font-sans text-xs"></div></div></div><div class="overflow-y-auto p-4" dir="ltr"><code class="whitespace-pre! language-c"><span><span>WARN(x, </span><span>"state=%d flags=%lx\n"</span><span>, state, flags);
> </span></span></code></div></div></pre>
>
> 是在给未来的你（或者 syzbot）留线索。
>
> 这是对调试者的基本礼貌。
>
> `WARN()`
> 无条件触发一次 warning。只要执行到这里，就打印 warning + 栈回溯。
>
> `WARN_ON(cond)`
>  条件触发 。当** **`cond` 为真时，触发一次 warning。
>
> `WARN_ON_ONCE(cond)`
>  条件触发，但只触发一次 。
> 不管这个条件以后再成立多少次，warning 只会打第一次。
