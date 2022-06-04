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

#include "PrecompiledHeader.h"

#include "common/StringUtil.h"

#include "Frontend/GameList.h"

#include "GameSummaryWidget.h"
#include "QtHost.h"

GameSummaryWidget::GameSummaryWidget(const GameList::Entry* entry, SettingsDialog* dialog, QWidget* parent)
{
	m_ui.setupUi(this);

	const QString base_path(QtHost::GetResourcesBasePath());
	for (int i = 0; i < m_ui.region->count(); i++)
	{
		m_ui.region->setItemIcon(i, QIcon(
										QStringLiteral("%1/icons/flags/%2.png").arg(base_path).arg(GameList::RegionToString(static_cast<GameList::Region>(i)))));
	}
	for (int i = 1; i < m_ui.compatibility->count(); i++)
	{
		m_ui.compatibility->setItemIcon(i, QIcon(
											   QStringLiteral("%1/icons/star-%2.png").arg(base_path).arg(i)));
	}

	populateUi(entry);
}

GameSummaryWidget::~GameSummaryWidget() = default;

void GameSummaryWidget::populateUi(const GameList::Entry* entry)
{
	m_ui.title->setText(QString::fromStdString(entry->title));
	m_ui.path->setText(QString::fromStdString(entry->path));
	m_ui.serial->setText(QString::fromStdString(entry->serial));
	m_ui.crc->setText(QString::fromStdString(StringUtil::StdStringFromFormat("%08X", entry->crc)));
	m_ui.type->setCurrentIndex(static_cast<int>(entry->type));
	m_ui.region->setCurrentIndex(static_cast<int>(entry->region));
	m_ui.compatibility->setCurrentIndex(static_cast<int>(entry->compatibility_rating));
}
