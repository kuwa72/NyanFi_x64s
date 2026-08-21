# NyanFi_x64s

![Screenshot](screenshot.png)

## 概要

キーボード操作主体の2画面ファイラーです(マウス操作も可能)。  
Windows 7 ～ Windows 11 で動作します。  

これは、64ビット従来版 V15.63 をベースに作成した、ライト/ダークモード表示可能なVCLスタイル版です。  
INIファイルなどは、従来版のものを流用できます。  

[スクリーンショット](doc/screenshot.md)

## 開発環境

C++Builder 12.1 (BCC64)

### OSS ツールチェインへの移植 (進行中)

VCL 依存を除き、無償のツールチェインだけでビルドできるようにする作業を進めています。
方針は [issue #1](https://github.com/kuwa72/NyanFi_x64s/issues/1)、現状は
[docs/port/phase0-report.md](docs/port/phase0-report.md) を参照してください。

基準ツールチェインは **MSYS2 UCRT64 (mingw-w64 GCC)** です。

```
# Windows (MSYS2 UCRT64)
pacman -S mingw-w64-ucrt-x86_64-{gcc,cmake,ninja}
cmake -B build -G Ninja && cmake --build build && ctest --test-dir build

# Linux からのクロスビルド
brew install cmake ninja mingw-w64   # または各環境のパッケージマネージャ
./scripts/build.sh                   # ビルドとテストを一括実行
```

