#include <tchar.h>

#include "errors.hpp"
#include "version.h"

extern "C" int __cdecl _purecall() {
	showError(NULL, _T(APPLICATION_TITLE) _T(": fatal error"), _T("Pure virtual function called. Aborting."));
#ifdef _DEBUG
	return 0;
#else
	abort();
#endif
}
