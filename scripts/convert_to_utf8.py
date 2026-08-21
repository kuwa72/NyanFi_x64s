#!/usr/bin/env python3
"""src/*.cpp, src/*.h を CP932 から BOM 無し UTF-8 へ変換する。

改行コードは元のまま (LF) を保持し、往復デコードで内容の同一性を検証する。
--check を渡すと変換せずに現状の文字コードを報告するだけ。
"""
import glob
import sys

TARGETS = sorted(glob.glob("src/*.cpp") + glob.glob("src/*.h"))


def classify(data: bytes) -> str:
    if all(c < 0x80 for c in data):
        return "ascii"
    try:
        data.decode("utf-8")
        return "utf-8"
    except UnicodeDecodeError:
        pass
    try:
        data.decode("cp932")
        return "cp932"
    except UnicodeDecodeError:
        return "unknown"


def main() -> int:
    check_only = "--check" in sys.argv
    counts = {}
    converted = 0
    for path in TARGETS:
        raw = open(path, "rb").read()
        kind = classify(raw)
        counts[kind] = counts.get(kind, 0) + 1
        if check_only or kind != "cp932":
            continue
        text = raw.decode("cp932")
        out = text.encode("utf-8")
        # 往復検証: UTF-8 として読み直した文字列が元と一致すること
        if out.decode("utf-8") != text:
            print(f"FAIL roundtrip: {path}", file=sys.stderr)
            return 1
        with open(path, "wb") as fp:
            fp.write(out)
        converted += 1
    print(f"files={len(TARGETS)} {counts} converted={converted}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
