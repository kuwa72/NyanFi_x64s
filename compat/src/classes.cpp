/**
 * @file compat/src/classes.cpp
 * @brief compat/classes.h の実装
 */
#include "compat/classes.h"

#include <cxxabi.h>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

#include "compat/encoding.h"
#include "compat/streams.h"
#include "compat/sysutils.h"

//===========================================================================
// TObject
//===========================================================================
UnicodeString TObject::ClassName() const
{
	// GCC (Itanium ABI) の typeid().name() をデマングルして実行時のクラス名を得る。
	// 実測では TStringList 系に対する ClassName/ClassNameIs の直接呼び出しは
	// 見つからなかった (ClassNameIs は主に VCL コントロール向け) が、契約として実装する。
	int status = 0;
	const char *mangled = typeid(*this).name();
	char *demangled = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
	std::string name = (status == 0 && demangled != nullptr) ? demangled : mangled;
	if (demangled) std::free(demangled);

	// "System::Classes::TStringList" のような名前空間修飾は末尾要素だけ残す
	const auto pos = name.rfind("::");
	if (pos != std::string::npos) name = name.substr(pos + 2);

	const std::wstring wname(name.begin(), name.end());
	return UnicodeString(wname);
}

bool TObject::ClassNameIs(const UnicodeString &name) const { return SameText(ClassName(), name); }

//===========================================================================
// TPersistent
//===========================================================================
void TPersistent::Assign(TPersistent * /*source*/)
{
	// 既定の TPersistent::Assign は「代入不可」を意味する。TStrings 側で
	// 具体的な代入をオーバーライドしているので、ここに来るのは想定外の型同士。
	throw Exception(UnicodeString(L"この型同士の Assign はサポートしていません"));
}

