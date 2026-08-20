#!/usr/bin/env python3
"""ビルド対象のファイルに、_T() で包まれていない非 ASCII リテラルが無いか調べる。

背景 (CLAUDE.md 規約1):
narrow リテラルのままだと実行時の ANSI コードページに依存し、英語版 Windows
(GitHub の CI ランナー) で文字化けする。CI で2回検出したので機械チェックにした。

対象は **ビルドに入っているファイルだけ**。未移植の GUI ファイル (MainFrm.cpp など)
には未変換のリテラルが 1,200 箇所以上残っているが、それらは Phase 3 の作業で、
今変換しても C++Builder 側でしか検証できない。

  python3 scripts/check_literals.py          # 違反があれば終了コード 1
"""
import glob
import os
import re
import subprocess
import sys


def built_files():
    """ビルド対象のファイル一覧を返す。"""
    files = []

    # シムだけでビルドしている src のファイルと、対応するヘッダ
    sources = open("cmake/phase0_sources.cmake", encoding="utf-8").read()
    for path in re.findall(r"^\t(src/\S+\.cpp)$", sources, re.M):
        files.append(path)
        header = path[:-4] + ".h"
        if os.path.exists(header):
            files.append(header)

    files += sorted(glob.glob("gui/*.cpp") + glob.glob("gui/*.h"))
    files += sorted(glob.glob("tests/core/*.cpp") + glob.glob("tests/compat/*.cpp"))
    files += sorted(glob.glob("tests/*.h"))
    return [f for f in files if os.path.exists(f)]


def main() -> int:
    files = built_files()
    print(f"対象: {len(files)} ファイル")

    result = subprocess.run(
        [sys.executable, "scripts/convert_narrow_literals.py", "--check", *files],
        capture_output=True,
        text=True,
    )
    print(result.stdout, end="")
    if result.stderr:
        print(result.stderr, end="", file=sys.stderr)

    if "変換対象: 0 箇所" in result.stdout:
        return 0

    print(
        "\n_T() で包まれていない非 ASCII リテラルがあります (CLAUDE.md 規約1)。\n"
        "  python3 scripts/convert_narrow_literals.py <該当ファイル>\n"
        "で変換してください。",
        file=sys.stderr,
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
