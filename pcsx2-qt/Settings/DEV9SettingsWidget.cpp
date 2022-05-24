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

#include <QtWidgets/QMessageBox>
#include <QtWidgets/QFileDialog>
#include <algorithm>

#include "common/StringUtil.h"

#include "pcsx2/HostSettings.h"

#include "DEV9SettingsWidget.h"
#include "EmuThread.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"
#include "SettingsDialog.h"

#include "HddCreateQt.h"

#include "DEV9/pcap_io.h"
#ifdef _WIN32
#include "DEV9/Win32/tap.h"
#endif
#include "DEV9/sockets.h"

static const char* s_api_name[] = {
	QT_TRANSLATE_NOOP("DEV9SettingsWidget", " "),
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

#define IP_RANGE_INTER "([0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5]|)"
#define IP_RANGE_FINAL "([0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5])"

// clang-format off
const QRegularExpression IPValidator::intermediateRegex{QStringLiteral("^" IP_RANGE_INTER "\\." IP_RANGE_INTER "\\." IP_RANGE_INTER "\\." IP_RANGE_INTER "$")};
const QRegularExpression IPValidator::finalRegex       {QStringLiteral("^" IP_RANGE_FINAL "\\." IP_RANGE_FINAL "\\." IP_RANGE_FINAL "\\." IP_RANGE_FINAL "$")};
// clang-format on

IPValidator::IPValidator(QObject* parent, bool allowEmpty)
	: QValidator(parent)
	, m_allowEmpty{allowEmpty}
{
}

QValidator::State IPValidator::validate(QString& input, int& pos) const
{
	if (input.isEmpty())
		return m_allowEmpty ? Acceptable : Intermediate;

	QRegularExpressionMatch m = finalRegex.match(input, 0, QRegularExpression::NormalMatch);
	if (m.hasMatch())
		return Acceptable;

	m = intermediateRegex.match(input, 0, QRegularExpression::PartialPreferCompleteMatch);
	if (m.hasMatch() || m.hasPartialMatch())
		return Intermediate;
	else
	{
		pos = input.size();
		return Invalid;
	}
}

IPItemDelegate::IPItemDelegate(QObject* parent)
	: QItemDelegate(parent)
{
}

QWidget* IPItemDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	QLineEdit* editor = new QLineEdit(parent);
	editor->setValidator(new IPValidator());
	return editor;
}

void IPItemDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
	QString value = index.model()->data(index, Qt::EditRole).toString();
	QLineEdit* line = static_cast<QLineEdit*>(editor);
	line->setText(value);
}

void IPItemDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
	QLineEdit* line = static_cast<QLineEdit*>(editor);
	QString value = line->text();
	model->setData(index, value);
}

void IPItemDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	editor->setGeometry(option.rect);
}


