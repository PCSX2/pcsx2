// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QtCore/QtCore>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QProgressDialog>

#include "QtProgressCallback.h"

#include "pcsx2/SIO/Memcard/MemoryCardFile.h"
#include "pcsx2/SIO/Memcard/MemoryCardFolder.h"

class MemoryCardConvertWorker : public QtAsyncProgressThread
{
public:
	MemoryCardConvertWorker(QWidget* parent, MemoryCardType type, MemoryCardFileType fileType, const std::string& srcFileName, const std::string& destFileName);
	~MemoryCardConvertWorker();

protected:
	void runAsync() override;

private:
	MemoryCardType type;
	MemoryCardFileType fileType;
	std::string srcFileName;
	std::string destFileName;


	bool ConvertToFile(const std::string& srcFolderName, const std::string& destFileName, const MemoryCardFileType type);
	bool ConvertToFolder(const std::string& srcFolderName, const std::string& destFileName, const MemoryCardFileType type);
};
