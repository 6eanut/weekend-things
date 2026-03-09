#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <unistd.h>
#include <errno.h>

/* Register definitions for riscv64 architecture */
#ifdef __riscv
    #include <linux/elf.h>
    #include <sys/uio.h>
    
    /* Prefer system header files */
    #ifdef __has_include
        #if __has_include(<asm/ptrace.h>)
            #include <asm/ptrace.h>
            #define HAVE_USER_REGS_STRUCT
        #elif __has_include(<linux/ptrace.h>)
            #include <linux/ptrace.h>
            #define HAVE_USER_REGS_STRUCT
        #endif
    #else
        /* 旧编译器：尝试直接包含 */
        #include <asm/ptrace.h>
        #define HAVE_USER_REGS_STRUCT
    #endif
    
    /* If system header files are not defined, define manually */
    #ifndef HAVE_USER_REGS_STRUCT
    struct user_regs_struct {
        unsigned long pc;
        unsigned long ra;
        unsigned long sp;
        unsigned long gp;
        unsigned long tp;
        unsigned long t0;
        unsigned long t1;
        unsigned long t2;
        unsigned long s0;
        unsigned long s1;
        unsigned long a0;
        unsigned long a1;
        unsigned long a2;
        unsigned long a3;
        unsigned long a4;
        unsigned long a5;
        unsigned long a6;
        unsigned long a7;  /* 系统调用号 */
        unsigned long s2;
        unsigned long s3;
        unsigned long s4;
        unsigned long s5;
        unsigned long s6;
        unsigned long s7;
        unsigned long s8;
        unsigned long s9;
        unsigned long s10;
        unsigned long s11;
        unsigned long t3;
        unsigned long t4;
        unsigned long t5;
        unsigned long t6;
    };
    #endif
#else
    #include <sys/user.h>
#endif

/* KCOV related macro definitions */
#define KCOV_INIT_TRACE          _IOR('c', 1, unsigned long)
#define KCOV_ENABLE              _IO('c', 100)
#define KCOV_DISABLE             _IO('c', 101)
#define KCOV_TRACE_PC            0

/* Coverage buffer size - can be set smaller since it will be cleared periodically */
#define COVER_SIZE (1024 * 1024)

// Comparison function for qsort deduplication
int compare_uint64(const void *a, const void *b) {
    uint64_t arg1 = *(const uint64_t *)a;
    uint64_t arg2 = *(const uint64_t *)b;
    if (arg1 < arg2) return -1;
    if (arg1 > arg2) return 1;
    return 0;
}

// Read syscall number (riscv64 specific, returns -1 on failure)
uint64_t get_syscall_number(pid_t pid) {
#ifdef __riscv
    struct user_regs_struct regs;
    struct iovec iov;
    iov.iov_base = &regs;
    iov.iov_len = sizeof(regs);
    
    // 使用 PTRACE_GETREGSET（更通用，推荐方式）
    if (ptrace(PTRACE_GETREGSET, pid, (void*)NT_PRSTATUS, &iov) == 0) {
        return regs.a7;
    }
#endif
    return (uint64_t)-1;
}

