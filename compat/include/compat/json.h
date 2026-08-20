/**
 * @file compat/json.h
 * @brief System.JSON (TJSONValue 系) 互換シム
 *
 * usr_str.cpp の format_Json / get_JsonValStr、JsonView.cpp、MainFrm.cpp
 * (CheckUpdateActionExecute) の実際の呼び出し箇所から必要な API を洗い出した。
 *
 * 使用形:
 *   val->ClassNameIs("TJSONTrue"|"TJSONFalse"|"TJSONNull"|"TJSONString"|
 *                     "TJSONObject"|"TJSONArray")
 *   val->Value() / val->ToJSON()
 *   obj->Count / obj->Pairs[i] (→TJSONPair*) / obj->GetValue(name)
 *   pair->JsonString (→TJSONString*) / pair->JsonValue (→TJSONValue*) / pair->ToJSON()
 *   ary->Count / ary->Size() / ary->Items[i] (→TJSONValue*)
 *   TJSONObject::ParseJSONValue(text, UseBool=false, RaiseExc=false)
 *   catch (EJSONParseException &e) { e.Message; e.Line; e.Position; }
 *
 * パーサ (ParseJSONValue) は src/ (JsonView.cpp, TxtViewer.cpp, MainFrm.cpp) から
 * 実際に呼ばれているため、RFC 8259 範囲の手書き再帰下降パーサを実装する。
 * \u エスケープはサロゲートペアも含め、コードユニットをそのまま UTF-16 の
 * UnicodeString へ積むだけでよい (wchar_t が 16bit の Windows 前提なので
 * ペアを明示的に合成する必要はない)。
 *
 * 既知の推測箇所 (実データでの検証はできていない):
 *   - TJSONTrue::Value()/TJSONFalse::Value() の具体的な文字列 ("True"/"False")
 *     は get_JsonValStr 側で ClassNameIs 分岐により迂回されるため src/ からは
 *     未検証。ここでは "True"/"False" (Delphi の Boolean.ToString 互換) とした。
 */
#ifndef NYANFI_COMPAT_JSON_H
#define NYANFI_COMPAT_JSON_H

#include <vector>

#include "compat/classes.h"
#include "compat/config.h"
#include "compat/exception.h"
#include "compat/ustring.h"

//---------------------------------------------------------------------------
/// JSON パース失敗時の例外 (EJSONParseException 相当)
class EJSONParseException : public Exception {
public:
	EJSONParseException(const UnicodeString &msg, int line, int position)
		: Exception(msg), Line(line), Position(position) {}

	int Line;		//!< 1 始まりの行番号
	int Position;	//!< 1 始まりの列番号 (その行内での文字位置)
};

//---------------------------------------------------------------------------
/// TJSONValue 相当。全 JSON 値の基底
class TJSONValue : public TObject {
public:
	~TJSONValue() override = default;

	/// この値の文字列表現 (TJSONString は中身、TJSONNumber は数値の文字列など)
	virtual UnicodeString Value() const = 0;
	/// JSON テキストとして直列化する (整形無しのコンパクト形式)
	virtual UnicodeString ToJSON() const = 0;

	/**
	 * @brief JSON テキストを解析する (System.JSON::TJSONObject::ParseJSONValue 相当)
	 * @param data JSON テキスト
	 * @param useBool true の場合 true/false を区別しない TJSONBool を使う予定だが、
	 *                src/ での呼び出しは全て useBool=false (既定値) であり
	 *                TJSONTrue/TJSONFalse を前提にした ClassNameIs 判定をしている
	 *                ため、Phase 0 では useBool=true は未対応 (false 相当で動作する)
	 * @param raiseExc true ならパース失敗時に EJSONParseException を送出する。
	 *                 false (既定) なら nullptr を返す
	 * @return 解析結果のルート値。所有権は呼び出し側に移る (delete が必要)
	 */
	static TJSONValue *ParseJSONValue(const UnicodeString &data, bool useBool = false, bool raiseExc = false);
};

//---------------------------------------------------------------------------
/// TJSONString 相当
class TJSONString : public TJSONValue {
public:
	explicit TJSONString(const UnicodeString &value) : value_(value) {}

	UnicodeString Value() const override { return value_; }
	UnicodeString ToJSON() const override;
	UnicodeString ClassName() const override { return "TJSONString"; }

private:
	UnicodeString value_;
};

//---------------------------------------------------------------------------
/// TJSONNumber 相当。値は元の文字表現をそのまま保持する
class TJSONNumber : public TJSONValue {
public:
	explicit TJSONNumber(const UnicodeString &raw) : raw_(raw) {}

