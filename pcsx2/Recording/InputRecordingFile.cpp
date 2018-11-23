#include "PrecompiledHeader.h"

#include "App.h"
#include "Common.h"
#include "Counters.h"
#include "MainFrame.h"
#include "MemoryTypes.h"

#include "InputRecordingFile.h"

#define HEADER_SIZE (sizeof(InputRecordingHeader)+4+4)
#define SAVESTATE_HEADER_SIZE (sizeof(bool))
#define BLOCK_HEADER_SIZE (0) 
#define BLOCK_DATA_SIZE (18*2)
#define BLOCK_SIZE (BLOCK_HEADER_SIZE+BLOCK_DATA_SIZE)

#define SEEKPOINT_FRAMEMAX (sizeof(InputRecordingHeader))
#define SEEKPOINT_UNDOCOUNT (sizeof(InputRecordingHeader)+4)
#define SEEKPOINT_SAVESTATE (SEEKPOINT_UNDOCOUNT+4)

long InputRecordingFile::_getBlockSeekPoint(const long & frame)
{
	if (savestate.fromSavestate) {
		return HEADER_SIZE
			+ SAVESTATE_HEADER_SIZE
			+ frame * BLOCK_SIZE;
	}
	else {
		return HEADER_SIZE + sizeof(bool) + (frame)*BLOCK_SIZE;
	}
}

// Inits the new (or existing) input recording file
bool InputRecordingFile::Open(const wxString path, bool fNewOpen, bool fromSaveState)
{
	Close();
	wxString mode = L"rb+";
	if (fNewOpen) {
		mode = L"wb+";
		MaxFrame = 0;
		UndoCount = 0;
		header.init();
	}
	recordingFile = wxFopen(path, mode);
	if ( recordingFile == NULL )
	{
		recordingConLog(wxString::Format("[REC]: Movie file opening failed. Error - %s\n", strerror(errno)));
		return false;
	}
	filename = path;

	// problems seem to be be based in how we are saving the savestate
	if (fNewOpen) {
		if (fromSaveState) {
			savestate.fromSavestate = true;
			// TODO - Return save-state data back into the movie file eventually.
			FILE* ssFileCheck = wxFopen(path + "_SaveState.p2s", "r");
			if (ssFileCheck != NULL) {
				wxCopyFile(path + "_SaveState.p2s", path + "_SaveState.p2s.bak", false);
			}
			fclose(ssFileCheck);
			StateCopy_SaveToFile(path + "_SaveState.p2s");
		}
		else {
			sApp.SysExecute();
		}
	}
	return true;
}

bool InputRecordingFile::Close()
{
	if (recordingFile == NULL)
		return false;
	writeHeader();
	writeSaveState();
	fclose(recordingFile);
	recordingFile = NULL;
	filename = "";
	return true;
}

bool InputRecordingFile::writeSaveState() {
	if (recordingFile == NULL)
		return false;

	fseek(recordingFile, SEEKPOINT_SAVESTATE, SEEK_SET);
	if (fwrite(&savestate.fromSavestate, sizeof(bool), 1, recordingFile) != 1) 
		return false;

	return true;
}

//----------------------------------
// write frame
//----------------------------------
bool InputRecordingFile::writeKeyBuf(const uint & frame, const uint port, const uint bufIndex, const u8 & buf)
{
	if (recordingFile == NULL)
		return false;

	long seek = _getBlockSeekPoint(frame) + BLOCK_HEADER_SIZE + 18 * port + bufIndex;

	if (fseek(recordingFile, seek, SEEK_SET) != 0)
		return false;
	if (fwrite(&buf, 1, 1, recordingFile) != 1)
		return false;

	fflush(recordingFile);
	return true;
}

//----------------------------------
// read frame
//----------------------------------
bool InputRecordingFile::readKeyBuf(u8 & result,const uint & frame, const uint port, const uint  bufIndex)
{
	if (recordingFile == NULL)
		return false;

	long seek = _getBlockSeekPoint(frame) + BLOCK_HEADER_SIZE + 18 * port + bufIndex;
	if (fseek(recordingFile, seek, SEEK_SET) != 0)
		return false;
	if (fread(&result, 1, 1, recordingFile) != 1)
		return false;

	return true;
}

//===================================
// pad
//===================================
void InputRecordingFile::getPadData(PadData & result, unsigned long frame)
{
	result.fExistKey = false;
	if (recordingFile == NULL)
		return;

	long seek = _getBlockSeekPoint(frame) + BLOCK_HEADER_SIZE;
	if (fseek(recordingFile, seek, SEEK_SET) != 0)
		return;
	if (fread(result.buf, 1, BLOCK_DATA_SIZE, recordingFile) == 0)
		return;

	result.fExistKey = true;
}

