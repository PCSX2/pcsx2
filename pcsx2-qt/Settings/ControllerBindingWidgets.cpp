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

#include <QtCore/QDir>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QScrollArea>
#include <algorithm>
#include "fmt/format.h"

#include "common/StringUtil.h"

#include "pcsx2/Host.h"
#include "pcsx2/PAD/Host/PAD.h"

#include "Settings/ControllerBindingWidgets.h"
#include "Settings/ControllerSettingsDialog.h"
#include "Settings/ControllerSettingWidgetBinder.h"
#include "Settings/SettingsDialog.h"
#include "QtHost.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"

#include "ui_USBBindingWidget_DrivingForce.h"
#include "ui_USBBindingWidget_GTForce.h"

ControllerBindingWidget::ControllerBindingWidget(QWidget* parent, ControllerSettingsDialog* dialog, u32 port)
	: QWidget(parent)
	, m_dialog(dialog)
	, m_config_section(fmt::format("Pad{}", port + 1))
	, m_port_number(port)
{
	m_ui.setupUi(this);
	m_ui.groupBox->setTitle(tr("Controller Port %1").arg(port + 1));

	populateControllerTypes();
	onTypeChanged();

	ControllerSettingWidgetBinder::BindWidgetToInputProfileString(
		m_dialog->getProfileSettingsInterface(), m_ui.controllerType, m_config_section, "Type", PAD::GetDefaultPadType(port));

	connect(m_ui.controllerType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ControllerBindingWidget::onTypeChanged);
	connect(m_ui.bindings, &QPushButton::clicked, this, &ControllerBindingWidget::onBindingsClicked);
	connect(m_ui.settings, &QPushButton::clicked, this, &ControllerBindingWidget::onSettingsClicked);
	connect(m_ui.macros, &QPushButton::clicked, this, &ControllerBindingWidget::onMacrosClicked);
	connect(m_ui.automaticBinding, &QPushButton::clicked, this, &ControllerBindingWidget::onAutomaticBindingClicked);
	connect(m_ui.clearBindings, &QPushButton::clicked, this, &ControllerBindingWidget::onClearBindingsClicked);
}

ControllerBindingWidget::~ControllerBindingWidget() = default;

QIcon ControllerBindingWidget::getIcon() const
{
	return m_bindings_widget->getIcon();
}

void ControllerBindingWidget::populateControllerTypes()
{
	for (const auto& [name, display_name] : PAD::GetControllerTypeNames())
		m_ui.controllerType->addItem(qApp->translate("Pad", display_name), QString::fromStdString(name));
}

void ControllerBindingWidget::onTypeChanged()
{
	const bool is_initializing = (m_ui.stackedWidget->count() == 0);
	m_controller_type = m_dialog->getStringValue(m_config_section.c_str(), "Type", PAD::GetDefaultPadType(m_port_number));

	if (m_bindings_widget)
	{
		m_ui.stackedWidget->removeWidget(m_bindings_widget);
		delete m_bindings_widget;
		m_bindings_widget = nullptr;
	}
	if (m_settings_widget)
	{
		m_ui.stackedWidget->removeWidget(m_settings_widget);
		delete m_settings_widget;
		m_settings_widget = nullptr;
	}
	if (m_macros_widget)
	{
		m_ui.stackedWidget->removeWidget(m_macros_widget);
		delete m_macros_widget;
		m_macros_widget = nullptr;
	}

	const PAD::ControllerInfo* cinfo = PAD::GetControllerInfo(m_controller_type);
	const bool has_settings = (cinfo && cinfo->num_settings > 0);
	const bool has_macros = (cinfo && cinfo->num_bindings > 0);
	m_ui.settings->setEnabled(has_settings);
	m_ui.macros->setEnabled(has_macros);

	if (m_controller_type == "DualShock2")
		m_bindings_widget = ControllerBindingWidget_DualShock2::createInstance(this);
	else
		m_bindings_widget = new ControllerBindingWidget_Base(this);

	m_ui.stackedWidget->addWidget(m_bindings_widget);
	m_ui.stackedWidget->setCurrentWidget(m_bindings_widget);

	if (has_settings)
	{
		const gsl::span<const SettingInfo> settings(cinfo->settings, cinfo->num_settings);
		m_settings_widget = new ControllerCustomSettingsWidget(
			settings, m_config_section, std::string(), "Pad", getDialog(), m_ui.stackedWidget);
		m_ui.stackedWidget->addWidget(m_settings_widget);
	}

	if (has_macros)
	{
		m_macros_widget = new ControllerMacroWidget(this);
		m_ui.stackedWidget->addWidget(m_macros_widget);
	}

	updateHeaderToolButtons();

	// no need to do this on first init, only changes
	if (!is_initializing)
		m_dialog->updateListDescription(m_port_number, this);
}

void ControllerBindingWidget::updateHeaderToolButtons()
{
	const QWidget* current_widget = m_ui.stackedWidget->currentWidget();
	const QSignalBlocker bindings_sb(m_ui.bindings);
	const QSignalBlocker settings_sb(m_ui.settings);
	const QSignalBlocker macros_sb(m_ui.macros);

	const bool is_bindings = (current_widget == m_bindings_widget);
	m_ui.bindings->setChecked(is_bindings);
	m_ui.automaticBinding->setEnabled(is_bindings);
	m_ui.clearBindings->setEnabled(is_bindings);
	m_ui.macros->setChecked(current_widget == m_macros_widget);
	m_ui.settings->setChecked((current_widget == m_settings_widget));
}

void ControllerBindingWidget::onBindingsClicked()
{
	m_ui.stackedWidget->setCurrentWidget(m_bindings_widget);
	updateHeaderToolButtons();
}

void ControllerBindingWidget::onSettingsClicked()
{
	if (!m_settings_widget)
		return;

	m_ui.stackedWidget->setCurrentWidget(m_settings_widget);
	updateHeaderToolButtons();
}

