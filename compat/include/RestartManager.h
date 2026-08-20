/**
 * @file RestartManager.h
 * @brief Restart Manager API の宣言 (mingw-w64 に無いため補う)
 *
 * usr_file_inf.cpp が「そのファイルを掴んでいるプロセス」を列挙するために使う。
 * Windows SDK には RestartManager.h があるので、この互換ヘッダは
 * ローカル検証用の mingw-w64 でのみ効く。宣言は SDK と同じ内容にしてある。
 * リンクには rstrtmgr.lib / -lrstrtmgr が必要。
 */
#ifndef NYANFI_FWD_RESTARTMANAGER_H
#define NYANFI_FWD_RESTARTMANAGER_H

#include "compat/config.h"

#if defined(__MINGW32__)

#ifdef __cplusplus
extern "C" {
#endif

#define CCH_RM_SESSION_KEY (32 * 2)
#define CCH_RM_MAX_APP_NAME 255
#define CCH_RM_MAX_SVC_NAME 63

#define RM_INVALID_TS_SESSION (-1)
#define RM_INVALID_PROCESS (-1)

typedef enum _RM_APP_TYPE {
	RmUnknownApp = 0,
	RmMainWindow = 1,
	RmOtherWindow = 2,
	RmService = 3,
	RmExplorer = 4,
	RmConsole = 5,
	RmCritical = 1000
} RM_APP_TYPE;

typedef enum _RM_SHUTDOWN_TYPE {
	RmForceShutdown = 0x1,
	RmShutdownOnlyRegistered = 0x10
} RM_SHUTDOWN_TYPE;

typedef enum _RM_APP_STATUS {
	RmStatusUnknown = 0x0,
	RmStatusRunning = 0x1,
	RmStatusStopped = 0x2,
	RmStatusStoppedOther = 0x4,
	RmStatusRestarted = 0x8,
	RmStatusErrorOnStop = 0x10,
	RmStatusErrorOnRestart = 0x20,
	RmStatusShutdownMasked = 0x40,
	RmStatusRestartMasked = 0x80
} RM_APP_STATUS;

typedef enum _RM_REBOOT_REASON {
	RmRebootReasonNone = 0x0,
	RmRebootReasonPermissionDenied = 0x1,
	RmRebootReasonSessionMismatch = 0x2,
	RmRebootReasonCriticalProcess = 0x4,
	RmRebootReasonCriticalService = 0x8,
	RmRebootReasonDetectedSelf = 0x10
} RM_REBOOT_REASON;

typedef struct _RM_UNIQUE_PROCESS {
	DWORD dwProcessId;
	FILETIME ProcessStartTime;
} RM_UNIQUE_PROCESS, *PRM_UNIQUE_PROCESS;

typedef struct _RM_PROCESS_INFO {
	RM_UNIQUE_PROCESS Process;
	WCHAR strAppName[CCH_RM_MAX_APP_NAME + 1];
	WCHAR strServiceShortName[CCH_RM_MAX_SVC_NAME + 1];
	RM_APP_TYPE ApplicationType;
	ULONG AppStatus;
	DWORD TSSessionId;
	BOOL bRestartable;
} RM_PROCESS_INFO, *PRM_PROCESS_INFO;

typedef void(WINAPI *RM_WRITE_STATUS_CALLBACK)(UINT nPercentComplete);

DWORD WINAPI RmStartSession(DWORD *pSessionHandle, DWORD dwSessionFlags, WCHAR strSessionKey[]);
DWORD WINAPI RmJoinSession(DWORD *pSessionHandle, const WCHAR strSessionKey[]);
DWORD WINAPI RmEndSession(DWORD dwSessionHandle);
DWORD WINAPI RmRegisterResources(DWORD dwSessionHandle, UINT nFiles, LPCWSTR rgsFileNames[],
                                 UINT nApplications, RM_UNIQUE_PROCESS rgApplications[],
                                 UINT nServices, LPCWSTR rgsServiceNames[]);
DWORD WINAPI RmGetList(DWORD dwSessionHandle, UINT *pnProcInfoNeeded, UINT *pnProcInfo,
                       RM_PROCESS_INFO rgAffectedApps[], LPDWORD lpdwRebootReasons);
DWORD WINAPI RmShutdown(DWORD dwSessionHandle, ULONG lActionFlags, RM_WRITE_STATUS_CALLBACK fnStatus);
DWORD WINAPI RmRestart(DWORD dwSessionHandle, DWORD dwRestartFlags, RM_WRITE_STATUS_CALLBACK fnStatus);

#ifdef __cplusplus
}
#endif

#else  // Windows SDK を使う場合は本物を読む

#include <restartmanager.h>

#endif

#endif  // NYANFI_FWD_RESTARTMANAGER_H
