#include "osdebugout.h"

std::wostream& operator<<(std::wostream& os, const std::string& s) {
	std::wstring ws;
	ws.assign(s.begin(), s.end());
	return os << ws;
}

#ifdef _WIN32
static int rateLimit = 0;
void _OSDebugOut(const TCHAR *psz_fmt, ...)
{
	if(rateLimit > 0 && rateLimit < 100)
	{
		rateLimit++;
		return;
	}
	else
	{
		//rateLimit = 1;
	}

	va_list args;
	va_start(args, psz_fmt);

#ifdef UNICODE
	int bufsize = _vscwprintf(psz_fmt, args) + 1;
	std::vector<WCHAR> msg(bufsize);
	vswprintf_s(&msg[0], bufsize, psz_fmt, args);
#else
	int bufsize = _vscprintf(psz_fmt, args) + 1;
	std::vector<char> msg(bufsize);
	vsprintf_s(&msg[0], bufsize, psz_fmt, args);
#endif

	//_vsnwprintf_s(&msg[0], bufsize, bufsize-1, psz_fmt, args);
	va_end(args);
	//static FILE *hfile = nullptr;
	//if (!hfile) {
	//	hfile = _wfopen(L"USBqemu-wheel.log", L"wb,ccs=UNICODE");
	//	if (!hfile) throw std::runtime_error("ass");
	//}
	//else
	//	fwprintf(hfile, L"%s", &msg[0]);
	OutputDebugString(&msg[0]);
}
#endif