void ControllerBindingWidget::onMacrosClicked()
{
	if (!m_macros_widget)
		return;

	m_ui.stackedWidget->setCurrentWidget(m_macros_widget);
	updateHeaderToolButtons();
}

void ControllerBindingWidget::onAutomaticBindingClicked()
{
	QMenu menu(this);
	bool added = false;

	for (const QPair<QString, QString>& dev : m_dialog->getDeviceList())
	{
		// we set it as data, because the device list could get invalidated while the menu is up
		QAction* action = menu.addAction(QStringLiteral("%1 (%2)").arg(dev.first).arg(dev.second));
		action->setData(dev.first);
		connect(action, &QAction::triggered, this, [this, action]() { doDeviceAutomaticBinding(action->data().toString()); });
		added = true;
	}

	if (!added)
	{
		QAction* action = menu.addAction(tr("No devices available"));
		action->setEnabled(false);
	}

	menu.exec(QCursor::pos());
}

void ControllerBindingWidget::onClearBindingsClicked()
{
	//: Binding: A pair of (host button, target button); Mapping: A list of bindings covering an entire controller. These are two different things (which might be the same in your language, please make sure to verify this).
	if (QMessageBox::question(QtUtils::GetRootWidget(this), tr("Clear Bindings"),
			//: Binding: A pair of (host button, target button); Mapping: A list of bindings covering an entire controller. These are two different things (which might be the same in your language, please make sure to verify this).
			tr("Are you sure you want to clear all bindings for this controller? This action cannot be undone.")) != QMessageBox::Yes)
	{
		return;
	}

	if (m_dialog->isEditingGlobalSettings())
	{
		{
			auto lock = Host::GetSettingsLock();
			PAD::ClearPortBindings(*Host::Internal::GetBaseSettingsLayer(), m_port_number);
		}
		Host::CommitBaseSettingChanges();
	}
	else
	{
		PAD::ClearPortBindings(*m_dialog->getProfileSettingsInterface(), m_port_number);
		m_dialog->getProfileSettingsInterface()->Save();
	}

	// force a refresh after clearing
	g_emu_thread->applySettings();
	onTypeChanged();
}

void ControllerBindingWidget::doDeviceAutomaticBinding(const QString& device)
{
	std::vector<std::pair<GenericInputBinding, std::string>> mapping = InputManager::GetGenericBindingMapping(device.toStdString());
	if (mapping.empty())
	{
		QMessageBox::critical(QtUtils::GetRootWidget(this), tr("Automatic Binding"),
			tr("No generic bindings were generated for device '%1'. The controller/source may not support automatic mapping.").arg(device));
		return;
	}

	bool result;
	if (m_dialog->isEditingGlobalSettings())
	{
		{
			auto lock = Host::GetSettingsLock();
			result = PAD::MapController(*Host::Internal::GetBaseSettingsLayer(), m_port_number, mapping);
		}
		if (result)
			Host::CommitBaseSettingChanges();
	}
	else
	{
		result = PAD::MapController(*m_dialog->getProfileSettingsInterface(), m_port_number, mapping);
		if (result)
		{
			m_dialog->getProfileSettingsInterface()->Save();
			g_emu_thread->reloadInputBindings();
		}
	}

	// force a refresh after mapping
	if (result)
	{
		g_emu_thread->applySettings();
		onTypeChanged();
	}
}

//////////////////////////////////////////////////////////////////////////

ControllerMacroWidget::ControllerMacroWidget(ControllerBindingWidget* parent)
	: QWidget(parent)
{
	m_ui.setupUi(this);
	setWindowTitle(tr("Controller Port %1 Macros").arg(parent->getPortNumber() + 1u));
	createWidgets(parent);
}

ControllerMacroWidget::~ControllerMacroWidget() = default;

void ControllerMacroWidget::updateListItem(u32 index)
{
	//: This is the full text that appears in each option of the 16 available macros, and reads like this:\n\nMacro 1\nNot Configured/Buttons configured
	m_ui.portList->item(static_cast<int>(index))->setText(tr("Macro %1\n%2").arg(index + 1).arg(m_macros[index]->getSummary()));
}

void ControllerMacroWidget::createWidgets(ControllerBindingWidget* parent)
{
	for (u32 i = 0; i < NUM_MACROS; i++)
	{
		m_macros[i] = new ControllerMacroEditWidget(this, parent, i);
		m_ui.container->addWidget(m_macros[i]);

		QListWidgetItem* item = new QListWidgetItem();
		item->setIcon(QIcon::fromTheme(QStringLiteral("flashlight-line")));
		m_ui.portList->addItem(item);
		updateListItem(i);
	}

	m_ui.portList->setCurrentRow(0);
	m_ui.container->setCurrentIndex(0);

	connect(m_ui.portList, &QListWidget::currentRowChanged, m_ui.container, &QStackedWidget::setCurrentIndex);
}

//////////////////////////////////////////////////////////////////////////