//===========================================================================
// 無名名前空間: TStrings 実装用の内部ヘルパ
//===========================================================================
namespace {

/// 制御文字/空白 (Delphi の #1..' ' 相当)。'\0' はここには含めない。
bool IsBlankChar(wchar_t c) { return c != 0 && c <= L' '; }

/**
 * @brief CommaText / DelimitedText の生成。QuoteChar==0 なら引用処理をしない。
 * @param strict true (StrictDelimiter=true) なら Delimiter/QuoteChar/空文字列だけを見て
 *               引用要否を決める。false (既定、CommaText は常にこちら) なら埋め込みの
 *               空白/制御文字も引用要否の判定に含める (Delphi の非 StrictDelimiter 挙動)。
 */
UnicodeString EncodeDelimited(const std::vector<UnicodeString> &texts, wchar_t delim, wchar_t quote, bool strict)
{
	UnicodeString result;
	const int count = static_cast<int>(texts.size());
	for (int i = 0; i < count; ++i) {
		UnicodeString field = texts[static_cast<std::size_t>(i)];
		bool needQuote = false;
		if (quote != 0) {
			if (field.IsEmpty()) needQuote = true;  // 空文字列も区別できるよう引用する
			for (int k = 1; k <= field.Length() && !needQuote; ++k) {
				const wchar_t c = field[k];
				if (c == delim || c == quote) needQuote = true;
				else if (!strict && IsBlankChar(c)) needQuote = true;
			}
		}
		if (needQuote) {
			UnicodeString escaped;
			escaped += quote;
			for (int k = 1; k <= field.Length(); ++k) {
				const wchar_t c = field[k];
				if (c == quote) escaped += quote;  // " -> "" のように 2 重化
				escaped += c;
			}
			escaped += quote;
			field = escaped;
		}
		result += field;
		if (i < count - 1) result += delim;
	}
	return result;
}

/**
 * @brief CommaText / DelimitedText の分解。QuoteChar==0 なら引用処理をしない。
 * @param strict true (StrictDelimiter=true) なら Delimiter のみをフィールド区切りとする
 *               (実測: usr_str.cpp の get_csv_array/get_csv_item はこちらを使い、
 *               引用符で囲まれていない値に埋め込まれた空白をそのまま保持する)。
 *               false (既定、CommaText は常にこちら) なら Delphi の非 StrictDelimiter
 *               実装に合わせ、引用符で囲まれていないフィールドの前後の空白/制御文字も
 *               区切りとして読み飛ばす。
 */
std::vector<UnicodeString> DecodeDelimited(const UnicodeString &value, wchar_t delim, wchar_t quote, bool strict)
{
	std::vector<UnicodeString> out;
	const wchar_t *p = value.c_str();
	if (*p == 0) return out;  // 空文字列は要素 0 件 (Delphi の while P^<>#0 ガードと同じ)

	if (strict) {
		while (true) {
			UnicodeString field;
			if (quote != 0 && *p == quote) {
				++p;
				while (*p) {
					if (*p == quote) {
						if (*(p + 1) == quote) {
							field += quote;
							p += 2;
							continue;
						}
						++p;
						break;
					}
					field += *p++;
				}
				while (*p && *p != delim) ++p;  // 閉じ引用符の後、delim までの余りは読み飛ばす
			}
			else {
				while (*p && *p != delim) field += *p++;
			}
			out.push_back(field);
			if (*p == delim) {
				++p;
				continue;
			}
			break;
		}
		return out;
	}

	// 非 strict (Delphi の既定挙動): フィールドの前後の空白/制御文字を区切りとみなす
	while (IsBlankChar(*p)) ++p;
	while (*p) {
		UnicodeString field;
		if (quote != 0 && *p == quote) {
			++p;
			while (*p) {
				if (*p == quote) {
					if (*(p + 1) == quote) {
						field += quote;
						p += 2;
						continue;
					}
					++p;
					break;
				}
				field += *p++;
			}
		}
		else {
			while (*p && !IsBlankChar(*p) && *p != delim) field += *p++;
		}
		out.push_back(field);

		while (IsBlankChar(*p)) ++p;  // フィールド直後の空白を読み飛ばす
		if (*p == delim) {
			++p;
			while (IsBlankChar(*p)) ++p;  // delim 直後の空白も読み飛ばす
			if (*p == 0) out.push_back(UnicodeString());  // 末尾が delim なら空文字列を追加
		}
		else {
			break;  // delim でも終端でもなければ (不正な形式) ここで打ち切る
		}
	}
	return out;
}

/// TStringList::Sort / CustomSort が共有するインデックスベースのクイックソート
/// (Delphi の Classes.pas 実装と同じ手順)
void QuickSortRange(TStringList *list, int lo, int hi, const std::function<int(int, int)> &compare)
{
	if (lo >= hi) return;
	int i = lo, j = hi;
	int pivot = (lo + hi) >> 1;
	do {
		while (compare(i, pivot) < 0) ++i;
		while (compare(j, pivot) > 0) --j;
		if (i <= j) {
			if (i != j) list->Exchange(i, j);
			if (pivot == i)
				pivot = j;
			else if (pivot == j)
				pivot = i;
			++i;
			--j;
		}
	} while (i <= j);
	if (lo < j) QuickSortRange(list, lo, j, compare);
	if (i < hi) QuickSortRange(list, i, hi, compare);
}

}  // namespace

//===========================================================================
// TStrings
//===========================================================================
TStrings::TStrings() : line_break_(sLineBreak) {}

TStrings::~TStrings() = default;  // Objects の所有権は解放しない (VCL と同じ、実測どおり)

//---------------------------------------------------------------------------
int TStrings::Add(const UnicodeString &s)
{
	items_.push_back(Item{s, nullptr});
	return static_cast<int>(items_.size()) - 1;
}

int TStrings::AddObject(const UnicodeString &s, TObject *obj)
{
	const int idx = Add(s);
	items_[static_cast<std::size_t>(idx)].object = obj;
	return idx;
}

void TStrings::AddStrings(TStrings *strings)
{
	if (!strings) return;
	const int n = strings->GetCount();
	for (int i = 0; i < n; ++i) AddObject(strings->StringAt(i), strings->ObjectAt(i));
}

void TStrings::Insert(int index, const UnicodeString &s)
{
	items_.insert(items_.begin() + index, Item{s, nullptr});
}

