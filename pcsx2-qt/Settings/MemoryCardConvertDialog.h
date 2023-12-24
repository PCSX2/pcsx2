// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