ControllerMacroEditWidget::ControllerMacroEditWidget(ControllerMacroWidget* parent, ControllerBindingWidget* bwidget, u32 index)
	: QWidget(parent)
	, m_parent(parent)
	, m_bwidget(bwidget)
	, m_index(index)
{
	m_ui.setupUi(this);

	ControllerSettingsDialog* dialog = m_bwidget->getDialog();
	const std::string& section = m_bwidget->getConfigSection();
	const PAD::ControllerInfo* cinfo = PAD::GetControllerInfo(m_bwidget->getControllerType());
	if (!cinfo)
	{
		// Shouldn't ever happen.
		return;
	}

	// load binds (single string joined by &)
	const std::string binds_string(dialog->getStringValue(section.c_str(), fmt::format("Macro{}Binds", index + 1u).c_str(), ""));
	const std::vector<std::string_view> buttons_split(StringUtil::SplitString(binds_string, '&', true));

	for (const std::string_view& button : buttons_split)
	{
		for (u32 i = 0; i < cinfo->num_bindings; i++)
		{
			if (button == cinfo->bindings[i].name)
			{
				m_binds.push_back(&cinfo->bindings[i]);
				break;
			}
		}
	}

	// populate list view
	for (u32 i = 0; i < cinfo->num_bindings; i++)
	{
		const InputBindingInfo& bi = cinfo->bindings[i];
		if (bi.bind_type == InputBindingInfo::Type::Motor)
			continue;

		QListWidgetItem* item = new QListWidgetItem();
		item->setText(qApp->translate("Pad", bi.display_name));
		item->setCheckState((std::find(m_binds.begin(), m_binds.end(), &bi) != m_binds.end()) ? Qt::Checked : Qt::Unchecked);
		m_ui.bindList->addItem(item);
	}

	ControllerSettingWidgetBinder::BindWidgetToInputProfileNormalized(
		dialog->getProfileSettingsInterface(), m_ui.pressure, section, fmt::format("Macro{}Pressure", index + 1u), 100.0f, 1.0f);
	connect(m_ui.pressure, &QSlider::valueChanged, this, &ControllerMacroEditWidget::onPressureChanged);
	onPressureChanged();

	m_frequency = dialog->getIntValue(section.c_str(), fmt::format("Macro{}Frequency", index + 1u).c_str(), 0);
	updateFrequencyText();

	m_ui.trigger->initialize(
		dialog->getProfileSettingsInterface(), InputBindingInfo::Type::Macro, section, fmt::format("Macro{}", index + 1u));

	connect(m_ui.increaseFrequency, &QAbstractButton::clicked, this, [this]() { modFrequency(1); });
	connect(m_ui.decreateFrequency, &QAbstractButton::clicked, this, [this]() { modFrequency(-1); });
	connect(m_ui.setFrequency, &QAbstractButton::clicked, this, &ControllerMacroEditWidget::onSetFrequencyClicked);
	connect(m_ui.bindList, &QListWidget::itemChanged, this, &ControllerMacroEditWidget::updateBinds);
}

ControllerMacroEditWidget::~ControllerMacroEditWidget() = default;

QString ControllerMacroEditWidget::getSummary() const
{
	QString str;
	for (const InputBindingInfo* bi : m_binds)
	{
		if (!str.isEmpty())
			str += static_cast<QChar>('/');
		str += qApp->translate("Pad", bi->display_name);
	}
	return str.isEmpty() ? tr("Not Configured") : str;
}

void ControllerMacroEditWidget::onPressureChanged()
{
	m_ui.pressureValue->setText(tr("%1%").arg(m_ui.pressure->value()));
}

void ControllerMacroEditWidget::onSetFrequencyClicked()
{
	bool okay;
	int new_freq = QInputDialog::getInt(
		this, tr("Set Frequency"), tr("Frequency: "), static_cast<int>(m_frequency), 0, std::numeric_limits<int>::max(), 1, &okay);
	if (!okay)
		return;

	m_frequency = static_cast<u32>(new_freq);
	updateFrequency();
}

void ControllerMacroEditWidget::modFrequency(s32 delta)
{
	if (delta < 0 && m_frequency == 0)
		return;

	m_frequency = static_cast<u32>(static_cast<s32>(m_frequency) + delta);
	updateFrequency();
}

void ControllerMacroEditWidget::updateFrequency()
{
	m_bwidget->getDialog()->setIntValue(
		m_bwidget->getConfigSection().c_str(), fmt::format("Macro{}Frequency", m_index + 1u).c_str(), static_cast<s32>(m_frequency));
	updateFrequencyText();
}

void ControllerMacroEditWidget::updateFrequencyText()
{
	if (m_frequency == 0)
		m_ui.frequencyText->setText(tr("Macro will not repeat."));
	else
		m_ui.frequencyText->setText(tr("Macro will toggle buttons every %1 frames.").arg(m_frequency));
}

void ControllerMacroEditWidget::updateBinds()
{
	ControllerSettingsDialog* dialog = m_bwidget->getDialog();
	const PAD::ControllerInfo* cinfo = PAD::GetControllerInfo(m_bwidget->getControllerType());
	if (!cinfo)
		return;

	std::vector<const InputBindingInfo*> new_binds;
	for (u32 i = 0, bind_index = 0; i < cinfo->num_bindings; i++)
	{
		const InputBindingInfo& bi = cinfo->bindings[i];
		if (bi.bind_type == InputBindingInfo::Type::Motor)
			continue;

		const QListWidgetItem* item = m_ui.bindList->item(static_cast<int>(bind_index));
		bind_index++;

		if (!item)
		{
			// shouldn't happen
			continue;
		}

		if (item->checkState() == Qt::Checked)
			new_binds.push_back(&bi);
	}
	if (m_binds == new_binds)
		return;

	m_binds = std::move(new_binds);

	std::string binds_string;
	for (const InputBindingInfo* bi : m_binds)
	{
		if (!binds_string.empty())
			binds_string.append(" & ");
		binds_string.append(bi->name);
	}

	const std::string& section = m_bwidget->getConfigSection();
	const std::string key(fmt::format("Macro{}Binds", m_index + 1u));
	if (binds_string.empty())
		dialog->clearSettingValue(section.c_str(), key.c_str());
	else
		dialog->setStringValue(section.c_str(), key.c_str(), binds_string.c_str());

	m_parent->updateListItem(m_index);
}

//////////////////////////////////////////////////////////////////////////

