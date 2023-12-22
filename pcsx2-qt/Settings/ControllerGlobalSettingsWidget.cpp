// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Settings/ControllerGlobalSettingsWidget.h"
#include "Settings/ControllerSettingsWindow.h"
#include "Settings/ControllerSettingWidgetBinder.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"

#include "pcsx2/Input/InputManager.h"
#include "pcsx2/Input/SDLInputSource.h"

ControllerGlobalSettingsWidget::ControllerGlobalSettingsWidget(QWidget* parent, ControllerSettingsWindow* dialog)
	: QWidget(parent)
	, m_dialog(dialog)
{
	m_ui.setupUi(this);

	SettingsInterface* sif = dialog->getProfileSettingsInterface();

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.enableSDLSource, "InputSources", "SDL", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.enableSDLEnhancedMode, "InputSources", "SDLControllerEnhancedMode", false);
	connect(m_ui.enableSDLSource, &QCheckBox::stateChanged, this, &ControllerGlobalSettingsWidget::updateSDLOptionsEnabled);
	connect(m_ui.ledSettings, &QToolButton::clicked, this, &ControllerGlobalSettingsWidget::ledSettingsClicked);

#ifdef _WIN32
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.enableSDLRawInput, "InputSources", "SDLRawInput", false);
#else
	m_ui.gridLayout_2->removeWidget(m_ui.enableSDLRawInput);
	m_ui.enableSDLRawInput->deleteLater();
	m_ui.enableSDLRawInput = nullptr;
#endif

	ControllerSettingWidgetBinder::BindWidgetToInputProfileBool(sif, m_ui.enableMouseMapping, "UI", "EnableMouseMapping", false);
	connect(m_ui.mouseSettings, &QToolButton::clicked, this, &ControllerGlobalSettingsWidget::mouseSettingsClicked);

	ControllerSettingWidgetBinder::BindWidgetToInputProfileBool(sif, m_ui.multitapPort1, "Pad", "MultitapPort1", false);
	ControllerSettingWidgetBinder::BindWidgetToInputProfileBool(sif, m_ui.multitapPort2, "Pad", "MultitapPort2", false);

#ifdef _WIN32
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.enableXInputSource, "InputSources", "XInput", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.enableDInputSource, "InputSources", "DInput", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.ignoreDInputInversion, "InputSources", "IgnoreDInputInversion", false);
#else
	m_ui.mainLayout->removeWidget(m_ui.xinputGroup);
	m_ui.xinputGroup->deleteLater();
	m_ui.xinputGroup = nullptr;
	m_ui.mainLayout->removeWidget(m_ui.dinputGroup);
	m_ui.dinputGroup->deleteLater();
	m_ui.dinputGroup = nullptr;
#endif

	if (dialog->isEditingProfile())
	{
		m_ui.useProfileHotkeyBindings->setChecked(m_dialog->getBoolValue("Pad", "UseProfileHotkeyBindings", false));
		connect(m_ui.useProfileHotkeyBindings, &QCheckBox::stateChanged, this, [this](int new_state) {
			m_dialog->setBoolValue("Pad", "UseProfileHotkeyBindings", (new_state == Qt::Checked));
			emit bindingSetupChanged();
		});
	}
	else
	{
		// remove profile options from the UI.
		m_ui.mainLayout->removeWidget(m_ui.profileSettings);
		m_ui.profileSettings->deleteLater();
		m_ui.profileSettings = nullptr;
	}

	for (QCheckBox* cb : {m_ui.multitapPort1, m_ui.multitapPort2})
		connect(cb, &QCheckBox::stateChanged, this, [this]() { emit bindingSetupChanged(); });

	updateSDLOptionsEnabled();
}

ControllerGlobalSettingsWidget::~ControllerGlobalSettingsWidget() = default;

void ControllerGlobalSettingsWidget::addDeviceToList(const QString& identifier, const QString& name)
{
	QListWidgetItem* item = new QListWidgetItem();
	item->setText(QStringLiteral("%1: %2").arg(identifier).arg(name));
	item->setData(Qt::UserRole, identifier);
	m_ui.deviceList->addItem(item);
}

void ControllerGlobalSettingsWidget::removeDeviceFromList(const QString& identifier)
{
	const int count = m_ui.deviceList->count();
	for (int i = 0; i < count; i++)
	{
		QListWidgetItem* item = m_ui.deviceList->item(i);
		if (item->data(Qt::UserRole) != identifier)
			continue;

		delete m_ui.deviceList->takeItem(i);
		break;
	}
}

void ControllerGlobalSettingsWidget::updateSDLOptionsEnabled()
{
	const bool enabled = m_ui.enableSDLSource->isChecked();
	m_ui.enableSDLEnhancedMode->setEnabled(enabled);
	m_ui.ledSettings->setEnabled(enabled);
#ifdef _WIN32
	m_ui.enableSDLRawInput->setEnabled(enabled);
#endif
}

