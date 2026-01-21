#include <Windows.h>

struct wndbreak_t
{
	HWND hwnd;

	HWND edRegs1;
	HWND stRegs[32];
	HWND liCallStack;
	HWND btContinue, btStep, btStepOver;
	HWND btAuto;
	HWND stStatus;
	HWND edReadBreak, edWriteBreak;
	HWND btReadBreak, btWriteBreak;
	HWND gbCallStack, gbRegs;
	LONG procGbCallStack, procGbRegs;

	bool continuePressed, stepPressed, stepOverPressed;
	bool autoContinue;
	bool isOpen;
	bool isUpdated;
};

struct wndgameshark_t
{
	HWND hwnd;

	HWND gbActiveCodes, gbCodeScanner;
	LONG procGbActiveCodes, procGbCodeScanner;
	
	HWND btScan, btReset;
	HWND btOk, btCancel, btApply;
	HWND btHex;

	HWND cbScanMode, cbDataType;
	HWND edSearch;

	HWND lbResults;

	HWND edCodes;

	HMENU dropMenu;
};

struct wndstruct_t
{
	HWND hwnd;

	HWND edScript;
	HWND edName, edSize;

	HWND btOk, btCancel;

	int editingId; // -1 if creating a new struct; otherwise the ID of the struct being edited
};

struct wndstructmanager_t
{
	HWND hwnd;

	HWND lbStructs;
	HWND btAdd, btEdit, btDelete, btOk;
};

struct wndedit_t
{
	HWND hwnd;

	UINT32 address;
	INT32 section;
};

extern wndedit_t wndEdit;
extern wndbreak_t wndBreak;
extern wndgameshark_t wndGameShark;
extern wndstruct_t wndStruct;
extern wndstructmanager_t wndStructManager;

extern HWND main_hwnd;
extern HWND listbox_hwnd;
extern HWND textbox_hwnd;

extern int sectionx[6];

extern int listbox_y;

extern HINSTANCE globalInst;

extern HFONT global_mainfont;
extern HFONT global_listfont;
extern HFONT global_boldlistfont;
extern HFONT global_gotoaddrfont;

void UpdateTextbox();

void CreateEditWindow(int section, int selection);
void RepositionEditWindow();
void SetEditWindowText(const char* text);

void ShowBreakWindow(bool show);
void GetBreakWindowText(char* out, int maxChars);
void SetBreakWindowText(const char* in);
void AddBreakWindowCall(const char* in);
void ClearBreakWindowCalls();
void SetBreakWindowStatus(const char* status);
UINT32 GetBreakWindowReadBreakpoint();
UINT32 GetBreakWindowWriteBreakpoint();
void SetBreakWindowReadBreakpoint(UINT32 address);
void SetBreakWindowWriteBreakpoint(UINT32 address);
void UpdateBreakWindowRegs();

void CreateStructManagerWindow();
void ShowStructManagerWindow(bool show);
void UpdateStructManagerWindow();

bool ProcessKey(int key);

bool Do_GoToAddress();
bool Do_GoToLabel();
bool Do_Find();
bool Do_EditRegs();
bool Do_SetDataTypes();
bool Do_PlaceStruct();
void Do_CreateStruct(int editingId);