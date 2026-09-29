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


def mnemonic_of(label: str) -> str | None:
    """単独 `&` の次文字を返す。無ければ None。`&&` は除外。"""
    m = re.search(r"(?<!&)&(?!&)(.)", label)
    return m.group(1) if m else None


def main() -> int:
    errors: list[str] = []
    warnings: list[str] = []
    dup_warned: dict[str, list[str]] = {}
    checked = 0
    for path in sorted(GUI.glob("*.cpp")):
        seen: dict[str, list[int]] = {}
        for lineno, line in enumerate(
            path.read_text(encoding="utf-8").splitlines(), 1
        ):
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
