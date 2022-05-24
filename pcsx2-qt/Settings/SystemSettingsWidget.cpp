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
#include <algorithm>

#include "pcsx2/HostSettings.h"

#include "EmuThread.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"
#include "SettingsDialog.h"
#include "SystemSettingsWidget.h"

static constexpr int MINIMUM_EE_CYCLE_RATE = -3;
static constexpr int MAXIMUM_EE_CYCLE_RATE = 3;
static constexpr int DEFAULT_EE_CYCLE_RATE = 0;
static constexpr int DEFAULT_EE_CYCLE_SKIP = 0;

SystemSettingsWidget::SystemSettingsWidget(SettingsDialog* dialog, QWidget* parent)
	: QWidget(parent)
	, m_dialog(dialog)
{
	SettingsInterface* sif = dialog->getSettingsInterface();

	m_ui.setupUi(this);

	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.eeCycleSkipping, "EmuCore/Speedhacks", "EECycleSkip", DEFAULT_EE_CYCLE_SKIP);

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.MTVU, "EmuCore/Speedhacks", "vuThread", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.instantVU1, "EmuCore/Speedhacks", "vu1Instant", true);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.eeRoundingMode, "EmuCore/CPU", "FPU.Roundmode", 3);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.vuRoundingMode, "EmuCore/CPU", "VU.Roundmode", 3);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.fastCDVD, "EmuCore/Speedhacks", "fastCDVD", false);

	if (m_dialog->isPerGameSettings())
	{
		m_ui.eeCycleRate->insertItem(
			0, tr("Use Global Setting [%1]")
				   .arg(m_ui.eeCycleRate->itemText(
					   std::clamp(Host::GetBaseIntSettingValue("EmuCore/Speedhacks", "EECycleRate", DEFAULT_EE_CYCLE_RATE) - MINIMUM_EE_CYCLE_RATE,
						   0, MAXIMUM_EE_CYCLE_RATE - MINIMUM_EE_CYCLE_RATE))));
		m_ui.eeClampMode->insertItem(0, tr("Use Global Setting [%1]").arg(m_ui.eeClampMode->itemText(getGlobalClampingModeIndex(false))));
		m_ui.vuClampMode->insertItem(0, tr("Use Global Setting [%1]").arg(m_ui.vuClampMode->itemText(getGlobalClampingModeIndex(true))));
	}

	const std::optional<int> cycle_rate =
		m_dialog->getIntValue("EmuCore/Speedhacks", "EECycleRate", sif ? std::nullopt : std::optional<int>(DEFAULT_EE_CYCLE_RATE));
	m_ui.eeCycleRate->setCurrentIndex(cycle_rate.has_value() ? (std::clamp(cycle_rate.value(), MINIMUM_EE_CYCLE_RATE, MAXIMUM_EE_CYCLE_RATE) +
																   (0 - MINIMUM_EE_CYCLE_RATE) + static_cast<int>(m_dialog->isPerGameSettings())) :
                                                               0);
	connect(m_ui.eeCycleRate, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
		std::optional<int> value;
		if (!m_dialog->isPerGameSettings() || index > 0)
			value = MINIMUM_EE_CYCLE_RATE + index - static_cast<int>(m_dialog->isPerGameSettings());
		m_dialog->setIntSettingValue("EmuCore/Speedhacks", "EECycleRate", value);
	});

	m_ui.eeClampMode->setCurrentIndex(getClampingModeIndex(false));
	m_ui.vuClampMode->setCurrentIndex(getClampingModeIndex(true));
	connect(m_ui.eeClampMode, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) { setClampingMode(false, index); });
	connect(m_ui.vuClampMode, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) { setClampingMode(true, index); });

	updateVU1InstantState();
	connect(m_ui.MTVU, &QCheckBox::stateChanged, this, &SystemSettingsWidget::updateVU1InstantState);
}

SystemSettingsWidget::~SystemSettingsWidget() = default;

void SystemSettingsWidget::updateVU1InstantState()
{
	m_ui.instantVU1->setEnabled(!m_dialog->getEffectiveBoolValue("EmuCore/Speedhacks", "vuThread", false));
}

int SystemSettingsWidget::getGlobalClampingModeIndex(bool vu) const
{
	if (Host::GetBaseBoolSettingValue("EmuCore/CPU/Recompiler", vu ? "vuSignOverflow" : "fpuFullMode", false))
		return 3;

	if (Host::GetBaseBoolSettingValue("EmuCore/CPU/Recompiler", vu ? "vuExtraOverflow" : "fpuExtraOverflow", false))
		return 2;

	if (Host::GetBaseBoolSettingValue("EmuCore/CPU/Recompiler", vu ? "vuOverflow" : "fpuOverflow", true))
		return 1;

	return 0;
}

int SystemSettingsWidget::getClampingModeIndex(bool vu) const
{
	// This is so messy... maybe we should just make the mode an int in the settings too...
	const bool base = m_dialog->isPerGameSettings() ? 1 : 0;
	std::optional<bool> default_false = m_dialog->isPerGameSettings() ? std::nullopt : std::optional<bool>(false);
	std::optional<bool> default_true = m_dialog->isPerGameSettings() ? std::nullopt : std::optional<bool>(true);

	std::optional<bool> third = m_dialog->getBoolValue("EmuCore/CPU/Recompiler", vu ? "vuSignOverflow" : "fpuFullMode", default_false);
	std::optional<bool> second = m_dialog->getBoolValue("EmuCore/CPU/Recompiler", vu ? "vuExtraOverflow" : "fpuExtraOverflow", default_false);
	std::optional<bool> first = m_dialog->getBoolValue("EmuCore/CPU/Recompiler", vu ? "vuOverflow" : "fpuOverflow", default_true);

	if (third.has_value() && third.value())
		return base + 3;
	if (second.has_value() && second.value())
		return base + 2;
	if (first.has_value() && first.value())
		return base + 1;
	else if (first.has_value())
		return base + 0; // none
	else
		return 0; // no per game override
}

void SystemSettingsWidget::setClampingMode(bool vu, int index)
{
	std::optional<bool> first, second, third;

	if (!m_dialog->isPerGameSettings() || index > 0)
	{
		const bool base = m_dialog->isPerGameSettings() ? 1 : 0;
		third = (index >= (base + 3));
		second = (index >= (base + 2));
		first = (index >= (base + 1));
	}

	m_dialog->setBoolSettingValue("EmuCore/CPU/Recompiler", vu ? "vuSignOverflow" : "fpuFullMode", third);
	m_dialog->setBoolSettingValue("EmuCore/CPU/Recompiler", vu ? "vuExtraOverflow" : "fpuExtraOverflow", second);
	m_dialog->setBoolSettingValue("EmuCore/CPU/Recompiler", vu ? "vuOverflow" : "fpuOverflow", first);
}
