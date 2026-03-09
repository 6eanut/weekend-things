## Get selftests/kvm coverage

common:

- access_tracking_perf_test
- coalesced_io_test
- demand_paging_test
- dirty_log_perf_test
- dirty_log_test
- guest_print_test
- irqfd_test
- kvm_binary_stats_test
- kvm_create_max_vcpus
- kvm_page_table_test
- memslot_modification_stress_test
- memslot_perf_test
- mmu_stress_test
- rseq_test
- set_memory_region_test
- steal_time

riscv:

- arch_timer
- ebreak_test
- get-reg-list
- sbi_pmu_test

steps:

```
# step1: 编译selftests/kvm
git clone git@github.com:kvm-riscv/linux.git
cd linux
git checkout riscv_kvm_fixes
make ARCH=riscv CROSS_COMPILE=riscv64-linux-gnu- olddefconfig	# config见linux-config
make ARCH=riscv CROSS_COMPILE=riscv64-linux-gnu- -j
make ARCH=riscv CROSS_COMPILE=riscv64-linux-gnu- headers_install
make ARCH=riscv CROSS_COMPILE=riscv64-linux-gnu- -C tools/testing/selftests/kvm install INSTALL_PATH=./kvm-selftest/

# step2: 启动qemu-system-riscv64
./start.sh
./ssh.sh
./scp.sh

# step3-1: kcov-wrapper-ptrace/host 
riscv64-linux-gnu-gcc kcov-wrapper-ptrace.c -o kcov-wrapper-ptrace
scp -P 18775 kcov-wrapper-ptrace root@localhost:/root
scp -P 18775 ./kvm-selftest/sbi_pmu_test root@localhost:/root
# step3-2: kcov-wrapper-ptrace/qemu
./kcov-wrapper-ptrace ./sbi_pmu_test
# step3-3: kcov-wrapper-ptrace/host
scp -P 18775 root@localhost:/root/pcs.txt .
python3 kcov-sym.py ./vmlinux pcs.txt

# step4: 分析files.txt
比对fuzz的files和selftests/kvm的files
```

automation:

见[automation.sh](script/automation/automation.sh)
