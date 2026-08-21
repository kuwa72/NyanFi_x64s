/**
 * @file thumb_thread.h
 * @brief サムネイル取得スレッド
 */
//---------------------------------------------------------------------------
#ifndef ThumbnailThreadH
#define ThumbnailThreadH

//---------------------------------------------------------------------------
#include <System.Classes.hpp>

//---------------------------------------------------------------------------
/**
 * @brief サムネイル取得スレッド
 */
class TThumbnailThread : public TThread
{
private:
	TMultiReadExclusiveWriteSynchronizer *TaskRWLock;

	//スレッドセーフを考慮したプロパティ
	bool FReqClear;
	bool __fastcall GetReqClear()
	{
		TaskRWLock->BeginRead();
		bool v = FReqClear;
		TaskRWLock->EndRead();
		return v;
	}
	void __fastcall SetReqClear(bool Value)
	{
		TaskRWLock->BeginWrite();
		FReqClear = Value;
		TaskRWLock->EndWrite();
	}

	bool FReqStart;
	bool __fastcall GetReqStart()
	{
		TaskRWLock->BeginRead();
		bool v = FReqStart;
		TaskRWLock->EndRead();
		return v;
	}
	void __fastcall SetReqStart(bool Value)
	{
		TaskRWLock->BeginWrite();
		FReqStart = Value;
		TaskRWLock->EndWrite();
	}

	bool FReqMake;
	bool __fastcall GetReqMake()
	{
		TaskRWLock->BeginRead();
		bool v = FReqMake;
		TaskRWLock->EndRead();
		return v;
	}
	void __fastcall SetReqMake(bool Value)
	{
		TaskRWLock->BeginWrite();
		FReqMake = Value;
		TaskRWLock->EndWrite();
	}

	bool FIsEmpty;
	bool __fastcall GetIsEmpty()
	{
		TaskRWLock->BeginRead();
		bool v = FIsEmpty;
		TaskRWLock->EndRead();
		return v;
	}
	void __fastcall SetIsEmpty(bool Value)
	{
		TaskRWLock->BeginWrite();
		FIsEmpty = Value;
		TaskRWLock->EndWrite();
	}

	int FCount;
	int __fastcall GetCount()
	{
		TaskRWLock->BeginRead();
		int v = ThumbnailList->Count;
		TaskRWLock->EndRead();
		return v;
	}

	void __fastcall Execute();

	// 下のプロパティ・プロキシから private のアクセサを呼ぶため
	template <class O, class T, T (O::*G)(), void (O::*S)(T)>
	friend class compat::RWMutableProperty;
	template <class O, class T, T (O::*G)()>
	friend class compat::ROMutableProperty;

public:
	HWND CallbackWnd;

	// __property を読み書きプロキシに置き換えたもの。getter が非 const・setter が
	// 値渡しなのは排他ロック (TaskRWLock) を取るためで、src 側の宣言は変えていない
	compat::RWMutableProperty<TThumbnailThread, bool, &TThumbnailThread::GetReqClear, &TThumbnailThread::SetReqClear> ReqClear{this};	//!< リストのクリア要求
	compat::RWMutableProperty<TThumbnailThread, bool, &TThumbnailThread::GetReqStart, &TThumbnailThread::SetReqStart> ReqStart{this};	//!< 取得スタート要求
	compat::RWMutableProperty<TThumbnailThread, bool, &TThumbnailThread::GetReqMake,  &TThumbnailThread::SetReqMake>  ReqMake{this};		//!< 個別作成要求
	compat::RWMutableProperty<TThumbnailThread, bool, &TThumbnailThread::GetIsEmpty,  &TThumbnailThread::SetIsEmpty>  IsEmpty{this};		//!< サムネイル未取得
	compat::ROMutableProperty<TThumbnailThread, int,  &TThumbnailThread::GetCount>                                    Count{this};		//!< リスト項目数

	int MakeIndex;
	int StartIndex;

	TStringList *ThumbnailList;	//!< サムネイルリスト
	UnicodeString __fastcall GetListItem(int idx);
	void __fastcall SetListItem(int idx, UnicodeString s);
	Graphics::TBitmap* __fastcall GetListBitmap(int idx);
	Graphics::TBitmap* __fastcall GetListBitmap(UnicodeString fnam);

	/**
	 * @brief コンストラクタ
	 * @param CreateSuspended 
	 */
	__fastcall TThumbnailThread(bool CreateSuspended);

	bool __fastcall FitSize(int *wd, int *hi);
	void __fastcall MakeThumbnail(int idx);
};
//---------------------------------------------------------------------------
#endif
