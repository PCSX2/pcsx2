// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <QtGui/QValidator>
#include <QtWidgets/QStyledItemDelegate>

#include "common/Pcsx2Types.h"

struct HostEntryUi
{
	std::string Url;
	std::string Desc;
	std::string Address = "0.0.0.0";
	bool Enabled;
};

struct PortEntryUi
{
	std::string Protocol;
	std::string Desc;
	u16 Port = 0;
	bool Enabled;
};

class IPValidator : public QValidator
{
	Q_OBJECT

public:
	explicit IPValidator(QObject* parent = nullptr, bool allowEmpty = false);
	virtual State validate(QString& input, int& pos) const override;

private:
	static const QRegularExpression intermediateRegex;
	static const QRegularExpression finalRegex;

	bool m_allowEmpty;
};

class IPItemDelegate : public QStyledItemDelegate
{
	Q_OBJECT

public:
	explicit IPItemDelegate(QObject* parent = nullptr);

protected:
	QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const;
	void setEditorData(QWidget* editor, const QModelIndex& index) const;
	void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const;
	void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const;
};

class ComboBoxItemDelegate : public QStyledItemDelegate
{
	Q_OBJECT

private:
	const char** m_items;
	const char* m_translation_ctx;

public:
	// items & translation_ctx needs be valid for the lifetime of QItemDelegate
	explicit ComboBoxItemDelegate(QObject* parent, const char** items, const char* translation_ctx);

protected:
	QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const;
	void setEditorData(QWidget* editor, const QModelIndex& index) const;
	void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const;
	void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const;
};

class SpinBoxItemDelegate : public QStyledItemDelegate
{
	Q_OBJECT

private:
	int m_min;
	int m_max;

public:
	explicit SpinBoxItemDelegate(QObject* parent, int min, int max);

protected:
	QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const;
	void setEditorData(QWidget* editor, const QModelIndex& index) const;
	void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const;
	void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const;
};
