#!/usr/bin/env python3
"""非 ASCII を含む narrow 文字列リテラルを _T(...) で包んで wide 化する。

背景 (issue #1):
C++Builder は narrow リテラルを CP932 で埋め込み、UnicodeString(const char*) は
実行時の ANSI コードページ (CP_ACP) で変換していた。つまり日本語の文字列は
「Windows のロケールが日本語である」ことに暗黙に依存していた。英語版 Windows
(GitHub ランナー) で走らせると文字化けし、実際に CI で検出された。

リテラルを wide にすればロケール非依存になり、同時に clang 系ツールチェイン
(UTF-8 以外の実行時文字コードを扱えない) でもビルドできるようになる。

  python3 scripts/convert_narrow_literals.py --check <paths...>   # 変換対象を表示
  python3 scripts/convert_narrow_literals.py <paths...>           # 変換を実行

隣接する文字列リテラル (`"あ" "い"` のような暗黙連結) は、ひとつでも非 ASCII を
含むなら **その連結の全体** を wide 化する。片方だけ wide にすると narrow と wide の
混在連結になり、規格上の扱いが処理系依存になるため。

除外: doctest のマクロ (TEST_CASE / SUBCASE / MESSAGE など) の引数は
`const char*` 固定なので wide 化してはいけない。表示用の文字列であり、
アサーションの正しさには影響しない。
"""
import glob
import re
import sys

#: 引数の narrow リテラルを wide 化してはいけないマクロ・関数
DENY_CALLERS = (
    "TEST_CASE", "TEST_CASE_TEMPLATE", "TEST_SUITE", "TEST_SUITE_BEGIN",
    "SUBCASE", "SCENARIO", "GIVEN", "WHEN", "THEN", "AND_WHEN", "AND_THEN",
    "MESSAGE", "INFO", "CAPTURE", "FAIL", "FAIL_CHECK", "WARN",
    "ClassNameIs",  # シムの ClassNameIs はクラス名 (ASCII) の比較用
)

_CALLER_RE = re.compile(r"([A-Za-z_][A-Za-z_0-9]*)\s*\(\s*$")


class Literal:
    """ソース上の 1 つの文字列リテラル"""

    def __init__(self, start: int, end: int, text: str, wide: bool, prefixed: bool, denied: bool = False):
        self.start = start        # 開始位置 (プレフィクスを含む先頭)
        self.end = end            # 終端の次の位置
        self.text = text          # 中身 (エスケープはそのまま)
        self.wide = wide          # L"..." / u"..." など
        self.prefixed = prefixed  # 既に _T(...) 等で包まれている
        self.denied = denied      # 除外リストのマクロ・関数の引数

    @property
    def has_non_ascii(self) -> bool:
        return any(ord(ch) > 0x7F for ch in self.text)

    @property
    def convertible(self) -> bool:
        return not self.wide and not self.prefixed and not self.denied and self.has_non_ascii


def scan(src: str):
    """文字列リテラルを走査して、隣接連結ごとにまとめた一覧を返す。"""
    runs = []       # [[Literal, ...], ...] 隣接連結のまとまり
    current = []    # 組み立て中の連結
    i = 0
    n = len(src)

    def flush():
        nonlocal current
        if current:
            runs.append(current)
            current = []

    while i < n:
        c = src[i]

        # コメント
        if src.startswith("//", i):
            j = src.find("\n", i)
            i = n if j < 0 else j
            flush()
            continue
        if src.startswith("/*", i):
            j = src.find("*/", i + 2)
            if j < 0:
                break
            i = j + 2
            flush()
            continue

        # 文字リテラル
        if c == "'":
            i += 1
            while i < n and src[i] != "'":
                i += 2 if src[i] == "\\" else 1
            i += 1
            flush()
            continue

        if c != '"':
            # 空白と改行だけなら連結が続いている可能性がある
            if not c.isspace():
                flush()
            i += 1
            continue

        # 文字列リテラル本体
        quote = i
        prefix_wide = False
        prefixed = False
        start = quote
        if quote > 0 and src[quote - 1] in "LuU":
            prefix_wide = True
            start = quote - 1
            if src[start - 1:start] == "8":  # u8"..."
                start -= 1
        head = src[max(0, quote - 8):quote]
        for macro in ("_T(", "_TEXT(", "TEXT("):
            if head.endswith(macro):
                prefixed = True
                break
        # 直前の呼び出し名が除外リストにあれば、そのリテラルは触らない
        denied = False
        caller = _CALLER_RE.search(src[max(0, quote - 64):quote])
        if caller and caller.group(1) in DENY_CALLERS:
            denied = True

        i = quote + 1
        buf = []
        while i < n and src[i] != '"':
            if src[i] == "\\":
                buf.append(src[i])
                i += 1
                if i < n:
                    buf.append(src[i])
                    i += 1
                continue
            buf.append(src[i])
            i += 1
        i += 1  # 終端の " を読み飛ばす
        current.append(Literal(start, i, "".join(buf), prefix_wide, prefixed, denied))

    flush()
    return runs


def convert(src: str):
    """変換後のソースと、変換した箇所数を返す。"""
    edits = []
    for run in scan(src):
        if not any(lit.convertible for lit in run):
            continue
        # 連結の一部が除外対象なら、連結全体を触らない。
        # narrow と wide が混在した連結になると (TEST_CASE("名前" _T("続き")) など)
        # マクロ側が要求する const char* にならず壊れる。
        if any(lit.denied for lit in run):
            continue
        # 変換するなら、その連結の narrow リテラル全部を wide にする
        for lit in run:
            if lit.wide or lit.prefixed:
                continue
            edits.append(lit)

    if not edits:
        return src, 0

    out = []
    pos = 0
    for lit in sorted(edits, key=lambda x: x.start):
        out.append(src[pos:lit.start])
        out.append("_T(" + src[lit.start:lit.end] + ")")
        pos = lit.end
    out.append(src[pos:])
    return "".join(out), len(edits)


def expand_paths(args):
    paths = []
    for a in args:
        if a.startswith("-"):
            continue
        paths.extend(sorted(glob.glob(a)))
    return paths


def main() -> int:
    check_only = "--check" in sys.argv
    paths = expand_paths(sys.argv[1:])
    if not paths:
        print("対象ファイルを指定してください (glob 可)", file=sys.stderr)
        return 2

    total = 0
    touched = 0
    for path in paths:
        src = open(path, encoding="utf-8").read()
        new, count = convert(src)
        if count == 0:
            continue
        total += count
        touched += 1
        print(f"{count:6d}  {path}")
        if check_only:
            continue
        # 検証: リテラルの中身が変わっていないこと
        before = [lit.text for run in scan(src) for lit in run]
        after = [lit.text for run in scan(new) for lit in run]
        if before != after:
            print(f"FAIL: リテラルの内容が変化した: {path}", file=sys.stderr)
            return 1
        open(path, "w", encoding="utf-8").write(new)

    verb = "変換対象" if check_only else "変換"
    print(f"\n{verb}: {total} 箇所 / {touched} ファイル")
    return 0


if __name__ == "__main__":
    sys.exit(main())
