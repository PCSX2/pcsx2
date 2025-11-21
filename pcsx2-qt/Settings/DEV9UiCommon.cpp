// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include <QtWidgets/QApplication>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QSpinBox>

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
	: QStyledItemDelegate(parent)
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

ComboBoxItemDelegate::ComboBoxItemDelegate(QObject* parent, const char** items, const char* translation_ctx)
	: QStyledItemDelegate(parent)
	, m_items{items}
	, m_translation_ctx{translation_ctx}
{
}

QWidget* ComboBoxItemDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	QComboBox* editor = new QComboBox(parent);

	for (int i = 0; m_items[i] != nullptr; i++)
		editor->addItem(m_translation_ctx ? qApp->translate(m_translation_ctx, m_items[i]) : QString::fromUtf8(m_items[i]));

	return editor;
}

void ComboBoxItemDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
	QString value = index.model()->data(index, Qt::EditRole).toString();
	QComboBox* cBox = static_cast<QComboBox*>(editor);
	cBox->setCurrentIndex(cBox->findText(value));
}

void ComboBoxItemDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
	QComboBox* cBox = static_cast<QComboBox*>(editor);
	QString value = cBox->currentText();
	model->setData(index, value, Qt::EditRole);
}

void ComboBoxItemDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	editor->setGeometry(option.rect);
}

SpinBoxItemDelegate::SpinBoxItemDelegate(QObject* parent, int min, int max)
	: QStyledItemDelegate(parent)
	, m_min{min}
	, m_max{max}
{
}

QWidget* SpinBoxItemDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	QSpinBox* editor = new QSpinBox(parent);
	editor->setMinimum(m_min);
	editor->setMaximum(m_max);
	return editor;
}

void SpinBoxItemDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
	int value = index.model()->data(index, Qt::EditRole).toInt();
	QSpinBox* sBox = static_cast<QSpinBox*>(editor);
	sBox->setValue(value);
}

void SpinBoxItemDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
	QSpinBox* sBox = static_cast<QSpinBox*>(editor);
	int value = sBox->value();
	model->setData(index, value, Qt::EditRole);
}

void SpinBoxItemDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	editor->setGeometry(option.rect);
}
