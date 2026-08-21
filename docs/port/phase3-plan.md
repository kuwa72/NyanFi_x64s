# Phase 3 の計画 — 既存機能の全移植

メンテナの方針: **「今ある機能は基本移植したい」** (2026-08-21)。

この文書は、その規模と着手順を**推測ではなく実測**で決めたもの。数字はすべて
`scripts/probe_phase3.sh` と `scripts/check_commands.py` の出力から取っている。

---

## 1. 規模 (実測)

### コマンド数

NyanFi の機能は `src/usr_cmdlist.cpp` の `set_CmdList` が持つコマンド表がそのまま
一覧になっている。モード文字列は `F`(ファイルペイン) / `S`(検索) / `V`(ビューア) /
`I`(画像ビューア) / `L`(リスト) で、`FVIL:` のように複数モードで使えるものもある。

| モード | コマンド数 |
|---|---|
| F (ファイルペイン) | **359** |
| V (テキストビューア) | 137 |
| I (画像ビューア) | 101 |
| L (リスト) | 18 |
| S (インクリメンタルサーチ) | 15 |
| **ユニーク合計** | **464** |

現状の実装は **F モード 359 個中 34 個 (9%)**。残り 325 個。

### 実装が置かれている場所

未実装 325 個が VCL のどのファイルに実装されているかを機械的に数えた。

| ファイル | 該当コマンド数 | 行数 |
|---|---|---|
| **`MainFrm.cpp`** | **292** | 38,502 |
| `AppDlg.cpp` | 5 | 1,966 |
| `EditHistDlg.cpp` | 5 | 1,395 |
| `GenInfDlg.cpp` / `GitView.cpp` | 2 / 2 | 1,644 / 2,093 |
| その他フォーム | 各 1 | — |
| 見つからず (メニュー専用など) | 8 | — |

**9割が `MainFrm.cpp` にある。** ここが本体。

### 未移植のコード量

| 区分 | 行数 |
|---|---|
| `MainFrm.cpp` | 38,502 |
| `Global.cpp` (共有状態とヘルパ) | 16,550 |
| その他ロジック層 (`usr_shell` / `usr_excmd` / `task_thread` / `UserFunc` / `UserMdl` ほか) | 約 27,000 |
| フォーム 77個 (`.dfm` 付き) | 約 28,000 |
| **合計** | **110,526** |

---

## 2. 依存の構造 (実測)

`Global.h` がほぼすべての土台になっている。`scripts/probe_phase3.sh` で1本ずつ
構文チェックすると、ロジック層のファイルはどれも `Global.h` の解決に失敗して
そこで止まる。

```
Global.h ──┬── Global.cpp        16,550行
           ├── MainFrm.cpp       38,502行  ← 未実装コマンドの 9割
           ├── usr_excmd.cpp      2,050行
           ├── task_thread.cpp    1,964行
           ├── UserFunc.cpp       1,642行
           ├── UserMdl.cpp        1,306行
           └── InpCmds.cpp          463行
```

**`Global.h` / `Global.cpp` を通すことが全体のボトルネック。** ここが通るまで、
上の 62,000行はどれも1行もコンパイルできない。

### `Global.cpp` に足りないもの (実測 458 エラー)

不足している VCL ヘッダを空スタブで埋めて測ると 458 エラー。内訳は
**シムに無い型が 25種ほど**に集中していて、種類は少ない。

| 不足している型 | 出現 |
|---|---|
| `TIcon` | 15 |
| `__property` (`MarkList.h` / `task_thread.h` ほか) | 14 |
| `TOwnerDrawState` / `TGridDrawState` | 12 |
| `TSpeedButton` / `TButton` / `TRadioButton` / `TGroupBox` | 15 |
| `Clipboard` | 5 |
| `TValueRelationship` (`EqualsValue` / `GreaterThanValue` / `LessThanValue`) | 22 |
| `Today` / `WithinPastMilliSeconds` (System.DateUtils) | 7 |
| `TShape` / `TPicture` / `TPngImage` / `TImageList` / `TVirtualImageList` | 11 |
| `TOSVersion` | 3 |
| その他 | 残り |