void TStrings::InsertObject(int index, const UnicodeString &s, TObject *obj)
{
	Insert(index, s);
	items_[static_cast<std::size_t>(index)].object = obj;
}

void TStrings::Delete(int index) { items_.erase(items_.begin() + index); }

void TStrings::Clear() { items_.clear(); }

void TStrings::Exchange(int index1, int index2)
{
	std::swap(items_[static_cast<std::size_t>(index1)], items_[static_cast<std::size_t>(index2)]);
}

void TStrings::Move(int curIndex, int newIndex)
{
	if (curIndex == newIndex) return;
	Item tmp = items_[static_cast<std::size_t>(curIndex)];
	items_.erase(items_.begin() + curIndex);
	items_.insert(items_.begin() + newIndex, tmp);
}

void TStrings::Assign(TPersistent *source)
{
	TStrings *src = dynamic_cast<TStrings *>(source);
	if (!src) {
		TPersistent::Assign(source);
		return;
	}
	Clear();
	AddStrings(src);
}

//---------------------------------------------------------------------------
int TStrings::IndexOf(const UnicodeString &s) const
{
	const int n = GetCount();
	for (int i = 0; i < n; ++i)
		if (CompareStrings(items_[static_cast<std::size_t>(i)].text, s) == 0) return i;
	return -1;
}

int TStrings::IndexOfName(const UnicodeString &name) const
{
	const int n = GetCount();
	for (int i = 0; i < n; ++i)
		if (CompareStrings(NameAt(i), name) == 0) return i;
	return -1;
}

int TStrings::IndexOfObject(TObject *obj) const
{
	const int n = GetCount();
	for (int i = 0; i < n; ++i)
		if (items_[static_cast<std::size_t>(i)].object == obj) return i;
	return -1;
}

//---------------------------------------------------------------------------
void TStrings::LoadFromFile(const UnicodeString &fileName) { LoadFromFile(fileName, nullptr); }

void TStrings::LoadFromFile(const UnicodeString &fileName, TEncoding *encoding)
{
	TFileStream fs(fileName, fmOpenRead | fmShareDenyNone);
	LoadFromStream(&fs, encoding);
}

void TStrings::SaveToFile(const UnicodeString &fileName) const { SaveToFile(fileName, nullptr); }

void TStrings::SaveToFile(const UnicodeString &fileName, TEncoding *encoding) const
{
	TFileStream fs(fileName, fmCreate);
	SaveToStream(&fs, encoding);
}

void TStrings::LoadFromStream(TStream *stream) { LoadFromStream(stream, nullptr); }

void TStrings::LoadFromStream(TStream *stream, TEncoding *encoding)
{
	const Int64 remain = stream->GetSize() - stream->GetPosition();
	TBytes buf;
	if (remain > 0) {
		buf.Length = static_cast<int>(remain);
		stream->ReadBuffer(buf, static_cast<int>(remain));
	}

	// 実測どおり: BOM があれば UTF-8/UTF-16 として読み、無ければ ANSI(既定コードページ) とする。
	// これは明示的に enc を指定した呼び出しに対しても優先される (実際の RTL の GetBufferEncoding 仕様)。
	TEncoding *resolved = encoding;
	const int skip = TEncoding::GetBufferEncoding(buf, resolved, TEncoding::Default);

	BeginUpdate();
	SetText(resolved->GetString(buf, skip, static_cast<int>(remain) - skip));
	EndUpdate();

	AdoptEncoding(resolved);
}

void TStrings::SaveToStream(TStream *stream) const { SaveToStream(stream, encoding_ ? encoding_ : TEncoding::Default); }

void TStrings::SaveToStream(TStream *stream, TEncoding *encoding) const
{
	TEncoding *enc = encoding ? encoding : (encoding_ ? encoding_ : TEncoding::Default);
	if (write_bom_) {
		const TBytes pre = enc->GetPreamble();
		if (pre.Length > 0) stream->WriteBuffer(pre, pre.Length);
	}
	const TBytes bytes = enc->GetBytes(GetText());
	if (bytes.Length > 0) stream->WriteBuffer(bytes, bytes.Length);
}

