// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Common.h"
#include "deci2.h"

void exchangeSD(DECI2_HEADER *h){
	u8	tmp	=h->source;
	h->source	=h->destination;
	h->destination	=tmp;
}
