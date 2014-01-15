// 这个文件是 MCF 的一部分。
// 有关具体授权说明，请参阅 MCFLicense.txt。
// Copyleft 2013. LH_Mouse. All wrongs reserved.

#ifndef __MCF_CRT_BAIL_H__
#define __MCF_CRT_BAIL_H__

#include "_crtdef.h"

__MCF_CRT_EXTERN void __MCF_Bail(const wchar_t *pwszDescription);
__MCF_CRT_EXTERN void __MCF_BailF(const wchar_t *pwszFormat, ...);

#endif
