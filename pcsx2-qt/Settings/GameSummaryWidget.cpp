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
#include "PAD/Host/PAD.h"

#include "GameSummaryWidget.h"
#include "SettingsDialog.h"
#include "MainWindow.h"
#include "QtHost.h"
#include "QtUtils.h"

#include <QtCore/QDir>
#include <QtWidgets/QFileDialog>

#include "fmt/format.h"

GameSummaryWidget::GameSummaryWidget(const GameList::Entry* entry, SettingsDialog* dialog, QWidget* parent)
	: m_dialog(dialog)
{
	m_ui.setupUi(this);

	const QString base_path(QtHost::GetResourcesBasePath());
	for (int i = 0; i < m_ui.region->count(); i++)
	{
		m_ui.region->setItemIcon(i,
			QIcon(QStringLiteral("%1/icons/flags/%2.png").arg(base_path).arg(GameList::RegionToString(static_cast<GameList::Region>(i)))));
	}
	for (int i = 1; i < m_ui.compatibility->count(); i++)
	{
		m_ui.compatibility->setItemIcon(i, QIcon(QStringLiteral("%1/icons/star-%2.png").arg(base_path).arg(i)));
	}

	m_entry_path = entry->path;
	populateInputProfiles();
	populateDetails(entry);
	populateDiscPath(entry);

	connect(m_ui.inputProfile, &QComboBox::currentIndexChanged, this, &GameSummaryWidget::onInputProfileChanged);
}

GameSummaryWidget::~GameSummaryWidget() = default;

void GameSummaryWidget::populateInputProfiles()
{
	for (const std::string& name : PAD::GetInputProfileNames())
		m_ui.inputProfile->addItem(QString::fromStdString(name));
}

void GameSummaryWidget::populateDetails(const GameList::Entry* entry)
{
	m_ui.title->setText(QString::fromStdString(entry->title));
	m_ui.path->setText(QString::fromStdString(entry->path));
	m_ui.serial->setText(QString::fromStdString(entry->serial));
	m_ui.crc->setText(QString::fromStdString(fmt::format("{:08X}", entry->crc)));
	m_ui.type->setCurrentIndex(static_cast<int>(entry->type));
	m_ui.region->setCurrentIndex(static_cast<int>(entry->region));
	m_ui.compatibility->setCurrentIndex(static_cast<int>(entry->compatibility_rating));

	std::optional<std::string> profile(m_dialog->getStringValue("EmuCore", "InputProfileName", std::nullopt));
	if (profile.has_value())
		m_ui.inputProfile->setCurrentIndex(m_ui.inputProfile->findText(QString::fromStdString(profile.value())));
	else
		m_ui.inputProfile->setCurrentIndex(0);
}

void GameSummaryWidget::populateDiscPath(const GameList::Entry* entry)
{
	if (entry->type == GameList::EntryType::ELF)
	{
		std::optional<std::string> iso_path(m_dialog->getStringValue("EmuCore", "DiscPath", std::nullopt));
		if (iso_path.has_value() && !iso_path->empty())
			m_ui.discPath->setText(QString::fromStdString(iso_path.value()));

		connect(m_ui.discPath, &QLineEdit::textChanged, this, &GameSummaryWidget::onDiscPathChanged);
		connect(m_ui.discPathBrowse, &QPushButton::clicked, this, &GameSummaryWidget::onDiscPathBrowseClicked);
		connect(m_ui.discPathClear, &QPushButton::clicked, m_ui.discPath, &QLineEdit::clear);
	}
	else
	{
		// Makes no sense to have disc override for a disc.
		m_ui.detailsFormLayout->removeRow(8);
		m_ui.discPath = nullptr;
		m_ui.discPathBrowse = nullptr;
		m_ui.discPathClear = nullptr;
	}
}

void GameSummaryWidget::onInputProfileChanged(int index)
{
	if (index == 0)
		m_dialog->setStringSettingValue("EmuCore", "InputProfileName", std::nullopt);
	else
		m_dialog->setStringSettingValue("EmuCore", "InputProfileName", m_ui.inputProfile->itemText(index).toUtf8());
}

void GameSummaryWidget::onDiscPathChanged(const QString& value)
{
	if (value.isEmpty())
		m_dialog->removeSettingValue("EmuCore", "DiscPath");
	else
		m_dialog->setStringSettingValue("EmuCore", "DiscPath", value.toStdString().c_str());

	// force rescan of elf to update the serial
	g_main_window->rescanFile(m_entry_path);

	// and re-fill our details (mainly the serial)
	auto lock = GameList::GetLock();
	const GameList::Entry* entry = GameList::GetEntryForPath(m_entry_path.c_str());
	if (entry)
		populateDetails(entry);
}

void GameSummaryWidget::onDiscPathBrowseClicked()
{
	const QString filename(QFileDialog::getOpenFileName(
		QtUtils::GetRootWidget(this), tr("Select Disc Path"), QString(), qApp->translate("MainWindow", MainWindow::DISC_IMAGE_FILTER)));
	if (filename.isEmpty())
		return;

	// let the signal take care of it
	m_ui.discPath->setText(QDir::toNativeSeparators(filename));
}
