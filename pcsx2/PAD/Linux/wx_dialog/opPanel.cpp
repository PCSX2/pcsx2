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

#include "opPanel.h"
#include "ImgHeader/dualshock2.h"
#include "ImgHeader/cross.h"
#include "ImgHeader/circle.h"
#include "ImgHeader/square.h"
#include "ImgHeader/triangle.h"
#include "ImgHeader/dp_left.h"
#include "ImgHeader/dp_right.h"
#include "ImgHeader/dp_up.h"
#include "ImgHeader/dp_bottom.h"
#include "ImgHeader/l1.h"
#include "ImgHeader/r1.h"
#include "ImgHeader/l2.h"
#include "ImgHeader/l3.h"
#include "ImgHeader/r2.h"
#include "ImgHeader/r3.h"
#include "ImgHeader/start.h"
#include "ImgHeader/select.h"
#include "ImgHeader/analog.h"
#include "ImgHeader/joystick_cursor.h"
#include "ImgHeader/arrow_up.h"
#include "ImgHeader/arrow_right.h"
#include "ImgHeader/arrow_bottom.h"
#include "ImgHeader/arrow_left.h"

opPanel::opPanel(wxWindow* parent,
				 wxWindowID id = wxID_ANY,
				 const wxPoint& pos = wxDefaultPosition,
				 const wxSize& size = wxDefaultSize)
	: wxPanel(parent, id, pos, size)
{
	m_picture[img_background] = EmbeddedImage<res_dualshock2>().Get();

	m_picture[img_start] = EmbeddedImage<res_start>().Get();
	m_picture[img_select] = EmbeddedImage<res_select>().Get();
	m_picture[img_analog] = EmbeddedImage<res_analog>().Get();

	m_picture[img_dp_left] = EmbeddedImage<res_dp_left>().Get();
	m_picture[img_dp_right] = EmbeddedImage<res_dp_right>().Get();
	m_picture[img_dp_up] = EmbeddedImage<res_dp_up>().Get();
	m_picture[img_dp_bottom] = EmbeddedImage<res_dp_bottom>().Get();

	m_picture[img_square] = EmbeddedImage<res_square>().Get();
	m_picture[img_circle] = EmbeddedImage<res_circle>().Get();
	m_picture[img_cross] = EmbeddedImage<res_cross>().Get();
	m_picture[img_triangle] = EmbeddedImage<res_triangle>().Get();

	m_picture[img_l1] = EmbeddedImage<res_l1>().Get();
	m_picture[img_l3] = EmbeddedImage<res_l3>().Get();
	m_picture[img_l2] = EmbeddedImage<res_l2>().Get();

	m_picture[img_r1] = EmbeddedImage<res_r1>().Get();
	m_picture[img_r3] = EmbeddedImage<res_r3>().Get();
	m_picture[img_r2] = EmbeddedImage<res_r2>().Get();

	m_picture[img_left_cursor] = EmbeddedImage<res_joystick_cursor>().Get();
	m_picture[img_right_cursor] = EmbeddedImage<res_joystick_cursor>().Get();

	m_picture[img_l_arrow_up] = EmbeddedImage<res_arrow_up>().Get();
	m_picture[img_l_arrow_right] = EmbeddedImage<res_arrow_right>().Get();
	m_picture[img_l_arrow_bottom] = EmbeddedImage<res_arrow_bottom>().Get();
	m_picture[img_l_arrow_left] = EmbeddedImage<res_arrow_left>().Get();

	m_picture[img_r_arrow_up] = EmbeddedImage<res_arrow_up>().Get();
	m_picture[img_r_arrow_right] = EmbeddedImage<res_arrow_right>().Get();
	m_picture[img_r_arrow_bottom] = EmbeddedImage<res_arrow_bottom>().Get();
	m_picture[img_r_arrow_left] = EmbeddedImage<res_arrow_left>().Get();

	for (int i = 0; i < NB_IMG; ++i)
	{
		m_show_image[i] = false;
		HideImg(i);
	}
	ShowImg(img_background);
	m_show_image[img_background] = true;

	m_left_cursor_x = 0;
	m_left_cursor_y = 0;
	m_right_cursor_x = 0;
	m_right_cursor_y = 0;
}

void opPanel::HideImg(int id)
{
	if (id < NB_IMG)
	{
		m_show_image[id] = false;
		Refresh();
	}
}

void opPanel::ShowImg(int id)
{
	if (id < NB_IMG)
	{
		m_show_image[id] = true;
		Refresh();
	}
}

