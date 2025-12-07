// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once
#include "ui_ShortcutCreationDialog.h"

#include <QtWidgets/QDialog>

class ShortcutCreationDialog final : public QDialog
{
	Q_OBJECT

public:
	ShortcutCreationDialog(QWidget* parent, const QString& title, const QString& path);
	~ShortcutCreationDialog() = default;

	/// Create desktop shortcut for games
	void CreateShortcut(const std::string name, const std::string game_path, std::vector<std::string> passed_cli_args, std::string custom_args, bool is_desktop);

	/// Escapes the given string for use with command line arguments.
	/// Returns a bool that indicates whether the escaping operation are lossless or not.
	bool EscapeShortcutCommandLine(std::string* cmdline);

protected:
	const QString m_title;
	const QString m_path;

private:
	Ui::ShortcutCreationDialog m_ui;
};
