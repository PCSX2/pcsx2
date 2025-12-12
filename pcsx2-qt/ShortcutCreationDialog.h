// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once
#include "ui_ShortcutCreationDialog.h"

#include <QtWidgets/QDialog>

class ShortcutCreationDialog final : public QDialog
{
	Q_OBJECT

public:
	ShortcutCreationDialog(QWidget* parent, const QString& game_title, const QString& game_serial, const QString& game_path);
	~ShortcutCreationDialog() = default;

	/// Creates shortcut for a game with user-specified launch arguments.
	void CreateGameShortcut(std::vector<std::string>& launch_arguments, std::string custom_arguments, bool is_desktop);

#if defined(_WIN32)
	/// Escapes a command line argument on Windows.
	static void EscapeCommandLineArgumentWindows(std::string& cli_argument);
#else
	/// Escapes a command line argument on Linux.
	/// Return value indicates whether the escaping operation is lossless.
	static bool EscapeCommandLineArgumentLinux(std::string& cli_argument);
#endif

	/// Fills a list with CLI arguments based on user selections.
	void FillArgumentsList(std::vector<std::string>& arg_list);

private:
	const std::string m_game_title;
	const std::string m_game_serial;
	std::string m_game_path;

	enum class StateType : uint8_t
	{
		None,
		Index,
		File
	};

	StateType m_save_state_type = StateType::None;
	std::string m_save_state_name = "";

	Ui::ShortcutCreationDialog m_ui;
};