ControllerCustomSettingsWidget::ControllerCustomSettingsWidget(gsl::span<const SettingInfo> settings, std::string config_section,
	std::string config_prefix, const char* translation_ctx, ControllerSettingsDialog* dialog, QWidget* parent_widget)
	: QWidget(parent_widget)
	, m_settings(settings)
	, m_config_section(std::move(config_section))
	, m_config_prefix(std::move(config_prefix))
	, m_dialog(dialog)
{
	if (settings.empty())
		return;

	QScrollArea* sarea = new QScrollArea(this);
	QWidget* swidget = new QWidget(sarea);
	sarea->setWidget(swidget);
	sarea->setWidgetResizable(true);
	sarea->setFrameShape(QFrame::StyledPanel);
	sarea->setFrameShadow(QFrame::Sunken);

	QGridLayout* swidget_layout = new QGridLayout(swidget);
	createSettingWidgets(translation_ctx, swidget, swidget_layout);

	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(sarea);
}

ControllerCustomSettingsWidget::~ControllerCustomSettingsWidget() = default;

static std::tuple<QString, QString> getPrefixAndSuffixForIntFormat(const QString& format)
{
	QString prefix, suffix;
	QRegularExpression re(QStringLiteral("(.*)%.*d(.*)"));
	QRegularExpressionMatch match(re.match(format));
	if (match.isValid())
	{
		prefix = match.captured(1).replace(QStringLiteral("%%"), QStringLiteral("%"));
		suffix = match.captured(2).replace(QStringLiteral("%%"), QStringLiteral("%"));
	}

	return std::tie(prefix, suffix);
}

static std::tuple<QString, QString, int> getPrefixAndSuffixForFloatFormat(const QString& format)
{
	QString prefix, suffix;
	int decimals = -1;

	QRegularExpression re(QStringLiteral("(.*)%.*([0-9]+)f(.*)"));
	QRegularExpressionMatch match(re.match(format));
	if (match.isValid())
	{
		prefix = match.captured(1).replace(QStringLiteral("%%"), QStringLiteral("%"));
		suffix = match.captured(3).replace(QStringLiteral("%%"), QStringLiteral("%"));

		bool decimals_ok;
		decimals = match.captured(2).toInt(&decimals_ok);
		if (!decimals_ok)
			decimals = -1;
	}
	else
	{
		re = QRegularExpression(QStringLiteral("(.*)%.*f(.*)"));
		match = re.match(format);
		prefix = match.captured(1).replace(QStringLiteral("%%"), QStringLiteral("%"));
		suffix = match.captured(2).replace(QStringLiteral("%%"), QStringLiteral("%"));
	}

	return std::tie(prefix, suffix, decimals);
}

