import os
import subprocess

def run_tymp2musicxml(input_file, output_file=None, nolyrics=False, verbose=False):
    """
    调用同目录下的 tymp2musicxml.exe，按指定参数运行。
    
    :param input_file:  输入的 .tymp 文件路径（相对或绝对）
    :param output_file: 输出的 .musicxml 文件路径（None 表示自动生成）
    :param nolyrics:    是否添加 -nolyrics 标志
    :param verbose:     是否添加 -v 标志
    :return: subprocess.CompletedProcess 对象
    """
    # 获取当前脚本所在目录
    script_dir = os.path.dirname(os.path.abspath(__file__))
    exe_path = os.path.join(script_dir, "tymp2musicxml.exe")
    
    # 构建命令列表
    cmd = [exe_path, input_file]
    
    # 如果有指定输出文件，先加进去（必须紧跟在输入文件后）
    if output_file:
        cmd.append(output_file)
    
    # 添加选项（顺序无关紧要）
    if nolyrics:
        cmd.append("-nolyrics")
    if verbose:
        cmd.append("-v")
    
    # 运行并等待完成
    try:
        result = subprocess.run(
    cmd,
    capture_output=True,
    text=True,
    errors='replace',   # 将无法解码的字节替换为 �
    check=True
)
        print("转换成功！")
        if result.stdout:
            print("标准输出:", result.stdout)
        if result.stderr:
            print("错误输出:", result.stderr)
        return result
    except subprocess.CalledProcessError as e:
        print(f"转换失败，返回码: {e.returncode}")
        print(e.stderr)
        raise

# 使用示例
if __name__ == "__main__":
    songname = input()
    # 例3：同时使用 -nolyrics 和 -v
    run_tymp2musicxml(songname, verbose=True, nolyrics=True)