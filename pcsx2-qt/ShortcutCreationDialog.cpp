// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "ShortcutCreationDialog.h"
#include "QtHost.h"
#include "QtUtils.h"
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include "common/Path.h"
#include "common/StringUtil.h"
#include "VMManager.h"

ShortcutCreationDialog::ShortcutCreationDialog(QWidget* parent, const QString& title, const QString& path)
	: QDialog(parent)
	, m_title(title)
	, m_path(path)
{
	m_ui.setupUi(this);
	this->setWindowTitle(tr("Create Shortcut For %1").arg(title));
	this->setWindowIcon(QtHost::GetAppIcon());

#if defined(_WIN32)
	m_ui.shortcutStartMenu->setText(tr("Start Menu"));
#else
	m_ui.shortcutStartMenu->setText(tr("Application Launcher"));
#endif

	QButtonGroup* speedGroup = new QButtonGroup(this);
	speedGroup->setExclusive(true);
	speedGroup->addButton(m_ui.fastForwardTurboOption);
	speedGroup->addButton(m_ui.fastForwardUnlimitedOption);
	m_ui.fastForwardTurboOption->setChecked(true);

	connect(m_ui.overrideBootELFButton, &QPushButton::clicked, [&]() {
		const QString path = QFileDialog::getOpenFileName(this, tr("Select ELF File"), QString(), tr("ELF Files (*.elf);;All Files (*.*)"));
		if (!path.isEmpty())
			m_ui.overrideBootELFPath->setText(Path::ToNativePath(path.toStdString()).c_str());
	});

	connect(m_ui.loadStateFileBrowse, &QPushButton::clicked, [&]() {
		const QString path = QFileDialog::getOpenFileName(this, tr("Select Save State File"), QString(), tr("Save States (*.p2s);;All Files (*.*)"));
		if (!path.isEmpty())
			m_ui.loadStateFilePath->setText(Path::ToNativePath(path.toStdString()).c_str());
	});

	connect(m_ui.overrideBootELFToggle, &QCheckBox::toggled, m_ui.overrideBootELFPath, &QLineEdit::setEnabled);
	connect(m_ui.overrideBootELFToggle, &QCheckBox::toggled, m_ui.overrideBootELFButton, &QPushButton::setEnabled);
	connect(m_ui.gameArgsToggle, &QCheckBox::toggled, m_ui.gameArgs, &QLineEdit::setEnabled);
	connect(m_ui.loadStateIndexToggle, &QCheckBox::toggled, m_ui.loadStateIndex, &QSpinBox::setEnabled);
	connect(m_ui.loadStateFileToggle, &QCheckBox::toggled, m_ui.loadStateFilePath, &QLineEdit::setEnabled);
	connect(m_ui.loadStateFileToggle, &QCheckBox::toggled, m_ui.loadStateFileBrowse, &QPushButton::setEnabled);
	connect(m_ui.bootOptionToggle, &QCheckBox::toggled, m_ui.bootOptionDropdown, &QPushButton::setEnabled);
	connect(m_ui.fullscreenMode, &QCheckBox::toggled, m_ui.fullscreenModeDropdown, &QPushButton::setEnabled);
	connect(m_ui.fastForwardOptionToggle, &QCheckBox::toggled, [this] {
		const bool enabled = m_ui.fastForwardOptionToggle->isChecked();
		m_ui.fastForwardTurboOption->setEnabled(enabled);
		m_ui.fastForwardUnlimitedOption->setEnabled(enabled);
	});

	m_ui.loadStateIndex->setMaximum(VMManager::NUM_SAVE_STATE_SLOTS);

	m_ui.iconPreview->setPixmap(QtHost::GetAppIcon().pixmap(m_ui.iconPreview->size()));
	m_ui.resetIconButton->setEnabled(false);

	connect(m_ui.browseIconButton, &QPushButton::clicked, [&]() {
#if defined(_WIN32)
		const QString filter = tr("Icon Files (*.ico);;All Files (*.*)");
#else
		const QString filter = tr("Image Files (*.png *.jpg *.svg *.webp);;All Files (*.*)");
#endif
		const QString icon_file = QFileDialog::getOpenFileName(this, tr("Select Icon"), QString(), filter);
		if (!icon_file.isEmpty())
		{
			QPixmap pixmap(icon_file);
			if (pixmap.isNull())
			{
				QMessageBox::critical(this, tr("Invalid Icon"), tr("The selected file could not be loaded as an icon."));
				return;
			}

			m_ui.iconPath->setText(Path::ToNativePath(icon_file.toStdString()).c_str());
			m_ui.iconPreview->setPixmap(pixmap.scaled(m_ui.iconPreview->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
			m_ui.resetIconButton->setEnabled(true);
		}
	});

	connect(m_ui.resetIconButton, &QPushButton::clicked, [&]() {
		m_ui.iconPath->clear();
		m_ui.iconPreview->setPixmap(QtHost::GetAppIcon().pixmap(m_ui.iconPreview->size()));
		m_ui.resetIconButton->setEnabled(false);
	});

	if (QtUtils::IsRunningInFlatpak())
	{
		m_ui.portableModeToggle->setEnabled(false);
		m_ui.shortcutDesktop->setEnabled(false);
		m_ui.shortcutStartMenu->setEnabled(false);
	}

	connect(m_ui.dialogButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(m_ui.dialogButtons, &QDialogButtonBox::accepted, this, [this]() {
		std::vector<std::string> args;

		if (m_ui.portableModeToggle->isChecked())
			args.push_back("-portable");

		if (m_ui.overrideBootELFToggle->isChecked() && !m_ui.overrideBootELFPath->text().isEmpty())
		{
			args.push_back("-elf");
			args.push_back(m_ui.overrideBootELFPath->text().toStdString());
		}

		if (m_ui.gameArgsToggle->isChecked() && !m_ui.gameArgs->text().isEmpty())
		{
			args.push_back("-gameargs");
			args.push_back(m_ui.gameArgs->text().toStdString());
		}

		if (m_ui.bootOptionToggle->isChecked())
			args.push_back(m_ui.bootOptionDropdown->currentIndex() ? "-slowboot" : "-fastboot");

		if (m_ui.loadStateIndexToggle->isChecked())
		{
			const s32 load_state_index = m_ui.loadStateIndex->value();
			if (load_state_index > 0 && load_state_index <= VMManager::NUM_SAVE_STATE_SLOTS)
			{
				args.push_back("-state");
				args.push_back(StringUtil::ToChars(load_state_index));
			}
		}

		if (m_ui.loadStateFileToggle->isChecked() && !m_ui.loadStateFilePath->text().isEmpty())
		{
			args.push_back("-statefile");
			args.push_back(m_ui.loadStateFilePath->text().toStdString());
		}

		if (m_ui.fullscreenMode->isChecked())
			args.push_back(m_ui.fullscreenModeDropdown->currentIndex() ? "-nofullscreen" : "-fullscreen");

		if (m_ui.bigPictureModeToggle->isChecked())
			args.push_back("-bigpicture");

		if (m_ui.fastForwardOptionToggle->isChecked())
		{
			if (m_ui.fastForwardTurboOption->isChecked())
				args.push_back("-turbo");
			else if (m_ui.fastForwardUnlimitedOption->isChecked())
				args.push_back("-unlimited");
		}

		std::string custom_args = m_ui.customArgsInput->text().toStdString();
		std::string icon_path = m_ui.iconPath->text().toStdString();

		QtUtils::CreateShortcut(this, m_title.toStdString(), m_path.toStdString(), std::move(args), custom_args, icon_path, m_ui.shortcutDesktop->isChecked());

		accept();
	});
}

#include "moc_ShortcutCreationDialog.cpp"
