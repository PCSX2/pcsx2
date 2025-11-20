// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "BIOSSettingsWidget.h"
#include "QtHost.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"
#include "SettingsWindow.h"

#include "pcsx2/Host.h"
#include "pcsx2/ps2/BiosTools.h"

#include "common/FileSystem.h"

#include <QtGui/QIcon>
#include <QtWidgets/QFileDialog>

BIOSSettingsWidget::BIOSSettingsWidget(SettingsWindow* settings_dialog, QWidget* parent)
	: SettingsWidget(settings_dialog, parent)
{
	SettingsInterface* sif = dialog()->getSettingsInterface();

	setupTab(m_ui);

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.fastBoot, "EmuCore", "EnableFastBoot", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.fastBootFastForward, "EmuCore", "EnableFastBootFastForward", false);
	SettingWidgetBinder::BindWidgetToFolderSetting(sif, m_ui.searchDirectory, m_ui.browseSearchDirectory, m_ui.openSearchDirectory,
		m_ui.resetSearchDirectory, "Folders", "Bios", Path::Combine(EmuFolders::DataRoot, "bios"));

	dialog()->registerWidgetHelp(m_ui.fastBoot, tr("Fast Boot"), tr("Checked"),
		tr("Patches the BIOS to skip the console's boot animation."));

	dialog()->registerWidgetHelp(m_ui.fastBootFastForward, tr("Fast Forward Boot"), tr("Unchecked"),
		tr("Removes emulation speed throttle until the game starts to reduce startup time."));

	refreshList();

	connect(m_ui.searchDirectory, &QLineEdit::textChanged, this, &BIOSSettingsWidget::refreshList);
	connect(m_ui.refresh, &QPushButton::clicked, this, &BIOSSettingsWidget::refreshList);
	connect(m_ui.fileList, &QTreeWidget::currentItemChanged, this, &BIOSSettingsWidget::listItemChanged);
	connect(m_ui.fastBoot, &QCheckBox::checkStateChanged, this, &BIOSSettingsWidget::fastBootChanged);
}

BIOSSettingsWidget::~BIOSSettingsWidget() = default;

void BIOSSettingsWidget::refreshList()
{
	const std::string search_dir = m_ui.searchDirectory->text().toStdString();
	populateList(m_ui.fileList, search_dir);
}

void BIOSSettingsWidget::populateList(QTreeWidget* list, const std::string& directory)
{
	const std::string selected_bios = Host::GetBaseStringSettingValue("Filenames", "BIOS");
	const QString res_path = QtHost::GetResourcesBasePath();

	QSignalBlocker blocker(list);
	list->clear();
	list->setEnabled(false);
	qApp->processEvents(QEventLoop::ExcludeUserInputEvents);

	FileSystem::FindResultsArray files;
	FileSystem::FindFiles(directory.c_str(), "*", FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_HIDDEN_FILES, &files);

	u32 bios_version, bios_region;
	std::string bios_description, bios_zone;

	for (const FILESYSTEM_FIND_DATA& fd : files)
	{
		if (!IsBIOS(fd.FileName.c_str(), bios_version, bios_description, bios_region, bios_zone))
			continue;

		const std::string_view bios_name = Path::GetFileName(fd.FileName);

		QTreeWidgetItem* item = new QTreeWidgetItem();
		item->setText(0, QtUtils::StringViewToQString(bios_name));
		item->setText(1, QString::fromStdString(bios_description));

		switch (bios_region)
		{
			case 0: // Japan
				item->setIcon(0, QIcon(QStringLiteral("%1/icons/flags/jp.svg").arg(res_path)));
				break;

			case 1: // USA
				item->setIcon(0, QIcon(QStringLiteral("%1/icons/flags/us.svg").arg(res_path)));
				break;

			case 2: // Europe
				item->setIcon(0, QIcon(QStringLiteral("%1/icons/flags/eu.svg").arg(res_path)));
				break;

			case 3: // Oceania
				item->setIcon(0, QIcon(QStringLiteral("%1/icons/flags/au.svg").arg(res_path)));
				break;

			case 4: // Asia
				item->setIcon(0, QIcon(QStringLiteral("%1/icons/flags/hk.svg").arg(res_path)));
				break;

			case 5: // Russia
				item->setIcon(0, QIcon(QStringLiteral("%1/icons/flags/ru.svg").arg(res_path)));
				break;

			case 6: // China
				item->setIcon(0, QIcon(QStringLiteral("%1/icons/flags/cn.svg").arg(res_path)));
				break;

			case 7: // Mexico
				item->setIcon(0, QIcon(QStringLiteral("%1/icons/flags/mx.svg").arg(res_path)));
				break;

			case 8: // T10K
			case 9: // Test
			case 10: // Free
			default:
				item->setIcon(0, QIcon(QStringLiteral("%1/icons/flags/jp.svg").arg(res_path)));
				break;
		}

		list->addTopLevelItem(item);

		if (selected_bios == bios_name)
		{
			list->selectionModel()->setCurrentIndex(list->indexFromItem(item), QItemSelectionModel::Select);
			item->setSelected(true);
		}
	}

	list->setEnabled(true);
}

void BIOSSettingsWidget::listItemChanged(const QTreeWidgetItem* current, const QTreeWidgetItem* previous)
{
	Host::SetBaseStringSettingValue("Filenames", "BIOS", current->text(0).toUtf8().constData());
	Host::CommitBaseSettingChanges();

	g_emu_thread->applySettings();
}

void BIOSSettingsWidget::fastBootChanged()
{
	const bool enabled = dialog()->getEffectiveBoolValue("EmuCore", "EnableFastBoot", true);
	m_ui.fastBootFastForward->setEnabled(enabled);
}
