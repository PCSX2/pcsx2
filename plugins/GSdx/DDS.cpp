#include "stdafx.h"
#include <DDS.h>

DDS::DDSFile DDS::CatchDDS(const char* fileName)
{
	std::fstream binaryIo;

	char* _headerData = new char[0x80];

	DDSHeader* _header;
	DDSFile _returnFile;

	binaryIo.open(fileName, std::ios::in | std::ios::binary);
	binaryIo.read(_headerData, 0x80);

	_header = reinterpret_cast<DDSHeader*>(_headerData);

	if (_header->Magic == 0x20534444)
	{
		int const _len = _header->Height * _header->Width * 4;
		std::vector<unsigned char> _tmpData;

		_tmpData.resize(_len);

		binaryIo.read(reinterpret_cast<char*>(_tmpData.data()), _len);
		binaryIo.close();

		_returnFile.Header = *_header;
		_returnFile.Data = _tmpData;

		return _returnFile;
	}

	else
		return DDSFile();
}
