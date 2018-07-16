#include "PrecompiledHeader.h"

#include "MemoryTypes.h"
#include "App.h"
#include "Common.h"
#include "Counters.h"

#include "InputRecordingFile.h"

#define HEADER_SIZE (sizeof(InputRecordingHeader)+4+4)
#define SAVESTATE_HEADER_SIZE (sizeof(bool) + sizeof(savestate.savestatesize) + sizeof(savestate.savestate[0]) * savestate.savestatesize)
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

// Temporary Shim, probably a better cross-platform way to correct usages of fopen_s
int fopen_s(FILE **f, const char *name, const char *mode) {
	int ret = 0;
	assert(f);
	*f = fopen(name, mode);
	if (!*f)
		ret = errno;
	return ret;
}

//----------------------------------
// file
//----------------------------------
bool InputRecordingFile::Open(const wxString fn, bool fNewOpen, VmStateBuffer *ss)
{
	Close();
	wxString mode = L"rb+";
	if (fNewOpen) {
		mode = L"wb+";
		MaxFrame = 0;
		UndoCount = 0;
		header.init();
	}
	if ( fopen_s(&fp, fn.ToAscii(), mode.c_str()) != 0)
	{
		recordingConLog(wxString::Format("[REC]: Movie file opening failed. Error - %s\n", strerror(errno)));
		return false;
	}
	filename = fn;

	if (fNewOpen) {
		if (ss) {
			savestate.fromSavestate = true;
			savestate.savestatesize = ss->GetLength();
			savestate.savestate.MakeRoomFor(ss->GetLength());
			for (size_t i = 0; i < ss->GetLength(); i++) {
				savestate.savestate[i] = (*ss)[i];
			}
		}
		else {
			sApp.SysExecute();
		}
	}
	return true;
}
bool InputRecordingFile::Close()
{
	if (fp == NULL)return false;
	writeHeader();
	writeSavestate();
	fclose(fp);
	fp = NULL;
	filename = "";
	return true;
}

//----------------------------------
// write frame
//----------------------------------
bool InputRecordingFile::writeKeyBuf(const uint & frame, const uint port, const uint bufIndex, const u8 & buf)
{
	if (fp == NULL)return false;

	long seek = _getBlockSeekPoint(frame) + BLOCK_HEADER_SIZE + 18 * port + bufIndex;
	if (fseek(fp, seek, SEEK_SET) != 0){
		return false;
	}
	if (fwrite(&buf, 1, 1, fp) != 1) {
		return false;
	}
	fflush(fp);
	return true;
}

//----------------------------------
// read frame
//----------------------------------
bool InputRecordingFile::readKeyBuf(u8 & result,const uint & frame, const uint port, const uint  bufIndex)
{
	if (fp == NULL)return false;

	long seek = _getBlockSeekPoint(frame) + BLOCK_HEADER_SIZE + 18 * port + bufIndex;
	if (fseek(fp, seek, SEEK_SET) != 0) {
		return false;
	}
	if (fread(&result, 1, 1, fp) != 1) {
		return false;
	}
	return true;
}




//===================================
// pad
//===================================
void InputRecordingFile::getPadData(PadData & result, unsigned long frame)
{
	result.fExistKey = false;
	if (fp == NULL)return;
	long seek = _getBlockSeekPoint(frame) + BLOCK_HEADER_SIZE;
	if (fseek(fp, seek, SEEK_SET) != 0)return;
	if (fread(result.buf, 1, BLOCK_DATA_SIZE, fp) == 0)return;
	result.fExistKey = true;
}
bool InputRecordingFile::DeletePadData(unsigned long frame)
{
	if (fp == NULL)return false;

	for (unsigned long i = frame; i < MaxFrame - 1; i++)
	{
		long seek1 = _getBlockSeekPoint(i+1) + BLOCK_HEADER_SIZE;
		long seek2 = _getBlockSeekPoint(i) + BLOCK_HEADER_SIZE;

		u8 buf[2][18];
		fseek(fp, seek1, SEEK_SET);
		fread(buf, 1, BLOCK_DATA_SIZE, fp);
		fseek(fp, seek2, SEEK_SET);
		fwrite(buf,1, BLOCK_DATA_SIZE, fp);
	}
	MaxFrame--;
	writeMaxFrame();
	fflush(fp);

	return true;
}
bool InputRecordingFile::InsertPadData(unsigned long frame, const PadData& key)
{
	if (fp == NULL)return false;
	if (!key.fExistKey)return false;

	for (unsigned long i = MaxFrame - 1; i >= frame; i--)
	{
		long seek1 = _getBlockSeekPoint(i) + BLOCK_HEADER_SIZE;
		long seek2 = _getBlockSeekPoint(i+1) + BLOCK_HEADER_SIZE;

		u8 buf[2][18];
		fseek(fp, seek1, SEEK_SET);
		fread(buf, 1, BLOCK_DATA_SIZE, fp);
		fseek(fp, seek2, SEEK_SET);
		fwrite(buf, 1, BLOCK_DATA_SIZE, fp);
	}
	{
		long seek = _getBlockSeekPoint(frame) + BLOCK_HEADER_SIZE;
		fseek(fp, seek, SEEK_SET);
		fwrite(key.buf, 1, BLOCK_DATA_SIZE, fp);
	}
	MaxFrame++;
	writeMaxFrame();
	fflush(fp);

	return true;
}
bool InputRecordingFile::UpdatePadData(unsigned long frame, const PadData& key)
{
	if (fp == NULL)return false;
	if (!key.fExistKey)return false;

	long seek = _getBlockSeekPoint(frame) + BLOCK_HEADER_SIZE;
	fseek(fp, seek, SEEK_SET);
	if (fwrite(key.buf, 1, BLOCK_DATA_SIZE, fp) == 0)return false;

	fflush(fp);
	return true;
}