void TStrings::BeginUpdate() { ++update_count_; }
void TStrings::EndUpdate()
{
	if (update_count_ > 0) --update_count_;
}

//---------------------------------------------------------------------------
int TStrings::GetCount() const { return static_cast<int>(items_.size()); }

UnicodeString TStrings::GetText() const
{
	UnicodeString result;
	for (const auto &item : items_) {
		result += item.text;
		result += line_break_;
	}
	return result;
}

void TStrings::SetText(const UnicodeString &value)
{
	BeginUpdate();
	Clear();
	const wchar_t *p = value.c_str();
	const wchar_t *start = p;
	while (*p) {
		if (*p == L'\r' || *p == L'\n') {
			Add(UnicodeString(start, static_cast<int>(p - start)));
			if (*p == L'\r' && *(p + 1) == L'\n') ++p;  // CRLF はまとめて 1 区切り
			++p;
			start = p;
		}
		else {
			++p;
		}
	}
	if (p != start) Add(UnicodeString(start, static_cast<int>(p - start)));
	EndUpdate();
}

UnicodeString TStrings::GetCommaText() const
{
	// CommaText は StrictDelimiter に関わらず常に非 strict (Delphi の実装どおり)
	std::vector<UnicodeString> texts;
	texts.reserve(items_.size());
	for (const auto &it : items_) texts.push_back(it.text);
	return EncodeDelimited(texts, L',', L'"', /*strict=*/false);
}

void TStrings::SetCommaText(const UnicodeString &value)
{
	BeginUpdate();
	Clear();
	for (const auto &f : DecodeDelimited(value, L',', L'"', /*strict=*/false)) Add(f);
	EndUpdate();
}

UnicodeString TStrings::GetDelimitedText() const
{
	std::vector<UnicodeString> texts;
	texts.reserve(items_.size());
	for (const auto &it : items_) texts.push_back(it.text);
	return EncodeDelimited(texts, delimiter_, quote_char_, strict_delimiter_);
}

void TStrings::SetDelimitedText(const UnicodeString &value)
{
	BeginUpdate();
	Clear();
	for (const auto &f : DecodeDelimited(value, delimiter_, quote_char_, strict_delimiter_)) Add(f);
	EndUpdate();
}

wchar_t TStrings::GetDelimiter() const { return delimiter_; }
void TStrings::SetDelimiter(wchar_t value) { delimiter_ = value; }
wchar_t TStrings::GetNameValueSeparator() const { return name_value_separator_; }
void TStrings::SetNameValueSeparator(wchar_t value) { name_value_separator_ = value; }
wchar_t TStrings::GetQuoteChar() const { return quote_char_; }
void TStrings::SetQuoteChar(wchar_t value) { quote_char_ = value; }
UnicodeString TStrings::GetLineBreak() const { return line_break_; }
void TStrings::SetLineBreak(const UnicodeString &value) { line_break_ = value; }

//---------------------------------------------------------------------------
UnicodeString &TStrings::StringAt(int index) { return items_[static_cast<std::size_t>(index)].text; }
const UnicodeString &TStrings::StringAt(int index) const { return items_[static_cast<std::size_t>(index)].text; }
TObject *&TStrings::ObjectAt(int index) { return items_[static_cast<std::size_t>(index)].object; }

UnicodeString TStrings::NameAt(int index) const
{
	const UnicodeString &s = items_[static_cast<std::size_t>(index)].text;
	const int p = s.Pos(name_value_separator_);
	if (p == 0) return UnicodeString();
	return s.SubString(1, p - 1);
}

UnicodeString TStrings::ValueAt(int index) const
{
	const UnicodeString &s = items_[static_cast<std::size_t>(index)].text;
	const int p = s.Pos(name_value_separator_);
	if (p == 0) return UnicodeString();
	return s.SubString(p + 1);
}

