// 这个文件是 MCF 的一部分。
// 有关具体授权说明，请参阅 MCFLicense.txt。
// Copyleft 2013 - 2016, LH_Mouse. All wrongs reserved.

#include "thread.h"
#include "mcfwin.h"
#include "fenv.h"
#include "avl_tree.h"
#include "bail.h"
#include "mingw_hacks.h"
#include "eh_top.h"
#include "clocks.h"
#include "../ext/assert.h"
#include "../ext/unref_param.h"
#include <stdlib.h>
#include <winternl.h>
#include <ntdef.h>

extern __attribute__((__dllimport__, __stdcall__))
NTSTATUS RtlCreateUserThread(HANDLE hProcess, const SECURITY_DESCRIPTOR *pSecurityDescriptor, BOOLEAN bSuspended,
	ULONG ulStackZeroBits, ULONG *pulStackReserved, ULONG *pulStackCommitted, PTHREAD_START_ROUTINE pfnThreadProc, VOID *pParam, HANDLE *pHandle, CLIENT_ID *pClientId);

extern __attribute__((__dllimport__, __stdcall__))
NTSTATUS NtDelayExecution(BOOLEAN bAlertable, const LARGE_INTEGER *pInterval);
extern __attribute__((__dllimport__, __stdcall__))
NTSTATUS NtYieldExecution(void);

extern __attribute__((__dllimport__, __stdcall__))
NTSTATUS NtSuspendThread(HANDLE hThread, LONG *plPrevCount);
extern __attribute__((__dllimport__, __stdcall__))
NTSTATUS NtResumeThread(HANDLE hThread, LONG *plPrevCount);

typedef struct tagTlsObject {
	MCFCRT_AvlNodeHeader vHeader;

	intptr_t nValue;

	struct tagThreadMap *pMap;
	struct tagTlsObject *pPrevByThread;
	struct tagTlsObject *pNextByThread;

	struct tagTlsKey *pKey;
	struct tagTlsObject *pPrevByKey;
	struct tagTlsObject *pNextByKey;
} TlsObject;

static int ObjectComparatorNodeKey(const MCFCRT_AvlNodeHeader *pObj1, intptr_t nKey2){
	const uintptr_t uKey1 = (uintptr_t)(((const TlsObject *)pObj1)->pKey);
	const uintptr_t uKey2 = (uintptr_t)(void *)nKey2;
	return (uKey1 < uKey2) ? -1 : ((uKey1 > uKey2) ? 1 : 0);
}
static int ObjectComparatorNodes(const MCFCRT_AvlNodeHeader *pObj1, const MCFCRT_AvlNodeHeader *pObj2){
	return ObjectComparatorNodeKey(pObj1, (intptr_t)(void *)(((const TlsObject *)pObj2)->pKey));
}

typedef struct tagThreadMap {
	SRWLOCK srwLock;
	MCFCRT_AvlRoot pavlObjects;
	struct tagTlsObject *pLastByThread;
} ThreadMap;

typedef struct tagTlsKey {
	MCFCRT_AvlNodeHeader vHeader;

	SRWLOCK srwLock;
	void (*pfnCallback)(intptr_t);
	struct tagTlsObject *pLastByKey;
} TlsKey;

static int KeyComparatorNodeKey(const MCFCRT_AvlNodeHeader *pObj1, intptr_t nKey2){
	const uintptr_t uKey1 = (uintptr_t)(void *)pObj1;
	const uintptr_t uKey2 = (uintptr_t)(void *)nKey2;
	return (uKey1 < uKey2) ? -1 : ((uKey1 > uKey2) ? 1 : 0);
}
static int KeyComparatorNodes(const MCFCRT_AvlNodeHeader *pObj1, const MCFCRT_AvlNodeHeader *pObj2){
	return KeyComparatorNodeKey(pObj1, (intptr_t)(void *)pObj2);
}

static SRWLOCK          g_csKeyMutex  = SRWLOCK_INIT;
static DWORD            g_dwTlsIndex  = TLS_OUT_OF_INDEXES;
static MCFCRT_AvlRoot   g_pavlKeys    = nullptr;

