// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include "ui_SettingsWindow.h"
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtWidgets/QWidget>
#include <array>
#include <memory>

class QWheelEvent;

class INISettingsInterface;
class SettingsInterface;

namespace GameList
{
	struct Entry;
}

class InterfaceSettingsWidget;
class GameListSettingsWidget;
class EmulationSettingsWidget;
class BIOSSettingsWidget;
class GameCheatSettingsWidget;
class GameFixSettingsWidget;
class GamePatchSettingsWidget;
class GraphicsSettingsWidget;
class AudioSettingsWidget;
class MemoryCardSettingsWidget;
class FolderSettingsWidget;
class DEV9SettingsWidget;
class AchievementSettingsWidget;
class AdvancedSettingsWidget;
class DebugSettingsWidget;

class SettingsWindow final : public QWidget
{
	Q_OBJECT

public:
	explicit SettingsWindow();
	SettingsWindow(std::unique_ptr<INISettingsInterface> sif, const GameList::Entry* game, std::string serial,
		u32 disc_crc, QString filename = QString());
	~SettingsWindow();

	static void openGamePropertiesDialog(const GameList::Entry* game, const std::string_view title, std::string serial, u32 disc_crc);
	static void closeGamePropertiesDialogs();

	SettingsInterface* getSettingsInterface() const;
	__fi bool isPerGameSettings() const { return static_cast<bool>(m_sif); }
	__fi const std::string& getSerial() const { return m_serial; }
	__fi u32 getDiscCRC() const { return m_disc_crc; }

	__fi InterfaceSettingsWidget* getInterfaceSettingsWidget() const { return m_interface_settings; }
	__fi GameListSettingsWidget* getGameListSettingsWidget() const { return m_game_list_settings; }
	__fi BIOSSettingsWidget* getBIOSSettingsWidget() const { return m_bios_settings; }
	__fi EmulationSettingsWidget* getEmulationSettingsWidget() const { return m_emulation_settings; }
	__fi GameCheatSettingsWidget* getGameCheatSettingsWidget() const { return m_game_cheat_settings_widget; }
	__fi GameFixSettingsWidget* getGameFixSettingsWidget() const { return m_game_fix_settings_widget; }
	__fi GamePatchSettingsWidget* getGamePatchSettingsWidget() const { return m_game_patch_settings_widget; }
	__fi GraphicsSettingsWidget* getGraphicsSettingsWidget() const { return m_graphics_settings; }
	__fi AudioSettingsWidget* getAudioSettingsWidget() const { return m_audio_settings; }
	__fi MemoryCardSettingsWidget* getMemoryCardSettingsWidget() const { return m_memory_card_settings; }
	__fi FolderSettingsWidget* getFolderSettingsWidget() const { return m_folder_settings; }
	__fi DEV9SettingsWidget* getDEV9SettingsWidget() const { return m_dev9_settings; }
	__fi AchievementSettingsWidget* getAchievementSettingsWidget() const { return m_achievement_settings; }
	__fi AdvancedSettingsWidget* getAdvancedSettingsWidget() const { return m_advanced_settings; }

	void registerWidgetHelp(QObject* object, QString title, QString recommended_value, QString text);
	bool eventFilter(QObject* object, QEvent* event) override;

	void setWindowTitle(const QString& title);

	QString getCategory() const;
	void setCategory(const char* category);

	// Helper functions for reading effective setting values (from game -> global settings).
	bool getEffectiveBoolValue(const char* section, const char* key, bool default_value) const;
	int getEffectiveIntValue(const char* section, const char* key, int default_value) const;
	float getEffectiveFloatValue(const char* section, const char* key, float default_value) const;
	std::string getEffectiveStringValue(const char* section, const char* key, const char* default_value) const;

	// Helper functions for reading setting values for this layer (game settings or global).
	std::optional<bool> getBoolValue(const char* section, const char* key, std::optional<bool> default_value) const;
	std::optional<int> getIntValue(const char* section, const char* key, std::optional<int> default_value) const;
	std::optional<float> getFloatValue(const char* section, const char* key, std::optional<float> default_value) const;
	std::optional<std::string> getStringValue(const char* section, const char* key, std::optional<const char*> default_value) const;
	void setBoolSettingValue(const char* section, const char* key, std::optional<bool> value);
	void setIntSettingValue(const char* section, const char* key, std::optional<int> value);
	void setFloatSettingValue(const char* section, const char* key, std::optional<float> value);
	void setStringSettingValue(const char* section, const char* key, std::optional<const char*> value);
	bool containsSettingValue(const char* section, const char* key) const;
	void removeSettingValue(const char* section, const char* key);
	void saveAndReloadGameSettings();

Q_SIGNALS:
	void settingsResetToDefaults();

private Q_SLOTS:
	void onCategoryCurrentRowChanged(int row);
	void onRestoreDefaultsClicked();
	void onCopyGlobalSettingsClicked();
	void onClearSettingsClicked();

protected:
	void closeEvent(QCloseEvent*) override;
	void wheelEvent(QWheelEvent* event) override;

private:
	enum : u32
	{
		MAX_SETTINGS_WIDGETS = 13
	};

	void setupUi(const GameList::Entry* game);

	void addWidget(QWidget* widget, QString title, QString icon, QString help_text);
	bool handleWheelEvent(QWheelEvent* event);

	SettingsWindow* reopen();

	std::unique_ptr<INISettingsInterface> m_sif;

	Ui::SettingsWindow m_ui;

	InterfaceSettingsWidget* m_interface_settings = nullptr;
	GameListSettingsWidget* m_game_list_settings = nullptr;
	BIOSSettingsWidget* m_bios_settings = nullptr;
	EmulationSettingsWidget* m_emulation_settings = nullptr;
	GameCheatSettingsWidget* m_game_cheat_settings_widget = nullptr;
	GameFixSettingsWidget* m_game_fix_settings_widget = nullptr;
	GamePatchSettingsWidget* m_game_patch_settings_widget = nullptr;
	GraphicsSettingsWidget* m_graphics_settings = nullptr;
	AudioSettingsWidget* m_audio_settings = nullptr;
	MemoryCardSettingsWidget* m_memory_card_settings = nullptr;
	FolderSettingsWidget* m_folder_settings = nullptr;
	DEV9SettingsWidget* m_dev9_settings = nullptr;
	AchievementSettingsWidget* m_achievement_settings = nullptr;
	AdvancedSettingsWidget* m_advanced_settings = nullptr;
	DebugSettingsWidget* m_debug_settings = nullptr;

	std::array<QString, MAX_SETTINGS_WIDGETS> m_category_help_text;

	QObject* m_current_help_widget = nullptr;
	QMap<QObject*, QString> m_widget_help_text_map;

	QString m_filename;

	std::string m_game_list_filename;
	std::string m_serial;
	u32 m_disc_crc;
};