	UnicodeString Value() const override { return raw_; }
	double AsDouble() const { return raw_.ToDouble(); }	//!< シム独自の補助 API
	UnicodeString ToJSON() const override { return raw_; }
	UnicodeString ClassName() const override { return "TJSONNumber"; }

private:
	UnicodeString raw_;
};

//---------------------------------------------------------------------------
/// TJSONTrue 相当
class TJSONTrue : public TJSONValue {
public:
	UnicodeString Value() const override { return "True"; }
	UnicodeString ToJSON() const override { return "true"; }
	UnicodeString ClassName() const override { return "TJSONTrue"; }
};

/// TJSONFalse 相当
class TJSONFalse : public TJSONValue {
public:
	UnicodeString Value() const override { return "False"; }
	UnicodeString ToJSON() const override { return "false"; }
	UnicodeString ClassName() const override { return "TJSONFalse"; }
};

/// TJSONNull 相当
class TJSONNull : public TJSONValue {
public:
	UnicodeString Value() const override { return UnicodeString(); }
	UnicodeString ToJSON() const override { return "null"; }
	UnicodeString ClassName() const override { return "TJSONNull"; }
};

//---------------------------------------------------------------------------
/// TJSONPair 相当。TJSONObject の 1 エントリ ("key": value)
class TJSONPair : public TObject {
public:
	TJSONPair(TJSONString *key, TJSONValue *value) : JsonString(key), JsonValue(value) {}
	~TJSONPair() override
	{
		delete JsonString;
		delete JsonValue;
	}
	TJSONPair(const TJSONPair &) = delete;
	TJSONPair &operator=(const TJSONPair &) = delete;

	UnicodeString ToJSON() const;
	UnicodeString ClassName() const override { return "TJSONPair"; }

	TJSONString *JsonString;	//!< キー
	TJSONValue *JsonValue;		//!< 値
};

//---------------------------------------------------------------------------
/// TJSONObject 相当
class TJSONObject : public TJSONValue {
public:
	~TJSONObject() override
	{
		for (TJSONPair *p : Pairs) delete p;
	}

	/// 追加 (パーサ専用。シム独自 API)
	void AddPair(TJSONString *key, TJSONValue *value)
	{
		Pairs.push_back(new TJSONPair(key, value));
		Count = static_cast<int>(Pairs.size());
	}

	/// キー名から値を検索する (無ければ nullptr)。ネストした JSONPath には非対応
	TJSONValue *GetValue(const UnicodeString &name) const
	{
		for (TJSONPair *p : Pairs) {
			if (p->JsonString && p->JsonString->Value() == name) return p->JsonValue;
		}
		return nullptr;
	}

	UnicodeString Value() const override { return UnicodeString(); }	//!< Object 自体は未使用 (呼び出し箇所無し)
	UnicodeString ToJSON() const override;
	UnicodeString ClassName() const override { return "TJSONObject"; }

	int Count = 0;					//!< 要素数 (obj->Count で参照)
	std::vector<TJSONPair *> Pairs;	//!< obj->Pairs[i] で参照 (std::vector が operator[] を提供)
};

//---------------------------------------------------------------------------
/// TJSONArray 相当
class TJSONArray : public TJSONValue {
public:
	~TJSONArray() override
	{
		for (TJSONValue *v : Items) delete v;
	}

	/// 追加 (パーサ専用。シム独自 API)
	void AddItem(TJSONValue *value)
	{
		Items.push_back(value);
		Count = static_cast<int>(Items.size());
	}

	int Size() const { return Count; }	//!< MainFrm.cpp が ->Size() で呼ぶ (Count の別名)

	UnicodeString Value() const override { return UnicodeString(); }	//!< Array 自体は未使用 (呼び出し箇所無し)
	UnicodeString ToJSON() const override;
	UnicodeString ClassName() const override { return "TJSONArray"; }

	int Count = 0;					//!< 要素数 (ary->Count で参照)
	std::vector<TJSONValue *> Items;	//!< ary->Items[i] で参照
};

namespace System {
namespace JSON {
using ::EJSONParseException;
using ::TJSONArray;
using ::TJSONFalse;
using ::TJSONNull;
using ::TJSONNumber;
using ::TJSONObject;
using ::TJSONPair;
using ::TJSONString;
using ::TJSONTrue;
using ::TJSONValue;
}  // namespace JSON
using namespace JSON;
}  // namespace System

namespace JSON = System::JSON;

#endif  // NYANFI_COMPAT_JSON_H
