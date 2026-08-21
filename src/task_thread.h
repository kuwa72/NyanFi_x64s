/**
 * @file task_thread.h
 * @brief タスク処理スレッド
 */
//---------------------------------------------------------------------------
#ifndef TaskThreadH
#define TaskThreadH

//---------------------------------------------------------------------------
#include <System.Classes.hpp>

//---------------------------------------------------------------------------
#define MIN_INTERVAL	1
#define WAIT_INTERVAL	50
#define SLOW_INTERVAL	500

//---------------------------------------------------------------------------
/**
 * @brief タスク設定
 */
class TaskConfig
{
public:
	int  CopyMode;
	bool CopyAll;
	UnicodeString CopyFmt;
	int  DstPosMode;
	bool KeepTime;
	bool NoSubDir;

	TDateTime FilterTime;
	int  FilterMode;

	UnicodeString CmdStr;
	UnicodeString InfStr;
	UnicodeString DistPath;
	UnicodeString WorkList;

	UnicodeString CvImg_f_ext;
	int  CvImg_quality;
	int  CvImg_ycrcb;
	int  CvImg_cmp_mode;
	int  CvImg_scale_mode;
	int  CvImg_scale_prm1;
	int  CvImg_scale_prm2;
	int  CvImg_scale_opt;
	bool CvImg_from_clip;
	bool CvImg_not_use_prvw;
	bool CvImg_grayscale;
	TColor CvImg_mgn_color;
	int  CvImg_chg_name_mode;
	UnicodeString CvImg_chg_name_str;
	UnicodeString CvImg_clip_name;

	UnicodeString Bakup_inc_mask;
	UnicodeString Bakup_exc_mask;
	UnicodeString Bakup_skip_dir;
	bool Bakup_sub_sw;
	bool Bakup_mirror;

	bool Distribute_sw;

	bool DirComp_sw;
	bool DirComp_ext_sw;

	TStringList *TaskList;
	TStringList *UpdDirList;

	TaskConfig();
	~TaskConfig();

	void Assign(TaskConfig *cp);
};

//---------------------------------------------------------------------------
//タスク設定リスト (TList から継承)
//---------------------------------------------------------------------------
class TaskConfigList : public TList
{
private:
	TaskConfig * __fastcall Get(int Index)
	{
		return (TaskConfig*)(TList::Get(Index));
	}
	void __fastcall Put(int Index, TaskConfig *Item)
	{
		TList::Put(Index, Item);
	}

	// Items (添字プロキシ) から private の Get/Put を呼ぶため
	template <class O, class T, T *(O::*G)(int), void (O::*S)(int, T *)>
	friend class compat::IndexedPtrProperty;

public:
	__fastcall TaskConfigList();
	__fastcall ~TaskConfigList();

	void __fastcall ClearAll();

	// __property TaskConfig * Items[int Index] = {read=Get, write=Put};
	// C++Builder の __property を添字プロキシに置き換えたもの
	compat::IndexedPtrProperty<TaskConfigList, TaskConfig,
	                           &TaskConfigList::Get, &TaskConfigList::Put> Items{this};
};

//---------------------------------------------------------------------------
/**
 * @brief タスク処理スレッド
 */
class TTaskThread : public TThread
{
private:
	bool IsRemote;			//コピー/移動元・先がネットワークドライブ

	LARGE_INTEGER LastTransferred;
	__int64 SmplSize;
	int  LastCount;

	bool ErrCancel;

	TStringList *TaskObjList;

	static DWORD CALLBACK ProgressRoutine(
		LARGE_INTEGER TotalFileSize, LARGE_INTEGER TotalBytesTransferred,
		LARGE_INTEGER StreamSize, LARGE_INTEGER StreamBytesTransferred,
		DWORD dwStreamNumber, DWORD dwCallbackReason,
		HANDLE hSourceFile, HANDLE hDestinationFile, LPVOID lpData);

	DWORD __fastcall ProgressCore(LARGE_INTEGER TotalFileSize, LARGE_INTEGER TotalBytesTransferred);

