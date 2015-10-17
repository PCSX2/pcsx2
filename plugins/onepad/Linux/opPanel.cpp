/*  opPanel.cpp
 *  Copyright (C) 2015
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
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

opPanel::opPanel(wxWindow *parent,
                 wxWindowID id=wxID_ANY,
                 const wxPoint &pos=wxDefaultPosition,
                 const wxSize &size=wxDefaultSize
                ): wxPanel( parent, id, pos, size)
{
    this->picture[img_background] = EmbeddedImage<res_dualshock2>().GetIcon();

    this->picture[img_start] = EmbeddedImage<res_start>().GetIcon();
    this->picture[img_select] = EmbeddedImage<res_select>().GetIcon();
    this->picture[img_analog] = EmbeddedImage<res_analog>().GetIcon();

    this->picture[img_dp_left] = EmbeddedImage<res_dp_left>().GetIcon();
    this->picture[img_dp_right] = EmbeddedImage<res_dp_right>().GetIcon();
    this->picture[img_dp_up] = EmbeddedImage<res_dp_up>().GetIcon();
    this->picture[img_dp_bottom] = EmbeddedImage<res_dp_bottom>().GetIcon();

    this->picture[img_square] = EmbeddedImage<res_square>().GetIcon();
    this->picture[img_circle] = EmbeddedImage<res_circle>().GetIcon();
    this->picture[img_cross] = EmbeddedImage<res_cross>().GetIcon();
    this->picture[img_triangle] = EmbeddedImage<res_triangle>().GetIcon();

    this->picture[img_l1] = EmbeddedImage<res_l1>().GetIcon();
    this->picture[img_l3] = EmbeddedImage<res_l3>().GetIcon();
    this->picture[img_l2] = EmbeddedImage<res_l2>().GetIcon();

    this->picture[img_r1] = EmbeddedImage<res_r1>().GetIcon();
    this->picture[img_r3] = EmbeddedImage<res_r3>().GetIcon();
    this->picture[img_r2] = EmbeddedImage<res_r2>().GetIcon();

    this->picture[img_left_cursor] = EmbeddedImage<res_joystick_cursor>().GetIcon();
    this->picture[img_right_cursor] = EmbeddedImage<res_joystick_cursor>().GetIcon();

    for(int i=0; i<NB_IMG; ++i)
    {
        this->show_image[i] = false;
        this->SaveSize(i);
        this->HideImg(i);
    }
    this->ShowImg(img_background);
    this->show_image[img_background] = true;

    this->left_cursor_x = 0;
    this->left_cursor_y = 0;
    this->right_cursor_x = 0;
    this->right_cursor_y = 0;
}

void opPanel::SaveSize(int id)
{
    this->img_size[id][0] = this->picture[id].GetWidth();
    this->img_size[id][1] = this->picture[id].GetHeight();
}

void opPanel::HideImg(int id)
{
    this->show_image[id] = false;
    this->Refresh();
}

void opPanel::ShowImg(int id)
{
    this->show_image[id] = true;
    this->Refresh();
}

void opPanel::MoveJoystick(int axe,int value)
{
    if(axe == 0)
    {
        this->left_cursor_x = value*30/40000;
    }
    else if(axe == 1)
    {
        this->left_cursor_y = value*30/40000;
    }
    else if( axe == 2)
    {
        this->right_cursor_x = value*30/40000;
    }
    else
    {
        this->right_cursor_y = value*30/40000;
    }
}

BEGIN_EVENT_TABLE(opPanel, wxPanel)
    EVT_PAINT(opPanel::OnPaint)
END_EVENT_TABLE()

void opPanel::OnPaint(wxPaintEvent& event)
{
    wxPaintDC dc(this);
    wxSize sz = GetClientSize();

    wxMemoryDC temp_background, temp_start, temp_select, temp_analog, temp_dp_left,
    temp_dp_right, temp_dp_up, temp_dp_bottom, temp_l1, temp_r1, temp_L3, temp_l2_2,
    temp_R3, temp_r2_2, temp_square, temp_circle, temp_cross, temp_triangle,
    temp_left_cursor, temp_right_cursor;

    temp_background.SelectObject(this->picture[img_background]);
    temp_start.SelectObject(this->picture[img_start]);
    temp_select.SelectObject(this->picture[img_select]);
    temp_analog.SelectObject(this->picture[img_analog]);
    temp_dp_left.SelectObject(this->picture[img_dp_left]);

    temp_dp_right.SelectObject(this->picture[img_dp_right]);
    temp_dp_up.SelectObject(this->picture[img_dp_up]);
    temp_dp_bottom.SelectObject(this->picture[img_dp_bottom]);
    temp_l1.SelectObject(this->picture[img_l1]);
    temp_r1.SelectObject(this->picture[img_r1]);
    temp_L3.SelectObject(this->picture[img_l3]);
    temp_l2_2.SelectObject(this->picture[img_l2]);

    temp_R3.SelectObject(this->picture[img_r3]);
    temp_r2_2.SelectObject(this->picture[img_r2]);
    temp_square.SelectObject(this->picture[img_square]);
    temp_circle.SelectObject(this->picture[img_circle]);
    temp_cross.SelectObject(this->picture[img_cross]);
    temp_triangle.SelectObject(this->picture[img_triangle]);

    temp_left_cursor.SelectObject(this->picture[img_left_cursor]);
    temp_right_cursor.SelectObject(this->picture[img_right_cursor]);

    if(this->show_image[img_background])
        dc.Blit(wxPoint(0, 0), temp_background.GetSize(), &temp_background, wxPoint(0, 0), wxCOPY, true);
    if(this->show_image[img_start])
        dc.Blit(wxPoint(526, 296), temp_start.GetSize(), &temp_start, wxPoint(0, 0), wxCOPY, true);
    if(this->show_image[img_select])
        dc.Blit(wxPoint(450, 297), temp_select.GetSize(), &temp_select, wxPoint(0, 0), wxCOPY, true);
    if(this->show_image[img_analog])
        dc.Blit(wxPoint(489, 358), temp_analog.GetSize(), &temp_analog, wxPoint(0, 0), wxCOPY, true);
    if(this->show_image[img_dp_left])
        dc.Blit(wxPoint(335, 292), temp_dp_left.GetSize(), &temp_dp_left, wxPoint(0, 0), wxCOPY, true);
    if(this->show_image[img_dp_right])
        dc.Blit(wxPoint(378, 292), temp_dp_right.GetSize(), &temp_dp_right, wxPoint(0, 0), wxCOPY, true);
    if(this->show_image[img_dp_up])
        dc.Blit(wxPoint(358, 269), temp_dp_up.GetSize(), &temp_dp_up, wxPoint(0, 0), wxCOPY, true);
    if(this->show_image[img_dp_bottom])
        dc.Blit(wxPoint(358, 312), temp_dp_bottom.GetSize(), &temp_dp_bottom, wxPoint(0, 0), wxCOPY, true);
    if(this->show_image[img_l1])
        dc.Blit(wxPoint(343, 186), temp_l1.GetSize(), &temp_l1, wxPoint(0, 0), wxCOPY, true);
    if(this->show_image[img_r1])
        dc.Blit(wxPoint(594, 186), temp_r1.GetSize(), &temp_r1, wxPoint(0, 0), wxCOPY, true);
    if(this->show_image[img_l3])
        dc.Blit(wxPoint(409, 344), temp_L3.GetSize(), &temp_L3, wxPoint(0, 0), wxCOPY, true);
    if(this->show_image[img_l2])
        dc.Blit(wxPoint(347, 158), temp_l2_2.GetSize(), &temp_l2_2, wxPoint(0, 0), wxCOPY, true);
    if(this->show_image[img_r3])
        dc.Blit(wxPoint(525, 344), temp_R3.GetSize(), &temp_R3, wxPoint(0, 0), wxCOPY, true);
    if(this->show_image[img_r2])
        dc.Blit(wxPoint(581, 158), temp_r2_2.GetSize(), &temp_r2_2, wxPoint(0, 0), wxCOPY, true);
    if(this->show_image[img_square])
        dc.Blit(wxPoint(573, 287), temp_square.GetSize(), &temp_square, wxPoint(0, 0), wxCOPY, true);
    if(this->show_image[img_circle])
        dc.Blit(wxPoint(647, 287), temp_circle.GetSize(), &temp_circle, wxPoint(0, 0), wxCOPY, true);
    if(this->show_image[img_cross])
        dc.Blit(wxPoint(610, 324), temp_cross.GetSize(), &temp_cross, wxPoint(0, 0), wxCOPY, true);
    if(this->show_image[img_triangle])
        dc.Blit(wxPoint(610, 250), temp_triangle.GetSize(), &temp_triangle, wxPoint(0, 0), wxCOPY, true);
    if(this->show_image[img_left_cursor])
        dc.Blit(wxPoint(439+this->left_cursor_x, 374+this->left_cursor_y), temp_left_cursor.GetSize(), &temp_left_cursor, wxPoint(0, 0), wxCOPY, true);
    if(this->show_image[img_right_cursor])
        dc.Blit(wxPoint(555+this->right_cursor_x, 374+this->right_cursor_y), temp_right_cursor.GetSize(), &temp_right_cursor, wxPoint(0, 0), wxCOPY, true);
}
