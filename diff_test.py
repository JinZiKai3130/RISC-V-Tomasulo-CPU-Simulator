#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
对拍脚本：用 data/ 下的每个测试数据分别运行 Tomasulo 模拟器（./code）
与 naive 模拟器（build/naive），比较两者输出（寄存器 a0 的低 8 位）。

用法：
  python3 diff_test.py               # 按需构建两个模拟器，然后对拍全部用例
  python3 diff_test.py --no-build    # 不重新构建，直接用现有二进制
  python3 diff_test.py --rebuild     # 强制重新构建
  python3 diff_test.py -t gcd expr   # 只对拍指定用例（不带 .data 后缀）

退出码：0 = 全部一致；1 = 存在不一致或运行出错；2 = 没有找到用例。
"""

import argparse
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
BUILD = os.path.join(ROOT, "build")
SIM_BIN = os.path.join(ROOT, "code")
NAIVE_BIN = os.path.join(BUILD, "naive")
NAIVE_SRC = os.path.join(ROOT, "naive_simulator.cpp")

# simulator 的源码依赖（任一比二进制新就重建）
SIM_SOURCES = (
    [os.path.join(ROOT, "src", f) for f in sorted(os.listdir(os.path.join(ROOT, "src")))]
    + [os.path.join(ROOT, "include", f) for f in sorted(os.listdir(os.path.join(ROOT, "include")))]
    + [os.path.join(ROOT, "CMakeLists.txt")]
)

DATA_ROOTS = [
    os.path.join(ROOT, "data", "testcases"),
    os.path.join(ROOT, "data", "sample"),
]

TIMEOUT = 120  # 单个用例超时（秒），防止死循环/跑飞


def newest_mtime(paths):
    """返回 paths 中存在文件的最新修改时间；都不存在返回 0。"""
    mt = 0.0
    for p in paths:
        if os.path.exists(p):
            mt = max(mt, os.path.getmtime(p))
    return mt


def build(force=False):
    """按需构建两个模拟器。"""
    # 1. naive：单文件，直接用 g++ 编译到 build/naive
    if force or not os.path.exists(NAIVE_BIN) or \
            os.path.getmtime(NAIVE_BIN) < os.path.getmtime(NAIVE_SRC):
        print("[build] g++ -O2 -std=c++17 naive_simulator.cpp -> build/naive")
        subprocess.run(
            ["g++", "-O2", "-std=c++17", NAIVE_SRC, "-o", NAIVE_BIN],
            check=True,
        )

    # 2. simulator：走 CMake，可执行文件输出到项目根目录（./code）
    if force or not os.path.exists(SIM_BIN) or \
            os.path.getmtime(SIM_BIN) < newest_mtime(SIM_SOURCES):
        print("[build] cmake --build build -> ./code")
        subprocess.run(["cmake", "--build", BUILD], check=True)


def run_case(binary, data_file):
    """运行单个模拟器，返回 (状态, 输出文本)。超时返回 ("TIMEOUT", "")。"""
    with open(data_file, "rb") as fin:
        try:
            r = subprocess.run(
                [binary],
                stdin=fin,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=TIMEOUT,
            )
        except subprocess.TimeoutExpired:
            return "TIMEOUT", ""
    if r.returncode != 0:
        return "RC%d" % r.returncode, r.stdout.decode(errors="replace").strip()
    return "OK", r.stdout.decode(errors="replace").strip()


def main():
    ap = argparse.ArgumentParser(description="Tomasulo 模拟器 vs naive 模拟器 对拍")
    ap.add_argument("--no-build", action="store_true",
                    help="跳过构建，直接用 build/ 下现有二进制")
    ap.add_argument("--rebuild", action="store_true",
                    help="强制重新构建（默认仅在源码比二进制新时构建）")
    ap.add_argument("-t", "--tests", nargs="*", default=None,
                    help="只对拍指定用例名（不带 .data），默认跑全部")
    args = ap.parse_args()

    if not args.no_build:
        build(force=args.rebuild)

    # 收集用例
    cases = []
    for root in DATA_ROOTS:
        if not os.path.isdir(root):
            continue
        for name in sorted(os.listdir(root)):
            if name.endswith(".data"):
                cases.append((name[:-5], os.path.join(root, name)))
    if args.tests:
        want = set(args.tests)
        cases = [(n, p) for n, p in cases if n in want]

    if not cases:
        print("没有找到任何 .data 用例")
        return 2

    # 对拍
    n_pass = n_fail = 0
    width = max(len(n) for n, _ in cases)
    print(f"{'用例':<{width}}  {'naive 输出':>12}  {'sim 输出':>12}  结果")
    print("-" * (width + 40))
    for name, path in cases:
        sn, on = run_case(NAIVE_BIN, path)
        ss, os_ = run_case(SIM_BIN, path)

        if sn != "OK" or ss != "OK":
            status = f"{sn}/{ss}" if sn != ss else sn
            n_fail += 1
        elif on == os_:
            status = "OK"
            n_pass += 1
        else:
            status = "MISMATCH"
            n_fail += 1

        print(f"{name:<{width}}  {on or '(空)':>12}  {os_ or '(空)':>12}  {status}")

    print("-" * (width + 40))
    print(f"通过 {n_pass} / {len(cases)}，失败 {n_fail}")
    return 0 if n_fail == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