void ControllerCustomSettingsWidget::createSettingWidgets(const char* translation_ctx, QWidget* widget_parent, QGridLayout* layout)
{
	SettingsInterface* sif = m_dialog->getProfileSettingsInterface();
	int current_row = 0;

	for (const SettingInfo& si : m_settings)
	{
		std::string key_name = m_config_prefix + si.name;

		switch (si.type)
		{
			case SettingInfo::Type::Boolean:
			{
				QCheckBox* cb = new QCheckBox(qApp->translate(translation_ctx, si.display_name), widget_parent);
				cb->setObjectName(QString::fromUtf8(si.name));
				ControllerSettingWidgetBinder::BindWidgetToInputProfileBool(
					sif, cb, m_config_section, std::move(key_name), si.BooleanDefaultValue());
				layout->addWidget(cb, current_row, 0, 1, 4);
				current_row++;
			}
			break;

			case SettingInfo::Type::Integer:
			{
				QSpinBox* sb = new QSpinBox(widget_parent);
				sb->setObjectName(QString::fromUtf8(si.name));
				sb->setMinimum(si.IntegerMinValue());
				sb->setMaximum(si.IntegerMaxValue());
				sb->setSingleStep(si.IntegerStepValue());
				if (si.format)
				{
					const auto [prefix, suffix] = getPrefixAndSuffixForIntFormat(QString::fromUtf8(si.format));
					sb->setPrefix(prefix);
					sb->setSuffix(suffix);
				}
				ControllerSettingWidgetBinder::BindWidgetToInputProfileInt(
					sif, sb, m_config_section, std::move(key_name), si.IntegerDefaultValue());
				layout->addWidget(new QLabel(qApp->translate(translation_ctx, si.display_name), widget_parent), current_row, 0);
				layout->addWidget(sb, current_row, 1, 1, 3);
				current_row++;
			}
			break;

			case SettingInfo::Type::IntegerList:
			{
				QComboBox* cb = new QComboBox(widget_parent);
				cb->setObjectName(QString::fromUtf8(si.name));
				for (u32 i = 0; si.options[i] != nullptr; i++)
					cb->addItem(qApp->translate(translation_ctx, si.options[i]));
				ControllerSettingWidgetBinder::BindWidgetToInputProfileInt(
					sif, cb, m_config_section, std::move(key_name), si.IntegerDefaultValue(), si.IntegerMinValue());
				layout->addWidget(new QLabel(qApp->translate(translation_ctx, si.display_name), widget_parent), current_row, 0);
				layout->addWidget(cb, current_row, 1, 1, 3);
				current_row++;
			}
			break;

			case SettingInfo::Type::Float:
			{
				QDoubleSpinBox* sb = new QDoubleSpinBox(widget_parent);
				sb->setObjectName(QString::fromUtf8(si.name));
				sb->setMinimum(si.FloatMinValue() * si.multiplier);
				sb->setMaximum(si.FloatMaxValue() * si.multiplier);
				sb->setSingleStep(si.FloatStepValue() * si.multiplier);

				if (si.format)
				{
					const auto [prefix, suffix, decimals] = getPrefixAndSuffixForFloatFormat(QString::fromUtf8(si.format));
					sb->setPrefix(prefix);
					if (decimals >= 0)
						sb->setDecimals(decimals);
					sb->setSuffix(suffix);
				}

				ControllerSettingWidgetBinder::BindWidgetToInputProfileFloat(
					sif, sb, m_config_section, std::move(key_name), si.FloatDefaultValue(), si.multiplier);
				layout->addWidget(new QLabel(qApp->translate(translation_ctx, si.display_name), widget_parent), current_row, 0);
				layout->addWidget(sb, current_row, 1, 1, 3);
				current_row++;
			}
			break;

			case SettingInfo::Type::String:
			{
				QLineEdit* le = new QLineEdit(widget_parent);
				le->setObjectName(QString::fromUtf8(si.name));
				ControllerSettingWidgetBinder::BindWidgetToInputProfileString(
					sif, le, m_config_section, std::move(key_name), si.StringDefaultValue());
				layout->addWidget(new QLabel(qApp->translate(translation_ctx, si.display_name), widget_parent), current_row, 0);
				layout->addWidget(le, current_row, 1, 1, 3);
				current_row++;
			}
			break;

			case SettingInfo::Type::StringList:
			{
				QComboBox* cb = new QComboBox(widget_parent);
				cb->setObjectName(QString::fromUtf8(si.name));
				if (si.get_options)
				{
					std::vector<std::pair<std::string, std::string>> options(si.get_options());
					for (const auto& [name, display_name] : options)
						cb->addItem(QString::fromStdString(display_name), QString::fromStdString(name));
				}
				else if (si.options)
				{
					for (u32 i = 0; si.options[i] != nullptr; i++)
						cb->addItem(qApp->translate(translation_ctx, si.options[i]), QString::fromUtf8(si.options[i]));
				}
				ControllerSettingWidgetBinder::BindWidgetToInputProfileString(
					sif, cb, m_config_section, std::move(key_name), si.StringDefaultValue());
				layout->addWidget(new QLabel(qApp->translate(translation_ctx, si.display_name), widget_parent), current_row, 0);
				layout->addWidget(cb, current_row, 1, 1, 3);
				current_row++;
			}
			break;

			case SettingInfo::Type::Path:
			{
				QLineEdit* le = new QLineEdit(widget_parent);
				le->setObjectName(QString::fromUtf8(si.name));
				QPushButton* browse_button = new QPushButton(tr("Browse..."), widget_parent);
				ControllerSettingWidgetBinder::BindWidgetToInputProfileString(
					sif, le, m_config_section, std::move(key_name), si.StringDefaultValue());
				connect(browse_button, &QPushButton::clicked, [this, le]() {
					const QString path(QDir::toNativeSeparators(QFileDialog::getOpenFileName(this, tr("Select File"))));
					if (!path.isEmpty())
						le->setText(path);
				});

				QHBoxLayout* hbox = new QHBoxLayout();
				hbox->addWidget(le, 1);
				hbox->addWidget(browse_button);

				layout->addWidget(new QLabel(qApp->translate(translation_ctx, si.display_name), widget_parent), current_row, 0);
				layout->addLayout(hbox, current_row, 1, 1, 3);
				current_row++;
			}
			break;
		}

		QLabel* label = new QLabel(si.description ? qApp->translate(translation_ctx, si.description) : QString(), widget_parent);
		label->setWordWrap(true);
		layout->addWidget(label, current_row++, 0, 1, 4);

		layout->addItem(new QSpacerItem(1, 10, QSizePolicy::Minimum, QSizePolicy::Fixed), current_row++, 0, 1, 4);
	}

	QHBoxLayout* bottom_hlayout = new QHBoxLayout();
	QPushButton* restore_defaults = new QPushButton(tr("Restore Default Settings"), this);
	restore_defaults->setIcon(QIcon::fromTheme(QStringLiteral("restart-line")));
	connect(restore_defaults, &QPushButton::clicked, this, &ControllerCustomSettingsWidget::restoreDefaults);
	bottom_hlayout->addStretch(1);
	bottom_hlayout->addWidget(restore_defaults);
	layout->addLayout(bottom_hlayout, current_row++, 0, 1, 4);

	layout->addItem(new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::Expanding), current_row++, 0, 1, 4);
}

void ControllerCustomSettingsWidget::restoreDefaults()
{
	for (const SettingInfo& si : m_settings)
	{
		const QString key(QString::fromStdString(si.name));

		switch (si.type)
		{
			case SettingInfo::Type::Boolean:
			{
				QCheckBox* widget = findChild<QCheckBox*>(QString::fromStdString(si.name));
				if (widget)
					widget->setChecked(si.BooleanDefaultValue());
			}
			break;

			case SettingInfo::Type::Integer:
			{
				QSpinBox* widget = findChild<QSpinBox*>(QString::fromStdString(si.name));
				if (widget)
					widget->setValue(si.IntegerDefaultValue());
			}
			break;

			case SettingInfo::Type::IntegerList:
			{
				QComboBox* widget = findChild<QComboBox*>(QString::fromStdString(si.name));
				if (widget)
					widget->setCurrentIndex(si.IntegerDefaultValue() - si.IntegerMinValue());
			}
			break;

			case SettingInfo::Type::Float:
			{
				QDoubleSpinBox* widget = findChild<QDoubleSpinBox*>(QString::fromStdString(si.name));
				if (widget)
					widget->setValue(si.FloatDefaultValue() * si.multiplier);
			}
			break;

			case SettingInfo::Type::String:
			{
				QLineEdit* widget = findChild<QLineEdit*>(QString::fromStdString(si.name));
				if (widget)
					widget->setText(QString::fromUtf8(si.StringDefaultValue()));
			}
			break;

			case SettingInfo::Type::StringList:
			{
				QComboBox* widget = findChild<QComboBox*>(QString::fromStdString(si.name));
				if (widget)
				{
					const QString default_value(QString::fromUtf8(si.StringDefaultValue()));
					int index = widget->findData(default_value);
					if (index < 0)
						index = widget->findText(default_value);
					if (index >= 0)
						widget->setCurrentIndex(index);
				}
			}
			break;

			case SettingInfo::Type::Path:
			{
				QLineEdit* widget = findChild<QLineEdit*>(QString::fromStdString(si.name));
				if (widget)
					widget->setText(QString::fromUtf8(si.StringDefaultValue()));
			}
			break;
		}
	}
}


