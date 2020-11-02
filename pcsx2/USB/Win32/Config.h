#ifndef WIN32_H
#define WIN32_H
#include <commctrl.h>

typedef struct Win32Handles
{
	HINSTANCE hInst;
	HWND hWnd;
	Win32Handles(HINSTANCE i, HWND w):
		hInst(i),
		hWnd(w)
	{
	}
} Win32Handles;

#define CHECKED_SET_MAX_INT(var, hDlg, nIDDlgItem, bSigned, min, max)\
do {\
	/*CheckControlTextIsNumber(GetDlgItem(hDlg, nIDDlgItem), bSigned, 0);*/\
	var = GetDlgItemInt(hDlg, nIDDlgItem, NULL, bSigned);\
	if (var < min)\
		var = min;\
	else if (var > max)\
	{\
		var = max;\
		SetDlgItemInt(hDlg, nIDDlgItem, var, bSigned);\
		SendMessage(GetDlgItem(hDlg, nIDDlgItem), EM_SETSEL, -2, -2);\
	}\
} while (0)
#endif