// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
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

protected:
	const QString m_title;
	const QString m_path;

private:
	Ui::ShortcutCreationDialog m_ui;
};