//////////////////////////////////////////////////////////////////////////

ControllerBindingWidget_Base::ControllerBindingWidget_Base(ControllerBindingWidget* parent)
	: QWidget(parent)
{
}

ControllerBindingWidget_Base::~ControllerBindingWidget_Base()
{
}

QIcon ControllerBindingWidget_Base::getIcon() const
{
	return QIcon::fromTheme("controller-strike-line");
}

void ControllerBindingWidget_Base::initBindingWidgets()
{
	const PAD::ControllerInfo* cinfo = PAD::GetControllerInfo(getControllerType());
	if (!cinfo)
		return;

	const std::string& config_section = getConfigSection();
	SettingsInterface* sif = getDialog()->getProfileSettingsInterface();

	for (u32 i = 0; i < cinfo->num_bindings; i++)
	{
		const InputBindingInfo& bi = cinfo->bindings[i];
		if (bi.bind_type == InputBindingInfo::Type::Axis || bi.bind_type == InputBindingInfo::Type::HalfAxis ||
			bi.bind_type == InputBindingInfo::Type::Button || bi.bind_type == InputBindingInfo::Type::Pointer ||
			bi.bind_type == InputBindingInfo::Type::Device)
		{
			InputBindingWidget* widget = findChild<InputBindingWidget*>(QString::fromStdString(bi.name));
			if (!widget)
			{
				Console.Error("(ControllerBindingWidget_Base) No widget found for '%s' (%s)", bi.name, cinfo->name);
				continue;
			}

			widget->initialize(sif, bi.bind_type, config_section, bi.name);
		}
	}

	switch (cinfo->vibration_caps)
	{
		case PAD::VibrationCapabilities::LargeSmallMotors:
		{
			InputVibrationBindingWidget* widget = findChild<InputVibrationBindingWidget*>(QStringLiteral("LargeMotor"));
			if (widget)
				widget->setKey(getDialog(), config_section, "LargeMotor");

			widget = findChild<InputVibrationBindingWidget*>(QStringLiteral("SmallMotor"));
			if (widget)
				widget->setKey(getDialog(), config_section, "SmallMotor");
		}
		break;

		case PAD::VibrationCapabilities::SingleMotor:
		{
			InputVibrationBindingWidget* widget = findChild<InputVibrationBindingWidget*>(QStringLiteral("Motor"));
			if (widget)
				widget->setKey(getDialog(), config_section, "Motor");
		}
		break;

		case PAD::VibrationCapabilities::NoVibration:
		default:
			break;
	}
}

ControllerBindingWidget_DualShock2::ControllerBindingWidget_DualShock2(ControllerBindingWidget* parent)
	: ControllerBindingWidget_Base(parent)
{
	m_ui.setupUi(this);
	initBindingWidgets();
}

ControllerBindingWidget_DualShock2::~ControllerBindingWidget_DualShock2()
{
}

QIcon ControllerBindingWidget_DualShock2::getIcon() const
{
	return QIcon::fromTheme("controller-line");
}

ControllerBindingWidget_Base* ControllerBindingWidget_DualShock2::createInstance(ControllerBindingWidget* parent)
{
	return new ControllerBindingWidget_DualShock2(parent);
}

//////////////////////////////////////////////////////////////////////////

USBDeviceWidget::USBDeviceWidget(QWidget* parent, ControllerSettingsDialog* dialog, u32 port)
	: QWidget(parent)
	, m_dialog(dialog)
	, m_config_section(fmt::format("USB{}", port + 1))
	, m_port_number(port)
{
	m_ui.setupUi(this);
	m_ui.groupBox->setTitle(tr("USB Port %1").arg(port + 1));

	populateDeviceTypes();
	populatePages();

	ControllerSettingWidgetBinder::BindWidgetToInputProfileString(
		m_dialog->getProfileSettingsInterface(), m_ui.deviceType, m_config_section, "Type", "None");

	connect(m_ui.deviceType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &USBDeviceWidget::onTypeChanged);
	connect(m_ui.deviceSubtype, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &USBDeviceWidget::onSubTypeChanged);
	connect(m_ui.bindings, &QPushButton::clicked, this, &USBDeviceWidget::onBindingsClicked);
	connect(m_ui.settings, &QPushButton::clicked, this, &USBDeviceWidget::onSettingsClicked);
	connect(m_ui.automaticBinding, &QPushButton::clicked, this, &USBDeviceWidget::onAutomaticBindingClicked);
	connect(m_ui.clearBindings, &QPushButton::clicked, this, &USBDeviceWidget::onClearBindingsClicked);
}

USBDeviceWidget::~USBDeviceWidget() = default;

QIcon USBDeviceWidget::getIcon() const
{
	return QIcon::fromTheme("usb-fill");
}

void USBDeviceWidget::populateDeviceTypes()
{
	for (const auto& [name, display_name] : USB::GetDeviceTypes())
		m_ui.deviceType->addItem(qApp->translate("USB", display_name), QString::fromUtf8(name));
}

