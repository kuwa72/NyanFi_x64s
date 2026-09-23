# OSS ツールチェインの実測記録

Issue #1 で使用するツールのバージョン、ビルド手順、検証結果を記録する。値は 2026-09-24 に `/tmp/wt-i1` と `/tmp` の build directory で実測した。

## 結論

- 基準は **MSYS2 UCRT64 の mingw-w64 GCC**。Linux ホストからは同じ `x86_64-w64-mingw32` ターゲットへクロスコンパイルする。
- ローカルで実測した toolchain は GCC 16.2.0、CMake 4.4.3、Ninja 1.13.2、Python 3.14.7、wxWidgets 3.3.3。
- core の Release ビルドと ctest、GUI の Release クロスビルド、`check_commands.py`、`check_literals.py` はすべて成功した。
- ローカルで wxWidgets をソースから再構築する `scripts/build_wx.sh` も別の prefix で実行し、wxWidgets 3.3.3 の configure/build/install まで通ることを確認した。
- Windows UCRT64 の実機起動は CI の `windows-ucrt64` ジョブで検証する。Linux のクロスコンパイル成功だけで Windows 実行を証明したわけではない。

## 実測環境

| 項目 | 実測値/場所 | 用途 |
|---|---|---|
| ホスト | `Linux ... microsoft-standard-WSL2 ... x86_64` | Linux からのクロスコンパイルと WSL interop でのテスト実行 |
| C++ compiler | `/home/linuxbrew/.linuxbrew/bin/x86_64-w64-mingw32-g++`、GCC 16.2.0、`x86_64-w64-mingw32` | C++20 の Win64 ビルド |
| C compiler | `x86_64-w64-mingw32-gcc`、GCC 16.2.0 | C 依存・リソースツールの検出 |
| binutils | `ar`/`ld`/`objdump` 2.47.20260726 | archive、link、PE import の確認 |
| resource compiler | `x86_64-w64-mingw32-windres` 2.47.20260726 | `nyanfi.rc` の windres 処理 |
| CMake | `/home/linuxbrew/.linuxbrew/bin/cmake` 4.4.3 | configure/build |
| CTest | `ctest` 4.4.3 | doctest 実行 |
| Ninja | `/home/linuxbrew/.linuxbrew/bin/ninja` 1.13.2 | build graph の実行 |
| Python | `python3` 3.14.7 | `scripts/check_*.py` と文字コード検査 |
| wxWidgets | `$HOME/opt/wx-3.3.3-mingw64/bin/wx-config` 3.3.3 | wxWidgets 3.3.3 の静的 MSW unicode ライブラリ |
| wx build tools | GNU Make 4.3、tar 1.35、curl 8.22.0、bash 5.2.21 | `scripts/build_wx.sh` の configure/build/install |

`wx-config --cxxflags` は `-D_FILE_OFFSET_BITS=64 -D__WXMSW__` と静的 MSW unicode include path を返し、`--libs core,base` は `libwx_mswu_core-3.3-x86_64-w64-mingw32.a` と `libwx_baseu-3.3-x86_64-w64-mingw32.a` を返した。生成された GUI は `file` で `PE32+ executable (GUI) x86-64` と判定された。

## 必要なツールとインストール前提

### mingw-w64

Linux 側では `x86_64-w64-mingw32-gcc`、`g++`、`windres`、`ar`、`ld` の一揃いが必要。CMake の toolchain file は以下の通り実行ファイルを前提にする。

- `x86_64-w64-mingw32-gcc`
- `x86_64-w64-mingw32-g++`
- `x86_64-w64-mingw32-windres`

Windows 側では同じ 계열を MSYS2 の `mingw-w64-ucrt-x86_64-gcc` パッケージから導入する。Windows SDK や Embarcadero RAD Studio はこの構成の前提ではない。

### CMake/Ninja/Python

- CMake は 3.24 以上を要求する（リポジトリの `CMakeLists.txt` の実設定）。
- build generator は Ninja を使う。
- Python 3 はリポジトリに同梱する doctest ではなく、`scripts/check_commands.py`、`scripts/check_literals.py`、文字コード変換補助の実行に使う。

### wxWidgets 3.3.3

`scripts/build_wx.sh` の既定値は次の通り。

- version: `WX_VERSION=3.3.3`
- prefix: `WX_PREFIX=$HOME/opt/wx-3.3.3-mingw64`
- work/cache: `WX_WORK=$HOME/.cache/nyanfi-wx`
- host: `x86_64-w64-mingw32`

スクリプトは GitHub の `wxWidgets-3.3.3.tar.bz2` を取得し、展開後、次 configure でソースから静的 MSW ライブラリを作る。

```text
../configure \
  --host=x86_64-w64-mingw32 \
  --build=$(../config.guess) \
  --prefix=$WX_PREFIX \
  --disable-shared \
  --disable-webview \
  --disable-mediactrl \
  --without-opengl \
  --without-subdirs \
  --disable-sysoptions \
  --with-msw \
  CXXFLAGS="-O2" \
  CFLAGS="-O2"
```

