// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include <QtWidgets/QMessageBox>
#include <QtWidgets/QFileDialog>
#include <algorithm>

#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"

#include "pcsx2/Host.h"
#include "pcsx2/INISettingsInterface.h"

#include "DEV9SettingsWidget.h"
#include "QtHost.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"
#include "SettingsWindow.h"

#include "HddCreateQt.h"

#include "DEV9/pcap_io.h"
#ifdef _WIN32
#include "DEV9/Win32/tap.h"
#endif
#include "DEV9/sockets.h"

static const char* s_api_name[] = {
	" ",
	QT_TRANSLATE_NOOP("DEV9SettingsWidget", "PCAP Bridged"),
	QT_TRANSLATE_NOOP("DEV9SettingsWidget", "PCAP Switched"),
	QT_TRANSLATE_NOOP("DEV9SettingsWidget", "TAP"),
	QT_TRANSLATE_NOOP("DEV9SettingsWidget", "Sockets"),
	nullptr,
};

static const char* s_dns_name[] = {
	QT_TRANSLATE_NOOP("DEV9SettingsWidget", "Manual"),
	QT_TRANSLATE_NOOP("DEV9SettingsWidget", "Auto"),
	QT_TRANSLATE_NOOP("DEV9SettingsWidget", "Internal"),
	nullptr,
};

using PacketReader::IP::IP_Address;

