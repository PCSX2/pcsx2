struct dlggoto_t
{
	HWND hwnd;
	HWND textBox;
	unsigned int address;
};

struct dlgfind_t
{
	HWND hwnd;
	char text[256];
	int type;
};

struct dlglabels_t
{
	HWND hwnd;
	char text[256];
	int curCategory;
	int curSort;
};

struct dlgprogress_t
{
	HWND hwnd;
	char text[256];
};

struct dlgplacestruct_t
{
	HWND hwnd;

	int structDefId;
	int numItems;
};

extern dlggoto_t dlgGoto;
extern dlgfind_t dlgFind;
extern dlglabels_t dlgLabels;
extern dlgprogress_t dlgProgress;
extern dlgplacestruct_t dlgPlaceStruct;

extern int currentDialog;

void CreateProgressDialog(const char* infoText);
void UpdateProgressDialog(int progress); // Works in 1/1000s
void DestroyProgressDialog();

void UpdateLabelDialogLabels();
void SearchLabelDialog(bool fromCurrentSel);