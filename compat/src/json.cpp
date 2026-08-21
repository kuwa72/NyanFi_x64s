/**
 * @file json.cpp
 * @brief compat/json.h の実装。RFC 8259 の範囲で完結する手書き再帰下降パーサ。
 */
#include "compat/json.h"

#include <cwchar>
#include <cwctype>
#include <memory>
#include <string>

namespace {

//---------------------------------------------------------------------------
/// JSON テキスト用の文字列直列化 (エスケープ処理)。TJSONString::ToJSON が使う
UnicodeString escape_json_string(const UnicodeString &s)
{
	std::wstring out;
	out += L'"';
	const wchar_t *p = s.c_str();
	const int len = s.Length();
	for (int i = 0; i < len; ++i) {
		const wchar_t c = p[i];
		switch (c) {
		case L'"': out += L"\\\""; break;
		case L'\\': out += L"\\\\"; break;
		case L'\b': out += L"\\b"; break;
		case L'\f': out += L"\\f"; break;
		case L'\n': out += L"\\n"; break;
		case L'\r': out += L"\\r"; break;
		case L'\t': out += L"\\t"; break;
		default:
			if (c < 0x20) {
				wchar_t buf[8];
				::swprintf(buf, 8, L"\\u%04x", static_cast<unsigned>(c));
				out += buf;
			}
			else {
				out += c;
			}
		}
	}
	out += L'"';
	return UnicodeString(out);
}

//---------------------------------------------------------------------------
/// JSON パーサ本体。data 全体を保持し、カーソル位置 (0 始まり) を前進させる
class JsonParser {
public:
	explicit JsonParser(const UnicodeString &data) : text_(data.c_str()), len_(data.Length()) {}

	/// ルート値を解析する。失敗時は failed_ = true にして nullptr を返す
	TJSONValue *ParseRoot()
	{
		skip_ws();
		TJSONValue *v = parse_value();
		if (!failed_) {
			skip_ws();
			if (pos_ != len_) fail(L"JSON の末尾に余分な文字があります");
		}
		if (failed_) {
			delete v;
			return nullptr;
		}
		return v;
	}

	bool Failed() const { return failed_; }
	const UnicodeString &ErrorMessage() const { return err_msg_; }
	int ErrorLine() const { return err_line_; }
	int ErrorPos() const { return err_col_; }

private:
	const wchar_t *text_;
	int len_;
	int pos_ = 0;
	bool failed_ = false;
	UnicodeString err_msg_;
	int err_line_ = 1;
	int err_col_ = 1;

	wchar_t cur() const { return (pos_ < len_) ? text_[pos_] : L'\0'; }

	void fail(const wchar_t *msg)
	{
		if (failed_) return;	//最初のエラーを優先する
		failed_ = true;
		err_msg_ = msg;

		//1 始まりの行/列を先頭から数え直す (パフォーマンスは JSON パースの
		//失敗時のみなので簡潔さを優先している)
		err_line_ = 1;
		err_col_ = 1;
		for (int i = 0; i < pos_ && i < len_; ++i) {
			if (text_[i] == L'\n') {
				++err_line_;
				err_col_ = 1;
			}
			else {
				++err_col_;
			}
		}
	}

	void skip_ws()
	{
		while (pos_ < len_) {
			const wchar_t c = text_[pos_];
			if (c == L' ' || c == L'\t' || c == L'\r' || c == L'\n') ++pos_;
			else break;
		}
	}

	bool match(wchar_t c)
	{
		if (cur() == c) {
			++pos_;
			return true;
		}
		return false;
	}

	bool match_literal(const wchar_t *lit)
	{
		const int n = static_cast<int>(wcslen(lit));
		if (pos_ + n > len_) return false;
		for (int i = 0; i < n; ++i) {
			if (text_[pos_ + i] != lit[i]) return false;
		}
		pos_ += n;
		return true;
	}