その後 `make -j$(nproc)` と `make install` を実行する。今回の実測では、既存導入済み prefix に加えて `/tmp/opencode/wx-i1-work` と `/tmp/opencode/wx-i1-prefix` を別の path に指定し、次のコマンドで 3.3.3 の configure/build/install が完了した。導入先には `bin/wx-config` が生成される。

```bash
WX_WORK=/tmp/opencode/wx-i1-work \
WX_PREFIX=/tmp/opencode/wx-i1-prefix \
scripts/build_wx.sh
```

## ツールチェーン file の役割

`cmake/toolchain-mingw-w64.cmake` は Linux から Windows ターゲットを作るための境界である。

```cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)
set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_RC_COMPILER  ${TOOLCHAIN_PREFIX}-windres)
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static -static-libgcc -static-libstdc++")
```

同時に、library/include/package は target root だけを検索し、program は host の PATH から探す設定になっている。Linux ネイティブ GCC は Windows の `.exe` を生成できないため、CMake の configure には必ずこの file を渡す。

ルート `CMakeLists.txt` の source は UTF-8、実行時 narrow literal は C++Builder との互換性のため `-fexec-charset=CP932` でコンパイルする。実測 compile command には以下が含まれる。

```text
-std=c++20
-finput-charset=UTF-8
-fexec-charset=CP932
-ffunction-sections
-fdata-sections
-fpermissive
```

`-fpermissive` は既存 `TStringList::Objects` の 32-bit ポインタ縮小キャストを GCC で通すための互換設定であり、clang 系へ移行時には別の修正が必要である。

## ビルド手順

### 1. core（ロジック層とテスト）

Release の再現コマンド:

```bash
cmake -S /tmp/wt-i1 -B /tmp/core-i1 -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DNYANFI_BUILD_TESTS=ON \
  -DNYANFI_BUILD_GUI=OFF \
  -DCMAKE_TOOLCHAIN_FILE=/tmp/wt-i1/cmake/toolchain-mingw-w64.cmake
cmake --build /tmp/core-i1 -j4
ctest --test-dir /tmp/core-i1 --output-on-failure
```

実測結果:

- configure: GNU 16.2.0 を検出し、Ninja build files を生成。
- build: `compat_tests.exe` と `core_tests.exe` まで完了。
- ctest: **2/2 PASS**（`compat_tests`、`core_tests`、合計 3.51 秒）。

### 2. `scripts/build.sh`

`scripts/build.sh` は `cmake/toolchain-mingw-w64.cmake` を明示し、Debug の configure → build → ctest をまとめて実行する。`BUILD_DIR` を指定すると build directory をリポジトリ外にできる。

```bash
BUILD_DIR=/tmp/build-script-i1 scripts/build.sh
```

今回の実測でも 219 個の compile/link ステップが進行し、`compat_tests` と `core_tests` の **2/2 PASS**（合計 3.02 秒）だった。`--no-test` を付けると ctest だけを省略できる。

### 3. wx GUI の Release クロスビルド

wxWidgets の prefix を明示して configure する。`gui/CMakeLists.txt` は `wx-config` を shell 経由で呼び、`--version`、`--cxxflags`、`--libs core,base` を取得する。CMake on Windows が `#!/bin/sh` を直接実行できないため、`sh`/`bash` 経由の問い合わせが実装されている。

```bash
cmake -S /tmp/wt-i1 -B /tmp/gui-i1 -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DNYANFI_BUILD_GUI=ON \
  -DNYANFI_BUILD_TESTS=OFF \
  -DCMAKE_TOOLCHAIN_FILE=/tmp/wt-i1/cmake/toolchain-mingw-w64.cmake \
  -DWX_CONFIG=$HOME/opt/wx-3.3.3-mingw64/bin/wx-config
cmake --build /tmp/gui-i1 --target nyanfi -j4
```

実測結果:

- configure: `wxWidgets 3.3.3` と `wx/msw/wx.rc` のパスを検出。
- build: **162/162** の compile/resource/link ステップが完了。
- 成果物: `/tmp/gui-i1/gui/nyanfi.exe`（約 32.6 MB、PE32+ GUI x86-64）。
- wx の MSW resource `wx/msw/wx.rc` は `nyanfi.rc` に埋め込まれている。Windres の include path を環境依存にしないための処理である。
- wx の import ライブラリではなく `.a` を使う静的導入だったため、ローカルでは wx の DLL を PATH に追加せずに link できた。

## 検査スクリプト

### コマンド名検査

```bash
python3 scripts/check_commands.py
```

実測出力:

```text
VCL のコマンド表: 464 個
gui/ が使うコマンド名: 405 個 (うち独自 1 個)

すべて VCL の表に存在する
```

`gui/` の `Execute()` が受け取るコマンド名と `src/usr_cmdlist.cpp` の VCL コマンド表を照合し、同じキーへの二重割り当ても検出する。`ShowCmdList` は VCL のコマンド表に無いが、GUI 固有の補助 dialog として `LOCAL_ONLY` に明示されている。

### narrow literal 検査

```bash
python3 scripts/check_literals.py
```

実測出力:

