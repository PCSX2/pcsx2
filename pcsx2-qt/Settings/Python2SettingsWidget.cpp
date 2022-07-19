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

#include "Python2SettingsWidget.h"
#include "SettingWidgetBinder.h"
#include "SettingsDialog.h"
#include "QtHost.h"

Python2SettingsWidget::Python2SettingsWidget(const GameList::Entry* entry, SettingsDialog* dialog, QWidget* parent)
	: m_dialog(dialog)
{
	SettingsInterface* sif = dialog->getSettingsInterface();

	m_ui.setupUi(this);

	const int gameTypeId = m_dialog->getIntValue("Python2/Game", "GameType", 0).value();
	connect(m_ui.gameType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Python2SettingsWidget::onGameTypeChanged);
	m_ui.gameType->setCurrentIndex(gameTypeId);

	SettingWidgetBinder::BindWidgetToStringSetting(sif, m_ui.hddIdPath, "DEV9/Hdd", "HddIdFile", "HDD_ID.bin");
	m_ui.hddIdPath->setEnabled(true);
	connect(m_ui.hddIdBrowse, &QPushButton::clicked, this, &Python2SettingsWidget::onHddIdBrowseClicked);

	SettingWidgetBinder::BindWidgetToStringSetting(sif, m_ui.ilinkIdPath, "Python2/System", "IlinkIdFile", "ILINK_ID.bin");
	m_ui.ilinkIdPath->setEnabled(true);
	connect(m_ui.ilinkIdBrowse, &QPushButton::clicked, this, &Python2SettingsWidget::onIlinkIdBrowseClicked);

	SettingWidgetBinder::BindWidgetToStringSetting(sif, m_ui.dongleBlackPath, "Python2/Game", "DongleBlackFile", "dongle_black.bin");
	m_ui.dongleBlackPath->setEnabled(true);
	connect(m_ui.dongleBlackBrowse, &QPushButton::clicked, this, &Python2SettingsWidget::onDongleBlackBrowseClicked);

	SettingWidgetBinder::BindWidgetToStringSetting(sif, m_ui.dongleWhitePath, "Python2/Game", "DongleWhiteFile", "dongle_white.bin");
	m_ui.dongleWhitePath->setEnabled(true);
	connect(m_ui.dongleWhiteBrowse, &QPushButton::clicked, this, &Python2SettingsWidget::onDongleWhiteBrowseClicked);

	SettingWidgetBinder::BindWidgetToStringSetting(sif, m_ui.player1CardPath, "Python2/Game", "Player1CardFile", "card1.txt");
	m_ui.player1CardPath->setEnabled(true);
	connect(m_ui.player1CardBrowse, &QPushButton::clicked, this, &Python2SettingsWidget::onPlayer1CardBrowseClicked);

	SettingWidgetBinder::BindWidgetToStringSetting(sif, m_ui.player2CardPath, "Python2/Game", "Player2CardFile", "card2.txt");
	m_ui.player2CardPath->setEnabled(true);
	connect(m_ui.player2CardBrowse, &QPushButton::clicked, this, &Python2SettingsWidget::onPlayer2CardBrowseClicked);

	// Nullable bools are weird so don't use BindWidgetToBoolSetting
	const bool dipsw1Value = m_dialog->getBoolValue("Python2/Game", "DIPSW1", false).value();
	connect(m_ui.dipsw1, QOverload<int>::of(&QCheckBox::stateChanged), this, [&](int state) { m_dialog->setBoolSettingValue("Python2/Game", "DIPSW1", state); });
	m_ui.dipsw1->setCheckState(dipsw1Value ? Qt::CheckState::Checked : Qt::CheckState::Unchecked);

	const bool dipsw2Value = m_dialog->getBoolValue("Python2/Game", "DIPSW2", false).value();
	connect(m_ui.dipsw2, QOverload<int>::of(&QCheckBox::stateChanged), this, [&](int state) { m_dialog->setBoolSettingValue("Python2/Game", "DIPSW2", state); });
	m_ui.dipsw2->setCheckState(dipsw2Value ? Qt::CheckState::Checked : Qt::CheckState::Unchecked);

	const bool dipsw3Value = m_dialog->getBoolValue("Python2/Game", "DIPSW3", false).value();
	connect(m_ui.dipsw3, QOverload<int>::of(&QCheckBox::stateChanged), this, [&](int state) { m_dialog->setBoolSettingValue("Python2/Game", "DIPSW3", state); });
	m_ui.dipsw3->setCheckState(dipsw3Value ? Qt::CheckState::Checked : Qt::CheckState::Unchecked);

	const bool dipsw4Value = m_dialog->getBoolValue("Python2/Game", "DIPSW4", false).value();
	connect(m_ui.dipsw4, QOverload<int>::of(&QCheckBox::stateChanged), this, [&](int state) { m_dialog->setBoolSettingValue("Python2/Game", "DIPSW4", state); });
	m_ui.dipsw4->setCheckState(dipsw4Value ? Qt::CheckState::Checked : Qt::CheckState::Unchecked);

	const bool force31kHzValue = m_dialog->getBoolValue("Python2/Game", "Force31kHz", false).value();
	connect(m_ui.force31khz, QOverload<int>::of(&QCheckBox::stateChanged), this, [&](int state) { m_dialog->setBoolSettingValue("Python2/Game", "Force31kHz", state); });
	m_ui.force31khz->setCheckState(force31kHzValue ? Qt::CheckState::Checked : Qt::CheckState::Unchecked);

	SettingWidgetBinder::BindWidgetToStringSetting(sif, m_ui.patchFilePath, "Python2/Game", "PatchFile", "patches.pnach");
	m_ui.patchFilePath->setEnabled(true);
	connect(m_ui.patchFileBrowse, &QPushButton::clicked, this, &Python2SettingsWidget::onPatchFileBrowseClicked);
}