	void __fastcall FinishTask();
	void __fastcall Execute();

	int  __fastcall GetFiles(UnicodeString pnam, UnicodeString mask, TStrings *lst, bool empSW = false, bool symSW = false);

	int  __fastcall GetFilesEx(UnicodeString pnam, UnicodeString inc_mask, UnicodeString exc_mask,
						TStrings *lst, bool subSW = false, UnicodeString skip_dir = EmptyStr, TStringList *d_lst = NULL,
						bool symSW = false, bool dateSW = false);

	void __fastcall GetDirs(UnicodeString pnam, TStrings *lst);

	void __fastcall CPY_core(UnicodeString fnam, UnicodeString dst_path,
								bool mov_sw = false, bool remove_ro = false, TStringList *skip_list = NULL);
	void __fastcall Task_CPY(UnicodeString prm, bool mov_sw = false);

	void __fastcall AddDebugLog(UnicodeString msg, UnicodeString info = EmptyStr);
	bool __fastcall EX_file_exists(UnicodeString fnam);
	bool __fastcall EX_dir_exists(UnicodeString dnam);
	bool __fastcall EX_set_writable(UnicodeString fnam);
	bool __fastcall EX_wait_file_ready(UnicodeString fnam);
	bool __fastcall EX_delete_File(UnicodeString fnam, bool use_trash = false);
	bool __fastcall EX_delete_Dir(UnicodeString dnam);

	void __fastcall DEL_core(UnicodeString fnam, bool use_trash, TStringList *skip_list = NULL);
	void __fastcall DEL_dir_trash(UnicodeString dnam);
	void __fastcall DEL_dirs(UnicodeString pnam, TStringList *skip_list = NULL);
	void __fastcall Task_DEL(UnicodeString prm);

	void __fastcall CMPDEL_core(UnicodeString fnam, TStringList *skip_list = NULL);
	void __fastcall Task_CMPDEL(UnicodeString prm);

	void __fastcall Task_CPYDIR(UnicodeString prm);

	void __fastcall Task_CVIMG(UnicodeString prm);
	void __fastcall Task_DLEXIF(UnicodeString prm);
	void __fastcall Task_EXTCHM(UnicodeString prm);

	void __fastcall Task_BACKUP(UnicodeString prm);

	TDateTime __fastcall SetDirTime(UnicodeString dnam);
	void __fastcall Task_DIRTIME(UnicodeString prm);

	void MakeCompList(UnicodeString dnam);
	void __fastcall Task_DIRCOMP(UnicodeString prm);

	void __fastcall AddLog(UnicodeString msg, bool must_log = false);

	TMultiReadExclusiveWriteSynchronizer *TaskRWLock;

	//スレッドセーフを考慮したプロパティ
	bool FTaskReady;
	bool __fastcall GetTaskReady()
	{
		TaskRWLock->BeginRead();
		bool v = FTaskReady;
		TaskRWLock->EndRead();
		return v;
	}
	void __fastcall SetTaskReady(bool Value)
	{
		TaskRWLock->BeginWrite();
		FTaskReady = Value;
		TaskRWLock->EndWrite();
	}

	bool FTaskFinished;
	bool __fastcall GetTaskFinished()
	{
		TaskRWLock->BeginRead();
		bool v = FTaskFinished;
		TaskRWLock->EndRead();
		return v;
	}
	void __fastcall SetTaskFinished(bool Value)
	{
		TaskRWLock->BeginWrite();
		FTaskFinished = Value;
		TaskRWLock->EndWrite();
	}

	bool FTaskCancel;
	bool __fastcall GetTaskCancel()
	{
		TaskRWLock->BeginRead();
		bool v = FTaskCancel;
		TaskRWLock->EndRead();
		return v;
	}
	void __fastcall SetTaskCancel(bool Value)
	{
		TaskRWLock->BeginWrite();
		FTaskCancel = Value;
		TaskRWLock->EndWrite();
	}

