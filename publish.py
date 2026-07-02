"""
Build Script read version from git tag, and package release files as zip.

Usage:
  python publish.py             Package existing files
  python publish.py --build     Build exe, then package it

Output: NTE-Custom-BAGEL_v{version}.zip
"""

import os
import re
import sys
import zipfile
import subprocess
import argparse
from pathlib import Path

PROJECT_DIR = Path(__file__).resolve().parent
EXE_NAME = "异环呗果图片上传器.exe"
BIN_DIR = PROJECT_DIR / "bin"
SRC_DIR = PROJECT_DIR / "src"

# 需要打包的文件（相对于 PROJECT_DIR）
RELEASE_FILES = [
    EXE_NAME,
    "CHAGNELOG.md",
    "NTEUploadBase.dll",
    "README_en.md",
    "README.md",
    "bin",        # 目录 — 递归收集
]

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
        version = re.sub(r"^v", "", tag)
        return version
    except subprocess.CalledProcessError:
        print("错误：无法获取 git tag，请确保存在 git tag。")
        raise


def find_msbuild() -> str:
    """查找 MSBuild 路径。"""
    result = subprocess.run(
        [
            "C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe",
            "-latest",
            "-products", "*",
            "-requires", "Microsoft.Component.MSBuild",
            "-find", "MSBuild\\**\\Bin\\MSBuild.exe",
        ],
        capture_output=True,
        text=True,
    )
    msbuild = result.stdout.strip()
    if not msbuild or not Path(msbuild).exists():
        print("错误：未找到 MSBuild。")
        sys.exit(1)
    return msbuild


def clean_all():
    """清理所有构建输出的二进制文件。"""
    msbuild = find_msbuild()
    cfg = ["/p:Configuration=Release", "/p:Platform=x64"]

    print("正在清理 exe 构建产物 ...")
    subprocess.run([msbuild, str(SRC_DIR / "ImagePreprocessor.vcxproj"), "/t:Clean", *cfg], cwd=SRC_DIR)

    print("正在清理 NTEUploadBase.dll 构建产物 ...")
    subprocess.run([msbuild, str(SRC_DIR / "libs" / "CloudUpload.vcxproj"), "/t:Clean", *cfg], cwd=SRC_DIR / "libs")

    print("正在清理 NTE-internal.dll 构建产物 ...")
    subprocess.run([msbuild, str(PROJECT_DIR / "lib" / "HTGame.BAGELImage.replace" / "in-game.photo.replacement" / "in-game.photo.replacement.vcxproj"), "/t:Clean", *cfg])

    # 额外清理输出目录的已知文件（Clean 目标不一定会删 OutDir 中所有文件）
    for f in [
        PROJECT_DIR / EXE_NAME,
        PROJECT_DIR / "异环呗果图片上传器.pdb",
        PROJECT_DIR / "异环呗果图片上传器.lib",
        PROJECT_DIR / "NTEUploadBase.dll",
        PROJECT_DIR / "NTEUploadBase.pdb",
        BIN_DIR / "NTE-internal.dll",
        BIN_DIR / "NTE-internal.pdb",
        BIN_DIR / "NTE-internal.lib",
    ]:
        if f.exists():
            f.unlink()
            print(f"  删除: {f.name}")

    print("清理完成")


def build_all():
    """构建所有发布组件：exe + NTEUploadBase.dll + bin/NTE-internal.dll"""
    msbuild = find_msbuild()
    cfg = ["/p:Configuration=Release", "/p:Platform=x64"]

    # 1. 构建 exe（独立 vcxproj，确保 OutDir 正确解析）
    print("正在构建 exe ...")
    exe_vcxproj = SRC_DIR / "ImagePreprocessor.vcxproj"
    ret = subprocess.run([msbuild, str(exe_vcxproj), *cfg], cwd=SRC_DIR)
    if ret.returncode != 0:
        print("错误：exe 构建失败。")
        sys.exit(1)
    exe_path = PROJECT_DIR / EXE_NAME
    if not exe_path.exists():
        print(f"错误：构建后未找到 {exe_path}")
        sys.exit(1)
    print(f"  -> {exe_path}")

    # 2. 构建 NTEUploadBase.dll
    print("正在构建 NTEUploadBase.dll ...")
    dll_vcxproj = SRC_DIR / "libs" / "CloudUpload.vcxproj"
    ret = subprocess.run([msbuild, str(dll_vcxproj), *cfg], cwd=SRC_DIR / "libs")
    if ret.returncode != 0:
        print("错误：NTEUploadBase.dll 构建失败。")
        sys.exit(1)
    dll_path = PROJECT_DIR / "NTEUploadBase.dll"
    if not dll_path.exists():
        print(f"错误：构建后未找到 {dll_path}")
        sys.exit(1)
    print(f"  -> {dll_path}")

    # 3. 构建 bin/NTE-internal.dll
    print("正在构建 NTE-internal.dll ...")
    internal_vcxproj = (
        PROJECT_DIR / "lib" / "HTGame.BAGELImage.replace"
        / "in-game.photo.replacement" / "in-game.photo.replacement.vcxproj"
    )
    ret = subprocess.run([msbuild, str(internal_vcxproj), *cfg])
    if ret.returncode != 0:
        print("错误：NTE-internal.dll 构建失败。")
        sys.exit(1)

    internal_path = BIN_DIR / "NTE-internal.dll"
    if not internal_path.exists():
        print(f"错误：构建后未找到 {internal_path}")
        sys.exit(1)
    print(f"  -> {internal_path}")


def collect_files(version: str) -> list[tuple[str, str]]:
    """
    收集需要打包的文件。
    返回 list[(arcname, realpath)]。
    """
    # zip 内的文件名重映射（key = 磁盘文件名, value = zip 内路径）
    arcname_rename = {
        EXE_NAME: f"NTE_Bagel_Uploader_v{version}.exe",
    }

    files: list[tuple[str, str]] = []

    for entry in RELEASE_FILES:
        path = PROJECT_DIR / entry
        if not path.exists():
            print(f"警告：未找到 {entry}，跳过")
            continue

        if path.is_dir():
            # 目录 — 递归收集，排除 replace.png
            for f in sorted(path.rglob("*")):
                if f.is_file() and f.name != "replace.png" and f.suffix != ".pdb":
                    arcname = f"{entry}/{f.relative_to(path).as_posix()}"
                    files.append((arcname, str(f)))
        else:
            arcname = arcname_rename.get(entry, entry)
            files.append((arcname, str(path)))

    return files


def main():
    parser = argparse.ArgumentParser(
        description="打包 NTE-Custom-BAGEL 发布文件"
    )
    parser.add_argument(
        "--build",
        action="store_true",
        help="先构建所有组件，再打包（否则仅打包已有文件）",
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="清理所有构建输出的二进制文件",
    )
    args = parser.parse_args()

    if args.clean:
        clean_all()
        return

    if args.build:
        build_all()

    version = get_version_from_git()
    project_name = PROJECT_DIR.name
    zip_name = f"{project_name}_v{version}.zip"
    zip_path = PROJECT_DIR / zip_name

    files = collect_files(version)
    if not files:
        print("错误：没有找到任何需要打包的文件。")
        return

    print(f"版本: v{version}")
    print(f"输出: {zip_path.name}")
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