DEV9SettingsWidget::DEV9SettingsWidget(SettingsDialog* dialog, QWidget* parent)
	: QWidget(parent)
	, m_dialog{dialog}
{
	SettingsInterface* sif = dialog->getSettingsInterface();

	m_ui.setupUi(this);

	//////////////////////////////////////////////////////////////////////////
	// Eth Enabled
	//////////////////////////////////////////////////////////////////////////
	//Connect needs to be after BindWidgetToBoolSetting to ensure correct order of execution for disabling a per game setting
	//but we then need to manually call onEthAutoChanged to update the UI on fist load
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.ethEnabled, "DEV9/Eth", "EthEnable", false);
	onEthEnabledChanged(m_ui.ethEnabled->checkState());
	connect(m_ui.ethEnabled, QOverload<int>::of(&QCheckBox::stateChanged), this, &DEV9SettingsWidget::onEthEnabledChanged);

	//////////////////////////////////////////////////////////////////////////
	// Eth Device Settings
	//////////////////////////////////////////////////////////////////////////
	connect(m_ui.ethDevType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DEV9SettingsWidget::onEthDeviceTypeChanged);

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
		m_ui.ethDevType->addItem(QString::fromUtf8(m_api_namelist[0]));

	for (int i = 1; m_api_namelist[i] != nullptr; i++)
		m_ui.ethDevType->addItem(QString::fromUtf8(m_api_namelist[i]));

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

	connect(m_ui.ethDev, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DEV9SettingsWidget::onEthDeviceChanged);

	//////////////////////////////////////////////////////////////////////////
	// DHCP Settings
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.ethInterceptDHCP, "DEV9/Eth", "InterceptDHCP", false);
	onEthDHCPInterceptChanged(m_ui.ethInterceptDHCP->checkState());
	connect(m_ui.ethInterceptDHCP, QOverload<int>::of(&QCheckBox::stateChanged), this, &DEV9SettingsWidget::onEthDHCPInterceptChanged);

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
	connect(m_ui.ethNetMaskAuto, QOverload<int>::of(&QCheckBox::stateChanged), this, [&](int state) { onEthAutoChanged(m_ui.ethNetMaskAuto, state, m_ui.ethNetMask, "DEV9/Eth", "AutoMask"); });

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.ethGatewayAuto, "DEV9/Eth", "AutoGateway", true);
	onEthAutoChanged(m_ui.ethGatewayAuto, m_ui.ethGatewayAuto->checkState(), m_ui.ethGatewayAddr, "DEV9/Eth", "AutoGateway");
	connect(m_ui.ethGatewayAuto, QOverload<int>::of(&QCheckBox::stateChanged), this, [&](int state) { onEthAutoChanged(m_ui.ethGatewayAuto, state, m_ui.ethGatewayAddr, "DEV9/Eth", "AutoGateway"); });

	SettingWidgetBinder::BindWidgetToEnumSetting(sif, m_ui.ethDNS1Mode, "DEV9/Eth", "ModeDNS1",
		s_dns_name, Pcsx2Config::DEV9Options::DnsModeNames, Pcsx2Config::DEV9Options::DnsModeNames[static_cast<int>(Pcsx2Config::DEV9Options::DnsMode::Auto)]);
	onEthDNSModeChanged(m_ui.ethDNS1Mode, m_ui.ethDNS1Mode->currentIndex(), m_ui.ethDNS1Addr, "DEV9/Eth", "ModeDNS1");
	connect(m_ui.ethDNS1Mode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [&](int index) { onEthDNSModeChanged(m_ui.ethDNS1Mode, index, m_ui.ethDNS1Addr, "DEV9/Eth", "ModeDNS1"); });

	SettingWidgetBinder::BindWidgetToEnumSetting(sif, m_ui.ethDNS2Mode, "DEV9/Eth", "ModeDNS2",
		s_dns_name, Pcsx2Config::DEV9Options::DnsModeNames, Pcsx2Config::DEV9Options::DnsModeNames[static_cast<int>(Pcsx2Config::DEV9Options::DnsMode::Auto)]);
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

	if (m_dialog->isPerGameSettings())
		m_ui.ethTabWidget->setTabEnabled(1, false);

	//////////////////////////////////////////////////////////////////////////
	// HDD Settings
	//////////////////////////////////////////////////////////////////////////
	connect(m_ui.hddEnabled, QOverload<int>::of(&QCheckBox::stateChanged), this, &DEV9SettingsWidget::onHddEnabledChanged);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.hddEnabled, "DEV9/Hdd", "HddEnable", false);

	connect(m_ui.hddFile, &QLineEdit::editingFinished, this, &DEV9SettingsWidget::onHddFileEdit);
	SettingWidgetBinder::BindWidgetToStringSetting(sif, m_ui.hddFile, "DEV9/Hdd", "HddFile", "DEV9hdd.raw");
	if (m_dialog->isPerGameSettings())
		m_ui.hddFile->setPlaceholderText(QString::fromUtf8(Host::GetBaseStringSettingValue("DEV9/Hdd", "HddFile", "DEV9hdd.raw")));
	connect(m_ui.hddBrowseFile, &QPushButton::clicked, this, &DEV9SettingsWidget::onHddBrowseFileClicked);

	//TODO: need a getUintValue for if 48bit support occurs
	const int size = (u64)m_dialog->getIntValue("DEV9/Hdd", "HddSizeSectors", 0).value() * 512 / (1024 * 1024 * 1024);

	if (m_dialog->isPerGameSettings())
	{
		const int sizeGlobal = (u64)Host::GetBaseIntSettingValue("DEV9/Hdd", "HddSizeSectors", 0) * 512 / (1024 * 1024 * 1024);
		m_ui.hddSizeSpinBox->setMinimum(39);
		m_ui.hddSizeSpinBox->setSpecialValueText(tr("Global [%1]").arg(sizeGlobal));
	}

	// clang-format off
	m_ui.hddSizeSlider ->setValue(size);
	m_ui.hddSizeSpinBox->setValue(size);

	connect(m_ui.hddSizeSlider,  QOverload<int>::of(&QSlider ::valueChanged), this, &DEV9SettingsWidget::onHddSizeSlide);
	connect(m_ui.hddSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &DEV9SettingsWidget::onHddSizeSpin );
	// clang-format on

	connect(m_ui.hddCreate, &QPushButton::clicked, this, &DEV9SettingsWidget::onHddCreateClicked);
}

