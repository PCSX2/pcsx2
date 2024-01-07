// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "pcsx2/SIO/Pad/Pad.h"
#include "QtHost.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"
#include "Settings/ControllerSettingWidgetBinder.h"
#include "Settings/InterfaceSettingsWidget.h"
#include "SetupWizardDialog.h"

#include <QtWidgets/QMessageBox>

SetupWizardDialog::SetupWizardDialog()
{
	setupUi();
	updatePageLabels(-1);
	updatePageButtons();
}

SetupWizardDialog::~SetupWizardDialog()
{
	if (m_bios_refresh_thread)
	{
		m_bios_refresh_thread->wait();
		delete m_bios_refresh_thread;
	}
}

void SetupWizardDialog::resizeEvent(QResizeEvent* event)
{
	QDialog::resizeEvent(event);
	resizeDirectoryListColumns();
}

bool SetupWizardDialog::canShowNextPage()
{
	const int current_page = m_ui.pages->currentIndex();

	switch (current_page)
	{
		case Page_BIOS:
		{
			if (!m_ui.biosList->currentItem())
			{
				if (QMessageBox::question(this, tr("Warning"),
						tr("A BIOS image has not been selected. PCSX2 <strong>will not</strong> be able to run games "
						   "without a BIOS image.<br><br>Are you sure you wish to continue without selecting a BIOS "
						   "image?")) != QMessageBox::Yes)
				{
					return false;
				}
			}
		}
		break;

		case Page_GameList:
		{
			if (m_ui.searchDirectoryList->rowCount() == 0)
			{
				if (QMessageBox::question(this, tr("Warning"),
						tr("No game directories have been selected. You will have to manually open any game dumps you "
						   "want to play, PCSX2's list will be empty.\n\nAre you sure you want to continue?")) !=
					QMessageBox::Yes)
				{
					return false;
				}
			}
		}
		break;

		default:
			break;
	}

	return true;
}

void SetupWizardDialog::previousPage()
{
	const int current_page = m_ui.pages->currentIndex();
	if (current_page == 0)
		return;

	m_ui.pages->setCurrentIndex(current_page - 1);
	updatePageLabels(current_page);
	updatePageButtons();
}

void SetupWizardDialog::nextPage()
{
	const int current_page = m_ui.pages->currentIndex();
	if (current_page == Page_Complete)
	{
		accept();
		return;
	}

	if (!canShowNextPage())
		return;

	const int new_page = current_page + 1;
	m_ui.pages->setCurrentIndex(new_page);
	updatePageLabels(current_page);
	updatePageButtons();
	pageChangedTo(new_page);
}

void SetupWizardDialog::pageChangedTo(int page)
{
	switch (page)
	{
		case Page_GameList:
			resizeDirectoryListColumns();
			break;

		default:
			break;
	}
}

void SetupWizardDialog::updatePageLabels(int prev_page)
{
	if (prev_page >= 0)
	{
		QFont prev_font = m_page_labels[prev_page]->font();
		prev_font.setBold(false);
		m_page_labels[prev_page]->setFont(prev_font);
	}

	const int page = m_ui.pages->currentIndex();
	QFont font = m_page_labels[page]->font();
	font.setBold(true);
	m_page_labels[page]->setFont(font);
}

void SetupWizardDialog::updatePageButtons()
{
	const int page = m_ui.pages->currentIndex();
	m_ui.next->setText((page == Page_Complete) ? tr("&Finish") : tr("&Next"));
	m_ui.back->setEnabled(page > 0);
}

void SetupWizardDialog::confirmCancel()
{
	if (QMessageBox::question(this, tr("Cancel Setup"),
			tr("Are you sure you want to cancel PCSX2 setup?\n\nAny changes have been saved, and the wizard will run "
			   "again next time you start PCSX2.")) != QMessageBox::Yes)
	{
		return;
	}

	reject();
}

void SetupWizardDialog::setupUi()
{
	m_ui.setupUi(this);

	m_ui.logo->setPixmap(QPixmap(QStringLiteral("%1/icons/AppIconLarge.png").arg(QtHost::GetResourcesBasePath())));

	m_ui.pages->setCurrentIndex(0);

	m_page_labels[Page_Language] = m_ui.labelLanguage;
	m_page_labels[Page_BIOS] = m_ui.labelBIOS;
	m_page_labels[Page_GameList] = m_ui.labelGameList;
	m_page_labels[Page_Controller] = m_ui.labelController;
	m_page_labels[Page_Complete] = m_ui.labelComplete;

	connect(m_ui.back, &QPushButton::clicked, this, &SetupWizardDialog::previousPage);
	connect(m_ui.next, &QPushButton::clicked, this, &SetupWizardDialog::nextPage);
	connect(m_ui.cancel, &QPushButton::clicked, this, &SetupWizardDialog::confirmCancel);

	setupLanguagePage();
	setupBIOSPage();
	setupGameListPage();
	setupControllerPage();
}

