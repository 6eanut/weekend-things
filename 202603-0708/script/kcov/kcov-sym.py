import subprocess
import sys
import os
from multiprocessing import Pool, cpu_count

def addr2line_worker(args):
    """
    单个进程的工作函数：处理一组 PC 地址
    """
    vmlinux_path, pcs = args
    # 使用 -e 指定镜像，不加 -f 默认输出 file:line
    # 也可以加上 -s 仅显示文件名，不显示路径
    cmd = ['addr2line', '-e', vmlinux_path]
    
    process = subprocess.Popen(
        cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=10**6
    )
    
    # 将这组地址一次性输入，减少系统调用开销
    input_data = '\n'.join(pcs)
    stdout, stderr = process.communicate(input=input_data)
    
    if stderr:
        print(f"Error: {stderr}", file=sys.stderr)
        
    return stdout.splitlines()

def main():
    if len(sys.argv) != 3:
        print("用法: python3 kcov-sym.py <vmlinux路径> <pcs.txt路径>")
        sys.exit(1)

    vmlinux_path = sys.argv[1]
    pcs_file = sys.argv[2]
    output_file = "files.txt"

    if not os.path.exists(vmlinux_path):
        print(f"错误: 找不到 vmlinux 文件: {vmlinux_path}")
        sys.exit(1)

    # 1. 读取所有地址
    with open(pcs_file, 'r') as f:
        all_pcs = [line.strip() for line in f if line.strip()]

    total_pcs = len(all_pcs)
    num_workers = 20
    # 将地址列表切分成与 CPU 核心数相等的块
    chunk_size = (total_pcs + num_workers - 1) // num_workers
    chunks = [all_pcs[i:i + chunk_size] for i in range(0, total_pcs, chunk_size)]

    print(f"[*] 正在处理 {total_pcs} 个地址，使用 {num_workers} 个并行任务...")

    # 2. 并行执行 addr2line
    pool = Pool(processes=num_workers)
    # 将参数打包：(vmlinux_path, pcs_chunk)
    worker_args = [(vmlinux_path, chunk) for chunk in chunks]
    
    results = pool.map(addr2line_worker, worker_args)
    pool.close()
    pool.join()

    # 3. 合并结果并写入文件
    print(f"[*] 写入结果到 {output_file}...")
    with open(output_file, 'w') as f:
        for lines in results:
            for line in lines:
                f.write(line + '\n')

    print("[+] 转换完成！")

if __name__ == "__main__":
    main()