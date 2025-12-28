# Linux eBPF

**eBPF简述**

eBPF允许在不修改内核源码、不加载内核模块的情况下，在内核触发特定事件(如网络包到达、系统调用、函数执行)时运行用户自定义的代码。

**bpf系统调用的定义**

```
#include <linux/bpf.h>
int bpf(int cmd, union bpf_attr *attr, unsigned int size);
```

* cmd：想让内核做什么，常见的有：
  * BPF_PROG_LOAD：加载一段eBPF程序到内核；
  * BPF_MAP_CREATE：创建一个map；
  * BPF_MAP_LOOKUP_ELEM：在map中查找数据；
* attr：一个复杂的联合体，根据cmd的不同，提供具体的参数；
* size：attr结构体的大小。

**bpf的工作流程**

当调用bpf加载一段程序时：

* 加载：用户态将写好的eBPF字节码通过bpf发送给内核；
* 验证：内核验证器会检查代码，确保它不会崩溃、不会非法访问内存、权限合规；
* JIT编译：验证通过后，字节码被翻译成机器原生的指令，以达到原生的执行性能；
* 挂载：程序被绑定到特定的钩子上，比如某个网卡或某个系统调用。

**bpf的特点**

为监控或修改内核行为提供了便捷方法，否则需要修改内核代码。

**bpf代码**

* JIT编译相关的代码存放在arch/***/net下面
