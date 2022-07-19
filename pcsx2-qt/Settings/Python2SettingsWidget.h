/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#include <QtWidgets/QWidget>

#include "ui_Python2SettingsWidget.h"

namespace GameList
{
	struct Entry;
}

class SettingsDialog;

class Python2SettingsWidget : public QWidget
{
	Q_OBJECT

private Q_SLOTS:
	void onGameTypeChanged(int index);
	void onHddIdBrowseClicked();
	void onIlinkIdBrowseClicked();
	void onDongleBlackBrowseClicked();
	void onDongleWhiteBrowseClicked();
	void onPatchFileBrowseClicked();
	void onPlayer1CardBrowseClicked();
	void onPlayer2CardBrowseClicked();

public:
	Python2SettingsWidget(const GameList::Entry* entry, SettingsDialog* dialog, QWidget* parent);
	~Python2SettingsWidget();

private:
	Ui::Python2SettingsWidget m_ui;
	SettingsDialog* m_dialog;
};
