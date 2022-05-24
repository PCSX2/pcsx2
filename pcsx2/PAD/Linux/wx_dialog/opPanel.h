/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <wx/wx.h>

#include "gui/EmbeddedImage.h"

enum gui_img
{
	img_l2,
	img_r2,
	img_l1,
	img_r1,
	img_triangle,
	img_circle,
	img_cross,
	img_square,
	img_select,
	img_l3,
	img_r3,
	img_start,
	img_dp_up,
	img_dp_right,
	img_dp_bottom,
	img_dp_left,
	img_left_cursor,
	img_right_cursor,
	img_analog,
	img_background, // background pic
	img_l_arrow_up,
	img_l_arrow_right,
	img_l_arrow_bottom,
	img_l_arrow_left,
	img_r_arrow_up,
	img_r_arrow_right,
	img_r_arrow_bottom,
	img_r_arrow_left
};

#define NB_IMG 28

class opPanel : public wxPanel
{
	wxBitmap m_picture[NB_IMG];
	bool m_show_image[NB_IMG];
	int m_left_cursor_x, m_left_cursor_y, m_right_cursor_x, m_right_cursor_y;
	wxDECLARE_EVENT_TABLE();
	void OnPaint(wxPaintEvent& event);

public:
	opPanel(wxWindow*, wxWindowID, const wxPoint&, const wxSize&);
	void HideImg(int);
	void ShowImg(int);
	void MoveJoystick(int, int);
};