	TJSONValue *parse_value()
	{
		if (failed_) return nullptr;
		skip_ws();
		const wchar_t c = cur();
		if (c == L'"') return parse_string_value();
		if (c == L'{') return parse_object();
		if (c == L'[') return parse_array();
		if (c == L't') {
			if (match_literal(L"true")) return new TJSONTrue();
			fail(L"'true' の解析に失敗しました");
			return nullptr;
		}
		if (c == L'f') {
			if (match_literal(L"false")) return new TJSONFalse();
			fail(L"'false' の解析に失敗しました");
			return nullptr;
		}
		if (c == L'n') {
			if (match_literal(L"null")) return new TJSONNull();
			fail(L"'null' の解析に失敗しました");
			return nullptr;
		}
		if (c == L'-' || (c >= L'0' && c <= L'9')) return parse_number();

		fail(L"予期しない文字です");
		return nullptr;
	}

	TJSONValue *parse_number()
	{
		const int start = pos_;
		if (cur() == L'-') ++pos_;
		if (cur() == L'0') {
			++pos_;
		}
		else if (cur() >= L'1' && cur() <= L'9') {
			while (cur() >= L'0' && cur() <= L'9') ++pos_;
		}
		else {
			fail(L"数値の解析に失敗しました");
			return nullptr;
		}
		if (cur() == L'.') {
			++pos_;
			if (!(cur() >= L'0' && cur() <= L'9')) {
				fail(L"小数部の解析に失敗しました");
				return nullptr;
			}
			while (cur() >= L'0' && cur() <= L'9') ++pos_;
		}
		if (cur() == L'e' || cur() == L'E') {
			++pos_;
			if (cur() == L'+' || cur() == L'-') ++pos_;
			if (!(cur() >= L'0' && cur() <= L'9')) {
				fail(L"指数部の解析に失敗しました");
				return nullptr;
			}
			while (cur() >= L'0' && cur() <= L'9') ++pos_;
		}
		return new TJSONNumber(UnicodeString(std::wstring(text_ + start, static_cast<std::size_t>(pos_ - start))));
	}

	/// 文字列リテラルの中身を解析する ("...") 。呼び出し時点で cur()=='"'
	bool parse_raw_string(std::wstring &out)
	{
		if (!match(L'"')) {
			fail(L"文字列が ' \" ' で始まっていません");
			return false;
		}
		while (true) {
			if (pos_ >= len_) {
				fail(L"文字列が閉じられていません");
				return false;
			}
			const wchar_t c = text_[pos_];
			if (c == L'"') {
				++pos_;
				return true;
			}
			if (c == L'\\') {
				++pos_;
				if (pos_ >= len_) {
					fail(L"不正なエスケープシーケンスです");
					return false;
				}
				const wchar_t e = text_[pos_];
				switch (e) {
				case L'"': out += L'"'; ++pos_; break;
				case L'\\': out += L'\\'; ++pos_; break;
				case L'/': out += L'/'; ++pos_; break;
				case L'b': out += L'\b'; ++pos_; break;
				case L'f': out += L'\f'; ++pos_; break;
				case L'n': out += L'\n'; ++pos_; break;
				case L'r': out += L'\r'; ++pos_; break;
				case L't': out += L'\t'; ++pos_; break;
				case L'u': {
					++pos_;
					if (pos_ + 4 > len_) {
						fail(L"\\u エスケープが不正です");
						return false;
					}
					unsigned code = 0;
					for (int i = 0; i < 4; ++i) {
						const wchar_t h = text_[pos_ + i];
						code <<= 4;
						if (h >= L'0' && h <= L'9') code |= static_cast<unsigned>(h - L'0');
						else if (h >= L'a' && h <= L'f') code |= static_cast<unsigned>(h - L'a' + 10);
						else if (h >= L'A' && h <= L'F') code |= static_cast<unsigned>(h - L'A' + 10);
						else {
							fail(L"\\u エスケープが不正です");
							return false;
						}
					}
					pos_ += 4;
					//サロゲートペアは 2 つの \u エスケープとして書かれるが、wchar_t が
					//16bit の Windows では各コードユニットをそのまま積むだけで UTF-16
					//として正しい表現になる (再合成は不要)
					out += static_cast<wchar_t>(code);
					break;
				}
				default:
					fail(L"不明なエスケープシーケンスです");
					return false;
				}
			}
			else {
				out += c;
				++pos_;
			}
		}
	}