void opPanel::MoveJoystick(int axe, int value)
{
	if (axe == 0)
	{
		m_left_cursor_x = value * 30 / 40000;
	}
	else if (axe == 1)
	{
		m_left_cursor_y = value * 30 / 40000;
	}
	else if (axe == 2)
	{
		m_right_cursor_x = value * 30 / 40000;
	}
	else
	{
		m_right_cursor_y = value * 30 / 40000;
	}
}

wxBEGIN_EVENT_TABLE(opPanel, wxPanel)
	EVT_PAINT(opPanel::OnPaint)
		wxEND_EVENT_TABLE()

			void opPanel::OnPaint(wxPaintEvent& event)
{
	wxPaintDC dc(this);

	wxMemoryDC temp_background, temp_start, temp_select, temp_analog, temp_dp_left,
		temp_dp_right, temp_dp_up, temp_dp_bottom, temp_l1, temp_r1, temp_L3, temp_l2_2,
		temp_R3, temp_r2_2, temp_square, temp_circle, temp_cross, temp_triangle,
		temp_left_cursor, temp_right_cursor, temp_l_arrow_up, temp_l_arrow_right,
		temp_l_arrow_bottom, temp_l_arrow_left, temp_r_arrow_up, temp_r_arrow_right,
		temp_r_arrow_bottom, temp_r_arrow_left;

	temp_background.SelectObject(m_picture[img_background]);
	temp_start.SelectObject(m_picture[img_start]);
	temp_select.SelectObject(m_picture[img_select]);
	temp_analog.SelectObject(m_picture[img_analog]);
	temp_dp_left.SelectObject(m_picture[img_dp_left]);

	temp_dp_right.SelectObject(m_picture[img_dp_right]);
	temp_dp_up.SelectObject(m_picture[img_dp_up]);
	temp_dp_bottom.SelectObject(m_picture[img_dp_bottom]);
	temp_l1.SelectObject(m_picture[img_l1]);
	temp_r1.SelectObject(m_picture[img_r1]);
	temp_L3.SelectObject(m_picture[img_l3]);
	temp_l2_2.SelectObject(m_picture[img_l2]);

	temp_R3.SelectObject(m_picture[img_r3]);
	temp_r2_2.SelectObject(m_picture[img_r2]);
	temp_square.SelectObject(m_picture[img_square]);
	temp_circle.SelectObject(m_picture[img_circle]);
	temp_cross.SelectObject(m_picture[img_cross]);
	temp_triangle.SelectObject(m_picture[img_triangle]);

	temp_left_cursor.SelectObject(m_picture[img_left_cursor]);
	temp_right_cursor.SelectObject(m_picture[img_right_cursor]);

	temp_l_arrow_up.SelectObject(m_picture[img_l_arrow_up]);
	temp_l_arrow_right.SelectObject(m_picture[img_l_arrow_right]);
	temp_l_arrow_bottom.SelectObject(m_picture[img_l_arrow_bottom]);
	temp_l_arrow_left.SelectObject(m_picture[img_l_arrow_left]);

	temp_r_arrow_up.SelectObject(m_picture[img_r_arrow_up]);
	temp_r_arrow_right.SelectObject(m_picture[img_r_arrow_right]);
	temp_r_arrow_bottom.SelectObject(m_picture[img_r_arrow_bottom]);
	temp_r_arrow_left.SelectObject(m_picture[img_r_arrow_left]);

	if (m_show_image[img_background])
		dc.Blit(wxPoint(0, 0), temp_background.GetSize(), &temp_background, wxPoint(0, 0), wxCOPY, true);
	if (m_show_image[img_start])
		dc.Blit(wxPoint(526, 296), temp_start.GetSize(), &temp_start, wxPoint(0, 0), wxCOPY, true);
	if (m_show_image[img_select])
		dc.Blit(wxPoint(450, 297), temp_select.GetSize(), &temp_select, wxPoint(0, 0), wxCOPY, true);
	if (m_show_image[img_analog])
		dc.Blit(wxPoint(489, 358), temp_analog.GetSize(), &temp_analog, wxPoint(0, 0), wxCOPY, true);
	if (m_show_image[img_dp_left])
		dc.Blit(wxPoint(334, 292), temp_dp_left.GetSize(), &temp_dp_left, wxPoint(0, 0), wxCOPY, true);
	if (m_show_image[img_dp_right])
		dc.Blit(wxPoint(378, 292), temp_dp_right.GetSize(), &temp_dp_right, wxPoint(0, 0), wxCOPY, true);
	if (m_show_image[img_dp_up])
		dc.Blit(wxPoint(358, 269), temp_dp_up.GetSize(), &temp_dp_up, wxPoint(0, 0), wxCOPY, true);
	if (m_show_image[img_dp_bottom])
		dc.Blit(wxPoint(358, 312), temp_dp_bottom.GetSize(), &temp_dp_bottom, wxPoint(0, 0), wxCOPY, true);
	if (m_show_image[img_l1])
		dc.Blit(wxPoint(343, 186), temp_l1.GetSize(), &temp_l1, wxPoint(0, 0), wxCOPY, true);
	if (m_show_image[img_r1])
		dc.Blit(wxPoint(593, 186), temp_r1.GetSize(), &temp_r1, wxPoint(0, 0), wxCOPY, true);
	if (m_show_image[img_l3])
		dc.Blit(wxPoint(409, 344), temp_L3.GetSize(), &temp_L3, wxPoint(0, 0), wxCOPY, true);
	if (m_show_image[img_l2])
		dc.Blit(wxPoint(346, 158), temp_l2_2.GetSize(), &temp_l2_2, wxPoint(0, 0), wxCOPY, true);
	if (m_show_image[img_r3])
		dc.Blit(wxPoint(525, 344), temp_R3.GetSize(), &temp_R3, wxPoint(0, 0), wxCOPY, true);
	if (m_show_image[img_r2])
		dc.Blit(wxPoint(582, 158), temp_r2_2.GetSize(), &temp_r2_2, wxPoint(0, 0), wxCOPY, true);
	if (m_show_image[img_square])
		dc.Blit(wxPoint(573, 287), temp_square.GetSize(), &temp_square, wxPoint(0, 0), wxCOPY, true);
	if (m_show_image[img_circle])
		dc.Blit(wxPoint(647, 287), temp_circle.GetSize(), &temp_circle, wxPoint(0, 0), wxCOPY, true);
	if (m_show_image[img_cross])
		dc.Blit(wxPoint(610, 324), temp_cross.GetSize(), &temp_cross, wxPoint(0, 0), wxCOPY, true);
	if (m_show_image[img_triangle])
		dc.Blit(wxPoint(610, 250), temp_triangle.GetSize(), &temp_triangle, wxPoint(0, 0), wxCOPY, true);
	if (m_show_image[img_left_cursor])
		dc.Blit(wxPoint(439 + m_left_cursor_x, 374 + m_left_cursor_y), temp_left_cursor.GetSize(), &temp_left_cursor, wxPoint(0, 0), wxCOPY, true);
	if (m_show_image[img_right_cursor])
		dc.Blit(wxPoint(555 + m_right_cursor_x, 374 + m_right_cursor_y), temp_right_cursor.GetSize(), &temp_right_cursor, wxPoint(0, 0), wxCOPY, true);

	if (m_show_image[img_l_arrow_up])
		dc.Blit(wxPoint(433, 357), temp_l_arrow_up.GetSize(), &temp_l_arrow_up, wxPoint(0, 0), wxCOPY, true);
	if (m_show_image[img_l_arrow_right])
		dc.Blit(wxPoint(423, 368), temp_l_arrow_right.GetSize(), &temp_l_arrow_right, wxPoint(0, 0), wxCOPY, true);
	if (m_show_image[img_l_arrow_bottom])
		dc.Blit(wxPoint(433, 357), temp_l_arrow_bottom.GetSize(), &temp_l_arrow_bottom, wxPoint(0, 0), wxCOPY, true);
	if (m_show_image[img_l_arrow_left])
		dc.Blit(wxPoint(423, 368), temp_l_arrow_left.GetSize(), &temp_l_arrow_left, wxPoint(0, 0), wxCOPY, true);

	if (m_show_image[img_r_arrow_up])
		dc.Blit(wxPoint(548, 357), temp_r_arrow_up.GetSize(), &temp_r_arrow_up, wxPoint(0, 0), wxCOPY, true);
	if (m_show_image[img_r_arrow_right])
		dc.Blit(wxPoint(539, 368), temp_r_arrow_right.GetSize(), &temp_r_arrow_right, wxPoint(0, 0), wxCOPY, true);
	if (m_show_image[img_r_arrow_bottom])
		dc.Blit(wxPoint(548, 357), temp_r_arrow_bottom.GetSize(), &temp_r_arrow_bottom, wxPoint(0, 0), wxCOPY, true);
	if (m_show_image[img_r_arrow_left])
		dc.Blit(wxPoint(539, 368), temp_r_arrow_left.GetSize(), &temp_r_arrow_left, wxPoint(0, 0), wxCOPY, true);
}
