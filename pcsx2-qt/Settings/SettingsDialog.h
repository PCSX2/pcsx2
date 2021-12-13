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

#pragma once
#include "ui_SettingsDialog.h"
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtWidgets/QDialog>
#include <array>

class InterfaceSettingsWidget;
class GameListSettingsWidget;
class EmulationSettingsWidget;
class BIOSSettingsWidget;
class SystemSettingsWidget;
class AdvancedSystemSettingsWidget;
class GameFixSettingsWidget;
class GraphicsSettingsWidget;
class AudioSettingsWidget;
class MemoryCardSettingsWidget;

class SettingsDialog final : public QDialog
{
	Q_OBJECT

public:
	enum class Category
	{
		InterfaceSettings,
		GameListSettings,
		BIOSSettings,
		EmulationSettings,
		SystemSettings,
		AdvancedSystemSettings,
		GameFixSettings,
		GraphicsSettings,
		AudioSettings,
		MemoryCardSettings,
		Count
	};

	explicit SettingsDialog(QWidget* parent = nullptr);
	~SettingsDialog();

	InterfaceSettingsWidget* getInterfaceSettingsWidget() const { return m_interface_settings; }
	GameListSettingsWidget* getGameListSettingsWidget() const { return m_game_list_settings; }
	BIOSSettingsWidget* getBIOSSettingsWidget() const { return m_bios_settings; }
	EmulationSettingsWidget* getEmulationSettingsWidget() const { return m_emulation_settings; }
	SystemSettingsWidget* getSystemSettingsWidget() const { return m_system_settings; }
	AdvancedSystemSettingsWidget* getAdvancedSystemSettingsWidget() const { return m_advanced_system_settings; }
	GameFixSettingsWidget* getGameFixSettingsWidget() const { return m_game_fix_settings_widget; }
	GraphicsSettingsWidget* getGraphicsSettingsWidget() const { return m_graphics_settings; }
	AudioSettingsWidget* getAudioSettingsWidget() const { return m_audio_settings; }
	MemoryCardSettingsWidget* getMemoryCardSettingsWidget() const { return m_memory_card_settings; }

	void registerWidgetHelp(QObject* object, QString title, QString recommended_value, QString text);
	bool eventFilter(QObject* object, QEvent* event) override;

Q_SIGNALS:
	void settingsResetToDefaults();

public Q_SLOTS:
	void setCategory(Category category);

private Q_SLOTS:
	void onCategoryCurrentRowChanged(int row);
	void onRestoreDefaultsClicked();

private:
	void setCategoryHelpTexts();

	Ui::SettingsDialog m_ui;

	InterfaceSettingsWidget* m_interface_settings = nullptr;
	GameListSettingsWidget* m_game_list_settings = nullptr;
	BIOSSettingsWidget* m_bios_settings = nullptr;
	EmulationSettingsWidget* m_emulation_settings = nullptr;
	SystemSettingsWidget* m_system_settings = nullptr;
	AdvancedSystemSettingsWidget* m_advanced_system_settings = nullptr;
	GameFixSettingsWidget* m_game_fix_settings_widget = nullptr;
	GraphicsSettingsWidget* m_graphics_settings = nullptr;
	AudioSettingsWidget* m_audio_settings = nullptr;
	MemoryCardSettingsWidget* m_memory_card_settings = nullptr;

	std::array<QString, static_cast<int>(Category::Count)> m_category_help_text;

	QObject* m_current_help_widget = nullptr;
	QMap<QObject*, QString> m_widget_help_text_map;
};
