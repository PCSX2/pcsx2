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
 using MSF = _cdvdSubQ;
// LBA stands for Logical Block Addressing
 using LBA = u32;

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

struct Track
{
  u32 number;
  u32 flags;
  std::string file;
  std::vector<std::pair<u32, MSF>> indices;
  cdvdTD mode;
  MSF start;
  std::optional<u32> length;
  std::optional<MSF> zero_pregap;

  const MSF* GetIndex(u32 n) const;

  inline bool HasFlag(TrackFlag flag) const { return (flags & static_cast<u32>(flag)) != 0; }
  inline void SetFlag(TrackFlag flag) { flags |= static_cast<u32>(flag); }
  inline void RemoveFlag(TrackFlag flag) { flags &= ~static_cast<u32>(flag); }
};

class File
{
public:
  File();
  ~File();

  const Track* GetTrack(u32 n) const;

  bool Parse(std::FILE* fp, Common::Error* error);

private:
  Track* GetMutableTrack(u32 n);

  void SetError(u32 line_number, Common::Error* error, const char* format, ...);

  static std::string_view GetToken(const char*& line);
  static std::optional<MSF> GetMSF(const std::string_view& token);

  bool ParseLine(const char* line, u32 line_number, Common::Error* error);

  bool HandleFileCommand(const char* line, u32 line_number, Common::Error* error);
  bool HandleTrackCommand(const char* line, u32 line_number, Common::Error* error);
  bool HandleIndexCommand(const char* line, u32 line_number, Common::Error* error);
  bool HandlePregapCommand(const char* line, u32 line_number, Common::Error* error);
  bool HandleFlagCommand(const char* line, u32 line_number, Common::Error* error);

  bool CompleteLastTrack(u32 line_number, Common::Error* error);
  bool SetTrackLengths(u32 line_number, Common::Error* error);

  std::vector<Track> m_tracks;
  std::optional<std::string> m_current_file;
  std::optional<Track> m_current_track;
};

} // namespace CueParser