bool __MCFCRT_ThreadEnvInit(){
	g_dwTlsIndex = TlsAlloc();
	if(g_dwTlsIndex == TLS_OUT_OF_INDEXES){
		return false;
	}
	return true;
}
void __MCFCRT_ThreadEnvUninit(){
	if(g_pavlKeys){
		MCFCRT_AvlNodeHeader *const pRoot = g_pavlKeys;
		g_pavlKeys = nullptr;

		TlsKey *pKey;
		MCFCRT_AvlNodeHeader *pCur = MCFCRT_AvlPrev(pRoot);
		while(pCur){
			pKey = (TlsKey *)pCur;
			pCur = MCFCRT_AvlPrev(pCur);
			free(pKey);
		}
		pCur = MCFCRT_AvlNext(pRoot);
		while(pCur){
			pKey = (TlsKey *)pCur;
			pCur = MCFCRT_AvlNext(pCur);
			free(pKey);
		}
		pKey = (TlsKey *)pRoot;
		free(pKey);
	}

	TlsFree(g_dwTlsIndex);
	g_dwTlsIndex = TLS_OUT_OF_INDEXES;
}

void __MCFCRT_TlsThreadCleanup(){
	ThreadMap *const pMap = TlsGetValue(g_dwTlsIndex);
	if(pMap){
		TlsObject *pObject = pMap->pLastByThread;
		while(pObject){
			TlsKey *const pKey = pObject->pKey;

			AcquireSRWLockExclusive(&(pKey->srwLock));
			{
				if(pKey->pLastByKey == pObject){
					pKey->pLastByKey = pObject->pPrevByKey;
				}
			}
			ReleaseSRWLockExclusive(&(pKey->srwLock));

			if(pKey->pfnCallback){
				(*pKey->pfnCallback)(pObject->nValue);
			}

			TlsObject *const pTemp = pObject->pPrevByThread;
			free(pObject);
			pObject = pTemp;
		}
		free(pMap);
		TlsSetValue(g_dwTlsIndex, nullptr);
	}

	__MCFCRT_RunEmutlsDtors();
}

void *MCFCRT_TlsAllocKey(void (*pfnCallback)(intptr_t)){
	TlsKey *const pKey = malloc(sizeof(TlsKey));
	if(!pKey){
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return nullptr;
	}
	InitializeSRWLock(&(pKey->srwLock));
	pKey->pfnCallback = pfnCallback;
	pKey->pLastByKey  = nullptr;

	AcquireSRWLockExclusive(&g_csKeyMutex);
	{
		MCFCRT_AvlAttach(&g_pavlKeys, (MCFCRT_AvlNodeHeader *)pKey, &KeyComparatorNodes);
	}
	ReleaseSRWLockExclusive(&g_csKeyMutex);

	return pKey;
}
bool MCFCRT_TlsFreeKey(void *pTlsKey){
	TlsKey *const pKey = pTlsKey;
	if(!pKey){
		SetLastError(ERROR_INVALID_PARAMETER);
		return false;
	}

	AcquireSRWLockExclusive(&g_csKeyMutex);
	{
		MCFCRT_AvlDetach((MCFCRT_AvlNodeHeader *)pKey);
	}
	ReleaseSRWLockExclusive(&g_csKeyMutex);

	TlsObject *pObject = pKey->pLastByKey;
	while(pObject){
		ThreadMap *const pMap = pObject->pMap;

		AcquireSRWLockExclusive(&(pMap->srwLock));
		{
			TlsObject *const pPrev = pObject->pPrevByThread;
			TlsObject *const pNext = pObject->pNextByThread;
			if(pPrev){
				pPrev->pNextByThread = pNext;
			}
			if(pNext){
				pNext->pPrevByThread = pPrev;
			}

			if(pMap->pLastByThread == pObject){
				pMap->pLastByThread = pObject->pPrevByThread;
			}
		}
		ReleaseSRWLockExclusive(&(pMap->srwLock));

		if(pKey->pfnCallback){
			(*pKey->pfnCallback)(pObject->nValue);
		}

		TlsObject *const pTemp = pObject->pPrevByKey;
		free(pObject);
		pObject = pTemp;
	}
	free(pKey);

	return true;
}