	TJSONValue *parse_string_value()
	{
		std::wstring s;
		if (!parse_raw_string(s)) return nullptr;
		return new TJSONString(UnicodeString(s));
	}

	TJSONValue *parse_object()
	{
		if (!match(L'{')) {
			fail(L"'{' が必要です");
			return nullptr;
		}
		std::unique_ptr<TJSONObject> obj(new TJSONObject());
		skip_ws();
		if (match(L'}')) return obj.release();

		while (true) {
			skip_ws();
			if (cur() != L'"') {
				fail(L"オブジェクトのキーは文字列である必要があります");
				return nullptr;
			}
			std::wstring key;
			if (!parse_raw_string(key)) return nullptr;
			skip_ws();
			if (!match(L':')) {
				fail(L"':' が必要です");
				return nullptr;
			}
			skip_ws();
			TJSONValue *v = parse_value();
			if (failed_) {
				delete v;
				return nullptr;
			}
			obj->AddPair(new TJSONString(UnicodeString(key)), v);

			skip_ws();
			if (match(L',')) continue;
			if (match(L'}')) break;
			fail(L"',' または '}' が必要です");
			return nullptr;
		}
		return obj.release();
	}

	TJSONValue *parse_array()
	{
		if (!match(L'[')) {
			fail(L"'[' が必要です");
			return nullptr;
		}
		std::unique_ptr<TJSONArray> ary(new TJSONArray());
		skip_ws();
		if (match(L']')) return ary.release();

		while (true) {
			skip_ws();
			TJSONValue *v = parse_value();
			if (failed_) {
				delete v;
				return nullptr;
			}
			ary->AddItem(v);

			skip_ws();
			if (match(L',')) continue;
			if (match(L']')) break;
			fail(L"',' または ']' が必要です");
			return nullptr;
		}
		return ary.release();
	}
};

}  // namespace

//---------------------------------------------------------------------------
TJSONValue *TJSONValue::ParseJSONValue(const UnicodeString &data, bool /*useBool*/, bool raiseExc)
{
	JsonParser parser(data);
	TJSONValue *v = parser.ParseRoot();
	if (parser.Failed()) {
		if (raiseExc) throw EJSONParseException(parser.ErrorMessage(), parser.ErrorLine(), parser.ErrorPos());
		return nullptr;
	}
	return v;
}

//---------------------------------------------------------------------------
UnicodeString TJSONString::ToJSON() const
{
	return escape_json_string(value_);
}

//---------------------------------------------------------------------------
UnicodeString TJSONPair::ToJSON() const
{
	UnicodeString out = JsonString->ToJSON();
	out += ":";
	out += (JsonValue ? JsonValue->ToJSON() : UnicodeString("null"));
	return out;
}

//---------------------------------------------------------------------------
UnicodeString TJSONObject::ToJSON() const
{
	UnicodeString out = "{";
	for (std::size_t i = 0; i < Pairs.size(); ++i) {
		if (i > 0) out += ",";
		out += Pairs[i]->ToJSON();
	}
	out += "}";
	return out;
}

//---------------------------------------------------------------------------
UnicodeString TJSONArray::ToJSON() const
{
	UnicodeString out = "[";
	for (std::size_t i = 0; i < Items.size(); ++i) {
		if (i > 0) out += ",";
		out += Items[i]->ToJSON();
	}
	out += "]";
	return out;
}
