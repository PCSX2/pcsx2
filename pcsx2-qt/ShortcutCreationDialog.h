// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once
#include "ui_ShortcutCreationDialog.h"

#include <QtWidgets/QDialog>
#include <qcontainerfwd.h>

class ShortcutCreationDialog final : public QDialog
{
	Q_OBJECT

public:
	ShortcutCreationDialog(QWidget* parent, const QString& title, const QString& path);
	~ShortcutCreationDialog() = default;

#if !defined(__APPLE__)
	// Create desktop shortcut for games
	void CreateShortcut(const std::string name, const std::string game_path, std::vector<std::string> passed_cli_args, std::string custom_args, bool is_desktop);
#endif

protected:
	QString m_title;
	QString m_path;
	bool m_desktop;
	Ui::ShortcutCreationDialog m_ui;
};
