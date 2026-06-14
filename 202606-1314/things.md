# Spec-kit, NDSS'26, Skill2MCP, Multica, CC loop, DeepSeekCompressor and Virtual Device Fuzzing

## Spec-kit

见https://github.com/6eanut/weekend-things/blob/main/202606-1314/things.md

## NDSS'26

本文通过揭示 Hypervisor 中 Guest-Host 内存隔离的不对称性，提出了一种全新的漏洞利用范式——跨域攻击（CDA）。CDA 将"在宿主机中寻找可利用结构"这一困难问题转化为"利用 Guest 内存作为可操作利用立足点"这一更易处理的问题。通过自动化的四阶段框架，论文在 QEMU 和 VirtualBox 上成功实现了对 15 个真实世界漏洞的利用，展示了 CDA 的广泛适用性和实际威力。此项工作不仅填补了 Hypervisor 自动化利用领域的空白，也为虚拟化安全的防御提供了新的视角和具体建议。

这篇文章应该没法作为相关工作，原因是：其不是fuzz，而是提出了新的攻击范式/漏洞利用范式。

不过一个特别值得思考的问题是，这类提出新的漏洞利用范式的论文，能否结合到fuzz的论文里面去？

## Skill2MCP

Skill和MCP的本质区别是：Skill是一个使用说明书，MCP是一个工具箱。

比如要烧水，Skill写的是先向锅里接水，然后把锅放火上，然后开火，然后等水沸腾，最后关火，MCP是开/关水龙头、把锅放水池/火上，水是否沸腾，开关火。

所以二者不存在谁替代谁，他俩的关系不像C和Python：实现一个计算器软件，用C也行，用Python也行。不是的，Skill和MCP做的是不同的事情

## Multica

https://github.com/multica-ai/multica/blob/main/README.zh-CN.md

https://multica.ai/6eanut-test/inbox

可玩性挺强的，玩着试试吧，用它做一下deepseekcompressor的事情

不咋好用，subagent已经可以满足需求，没必要搞形式上这么复杂的东西

## CC loop

感觉和spec-kit做的是一个事情？基于规格说明书批量生成内容之类的

好像可以按时间设置，即运行多久之后再停止

目前感觉没啥意思

## DeepSeekCompressor

* [X] DeepSeekCompressor可以在K100上运行

实现过了，主要适配几个部分：把原本依赖于vllm完整运行环境的kernel提取成了可以独立运行的kernel（通过改api，把一些全局公共资源改成输入参数）；softmax的api适配等；有一些测试，比如运行测试、输出形状测试、数值正确性(拿python-cpu版本实现)、state cache正确性、边界情况等

为啥要把kv cache内存布局从3d改成2d：因为原始3d布局依赖vllm的特殊内存管理，在独立测试中无法复现，所以改用了2d。

下一阶段可以做的事情是，把适配后的kernel替换进去，看看能否跑通(不知道整个workflow中是否存在除了compressor之外其余无法适配的算子)

可能比较可信的测试方法是端到端的测试，独立测试不太可信

* [X] cudanalyst可以对triton代码做迭代优化

改动量不大，正确性/速度评测给适配了就ok，然后是提示词(拿triton写的gemm做了测试，正确性验证用的是cpu端的matmul)

## Virtual Device Fuzzing

* [X] VDF: Targeted Evolutionary Fuzz Testing of Virtual Devices(RAID'17)

只对虚拟化设备相关的源码做插桩，只收集这部分的覆盖率情况，而后指导模糊测试

* [X] HYPER-CUBE: High-Dimensional Hypervisor Fuzzing(NDSS'20)

hypervisor的实现是多样的，之前工作会针对某一特定实现做定制fuzz，它提出这样通用性不好，所以我们的paper可能也要谈通用性，比如除了kvm，其他的hypervisor可以测试吗？需要面临什么挑战？

* [X] Nyx: Greybox Hypervisor Fuzzing using Fast Snapshots and Affine Types(USENIX Security 21)

对hypervisor做fuzz，一个很重要的需求是能够让每次测试种子执行的环境是干净的，这意味着需要重启/录制重放，但这在hypervisor的场景下代价很高，所以这篇文章提出了一种新的重制方法，即一个种子执行完之后不是重启，而是重制状态，把写的内容退回即可。

> 作者发现，要在 Hypervisor 这种高度有状态的系统上实现覆盖引导 Fuzzing，关键在于**确保每个****测试用例****的可重复性** ——即每个测试用例必须从一个完全确定的、干净的状态开始执行。这意味着需要在**每次执行后重置整个****虚拟机****的全部状态** （包括内存、CPU 寄存器、所有模拟设备状态以及硬盘内容）。
>
> 传统的 VM 快照恢复机制（如 QEMU 的** **`loadvm` 命令）速度极慢，会在序列化/反序列化过程中浪费大量时间。作者的洞察是：可以通过**增量式****脏页****追踪 +** ******内存****直接复制 + 磁盘覆盖层**的方式，将快照恢复的操作从全量序列化转变为仅重置被修改的部分，从而将恢复速度提升到每秒数千次以上，接近 AFL 的 fork server 性能。

* [X] V-Shuttle: Scalable and Semantics-Aware Hypervisor Virtual Device Fuzzing(CCS'21)

vm-exit开销大，它提出类似注入的手段来解决。可能需要找一个注入方法带来的问题，比如得大量修改代码之类的，而且可能会引入假阳性和假阴性(修改代码带来的问题)

* [X] Morphuzz: Bending (Input) Space to Fuzz Virtual Devices(USENIX Security 22)

利用了hypervisor内在特性，这个很关键，我们也要强调，哪些问题是kvm特有的，解决方案如何是kvm特有的

* [X] ViDeZZo: Dependency-aware Virtual Device Fuzzing(SP'23)

提出了消息间/内部具有依赖关系，提出了一个dsl，编码这种依赖，测试器会从已有的种子里学习新依赖

* [X] HYPERPILL: Fuzzing for Hypervisor-bugs by Leveraging the Hardware Virtualization Interface(USENIX Security 24)

利用硬件规范去做vm-exit注入，很厉害
