// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_ToolbarCustomizationDialog.h"

#include <vector>

class QAction;

struct ToolbarActionInfo
{
	const char* id = nullptr;
	QAction* action = nullptr;
};

class ToolbarCustomizationDialog final : public QDialog
{
	Q_OBJECT

public:
	explicit ToolbarCustomizationDialog(QWidget* parent, std::vector<ToolbarActionInfo> action_registry);
	~ToolbarCustomizationDialog() override;

private Q_SLOTS:
	void onAddClicked();
	void onRemoveClicked();
	void onMoveUpClicked();
	void onMoveDownClicked();
	void onAddSeparatorClicked();
	void onRestoreDefaultsClicked();
	void onAccepted();
	void updateButtonStates();

private:
	void populateFromLayout(const QString& layout_str);

	std::vector<ToolbarActionInfo> m_action_registry;
	Ui::ToolbarCustomizationDialog m_ui;
};