Phase 0 で `usr_*.cpp` を通したときと**同じ種類の作業**で、規模も同程度。

不足していた VCL ヘッダ (16種) は転送ヘッダを足すだけで済む:
`Vcl.Controls` / `Vcl.StdCtrls` / `Vcl.ExtCtrls` / `Vcl.Forms` / `Vcl.Mask` /
`Vcl.ActnList` / `Vcl.ImgList` / `Vcl.Menus` / `Vcl.Dialogs` / `Vcl.ExtDlgs` /
`Vcl.StdActns` / `Vcl.ImageCollection` / `Vcl.BaseImageCollection` /
`Vcl.VirtualImageList` / `System.Actions` / `System.ImageList`。
これに加えて `System.Character` / `System.WideStrUtils` / `IdURI` / `Vcl.Styles`。

---

## 3. 着手順

### 第1段: `Global.h` / `Global.cpp` を通す (ボトルネック解消)

- 不足ヘッダ 20種の転送ヘッダを足す
- 不足している型 25種を `compat/gui_stubs.h` に**宣言のみ**で足す (規約4)
- `__property` を添字プロキシに置き換える (`MarkList.h` / `task_thread.h` /
  `UserMdl.h` / `InpExDlg.h` / `InpDir.h` / `thumb_thread.h`。既存の
  `usr_shell.h` の `TItemsProperty` と同じ手口)
- `Global.cpp` の回帰テストを書く (共有状態の初期化・ini 読み書き・パス操作)

これが済むと `usr_excmd` / `task_thread` / `UserFunc` / `UserMdl` / `InpCmds`
(計 7,400行) も射程に入る。

### 第2段: `MainFrm.cpp` のコマンド実装を機能群ごとに取り出す

`MainFrm.cpp` は `TNyanFiForm` (DFM 付きフォーム) なので**そのままはコンパイルしない**。
コマンドの実装 (`XxxActionExecute`) を機能群ごとに wx 側へ移す。1群 10〜20コマンドで
1 PR にする。**群の中の判断・変換・計算は wx 非依存の純関数にして doctest で固定する**
(規約8)。

順序は「よく使う順 × src の依存が浅い順」。

