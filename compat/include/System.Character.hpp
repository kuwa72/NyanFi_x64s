/// C++Builder の System.Character.hpp に対応する互換ヘッダ
///
/// `TCharacter` の静的メソッド群 (IsDigit / IsLetter など) を提供するユニット。
/// src 側では Global.cpp / MainFrm.cpp が include しているが、**実際には
/// TCharacter を1箇所も使っていない** (grep で確認)。C++Builder のプロジェクト
/// テンプレートが付けた include が残っているだけなので、中身は空でよい。
/// 使う箇所が出てきたらここに足す。
#ifndef NYANFI_FWD_SYSTEM_CHARACTER_HPP
#define NYANFI_FWD_SYSTEM_CHARACTER_HPP

#endif