DEV9SettingsWidget::DEV9SettingsWidget(SettingsWindow* dialog, QWidget* parent)
	: QWidget(parent)
	, m_dialog{dialog}
{
	SettingsInterface* sif = dialog->getSettingsInterface();

	m_ui.setupUi(this);

	//////////////////////////////////////////////////////////////////////////
	// Eth Enabled
	//////////////////////////////////////////////////////////////////////////
	//Connect needs to be after BindWidgetToBoolSetting to ensure correct order of execution for disabling a per game setting
	//we then need to manually call onEthAutoChanged to update the UI on fist load (done in show)
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.ethEnabled, "DEV9/Eth", "EthEnable", false);
	connect(m_ui.ethEnabled, &QCheckBox::checkStateChanged, this, &DEV9SettingsWidget::onEthEnabledChanged);

	//////////////////////////////////////////////////////////////////////////
	// Eth Device Settings
	//////////////////////////////////////////////////////////////////////////
	connect(m_ui.ethDevType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DEV9SettingsWidget::onEthDeviceTypeChanged);
	connect(m_ui.ethDev, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DEV9SettingsWidget::onEthDeviceChanged);
	//Comboboxes populated in show event

	//////////////////////////////////////////////////////////////////////////
	// DHCP Settings
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.ethInterceptDHCP, "DEV9/Eth", "InterceptDHCP", false);
	onEthDHCPInterceptChanged(m_ui.ethInterceptDHCP->checkState());
	connect(m_ui.ethInterceptDHCP, &QCheckBox::checkStateChanged, this, &DEV9SettingsWidget::onEthDHCPInterceptChanged);

	//IP settings
	const IPValidator* ipValidator = new IPValidator(this, m_dialog->isPerGameSettings());

	// clang-format off
	m_ui.ethPS2Addr    ->setValidator(ipValidator);
	m_ui.ethNetMask    ->setValidator(ipValidator);
	m_ui.ethGatewayAddr->setValidator(ipValidator);
	m_ui.ethDNS1Addr   ->setValidator(ipValidator);
	m_ui.ethDNS2Addr   ->setValidator(ipValidator);

	if (m_dialog->isPerGameSettings())
	{
		m_ui.ethPS2Addr    ->setText(QString::fromUtf8(m_dialog->getStringValue("DEV9/Eth", "PS2IP",   "").value().c_str()));
		m_ui.ethNetMask    ->setText(QString::fromUtf8(m_dialog->getStringValue("DEV9/Eth", "Mask",    "").value().c_str()));
		m_ui.ethGatewayAddr->setText(QString::fromUtf8(m_dialog->getStringValue("DEV9/Eth", "Gateway", "").value().c_str()));
		m_ui.ethDNS1Addr   ->setText(QString::fromUtf8(m_dialog->getStringValue("DEV9/Eth", "DNS1",    "").value().c_str()));
		m_ui.ethDNS2Addr   ->setText(QString::fromUtf8(m_dialog->getStringValue("DEV9/Eth", "DNS2",    "").value().c_str()));

		m_ui.ethPS2Addr    ->setPlaceholderText(QString::fromUtf8(Host::GetBaseStringSettingValue("DEV9/Eth", "PS2IP",   "0.0.0.0").c_str()));
		m_ui.ethNetMask    ->setPlaceholderText(QString::fromUtf8(Host::GetBaseStringSettingValue("DEV9/Eth", "Mask",    "0.0.0.0").c_str()));
		m_ui.ethGatewayAddr->setPlaceholderText(QString::fromUtf8(Host::GetBaseStringSettingValue("DEV9/Eth", "Gateway", "0.0.0.0").c_str()));
		m_ui.ethDNS1Addr   ->setPlaceholderText(QString::fromUtf8(Host::GetBaseStringSettingValue("DEV9/Eth", "DNS1",    "0.0.0.0").c_str()));
		m_ui.ethDNS2Addr   ->setPlaceholderText(QString::fromUtf8(Host::GetBaseStringSettingValue("DEV9/Eth", "DNS2",    "0.0.0.0").c_str()));
	}
	else
	{
		m_ui.ethPS2Addr    ->setText(QString::fromUtf8(m_dialog->getStringValue("DEV9/Eth", "PS2IP",   "0.0.0.0").value().c_str()));
		m_ui.ethNetMask    ->setText(QString::fromUtf8(m_dialog->getStringValue("DEV9/Eth", "Mask",    "0.0.0.0").value().c_str()));
		m_ui.ethGatewayAddr->setText(QString::fromUtf8(m_dialog->getStringValue("DEV9/Eth", "Gateway", "0.0.0.0").value().c_str()));
		m_ui.ethDNS1Addr   ->setText(QString::fromUtf8(m_dialog->getStringValue("DEV9/Eth", "DNS1",    "0.0.0.0").value().c_str()));
		m_ui.ethDNS2Addr   ->setText(QString::fromUtf8(m_dialog->getStringValue("DEV9/Eth", "DNS2",    "0.0.0.0").value().c_str()));
	}

	connect(m_ui.ethPS2Addr,     &QLineEdit::editingFinished, this, [&]() { onEthIPChanged(m_ui.ethPS2Addr,     "DEV9/Eth", "PS2IP"  ); });
	connect(m_ui.ethNetMask,     &QLineEdit::editingFinished, this, [&]() { onEthIPChanged(m_ui.ethNetMask,     "DEV9/Eth", "Mask"   ); });
	connect(m_ui.ethGatewayAddr, &QLineEdit::editingFinished, this, [&]() { onEthIPChanged(m_ui.ethGatewayAddr, "DEV9/Eth", "Gateway"); });
	connect(m_ui.ethDNS1Addr,    &QLineEdit::editingFinished, this, [&]() { onEthIPChanged(m_ui.ethDNS1Addr,    "DEV9/Eth", "DNS1"   ); });
	connect(m_ui.ethDNS2Addr,    &QLineEdit::editingFinished, this, [&]() { onEthIPChanged(m_ui.ethDNS2Addr,    "DEV9/Eth", "DNS2"   ); });
	// clang-format on

	//Auto
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.ethNetMaskAuto, "DEV9/Eth", "AutoMask", true);
	onEthAutoChanged(m_ui.ethNetMaskAuto, m_ui.ethNetMaskAuto->checkState(), m_ui.ethNetMask, "DEV9/Eth", "AutoMask");
	connect(m_ui.ethNetMaskAuto, &QCheckBox::checkStateChanged, this, [&](Qt::CheckState state) { onEthAutoChanged(m_ui.ethNetMaskAuto, state, m_ui.ethNetMask, "DEV9/Eth", "AutoMask"); });

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.ethGatewayAuto, "DEV9/Eth", "AutoGateway", true);
	onEthAutoChanged(m_ui.ethGatewayAuto, m_ui.ethGatewayAuto->checkState(), m_ui.ethGatewayAddr, "DEV9/Eth", "AutoGateway");
	connect(m_ui.ethGatewayAuto, &QCheckBox::checkStateChanged, this, [&](Qt::CheckState state) { onEthAutoChanged(m_ui.ethGatewayAuto, state, m_ui.ethGatewayAddr, "DEV9/Eth", "AutoGateway"); });

	SettingWidgetBinder::BindWidgetToEnumSetting(sif, m_ui.ethDNS1Mode, "DEV9/Eth", "ModeDNS1",
		s_dns_name, Pcsx2Config::DEV9Options::DnsModeNames, Pcsx2Config::DEV9Options::DnsModeNames[static_cast<int>(Pcsx2Config::DEV9Options::DnsMode::Auto)], "DEV9SettingsWidget");
	onEthDNSModeChanged(m_ui.ethDNS1Mode, m_ui.ethDNS1Mode->currentIndex(), m_ui.ethDNS1Addr, "DEV9/Eth", "ModeDNS1");
	connect(m_ui.ethDNS1Mode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [&](int index) { onEthDNSModeChanged(m_ui.ethDNS1Mode, index, m_ui.ethDNS1Addr, "DEV9/Eth", "ModeDNS1"); });

	SettingWidgetBinder::BindWidgetToEnumSetting(sif, m_ui.ethDNS2Mode, "DEV9/Eth", "ModeDNS2",
		s_dns_name, Pcsx2Config::DEV9Options::DnsModeNames, Pcsx2Config::DEV9Options::DnsModeNames[static_cast<int>(Pcsx2Config::DEV9Options::DnsMode::Auto)], "DEV9SettingsWidget");
	onEthDNSModeChanged(m_ui.ethDNS2Mode, m_ui.ethDNS2Mode->currentIndex(), m_ui.ethDNS2Addr, "DEV9/Eth", "ModeDNS2");
	connect(m_ui.ethDNS2Mode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [&](int index) { onEthDNSModeChanged(m_ui.ethDNS2Mode, index, m_ui.ethDNS2Addr, "DEV9/Eth", "ModeDNS2"); });

	//////////////////////////////////////////////////////////////////////////
	// DNS Settings
	//////////////////////////////////////////////////////////////////////////
	m_ethHost_model = new QStandardItemModel(0, 4, m_ui.ethHosts);

	QStringList headers;
	headers.push_back(tr("Name"));
	headers.push_back(tr("Url"));
	headers.push_back(tr("Address"));
	headers.push_back(tr("Enabled"));
	m_ethHost_model->setHorizontalHeaderLabels(headers);

	connect(m_ethHost_model, QOverload<QStandardItem*>::of(&QStandardItemModel::itemChanged), this, &DEV9SettingsWidget::onEthHostEdit);

	m_ethHosts_proxy = new QSortFilterProxyModel(m_ui.ethHosts);
	m_ethHosts_proxy->setSourceModel(m_ethHost_model);

	m_ui.ethHosts->setModel(m_ethHosts_proxy);
	m_ui.ethHosts->setItemDelegateForColumn(2, new IPItemDelegate(m_ui.ethHosts));

	RefreshHostList();

	m_ui.ethHosts->installEventFilter(this);

	connect(m_ui.ethHostAdd, &QPushButton::clicked, this, &DEV9SettingsWidget::onEthHostAdd);
	connect(m_ui.ethHostDel, &QPushButton::clicked, this, &DEV9SettingsWidget::onEthHostDel);

	connect(m_ui.ethHostExport, &QPushButton::clicked, this, &DEV9SettingsWidget::onEthHostExport);
	connect(m_ui.ethHostImport, &QPushButton::clicked, this, &DEV9SettingsWidget::onEthHostImport);

	connect(m_ui.ethHostPerGame, &QPushButton::clicked, this, &DEV9SettingsWidget::onEthHostPerGame);

	//////////////////////////////////////////////////////////////////////////
	// HDD Settings
	//////////////////////////////////////////////////////////////////////////
	connect(m_ui.hddEnabled, &QCheckBox::checkStateChanged, this, &DEV9SettingsWidget::onHddEnabledChanged);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.hddEnabled, "DEV9/Hdd", "HddEnable", false);

	if (m_dialog->isPerGameSettings())
	{
		m_ui.hddFile->setText(QString::fromUtf8(m_dialog->getStringValue("DEV9/Hdd", "HddFile", "").value().c_str()));
		m_ui.hddFile->setPlaceholderText(QString::fromUtf8(Host::GetBaseStringSettingValue("DEV9/Hdd", "HddFile", "DEV9hdd.raw")));
	}
	else
		m_ui.hddFile->setText(QString::fromUtf8(m_dialog->getStringValue("DEV9/Hdd", "HddFile", "DEV9hdd.raw").value().c_str()));

	connect(m_ui.hddLBA48, &QCheckBox::checkStateChanged, this, &DEV9SettingsWidget::onHddLBA48Changed);

	UpdateHddSizeUIValues();

	connect(m_ui.hddFile, &QLineEdit::textChanged, this, &DEV9SettingsWidget::onHddFileTextChange);
	connect(m_ui.hddFile, &QLineEdit::editingFinished, this, &DEV9SettingsWidget::onHddFileEdit);
	connect(m_ui.hddBrowseFile, &QPushButton::clicked, this, &DEV9SettingsWidget::onHddBrowseFileClicked);

	connect(m_ui.hddSizeSlider, QOverload<int>::of(&QSlider::valueChanged), this, &DEV9SettingsWidget::onHddSizeSlide);
	SettingWidgetBinder::SettingAccessor<QSpinBox>::connectValueChanged(m_ui.hddSizeSpinBox, [&]() { onHddSizeAccessorSpin(); });

	connect(m_ui.hddCreate, &QPushButton::clicked, this, &DEV9SettingsWidget::onHddCreateClicked);
}

