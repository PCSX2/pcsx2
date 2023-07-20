/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2023 PCSX2 Dev Team
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

#include "MainWindow.h"
#include "QtHost.h"
#include "QtUtils.h"
#include "Settings/GamePatchSettingsWidget.h"
#include "Settings/SettingsDialog.h"

#include "pcsx2/GameList.h"
#include "pcsx2/Patch.h"

#include <algorithm>

GamePatchDetailsWidget::GamePatchDetailsWidget(std::string name, const std::string& author,
	const std::string& description, bool enabled, SettingsDialog* dialog, QWidget* parent)
	: QWidget(parent)
	, m_dialog(dialog)
	, m_name(name)
{
	m_ui.setupUi(this);

	m_ui.name->setText(QString::fromStdString(name));
	m_ui.description->setText(
		tr("<strong>Author: </strong>%1<br>%2")
			.arg(author.empty() ? tr("Unknown") : QString::fromStdString(author))
			.arg(description.empty() ? tr("No description provided.") : QString::fromStdString(description)));

	pxAssert(dialog->getSettingsInterface());
	m_ui.enabled->setChecked(enabled);
	connect(m_ui.enabled, &QCheckBox::stateChanged, this, &GamePatchDetailsWidget::onEnabledStateChanged);
}

GamePatchDetailsWidget::~GamePatchDetailsWidget() = default;

void GamePatchDetailsWidget::onEnabledStateChanged(int state)
{
	SettingsInterface* si = m_dialog->getSettingsInterface();
	if (state == Qt::Checked)
		si->AddToStringList("Patches", "Enable", m_name.c_str());
	else
		si->RemoveFromStringList("Patches", "Enable", m_name.c_str());

	si->Save();
	g_emu_thread->reloadGameSettings();
}

GamePatchSettingsWidget::GamePatchSettingsWidget(SettingsDialog* dialog, QWidget* parent)
	: m_dialog(dialog)
{
	m_ui.setupUi(this);
	m_ui.scrollArea->setFrameShape(QFrame::WinPanel);
	m_ui.scrollArea->setFrameShadow(QFrame::Sunken);

	connect(m_ui.reload, &QPushButton::clicked, this, &GamePatchSettingsWidget::onReloadClicked);

	reloadList();
}

GamePatchSettingsWidget::~GamePatchSettingsWidget() = default;

void GamePatchSettingsWidget::onReloadClicked()
{
	reloadList();

	// reload it on the emu thread too, so it picks up any changes
	g_emu_thread->reloadPatches();
}

void GamePatchSettingsWidget::reloadList()
{
	// Patches shouldn't have any unlabelled patch groups, because they're new.
	std::vector<Patch::PatchInfo> patches = Patch::GetPatchInfo(m_dialog->getSerial(), m_dialog->getDiscCRC(), false, nullptr);
	std::vector<std::string> enabled_list =
		m_dialog->getSettingsInterface()->GetStringList(Patch::PATCHES_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY);

	delete m_ui.scrollArea->takeWidget();

	QWidget* container = new QWidget(m_ui.scrollArea);
	QVBoxLayout* layout = new QVBoxLayout(container);
	layout->setContentsMargins(0, 0, 0, 0);

	if (!patches.empty())
	{
		bool first = true;

		for (Patch::PatchInfo& pi : patches)
		{
			if (!first)
			{
				QFrame* frame = new QFrame(container);
				frame->setFrameShape(QFrame::HLine);
				frame->setFrameShadow(QFrame::Sunken);
				layout->addWidget(frame);
			}
			else
			{
				first = false;
			}

			const bool enabled = (std::find(enabled_list.begin(), enabled_list.end(), pi.name) != enabled_list.end());
			GamePatchDetailsWidget* it =
				new GamePatchDetailsWidget(std::move(pi.name), pi.author, pi.description, enabled, m_dialog, container);
			layout->addWidget(it);
		}
	}
	else
	{
		QLabel* label = new QLabel(tr("There are no patches available for this game."), container);
		layout->addWidget(label);
	}

	layout->addStretch(1);

	m_ui.scrollArea->setWidget(container);
}
