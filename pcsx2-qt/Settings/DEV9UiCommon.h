// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QtGui/QValidator>
#include <QtWidgets/QItemDelegate>

struct HostEntryUi
{
	std::string Url;
	std::string Desc;
	std::string Address = "0.0.0.0";
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

class IPItemDelegate : public QItemDelegate
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
