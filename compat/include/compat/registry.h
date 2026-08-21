/**
 * @file compat/registry.h
 * @brief System.Win.Registry (TRegistry) 互換シム
 *
 * 実際に使われているのは usr_wic.cpp / usr_file_ex.cpp / UserMdl.cpp /
 * Global.cpp の 4 箇所で、いずれも RootKey / OpenKeyReadOnly / CloseKey /
 * ReadString / CurrentPath / GetKeyNames のみ (実測: OpenKeyReadOnly 4 /
 * CloseKey 4 / ReadString 4 相当、CurrentPath と GetKeyNames は usr_wic.cpp で
 * 1 回ずつ)。ReadInteger / ValueExists / KeyExists は TRegistry では未使用
 * (これらの呼び出しは全て TIniFile 相当のオブジェクト (IniFile / cfg_file 等)
 * に対するもので、TRegistry には出現しないことを grep で確認済み)。
 * ただし API としての完全性のため、コスト・リスクの低い範囲でこれらも
 * 追加実装した。
 *
 * Win32 の RegOpenKeyExW / RegQueryValueExW / RegEnumKeyExW で実装する。
 */
#ifndef NYANFI_COMPAT_REGISTRY_H
#define NYANFI_COMPAT_REGISTRY_H

#include "compat/classes.h"
#include "compat/config.h"
#include "compat/property.h"
#include "compat/ustring.h"

/**
 * @brief System.Win.Registry::TRegistry 相当
 * @details 読み取り専用の用途のみを実装している (src/ での実測に基づく)。
 *          書き込み系 API (WriteString 等) は src/ で未使用のため実装していない。
 */
class TRegistry : public TObject {
public:
	TRegistry();
	~TRegistry() override;
	TRegistry(const TRegistry &) = delete;
	TRegistry &operator=(const TRegistry &) = delete;

	HKEY GetRootKey() const { return root_key_; }
	void SetRootKey(HKEY key) { root_key_ = key; }

	/// 指定キーを読み取り専用で開く。成功時 true
	bool OpenKeyReadOnly(const UnicodeString &key);
	/// 開いているキーを閉じる
	void CloseKey();

	/// 値を文字列として読む (REG_SZ/REG_EXPAND_SZ/REG_DWORD を許容する)
	UnicodeString ReadString(const UnicodeString &name) const;
	/// 値を整数として読む (REG_DWORD/REG_SZ を許容する)
	int ReadInteger(const UnicodeString &name) const;
	/// 値が存在するか
	bool ValueExists(const UnicodeString &name) const;
	/// サブキーが存在するか (現在開いているキーからの相対パス)
	bool KeyExists(const UnicodeString &subKey) const;
	/// 開いているキー配下のサブキー名一覧を取得する (usr_wic.cpp が使用)
	void GetKeyNames(TStringList *list) const;

	compat::RWValueProperty<TRegistry, HKEY, &TRegistry::GetRootKey, &TRegistry::SetRootKey> RootKey{this};
	/// 現在開いているキーのフルパス (usr_wic.cpp が参照)
	UnicodeString CurrentPath;

private:
	HKEY root_key_ = HKEY_CURRENT_USER;
	HKEY cur_key_ = nullptr;
};

namespace System {
namespace Win {
namespace Registry {
using ::TRegistry;
}  // namespace Registry
using namespace Registry;
}  // namespace Win
}  // namespace System

#endif  // NYANFI_COMPAT_REGISTRY_H