void DEV9SettingsWidget::onEthEnabledChanged(int state)
{
	const bool enabled = state == Qt::CheckState::PartiallyChecked ? Host::GetBaseBoolSettingValue("DEV9/Eth", "EthEnable", false) : state;

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

void DEV9SettingsWidget::onEthDHCPInterceptChanged(int state)
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

void DEV9SettingsWidget::onEthAutoChanged(QCheckBox* sender, int state, QLineEdit* input, const char* section, const char* key)
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

void DEV9SettingsWidget::onHddEnabledChanged(int state)
{
	const bool enabled = state == Qt::CheckState::PartiallyChecked ? m_dialog->getEffectiveBoolValue("DEV9/Hdd", "HddEnable", false) : state;

	m_ui.hddFile->setEnabled(enabled);
	m_ui.hddFileLabel->setEnabled(enabled);
	m_ui.hddBrowseFile->setEnabled(enabled);
	m_ui.hddSizeLabel->setEnabled(enabled);
	m_ui.hddSizeSlider->setEnabled(enabled);
	m_ui.hddSizeMaxLabel->setEnabled(enabled);
	m_ui.hddSizeMinLabel->setEnabled(enabled);
	m_ui.hddSizeSpinBox->setEnabled(enabled);
	m_ui.hddCreate->setEnabled(enabled);
}

void DEV9SettingsWidget::onHddBrowseFileClicked()
{
	QString path =
		QDir::toNativeSeparators(QFileDialog::getSaveFileName(QtUtils::GetRootWidget(this), tr("HDD Image File"),
			!m_ui.hddFile->text().isEmpty() ? m_ui.hddFile->text() : "DEV9hdd.raw", tr("HDD (*.raw)"), nullptr,
			QFileDialog::DontConfirmOverwrite));

	if (path.isEmpty())
		return;

	m_ui.hddFile->setText(path);
	m_ui.hddFile->editingFinished();
}

void DEV9SettingsWidget::onHddFileEdit()
{
	//Check if file exists, if so set HddSize to correct value
	//GHC uses UTF8 on all platforms
	ghc::filesystem::path hddPath(m_ui.hddFile->text().toUtf8().constData());

	if (hddPath.empty())
		return;

	if (hddPath.is_relative())
	{
		ghc::filesystem::path path(EmuFolders::Settings);
		hddPath = path / hddPath;
	}

	if (!ghc::filesystem::exists(hddPath))
		return;

	const uintmax_t size = ghc::filesystem::file_size(hddPath);

	const u32 sizeSectors = (size / 512);
	const int sizeGB = size / 1024 / 1024 / 1024;

	QSignalBlocker sb1(m_ui.hddSizeSpinBox);
	QSignalBlocker sb2(m_ui.hddSizeSlider);
	m_ui.hddSizeSpinBox->setValue(sizeGB);
	m_ui.hddSizeSlider->setValue(sizeGB);

	m_dialog->setIntSettingValue("DEV9/Hdd", "HddSizeSectors", (int)sizeSectors);
}

void DEV9SettingsWidget::onHddSizeSlide(int i)
{
	QSignalBlocker sb(m_ui.hddSizeSpinBox);
	m_ui.hddSizeSpinBox->setValue(i);

	m_dialog->setIntSettingValue("DEV9/Hdd", "HddSizeSectors", (int)((s64)i * 1024 * 1024 * 1024 / 512));
}

void DEV9SettingsWidget::onHddSizeSpin(int i)
{
	QSignalBlocker sb(m_ui.hddSizeSlider);
	m_ui.hddSizeSlider->setValue(i);

	//TODO: need a setUintSettingValue for if 48bit support occurs
	if (i == 39)
		m_dialog->setIntSettingValue("DEV9/Hdd", "HddSizeSectors", std::nullopt);
	else
		m_dialog->setIntSettingValue("DEV9/Hdd", "HddSizeSectors", i * (1024 * 1024 * 1024 / 512));
}

void DEV9SettingsWidget::onHddCreateClicked()
{
	//Do the thing
	ghc::filesystem::path hddPath(m_ui.hddFile->text().toUtf8().constData());

	u64 sizeBytes = (u64)m_dialog->getEffectiveIntValue("DEV9/Hdd", "HddSizeSectors", 0) * 512;
	if (sizeBytes == 0 || hddPath.empty())
	{
		QMessageBox::warning(this, QObject::tr("HDD Creator"),
			QObject::tr("Failed to create HDD image"),
			QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
		return;
	}

	if (hddPath.is_relative())
	{
		//Note, EmuFolders is still wx strings
		ghc::filesystem::path path(EmuFolders::Settings);
		hddPath = path / hddPath;
	}

	if (ghc::filesystem::exists(hddPath))
	{
		//GHC uses UTF8 on all platforms
		QMessageBox::StandardButton selection =
			QMessageBox::question(this, tr("Overwrite File?"),
				tr("HDD image \"%1\" already exists?\n\n"
				   "Do you want to overwrite?")
					.arg(QString::fromUtf8(hddPath.u8string().c_str())),
				QMessageBox::Yes | QMessageBox::No);
		if (selection == QMessageBox::No)
			return;
		else
			ghc::filesystem::remove(hddPath);
	}

	HddCreateQt hddCreator(this);
	hddCreator.filePath = hddPath;
	hddCreator.neededSize = sizeBytes;
	hddCreator.Start();

	if (!hddCreator.errored)
	{
		QMessageBox::information(this, tr("HDD Creator"),
			tr("HDD image created"),
			QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
	}
}

void DEV9SettingsWidget::showEvent(QShowEvent* event)
{
	QWidget::showEvent(event);

	//The API combobox dosn't set the EthApi field, that is performed by the device combobox (in addition to saving the device)
	//This means that this setting can get out of sync with true value, so revert to that if the ui is closed and opened
	const std::string value = m_dialog->getStringValue("DEV9/Eth", "EthApi", Pcsx2Config::DEV9Options::NetApiNames[static_cast<int>(Pcsx2Config::DEV9Options::NetApi::Unset)]).value();

	//disconnect temporally to prevent saving a vaule already in the config file
	disconnect(m_ui.ethDev, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DEV9SettingsWidget::onEthDeviceChanged);
	for (int i = 0; m_api_namelist[i] != nullptr; i++)
	{
		if (value == m_api_valuelist[i])
		{
			m_ui.ethDevType->setCurrentIndex(i);
			break;
		}
	}
	connect(m_ui.ethDev, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DEV9SettingsWidget::onEthDeviceChanged);
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

void DEV9SettingsWidget::RefreshHostList()
{
	while (m_ethHost_model->rowCount() > 0)
		m_ethHost_model->removeRow(0);

	//Load list
	std::vector<HostEntryUi> hosts;

	const int hostLength = CountHostsConfig();
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

	for (int i = 0; i < hostLength; i++)
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
