// 这个文件是 MCF 的一部分。
// 有关具体授权说明，请参阅 MCFLicense.txt。
// Copyleft 2013 - 2014, LH_Mouse. All wrongs reserved.

#include "../../env/_crtdef.h"
#include "_mathasm.h"
#include <limits.h>

// postive pow float unsigned

#define UNROLLED	\
		if(++i == 32){	\
			break;	\
		}	\
		ret *= ret;	\
		mask >>= 1;	\
		if((y & mask) != 0){	\
			ret *= x;	\
		}

static float ppowfu(float x, uint32_t y){
	if(y == 0){
		return 1.0f;
	}
	register float ret = x;
	uint32_t i = (uint32_t)__builtin_clz(y);
	uint32_t mask = 0x80000000u >> i;
	for(;;){
		UNROLLED
		UNROLLED
		UNROLLED
		UNROLLED
		UNROLLED
		UNROLLED
		UNROLLED
		UNROLLED
	}
	return ret;
}
static double ppowdu(double x, uint32_t y){
	if(y == 0){
		return 1.0;
	}
	register double ret = x;
	uint32_t i = (uint32_t)__builtin_clz(y);
	uint32_t mask = 0x80000000u >> i;
	for(;;){
		UNROLLED
		UNROLLED
		UNROLLED
		UNROLLED
		UNROLLED
		UNROLLED
		UNROLLED
		UNROLLED
	}
	return ret;
}
static long double ppowlu(long double x, uint32_t y){
	if(y == 0){
		return 1.0l;
	}
	register long double ret = x;
	uint32_t i = (uint32_t)__builtin_clz(y);
	uint32_t mask = 0x80000000u >> i;
	for(;;){
		UNROLLED
		UNROLLED
		UNROLLED
		UNROLLED
		UNROLLED
		UNROLLED
		UNROLLED
		UNROLLED
	}
	return ret;
}

// postive pow float float
static __attribute__((__cdecl__)) float ppowf(float x, float y){
	register float ret;
	__asm__ __volatile__(
		"fld dword ptr[%2] \n"
		"fld dword ptr[%1] \n"
		"fyl2x \n"
		"fld st \n"
		"frndint \n"
		"fsub st(1), st \n"
		"fld1 \n"
		"fscale \n"
		"fstp st(1) \n"
		"fxch st(1) \n"
		"f2xm1 \n"
		"fld1 \n"
		"faddp st(1), st \n"
		"fmulp st(1), st \n"
		__MCF_FLT_RET_ST("%1")
		: __MCF_FLT_RET_CONS(ret)
		: "m"(x), "m"(y)
	);
	return ret;
}
static __attribute__((__cdecl__)) double ppowd(double x, double y){
	register double ret;
	__asm__ __volatile__(
		"fld qword ptr[%2] \n"
		"fld qword ptr[%1] \n"
		"fyl2x \n"
		"fld st \n"
		"frndint \n"
		"fsub st(1), st \n"
		"fld1 \n"
		"fscale \n"
		"fstp st(1) \n"
		"fxch st(1) \n"
		"f2xm1 \n"
		"fld1 \n"
		"faddp st(1), st \n"
		"fmulp st(1), st \n"
		__MCF_DBL_RET_ST("%1")
		: __MCF_DBL_RET_CONS(ret)
		: "m"(x), "m"(y)
	);
	return ret;
}
static __attribute__((__cdecl__)) __MCF_LDBL_DECL(ppowl, double x, double y){
	register long double ret;
	__asm__ __volatile__(
		"fld tbyte ptr[%2] \n"
		"fld tbyte ptr[%1] \n"
		"fyl2x \n"
		"fld st \n"
		"frndint \n"
		"fsub st(1), st \n"
		"fld1 \n"
		"fscale \n"
		"fstp st(1) \n"
		"fxch st(1) \n"
		"f2xm1 \n"
		"fld1 \n"
		"faddp st(1), st \n"
		"fmulp st(1), st \n"
		__MCF_LDBL_RET_ST()
		: __MCF_LDBL_RET_CONS(ret)
		: "m"(x), "m"(y), __MCF_LDBL_RET_CONS_IN()
	);
	__MCF_LDBL_RETURN(ret);
}

