#include "stdafx.h"
#include <DDS.h>

DDS::DDSFile DDS::CatchDDS(const char* fileName) {
	std::fstream binaryIo;

	char* _headerData = new char[0x80];

	DDSHeader* _header;
	DDSFile _returnFile;

	binaryIo.open(fileName, std::ios::in | std::ios::binary);
	binaryIo.read(_headerData, 0x80);

	_header = reinterpret_cast<DDSHeader*>(_headerData);

	if (_header->Magic == 0x20534444) {
		int const _len = _header->Height * _header->Width * 4;

		std::vector<unsigned char> _tmpData;
		std::vector<unsigned char> _tmpFix;

		_tmpData.resize(_len);

		binaryIo.read(reinterpret_cast<char*>(_tmpData.data()), _len);
		binaryIo.close();

		_returnFile.Header = *_header;
		_returnFile.Data = _tmpData;

		if (_header->ColorFlag == 0x00FF0000) {
			for (int i = 0; i < _len; i += 4) {
				_tmpFix.push_back(_tmpData[i + 2]);
				_tmpFix.push_back(_tmpData[i + 1]);
				_tmpFix.push_back(_tmpData[i + 0]);
				_tmpFix.push_back(_tmpData[i + 3]);
			}

			_returnFile.Data = _tmpFix;
		}

		return _returnFile;
	}

	else
		return DDSFile();
}