void DEV9SettingsWidget::onEthEnabledChanged(Qt::CheckState state)
{
	const bool enabled = state == Qt::CheckState::PartiallyChecked ? Host::GetBaseBoolSettingValue("DEV9/Eth", "EthEnable", false) : state;

	//Populate Eth Device Settings
	if (enabled)
		LoadAdapters();

	m_ui.ethDevType->setEnabled(enabled);
	m_ui.ethDevTypeLabel->setEnabled(enabled);
	m_ui.ethDevLabel->setEnabled(enabled);
	m_ui.ethDev->setEnabled(enabled);
	m_ui.ethTabWidget->setEnabled(enabled);
}

void DEV9SettingsWidget::onEthDeviceTypeChanged(int index)
{
	{
		QSignalBlocker sb(m_ui.ethDev);
		m_ui.ethDev->clear();
	}

	Pcsx2Config::DEV9Options::NetApi selectedApi{Pcsx2Config::DEV9Options::NetApi ::Unset};

	if (index > 0)
	{
		std::vector<AdapterEntry> list = m_adapter_list[static_cast<u32>(m_api_list[index])];

		const std::string value = m_dialog->getEffectiveStringValue("DEV9/Eth", "EthDevice", "");
		for (size_t i = 0; i < list.size(); i++)
		{
			m_ui.ethDev->addItem(QString::fromUtf8(list[i].name));
			if (list[i].guid == value)
				m_ui.ethDev->setCurrentIndex(i);
		}

		selectedApi = m_api_list[index];
	}

	if (m_dialog->isPerGameSettings())
	{
		if (index == 0)
		{
			std::vector<AdapterEntry> list = m_adapter_list[static_cast<u32>(m_api_list[index])];
			m_ui.ethDev->addItem(tr("Use Global Setting [%1]").arg(QString::fromUtf8(list[0].name)));
			m_ui.ethDev->setCurrentIndex(0);
			m_ui.ethDev->setEnabled(false);

			selectedApi = m_global_api;
		}
		else
			m_ui.ethDev->setEnabled(true);
	}

	switch (selectedApi)
	{
#ifdef _WIN32
		case Pcsx2Config::DEV9Options::NetApi::TAP:
			m_adapter_options = TAPAdapter::GetAdapterOptions();
			break;
#endif
		case Pcsx2Config::DEV9Options::NetApi::PCAP_Bridged:
		case Pcsx2Config::DEV9Options::NetApi::PCAP_Switched:
			m_adapter_options = PCAPAdapter::GetAdapterOptions();
			break;
		case Pcsx2Config::DEV9Options::NetApi::Sockets:
			m_adapter_options = SocketAdapter::GetAdapterOptions();
			break;
		default:
			m_adapter_options = AdapterOptions::None;
			break;
	}

	m_ui.ethInterceptDHCPLabel->setEnabled((m_adapter_options & AdapterOptions::DHCP_ForcedOn) == AdapterOptions::None);
	m_ui.ethInterceptDHCP->setEnabled((m_adapter_options & AdapterOptions::DHCP_ForcedOn) == AdapterOptions::None);
	onEthDHCPInterceptChanged(m_ui.ethInterceptDHCP->checkState());
}

void DEV9SettingsWidget::onEthDeviceChanged(int index)
{
	if (index > 0)
	{
		const AdapterEntry& adapter = m_adapter_list[static_cast<u32>(m_api_list[m_ui.ethDevType->currentIndex()])][index];

		m_dialog->setStringSettingValue("DEV9/Eth", "EthApi", Pcsx2Config::DEV9Options::NetApiNames[static_cast<int>(adapter.type)]);
		m_dialog->setStringSettingValue("DEV9/Eth", "EthDevice", adapter.guid.c_str());
	}
	else if (m_dialog->isPerGameSettings() && m_ui.ethDevType->currentIndex() == 0 && index == 0)
	{
		m_dialog->setStringSettingValue("DEV9/Eth", "EthApi", std::nullopt);
		m_dialog->setStringSettingValue("DEV9/Eth", "EthDevice", std::nullopt);
	}
}

