#pragma once
#ifndef __KEY_MOVIE_ONFILE_H__
#define __KEY_MOVIE_ONFILE_H__

#include "PadData.h"
#include "System.h"

//----------------------------
// header
// fseek since the direct place designation is done with, the order of the variables is important
// Specifically, "FrameMax" +2 "UndoCount" is located at the position of +6 bytes
// fseek�Œ��ڏꏊ�w�肵�Ă���̂ŕϐ��̏��Ԃ͑厖
// ��̓I�ɂ́uFrameMax�v��+2�uUndoCount�v��+6�o�C�g�̈ʒu�ɔz�u
//----------------------------
struct InputRecordingHeader
{
	u8 version = 3;
	u8 ID = 0xCC; // Especially, there is no meaning, a key file or ID for judgment
				  // ���ɈӖ��͂Ȃ��Akey�t�@�C��������p��ID

	char emu[50] = "PCSX2-1.4.0-rr";
	char author[50] = "";
	char cdrom[50] = "";

public:
	void setAuthor(wxString author);
	void setCdrom(wxString cdrom);
	void init();
};

//----------------------------
// InputRecordingSavestate
// Contains info about the starting point of the movie
//----------------------------
struct InputRecordingSavestate
{
	bool fromSavestate = false; // Whether we start from the savestate or from power-on
	unsigned int savestatesize; // The size of the savestate
	VmStateBuffer savestate; // The savestate
};

//----------------------------
// InputRecordingFile
//----------------------------
class InputRecordingFile {
public:
	InputRecordingFile() {}
	~InputRecordingFile() { Close(); }
public:

	// file
	bool Open(const wxString fn, bool fNewOpen, VmStateBuffer *ss = nullptr);
	bool Close();

	// movie
	bool writeKeyBuf(const uint & frame, const uint port, const uint bufIndex, const u8 & buf);
	bool readKeyBuf(u8 & result, const uint & frame, const uint port, const uint bufIndex);

	// pad data
	void getPadData(PadData & result_pad, unsigned long frame);
	bool DeletePadData(unsigned long frame);
	bool InsertPadData(unsigned long frame, const PadData& key);
	bool UpdatePadData(unsigned long frame, const PadData& key);

	// convert
	void ConvertV2ToV3(wxString filename);
	void ConvertV1_XToV2(wxString filename);
	void ConvertV1ToV2(wxString filename);
	void ConvertLegacy(wxString filename);

private:
	FILE * fp=NULL;
	wxString filename = "";

private:

	//--------------------
	// block 
	//--------------------
	long _getBlockSeekPoint(const long & frame);


public:
	//--------------------
	// header
	//--------------------
	InputRecordingHeader& getHeader() { return header; }
	unsigned long& getMaxFrame() { return MaxFrame; }
	unsigned long& getUndoCount() { return UndoCount; }
	const wxString & getFilename() { return filename; }

	bool writeHeader();
	bool writeSavestate();
	bool writeMaxFrame();

	bool readHeaderAndCheck();
	void updateFrameMax(unsigned long frame);
	void addUndoCount();

private:
	InputRecordingHeader header;
	InputRecordingSavestate savestate;
	unsigned long  MaxFrame = 0;
	unsigned long  UndoCount = 0;


};



#endif
