"""
打包脚本：读取 git tag 版本号，将 exe 和 bin 目录打包为 zip。
用法: python package.py
输出: NTE-Custom-BAGEL_v{version}.zip
"""

import os
import re
import zipfile
import subprocess
from pathlib import Path

PROJECT_DIR = Path(__file__).resolve().parent
EXE_PATH = PROJECT_DIR / "异环呗果图片上传器.exe"
BIN_DIR = PROJECT_DIR / "bin"


def get_version_from_git() -> str:
    """从最近的 git tag 获取版本号，去掉前导 v。"""
    try:
        result = subprocess.run(
            ["git", "describe", "--tags", "--abbrev=0"],
            cwd=PROJECT_DIR,
            capture_output=True,
            text=True,
            check=True,
        )
        tag = result.stdout.strip()
        # 去掉前导 'v'，如 v1.0.2 -> 1.0.2
        version = re.sub(r"^v", "", tag)
        return version
    except subprocess.CalledProcessError:
        print("错误：无法获取 git tag，请确保存在 git tag。")
        raise


def collect_files() -> list[tuple[str, str]]:
    """
    收集需要打包的文件。
    返回 list[(arcname, realpath)]。
    """
    files: list[tuple[str, str]] = []

    # exe（放在 zip 根目录）
    if not EXE_PATH.exists():
        print(f"警告：未找到 {EXE_PATH}")
    else:
        files.append((EXE_PATH.name, str(EXE_PATH)))

    # bin 目录（保留目录结构，排除 replace.png）
    if not BIN_DIR.exists():
        print(f"警告：未找到 {BIN_DIR}")
    else:
        for entry in sorted(BIN_DIR.rglob("*")):
            if entry.is_file() and entry.name != "replace.png":
                arcname = f"bin/{entry.relative_to(BIN_DIR).as_posix()}"
                files.append((arcname, str(entry)))

    return files


def main():
    version = get_version_from_git()
    project_name = PROJECT_DIR.name  # NTE-Custom-BAGEL
    zip_name = f"{project_name}_v{version}.zip"
    zip_path = PROJECT_DIR / zip_name

    files = collect_files()
    if not files:
        print("错误：没有找到任何需要打包的文件。")
        return

    print(f"版本: v{version}")
    print(f"输出: {zip_path}")
    print(f"文件数: {len(files)}")
    print()

    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for arcname, realpath in files:
            print(f"  添加: {arcname}")
            zf.write(realpath, arcname)

    print()
    print(f"打包完成: {zip_path}")


if __name__ == "__main__":
    main()