void TStrings::SetValueAt(int index, const UnicodeString &value)
{
	if (!value.IsEmpty()) {
		StringAt(index) = NameAt(index) + name_value_separator_ + value;
	}
	else {
		Delete(index);
	}
}

UnicodeString TStrings::ValueOf(const UnicodeString &name) const
{
	const int idx = IndexOfName(name);
	if (idx < 0) return UnicodeString();
	return ValueAt(idx);
}

void TStrings::SetValue(const UnicodeString &name, const UnicodeString &value)
{
	// 実測 RTL 挙動: 新規追加時はもちろん、既存行を書き換える場合も
	// (SetValueAt のように) 現在の行から再抽出した Name ではなく、引数の
	// name をそのまま使って再構成する。新規追加した空プレースホルダ行には
	// そもそも Name が無い (NameAt は空文字列を返す) ため、SetValueAt には
	// 委譲できないことに注意。
	int idx = IndexOfName(name);
	if (!value.IsEmpty()) {
		if (idx < 0) idx = Add(UnicodeString());
		StringAt(idx) = name + name_value_separator_ + value;
	}
	else if (idx >= 0) {
		Delete(idx);
	}
}

//---------------------------------------------------------------------------
TStrings::StringRef &TStrings::StringRef::operator=(const UnicodeString &value)
{
	if (index_ >= 0)
		owner_->SetValueAt(index_, value);
	else
		owner_->SetValue(name_, value);
	UnicodeString::operator=(value);
	return *this;
}

//---------------------------------------------------------------------------
int TStrings::CompareStrings(const UnicodeString &a, const UnicodeString &b) const { return CompareText(a, b); }

void TStrings::AdoptEncoding(TEncoding *encoding)
{
	if (!encoding) {
		owned_encoding_.reset();
		encoding_ = nullptr;
		return;
	}
	const bool isStatic = (encoding == TEncoding::UTF8 || encoding == TEncoding::Unicode ||
	                        encoding == TEncoding::BigEndianUnicode || encoding == TEncoding::ANSI ||
	                        encoding == TEncoding::Default || encoding == TEncoding::ASCII);
	if (isStatic) {
		owned_encoding_.reset();
		encoding_ = encoding;
	}
	else {
		// 呼び出し元が `std::unique_ptr<TEncoding>` で即座に delete する形が多いため、
		// 生ポインタをそのまま保持せず複製して所有権を持つ (解放後アクセス対策)。
		owned_encoding_.reset(new TEncoding(encoding->GetCodePage()));
		encoding_ = owned_encoding_.get();
	}
}

//===========================================================================
// TStringList
//===========================================================================
TStringList::TStringList() = default;
TStringList::~TStringList() = default;

int TStringList::Add(const UnicodeString &s)
{
	if (!sorted_) return TStrings::Add(s);

	int idx = 0;
	bool found = false;
	int lo = 0, hi = GetCount() - 1;
	while (lo <= hi) {
		const int mid = (lo + hi) >> 1;
		const int c = CompareStrings(StringAt(mid), s);
		if (c < 0) {
			lo = mid + 1;
		}
		else {
			hi = mid - 1;
			if (c == 0) found = true;
		}
	}
	idx = lo;

	if (found) {
		if (duplicates_ == dupIgnore) return idx;
		if (duplicates_ == dupError) throw EStringListError(UnicodeString(L"リストにはすでに同じ文字列があります"));
		// dupAccept: そのまま挿入する
	}
	TStrings::Insert(idx, s);
	return idx;
}

void TStringList::Insert(int index, const UnicodeString &s)
{
	if (sorted_) throw EStringListError(UnicodeString(L"Sorted なリストへ直接 Insert することはできません"));
	TStrings::Insert(index, s);
}

void TStringList::Sort()
{
	QuickSortRange(this, 0, GetCount() - 1, [this](int a, int b) { return CompareStrings(StringAt(a), StringAt(b)); });
}

void TStringList::CustomSort(TStringListSortCompare compare)
{
	if (!compare) return;
	QuickSortRange(this, 0, GetCount() - 1, [this, compare](int a, int b) { return compare(this, a, b); });
}