void SetupWizardDialog::setupLanguagePage()
{
	SettingWidgetBinder::BindWidgetToEnumSetting(nullptr, m_ui.theme, "UI", "Theme",
		InterfaceSettingsWidget::THEME_NAMES, InterfaceSettingsWidget::THEME_VALUES, QtHost::GetDefaultThemeName(), "InterfaceSettingsWidget");
	connect(m_ui.theme, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SetupWizardDialog::themeChanged);

	for (const std::pair<QString, QString>& it : QtHost::GetAvailableLanguageList())
		m_ui.language->addItem(it.first, it.second);
	SettingWidgetBinder::BindWidgetToStringSetting(
		nullptr, m_ui.language, "UI", "Language", QtHost::GetDefaultLanguage());
	connect(
		m_ui.language, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SetupWizardDialog::languageChanged);

	SettingWidgetBinder::BindWidgetToBoolSetting(
		nullptr, m_ui.autoUpdateEnabled, "AutoUpdater", "CheckAtStartup", true);
}

void SetupWizardDialog::themeChanged()
{
	// Main window gets recreated at the end here anyway, so it's fine to just yolo it.
	QtHost::UpdateApplicationTheme();
}

void SetupWizardDialog::languageChanged()
{
	// Skip the recreation, since we don't have many dynamic UI elements.
	QtHost::InstallTranslator(this);
	m_ui.retranslateUi(this);
}

void SetupWizardDialog::setupBIOSPage()
{
	SettingWidgetBinder::BindWidgetToFolderSetting(nullptr, m_ui.biosSearchDirectory, m_ui.browseBiosSearchDirectory,
		m_ui.openBiosSearchDirectory, m_ui.resetBiosSearchDirectory, "Folders", "Bios",
		Path::Combine(EmuFolders::DataRoot, "bios"));

	refreshBiosList();

	connect(m_ui.biosSearchDirectory, &QLineEdit::textChanged, this, &SetupWizardDialog::refreshBiosList);
	connect(m_ui.refreshBiosList, &QPushButton::clicked, this, &SetupWizardDialog::refreshBiosList);
	connect(m_ui.biosList, &QTreeWidget::currentItemChanged, this, &SetupWizardDialog::biosListItemChanged);
}

void SetupWizardDialog::refreshBiosList()
{
	if (m_bios_refresh_thread)
	{
		m_bios_refresh_thread->wait();
		delete m_bios_refresh_thread;
	}

	QSignalBlocker blocker(m_ui.biosList);
	m_ui.biosList->clear();
	m_ui.biosList->setEnabled(false);

	m_bios_refresh_thread = new BIOSSettingsWidget::RefreshThread(this, m_ui.biosSearchDirectory->text());
	m_bios_refresh_thread->start();
}

void SetupWizardDialog::biosListItemChanged(const QTreeWidgetItem* current, const QTreeWidgetItem* previous)
{
	Host::SetBaseStringSettingValue("Filenames", "BIOS", current->text(0).toUtf8().constData());
	Host::CommitBaseSettingChanges();
	g_emu_thread->applySettings();
}

void SetupWizardDialog::listRefreshed(const QVector<BIOSInfo>& items)
{
	QSignalBlocker sb(m_ui.biosList);
	BIOSSettingsWidget::populateList(m_ui.biosList, items);
	m_ui.biosList->setEnabled(true);
}

void SetupWizardDialog::setupGameListPage()
{
	m_ui.searchDirectoryList->setSelectionMode(QAbstractItemView::SingleSelection);
	m_ui.searchDirectoryList->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_ui.searchDirectoryList->setAlternatingRowColors(true);
	m_ui.searchDirectoryList->setShowGrid(false);
	m_ui.searchDirectoryList->horizontalHeader()->setHighlightSections(false);
	m_ui.searchDirectoryList->verticalHeader()->hide();
	m_ui.searchDirectoryList->setCurrentIndex({});
	m_ui.searchDirectoryList->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);

	connect(m_ui.searchDirectoryList, &QTableWidget::customContextMenuRequested, this,
		&SetupWizardDialog::onDirectoryListContextMenuRequested);
	connect(m_ui.addSearchDirectoryButton, &QPushButton::clicked, this,
		&SetupWizardDialog::onAddSearchDirectoryButtonClicked);
	connect(m_ui.removeSearchDirectoryButton, &QPushButton::clicked, this,
		&SetupWizardDialog::onRemoveSearchDirectoryButtonClicked);

	refreshDirectoryList();
}