void (*MCFCRT_TlsGetCallback(void *pTlsKey))(intptr_t){
	TlsKey *const pKey = pTlsKey;
	if(!pKey){
		SetLastError(ERROR_INVALID_PARAMETER);
		return nullptr;
	}
	SetLastError(ERROR_SUCCESS);
	return pKey->pfnCallback;
}
bool MCFCRT_TlsGet(void *pTlsKey, bool *restrict pbHasValue, intptr_t *restrict pnValue){
	TlsKey *const pKey = pTlsKey;
	if(!pKey){
		SetLastError(ERROR_INVALID_PARAMETER);
		return false;
	}

	*pbHasValue = false;

	ThreadMap *const pMap = TlsGetValue(g_dwTlsIndex);
	if(!pMap){
		return true;
	}

	AcquireSRWLockExclusive(&(pMap->srwLock));
	{
		TlsObject *const pObject = (TlsObject *)MCFCRT_AvlFind(
			&(pMap->pavlObjects), (intptr_t)pKey, &ObjectComparatorNodeKey);
		if(pObject){
			*pbHasValue = true;
			*pnValue = pObject->nValue;
		}
	}
	ReleaseSRWLockExclusive(&(pMap->srwLock));

	return true;
}
bool MCFCRT_TlsReset(void *pTlsKey, intptr_t nNewValue){
	TlsKey *const pKey = pTlsKey;
	if(!pKey){
		SetLastError(ERROR_INVALID_PARAMETER);
		return false;
	}

	bool bHasOldValue;
	intptr_t nOldValue;
	if(!MCFCRT_TlsExchange(pTlsKey, &bHasOldValue, &nOldValue, nNewValue)){
		if(pKey->pfnCallback){
			const DWORD dwErrorCode = GetLastError();
			(*pKey->pfnCallback)(nNewValue);
			SetLastError(dwErrorCode);
		}
		return false;
	}
	if(bHasOldValue){
		if(pKey->pfnCallback){
			(*pKey->pfnCallback)(nOldValue);
		}
	}
	return true;
}
bool MCFCRT_TlsExchange(void *pTlsKey, bool *restrict pbHasOldValue, intptr_t *restrict pnOldValue, intptr_t nNewValue){
	TlsKey *const pKey = pTlsKey;
	if(!pKey){
		SetLastError(ERROR_INVALID_PARAMETER);
		return false;
	}

	*pbHasOldValue = false;

	ThreadMap *pMap = TlsGetValue(g_dwTlsIndex);
	if(!pMap){
		pMap = malloc(sizeof(ThreadMap));
		if(!pMap){
			SetLastError(ERROR_NOT_ENOUGH_MEMORY);
			return false;
		}
		InitializeSRWLock(&(pMap->srwLock));
		pMap->pavlObjects   = nullptr;
		pMap->pLastByThread = nullptr;

		TlsSetValue(g_dwTlsIndex, pMap);
	}

	AcquireSRWLockExclusive(&(pMap->srwLock));
	{
		TlsObject *const pObject = (TlsObject *)MCFCRT_AvlFind(
			&(pMap->pavlObjects), (intptr_t)pKey, &ObjectComparatorNodeKey);
		if(pObject){
			*pbHasOldValue = true;
			*pnOldValue = pObject->nValue;
			pObject->nValue = nNewValue;
		}
	}
	ReleaseSRWLockExclusive(&(pMap->srwLock));

	if(!*pbHasOldValue){
		TlsObject *const pObject = malloc(sizeof(TlsObject));
		if(!pObject){
			SetLastError(ERROR_NOT_ENOUGH_MEMORY);
			return false;
		}
		pObject->nValue = nNewValue;
		pObject->pMap = pMap;
		pObject->pKey = pKey;

		AcquireSRWLockExclusive(&(pMap->srwLock));
		{
			TlsObject *const pPrev = pMap->pLastByThread;
			pMap->pLastByThread = pObject;

			pObject->pPrevByThread = pPrev;
			pObject->pNextByThread = nullptr;
			if(pPrev){
				pPrev->pNextByThread = pObject;
			}
			MCFCRT_AvlAttach(&(pMap->pavlObjects), (MCFCRT_AvlNodeHeader *)pObject, &ObjectComparatorNodes);
		}
		ReleaseSRWLockExclusive(&(pMap->srwLock));

		AcquireSRWLockExclusive(&(pKey->srwLock));
		{
			TlsObject *const pPrev = pKey->pLastByKey;
			pKey->pLastByKey = pObject;

			pObject->pPrevByKey = pPrev;
			pObject->pNextByKey = nullptr;
			if(pPrev){
				pPrev->pNextByKey = pObject;
			}
		}
		ReleaseSRWLockExclusive(&(pKey->srwLock));
	}

	return true;
}

