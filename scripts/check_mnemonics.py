#!/usr/bin/env python3
"""ダイアログUI項目のニーモニック検査 (Issue #93)。

gui/*.cpp の `new wxButton/wxCheckBox/wxRadioButton` のうち、
標準IDボタン (OK/Cancel/Yes/No/Close) 以外はラベルにニーモニック
(`&X`、1つの `&`) を持たなければならない。キーボード完結のため。

- リテラルラベル (`_T("保存(&S)")` / `"..."` / `to_wx(...)`) が対象。
- 変数・関数結果ラベルは解決不能のため警告のみ (件数を報告)。
- 同一ファイル内のニーモニック重複は警告のみ (複数ダイアログ同居のため)。
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GUI = ROOT / "gui"

STOCK_IDS = {
    "wxID_OK", "wxID_CANCEL", "wxID_YES", "wxID_NO", "wxID_CLOSE",
    "wxID_APPLY", "wxID_SAVE",
}

CTRL_RE = re.compile(
    r"new\s+wx(?:Button|CheckBox|RadioButton)\s*\(\s*[^,]+,\s*([^,]+),\s*(.*)$"
)
LITERAL_RE = re.compile(r'(?:to_wx\s*\(\s*)?(?:_T\s*\(\s*)?"((?:[^"\\]|\\.)*)"')

# wxRadioBox: ボックスラベル (第3引数) と選択肢配列の両方にニーモニックが必要。
# (wxChoice/wxComboBox の項目は Alt 処理が無いため対象外)
RADIOBOX_RE = re.compile(r"new\s+wxRadioBox\s*\(")
CHOICE_ADD_RE = re.compile(r"(\w+)\.Add\(to_wx\(_T\(\"((?:[^\"\\]|\\.)*)\"\)\)")
BUILDER_DEF_RE = re.compile(r"wxArrayString\s+(\w+)\s*\(\s*\)")


def mnemonic_of(label: str) -> str | None:
    """単独 `&` の次文字を返す。無ければ None。`&&` は除外。"""
    m = re.search(r"(?<!&)&(?!&)(.)", label)
    return m.group(1) if m else None


def split_top_args(text: str, open_pos: int) -> list[str]:
    """open_pos の '(' に対応する引数リストをトップレベルカンマで分割する。"""
    args: list[str] = []
    depth = 0
    cur: list[str] = []
    in_str = False
    esc = False
    for ch in text[open_pos:]:
        if in_str:
            cur.append(ch)
            if esc:
                esc = False
            elif ch == "\\":
                esc = True
            elif ch == '"':
                in_str = False
            continue
        if ch == '"':
            in_str = True
            cur.append(ch)
        elif ch == "(":
            depth += 1
            if depth > 1:
                cur.append(ch)
        elif ch == ")":
            depth -= 1
            if depth == 0:
                args.append("".join(cur))
                break
            cur.append(ch)
        elif ch == "," and depth == 1:
            args.append("".join(cur))
            cur = []
        else:
            cur.append(ch)
    return args


def check_radioboxes(
    path, text: str, errors: list[str], warnings: list[str]
) -> int:
    """wxRadioBox のボックスラベルと選択肢配列を検査する。戻り値は検査件数。"""
    checked = 0
    lines = text.splitlines()
    radio_vars: set[str] = set()
    radio_builders: set[str] = set()
    for m in RADIOBOX_RE.finditer(text):
        args = split_top_args(text, m.end() - 1)
        lineno = text.count("\n", 0, m.start()) + 1
        if len(args) < 6:
            warnings.append(f"{path.name}:{lineno}: RadioBox引数を読めない (要目視)")
            continue
        lm = LITERAL_RE.search(args[2])
        if lm:
            label = lm.group(1)
            if label:
                checked += 1
                if mnemonic_of(label) is None:
                    errors.append(
                        f"{path.name}:{lineno}: RadioBoxラベルにニーモニック無し: {label}"
                    )
        sixth = args[5].strip()
        bm = re.match(r"(\w+)\s*\(\s*\)$", sixth)
        if bm:
            radio_builders.add(bm.group(1))
        elif re.match(r"^[A-Za-z_]\w*$", sixth):
            radio_vars.add(sixth)
    # 生成関数の本体 (wxArrayString Name() ... ^}) 内の Add を検査
    if radio_builders:
        cur_builder: str | None = None
        for lineno, line in enumerate(lines, 1):
            if cur_builder is None:
                dm = BUILDER_DEF_RE.search(line)
                if dm and dm.group(1) in radio_builders:
                    cur_builder = dm.group(1)
                continue
            if line == "}":
                cur_builder = None
                continue
            am = CHOICE_ADD_RE.search(line)
            if am:
                checked += 1
                if mnemonic_of(am.group(2)) is None:
                    errors.append(
                        f"{path.name}:{lineno}: RadioBox選択肢にニーモニック無し"
                        f" ({cur_builder}): {am.group(2)}"
                    )
    # 変数配列の Add を検査
    if radio_vars:
        for lineno, line in enumerate(lines, 1):
            am = CHOICE_ADD_RE.search(line)
            if am and am.group(1) in radio_vars:
                checked += 1
                if mnemonic_of(am.group(2)) is None:
                    errors.append(
                        f"{path.name}:{lineno}: RadioBox選択肢にニーモニック無し"
                        f" ({am.group(1)}): {am.group(2)}"
                    )
    return checked


def main() -> int:
    errors: list[str] = []
    warnings: list[str] = []
    dup_warned: dict[str, list[str]] = {}
    checked = 0
    for path in sorted(GUI.glob("*.cpp")):
        seen: dict[str, list[int]] = {}
        text = path.read_text(encoding="utf-8")
        checked += check_radioboxes(path, text, errors, warnings)
        for lineno, line in enumerate(text.splitlines(), 1):
            m = CTRL_RE.search(line)
            if not m:
                continue
            ident, rest = m.group(1).strip(), m.group(2)
            if ident in STOCK_IDS:
                continue
            lm = LITERAL_RE.search(rest)
            if not lm:
                warnings.append(f"{path.name}:{lineno}: 動的ラベル (要目視)")
                continue
            label = lm.group(1)
            checked += 1
            mn = mnemonic_of(label)
            if mn is None:
                errors.append(f"{path.name}:{lineno}: ニーモニック無し: {label}")
            else:
                seen.setdefault(mn.upper(), []).append(lineno)
        for mn, lines in seen.items():
            if len(lines) > 1:
                dup_warned.setdefault(path.name, []).append(
                    f"&{mn}: {lines}"
                )
    for name, ds in dup_warned.items():
        for d in ds:
            warnings.append(f"{name}: ニーモニック重複の疑い: {d}")
    for w in warnings:
        print(f"WARN: {w}")
    for e in errors:
        print(f"ERROR: {e}")
    print(f"検査: {checked} 件 / エラー: {len(errors)} 件 / 警告: {len(warnings)} 件")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
