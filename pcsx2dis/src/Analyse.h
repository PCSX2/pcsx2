struct value_t
{
	UINT32 v;
	char str[256];
	bool known;
};

struct reg_t
{
	char name[17];
	char currentname[50];
	value_t value;
};

extern reg_t regs[32];

void SetExactValue(reg_t* r, UINT32 value);

// Analyse region: Main analysis function
void AnalyseRegion(UINT32 start, UINT32 end, bool setcomments);
void AnalyseVisible();

void EvalInstruction(UINT32 code, UINT32 address, bool setcomments);
void ClearRegisters();

UINT32 FindFunctionStart(UINT32 address);
UINT32 FindFunctionEnd(UINT32 address);

void FindLabels();
void FindReferences();
void FindFunctions();
void FindPattern(const char* string, int type, bool reverse = 0);
void FindGp();

void UpdateReference(UINT32 address);