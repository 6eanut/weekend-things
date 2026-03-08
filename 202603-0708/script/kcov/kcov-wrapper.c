#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

/* KCOV 相关宏定义 */
#define KCOV_INIT_TRACE          _IOR('c', 1, unsigned long)
#define KCOV_ENABLE              _IO('c', 100)
#define KCOV_DISABLE             _IO('c', 101)
#define KCOV_TRACE_PC            0

/* 覆盖率缓冲区大小 (可以根据需求调整，例如 1024 * 1024) */
#define COVER_SIZE 268435455UL

// 比较函数，用于 qsort 去重
int compare_uint64(const void *a, const void *b) {
    uint64_t arg1 = *(const uint64_t *)a;
    uint64_t arg2 = *(const uint64_t *)b;
    if (arg1 < arg2) return -1;
    if (arg1 > arg2) return 1;
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <prog> [args...]\n", argv[0]);
        return 1;
    }

    int fd;
    uint64_t *cover, n, i;

    // 1. 打开 KCOV 设备
    fd = open("/sys/kernel/debug/kcov", O_RDWR);
    if (fd == -1) {
        perror("open /sys/kernel/debug/kcov 失败，请检查权限或内核配置");
        return 1;
    }

    // 2. 初始化追踪并映射内存
    if (ioctl(fd, KCOV_INIT_TRACE, COVER_SIZE)) {
        perror("ioctl(KCOV_INIT_TRACE) 失败");
        return 1;
    }
    cover = (uint64_t *)mmap(NULL, COVER_SIZE * sizeof(uint64_t),
                             PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (cover == MAP_FAILED) {
        perror("mmap 失败");
        return 1;
    }

    // 3. Fork 子进程运行目标程序
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork 失败");
        return 1;
    }

    if (pid == 0) {
        /* 子进程阶段 */
        // 开启当前线程的 KCOV
        if (ioctl(fd, KCOV_ENABLE, KCOV_TRACE_PC)) {
            perror("ioctl(KCOV_ENABLE) 失败");
            exit(1);
        }

        // 重置缓冲区计数
        __atomic_store_n(&cover[0], 0, __ATOMIC_RELAXED);

        // 执行目标程序
        execvp(argv[1], &argv[1]);
        
        perror("execvp 失败");
        exit(1);
    }

    /* 父进程阶段 */
    int status;
    waitpid(pid, &status, 0);

    // 读取收集到的 PC 数量 (第一个元素是计数)
    n = __atomic_load_n(&cover[0], __ATOMIC_ACQUIRE);
    
    // 停止 KCOV (可选，因为 fd 关掉会自动停止)
    ioctl(fd, KCOV_DISABLE);

    if (n == 0) {
        printf("未收集到任何 PC。请确保目标程序触发了内核调用。\n");
        return 0;
    }

    // 4. 处理数据：排序并去重
    uint64_t *pcs = malloc(n * sizeof(uint64_t));
    memcpy(pcs, &cover[1], n * sizeof(uint64_t));
    
    qsort(pcs, n, sizeof(uint64_t), compare_uint64);

    // 写入文件并计算去重后的数量
    FILE *fp = fopen("pcs.txt", "w");
    if (!fp) {
        perror("无法创建 pcs.txt");
        return 1;
    }

    uint64_t unique_count = 0;
    if (n > 0) {
        fprintf(fp, "0x%lx\n", pcs[0]);
        unique_count = 1;
        for (i = 1; i < n; i++) {
            if (pcs[i] != pcs[i-1]) {
                fprintf(fp, "0x%lx\n", pcs[i]);
                unique_count++;
            }
        }
    }
    fclose(fp);

    // 5. 打印统计信息
    printf("--- KCOV 收集完成 ---\n");
    printf("总计收集 PC 数量: %lu\n", n);
    printf("去重后唯一 PC 数量: %lu\n", unique_count);
    printf("结果已保存至: pcs.txt\n");

    // 清理
    free(pcs);
    munmap(cover, COVER_SIZE * sizeof(uint64_t));
    close(fd);

    return 0;
}