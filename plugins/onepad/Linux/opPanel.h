/*  opPanel.h
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

#pragma once

#ifndef __OPPANEL_H__
#define __OPPANEL_H__

#include <wx/wx.h>

#include "EmbeddedImage.h"

enum gui_img {
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
    img_background // background pic
};

#define NB_IMG 20

class opPanel : public wxPanel
{
    wxBitmap picture[NB_IMG];
    int img_size[NB_IMG][2];
    bool show_image[NB_IMG];
    int left_cursor_x, left_cursor_y, right_cursor_x, right_cursor_y;
    void SaveSize(int);
    DECLARE_EVENT_TABLE()
    void OnPaint(wxPaintEvent& event);

public:
    opPanel(wxWindow*, wxWindowID, const wxPoint&, const wxSize&);
    void HideImg(int);
    void ShowImg(int);
    void MoveJoystick(int, int);
};

#endif // __OPPANEL_H__