float powf(float x, float y){
	if(y == 0){
		return 1.0f;
	}
	if(x == 0){
		return 0.0f;
	}

	const float whole = __builtin_floorf(y);
	const float frac = y - whole;
	if(x > 0){
		if(y > INT_MAX){
			return ppowf(x, y);
		} else if(y < INT_MIN){
			return 1.0f / ppowf(x, -y);
		} else {
			float ret;
			if(y > 0){
				ret = ppowfu(x, (uint32_t)whole);
			} else {
				ret = 1.0f / ppowfu(x, (uint32_t)-whole);
			}
			if(frac != 0){
				ret *= ppowf(x, frac);
			}
			return ret;
		}
	} else {
		if(frac != 0){
			return -0.0f / 0.0f;
		}
		if(y > INT_MAX){
			if(__builtin_fmodf(whole, 2.0f) == 0.0f){
				return ppowf(-x, whole);
			} else {
				return -ppowf(-x, whole);
			}
		} else if(y < INT_MIN){
			if(__builtin_fmodf(whole, 2.0f) == 0.0f){
				return 1.0f / ppowf(-x, whole);
			} else {
				return -1.0f / ppowf(-x, whole);
			}
		} else {
			if(whole > 0){
				const uint32_t idx = (uint32_t)whole;
				if(idx % 2 == 0){
					return ppowfu(-x, idx);
				} else {
					return -ppowfu(-x, idx);
				}
			} else {
				const uint32_t idx = (uint32_t)-whole;
				if(idx % 2 == 0){
					return 1.0f / ppowfu(-x, idx);
				} else {
					return -1.0f / ppowfu(-x, idx);
				}
			}
		}
	}
}
double pow(double x, double y){
	if(y == 0){
		return 1.0;
	}
	if(x == 0){
		return 0.0;
	}

	const double whole = __builtin_floor(y);
	const double frac = y - whole;
	if(x > 0){
		if(y > INT_MAX){
			return ppowd(x, y);
		} else if(y < INT_MIN){
			return 1.0 / ppowd(x, -y);
		} else {
			double ret;
			if(y > 0){
				ret = ppowdu(x, (uint32_t)whole);
			} else {
				ret = 1.0 / ppowdu(x, (uint32_t)-whole);
			}
			if(frac != 0){
				ret *= ppowd(x, frac);
			}
			return ret;
		}
	} else {
		if(frac != 0){
			return -0.0 / 0.0;
		}
		if(y > INT_MAX){
			if(__builtin_fmod(whole, 2.0) == 0.0){
				return ppowd(-x, whole);
			} else {
				return -ppowd(-x, whole);
			}
		} else if(y < INT_MIN){
			if(__builtin_fmod(whole, 2.0) == 0.0){
				return 1.0 / ppowd(-x, whole);
			} else {
				return -1.0 / ppowd(-x, whole);
			}
		} else {
			if(whole > 0){
				const uint32_t idx = (uint32_t)whole;
				if(idx % 2 == 0){
					return ppowdu(-x, idx);
				} else {
					return -ppowdu(-x, idx);
				}
			} else {
				const uint32_t idx = (uint32_t)-whole;
				if(idx % 2 == 0){
					return 1.0 / ppowdu(-x, idx);
				} else {
					return -1.0 / ppowdu(-x, idx);
				}
			}
		}
	}
}
long double powl(long double x, long double y){
	if(y == 0){
		return 1.0l;
	}
	if(x == 0){
		return 0.0l;
	}

	const long double whole = __builtin_floorl(y);
	const long double lrac = y - whole;
	if(x > 0){
		if(y > INT_MAX){
			return ppowl(x, y);
		} else if(y < INT_MIN){
			return 1.0l / ppowl(x, -y);
		} else {
			long double ret;
			if(y > 0){
				ret = ppowlu(x, (uint32_t)whole);
			} else {
				ret = 1.0l / ppowlu(x, (uint32_t)-whole);
			}
			if(lrac != 0){
				ret *= ppowl(x, lrac);
			}
			return ret;
		}
	} else {
		if(lrac != 0){
			return -0.0l / 0.0l;
		}
		if(y > INT_MAX){
			if(__builtin_fmodl(whole, 2.0l) == 0.0l){
				return ppowl(-x, whole);
			} else {
				return -ppowl(-x, whole);
			}
		} else if(y < INT_MIN){
			if(__builtin_fmodl(whole, 2.0l) == 0.0l){
				return 1.0l / ppowl(-x, whole);
			} else {
				return -1.0l / ppowl(-x, whole);
			}
		} else {
			if(whole > 0){
				const uint32_t idx = (uint32_t)whole;
				if(idx % 2 == 0){
					return ppowlu(-x, idx);
				} else {
					return -ppowlu(-x, idx);
				}
			} else {
				const uint32_t idx = (uint32_t)-whole;
				if(idx % 2 == 0){
					return 1.0l / ppowlu(-x, idx);
				} else {
					return -1.0l / ppowlu(-x, idx);
				}
			}
		}
	}
}
