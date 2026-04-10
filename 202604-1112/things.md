# Fuzz and Testcase

fuzz覆盖的addr，通过syzkaller就可以获得，很容易。

selftest覆盖的addr，按照文档，编译成可执行文件，被kcov-wrapper-trace包裹着执行得到pcs.txt，然后被kcov-sym.py处理就行了。

kvm-unit-tests覆盖的addr，因为kvm-unit-tests编译得到的是flat(guest code)，所以需要按照anup在kvm riscv仓库里面的howto wiki去编译静态的kvmtool出来，然后把kvmtool放到qemu里面，用法在howto wiki里面也有，把guest code换成kvm-unit-tests的即可。最后同样是用kcov-wrapper-trace和kcov-sym.py
