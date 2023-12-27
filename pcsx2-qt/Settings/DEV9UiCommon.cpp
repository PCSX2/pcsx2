// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include <QtWidgets/QLineEdit>

#include "DEV9UiCommon.h"

#define IP_RANGE_INTER "([0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5]|)"
#define IP_RANGE_FINAL "([0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5])"

// clang-format off
const QRegularExpression IPValidator::intermediateRegex{QStringLiteral("^" IP_RANGE_INTER "\\." IP_RANGE_INTER "\\." IP_RANGE_INTER "\\." IP_RANGE_INTER "$")};
const QRegularExpression IPValidator::finalRegex       {QStringLiteral("^" IP_RANGE_FINAL "\\." IP_RANGE_FINAL "\\." IP_RANGE_FINAL "\\." IP_RANGE_FINAL "$")};
// clang-format on

IPValidator::IPValidator(QObject* parent, bool allowEmpty)
	: QValidator(parent)
	, m_allowEmpty{allowEmpty}
{
}

QValidator::State IPValidator::validate(QString& input, int& pos) const
{
	if (input.isEmpty())
		return m_allowEmpty ? Acceptable : Intermediate;

	QRegularExpressionMatch m = finalRegex.match(input, 0, QRegularExpression::NormalMatch);
	if (m.hasMatch())
		return Acceptable;

	m = intermediateRegex.match(input, 0, QRegularExpression::PartialPreferCompleteMatch);
	if (m.hasMatch() || m.hasPartialMatch())
		return Intermediate;
	else
	{
		pos = input.size();
		return Invalid;
	}
}

IPItemDelegate::IPItemDelegate(QObject* parent)
	: QItemDelegate(parent)
{
}

QWidget* IPItemDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	QLineEdit* editor = new QLineEdit(parent);
	editor->setValidator(new IPValidator());
	return editor;
}

void IPItemDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
	QString value = index.model()->data(index, Qt::EditRole).toString();
	QLineEdit* line = static_cast<QLineEdit*>(editor);
	line->setText(value);
}

void IPItemDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
	QLineEdit* line = static_cast<QLineEdit*>(editor);
	QString value = line->text();
	model->setData(index, value);
}

void IPItemDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	editor->setGeometry(option.rect);
}
