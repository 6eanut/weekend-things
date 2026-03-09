#!/bin/bash
set -euo pipefail
set -x  # 开启命令追踪，显示每条命令

# ==============================
# 配置区域
# ==============================
SSH_PORT=18775
SSH_TARGET=root@localhost
KCOV_WRAPPER="./kcov-wrapper-ptrace"
KCOV_SYM="kcov-sym.py"
KVM_SELFTEST_DIR="./kvm-selftest"
RESULT_DIR="./results"

mkdir -p "$RESULT_DIR"

# 测试列表
TESTS=(
    access_tracking_perf_test
    coalesced_io_test
    demand_paging_test
    dirty_log_perf_test
    dirty_log_test
    guest_print_test
    irqfd_test
    kvm_binary_stats_test
    kvm_create_max_vcpus
    kvm_page_table_test
    memslot_modification_stress_test
    memslot_perf_test
    mmu_stress_test
    rseq_test
    set_memory_region_test
    steal_time
    arch_timer
    ebreak_test
    get-reg-list
    sbi_pmu_test
)

# ==============================
# 函数：检查 QEMU 是否存活
# ==============================
check_qemu_alive() {
    ssh -p "$SSH_PORT" -o ConnectTimeout=20 "$SSH_TARGET" "echo ok" &>/dev/null || {
        echo "!!! QEMU 已经死掉，终止运行 !!!"
        exit 1
    }
}

# ==============================
# 主循环
# ==============================
for t in "${TESTS[@]}"; do
    check_qemu_alive

    ssh -p "$SSH_PORT" "$SSH_TARGET" "rm -f pcs.txt"
    ssh -p "$SSH_PORT" "$SSH_TARGET" "rm -f pcs_detailed.txt"

    ssh -p "$SSH_PORT" "$SSH_TARGET" "$KCOV_WRAPPER $KVM_SELFTEST_DIR/$t"

    check_qemu_alive

    PCS_HOST="$RESULT_DIR/${t}_pcs.txt"
    scp -P "$SSH_PORT" "$SSH_TARGET":/root/pcs.txt "$PCS_HOST"
    
    PCS_DETAILED_HOST="$RESULT_DIR/${t}_pcs_detailed.txt"
    scp -P "$SSH_PORT" "$SSH_TARGET":/root/pcs_detailed.txt "$PCS_DETAILED_HOST"

    python3 "$KCOV_SYM" /home/jiakai/0308selftest_kvm_kcov/kvm-riscv/vmlinux "$PCS_HOST"
    
    mv files.txt "$RESULT_DIR/${t}_files.txt"
done