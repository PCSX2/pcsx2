// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Common.h"
#include "deci2.h"

void exchangeSD(DECI2_HEADER *h){
	u8	tmp	=h->source;
	h->source	=h->destination;
	h->destination	=tmp;
}