// Save PC data from current buffer
void save_pcs(uint64_t *cover, FILE *fp, uint64_t *total_pcs, uint64_t syscall_num) {
    uint64_t n = __atomic_load_n(&cover[0], __ATOMIC_ACQUIRE);
    
    if (n == 0) return;
    
    // Print debug information (optional)
    // if (syscall_num != (uint64_t)-1) {
    //     fprintf(stderr, "[Syscall %lu] 收集 %lu 个 PC\n", syscall_num, n);
    // }
    
    // Write directly to file (with syscall marker)
    for (uint64_t i = 1; i <= n && i < COVER_SIZE; i++) {
        fprintf(fp, "0x%lx", cover[i]);
        if (syscall_num != (uint64_t)-1) {
            fprintf(fp, " syscall=%lu", syscall_num);
        }
        fprintf(fp, "\n");
    }
    
    *total_pcs += n;
    
    // Clear buffer count (prepare for next collection)
    __atomic_store_n(&cover[0], 0, __ATOMIC_RELAXED);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <prog> [args...]\n", argv[0]);
        fprintf(stderr, "Use ptrace to collect KCOV data in segments to avoid buffer overflow\n");
        return 1;
    }

    int fd;
    uint64_t *cover;
    uint64_t total_pcs = 0;
    uint64_t syscall_count = 0;

    // 1. 打开 KCOV 设备
    fd = open("/sys/kernel/debug/kcov", O_RDWR);
    if (fd == -1) {
        perror("Failed to open /sys/kernel/debug/kcov");
        fprintf(stderr, "Please check:\n");
        fprintf(stderr, "  1. Is CONFIG_KCOV enabled in the kernel?\n");
        fprintf(stderr, "  2. Is debugfs mounted: mount -t debugfs none /sys/kernel/debug\n");
        fprintf(stderr, "  3. Do you have permission to access /sys/kernel/debug/kcov?\n");
        return 1;
    }

    // 2. 初始化追踪并映射内存
    if (ioctl(fd, KCOV_INIT_TRACE, COVER_SIZE)) {
        perror("ioctl(KCOV_INIT_TRACE) failed");
        close(fd);
        return 1;
    }
    
    cover = (uint64_t *)mmap(NULL, COVER_SIZE * sizeof(uint64_t),
                             PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (cover == MAP_FAILED) {
        perror("mmap failed");
        close(fd);
        return 1;
    }

    // 3. 打开输出文件
    FILE *fp = fopen("pcs_detailed.txt", "w");
    if (!fp) {
        perror("Cannot create pcs_detailed.txt");
        munmap(cover, COVER_SIZE * sizeof(uint64_t));
        close(fd);
        return 1;
    }

    // 4. Fork 子进程
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        fclose(fp);
        munmap(cover, COVER_SIZE * sizeof(uint64_t));
        close(fd);
        return 1;
    }

    if (pid == 0) {
        /* ===== 子进程 ===== */
        
        // 允许父进程 trace 自己
        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) == -1) {
            perror("ptrace(TRACEME) failed");
            exit(1);
        }

        // 开启 KCOV（一次性，持续运行）
        if (ioctl(fd, KCOV_ENABLE, KCOV_TRACE_PC)) {
            perror("ioctl(KCOV_ENABLE) failed");
            exit(1);
        }

        // 重置缓冲区
        __atomic_store_n(&cover[0], 0, __ATOMIC_RELAXED);

        // 执行目标程序
        execvp(argv[1], &argv[1]);
        
        perror("execvp failed");
        exit(1);
    }

    /* ===== 父进程 ===== */
    
    int status;
    int in_syscall = 0; // 0: 系统调用入口, 1: 系统调用出口
    
    // 等待子进程第一次停止（execvp 之前）
    waitpid(pid, &status, 0);
    
    // 开始跟踪系统调用
    if (ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACESYSGOOD) == -1) {
        perror("ptrace(SETOPTIONS) failed");
        kill(pid, SIGKILL);
        goto cleanup;
    }
    
    fprintf(stderr, "Starting to trace process %d...\n", pid);
    
    // 主循环：在每个系统调用边界处停止
    while (1) {
        // 继续执行到下一个系统调用边界
        if (ptrace(PTRACE_SYSCALL, pid, 0, 0) == -1) {
            perror("ptrace(SYSCALL) failed");
            break;
        }
        
        // 等待停止
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid failed");
            break;
        }
        
        // 检查进程是否退出
        if (WIFEXITED(status)) {
            fprintf(stderr, "Process exited with code: %d\n", WEXITSTATUS(status));
            break;
        }
        
        if (WIFSIGNALED(status)) {
            fprintf(stderr, "Process terminated by signal: %d\n", WTERMSIG(status));
            break;
        }
        
        // 检查是否是系统调用停止
        if (WIFSTOPPED(status) && WSTOPSIG(status) == (SIGTRAP | 0x80)) {
            if (!in_syscall) {
                // 系统调用入口
                in_syscall = 1;
            } else {
                // 系统调用出口
                in_syscall = 0;
                syscall_count++;
                
                // 尝试读取系统调用号
                uint64_t syscall_num = get_syscall_number(pid);
                
                // 读取并保存当前收集到的 PC 数据
                save_pcs(cover, fp, &total_pcs, syscall_num);
            }
        } else if (WIFSTOPPED(status)) {
            // 其他信号，传递给子进程
            int sig = WSTOPSIG(status);
            if (ptrace(PTRACE_SYSCALL, pid, 0, sig) == -1) {
                perror("ptrace signal delivery failed");
                break;
            }
            continue;
        }
    }
    
    // 读取最后可能残留的数据
    save_pcs(cover, fp, &total_pcs, (uint64_t)-1);
    
cleanup:
    fclose(fp);
    
    // 5. 去重并生成最终文件
    fprintf(stderr, "\nProcessing collected data...\n");
    
    // 读取所有 PC（仅 PC 部分）
    fp = fopen("pcs_detailed.txt", "r");
    if (!fp) {
        perror("Cannot reopen pcs_detailed.txt");
        goto final_cleanup;
    }
    
    // 动态数组存储 PC
    size_t capacity = 1024 * 1024;
    uint64_t *all_pcs = malloc(capacity * sizeof(uint64_t));
    size_t count = 0;
    
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        uint64_t pc;
        if (sscanf(line, "0x%lx", &pc) == 1) {
            if (count >= capacity) {
                capacity *= 2;
                all_pcs = realloc(all_pcs, capacity * sizeof(uint64_t));
            }
            all_pcs[count++] = pc;
        }
    }
    fclose(fp);
    
    // 排序并去重
    qsort(all_pcs, count, sizeof(uint64_t), compare_uint64);
    
    FILE *fp_unique = fopen("pcs.txt", "w");
    if (!fp_unique) {
        perror("Cannot create pcs.txt");
        free(all_pcs);
        goto final_cleanup;
    }
    
    uint64_t unique_count = 0;
    if (count > 0) {
        fprintf(fp_unique, "0x%lx\n", all_pcs[0]);
        unique_count = 1;
        for (size_t i = 1; i < count; i++) {
            if (all_pcs[i] != all_pcs[i-1]) {
                fprintf(fp_unique, "0x%lx\n", all_pcs[i]);
                unique_count++;
            }
        }
    }
    fclose(fp_unique);
    free(all_pcs);
    
    // 6. 打印统计信息
    printf("\n======== KCOV collection completed ========\n");
    printf("Number of traced syscalls: %lu\n", syscall_count);
    printf("Total PCs collected: %lu\n", total_pcs);
    printf("Unique PCs after deduplication: %lu\n", unique_count);
    printf("Detailed data (with syscall markers): pcs_detailed.txt\n");
    printf("Deduplicated PC list: pcs.txt\n");
    printf("================================\n");

final_cleanup:
    // 清理资源
    munmap(cover, COVER_SIZE * sizeof(uint64_t));
    close(fd);

    return 0;
}
