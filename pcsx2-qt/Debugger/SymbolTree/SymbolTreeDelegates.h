// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QtWidgets/QStyledItemDelegate>

#include "DebugTools/SymbolGuardian.h"

class SymbolTreeValueDelegate : public QStyledItemDelegate
{
	Q_OBJECT

public:
	SymbolTreeValueDelegate(
		DebugInterface& cpu,
		QObject* parent = nullptr);

	QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
	void setEditorData(QWidget* editor, const QModelIndex& index) const override;
	void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;

protected:
	// Without this, setModelData would only be called when a combo box was
	// deselected rather than when an option was picked.
	void onComboBoxIndexChanged(int index);

	DebugInterface& m_cpu;
};

class SymbolTreeLocationDelegate : public QStyledItemDelegate
{
	Q_OBJECT

public:
	SymbolTreeLocationDelegate(
		DebugInterface& cpu,
		u32 alignment,
		QObject* parent = nullptr);

	QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
	void setEditorData(QWidget* editor, const QModelIndex& index) const override;
	void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;

protected:
	DebugInterface& m_cpu;
	u32 m_alignment;
};

class SymbolTreeTypeDelegate : public QStyledItemDelegate
{
	Q_OBJECT

public:
	SymbolTreeTypeDelegate(
		DebugInterface& cpu,
		QObject* parent = nullptr);

	QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
	void setEditorData(QWidget* editor, const QModelIndex& index) const override;
	void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;

protected:
	DebugInterface& m_cpu;
};