void Python2SettingsWidget::onGameTypeChanged(int index)
{
	m_dialog->setIntSettingValue("Python2/Game", "GameType", index);
}

void Python2SettingsWidget::onHddIdBrowseClicked()
{
	QString path =
		QDir::toNativeSeparators(QFileDialog::getOpenFileName(QtUtils::GetRootWidget(this), tr("HDD ID File"),
			!m_ui.hddIdPath->text().isEmpty() ? m_ui.hddIdPath->text() : "HDD_ID.bin", tr("BIN (*.bin)"), nullptr,
			QFileDialog::DontConfirmOverwrite));

	if (path.isEmpty())
		return;

	m_ui.hddIdPath->setText(path);
	m_ui.hddIdPath->editingFinished();
}

void Python2SettingsWidget::onIlinkIdBrowseClicked()
{
	QString path =
		QDir::toNativeSeparators(QFileDialog::getOpenFileName(QtUtils::GetRootWidget(this), tr("ILINK ID File"),
			!m_ui.ilinkIdPath->text().isEmpty() ? m_ui.ilinkIdPath->text() : "ILINK_ID.bin", tr("BIN (*.bin)"), nullptr,
			QFileDialog::DontConfirmOverwrite));

	if (path.isEmpty())
		return;

	m_ui.ilinkIdPath->setText(path);
	m_ui.ilinkIdPath->editingFinished();
}

void Python2SettingsWidget::onDongleBlackBrowseClicked()
{
	QString path =
		QDir::toNativeSeparators(QFileDialog::getOpenFileName(QtUtils::GetRootWidget(this), tr("Dongle File"),
			!m_ui.dongleBlackPath->text().isEmpty() ? m_ui.dongleBlackPath->text() : "dongle_black.bin", tr("BIN (*.bin)"), nullptr,
			QFileDialog::DontConfirmOverwrite));

	if (path.isEmpty())
		return;

	m_ui.dongleBlackPath->setText(path);
	m_ui.dongleBlackPath->editingFinished();
}

void Python2SettingsWidget::onDongleWhiteBrowseClicked()
{
	QString path =
		QDir::toNativeSeparators(QFileDialog::getOpenFileName(QtUtils::GetRootWidget(this), tr("Dongle File"),
			!m_ui.dongleWhitePath->text().isEmpty() ? m_ui.dongleWhitePath->text() : "dongle_white.bin", tr("BIN (*.bin)"), nullptr,
			QFileDialog::DontConfirmOverwrite));

	if (path.isEmpty())
		return;

	m_ui.dongleWhitePath->setText(path);
	m_ui.dongleWhitePath->editingFinished();
}

void Python2SettingsWidget::onPatchFileBrowseClicked()
{
	QString path =
		QDir::toNativeSeparators(QFileDialog::getOpenFileName(QtUtils::GetRootWidget(this), tr("Patch File"),
			!m_ui.patchFilePath->text().isEmpty() ? m_ui.patchFilePath->text() : "patches.pnach", tr("Patch (*.pnach)"), nullptr,
			QFileDialog::DontConfirmOverwrite));

	if (path.isEmpty())
		return;

	m_ui.patchFilePath->setText(path);
	m_ui.patchFilePath->editingFinished();
}

void Python2SettingsWidget::onPlayer1CardBrowseClicked()
{
	QString path =
		QDir::toNativeSeparators(QFileDialog::getOpenFileName(QtUtils::GetRootWidget(this), tr("Card File"),
			!m_ui.player1CardPath->text().isEmpty() ? m_ui.player1CardPath->text() : "card1.txt", nullptr, nullptr,
			QFileDialog::DontConfirmOverwrite));

	if (path.isEmpty())
		return;

	m_ui.player1CardPath->setText(path);
	m_ui.player1CardPath->editingFinished();
}

void Python2SettingsWidget::onPlayer2CardBrowseClicked()
{
	QString path =
		QDir::toNativeSeparators(QFileDialog::getOpenFileName(QtUtils::GetRootWidget(this), tr("Card File"),
			!m_ui.player2CardPath->text().isEmpty() ? m_ui.player2CardPath->text() : "card2.txt", nullptr, nullptr,
			QFileDialog::DontConfirmOverwrite));

	if (path.isEmpty())
		return;

	m_ui.player2CardPath->setText(path);
	m_ui.player2CardPath->editingFinished();
}

Python2SettingsWidget::~Python2SettingsWidget() = default;
