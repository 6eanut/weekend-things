研究重点其实有两个：

1.在QEMU中，当开启-machine virt,aclint=on,aia=aplic-imsic,aia-guests=3,iommu-sys=on,acpi=on -cpu max时，会发生什么(会执行什么样的ioctl)

2.kvm-unit-tests里面都测到了哪些，是否涉及AIA