	bool FTaskPause;
	bool __fastcall GetTaskPause()
	{
		TaskRWLock->BeginRead();
		bool v = FTaskPause;
		TaskRWLock->EndRead();
		return v;
	}
	void __fastcall SetTaskPause(bool Value)
	{
		TaskRWLock->BeginWrite();
		FTaskPause = Value;
		TaskRWLock->EndWrite();
	}

	bool FTaskAskSame;
	bool __fastcall GetTaskAskSame()
	{
		TaskRWLock->BeginRead();
		bool v = FTaskAskSame;
		TaskRWLock->EndRead();
		return v;
	}
	void __fastcall SetTaskAskSame(bool Value)
	{
		TaskRWLock->BeginWrite();
		FTaskAskSame = Value;
		TaskRWLock->EndWrite();
	}

	// 下のプロパティ・プロキシから private のアクセサを呼ぶため
	template <class O, class T, T (O::*G)(), void (O::*S)(T)>
	friend class compat::RWMutableProperty;
	template <class O, class T, T (O::*G)()>
	friend class compat::ROMutableProperty;

public:
	// __property を読み書きプロキシに置き換えたもの。getter が非 const・setter が
	// 値渡しなのは排他ロック (TaskRWLock) を取るためで、src 側の宣言は変えていない
	compat::RWMutableProperty<TTaskThread, bool, &TTaskThread::GetTaskReady,    &TTaskThread::SetTaskReady>    TaskReady{this};
	compat::RWMutableProperty<TTaskThread, bool, &TTaskThread::GetTaskFinished, &TTaskThread::SetTaskFinished> TaskFinished{this};	//!< タスクが終了
	compat::RWMutableProperty<TTaskThread, bool, &TTaskThread::GetTaskCancel,   &TTaskThread::SetTaskCancel>   TaskCancel{this};		//!< 中断
	compat::RWMutableProperty<TTaskThread, bool, &TTaskThread::GetTaskPause,    &TTaskThread::SetTaskPause>    TaskPause{this};		//!< 一旦停止
	compat::RWMutableProperty<TTaskThread, bool, &TTaskThread::GetTaskAskSame,  &TTaskThread::SetTaskAskSame>  TaskAskSame{this};	//!< 同名ファイル処理を要求

	int Tag;
	int ID;

	bool TaskIsFast;			//!< 高速実行中
	bool ReqTaskSlow;			//!< 低速実行を要求
	bool DirDeleted;			//!< ディレクトリが削除された
	bool LnkDeleted;			//!< ハードリンクが削除された

	int  TaskTime;
	int  PreCount, SubCount, OkCount, SkipCount, RenCount, ErrCount;
	int  StartCount;

	int  Speed;					//!< 転送速度(byte/ms)
	int  RemCount;				//!< 残り時間(ms)
	__int64 CurTotalSize;

	UnicodeString CurFileName;	//!< 処理対象
	double CurProgress;			//!< 現在コピー/移動中ファイルの進捗率

	UnicodeString DstFileName;	//!< コピー/移動/変換先

	UnicodeString CurSrcName;	//!< コピー/移動元
	UnicodeString CurDstName;	//!< コピー/移動先

	UnicodeString FirstDstName;	//!< 最初にコピー/移動したファイル名
	UnicodeString LastDstName;	//!< その時点で最後にコピー/移動したファイル名

	TaskConfig *Config;

	bool IsSamePath;
	UnicodeString CopyName;
	UnicodeString CloneName;

	UnicodeString CmdName;

	/**
	 * @brief コンストラクタ
	 * @param CreateSuspended 
	 */
	__fastcall TTaskThread(bool CreateSuspended);

	/** @brief タスクの開始 */
	bool __fastcall TaskStart();

	void WaitIfPause()
	{
		while (TaskPause && !TaskCancel) Sleep(WAIT_INTERVAL);
	}
};
//---------------------------------------------------------------------------
extern int TaskIdCount;

#endif
