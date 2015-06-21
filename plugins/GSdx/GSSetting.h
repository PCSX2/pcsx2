/*
 *	Copyright (C) 2007-2015 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#pragma once

#include "stdafx.h"

struct GSSetting
{
	uint32 id;
	std::string name;
	std::string note;


	GSSetting(uint32 id, const char* name, const char* note)
	{
		this->id = id;
		this->name = name;
		this->note = note;
	}
};

const char* dialog_message(int ID, bool* updateText = NULL);

#ifdef __linux__
enum {
	IDC_SKIPDRAWHACK,
	IDC_SKIPDRAWHACKEDIT,
	IDC_STATIC_SKIPDRAW,
	IDC_ALPHAHACK,
	IDC_OFFSETHACK,
	IDC_SPRITEHACK,
	IDC_WILDHACK,
	IDC_MSAACB,
	IDC_STATIC_MSAA,
	IDC_AGGRESSIVECRC,
	IDC_ALPHASTENCIL,
	IDC_CHECK_DISABLE_ALL_HACKS,
	IDC_ALIGN_SPRITE,
	IDC_ROUND_SPRITE,
	IDC_TCOFFSETX,
	IDC_TCOFFSETX2,
	IDC_STATIC_TCOFFSETX,
	IDC_TCOFFSETY,
	IDC_TCOFFSETY2,
	IDC_STATIC_TCOFFSETY,
	IDC_PALTEX,
	IDC_ACCURATE_BLEND,
	IDC_ACCURATE_DATE,
	IDC_ACCURATE_COLCLIP,
	IDC_ACCURATE_FBMASK,
	IDC_CRC_LEVEL
};
#endif
