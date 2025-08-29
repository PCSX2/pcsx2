// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "ShortcutCreationDialog.h"
#include "QtUtils.h"
#include "QtHost.h"
#include <common/HostSys.h>
#include <fmt/format.h>

#include <QtWidgets/QFileDialog>
#include <qline.h>
#include <qlineedit.h>
#include <qpushbutton.h>


ShortcutCreationDialog::ShortcutCreationDialog(QWidget* parent, const QString& title, const QString& path)
	: QDialog(parent)
	, m_title(title)
	, m_path(path)
{
	m_ui.setupUi(this);
	this->setWindowTitle(tr("Creating Shortcut For %1").arg(title));
	this->setWindowIcon(QtHost::GetAppIcon());

#if defined(__WIN32__) 
	m_ui.shortcutStartMenu->setText(tr("Start Menu"));
#else
	m_ui.shortcutStartMenu->setText(tr("Application Launcher"));
#endif

	connect(m_ui.overrideBootELFButton, &QPushButton::clicked, [&]() {
		const QString path = QFileDialog::getOpenFileName(this, tr("Select ELF File"), QString(), tr("ELF Files (*.elf);;All Files (*.*)"));
		if (!path.isEmpty())
			m_ui.overrideBootELFPath->setText(path);
	});

	connect(m_ui.loadStateFileBrowse, &QPushButton::clicked, [&]() {
		const QString path = QFileDialog::getOpenFileName(this, tr("Select Save State File"), QString(), tr("Save States (*.p2s);;All Files (*.*)"));
		if (!path.isEmpty())
			m_ui.loadStateFileToggle->setText(path);
	});

	connect(m_ui.overrideBootELFToggle, &QCheckBox::toggled, m_ui.overrideBootELFPath, &QLineEdit::setEnabled);
	connect(m_ui.overrideBootELFToggle, &QCheckBox::toggled, m_ui.overrideBootELFButton, &QPushButton::setEnabled);
	connect(m_ui.gameArgsToggle, &QCheckBox::toggled, m_ui.gameArgs, &QLineEdit::setEnabled);
	connect(m_ui.loadStateIndexToggle, &QCheckBox::toggled, m_ui.loadStateIndex, &QSpinBox::setEnabled);
	connect(m_ui.loadStateFileToggle, &QCheckBox::toggled, m_ui.loadStateFilePath, &QLineEdit::setEnabled);
	connect(m_ui.loadStateFileToggle, &QCheckBox::toggled, m_ui.loadStateFileBrowse, &QPushButton::setEnabled);
	connect(m_ui.bootOptionToggle, &QCheckBox::toggled, m_ui.bootOptionDropdown, &QPushButton::setEnabled);
	connect(m_ui.fullscreenMode, &QCheckBox::toggled, m_ui.fullscreenModeDropdown, &QPushButton::setEnabled);

	m_ui.shortcutDesktop->setChecked(true);
	m_ui.overrideBootELFPath->setEnabled(false);
	m_ui.overrideBootELFButton->setEnabled(false);
	m_ui.gameArgs->setEnabled(false);
	m_ui.bootOptionDropdown->setEnabled(false);
	m_ui.fullscreenModeDropdown->setEnabled(false);
	m_ui.loadStateIndex->setEnabled(false);
	m_ui.loadStateFileBrowse->setEnabled(false);
	m_ui.loadStateFilePath->setEnabled(false);

	m_ui.customArgsInstruction->setText(tr("You may add additional (space-separated) <a href=\"https://pcsx2.net/docs/post/cli/\">custom arguments</a> that are not listed above here:"));
	m_ui.customArgsInstruction->setTextFormat(Qt::RichText);
	m_ui.customArgsInstruction->setTextInteractionFlags(Qt::TextBrowserInteraction);
	m_ui.customArgsInstruction->setOpenExternalLinks(true);

	connect(m_ui.dialogButtons, &QDialogButtonBox::accepted, this, [&]() {
		std::string args;

		if (m_ui.portableModeToggle->isChecked())
			args += "-portable ";

		if (m_ui.overrideBootELFToggle->isChecked() && !m_ui.overrideBootELFPath->text().isEmpty())
			args += fmt::format("-elf \"{}\" ", m_ui.overrideBootELFPath->text().toStdString());

		if (m_ui.gameArgsToggle->isChecked() && !m_ui.gameArgs->text().isEmpty())
			args += fmt::format("-gameargs \"{}\" ", m_ui.gameArgs->text().toStdString());

		if (m_ui.bootOptionToggle->isChecked() && m_ui.bootOptionDropdown->currentIndex() == 0)
			args += "-fastboot ";
		else if (m_ui.bootOptionToggle->isChecked() && m_ui.bootOptionDropdown->currentIndex() == 1)
			args += "-slowboot ";

		if (m_ui.loadStateIndexToggle->isChecked())
			args += fmt::format("-state {} ", m_ui.loadStateIndex->value());
		else if (m_ui.loadStateIndexToggle->isChecked() && !m_ui.loadStateFilePath->text().isEmpty())
			args += fmt::format("-statefile \"{}\" ", m_ui.loadStateFilePath->text().toStdString());

		if (m_ui.fullscreenMode->isChecked() && m_ui.fullscreenModeDropdown->currentIndex() == 0)
			args += "-fullscreen ";
		else if (m_ui.fullscreenMode->isChecked() && m_ui.fullscreenModeDropdown->currentIndex() == 1)
			args += "-nofullscreen ";

		if (m_ui.bigPictureModeToggle->isChecked())
			args += "-bigpicture ";

		if (m_ui.customArgsInput->text().isEmpty())
			args += m_ui.customArgsInput->text().toStdString() + " ";

		if (!args.empty() && args.back() == ' ')
			args.pop_back();

		if (m_ui.shortcutDesktop->isChecked())
			m_desktop = true;
		else if (m_ui.shortcutStartMenu->isChecked())
			m_desktop = false;

		Common::CreateShortcut(title.toStdString(), path.toStdString(), args, m_desktop);
		accept();
	});
	connect(m_ui.dialogButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