int MCFCRT_AtEndThread(void (*pfnProc)(intptr_t), intptr_t nContext){
	void *const pKey = MCFCRT_TlsAllocKey(pfnProc);
	if(!pKey){
		return -1;
	}
	if(!MCFCRT_TlsReset(pKey, nContext)){
		const DWORD dwLastError = GetLastError();
		MCFCRT_TlsFreeKey(pKey);
		SetLastError(dwLastError);
		return -1;
	}
	return 0;
}

typedef struct tagThreadInitParams {
	unsigned (*pfnProc)(intptr_t);
	intptr_t nParam;
} ThreadInitParams;

static __MCFCRT_C_STDCALL __MCFCRT_HAS_EH_TOP
DWORD CRTThreadProc(LPVOID pParam){
	DWORD dwExitCode;
	__MCFCRT_EH_TOP_BEGIN
	{
		const ThreadInitParams vInitParams = *(ThreadInitParams *)pParam;
		free(pParam);

		__MCFCRT_FEnvInit();

		dwExitCode = (*vInitParams.pfnProc)(vInitParams.nParam);
	}
	__MCFCRT_EH_TOP_END
	return dwExitCode;
}

void *MCFCRT_CreateThread(unsigned (*pfnThreadProc)(intptr_t), intptr_t nParam, bool bSuspended, uintptr_t *restrict puThreadId){
	ThreadInitParams *const pInitParams = malloc(sizeof(ThreadInitParams));
	if(!pInitParams){
		return nullptr;
	}
	pInitParams->pfnProc = pfnThreadProc;
	pInitParams->nParam  = nParam;

	ULONG ulStackReserved = 0, ulStackCommitted = 0;
	HANDLE hThread;
	CLIENT_ID vClientId;
	const NTSTATUS lStatus = RtlCreateUserThread(GetCurrentProcess(), nullptr, bSuspended, 0, &ulStackReserved, &ulStackCommitted, &CRTThreadProc, pInitParams, &hThread, &vClientId);
	if(!NT_SUCCESS(lStatus)){
		free(pInitParams);
		SetLastError(RtlNtStatusToDosError(lStatus));
		return nullptr;
	}
	if(puThreadId){
		*puThreadId = (uintptr_t)vClientId.UniqueThread;
	}
	return (void *)hThread;
}
void MCFCRT_CloseThread(void *hThread){

	const NTSTATUS lStatus = NtClose((HANDLE)hThread);
	if(!NT_SUCCESS(lStatus)){
		ASSERT_MSG(false, L"NtClose() 失败。");
	}
}

uintptr_t MCFCRT_GetCurrentThreadId(){
	return GetCurrentThreadId();
}

