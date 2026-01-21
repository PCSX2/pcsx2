struct mainlist_s
{
	unsigned int address;
	unsigned int markeraddress;

	float scrollSpeed; // In lines per second
	float addressFraction; // Percentage between this line and the next (0.0 to 1.0, used for scrolling)
	unsigned int lastScrollUpdateTime;

	char addresses[600][512];
	char labels[600][512];
	char code[600][512];
	char comments[600][512];
	char datatypes[600];
	unsigned int backColours[600][4];
	unsigned int frontColours[600];
	char numBackColours[600];
	
	int numitems;
	int maxitems;

	int sel;

	bool markervisible;

	unsigned int* addrhist;
	int           addrhistpos;
	int           addrhistmax;
};

extern mainlist_s list;

enum HistFlags
{
	HIST_NOCHANGE = 0,
	HIST_ADDNEW = 1,
	HIST_UPDATEOLD = 2
};

inline void ScrollUp(int amount)
{
	if (! lines) return;
	for (int i = 0; i < amount; i ++)
	{
		if (list.address == 0)
			break;

		int shiftbytes = datasizes[lines[(list.address - 1) / 4].datatype];
		
		if (shiftbytes > list.address)
		{
			list.address = 0;
			break;
		}
	
		list.address -= shiftbytes;
	}

	int ds = datasizes[lines[list.address / 4].datatype];
	list.address = list.address / ds * ds; // Hackfix
}

inline void ScrollDown(int amount)
{
	if (! lines) return;
	for (int i = 0; i < amount; i ++)
	{
		if (list.address + list.maxitems * 4 >= memlen8)
		{
			if (memlen8 >= list.maxitems * 4)
				list.address = memlen8 - (list.maxitems * 4);
			else
			{
				list.address = 0;
				return; // Crashfix hack
			}
			break;
		}

		int shiftbytes = datasizes[lines[list.address / 4].datatype];

		list.address += shiftbytes;
	}

	int ds = datasizes[lines[list.address / 4].datatype];
	list.address = list.address / ds * ds; // Hackfix
}

inline UINT32 GetSelAddress(int sel)
{
	UINT32 addr = list.address;

	if (sel == -1)
		sel = list.sel;

	for (int i = 0; i < sel && addr / 4 < memlen8; i ++)
		addr += datasizes[lines[addr / 4].datatype];

	return addr;
}

inline int GetAddressSel(UINT32 address)
{
	int sel = 0;
	UINT32 addr = list.address;

	if (address < list.address)
		return -1;

	for (sel = 0; sel < list.maxitems && addr < memlen8; sel ++)
	{
		if (addr == address)
			return sel; // Success return is here <---

		addr += datasizes[lines[addr / 4].datatype];
	}

	return -1;
}

void UpdateList();
void UpdateListScroll();

void ClearItems();

void AddrHistAdd();
void AddrHistBack();
void AddrHistForward();

void Goto(UINT32 address, HistFlags histFlags = (HistFlags)(HIST_ADDNEW | HIST_UPDATEOLD));