//===================================
// header
//===================================
bool InputRecordingFile::readHeaderAndCheck()
{
	if (fp == NULL)return false;
	rewind(fp);
	if (fread(&header, sizeof(InputRecordingHeader), 1, fp) != 1)return false;
	if (fread(&MaxFrame, 4, 1, fp) != 1)return false;
	if (fread(&UndoCount, 4, 1, fp) != 1)return false;
	if (fread(&savestate.fromSavestate, sizeof(bool), 1, fp) != 1) return false;
	if (savestate.fromSavestate) {
		// We read the size (and the savestate) only if we must
		if (fread(&savestate.savestatesize, sizeof(savestate.savestatesize), 1, fp) != 1) return false;
		if (savestate.savestatesize == 0) {
			recordingConLog(L"[REC]: Invalid size of the savestate.\n");
			return false;
		}

		savestate.savestate.MakeRoomFor(savestate.savestatesize);
		// We read "savestatesize" * the size of a cell
		if (fread(savestate.savestate.GetPtr(), sizeof(savestate.savestate[0]), savestate.savestatesize, fp)
			!= savestate.savestatesize) return false;

		// We load the savestate
		memLoadingState load(savestate.savestate);
		UI_DisableSysActions();
		GetCoreThread().Pause();
		SysClearExecutionCache();
		load.FreezeAll();
		GetCoreThread().Resume();
	}
	else {
		sApp.SysExecute();
	}

	// ID
	if (header.ID != 0xCC) {
		return false;
	}
	// ver
	if (header.version != 3) {
		return false;
	}
	return true;
}
bool InputRecordingFile::writeHeader()
{
	if (fp == NULL)return false;
	rewind(fp);
	if (fwrite(&header, sizeof(InputRecordingHeader), 1, fp) != 1) return false;
	return true;
}
bool InputRecordingFile::writeSavestate()
{
	if (fp == NULL) return false;
	fseek(fp, SEEKPOINT_SAVESTATE, SEEK_SET);
	if (fwrite(&savestate.fromSavestate, sizeof(bool), 1, fp) != 1) return false;

	if (savestate.fromSavestate) {
		if (fwrite(&savestate.savestatesize, sizeof(savestate.savestatesize), 1, fp) != 1) return false;
		if (fwrite(savestate.savestate.GetPtr(), sizeof(savestate.savestate[0]), savestate.savestatesize, fp)
			!= savestate.savestatesize) return false;
	}
	return true;
}
bool InputRecordingFile::writeMaxFrame()
{
	if (fp == NULL)return false;
	fseek(fp, SEEKPOINT_FRAMEMAX, SEEK_SET);
	if (fwrite(&MaxFrame, 4, 1, fp) != 1) return false;
	return true;
}
void InputRecordingFile::updateFrameMax(unsigned long frame)
{
	if (MaxFrame >= frame) {
		return;
	}
	MaxFrame = frame;
	if (fp == NULL)return ;
	fseek(fp, SEEKPOINT_FRAMEMAX, SEEK_SET);
	fwrite(&MaxFrame, 4, 1, fp);
}
void InputRecordingFile::addUndoCount()
{
	UndoCount++;
	if (fp == NULL)return;
	fseek(fp, SEEKPOINT_UNDOCOUNT, SEEK_SET);
	fwrite(&UndoCount, 4, 1, fp);

}

void InputRecordingHeader::setAuthor(wxString _author)
{
	int max = ArraySize(author) - 1;
	strncpy(author, _author.c_str(), max);
	author[max] = 0;
}
void InputRecordingHeader::setCdrom(wxString _cdrom)
{
	int max = ArraySize(cdrom) - 1;
	strncpy(cdrom, _cdrom.c_str(), max);
	cdrom[max] = 0;
}
void InputRecordingHeader::init()
{
	memset(author, 0, ArraySize(author));
	memset(cdrom, 0, ArraySize(cdrom));
}
