/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#include <QtWidgets/QDialog>

#include "common/FileSystem.h"

#include "ui_MemoryCardConvertDialog.h"

#include "MemoryCardConvertWorker.h"

#include "pcsx2/SIO/Memcard/MemoryCardFile.h"

class MemoryCardConvertDialog final : public QDialog
{
	Q_OBJECT

public:
	explicit MemoryCardConvertDialog(QWidget* parent, QString selectedCard);
	~MemoryCardConvertDialog();

	bool IsSetup();
	void onStatusUpdated();
	void onProgressUpdated(int value, int range);
	void onThreadFinished();

private Q_SLOTS:
	void ConvertCard();
	void ConvertCallback();

private:
	void StartThread();
	void CancelThread();
	void UpdateEnabled();
	bool SetupPicklist();
	void SetType(MemoryCardType type, MemoryCardFileType fileType, const QString& description);
	void SetType_8();
	void SetType_16();
	void SetType_32();
	void SetType_64();
	void SetType_Folder();
	
	Ui::MemoryCardConvertDialog m_ui;

	bool isSetup = false;
	AvailableMcdInfo m_srcCardInfo;
	QString m_selectedCard;
	QString m_destCardName;

	MemoryCardType m_type = MemoryCardType::File;
	MemoryCardFileType m_fileType = MemoryCardFileType::PS2_8MB;
	std::unique_ptr<MemoryCardConvertWorker> m_thread;

	static constexpr u32 FLAGS = FILESYSTEM_FIND_RECURSIVE | FILESYSTEM_FIND_FOLDERS | FILESYSTEM_FIND_FILES;
};

// Card capacities computed from freshly formatted superblocks. 
namespace CardCapacity
{
	static constexpr size_t _8_MB = 0x1f40 * 512 * 2; //(0x1fc7 - 0x29) * 2 * 512;
	static constexpr size_t _16_MB = 0x3e80 * 512 * 2; //(0x3fa7 - 0x49) * 2 * 512;
	static constexpr size_t _32_MB = 0x7d00 * 512 * 2; //(0x7f67 - 0x89) * 2 * 512;
	static constexpr size_t _64_MB = 0xfde8 * 512 * 2; //(0xfee7 - 0x0109) * 2 * 512;
}