```text
対象: 388 ファイル

変換対象: 0 箇所 / 0 ファイル
```

検査対象は `cmake/phase0_sources.cmake` の `src`、`gui/`、core/compat の tests のうちビルド対象。`convert_narrow_literals.py --check` を呼び、non-ASCII の narrow literal が残っていれば終了コード 1 になる。

### 構文プローブ

```bash
scripts/probe.sh
```

全 22 candidate を `-fsyntax-only` で検査し、今回の結果は **22/22 PASS、18,031/18,031 行**。この検査は「各 `.cpp` が headers まで含めて構文を通す」測定であり、object の最終 link 成功を意味しない。`Global.cpp` などは構文 PASS でも、未定義 symbol が残る。

## CTest と Windows 実行

WSL2 interop では mingw-w64 が生成した `.exe` を Linux shell から直接実行でき、今回の `ctest` はそれで 2/2 PASS になった。これは mingw のテスト実行経路であり、Windows ネイティブの GUI 起動確認ではない。

GitHub Actions の `windows-ucrt64` ジョブは次を行う。

1. MSYS2 UCRT64 に `mingw-w64-ucrt-x86_64-gcc`, `cmake`, `ninja`, `wxwidgets3.2-msw` を導入。
2. `NYANFI_BUILD_GUI=ON` と `/ucrt64/bin/wx-config` で configure/build。
3. `ctest --test-dir build --output-on-failure`。
4. `objdump -p build/tests/core_tests.exe` で CRT import を記録。
5. `nyanfi.exe` を 5 秒起動し、早期終了しないことを確認。

Linux `linux-cross` ジョブは `g++-mingw-w64-x86-64`, `cmake`, `ninja-build` を導入し、2 つの Python check、CMake configure、build を行う。Windows 側が成果物と実行の基準であり、Linux ジョブはクロスコンパイルの維持確認である。

## CI の実行方法

PR 作成後は次で Jobs を確認する。

```bash
gh pr checks <PR番号>
```

両 job（`Linux クロスビルド`、`MSYS2 UCRT64 (基準)`）が pass するまで待つ。PR 上で check が report されない場合は、分岐を指定して手動実行する。

```bash
gh workflow run port-ci --ref issue/01-vcl-dependency-audit
gh pr checks <PR番号>
```

マージは行わない。

## 制約と失敗時の切り分け

- **wx-config が見つからない**: `WX_CONFIG` に `.../bin/wx-config` の実ファイルを指定する。`WX_PREFIX` だけを指定しても CMake の `find_program` が見つけない場合は absolute path を使う。
- **`wx.rc` が見つからない**: `wx/msw/wx.rc` の include path がない。wx-config の prefix と lib/include の対応を確認し、CMake の cache を削除して再 configure する。
- **共有 wx で `__imp_...`**: GUI 全体に `-static` を機械的に付けない。MSYS2 の shared wx では import library が必要。現在のローカル検証は wx 3.3.3 の静的導入であり、CI の shared wx 3.2 とは条件が異なる。
- **`winsock2.h` warning**: wx MSW header と Windows header の include順による warning。GUI の build/link と無関係だが、ログを推測で解釈せず、include順を確認する。
- **clang 系**: `-fexec-charset=CP932` を受け付けないため、基準 toolchain ではない。既存 narrow literal と `(DWORD)obj` の修正を別途実施する必要がある。
- **外部 DLL**: `usr_arc.cpp`、`usr_migemo.cpp`、`usr_xd2tx.cpp` は動的ロードする。今回の成功は compile/link までで、DLL の実行時 availability は別途確認する。
- **C++Builder/BCC64**: OSS 基準の検証対象ではない。今回の測定は Linux mingw-w64 と CI 用 Windows UCRT64 であり、BCC64 の build は行っていない。

## 実測コマンド一覧

```bash
# versions
cmake --version
ninja --version
python3 --version
x86_64-w64-mingw32-g++ --version
x86_64-w64-mingw32-windres --version
$HOME/opt/wx-3.3.3-mingw64/bin/wx-config --version

# core
cmake -S /tmp/wt-i1 -B /tmp/core-i1 -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DNYANFI_BUILD_TESTS=ON -DNYANFI_BUILD_GUI=OFF \
  -DCMAKE_TOOLCHAIN_FILE=/tmp/wt-i1/cmake/toolchain-mingw-w64.cmake
cmake --build /tmp/core-i1 -j4
ctest --test-dir /tmp/core-i1 --output-on-failure

# GUI
cmake -S /tmp/wt-i1 -B /tmp/gui-i1 -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DNYANFI_BUILD_GUI=ON -DNYANFI_BUILD_TESTS=OFF \
  -DCMAKE_TOOLCHAIN_FILE=/tmp/wt-i1/cmake/toolchain-mingw-w64.cmake \
  -DWX_CONFIG=$HOME/opt/wx-3.3.3-mingw64/bin/wx-config
cmake --build /tmp/gui-i1 --target nyanfi -j4

# checks
scripts/probe.sh
python3 scripts/check_commands.py
python3 scripts/check_literals.py
```
