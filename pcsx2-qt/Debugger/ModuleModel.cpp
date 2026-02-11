// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "ModuleModel.h"

#include "QtUtils.h"
#include "fmt/format.h"

ModuleModel::ModuleModel(DebugInterface& cpu, QObject* parent)
	: QAbstractTableModel(parent)
	, m_cpu(cpu)
{
}

int ModuleModel::rowCount(const QModelIndex&) const
{
	return m_cpu.GetModuleList().size();
}

int ModuleModel::columnCount(const QModelIndex&) const
{
	return ModuleModel::COLUMN_COUNT;
}

QVariant ModuleModel::data(const QModelIndex& index, int role) const
{
	size_t row = static_cast<size_t>(index.row());
	if (row >= m_modules.size())
		return QVariant();

	const IopMod* mod = &m_modules[row];

	if (role == Qt::DisplayRole)
	{
		switch (index.column())
		{
			case ModuleModel::ModuleColumns::NAME:
				return mod->name.c_str();
			case ModuleModel::ModuleColumns::VERSION:
				return fmt::format("{}.{}", mod->version >> 8, mod->version & 0xff).c_str();
			case ModuleModel::ModuleColumns::ENTRY:
				return QtUtils::FilledQStringFromValue(mod->entry, 16);
			case ModuleModel::ModuleColumns::GP:
				return QtUtils::FilledQStringFromValue(mod->gp, 16);
			case ModuleModel::ModuleColumns::TEXT_SECTION:
			{
				return QString("[%1 - %2]").arg(QtUtils::FilledQStringFromValue(mod->text_addr, 16), QtUtils::FilledQStringFromValue(mod->text_addr + mod->text_size - 1, 16));
			}
			case ModuleModel::ModuleColumns::DATA_SECTION:
			{
				u32 addr = mod->text_addr + mod->text_size;
				return QString("[%1 - %2]").arg(QtUtils::FilledQStringFromValue(addr, 16), QtUtils::FilledQStringFromValue(addr + mod->data_size - 1, 16));
			}
			case ModuleModel::ModuleColumns::BSS_SECTION:
			{
				if (mod->bss_size == 0)
				{
					return "";
				}
				u32 addr = mod->text_addr + mod->text_size + mod->data_size;
				return QString("[%1 - %2]").arg(QtUtils::FilledQStringFromValue(addr, 16), QtUtils::FilledQStringFromValue(addr + mod->bss_size - 1, 16));
			}
		}
	}
	else if (role == Qt::UserRole)
	{
		switch (index.column())
		{
			case ModuleModel::ModuleColumns::NAME:
				return mod->name.c_str();
			case ModuleModel::ModuleColumns::VERSION:
				return mod->version;
			case ModuleModel::ModuleColumns::ENTRY:
				return mod->entry;
			case ModuleModel::ModuleColumns::GP:
				return mod->gp;
			case ModuleModel::ModuleColumns::TEXT_SECTION:
				return mod->text_addr;
			case ModuleModel::ModuleColumns::DATA_SECTION:
				return mod->text_addr + mod->text_size;
			case ModuleModel::ModuleColumns::BSS_SECTION:
			{
				if (mod->bss_size == 0)
				{
					return 0;
				}
				return mod->text_addr + mod->text_size + mod->data_size;
			}
		}
	}
	return QVariant();
}

QVariant ModuleModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
	{
		switch (section)
		{
			case ModuleColumns::NAME:
				//: Warning: short space limit. Abbreviate if needed.
				return tr("NAME");
			case ModuleColumns::VERSION:
				//: Warning: short space limit. Abbreviate if needed.
				return tr("VERSION");
			case ModuleColumns::ENTRY:
				//: Warning: short space limit. Abbreviate if needed. // Entrypoint of the executable
				return tr("ENTRY");
			case ModuleColumns::GP:
				//: Warning: short space limit. Abbreviate if needed.
				return tr("GP");
			case ModuleColumns::TEXT_SECTION:
				//: Warning: short space limit. Abbreviate if needed. // Text section of the executable
				return tr("TEXT");
			case ModuleColumns::DATA_SECTION:
				//: Warning: short space limit. Abbreviate if needed. // Data section of the executable
				return tr("DATA");
			case ModuleColumns::BSS_SECTION:
				//: Warning: short space limit. Abbreviate if needed. // BSS section of the executable
				return tr("BSS");
			default:
				return QVariant();
		}
	}
	return QVariant();
}

void ModuleModel::refreshData()
{
	beginResetModel();
	m_modules = m_cpu.GetModuleList();
	endResetModel();
}
