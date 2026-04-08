针对arch/riscv/kvm做测试的两种手段：

* fuzz(我们想做的工作)
* testcase(selftests/kvm + kvm-unit-tests)

需要做的事情：

* 收集testcase的覆盖情况(文件-内核函数-基本块-测试用例)
  * selftests/kvm，编译，得到二进制，用kcov-wrapper-trace执行
  * kvm-unit-tests，编译，得到kvmtool二进制，用lkml执行，用kcov-wrapper-trace执行(参考anup的howtokvm仓库里的wiki，里面写的如何构造kvmtool)
* 收集fuzz的覆盖情况(文件-内核函数-基本块)
  * syzkaller提供
* 比对二者，生成一个html，分为两部分
  * 第一部分是三者(fuzz, selftests/kvm, kvm-unit-tests)各自的覆盖情况
    * 每个对应着一个tree，根是arch/riscv/kvm，根的儿子是所有的文件，文件的儿子是所有的内核函数，内核函数的儿子是所有的基本块。其反应覆盖情况
    * 最好是有一个按钮，选择展示三者的哪一者，原本tree是灰色的，然后亮起的是该者覆盖到的
  * 第二部分是对比：
    * fuzz和sefltests/kvm的比对，看哪些内核函数是selftests/kvm覆盖到而fuzz覆盖不到的，并且表注是由哪个测试用例覆盖的(如果可以精确到基本块就更好)
    * fuzz和kvm-unit-tests的比对，看哪些内核函数是kvm-unit-tests覆盖到而fuzz覆盖不到的，并且表注是由哪个测试用例覆盖的(如果可以精确到基本块就更好)

待细化...(AI辅助时代，一定要自己先规划清楚，然后再让AI去做)
