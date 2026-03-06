启发：

* 哪些汇编指令是特别的；
* 哪些寄存器是特别的；
* IO设备；
* 中断/异常向量表的注册；
* AIA；
* ARM有的，RV都可以有

其实现有AMD64/ARM64的实现并没有很丰富，也不是很复杂。

# AMD64 SYZOS

### CPUID

在x86里面，有一条汇编指令是cpuid。

### WRMSR/RDMSR

MSR -- Model Specfic Register

wrmsr和rdmsr都是x86里面的汇编指令，而且是特权指令，跟上面的cpuid用法类似吧。

### WR_CRN/WR_DRN

CR指的是control register，DR指的是debug register，这属于是写特殊的寄存器。

### IN_DX/OUT_DX

x86的IO是独立编址，需要in/out指令来触发，这和cpuid属于一类。

### SET_IRQ_HANDLER

注册中断向量表，实现很简单，一种是直接iretq，另一种是调用uexit把信息写一下然后再iretq。其实就是不做处理，可以理解为执行下一条指令？

### ENABLE_NESTED

分amd和intel两种处理，反正就类似于有一些寄存器值控制着是否能嵌套虚拟化，需要开一下，再一个就是设置一些初始状态，类似于设置为rv s mode一样。

### NESTED_CREATE_VM

准备L2上下文，设置页表、寄存器啥的。

### NESTED_LOAD_CODE

这是把l2要执行的代码放到create_vm那一步创建的内存里

### NESTED_VMLAUNCH

vmrun是amd的一个硬件指令，用来启动虚拟机。剩下的就不看了吧，因为rv现在其实嵌套虚拟化的代码还没合入主线，而且x86是cisc，和rv共性不大，下面看arm64吧。

### NESTED_VMRESUME/NESTED_INTEL_VMWRITE_MASK/NESTED_AMD_WMCB_WRITE_MASK/NESTED_AMD_INVLPGA/NESTED_AMD_STGI/NESTED_AMD_CLGI/NESTED_AMD_INJECT_EVENT/NESTED_AMD_SET_INTERCEPT/NESTED_AMD_VMLOAD/NESTED_AMD_VMSAVE

# ARM64 SYZOS

### MSR/MRS

system register读写的指令，rv是csr。

### SMC

Secure Monitor Cal，用于在normal world和secure world之间切换

### HVC

HyperVisor Call，guest想要执行hypervisor的服务程序，执行htpervisor；类似于rv的sbi？

### IRQ_SETUP

设置中断向量表

### MEMWRITE

就是普通的写内存，rv也可以有。

### ITS_SETUP

ITS类似于PLIC

### ITS_SEND_CMD

关于PLIC的一顿操作，类似于RV里面关于AIA的操作吧。

### ERET

Exception RETurn

### SVC

SuperVisor Call