void MCFCRT_Sleep(uint64_t u64UntilFastMonoClock){
	LARGE_INTEGER liTimeout;
	const uint64_t u64Now = MCFCRT_GetFastMonoClock();
	if(u64Now >= u64UntilFastMonoClock){
		liTimeout.QuadPart = 0;
	} else {
		const uint64_t u64DeltaMillisec = u64UntilFastMonoClock - u64Now;
		const int64_t n64Delta100Nanosec = (int64_t)(u64DeltaMillisec * 10000);
		if((uint64_t)(n64Delta100Nanosec / 10000) != u64DeltaMillisec){
			liTimeout.QuadPart = INT64_MIN;
		} else {
			liTimeout.QuadPart = -n64Delta100Nanosec;
		}
	}
	const NTSTATUS lStatus = NtDelayExecution(false, &liTimeout);
	if(!NT_SUCCESS(lStatus)){
		ASSERT_MSG(false, L"NtDelayExecution() 失败。");
	}
}
bool MCFCRT_AlertableSleep(uint64_t u64UntilFastMonoClock){
	LARGE_INTEGER liTimeout;
	const uint64_t u64Now = MCFCRT_GetFastMonoClock();
	if(u64Now >= u64UntilFastMonoClock){
		liTimeout.QuadPart = 0;
	} else {
		const uint64_t u64DeltaMillisec = u64UntilFastMonoClock - u64Now;
		const int64_t n64Delta100Nanosec = (int64_t)(u64DeltaMillisec * 10000);
		if((uint64_t)(n64Delta100Nanosec / 10000) != u64DeltaMillisec){
			liTimeout.QuadPart = INT64_MIN;
		} else {
			liTimeout.QuadPart = -n64Delta100Nanosec;
		}
	}
	const NTSTATUS lStatus = NtDelayExecution(true, &liTimeout);
	if(!NT_SUCCESS(lStatus)){
		ASSERT_MSG(false, L"NtDelayExecution() 失败。");
	}
	if(lStatus == STATUS_TIMEOUT){
		return false;
	}
	return true;
}
void MCFCRT_AlertableSleepInfinitely(){
	LARGE_INTEGER liTimeout;
	liTimeout.QuadPart = INT64_MAX;
	const NTSTATUS lStatus = NtDelayExecution(true, &liTimeout);
	if(!NT_SUCCESS(lStatus)){
		ASSERT_MSG(false, L"NtDelayExecution() 失败。");
	}
}
void MCFCRT_YieldThread(){
	const NTSTATUS lStatus = NtYieldExecution();
	if(!NT_SUCCESS(lStatus)){
		ASSERT_MSG(false, L"NtYieldExecution() 失败。");
	}
}

long MCFCRT_SuspendThread(void *hThread){
	LONG lPrevCount;
	const NTSTATUS lStatus = NtSuspendThread((HANDLE)hThread, &lPrevCount);
	if(!NT_SUCCESS(lStatus)){
		ASSERT_MSG(false, L"NtSuspendThread() 失败。");
	}
	return lPrevCount;
}
long MCFCRT_ResumeThread(void *hThread){
	LONG lPrevCount;
	const NTSTATUS lStatus = NtResumeThread((HANDLE)hThread, &lPrevCount);
	if(!NT_SUCCESS(lStatus)){
		ASSERT_MSG(false, L"NtResumeThread() 失败。");
	}
	return lPrevCount;
}

bool MCFCRT_WaitForThread(void *hThread, MCFCRT_STD uint64_t u64UntilFastMonoClock){
	LARGE_INTEGER liTimeout;
	liTimeout.QuadPart = 0;
	if(u64UntilFastMonoClock != 0){
		const uint64_t u64Now = MCFCRT_GetFastMonoClock();
		if(u64Now < u64UntilFastMonoClock){
			const uint64_t u64DeltaMillisec = u64UntilFastMonoClock - u64Now;
			const int64_t n64Delta100Nanosec = (int64_t)(u64DeltaMillisec * 10000);
			if((uint64_t)(n64Delta100Nanosec / 10000) != u64DeltaMillisec){
				liTimeout.QuadPart = INT64_MIN;
			} else {
				liTimeout.QuadPart = -n64Delta100Nanosec;
			}
		}
	}
	const NTSTATUS lStatus = NtWaitForSingleObject((HANDLE)hThread, false, &liTimeout);
	if(!NT_SUCCESS(lStatus)){
		ASSERT_MSG(false, L"NtWaitForSingleObject() 失败。");
	}
	if(lStatus == STATUS_TIMEOUT){
		return false;
	}
	return true;
}
void MCFCRT_WaitForThreadInfinitely(void *hThread){
	const NTSTATUS lStatus = NtWaitForSingleObject((HANDLE)hThread, false, nullptr);
	if(!NT_SUCCESS(lStatus)){
		ASSERT_MSG(false, L"NtWaitForSingleObject() 失败。");
	}
}