void USBDeviceWidget::populatePages()
{
	m_device_type = m_dialog->getStringValue(m_config_section.c_str(), "Type", "None");
	m_device_subtype = m_dialog->getIntValue(m_config_section.c_str(), fmt::format("{}_subtype", m_device_type).c_str(), 0);

	{
		QSignalBlocker sb(m_ui.deviceSubtype);
		m_ui.deviceSubtype->clear();
		for (const char* subtype : USB::GetDeviceSubtypes(m_device_type))
			m_ui.deviceSubtype->addItem(qApp->translate("USB", subtype));
		m_ui.deviceSubtype->setCurrentIndex(m_device_subtype);
		m_ui.deviceSubtype->setVisible(m_ui.deviceSubtype->count() > 0);
	}

	if (m_bindings_widget)
	{
		m_ui.stackedWidget->removeWidget(m_bindings_widget);
		delete m_bindings_widget;
		m_bindings_widget = nullptr;
	}
	if (m_settings_widget)
	{
		m_ui.stackedWidget->removeWidget(m_settings_widget);
		delete m_settings_widget;
		m_settings_widget = nullptr;
	}

	const gsl::span<const InputBindingInfo> bindings(USB::GetDeviceBindings(m_device_type, m_device_subtype));
	const gsl::span<const SettingInfo> settings(USB::GetDeviceSettings(m_device_type, m_device_subtype));
	m_ui.bindings->setEnabled(!bindings.empty());
	m_ui.settings->setEnabled(!settings.empty());

	if (!bindings.empty())
	{
		m_bindings_widget = USBBindingWidget::createInstance(m_device_type, m_device_subtype, bindings, this);
		if (m_bindings_widget)
		{
			m_ui.stackedWidget->addWidget(m_bindings_widget);
			m_ui.stackedWidget->setCurrentWidget(m_bindings_widget);
		}
	}

	if (!settings.empty())
	{
		m_settings_widget = new ControllerCustomSettingsWidget(
			settings, m_config_section, m_device_type + "_", "USB", m_dialog, m_ui.stackedWidget);
		m_ui.stackedWidget->addWidget(m_settings_widget);
	}

	updateHeaderToolButtons();
}

void USBDeviceWidget::onTypeChanged()
{
	populatePages();
	m_dialog->updateListDescription(m_port_number, this);
}

void USBDeviceWidget::onSubTypeChanged(int new_index)
{
	m_dialog->setIntValue(m_config_section.c_str(), fmt::format("{}_subtype", m_device_type).c_str(), new_index);
	onTypeChanged();
}

void USBDeviceWidget::updateHeaderToolButtons()
{
	const QWidget* current_widget = m_ui.stackedWidget->currentWidget();
	const QSignalBlocker bindings_sb(m_ui.bindings);
	const QSignalBlocker settings_sb(m_ui.settings);

	const bool is_bindings = (m_bindings_widget && current_widget == m_bindings_widget);
	const bool is_settings = (m_settings_widget && current_widget == m_settings_widget);
	m_ui.bindings->setChecked(is_bindings);
	m_ui.automaticBinding->setEnabled(is_bindings);
	m_ui.clearBindings->setEnabled(is_bindings);
	m_ui.settings->setChecked(is_settings);
}

void USBDeviceWidget::onBindingsClicked()
{
	if (!m_bindings_widget)
		return;

	m_ui.stackedWidget->setCurrentWidget(m_bindings_widget);
	updateHeaderToolButtons();
}

void USBDeviceWidget::onSettingsClicked()
{
	if (!m_settings_widget)
		return;

	m_ui.stackedWidget->setCurrentWidget(m_settings_widget);
	updateHeaderToolButtons();
}

void USBDeviceWidget::onAutomaticBindingClicked()
{
	QMenu menu(this);
	bool added = false;

	for (const QPair<QString, QString>& dev : m_dialog->getDeviceList())
	{
		// we set it as data, because the device list could get invalidated while the menu is up
		QAction* action = menu.addAction(QStringLiteral("%1 (%2)").arg(dev.first).arg(dev.second));
		action->setData(dev.first);
		connect(action, &QAction::triggered, this, [this, action]() { doDeviceAutomaticBinding(action->data().toString()); });
		added = true;
	}

	if (!added)
	{
		QAction* action = menu.addAction(tr("No devices available"));
		action->setEnabled(false);
	}

	menu.exec(QCursor::pos());
}

void USBDeviceWidget::onClearBindingsClicked()
{
	if (QMessageBox::question(QtUtils::GetRootWidget(this), tr("Clear Bindings"),
			tr("Are you sure you want to clear all bindings for this controller? This action cannot be undone.")) != QMessageBox::Yes)
	{
		return;
	}

	if (m_dialog->isEditingGlobalSettings())
	{
		{
			auto lock = Host::GetSettingsLock();
			USB::ClearPortBindings(*Host::Internal::GetBaseSettingsLayer(), m_port_number);
		}
		Host::CommitBaseSettingChanges();
	}
	else
	{
		USB::ClearPortBindings(*m_dialog->getProfileSettingsInterface(), m_port_number);
		m_dialog->getProfileSettingsInterface()->Save();
	}

	// force a refresh after clearing
	g_emu_thread->applySettings();
	onTypeChanged();
}

void USBDeviceWidget::doDeviceAutomaticBinding(const QString& device)
{
	std::vector<std::pair<GenericInputBinding, std::string>> mapping = InputManager::GetGenericBindingMapping(device.toStdString());
	if (mapping.empty())
	{
		QMessageBox::critical(QtUtils::GetRootWidget(this), tr("Automatic Binding"),
			tr("No generic bindings were generated for device '%1'. The controller/source may not support automatic mapping.").arg(device));
		return;
	}

	bool result;
	if (m_dialog->isEditingGlobalSettings())
	{
		{
			auto lock = Host::GetSettingsLock();
			result = USB::MapDevice(*Host::Internal::GetBaseSettingsLayer(), m_port_number, mapping);
		}
		if (result)
			Host::CommitBaseSettingChanges();
	}
	else
	{
		result = USB::MapDevice(*m_dialog->getProfileSettingsInterface(), m_port_number, mapping);
		if (result)
		{
			m_dialog->getProfileSettingsInterface()->Save();
			g_emu_thread->reloadInputBindings();
		}
	}

	// force a refresh after mapping
	if (result)
	{
		g_emu_thread->applySettings();
		onTypeChanged();
	}
}

