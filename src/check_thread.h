/**
 * @file check_thread.h
 * @brief UNCパスの存在チェック・スレッド
 */
//---------------------------------------------------------------------------
#ifndef TCheckPathThreadH
#define TCheckPathThreadH
//---------------------------------------------------------------------------
#include <System.Classes.hpp>

//---------------------------------------------------------------------------
/**
 * @brief UNCパスの存在チェック・スレッド
 */
class TCheckPathThread : public TThread
{
private:
	void __fastcall Execute();

	TMultiReadExclusiveWriteSynchronizer *TaskRWLock;

	//スレッドセーフを考慮したプロパティ
	UnicodeString FPathName;
	UnicodeString __fastcall GetPathName()
	{
		TaskRWLock->BeginRead();
		UnicodeString v = FPathName;
		TaskRWLock->EndRead();
		return v;
	}
	void __fastcall SetPathName(UnicodeString Value)
	{
		TaskRWLock->BeginWrite();
		FPathName = Value;
		TaskRWLock->EndWrite();
	}

	// 上のプロパティ・プロキシから private のアクセサを呼ぶため
	template <class O, class T, T (O::*G)(), void (O::*S)(T)>
	friend class compat::RWMutableProperty;

public:
	// __property を読み書きプロキシに置き換えたもの (排他ロックを取るため
	// getter が非 const・setter が値渡し。src 側の宣言は変えていない)
	compat::RWMutableProperty<TCheckPathThread, UnicodeString,
	                          &TCheckPathThread::GetPathName, &TCheckPathThread::SetPathName> PathName{this};

	bool isOk;
	UnicodeString ErrMsg;
	unsigned int  ErrCode;

	__fastcall TCheckPathThread(bool CreateSuspended);
};
//---------------------------------------------------------------------------
#endif