| # | 機能群 | 主なコマンド | 数 |
|---|---|---|---|
| 1 | カーソル・選択 | `CursorUpSel` `CursorDownSel` `PageUpSel` `SelReverseAll` `SelectUp` `NextSelItem` `PrevSelItem` `MatchSelect` `DateSelect` `SelEmptyDir` `SelAllFile` | 約 20 |
| 2 | 表示切替 | `ShowHideAtr` `ShowSystemAtr` `ShowByteSize` `ShowIcon` `HideSizeTime` `FileListOnly` `ShowTabBar` `BorderLeft/Right/Center` `EqualListWidth` `WidenCurList` `SwapLR` `WinMaximize/Minimize/Normal` | 約 20 |
| 3 | ディレクトリ移動 | `ChangeDir` `ChangeDrive` `NextDrive` `PrevDrive` `ToRoot` `ToOppSameItem` `CurrToOpp` `CurrFromOpp` `CsrDirToOpp` `PushDir` `PopDir` `DirStack` `SubDirList` `SpecialDirList` | 約 20 |
| 4 | 登録ディレクトリ・タブ | `RegDirDlg` `RegDirPopup` `ChangeRegDir` `TabDlg` `TabHome` `ToTab` `MoveTab` `SoloTab` `SaveTabGroup` `LoadTabGroup` `FixTabPath` `SyncLR` | 約 15 |
| 5 | ファイル操作 | `CopyTo` `MoveTo` `Backup` `Clone` `CopyDir` `Paste` `CopyToClip` `CutToClip` `CopyFileName` `NameToUpper/Lower` `SwapName` `UndoRename` `NewFile` `NewTextFile` `CreateDirsDlg` | 約 25 |
| 6 | リンク・属性 | `CreateShortcut` `CreateHardLink` `CreateSymLink` `CreateJunction` `FindHardLink` `SetDirTime` `SetExifTime` `SetArcTime` `SetFolderIcon` `CompressDir` | 約 12 |
| 7 | アーカイブ | `Pack` `PackToCurr` `UnPack` `UnPackToCurr` `ListArchive` `TestArchive` `UpdateFromArc` | 7 |
| 8 | 検索・比較 | `FindFileDlg` `FindDirDlg` `FindFileDirDlg` `FindDuplDlg` `FindTag` `DiffDir` `CompareDlg` `CompareHash` `GetHash` `SelOnlyCur` `結果リスト` 系 | 約 20 |
| 9 | ビューア・変換 | `JsonViewer` `XmlViewer` `ViewTail` `WatchTail` `ListText` `ListTree` `CountLines` `JoinText` `Convert*` `Extract*` | 約 30 |
| 10 | 外部連携 | `ExeCommandLine` `ExeExtTool` `ExeExtMenu` `ContextMenu` `OpenByExp` `CommandPrompt` `PowerShell` `WinTerminal` `FileRun` | 約 15 |
| 11 | ワークリスト | `WorkList` `NewWorkList` `SaveWorkList` `LoadWorkList` `WorkItem*` `SetAlias` `InsSeparator` | 約 15 |
| 12 | ログ・情報 | `ShowLogWin` `ToLog` `ScrollUpLog` `LogFileInfo` `ListNyanFi` `AboutNyanFi` `CalcDirSize` `DriveGraph` `FileExtList` | 約 15 |
| 13 | 設定 | `OptionDlg` `EditIniFile` `ViewIniFile` `DotNyanDlg` `SetSttBarFmt` `KeyList` の編集 | 約 12 |

### 第3段: V / I / L / S モード

テキストビューア 137、画像ビューア 101、リスト 18、検索 15。骨格はあるので
コマンドを埋めていく作業になる。

---

## 4. 判断をお願いしたい範囲

「基本移植したい」に含めるかどうか、はっきりしないものを挙げる。
**指示が無ければ最後に回し、それ以外を先に片付ける。**

| 群 | コマンド | 気になる点 |
|---|---|---|
| FTP | `FTPConnect` `FTPDisconnect` `FTPChmod` | Indy (VCL 同梱ライブラリ) 依存。OSS 版では別のライブラリに置き換えることになる |
| Git 連携 | `GitViewer` `GitDiff` `GitGrep` `OpenGitURL` `RepositoryList` `SelGitChanged` | `GitView.cpp` 2,093行 + フォーム。外部 git コマンド依存 |
| 電源・システム | `PowerOff` `Reboot` `LockComputer` `LockKeyMouse` `MonitorOff` `MuteVolume` | ファイラの機能として要るかどうか |
| 更新確認 | `CheckUpdate` | 本家のサーバを見に行く。フォークでは意味が変わる |
| ヘルプ | `HelpContents` `HelpCurWord` | 本家のヘルプファイルが前提 |

---

## 5. 進め方

Phase 0〜2 と同じループを続ける。

1. `scripts/probe_phase3.sh` で計測してから着手する
2. 契約 (ヘッダとテスト) を先に書く
3. 実装はサブエージェントに分割して渡す。**実呼び出し箇所を grep して報告させる**
4. 統合時にコーディネータが独立に検証する。**破壊的な経路は必ず読む** (規約9)
5. 1機能群 = 1 PR。レビュー可能な量を保つ
6. 見つけた不具合と決めた値は報告書に記録する

### 見積もり

1機能群あたり 10〜25コマンド、コミット 3〜6本、PR 1本。全 13群 + V/I/L/S で
**PR 15〜20本**の規模になる。第1段 (`Global.cpp`) だけは前提工事なので独立させる。