void ControllerGlobalSettingsWidget::ledSettingsClicked()
{
	ControllerLEDSettingsDialog dialog(this, m_dialog);
	dialog.exec();
}

void ControllerGlobalSettingsWidget::mouseSettingsClicked()
{
	ControllerMouseSettingsDialog dialog(this, m_dialog);
	dialog.exec();
}

ControllerLEDSettingsDialog::ControllerLEDSettingsDialog(QWidget* parent, ControllerSettingsWindow* dialog)
	: QDialog(parent)
	, m_dialog(dialog)
{
	m_ui.setupUi(this);

	linkButton(m_ui.SDL0LED, 0);
	linkButton(m_ui.SDL1LED, 1);
	linkButton(m_ui.SDL2LED, 2);
	linkButton(m_ui.SDL3LED, 3);

	connect(m_ui.buttonBox->button(QDialogButtonBox::Close), &QPushButton::clicked, this, &QDialog::accept);
}

ControllerLEDSettingsDialog::~ControllerLEDSettingsDialog() = default;

void ControllerLEDSettingsDialog::linkButton(ColorPickerButton* button, u32 player_id)
{
	std::string key(fmt::format("Player{}LED", player_id));
	const u32 current_value = SDLInputSource::ParseRGBForPlayerId(m_dialog->getStringValue("SDLExtra", key.c_str(), ""), player_id);
	button->setColor(current_value);

	connect(button, &ColorPickerButton::colorChanged, this, [this, key = std::move(key)](u32 new_rgb) {
		m_dialog->setStringValue("SDLExtra", key.c_str(), fmt::format("{:06X}", new_rgb).c_str());
	});
}

ControllerMouseSettingsDialog::ControllerMouseSettingsDialog(QWidget* parent, ControllerSettingsWindow* dialog)
	: QDialog(parent)
{
	m_ui.setupUi(this);

	SettingsInterface* sif = dialog->getProfileSettingsInterface();

	m_ui.icon->setPixmap(QIcon::fromTheme(QStringLiteral("mouse-line")).pixmap(32, 32));

	ControllerSettingWidgetBinder::BindWidgetToInputProfileFloat(sif, m_ui.pointerXSpeedSlider, "Pad", "PointerXSpeed", 40.0f);
	ControllerSettingWidgetBinder::BindWidgetToInputProfileFloat(sif, m_ui.pointerYSpeedSlider, "Pad", "PointerYSpeed", 40.0f);
	ControllerSettingWidgetBinder::BindWidgetToInputProfileFloat(sif, m_ui.pointerXDeadZoneSlider, "Pad", "PointerXDeadZone", 20.0f);
	ControllerSettingWidgetBinder::BindWidgetToInputProfileFloat(sif, m_ui.pointerYDeadZoneSlider, "Pad", "PointerYDeadZone", 20.0f);
	ControllerSettingWidgetBinder::BindWidgetToInputProfileFloat(sif, m_ui.pointerInertiaSlider, "Pad", "PointerInertia", 10.0f);

	connect(m_ui.pointerXSpeedSlider, &QSlider::valueChanged, this, [this](int value) { m_ui.pointerXSpeedVal->setText(QStringLiteral("%1").arg(value)); });
	connect(m_ui.pointerYSpeedSlider, &QSlider::valueChanged, this, [this](int value) { m_ui.pointerYSpeedVal->setText(QStringLiteral("%1").arg(value)); });
	connect(m_ui.pointerXDeadZoneSlider, &QSlider::valueChanged, this, [this](int value) { m_ui.pointerXDeadZoneVal->setText(QStringLiteral("%1").arg(value)); });
	connect(m_ui.pointerYDeadZoneSlider, &QSlider::valueChanged, this, [this](int value) { m_ui.pointerYDeadZoneVal->setText(QStringLiteral("%1").arg(value)); });
	connect(m_ui.pointerInertiaSlider, &QSlider::valueChanged, this, [this](int value) { m_ui.pointerInertiaVal->setText(QStringLiteral("%1").arg(value)); });

	m_ui.pointerXSpeedVal->setText(QStringLiteral("%1").arg(m_ui.pointerXSpeedSlider->value()));
	m_ui.pointerYSpeedVal->setText(QStringLiteral("%1").arg(m_ui.pointerYSpeedSlider->value()));
	m_ui.pointerXDeadZoneVal->setText(QStringLiteral("%1").arg(m_ui.pointerXDeadZoneSlider->value()));
	m_ui.pointerYDeadZoneVal->setText(QStringLiteral("%1").arg(m_ui.pointerYDeadZoneSlider->value()));
	m_ui.pointerInertiaVal->setText(QStringLiteral("%1").arg(m_ui.pointerInertiaSlider->value()));

	connect(m_ui.buttonBox->button(QDialogButtonBox::Close), &QPushButton::clicked, this, &QDialog::accept);
}

ControllerMouseSettingsDialog::~ControllerMouseSettingsDialog() = default;