bool TStringList::GetSorted() const { return sorted_; }
void TStringList::SetSorted(bool value)
{
	if (sorted_ != value) {
		if (value) Sort();
		sorted_ = value;
	}
}

bool TStringList::GetCaseSensitive() const { return case_sensitive_; }
void TStringList::SetCaseSensitive(bool value) { case_sensitive_ = value; }

TDuplicates TStringList::GetDuplicates() const { return duplicates_; }
void TStringList::SetDuplicates(TDuplicates value) { duplicates_ = value; }

int TStringList::CompareStrings(const UnicodeString &a, const UnicodeString &b) const
{
	return case_sensitive_ ? CompareStr(a, b) : CompareText(a, b);
}

//===========================================================================
// TList
//===========================================================================
TList::~TList()
{
	// Clear() を呼んでおくが、ここは基底クラスのデストラクタ本体なので
	// Notify() の仮想ディスパッチは TList::Notify (既定では何もしない) にしか
	// 届かない (ヘッダのコメント参照)。Delete/Remove/Clear をオブジェクトの
	// 寿命中に明示的に呼ぶ経路では正しく最派生クラスの Notify に届く。
	Clear();
}

int TList::Add(void *item)
{
	items_.push_back(item);
	const int index = static_cast<int>(items_.size()) - 1;
	Notify(item, lnAdded);
	return index;
}

void TList::Insert(int index, void *item)
{
	items_.insert(items_.begin() + index, item);
	Notify(item, lnAdded);
}

void TList::Delete(int index)
{
	void *item = items_[static_cast<std::size_t>(index)];
	items_.erase(items_.begin() + index);
	Notify(item, lnDeleted);
}

int TList::Remove(void *item)
{
	const int index = IndexOf(item);
	if (index >= 0) Delete(index);
	return index;
}

void *TList::Extract(void *item)
{
	const int index = IndexOf(item);
	if (index < 0) return nullptr;
	void *value = items_[static_cast<std::size_t>(index)];
	items_.erase(items_.begin() + index);
	Notify(value, lnExtracted);
	return value;
}

void TList::Clear()
{
	// Delphi の TList.Clear と同じく、末尾から 1 件ずつ Notify(lnDeleted) して破棄する
	while (!items_.empty()) Delete(static_cast<int>(items_.size()) - 1);
}

int TList::IndexOf(void *item) const
{
	for (std::size_t i = 0; i < items_.size(); ++i)
		if (items_[i] == item) return static_cast<int>(i);
	return -1;
}

void TList::SetCount(int value)
{
	if (value < 0) value = 0;
	while (static_cast<int>(items_.size()) > value) Delete(static_cast<int>(items_.size()) - 1);
	while (static_cast<int>(items_.size()) < value) items_.push_back(nullptr);  // Delphi 同様、埋める分は Notify しない
}

void TList::Notify(void * /*ptr*/, TListNotification /*action*/)
{
	// 既定では何もしない (TDropTargetList などがオーバーライドして所有権解放に使う)
}

//---------------------------------------------------------------------------
// TMultiReadExclusiveWriteSynchronizer
//---------------------------------------------------------------------------
TMultiReadExclusiveWriteSynchronizer::TMultiReadExclusiveWriteSynchronizer()
{
	::InitializeSRWLock(&lock_);
}

TMultiReadExclusiveWriteSynchronizer::~TMultiReadExclusiveWriteSynchronizer() = default;

void TMultiReadExclusiveWriteSynchronizer::BeginRead()
{
	::AcquireSRWLockShared(&lock_);
}

void TMultiReadExclusiveWriteSynchronizer::EndRead()
{
	::ReleaseSRWLockShared(&lock_);
}

/// @return bool Delphi 互換で常に true (取得できるまで待つ)
bool TMultiReadExclusiveWriteSynchronizer::BeginWrite()
{
	::AcquireSRWLockExclusive(&lock_);
	return true;
}

void TMultiReadExclusiveWriteSynchronizer::EndWrite()
{
	::ReleaseSRWLockExclusive(&lock_);
}
