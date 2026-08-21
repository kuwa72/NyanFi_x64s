/**
 * @file usr_mmfile.h
 * @brief メモリマップドファイル
 */
//---------------------------------------------------------------------------
#ifndef UsrMMFileH
#define UsrMMFileH

//---------------------------------------------------------------------------
#define MAX_MEMMAP_SIZE	1073741824L		//!< 最大マップサイズ		   (1GB)
#define FAILED_BUF_SIZE	8388608L		//!< マップ失敗時の読込サイズ (8MB)

//---------------------------------------------------------------------------
/**
 * @brief メモリマップドファイル
 */
class MemMapFile
{
private:
	HANDLE hFile;
	HANDLE hMap;
	BYTE *pPointer;		//マップ用ポインタ

	TBytes ByteBuff;	//通常読込用バッファ

	void Initialize();	//初期化

	BYTE Get(unsigned int Index)
	{
		if (isMaped) {
			if (!pPointer || Index>=FileSize) return 0;
			BYTE *p = pPointer;
			p += Index;
			return *p;
		}
		else
			return (Index<BuffSize)? ByteBuff[Index] : 0;
	}

public:
	/**
	 * @brief Bytes[Index] の読み取り
	 * @details 旧 `__property BYTE Bytes[unsigned int Index] = {read=Get};`
	 *          C++Builder 拡張のプロパティ構文は clang-cl / mingw-w64 では通らないため、
	 *          呼び出し形 (`mmf->Bytes[i]`) を変えずに済む添字プロキシへ置き換えた
	 *          (issue #1 Phase 0)。標準C++のみで書いてあるので BCC64 でもそのまま通る。
	 */
	class TBytesProperty
	{
	public:
		explicit TBytesProperty(MemMapFile *owner) : Owner(owner) {}

		BYTE operator[](unsigned int index) const { return Owner->Get(index); }

	private:
		MemMapFile *Owner;
	};

	TBytesProperty Bytes{this};

	__int64 FileSize;		//!< ファイルサイズ
	unsigned int BuffSize;	//!< バッファ(マップ)サイズ
	bool MapEnabled;		//!< マップ有効
	bool isMaped;			//!< マップされている
	UnicodeString ErrMsg;	//!< エラーメッセージ

	/* @brief コンストラクタ */
	MemMapFile();

	~MemMapFile();

	/**
	 * @brief 読み取り専用のメモリマップドファイルとして開く@n
			  開けない場合はメモリに通常読み込み
	 * 
	 * @param fnam ファイル名
	 * @param top_adr 先頭アドレス
	 * @param max_size 最大サイズ
	 * @return true 成功
	 */
	bool OpenRO(UnicodeString fnam, __int64 top_adr, unsigned int max_size);

	/** @brief 閉じて初期化 */
	void Close();
};
//---------------------------------------------------------------------------
#endif
