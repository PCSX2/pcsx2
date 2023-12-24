// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "MemoryCardConvertWorker.h"

#include "common/Console.h"
#include "common/Path.h"
#include "common/FileSystem.h"

MemoryCardConvertWorker::MemoryCardConvertWorker(QWidget* parent, MemoryCardType type, MemoryCardFileType fileType, const std::string& srcFileName, const std::string& destFileName)
		: QtAsyncProgressThread(parent)
{
	this->type = type;
	this->fileType = fileType;
	this->srcFileName = srcFileName;
	this->destFileName = destFileName;
}

MemoryCardConvertWorker::~MemoryCardConvertWorker() = default;

void MemoryCardConvertWorker::runAsync()
{
	switch (type)
	{
		case MemoryCardType::File:
			ConvertToFolder(srcFileName, destFileName, fileType);
			break;
		case MemoryCardType::Folder:
			ConvertToFile(srcFileName, destFileName, fileType);
			break;
		default:
			break;
	}
}

bool MemoryCardConvertWorker::ConvertToFile(const std::string& srcFolderName, const std::string& destFileName, const MemoryCardFileType type)
{
	const std::string srcPath(Path::Combine(EmuFolders::MemoryCards, srcFolderName));
	const std::string destPath(Path::Combine(EmuFolders::MemoryCards, destFileName));
	size_t sizeInMB = 0;

	switch (type)
	{
		case MemoryCardFileType::PS2_8MB:
			sizeInMB = 8;
			break;
		case MemoryCardFileType::PS2_16MB:
			sizeInMB = 16;
			break;
		case MemoryCardFileType::PS2_32MB:
			sizeInMB = 32;
			break;
		case MemoryCardFileType::PS2_64MB:
			sizeInMB = 64;
			break;
		default:
			Console.Error("%s(%s, %s, %d) Received invalid MemoryCardFileType, aborting", __FUNCTION__, srcPath.c_str(), destPath.c_str(), type);
			return false;
	}

	FolderMemoryCard sourceFolderMemoryCard;
	Pcsx2Config::McdOptions config;
	config.Enabled = true;
	config.Type = MemoryCardType::Folder;
	sourceFolderMemoryCard.Open(srcPath, config, (sizeInMB * 1024 * 1024) / FolderMemoryCard::ClusterSize, false, "");
	const size_t capacity = sourceFolderMemoryCard.GetSizeInClusters() * FolderMemoryCard::ClusterSizeRaw;

	std::vector<u8> sourceBuffer;
	sourceBuffer.resize(capacity);
	size_t address = 0;
	this->SetProgressRange(capacity);
	this->SetProgressValue(0);

	while (address < capacity)
	{
		sourceFolderMemoryCard.Read(sourceBuffer.data() + address, address, FolderMemoryCard::PageSizeRaw);
		address += FolderMemoryCard::PageSizeRaw;
		
		// Only report progress every 16 pages. Substantially speeds up the conversion.
		if (address % (FolderMemoryCard::PageSizeRaw * 16) == 0)
			this->SetProgressValue(address);
	}

	bool writeResult = FileSystem::WriteBinaryFile(destPath.c_str(), sourceBuffer.data(), sourceBuffer.size());

	if (!writeResult)
	{
		Console.Error("%s(%s, %s, %d) Failed to write Memory Card contents to file", __FUNCTION__, srcPath.c_str(), destPath.c_str(), type);
		return false;
	}
#ifdef _WIN32
	else
	{
		FileSystem::SetPathCompression(destPath.c_str(), true);
	}
#endif

	sourceFolderMemoryCard.Close(false);
	return true;
}

bool MemoryCardConvertWorker::ConvertToFolder(const std::string& srcFileName, const std::string& destFolderName, const MemoryCardFileType type)
{
	const std::string srcPath(Path::Combine(EmuFolders::MemoryCards, srcFileName));
	const std::string destPath(Path::Combine(EmuFolders::MemoryCards, destFolderName));

	FolderMemoryCard targetFolderMemoryCard;
	Pcsx2Config::McdOptions config;
	config.Enabled = true;
	config.Type = MemoryCardType::Folder;

	std::optional<std::vector<u8>> sourceBufferOpt = FileSystem::ReadBinaryFile(srcPath.c_str());

	if (!sourceBufferOpt.has_value())
	{
		Console.Error("%s(%s, %s, %d) Failed to open file Memory Card!", __FUNCTION__, srcFileName.c_str(), destFolderName.c_str(), type);
		return false;
	}

	std::vector<u8> sourceBuffer = sourceBufferOpt.value();
	// Set progress bar to the literal number of bytes in the memcard.
	// Plus two because there is a lag period after the Save calls complete
	// where the progress bar stalls out; this lets us stop the progress bar
	// just shy of 50 and 100% so it seems like its still doing some work.
	this->SetProgressRange((sourceBuffer.size() * 2) + 2);
	this->SetProgressValue(0);

	// Attempt the write twice. Once with writes being simulated rather than truly committed.
	// Again with actual writes. If a file memcard has a corrupted page or something which would
	// cause the conversion to fail, it will fail on the simulated run, with no files committed
	// to the filesystem yet.
	for (int i = 0; i < 2; i++)
	{
		bool simulateWrites = (i == 0);
		targetFolderMemoryCard.Open(destPath, config, 0, false, "", simulateWrites);

		size_t address = 0;

		while (address < sourceBuffer.size())
		{
			targetFolderMemoryCard.Save(sourceBuffer.data() + address, address, FolderMemoryCard::PageSizeRaw);
			address += FolderMemoryCard::PageSizeRaw;
			
			// Only report progress every 16 pages. Substantially speeds up the conversion.
			if (address % (FolderMemoryCard::PageSizeRaw * 16) == 0)
				this->SetProgressValue(address + (i * sourceBuffer.size()));
		}

		targetFolderMemoryCard.Close();

		// If the source file Memory Card was larger than 8 MB, the raw copy will have also made the superblock of
		// the destination folder Memory Card larger than 8 MB. For compatibility, we always want folder Memory Cards
		// to report 8 MB, so we'll override that here. Don't do this on the simulated run, only the actual.
		if (!simulateWrites && sourceBuffer.size() != FolderMemoryCard::TotalSizeRaw)
		{
			targetFolderMemoryCard.Open(destPath, config, 0, false, "", simulateWrites);
			targetFolderMemoryCard.SetSizeInMB(8);
			targetFolderMemoryCard.Close();
		}

		this->IncrementProgressValue();
	}

	return true;
}