void SetupWizardDialog::onDirectoryListContextMenuRequested(const QPoint& point)
{
	QModelIndexList selection = m_ui.searchDirectoryList->selectionModel()->selectedIndexes();
	if (selection.size() < 1)
		return;

	const int row = selection[0].row();

	QMenu menu;
	menu.addAction(tr("Remove"), [this]() { onRemoveSearchDirectoryButtonClicked(); });
	menu.addSeparator();
	menu.addAction(tr("Open Directory..."),
		[this, row]() { QtUtils::OpenURL(this, QUrl::fromLocalFile(m_ui.searchDirectoryList->item(row, 0)->text())); });
	menu.exec(m_ui.searchDirectoryList->mapToGlobal(point));
}

void SetupWizardDialog::onAddSearchDirectoryButtonClicked()
{
	QString dir = QDir::toNativeSeparators(QFileDialog::getExistingDirectory(this, tr("Select Search Directory")));

	if (dir.isEmpty())
		return;

	QMessageBox::StandardButton selection = QMessageBox::question(this, tr("Scan Recursively?"),
		tr("Would you like to scan the directory \"%1\" recursively?\n\nScanning recursively takes "
		   "more time, but will identify files in subdirectories.")
			.arg(dir),
		QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
	if (selection == QMessageBox::Cancel)
		return;

	const bool recursive = (selection == QMessageBox::Yes);
	const std::string spath = dir.toStdString();
	Host::RemoveBaseValueFromStringList("GameList", recursive ? "Paths" : "RecursivePaths", spath.c_str());
	Host::AddBaseValueToStringList("GameList", recursive ? "RecursivePaths" : "Paths", spath.c_str());
	Host::CommitBaseSettingChanges();
	refreshDirectoryList();
}

void SetupWizardDialog::onRemoveSearchDirectoryButtonClicked()
{
	const int row = m_ui.searchDirectoryList->currentRow();
	std::unique_ptr<QTableWidgetItem> item((row >= 0) ? m_ui.searchDirectoryList->takeItem(row, 0) : nullptr);
	if (!item)
		return;

	const std::string spath = item->text().toStdString();
	if (!Host::RemoveBaseValueFromStringList("GameList", "Paths", spath.c_str()) &&
		!Host::RemoveBaseValueFromStringList("GameList", "RecursivePaths", spath.c_str()))
	{
		return;
	}

	Host::CommitBaseSettingChanges();
	refreshDirectoryList();
}

void SetupWizardDialog::addPathToTable(const std::string& path, bool recursive)
{
	const int row = m_ui.searchDirectoryList->rowCount();
	m_ui.searchDirectoryList->insertRow(row);

	QTableWidgetItem* item = new QTableWidgetItem();
	item->setText(QString::fromStdString(path));
	item->setFlags(item->flags() & ~(Qt::ItemIsEditable));
	m_ui.searchDirectoryList->setItem(row, 0, item);

	QCheckBox* cb = new QCheckBox(m_ui.searchDirectoryList);
	m_ui.searchDirectoryList->setCellWidget(row, 1, cb);
	cb->setChecked(recursive);

	connect(cb, &QCheckBox::stateChanged, [item](int state) {
		const std::string path(item->text().toStdString());
		if (state == Qt::Checked)
		{
			Host::RemoveBaseValueFromStringList("GameList", "Paths", path.c_str());
			Host::AddBaseValueToStringList("GameList", "RecursivePaths", path.c_str());
		}
		else
		{
			Host::RemoveBaseValueFromStringList("GameList", "RecursivePaths", path.c_str());
			Host::AddBaseValueToStringList("GameList", "Paths", path.c_str());
		}
		Host::CommitBaseSettingChanges();
	});
}

void SetupWizardDialog::refreshDirectoryList()
{
	QSignalBlocker sb(m_ui.searchDirectoryList);
	while (m_ui.searchDirectoryList->rowCount() > 0)
		m_ui.searchDirectoryList->removeRow(0);

	std::vector<std::string> path_list = Host::GetBaseStringListSetting("GameList", "Paths");
	for (const std::string& entry : path_list)
		addPathToTable(entry, false);

	path_list = Host::GetBaseStringListSetting("GameList", "RecursivePaths");
	for (const std::string& entry : path_list)
		addPathToTable(entry, true);

	m_ui.searchDirectoryList->sortByColumn(0, Qt::AscendingOrder);
}

void SetupWizardDialog::resizeDirectoryListColumns()
{
	QtUtils::ResizeColumnsForTableView(m_ui.searchDirectoryList, {-1, 100});
}

void SetupWizardDialog::setupControllerPage()
{
	static constexpr u32 NUM_PADS = 2;

	struct PadWidgets
	{
		QComboBox* type_combo;
		QLabel* mapping_result;
		QToolButton* mapping_button;
	};
	const PadWidgets pad_widgets[NUM_PADS] = {
		{m_ui.controller1Type, m_ui.controller1Mapping, m_ui.controller1AutomaticMapping},
		{m_ui.controller2Type, m_ui.controller2Mapping, m_ui.controller2AutomaticMapping},
	};

	for (u32 port = 0; port < NUM_PADS; port++)
	{
		const std::string section = fmt::format("Pad{}", port + 1);
		const PadWidgets& w = pad_widgets[port];

		for (const auto& [name, display_name] : Pad::GetControllerTypeNames())
			w.type_combo->addItem(QString::fromUtf8(display_name), QString::fromUtf8(name));
		ControllerSettingWidgetBinder::BindWidgetToInputProfileString(
			nullptr, w.type_combo, section, "Type", Pad::GetControllerInfo(Pad::GetDefaultPadType(port))->name);

		w.mapping_result->setText((port == 0) ? tr("Default (Keyboard)") : tr("Default (None)"));

		connect(w.mapping_button, &QAbstractButton::clicked, this,
			[this, port, label = w.mapping_result]() { openAutomaticMappingMenu(port, label); });
	}

	// Trigger enumeration to populate the device list.
	connect(g_emu_thread, &EmuThread::onInputDevicesEnumerated, this, &SetupWizardDialog::onInputDevicesEnumerated);
	connect(g_emu_thread, &EmuThread::onInputDeviceConnected, this, &SetupWizardDialog::onInputDeviceConnected);
	connect(g_emu_thread, &EmuThread::onInputDeviceDisconnected, this, &SetupWizardDialog::onInputDeviceDisconnected);
	g_emu_thread->enumerateInputDevices();
}

void SetupWizardDialog::openAutomaticMappingMenu(u32 port, QLabel* update_label)
{
	QMenu menu(this);
	bool added = false;

	for (const QPair<QString, QString>& dev : m_device_list)
	{
		// we set it as data, because the device list could get invalidated while the menu is up
		QAction* action = menu.addAction(QStringLiteral("%1 (%2)").arg(dev.first).arg(dev.second));
		action->setData(dev.first);
		connect(action, &QAction::triggered, this, [this, port, update_label, action]() {
			doDeviceAutomaticBinding(port, update_label, action->data().toString());
		});
		added = true;
	}

	if (!added)
	{
		QAction* action = menu.addAction(tr("No devices available"));
		action->setEnabled(false);
	}

	menu.exec(QCursor::pos());
}

void SetupWizardDialog::doDeviceAutomaticBinding(u32 port, QLabel* update_label, const QString& device)
{
	std::vector<std::pair<GenericInputBinding, std::string>> mapping =
		InputManager::GetGenericBindingMapping(device.toStdString());
	if (mapping.empty())
	{
		QMessageBox::critical(this, tr("Automatic Binding"),
			tr("No generic bindings were generated for device '%1'. The controller/source may not support automatic "
			   "mapping.")
				.arg(device));
		return;
	}

	bool result;
	{
		auto lock = Host::GetSettingsLock();
		result = Pad::MapController(*Host::Internal::GetBaseSettingsLayer(), port, mapping);
	}
	if (!result)
		return;

	Host::CommitBaseSettingChanges();

	update_label->setText(device);
}

void SetupWizardDialog::onInputDevicesEnumerated(const QList<QPair<QString, QString>>& devices)
{
	m_device_list = devices;
}

void SetupWizardDialog::onInputDeviceConnected(const QString& identifier, const QString& device_name)
{
	m_device_list.emplace_back(identifier, device_name);
}

void SetupWizardDialog::onInputDeviceDisconnected(const QString& identifier)
{
	for (auto iter = m_device_list.begin(); iter != m_device_list.end(); ++iter)
	{
		if (iter->first == identifier)
		{
			m_device_list.erase(iter);
			break;
		}
	}
}
