// 这个文件是 MCF 的一部分。
// 有关具体授权说明，请参阅 MCFLicense.txt。
// Copyleft 2013. LH_Mouse. All wrongs reserved.

#define WIN32_LEAN_AND_MEAN

#include "../MCFCRT.h"
#include <windows.h>

// -static -nostartfiles -Wl,-e__MCFExeStartup,--disable-runtime-pseudo-reloc,--disable-auto-import,-lmcfcrt,-lstdc++,-lgcc,-lgcc_eh,-lmingwex,-lmcfcrt

__MCF_CRT_EXTERN unsigned long __MCF_CRT_ExeInitializeArgV();
__MCF_CRT_EXTERN void __MCF_CRT_ExeUninitializeArgV();

extern unsigned int MCFMain();

extern int __stdcall __MCFExeStartup(){
	DWORD dwExitCode;

#define TRY(exp)		if((dwExitCode = (exp)) == ERROR_SUCCESS){ ((void)0)
#define CLEANUP(exp)	(exp); } ((void)0)

	TRY(__MCF_CRT_Begin());
	TRY(__MCF_CRT_ThreadInitialize());
	TRY(__MCF_CRT_ExeInitializeArgV());

	dwExitCode = MCFMain();

	CLEANUP(__MCF_CRT_ExeUninitializeArgV());
	CLEANUP(__MCF_CRT_ThreadUninitialize());
	CLEANUP(__MCF_CRT_End());

	ExitProcess(dwExitCode);
	return (int)dwExitCode;		// 这有什么意义吗？
}

#ifndef __amd64__
extern __attribute__((noreturn, alias("__MCFExeStartup@0"))) void __cdecl _MCFExeStartup();
#endif
