#include "Hacking.h"

enum DataType
{
	DT_BYTE = 0,
	DT_HALF = 1,
	DT_WORD = 2,
	DT_FLOAT = 3,
	DT_STRING = 4
};

enum ScanType
{
	ST_EQUAL = 0,
	ST_MORETHAN = 1,
	ST_LESSTHAN = 2,
	ST_CHANGED = 3,
	ST_UNCHANGED = 4,
	ST_INCREASED = 5,
	ST_DECREASED = 6,
	ST_UNKNOWN = 7
};

extern char* gameSharkCodeString;

extern GameSharkCode* gameSharkCodes;
extern int numGameSharkCodes;

void GameSharkAddCode(UINT32 address, UINT32 value);
void GameSharkUpdateCodes();

void GameSharkScan(const char* value, DataType dataType, ScanType scanType);
void GameSharkResetScanner();