#pragma once
#include "CDVDcommon.h"

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace Common {
class Error;
}

namespace CueParser {

// Defines in CDVDAccess for audio track types
// using TrackMode = CDImage::TrackMode;

// "MSF" would be SetSector in the cdr struct
 //using MSF = cdvdSubQ;

enum : s32
{
  MIN_TRACK_NUMBER = 1,
  MAX_TRACK_NUMBER = 99,
  MIN_INDEX_NUMBER = 0,
  MAX_INDEX_NUMBER = 99
};

enum class TrackFlag : u32
{
  PreEmphasis = (1 << 0),
  CopyPermitted = (1 << 1),
  FourChannelAudio = (1 << 2),
  SerialCopyManagement = (1 << 3),
};

class File
{
public:
  File();
  ~File();

  const cdvdSubQ* GetTrack(u32 n) const;

  bool Parse(std::FILE* fp, Common::Error* error);

private:
  cdvdSubQ* GetMutableTrack(u32 n);
  cdvdTD trackDescriptor;

  void SetError(u32 line_number, Common::Error* error, const char* format, ...);

  static std::string_view GetToken(const char*& line);
  static std::optional<cdvdSubQ> GetMSF(const std::string_view& token);

  bool ParseLine(const char* line, u32 line_number, Common::Error* error);

  bool HandleFileCommand(const char* line, u32 line_number, Common::Error* error);
  bool HandleTrackCommand(const char* line, u32 line_number, Common::Error* error);
  bool HandleIndexCommand(const char* line, u32 line_number, Common::Error* error);
  bool HandlePregapCommand(const char* line, u32 line_number, Common::Error* error);
  bool HandleFlagCommand(const char* line, u32 line_number, Common::Error* error);

  bool CompleteLastTrack(u32 line_number, Common::Error* error);
  bool SetTrackLengths(u32 line_number, Common::Error* error);

  std::optional<std::string> m_current_file;
  std::optional<cdvdSubQ> m_current_track;
};

} // namespace CueParser