//////////////////////////////////////////////////////////////////////////

USBBindingWidget::USBBindingWidget(USBDeviceWidget* parent)
	: QWidget(parent)
{
}

USBBindingWidget::~USBBindingWidget()
{
}

QIcon USBBindingWidget::getIcon() const
{
	return QIcon::fromTheme("controller-strike-line");
}

std::string USBBindingWidget::getBindingKey(const char* binding_name) const
{
	return USB::GetConfigSubKey(getDeviceType(), binding_name);
}

void USBBindingWidget::createWidgets(gsl::span<const InputBindingInfo> bindings)
{
	QGroupBox* axis_gbox = nullptr;
	QGridLayout* axis_layout = nullptr;
	QGroupBox* button_gbox = nullptr;
	QGridLayout* button_layout = nullptr;
	SettingsInterface* sif = getDialog()->getProfileSettingsInterface();

	QScrollArea* scrollarea = new QScrollArea(this);
	QWidget* scrollarea_widget = new QWidget(scrollarea);
	scrollarea->setWidget(scrollarea_widget);
	scrollarea->setWidgetResizable(true);
	scrollarea->setFrameShape(QFrame::StyledPanel);
	scrollarea->setFrameShadow(QFrame::Plain);

	// We do axes and buttons separately, so we can figure out how many columns to use.
	constexpr int NUM_AXIS_COLUMNS = 2;
	int column = 0;
	int row = 0;
	for (const InputBindingInfo& bi : bindings)
	{
		if (bi.bind_type == InputBindingInfo::Type::Axis || bi.bind_type == InputBindingInfo::Type::HalfAxis ||
			bi.bind_type == InputBindingInfo::Type::Pointer || bi.bind_type == InputBindingInfo::Type::Device)
		{
			if (!axis_gbox)
			{
				axis_gbox = new QGroupBox(tr("Axes"), scrollarea_widget);
				axis_layout = new QGridLayout(axis_gbox);
			}

			QGroupBox* gbox = new QGroupBox(qApp->translate("USB", bi.display_name), axis_gbox);
			QVBoxLayout* temp = new QVBoxLayout(gbox);
			InputBindingWidget* widget = new InputBindingWidget(gbox, sif, bi.bind_type, getConfigSection(), getBindingKey(bi.name));
			temp->addWidget(widget);
			axis_layout->addWidget(gbox, row, column);
			if ((++column) == NUM_AXIS_COLUMNS)
			{
				column = 0;
				row++;
			}
		}
	}
	if (axis_gbox)
		axis_layout->addItem(new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::Expanding), ++row, 0);

	const int num_button_columns = axis_layout ? 2 : 4;
	row = 0;
	column = 0;
	for (const InputBindingInfo& bi : bindings)
	{
		if (bi.bind_type == InputBindingInfo::Type::Button)
		{
			if (!button_gbox)
			{
				button_gbox = new QGroupBox(tr("Buttons"), scrollarea_widget);
				button_layout = new QGridLayout(button_gbox);
			}

			QGroupBox* gbox = new QGroupBox(qApp->translate("USB", bi.display_name), button_gbox);
			QVBoxLayout* temp = new QVBoxLayout(gbox);
			InputBindingWidget* widget = new InputBindingWidget(gbox, sif, bi.bind_type, getConfigSection(), getBindingKey(bi.name));
			temp->addWidget(widget);
			button_layout->addWidget(gbox, row, column);
			if ((++column) == num_button_columns)
			{
				column = 0;
				row++;
			}
		}
	}

	if (button_gbox)
		button_layout->addItem(new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::Expanding), ++row, 0);

	if (!axis_gbox && !button_gbox)
		return;

	QHBoxLayout* layout = new QHBoxLayout(scrollarea_widget);
	if (axis_gbox)
		layout->addWidget(axis_gbox);
	if (button_gbox)
		layout->addWidget(button_gbox);
	layout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum));

	QHBoxLayout* main_layout = new QHBoxLayout(this);
	main_layout->addWidget(scrollarea);
}


void USBBindingWidget::bindWidgets(gsl::span<const InputBindingInfo> bindings)
{
	SettingsInterface* sif = getDialog()->getProfileSettingsInterface();

	for (const InputBindingInfo& bi : bindings)
	{
		if (bi.bind_type == InputBindingInfo::Type::Axis || bi.bind_type == InputBindingInfo::Type::HalfAxis ||
			bi.bind_type == InputBindingInfo::Type::Button || bi.bind_type == InputBindingInfo::Type::Pointer ||
			bi.bind_type == InputBindingInfo::Type::Device)
		{
			InputBindingWidget* widget = findChild<InputBindingWidget*>(QString::fromUtf8(bi.name));
			if (!widget)
			{
				Console.Error("(USBBindingWidget) No widget found for '%s'.", bi.name);
				continue;
			}

			widget->initialize(sif, bi.bind_type, getConfigSection(), getBindingKey(bi.name));
		}
	}
}

USBBindingWidget* USBBindingWidget::createInstance(
	const std::string& type, u32 subtype, gsl::span<const InputBindingInfo> bindings, USBDeviceWidget* parent)
{
	USBBindingWidget* widget = new USBBindingWidget(parent);
	bool has_template = false;

	if (type == "Pad")
	{
		if (subtype == 0) // Generic or Driving Force
		{
			Ui::USBBindingWidget_DrivingForce().setupUi(widget);
			has_template = true;
		}
		else if (subtype == 3) // GT Force
		{
			Ui::USBBindingWidget_GTForce().setupUi(widget);
			has_template = true;
		}
	}

	if (has_template)
		widget->bindWidgets(bindings);
	else
		widget->createWidgets(bindings);

	return widget;
}
