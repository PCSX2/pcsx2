// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "BaseblockEx.h"

BASEBLOCKEX* BaseBlocks::New(u32 startpc, uptr fnptr)
{
	std::pair<linkiter_t, linkiter_t> range = links.equal_range(startpc);
	for (linkiter_t i = range.first; i != range.second; ++i)
		*(u32*)i->second = fnptr - (i->second + 4);

	return blocks.insert(startpc, fnptr);
}

int BaseBlocks::LastIndex(u32 startpc) const
{
	if (0 == blocks.size())
		return -1;

	int imin = 0, imax = blocks.size() - 1;

	while (imin != imax)
	{
		const int imid = (imin + imax + 1) >> 1;

		if (blocks[imid].startpc > startpc)
			imax = imid - 1;
		else
			imin = imid;
	}

	return imin;
}

#if 0
BASEBLOCKEX* BaseBlocks::GetByX86(uptr ip)
{
	if (0 == blocks.size())
		return 0;

	int imin = 0, imax = blocks.size() - 1;

	while(imin != imax) {
		const int imid = (imin+imax+1)>>1;

		if (blocks[imid].fnptr > ip)
			imax = imid - 1;
		else
			imin = imid;
	}

	if (ip < blocks[imin].fnptr ||
		ip >= blocks[imin].fnptr + blocks[imin].x86size)
		return 0;

	return &blocks[imin];
}
#endif

void BaseBlocks::Link(u32 pc, s32* jumpptr)
{
	BASEBLOCKEX* targetblock = Get(pc);
	if (targetblock && targetblock->startpc == pc)
		*jumpptr = (s32)(targetblock->fnptr - (sptr)(jumpptr + 1));
	else
		*jumpptr = (s32)(recompiler - (sptr)(jumpptr + 1));
	links.insert(std::pair<u32, uptr>(pc, (uptr)jumpptr));
}