bool InputRecordingFile::DeletePadData(unsigned long frame)
{
	if (recordingFile == NULL)
		return false;

	for (unsigned long i = frame; i < MaxFrame - 1; i++)
	{
		long seek1 = _getBlockSeekPoint(i+1) + BLOCK_HEADER_SIZE;
		long seek2 = _getBlockSeekPoint(i) + BLOCK_HEADER_SIZE;

		u8 buf[2][18];
		fseek(recordingFile, seek1, SEEK_SET);
		fread(buf, 1, BLOCK_DATA_SIZE, recordingFile);
		fseek(recordingFile, seek2, SEEK_SET);
		fwrite(buf,1, BLOCK_DATA_SIZE, recordingFile);
	}
	MaxFrame--;
	writeMaxFrame();
	fflush(recordingFile);

	return true;
}

bool InputRecordingFile::InsertPadData(unsigned long frame, const PadData& key)
{
	if (recordingFile == NULL)
		return false;
	if (!key.fExistKey)
		return false;

	for (unsigned long i = MaxFrame - 1; i >= frame; i--)
	{
		long seek1 = _getBlockSeekPoint(i) + BLOCK_HEADER_SIZE;
		long seek2 = _getBlockSeekPoint(i+1) + BLOCK_HEADER_SIZE;

		u8 buf[2][18];
		fseek(recordingFile, seek1, SEEK_SET);
		fread(buf, 1, BLOCK_DATA_SIZE, recordingFile);
		fseek(recordingFile, seek2, SEEK_SET);
		fwrite(buf, 1, BLOCK_DATA_SIZE, recordingFile);
	}
	{
		long seek = _getBlockSeekPoint(frame) + BLOCK_HEADER_SIZE;
		fseek(recordingFile, seek, SEEK_SET);
		fwrite(key.buf, 1, BLOCK_DATA_SIZE, recordingFile);
	}
	MaxFrame++;
	writeMaxFrame();
	fflush(recordingFile);

	return true;
}

bool InputRecordingFile::UpdatePadData(unsigned long frame, const PadData& key)
{
	if (recordingFile == NULL) 
		return false;
	if (!key.fExistKey) 
		return false;

	long seek = _getBlockSeekPoint(frame) + BLOCK_HEADER_SIZE;
	fseek(recordingFile, seek, SEEK_SET);
	if (fwrite(key.buf, 1, BLOCK_DATA_SIZE, recordingFile) == 0) 
		return false;

	fflush(recordingFile);
	return true;
}

// TODO - see if we can get the actual game name, not just the ISO name
// Verify header of recording file
bool InputRecordingFile::readHeaderAndCheck()
{
	if (recordingFile == NULL) return false;
	rewind(recordingFile);
	if (fread(&header, sizeof(InputRecordingHeader), 1, recordingFile) != 1) return false;
	if (fread(&MaxFrame, 4, 1, recordingFile) != 1) return false;
	if (fread(&UndoCount, 4, 1, recordingFile) != 1) return false;
	if (fread(&savestate.fromSavestate, sizeof(bool), 1, recordingFile) != 1) return false;
	if (savestate.fromSavestate) {
		// TODO - check to see if the file is there, if it AINT, return false, throw an error, ETC (SAY WHAT FILE WE ARE LOOKING FOR)
		StateCopy_LoadFromFile(filename + "_SaveState.p2s");
	}
	else {
		sApp.SysExecute();
	}

	// Check for current verison
	// TODO - more specific log if fails for this reason
	if (header.version != 1) {
		return false;
	}
	return true;
}
bool InputRecordingFile::writeHeader()
{
	if (recordingFile == NULL) return false;
	rewind(recordingFile);
	if (fwrite(&header, sizeof(InputRecordingHeader), 1, recordingFile) != 1) return false;
	return true;
}

bool InputRecordingFile::writeMaxFrame()
{
	if (recordingFile == NULL)return false;
	fseek(recordingFile, SEEKPOINT_FRAMEMAX, SEEK_SET);
	if (fwrite(&MaxFrame, 4, 1, recordingFile) != 1) return false;
	return true;
}

void InputRecordingFile::updateFrameMax(unsigned long frame)
{
	if (MaxFrame >= frame) {
		return;
	}
	MaxFrame = frame;
	if (recordingFile == NULL)return ;
	fseek(recordingFile, SEEKPOINT_FRAMEMAX, SEEK_SET);
	fwrite(&MaxFrame, 4, 1, recordingFile);
}

void InputRecordingFile::addUndoCount()
{
	UndoCount++;
	if (recordingFile == NULL)
		return;
	fseek(recordingFile, SEEKPOINT_UNDOCOUNT, SEEK_SET);
	fwrite(&UndoCount, 4, 1, recordingFile);
}

void InputRecordingHeader::setAuthor(wxString _author)
{
	int max = ArraySize(author) - 1;
	strncpy(author, _author.c_str(), max);
	author[max] = 0;
}

void InputRecordingHeader::setGameName(wxString _gameName)
{
	int max = ArraySize(gameName) - 1;
	strncpy(gameName, _gameName.c_str(), max);
	gameName[max] = 0;
}

void InputRecordingHeader::init()
{
	memset(author, 0, ArraySize(author));
	memset(gameName, 0, ArraySize(gameName));
}
