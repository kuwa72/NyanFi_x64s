#!/usr/bin/env python3
"""gui/ が使うコマンド名が VCL のコマンド表に実在するかを確認する。

NyanFi のキー割り当ては ini の "KeyFuncList" に「キー名=コマンド名」で入っている。
コマンド名の綴りが VCL 版 (src/usr_cmdlist.cpp の set_CmdList) と違うと、
**本物の ini を読み込んでもそのコマンドだけ黙って効かない**。

実際に8件ずれていた (ChangePane / MarkAll / MarkItem / Refresh / ShowKeyList /
UnMarkAll / UpDir と、表に無い ShowCmdList)。ビルドもテストも通るので気づけない。

    python3 scripts/check_commands.py          # 確認 (CI が実行する)
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# VCL に対応するコマンドが無く、本フォークで足したもの。
# ここに足すときは「VCL の表を grep して無いことを確認した」上で理由を書く。
LOCAL_ONLY = {
	# コマンド表 (set_CmdList) にコマンド一覧を開くエントリが無い。
	# VCL では CmdListDlg はキー割り当ての設定中にだけ開く補助ダイアログで、
	# コマンドとしては呼べない。本フォークでは F12 で見られるようにした
	"ShowCmdList": "VCL はコマンドとして持たない (CmdListDlg は設定用の補助ダイアログ)",
}


def vcl_commands() -> dict:
	"""src/usr_cmdlist.cpp の set_CmdList からコマンド名→説明を取る。

	1行の形式は `_T("<モード文字列>:<コマンド名>=<説明>\\n")`。
	モード文字列は "F" だけでなく "FVIL" のように複数のこともある
	(usr_cmdlist.cpp の ScrModeIdStr = "FSVIL")。
	"""
	src = (ROOT / "src" / "usr_cmdlist.cpp").read_text(encoding="utf-8")
	rows = re.findall(r'_T\("([A-Z]+):([A-Za-z0-9_]+)=(.*?)\\n"\)', src, re.S)
	out = {}
	for modes, name, desc in rows:
		out.setdefault(name, desc)
	return out


def gui_commands() -> dict:
	"""gui/ が Execute() で受けているコマンド名 → 出現箇所"""
	found = {}
	for path in sorted((ROOT / "gui").glob("*.cpp")):
		text = path.read_text(encoding="utf-8")
		for m in re.finditer(r'SameStr\(command, _T\("([A-Za-z0-9_]+)"\)\)', text):
			line = text[: m.start()].count("\n") + 1
			found.setdefault(m.group(1), f"{path.relative_to(ROOT)}:{line}")
	# 既定の割り当て表が指しているコマンド名も対象にする
	km = ROOT / "gui" / "key_map.cpp"
	text = km.read_text(encoding="utf-8")
	for m in re.finditer(r'Assign\(_T\("[^"]*"\), _T\("([A-Za-z0-9_]+)"\)\)', text):
		line = text[: m.start()].count("\n") + 1
		found.setdefault(m.group(1), f"{km.relative_to(ROOT)}:{line}")
	return found


def main() -> int:
	vcl = vcl_commands()
	gui = gui_commands()

	# get_CsrKeyCmd() が返す名前は set_CmdList の表には無いが VCL の実装が使う
	csr = (ROOT / "src" / "usr_cmdlist.cpp").read_text(encoding="utf-8")
	m = re.search(r'get_CsrKeyCmd[^{]*\{(.*?)\n\}', csr, re.S)
	for name in re.findall(r'return "([A-Za-z0-9_]+)"', m.group(1) if m else ""):
		vcl.setdefault(name, "get_CsrKeyCmd が返す名前")

	bad = {n: w for n, w in gui.items() if n not in vcl and n not in LOCAL_ONLY}

	print(f"VCL のコマンド表: {len(vcl)} 個")
	print(f"gui/ が使うコマンド名: {len(gui)} 個 (うち独自 {len(LOCAL_ONLY)} 個)")
	if not bad:
		print("\nすべて VCL の表に存在する")
		return 0

	print(f"\n表に無いコマンド名が {len(bad)} 件あります:")
	for n, where in sorted(bad.items()):
		print(f"  {n:20} {where}")
	print("\n本物の ini (KeyFuncList) を読み込んでもこの名前では効きません。")
	print("VCL の綴りに直すか、独自コマンドなら LOCAL_ONLY に理由付きで足してください。")
	return 1


if __name__ == "__main__":
	sys.exit(main())