void DEV9SettingsWidget::onEthDHCPInterceptChanged(Qt::CheckState state)
{
	const bool enabled = (state == Qt::CheckState::PartiallyChecked ? Host::GetBaseBoolSettingValue("DEV9/Eth", "InterceptDHCP", false) : state) ||
						 ((m_adapter_options & AdapterOptions::DHCP_ForcedOn) == AdapterOptions::DHCP_ForcedOn);

	// clang-format off
	const bool ipOverride = (m_adapter_options & AdapterOptions::DHCP_OverrideIP)     == AdapterOptions::DHCP_OverrideIP;
	const bool snOverride = (m_adapter_options & AdapterOptions::DHCP_OverideSubnet)  == AdapterOptions::DHCP_OverideSubnet;
	const bool gwOverride = (m_adapter_options & AdapterOptions::DHCP_OverideGateway) == AdapterOptions::DHCP_OverideGateway;
	// clang-format on

	m_ui.ethPS2Addr->setEnabled(enabled && !ipOverride);
	m_ui.ethPS2AddrLabel->setEnabled(enabled && !ipOverride);

	m_ui.ethNetMaskLabel->setEnabled(enabled && !snOverride);
	m_ui.ethNetMaskAuto->setEnabled(enabled && !snOverride);
	onEthAutoChanged(m_ui.ethNetMaskAuto, m_ui.ethNetMaskAuto->checkState(), m_ui.ethNetMask, "DEV9/Eth", "AutoMask");

	m_ui.ethGatewayAddrLabel->setEnabled(enabled && !gwOverride);
	m_ui.ethGatewayAuto->setEnabled(enabled && !gwOverride);
	onEthAutoChanged(m_ui.ethGatewayAuto, m_ui.ethGatewayAuto->checkState(), m_ui.ethGatewayAddr, "DEV9/Eth", "AutoGateway");

	m_ui.ethDNS1AddrLabel->setEnabled(enabled);
	m_ui.ethDNS1Mode->setEnabled(enabled);
	onEthDNSModeChanged(m_ui.ethDNS1Mode, m_ui.ethDNS1Mode->currentIndex(), m_ui.ethDNS1Addr, "DEV9/Eth", "ModeDNS1");

	m_ui.ethDNS2AddrLabel->setEnabled(enabled);
	m_ui.ethDNS2Mode->setEnabled(enabled);
	onEthDNSModeChanged(m_ui.ethDNS2Mode, m_ui.ethDNS2Mode->currentIndex(), m_ui.ethDNS2Addr, "DEV9/Eth", "ModeDNS2");
}

void DEV9SettingsWidget::onEthIPChanged(QLineEdit* sender, const char* section, const char* key)
{
	//Alow clearing a per-game ip setting
	if (sender->text().isEmpty())
	{
		if (m_dialog->getStringValue(section, key, std::nullopt).has_value())
			m_dialog->setStringSettingValue(section, key, std::nullopt);
		return;
	}

	//should already be validated
	u8 bytes[4];
	std::string inputString = sender->text().toUtf8().constData();
	sscanf(inputString.c_str(), "%hhu.%hhu.%hhu.%hhu", &bytes[0], &bytes[1], &bytes[2], &bytes[3]);

	std::string neatStr = StringUtil::StdStringFromFormat("%u.%u.%u.%u", bytes[0], bytes[1], bytes[2], bytes[3]);

	sender->setText(QString::fromUtf8(neatStr.c_str()));

	std::string oldval = m_dialog->getStringValue(section, key, "0.0.0.0").value();
	if (neatStr != oldval)
		m_dialog->setStringSettingValue(section, key, neatStr.c_str());
}

void DEV9SettingsWidget::onEthAutoChanged(QCheckBox* sender, Qt::CheckState state, QLineEdit* input, const char* section, const char* key)
{
	if (sender->isEnabled())
	{
		const bool manual = !(state == Qt::CheckState::PartiallyChecked ? Host::GetBaseBoolSettingValue(section, key, true) : state);
		input->setEnabled(manual);
	}
	else
		input->setEnabled(false);
}

void DEV9SettingsWidget::onEthDNSModeChanged(QComboBox* sender, int index, QLineEdit* input, const char* section, const char* key)
{
	if (sender->isEnabled())
	{
		if (m_dialog->isPerGameSettings())
		{
			if (index == 0)
			{
				const std::string value = Host::GetBaseStringSettingValue(section, key, Pcsx2Config::DEV9Options::DnsModeNames[static_cast<int>(Pcsx2Config::DEV9Options::DnsMode::Auto)]);
				for (int i = 0; Pcsx2Config::DEV9Options::DnsModeNames[i] != nullptr; i++)
				{
					if (value == Pcsx2Config::DEV9Options::DnsModeNames[i])
					{
						index = i;
						break;
					}
				}
			}
			else
				index--;
		}
		const bool manual = index == static_cast<int>(Pcsx2Config::DEV9Options::DnsMode::Manual);
		input->setEnabled(manual);
	}
	else
		input->setEnabled(false);
}

void DEV9SettingsWidget::onEthHostAdd()
{
	HostEntryUi host;
	host.Desc = "New Host";
	host.Enabled = false;
	AddNewHostConfig(host);

	//Select new Item
	const QModelIndex viewIndex = m_ethHosts_proxy->mapFromSource(m_ethHost_model->index(m_ethHost_model->rowCount() - 1, 1));
	m_ui.ethHosts->scrollTo(viewIndex, QAbstractItemView::EnsureVisible);
	m_ui.ethHosts->selectionModel()->setCurrentIndex(viewIndex, QItemSelectionModel::ClearAndSelect);
}

void DEV9SettingsWidget::onEthHostDel()
{
	if (m_ui.ethHosts->selectionModel()->hasSelection())
	{
		const QModelIndex selectedIndex = m_ui.ethHosts->selectionModel()->currentIndex();
		const int modelRow = m_ethHosts_proxy->mapToSource(selectedIndex).row();
		DeleteHostConfig(modelRow);
	}
}

