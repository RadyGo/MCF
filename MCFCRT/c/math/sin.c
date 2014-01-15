// 这个文件是 MCF 的一部分。
// 有关具体授权说明，请参阅 MCFLicense.txt。
// Copyleft 2013. LH_Mouse. All wrongs reserved.

#include "../../env/_crtdef.h"
#include "_mathasm.h"

__MCF_CRT_EXTERN float sinf(float x){
	register float ret;
	__asm__ __volatile__(
		"fld dword ptr[%1] \n"
		"fsin \n"
		"fstsw ax \n"
		"test ah, 4 \n"
		"jz 1f \n"
		"	fldpi \n"
		"	fadd st, st \n"
		"	fxch st(1) \n"
		"	2: \n"
		"		fprem \n"
		"		fstsw ax \n"
		"		test ah, 4 \n"
		"	jnz 2b \n"
		"	fstp st(1) \n"
		"	fsin \n"
		"1: \n"
		__FLT_RET_ST("%1")
		: __FLT_RET_CONS(ret)
		: "m"(x)
		: "eax"
	);
	return ret;
}

__MCF_CRT_EXTERN double sin(double x){
	register double ret;
	__asm__ __volatile__(
		"fld qword ptr[%1] \n"
		"fsin \n"
		"fstsw ax \n"
		"test ah, 4 \n"
		"jz 1f \n"
		"	fldpi \n"
		"	fadd st, st \n"
		"	fxch st(1) \n"
		"	2: \n"
		"		fprem \n"
		"		fstsw ax \n"
		"		test ah, 4 \n"
		"	jnz 2b \n"
		"	fstp st(1) \n"
		"	fsin \n"
		"1: \n"
		__DBL_RET_ST("%1")
		: __DBL_RET_CONS(ret)
		: "m"(x)
		: "eax"
	);
	return ret;
}

__MCF_CRT_EXTERN __LDBL_DECL(sinl, long double x){
	register long double ret;
	__asm__ __volatile__(
		"fld tbyte ptr[%1] \n"
		"fsin \n"
		"fstsw ax \n"
		"test ah, 4 \n"
		"jz 1f \n"
		"	fldpi \n"
		"	fadd st, st \n"
		"	fxch st(1) \n"
		"	2: \n"
		"		fprem \n"
		"		fstsw ax \n"
		"		test ah, 4 \n"
		"	jnz 2b \n"
		"	fstp st(1) \n"
		"	fsin \n"
		"1: \n"
		__LDBL_RET_ST()
		: __LDBL_RET_CONS(ret)
		: "m"(x), __LDBL_RET_CONS_IN()
		: "eax"
	);
	__LDBL_RETURN(ret);
}
