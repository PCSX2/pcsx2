/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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

#include "GSSetting.h"
#include "common/RedtapeWindows.h"

class GSDialog
{
	int m_id;

	static INT_PTR CALLBACK DialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	static UINT GetTooltipStructSize();

protected:
	HWND m_hWnd;

	virtual void OnInit() {}
	virtual bool OnMessage(UINT message, WPARAM wParam, LPARAM lParam);
	virtual bool OnCommand(HWND hWnd, UINT id, UINT code);

public:
	GSDialog(UINT id);
	virtual ~GSDialog() {}

	int GetId() const { return m_id; }

	INT_PTR DoModal();

	std::wstring GetText(UINT id);
	int GetTextAsInt(UINT id);

	void SetText(UINT id, const wchar_t* str);
	void SetTextAsInt(UINT id, int i);

	void ComboBoxInit(UINT id, const std::vector<GSSetting>& settings, int32_t selectionValue, int32_t maxValue = INT32_MAX);
	int ComboBoxAppend(UINT id, const char* str, LPARAM data = 0, bool select = false);
	int ComboBoxAppend(UINT id, const wchar_t* str, LPARAM data = 0, bool select = false);
	bool ComboBoxGetSelData(UINT id, INT_PTR& data);
	void ComboBoxFixDroppedWidth(UINT id);

	void OpenFileDialog(UINT id, const wchar_t* title);

	void AddTooltip(UINT id);

	static void InitCommonControls();

private:
	int BoxAppend(HWND& hWnd, int item, LPARAM data = 0, bool select = false);
};