void DEV9SettingsWidget::onEthHostExport()
{
	std::vector<HostEntryUi> hosts = ListHostsConfig().value();

	DEV9DnsHostDialog exportDialog(hosts, this);

	std::optional<std::vector<HostEntryUi>> selectedHosts = exportDialog.PromptList();

	if (!selectedHosts.has_value())
		return;

	hosts = selectedHosts.value();

	if (hosts.size() == 0)
		return;

	QString path =
		QDir::toNativeSeparators(QFileDialog::getSaveFileName(QtUtils::GetRootWidget(this), tr("Hosts File"),
			"hosts.ini", tr("ini (*.ini)"), nullptr));

	if (path.isEmpty())
		return;

	std::unique_ptr<INISettingsInterface> exportFile = std::make_unique<INISettingsInterface>(path.toUtf8().constData());

	//Count is not exported
	for (size_t i = 0; i < hosts.size(); i++)
	{
		std::string section = "Host" + std::to_string(i);
		HostEntryUi entry = hosts[i];

		// clang-format off
		exportFile->SetStringValue(section.c_str(), "Url", entry.Url.c_str());
		exportFile->SetStringValue(section.c_str(), "Desc", entry.Desc.c_str());
		exportFile->SetStringValue(section.c_str(), "Address", entry.Address.c_str());
		exportFile->SetBoolValue(section.c_str(),   "Enabled", entry.Enabled);
		// clang-format on
	}

	exportFile->Save();

	QMessageBox::information(this, tr("DNS Hosts"),
		tr("Exported Successfully"),
		QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
}

void DEV9SettingsWidget::onEthHostImport()
{
	std::vector<HostEntryUi> hosts;

	QString path =
		QDir::toNativeSeparators(QFileDialog::getOpenFileName(QtUtils::GetRootWidget(this), tr("Hosts File"),
			"hosts.ini", tr("ini (*.ini)"), nullptr));

	if (path.isEmpty())
		return;

	std::unique_ptr<INISettingsInterface> importFile = std::make_unique<INISettingsInterface>(path.toUtf8().constData());

	if (!importFile->Load())
	{
		QMessageBox::warning(this, tr("DNS Hosts"),
			tr("Failed to open file"),
			QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
		return;
	}

	size_t count = 0;
	while (true)
	{
		std::string section = "Host" + std::to_string(count);
		HostEntryUi entry;

		entry.Url = importFile->GetStringValue(section.c_str(), "Url");

		if (entry.Url.empty())
			break;

		// clang-format off
		entry.Desc    = importFile->GetStringValue(section.c_str(), "Desc");
		entry.Address = importFile->GetStringValue(section.c_str(), "Address");
		entry.Enabled = importFile->GetBoolValue  (section.c_str(), "Enabled");
		// clang-format on

		hosts.push_back(entry);
		count++;
	}

	if (hosts.size() == 0)
	{
		QMessageBox::warning(this, tr("DNS Hosts"),
			tr("No Hosts in file"),
			QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
		return;
	}

	DEV9DnsHostDialog exportDialog(hosts, this);

	std::optional<std::vector<HostEntryUi>> selectedHosts = exportDialog.PromptList();

	if (!selectedHosts.has_value())
		return;

	hosts = selectedHosts.value();

	if (hosts.size() == 0)
		return;

	for (size_t i = 0; i < hosts.size(); i++)
		AddNewHostConfig(hosts[i]);

	QMessageBox::information(this, tr("DNS Hosts"),
		tr("Imported Successfully"),
		QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
}

void DEV9SettingsWidget::onEthHostPerGame()
{
	const std::optional<int> hostLengthOpt = m_dialog->getIntValue("DEV9/Eth/Hosts", "Count", std::nullopt);
	if (!hostLengthOpt.has_value())
	{
		QMessageBox::StandardButton ret = QMessageBox::question(this, tr("Per Game Host list"),
			tr("Copy global settings?"),
			QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No | QMessageBox::StandardButton::Cancel, QMessageBox::StandardButton::Yes);

		switch (ret)
		{
			case QMessageBox::StandardButton::No:
				m_dialog->setIntSettingValue("DEV9/Eth/Hosts", "Count", 0);
				break;

			case QMessageBox::StandardButton::Yes:
			{
				m_dialog->setIntSettingValue("DEV9/Eth/Hosts", "Count", 0);
				std::vector<HostEntryUi> hosts = ListBaseHostsConfig();
				for (size_t i = 0; i < hosts.size(); i++)
					AddNewHostConfig(hosts[i]);
				break;
			}

			case QMessageBox::StandardButton::Cancel:
				return;

			default:
				return;
		}
	}
	else
	{
		QMessageBox::StandardButton ret = QMessageBox::question(this, tr("Per Game Host list"),
			tr("Delete per game host list?"),
			QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::Cancel, QMessageBox::StandardButton::Yes);

		if (ret == QMessageBox::StandardButton::Yes)
		{
			const int hostLength = CountHostsConfig();
			for (int i = hostLength - 1; i >= 0; i--)
				DeleteHostConfig(i);
		}
		m_dialog->setIntSettingValue("DEV9/Eth/Hosts", nullptr, std::nullopt);
	}

	RefreshHostList();
}

void DEV9SettingsWidget::onEthHostEdit(QStandardItem* item)
{
	const int row = item->row();
	std::string section = "DEV9/Eth/Hosts/Host" + std::to_string(row);
	switch (item->column())
	{
		case 0: //Name
			m_dialog->setStringSettingValue(section.c_str(), "Desc", item->text().toUtf8().constData());
			break;
		case 1: //URL
			m_dialog->setStringSettingValue(section.c_str(), "Url", item->text().toUtf8().constData());
			break;
		case 2: //IP
			m_dialog->setStringSettingValue(section.c_str(), "Address", item->text().toUtf8().constData());
			break;
		case 3: //Enabled
			m_dialog->setBoolSettingValue(section.c_str(), "Enabled", item->checkState() == Qt::CheckState::Checked);
			break;
		default:
			break;
	}
}

void DEV9SettingsWidget::onHddEnabledChanged(Qt::CheckState state)
{
	const bool enabled = state == Qt::CheckState::PartiallyChecked ? Host::GetBaseBoolSettingValue("DEV9/Hdd", "HddEnable", false) : state;

	m_ui.hddFile->setEnabled(enabled);
	m_ui.hddFileLabel->setEnabled(enabled);
	m_ui.hddBrowseFile->setEnabled(enabled);
	m_ui.hddCreate->setEnabled(enabled);

	UpdateHddSizeUIEnabled();
}

void DEV9SettingsWidget::onHddBrowseFileClicked()
{
	QString path =
		QDir::toNativeSeparators(QFileDialog::getSaveFileName(QtUtils::GetRootWidget(this), tr("HDD Image File"),
			!m_ui.hddFile->text().isEmpty() ? m_ui.hddFile->text() : (!m_ui.hddFile->placeholderText().isEmpty() ? m_ui.hddFile->placeholderText() : "DEV9hdd.raw"),
			tr("HDD (*.raw)"), nullptr, QFileDialog::DontConfirmOverwrite));

	if (path.isEmpty())
		return;

	m_ui.hddFile->setText(path);
	m_ui.hddFile->editingFinished();
}

void DEV9SettingsWidget::onHddFileTextChange()
{
	UpdateHddSizeUIEnabled();

	// Force update so user doesn't have to exit text box
	std::string hddPath(m_ui.hddFile->text().toStdString());
	if (hddPath.empty())
		UpdateHddSizeUIValues();
}

void DEV9SettingsWidget::onHddFileEdit()
{
	// Check if file exists, if so set HddSize to correct value.
	// Also save the hddPath setting
	std::string hddPath(m_ui.hddFile->text().toStdString());
	if (hddPath.empty())
		m_dialog->setStringSettingValue("DEV9/Hdd", "HddFile", std::nullopt);
	else
		m_dialog->setStringSettingValue("DEV9/Hdd", "HddFile", hddPath.c_str());

	UpdateHddSizeUIValues();
}

void DEV9SettingsWidget::onHddSizeSlide(int i)
{
	QSignalBlocker sb(m_ui.hddSizeSpinBox);
	m_ui.hddSizeSpinBox->setValue(i);
}

void DEV9SettingsWidget::onHddSizeAccessorSpin()
{
	QSignalBlocker sb(m_ui.hddSizeSlider);
	m_ui.hddSizeSlider->setValue(m_ui.hddSizeSpinBox->value());
}

void DEV9SettingsWidget::onHddLBA48Changed(Qt::CheckState state)
{
	m_ui.hddSizeSlider->setMaximum((state != Qt::Unchecked) ? 2000 : 120);
	m_ui.hddSizeSpinBox->setMaximum((state != Qt::Unchecked) ? 2000 : 120);
	m_ui.hddSizeMaxLabel->setText((state != Qt::Unchecked) ? tr("2000") : tr("120"));
	// Bump up min size to have ticks align with 100GiB sizes
	m_ui.hddSizeSlider->setMinimum((state != Qt::Unchecked) ? 100 : 40);
	m_ui.hddSizeSpinBox->setMinimum((state != Qt::Unchecked) ? 100 : 40);
	m_ui.hddSizeMinLabel->setText((state != Qt::Unchecked) ? tr("100") : tr("40"));

	m_ui.hddSizeSlider->setTickInterval((state != Qt::Unchecked) ? 100 : 5);
}

void DEV9SettingsWidget::onHddCreateClicked()
{
	//Do the thing
	std::string hddPath(m_ui.hddFile->text().toStdString());

	const u64 sizeBytes = (u64)m_ui.hddSizeSpinBox->value() * (u64)(1024 * 1024 * 1024);

	if (sizeBytes == 0 || hddPath.empty())
	{
		QMessageBox::warning(this, QObject::tr("HDD Creator"),
			QObject::tr("Failed to create HDD image"),
			QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
		return;
	}

	if (!Path::IsAbsolute(hddPath))
		hddPath = Path::Combine(EmuFolders::Settings, hddPath);

	if (FileSystem::FileExists(hddPath.c_str()))
	{
		QMessageBox::StandardButton selection =
			QMessageBox::question(this, tr("Overwrite File?"),
				tr("HDD image \"%1\" already exists.\n\n"
				   "Do you want to overwrite?")
					.arg(QString::fromStdString(hddPath)),
				QMessageBox::Yes | QMessageBox::No);
		if (selection == QMessageBox::No)
			return;
		else
			FileSystem::DeleteFilePath(hddPath.c_str());
	}

	HddCreateQt hddCreator(this);
	hddCreator.filePath = std::move(hddPath);
	hddCreator.neededSize = sizeBytes;
	hddCreator.Start();

	if (!hddCreator.errored)
	{
		QMessageBox::information(this, tr("HDD Creator"),
			tr("HDD image created"),
			QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
	}
}

void DEV9SettingsWidget::UpdateHddSizeUIEnabled()
{
	std::string hddPath(m_ui.hddFile->text().toStdString());

	bool enableSizeUI;
	if (m_dialog->isPerGameSettings() && hddPath.empty())
		enableSizeUI = false;
	else
		enableSizeUI = m_ui.hddFile->isEnabled();

	m_ui.hddLBA48->setEnabled(enableSizeUI);
	m_ui.hddSizeLabel->setEnabled(enableSizeUI);
	m_ui.hddSizeSlider->setEnabled(enableSizeUI);
	m_ui.hddSizeMaxLabel->setEnabled(enableSizeUI);
	m_ui.hddSizeMinLabel->setEnabled(enableSizeUI);
	m_ui.hddSizeSpinBox->setEnabled(enableSizeUI);
}

void DEV9SettingsWidget::UpdateHddSizeUIValues()
{
	std::string hddPath(m_ui.hddFile->text().toStdString());

	if (m_dialog->isPerGameSettings() && hddPath.empty())
		hddPath = m_ui.hddFile->placeholderText().toStdString();

	if (!Path::IsAbsolute(hddPath))
		hddPath = Path::Combine(EmuFolders::Settings, hddPath);

	if (!FileSystem::FileExists(hddPath.c_str()))
		return;

	const s64 size = FileSystem::GetPathFileSize(hddPath.c_str());
	if (size < 0)
		return;

	if (size > static_cast<s64>(120) * 1024 * 1024 * 1024)
		m_ui.hddLBA48->setChecked(true);
	else
		m_ui.hddLBA48->setChecked(false);

	const int sizeGB = size / 1024 / 1024 / 1024;
	QSignalBlocker sb1(m_ui.hddSizeSpinBox);
	QSignalBlocker sb2(m_ui.hddSizeSlider);
	m_ui.hddSizeSpinBox->setValue(sizeGB);
	m_ui.hddSizeSlider->setValue(sizeGB);
}

void DEV9SettingsWidget::showEvent(QShowEvent* event)
{
	QWidget::showEvent(event);

	//Update the ethernet UI, as not done by constructor
	if (m_firstShow)
		onEthEnabledChanged(m_ui.ethEnabled->checkState());

	if (m_adaptersLoaded)
	{
		//The API combobox dosn't set the EthApi field, that is performed by the device combobox (in addition to saving the device)
		//This means that this setting can get out of sync with true value, so revert to that if the ui is closed and opened
		const std::string value = m_dialog->getStringValue("DEV9/Eth", "EthApi", Pcsx2Config::DEV9Options::NetApiNames[static_cast<int>(Pcsx2Config::DEV9Options::NetApi::Unset)]).value();

		//SignalBlocker to prevent saving a value already in the config file
		QSignalBlocker sb(m_ui.ethDev);
		for (int i = 0; m_api_namelist[i] != nullptr; i++)
		{
			if (value == m_api_valuelist[i])
			{
				m_ui.ethDevType->setCurrentIndex(i);
				break;
			}
		}
	}

	m_firstShow = false;
}

/*
 * QtUtils::ResizeColumnsForTableView() needs the widget to already be the correct size
 * Doing this in our resizeEvent (like GameListWidget does) dosn't work if the ui is
 * hidden, maybe because our table is nested within group & tab widgets
 * We could also listern to out show event, but we also need to listern to the tab
 * changed signal, in the event that another tab is selected when our ui is shown
 * 
 * Instead, lets use an eventFilter to determine exactly when the host table is shown
 * However, the eventFilter is ran before the widgets event handler, meaning the table
 * is still the wrong size, so we also need to check the show event
 */
bool DEV9SettingsWidget::eventFilter(QObject* object, QEvent* event)
{
	if (object == m_ui.ethHosts)
	{
		//Check isVisible to avoind an unnessecery call to ResizeColumnsForTableView()
		if (event->type() == QEvent::Resize && m_ui.ethHosts->isVisible())
			QtUtils::ResizeColumnsForTableView(m_ui.ethHosts, {-1, 170, 90, 80});
		else if (event->type() == QEvent::Show)
			QtUtils::ResizeColumnsForTableView(m_ui.ethHosts, {-1, 170, 90, 80});
	}
	return false;
}

void DEV9SettingsWidget::AddAdapter(const AdapterEntry& adapter)
{
	//divide into seperate adapter lists

	if (std::find(m_api_list.begin(), m_api_list.end(), adapter.type) == m_api_list.end())
		m_api_list.push_back(adapter.type);
	const u32 idx = static_cast<u32>(adapter.type);

	while (m_adapter_list.size() <= idx)
	{
		//Add blank adapter
		AdapterEntry blankAdapter;
		blankAdapter.guid = "";
		blankAdapter.name = "";
		blankAdapter.type = static_cast<Pcsx2Config::DEV9Options::NetApi>(m_adapter_list.size());
		m_adapter_list.push_back({blankAdapter});
	}

	m_adapter_list[idx].push_back(adapter);
}

void DEV9SettingsWidget::LoadAdapters()
{
	if (m_adaptersLoaded)
		return;

	QSignalBlocker sb(m_ui.ethDev);

	m_api_list.push_back(Pcsx2Config::DEV9Options::NetApi::Unset);

	for (const AdapterEntry& adapter : PCAPAdapter::GetAdapters())
		AddAdapter(adapter);
#ifdef _WIN32
	for (const AdapterEntry& adapter : TAPAdapter::GetAdapters())
		AddAdapter(adapter);
#endif
	for (const AdapterEntry& adapter : SocketAdapter::GetAdapters())
		AddAdapter(adapter);

	std::sort(m_api_list.begin(), m_api_list.end());
	for (auto& list : m_adapter_list)
		std::sort(list.begin(), list.end(), [](const AdapterEntry& a, AdapterEntry& b) { return a.name < b.name; });

	for (const Pcsx2Config::DEV9Options::NetApi& na : m_api_list)
	{
		m_api_namelist.push_back(s_api_name[static_cast<int>(na)]);
		m_api_valuelist.push_back(Pcsx2Config::DEV9Options::NetApiNames[static_cast<int>(na)]);
	}

	m_api_namelist.push_back(nullptr);
	m_api_valuelist.push_back(nullptr);

	//We replace the blank entry with one for global settings
	if (m_dialog->isPerGameSettings())
	{
		const std::string valueAPI = Host::GetBaseStringSettingValue("DEV9/Eth", "EthApi", Pcsx2Config::DEV9Options::NetApiNames[static_cast<int>(Pcsx2Config::DEV9Options::NetApi::Unset)]);
		for (int i = 0; Pcsx2Config::DEV9Options::NetApiNames[i] != nullptr; i++)
		{
			if (valueAPI == Pcsx2Config::DEV9Options::NetApiNames[i])
			{
				m_global_api = static_cast<Pcsx2Config::DEV9Options::NetApi>(i);
				break;
			}
		}

		std::vector<AdapterEntry> baseList = m_adapter_list[static_cast<u32>(m_global_api)];

		std::string baseAdapter = " ";
		const std::string valueGUID = Host::GetBaseStringSettingValue("DEV9/Eth", "EthDevice", "");
		for (size_t i = 0; i < baseList.size(); i++)
		{
			if (baseList[i].guid == valueGUID)
			{
				baseAdapter = baseList[i].name;
				break;
			}
		}

		m_adapter_list[static_cast<u32>(Pcsx2Config::DEV9Options::NetApi::Unset)][0].name = baseAdapter;
	}

	if (m_dialog->isPerGameSettings())
		m_ui.ethDevType->addItem(tr("Use Global Setting [%1]").arg(QString::fromUtf8(Pcsx2Config::DEV9Options::NetApiNames[static_cast<u32>(m_global_api)])));
	else
		m_ui.ethDevType->addItem(qApp->translate("DEV9SettingsWidget", m_api_namelist[0]));

	for (int i = 1; m_api_namelist[i] != nullptr; i++)
		m_ui.ethDevType->addItem(qApp->translate("DEV9SettingsWidget", m_api_namelist[i]));

	const std::string value = m_dialog->getStringValue("DEV9/Eth", "EthApi", Pcsx2Config::DEV9Options::NetApiNames[static_cast<int>(Pcsx2Config::DEV9Options::NetApi::Unset)]).value();

	for (int i = 0; m_api_namelist[i] != nullptr; i++)
	{
		if (value == m_api_valuelist[i])
		{
			m_ui.ethDevType->setCurrentIndex(i);
			break;
		}
	}
	//onEthDeviceTypeChanged gets called automatically

	m_adaptersLoaded = true;
}

void DEV9SettingsWidget::RefreshHostList()
{
	while (m_ethHost_model->rowCount() > 0)
		m_ethHost_model->removeRow(0);

	bool enableHostsUi;

	std::vector<HostEntryUi> hosts;

	if (m_dialog->isPerGameSettings())
	{
		m_ui.ethHostPerGame->setVisible(true);

		std::optional<std::vector<HostEntryUi>> hostsOpt = ListHostsConfig();
		if (hostsOpt.has_value())
		{
			m_ui.ethHostPerGame->setText(tr("Use Global"));
			hosts = hostsOpt.value();
			enableHostsUi = true;
		}
		else
		{
			m_ui.ethHostPerGame->setText(tr("Override"));
			hosts = ListBaseHostsConfig();
			enableHostsUi = false;
		}
	}
	else
	{
		m_ui.ethHostPerGame->setVisible(false);
		hosts = ListHostsConfig().value();
		enableHostsUi = true;
	}

	m_ui.ethHosts->setEnabled(enableHostsUi);
	m_ui.ethHostAdd->setEnabled(enableHostsUi);
	m_ui.ethHostDel->setEnabled(enableHostsUi);
	m_ui.ethHostExport->setEnabled(enableHostsUi);
	m_ui.ethHostImport->setEnabled(enableHostsUi);

	//Load list
	for (size_t i = 0; i < hosts.size(); i++)
	{
		HostEntryUi entry = hosts[i];
		const int row = m_ethHost_model->rowCount();
		m_ethHost_model->insertRow(row);

		QSignalBlocker sb(m_ethHost_model);

		QStandardItem* nameItem = new QStandardItem();
		nameItem->setText(QString::fromStdString(entry.Desc));
		m_ethHost_model->setItem(row, 0, nameItem);

		QStandardItem* urlItem = new QStandardItem();
		urlItem->setText(QString::fromStdString(entry.Url));
		m_ethHost_model->setItem(row, 1, urlItem);

		QStandardItem* addressItem = new QStandardItem();
		addressItem->setText(QString::fromStdString(entry.Address));
		m_ethHost_model->setItem(row, 2, addressItem);

		QStandardItem* enabledItem = new QStandardItem();
		enabledItem->setEditable(false);
		enabledItem->setCheckable(true);
		enabledItem->setCheckState(entry.Enabled ? Qt::CheckState::Checked : Qt::CheckState::Unchecked);
		m_ethHost_model->setItem(row, 3, enabledItem);
	}

	m_ui.ethHosts->sortByColumn(0, Qt::AscendingOrder);
}

int DEV9SettingsWidget::CountHostsConfig()
{
	return m_dialog->getIntValue("DEV9/Eth/Hosts", "Count", 0).value();
}

std::optional<std::vector<HostEntryUi>> DEV9SettingsWidget::ListHostsConfig()
{
	std::vector<HostEntryUi> hosts;

	std::optional<int> hostLengthOpt;
	if (m_dialog->isPerGameSettings())
	{
		hostLengthOpt = m_dialog->getIntValue("DEV9/Eth/Hosts", "Count", std::nullopt);
		if (!hostLengthOpt.has_value())
			return std::nullopt;
	}
	else
		hostLengthOpt = m_dialog->getIntValue("DEV9/Eth/Hosts", "Count", 0);

	const int hostLength = hostLengthOpt.value();
	for (int i = 0; i < hostLength; i++)
	{
		std::string section = "DEV9/Eth/Hosts/Host" + std::to_string(i);

		HostEntryUi entry;
		entry.Url = m_dialog->getStringValue(section.c_str(), "Url", "").value();
		entry.Desc = m_dialog->getStringValue(section.c_str(), "Desc", "").value();
		entry.Address = m_dialog->getStringValue(section.c_str(), "Address", "").value();
		entry.Enabled = m_dialog->getBoolValue(section.c_str(), "Enabled", false).value();
		hosts.push_back(entry);
	}

	return hosts;
}

std::vector<HostEntryUi> DEV9SettingsWidget::ListBaseHostsConfig()
{
	std::vector<HostEntryUi> hosts;

	const int hostLength = Host::GetBaseIntSettingValue("DEV9/Eth/Hosts", "Count", 0);
	for (int i = 0; i < hostLength; i++)
	{
		std::string section = "DEV9/Eth/Hosts/Host" + std::to_string(i);

		HostEntryUi entry;
		entry.Url = Host::GetBaseStringSettingValue(section.c_str(), "Url", "");
		entry.Desc = Host::GetBaseStringSettingValue(section.c_str(), "Desc", "");
		entry.Address = Host::GetBaseStringSettingValue(section.c_str(), "Address", "");
		entry.Enabled = Host::GetBaseBoolSettingValue(section.c_str(), "Enabled", false);
		hosts.push_back(entry);
	}

	return hosts;
}

void DEV9SettingsWidget::AddNewHostConfig(const HostEntryUi& host)
{
	const int hostLength = CountHostsConfig();
	std::string section = "DEV9/Eth/Hosts/Host" + std::to_string(hostLength);
	// clang-format off
	m_dialog->setStringSettingValue(section.c_str(), "Url",     host.Url.c_str());
	m_dialog->setStringSettingValue(section.c_str(), "Desc",    host.Desc.c_str());
	m_dialog->setStringSettingValue(section.c_str(), "Address", host.Address.c_str());
	m_dialog->setBoolSettingValue  (section.c_str(), "Enabled", host.Enabled);
	// clang-format on
	m_dialog->setIntSettingValue("DEV9/Eth/Hosts", "Count", hostLength + 1);
	RefreshHostList();
}

void DEV9SettingsWidget::DeleteHostConfig(int index)
{
	const int hostLength = CountHostsConfig();

	//Shuffle entries down to ovewrite deleted entry
	for (int i = index; i < hostLength - 1; i++)
	{
		std::string section = "DEV9/Eth/Hosts/Host" + std::to_string(i);
		std::string sectionAhead = "DEV9/Eth/Hosts/Host" + std::to_string(i + 1);

		// clang-format off
		m_dialog->setStringSettingValue(section.c_str(), "Url",     m_dialog->getStringValue(sectionAhead.c_str(), "Url",     "").value().c_str());
		m_dialog->setStringSettingValue(section.c_str(), "Desc",    m_dialog->getStringValue(sectionAhead.c_str(), "Desc",    "").value().c_str());
		m_dialog->setStringSettingValue(section.c_str(), "Address", m_dialog->getStringValue(sectionAhead.c_str(), "Address", "0.0.0.0").value().c_str());
		m_dialog->setBoolSettingValue  (section.c_str(), "Enabled", m_dialog->getBoolValue  (sectionAhead.c_str(), "Enabled", false).value());
		// clang-format on
	}

	//Delete last entry
	std::string section = "DEV9/Eth/Hosts/Host" + std::to_string(hostLength - 1);
	//Specifying a value of nullopt will delete the key
	//if the key is a nullptr, the whole section is deleted
	m_dialog->setStringSettingValue(section.c_str(), nullptr, std::nullopt);

	m_dialog->setIntSettingValue("DEV9/Eth/Hosts", "Count", hostLength - 1);
	RefreshHostList();
}

DEV9SettingsWidget::~DEV9SettingsWidget() = default;
