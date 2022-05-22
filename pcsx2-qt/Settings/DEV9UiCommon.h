/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

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
