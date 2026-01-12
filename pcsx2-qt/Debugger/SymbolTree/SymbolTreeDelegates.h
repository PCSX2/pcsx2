// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "Debugger/SymbolTree/SymbolTreeNode.h"

#include "DebugTools/DebugInterface.h"

#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QLineEdit>

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

private:
	// These make it so the values inputted are written back to memory
	// immediately when the widgets are interacted with rather than when they
	// are deselected.
	void onCheckBoxStateChanged(Qt::CheckState state);
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

private:
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

private:
	DebugInterface& m_cpu;
};

class SymbolTreeIntegerLineEdit : public QLineEdit
{
	Q_OBJECT

public:
	SymbolTreeIntegerLineEdit(SymbolTreeDisplayOptions display_options, s32 size_bits, QWidget* parent);

	std::optional<u64> unsignedValue();
	void setUnsignedValue(u64 value);

	std::optional<s64> signedValue();
	void setSignedValue(s64 value);

private:
	SymbolTreeDisplayOptions m_display_options;
	s32 m_size_bits;
};
