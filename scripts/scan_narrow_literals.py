#!/usr/bin/env python3
"""非 ASCII を含む narrow 文字列リテラルを数える。

背景 (issue #1 / Phase 0):
C++Builder は narrow リテラルを CP932 として扱い、UnicodeString(const char*)
は CP_ACP で変換していた。mingw-w64 は -fexec-charset=CP932 でこの挙動を再現
できるが、本番ターゲットの clang-cl は UTF-8 以外の実行時文字コードを扱えない。
そのため clang-cl へ移る前に、非 ASCII を含む narrow リテラルを wide
(_T(...) / L"...") へ機械変換する必要がある。その作業量を測る。

  python3 scripts/scan_narrow_literals.py            # 集計のみ
  python3 scripts/scan_narrow_literals.py --list     # 該当箇所を列挙
"""
import glob
import sys


def scan(path: str):
    """(行番号, リテラル) の一覧を返す。wide 扱いのものは除外する。"""
    src = open(path, encoding="utf-8").read()
    found = []
    i = 0
    line = 1
    n = len(src)
    while i < n:
        c = src[i]
        if c == "\n":
            line += 1
            i += 1
            continue
        # コメントを飛ばす
        if src.startswith("//", i):
            j = src.find("\n", i)
            i = n if j < 0 else j
            continue
        if src.startswith("/*", i):
            j = src.find("*/", i + 2)
            if j < 0:
                break
            line += src.count("\n", i, j)
            i = j + 2
            continue
        # 文字リテラルを飛ばす
        if c == "'":
            i += 1
            while i < n and src[i] != "'":
                i += 2 if src[i] == "\\" else 1
            i += 1
            continue
        if c != '"':
            i += 1
            continue
        # 文字列リテラル。開始位置の直前を見て wide かどうか判定する
        start = i
        prefix_wide = False
        k = start - 1
        if k >= 0 and src[k] in "LuU":
            # L"..." / u"..." / U"..." / u8"..." は narrow 問題の対象外
            prefix_wide = True
        head = src[max(0, start - 8):start]
        if head.endswith("_T(") or head.endswith("_TEXT(") or head.endswith("TEXT("):
            prefix_wide = True
        i += 1
        buf = []
        while i < n and src[i] != '"':
            if src[i] == "\\":
                buf.append(src[i])
                i += 1
                if i < n:
                    buf.append(src[i])
                    i += 1
                continue
            if src[i] == "\n":
                line += 1
            buf.append(src[i])
            i += 1
        i += 1
        text = "".join(buf)
        if not prefix_wide and any(ord(ch) > 0x7F for ch in text):
            found.append((line, text))
    return found


def main() -> int:
    show_list = "--list" in sys.argv
    files = sorted(glob.glob("src/*.cpp") + glob.glob("src/*.h"))
    per_file = {}
    total = 0
    for path in files:
        hits = scan(path)
        if hits:
            per_file[path] = hits
            total += len(hits)
    for path, hits in sorted(per_file.items(), key=lambda kv: -len(kv[1])):
        print(f"{len(hits):6d}  {path}")
        if show_list:
            for line, text in hits:
                snippet = text if len(text) <= 60 else text[:57] + "..."
                print(f"        {path}:{line}: \"{snippet}\"")
    print(f"\n非 ASCII を含む narrow リテラル: {total} 箇所 / {len(per_file)} ファイル (全 {len(files)} ファイル中)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
