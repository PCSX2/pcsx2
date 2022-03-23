/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 *  Original code from DuckStation by Stenznek
 *  Modified by Weirdbeardgame for PCSX2 emu
 */

#include "PrecompiledHeader.h"
#include "CueParser.h"
#include "common/StringUtil.h"
#include "CDVD.h"
#include <cstdarg>
//Log_SetChannel(CueParser);

namespace CueParser {

static bool TokenMatch(const std::string_view& s1, const char* token)
{
  const size_t token_len = std::strlen(token);
  if (s1.length() != token_len)
    return false;

  return (StringUtil::Strncasecmp(s1.data(), token, token_len) == 0);
}

File::File() = default;

File::~File() = default;

const Track* File::GetTrack(u32 n) const
{
  for (const auto& it : m_tracks)
  {
    if (it.number == n)
      return &it;
  }

  return nullptr;
}

Track* File::GetMutableTrack(u32 n)
{
  for (auto& it : m_tracks)
  {
    if (it.number == n)
      return &it;
  }

  return nullptr;
}

bool File::Parse(std::FILE* fp, Common::Error* error)
{
  char line[1024];
  u32 line_number = 1;
  while (std::fgets(line, sizeof(line), fp))
  {
    if (!ParseLine(line, line_number, error))
      return false;

    line_number++;
  }

  if (!CompleteLastTrack(line_number, error))
    return false;

  if (!SetTrackLengths(line_number, error))
    return false;

  return true;
}

void File::SetError(u32 line_number, Common::Error* error, const char* format, ...)
{
  std::va_list ap;
  //SmallString str;
  va_start(ap, format);
  //str.FormatVA(format, ap);
  va_end(ap);

  //Log_ErrorPrintf("Cue parse error at line %u: %s", line_number, str.GetCharArray());

  //if (error)
    //error->SetFormattedMessage("Cue parse error at line %u: %s", line_number, str.GetCharArray());
}

std::string_view File::GetToken(const char*& line)
{
  std::string_view ret;

  const char* start = line;
  while (std::isspace(*start) && *start != '\0')
    start++;

  if (*start == '\0')
    return ret;

  const char* end;
  const bool quoted = *start == '\"';
  if (quoted)
  {
    start++;
    end = start;
    while (*end != '\"' && *end != '\0')
      end++;

    if (*end != '\"')
      return ret;

    ret = std::string_view(start, static_cast<size_t>(end - start));

    // eat closing "
    end++;
  }
  else
  {
    end = start;
    while (!std::isspace(*end) && *end != '\0')
      end++;

    ret = std::string_view(start, static_cast<size_t>(end - start));
  }

  line = end;
  return ret;
}

std::optional<MSF> File::GetMSF(const std::string_view& token)
{
  static const s32 max_values[] = {std::numeric_limits<s32>::max(), 60, 75};

  u32 parts[3] = {};
  u32 part = 0;

  u32 start = 0;
  for (;;)
  {
    while (start < token.length() && token[start] < '0' && token[start] <= '9')
      start++;

    if (start == token.length())
      return std::nullopt;

    u32 end = start;
    while (end < token.length() && token[end] >= '0' && token[end] <= '9')
      end++;

    const std::optional<s32> value = StringUtil::FromChars<s32>(token.substr(start, end - start));
    if (!value.has_value() || value.value() < 0 || value.value() > max_values[part])
      return std::nullopt;

    parts[part] = static_cast<u32>(value.value());
    part++;

    if (part == 3)
      break;

    while (end < token.length() && std::isspace(token[end]))
      end++;
    if (end == token.length() || token[end] != ':')
      return std::nullopt;

    start = end + 1;
  }

  MSF ret;
  ret.trackM = static_cast<u8>(parts[0]);
  ret.trackS = static_cast<u8>(parts[1]);
  ret.trackF = static_cast<u8>(parts[2]);
  return ret;
}

bool File::ParseLine(const char* line, u32 line_number, Common::Error* error)
{
  const std::string_view command(GetToken(line));
  if (command.empty())
    return true;

  if (TokenMatch(command, "REM"))
  {
    // comment, eat it
    return true;
  }

  if (TokenMatch(command, "FILE"))
    return HandleFileCommand(line, line_number, error);
  else if (TokenMatch(command, "TRACK"))
    return HandleTrackCommand(line, line_number, error);
  else if (TokenMatch(command, "INDEX"))
    return HandleIndexCommand(line, line_number, error);
  else if (TokenMatch(command, "PREGAP"))
    return HandlePregapCommand(line, line_number, error);
  else if (TokenMatch(command, "FLAGS"))
    return HandleFlagCommand(line, line_number, error);

  if (TokenMatch(command, "POSTGAP"))
  {
    //Log_WarningPrintf("Ignoring '%*s' command", static_cast<int>(command.size()), command.data());
    return true;
  }

  // stuff we definitely ignore
  if (TokenMatch(command, "CATALOG") || TokenMatch(command, "CDTEXTFILE") || TokenMatch(command, "ISRC") ||
      TokenMatch(command, "TRACK_ISRC") || TokenMatch(command, "TITLE") || TokenMatch(command, "PERFORMER") ||
      TokenMatch(command, "SONGWRITER") || TokenMatch(command, "COMPOSER") || TokenMatch(command, "ARRANGER") ||
      TokenMatch(command, "MESSAGE") || TokenMatch(command, "DISC_ID") || TokenMatch(command, "GENRE") ||
      TokenMatch(command, "TOC_INFO1") || TokenMatch(command, "TOC_INFO2") || TokenMatch(command, "UPC_EAN") ||
      TokenMatch(command, "SIZE_INFO"))
  {
    return true;
  }

  SetError(line_number, error, "Invalid command '%*s'", static_cast<int>(command.size()), command.data());
  return false;
}

bool File::HandleFileCommand(const char* line, u32 line_number, Common::Error* error)
{
  const std::string_view filename(GetToken(line));
  const std::string_view mode(GetToken(line));

  if (filename.empty())
  {
    SetError(line_number, error, "Missing filename");
    return false;
  }

  if (!TokenMatch(mode, "BINARY"))
  {
    SetError(line_number, error, "Only BINARY modes are supported");
    return false;
  }

  m_current_file = filename;
  //Log_DebugPrintf("File '%s'", m_current_file->c_str());
  return true;
}

bool File::HandleTrackCommand(const char* line, u32 line_number, Common::Error* error)
{
  if (!CompleteLastTrack(line_number, error))
    return false;

  if (!m_current_file.has_value())
  {
    SetError(line_number, error, "Starting a track declaration without a file set");
    return false;
  }

  const std::string_view track_number_str(GetToken(line));
  if (track_number_str.empty())
  {
    SetError(line_number, error, "Missing track number");
    return false;
  }

  const std::optional<s32> track_number = StringUtil::FromChars<s32>(track_number_str);
  if (track_number.value_or(0) < MIN_TRACK_NUMBER || track_number.value_or(0) > MAX_TRACK_NUMBER)
  {
    SetError(line_number, error, "Invalid track number %d", track_number.value_or(0));
    return false;
  }

  const std::string_view mode_str = GetToken(line);
  cdvdTD mode;
  if (TokenMatch(mode_str, "AUDIO"))
    mode.type = CDVD_MODE_2352;
  else if (TokenMatch(mode_str, "MODE1/2048"))
    mode.type = CDVD_MODE_2048;
  else if (TokenMatch(mode_str, "MODE1/2352"))
    mode.type = CDVD_MODE_2352;
  else if (TokenMatch(mode_str, "MODE2/2336"))
    mode.type = CDVD_MODE2_TRACK;
  else if (TokenMatch(mode_str, "MODE2/2048"))
    mode.type = CDVD_MODE_2048;
  else if (TokenMatch(mode_str, "MODE2/2342"))
    mode.type = CDVD_MODE2_FORM2;
  else if (TokenMatch(mode_str, "MODE2/2332"))
    mode.type = CDVD_MODE_2332;
  else if (TokenMatch(mode_str, "MODE2/2352"))
    mode.type = CDVD_MODE_2352;
  else
  {
    SetError(line_number, error, "Invalid mode: '%*s'", static_cast<int>(mode_str.length()), mode_str.data());
    return false;
  }

  m_current_track = Track();
  m_current_track->number = static_cast<u32>(track_number.value());
  m_current_track->file = m_current_file.value();
  m_current_track->mode = mode;
  return true;
}

bool File::HandleIndexCommand(const char* line, u32 line_number, Common::Error* error)
{
  if (!m_current_track.has_value())
  {
    SetError(line_number, error, "Setting index without track");
    return false;
  }

  const std::string_view index_number_str(GetToken(line));
  if (index_number_str.empty())
  {
    SetError(line_number, error, "Missing index number");
    return false;
  }

  const std::optional<s32> index_number = StringUtil::FromChars<s32>(index_number_str);
  if (index_number.value_or(-1) < MIN_INDEX_NUMBER || index_number.value_or(-1) > MAX_INDEX_NUMBER)
  {
    SetError(line_number, error, "Invalid index number %d", index_number.value_or(-1));
    return false;
  }

  if (m_current_track->GetIndex(static_cast<u32>(index_number.value())) != nullptr)
  {
    SetError(line_number, error, "Duplicate index %d", index_number.value());
    return false;
  }

  const std::string_view msf_str(GetToken(line));
  if (msf_str.empty())
  {
    SetError(line_number, error, "Missing index location");
    return false;
  }

  const std::optional<MSF> msf(GetMSF(msf_str));
  if (!msf.has_value())
  {
    SetError(line_number, error, "Invalid index location '%*s'", static_cast<int>(msf_str.size()), msf_str.data());
    return false;
  }

  m_current_track->indices.emplace_back(static_cast<u32>(index_number.value()), msf.value());
  return true;
}

bool File::HandlePregapCommand(const char* line, u32 line_number, Common::Error* error)
{
  if (!m_current_track.has_value())
  {
    SetError(line_number, error, "Setting pregap without track");
    return false;
  }

  if (m_current_track->zero_pregap.has_value())
  {
    SetError(line_number, error, "Pregap already specified for track %u", m_current_track->number);
    return false;
  }

  const std::string_view msf_str(GetToken(line));
  if (msf_str.empty())
  {
    SetError(line_number, error, "Missing pregap location");
    return false;
  }

  const std::optional<MSF> msf(GetMSF(msf_str));
  if (!msf.has_value())
  {
    SetError(line_number, error, "Invalid pregap location '%*s'", static_cast<int>(msf_str.size()), msf_str.data());
    return false;
  }

  m_current_track->zero_pregap = std::move(msf);
  return true;
}

bool File::HandleFlagCommand(const char* line, u32 line_number, Common::Error* error)
{
  if (!m_current_track.has_value())
  {
    SetError(line_number, error, "Flags command outside of track");
    return false;
  }

  for (;;)
  {
    const std::string_view token(GetToken(line));
    if (token.empty())
      break;

    if (TokenMatch(token, "PRE"))
      m_current_track->SetFlag(TrackFlag::PreEmphasis);
    else if (TokenMatch(token, "DCP"))
      m_current_track->SetFlag(TrackFlag::CopyPermitted);
    else if (TokenMatch(token, "4CH"))
      m_current_track->SetFlag(TrackFlag::FourChannelAudio);
    else if (TokenMatch(token, "SCMS"))
      m_current_track->SetFlag(TrackFlag::SerialCopyManagement);
  }
    //else
      //Log_WarningPrintf("Unknown track flag '%*s'", static_cast<int>(token.size()), token.data());

  return true;
}

bool File::CompleteLastTrack(u32 line_number, Common::Error* error)
{
  if (!m_current_track.has_value())
    return true;

  const MSF* index1 = m_current_track->GetIndex(1);
  if (!index1)
  {
    SetError(line_number, error, "Track %u is missing index 1", m_current_track->number);
    return false;
  }

  // check indices
  for (const auto& [index_number, index_msf] : m_current_track->indices)
  {
    if (index_number == 0)
      continue;

    const MSF* prev_index = m_current_track->GetIndex(index_number - 1);
    /*if (prev_index && *prev_index > index_msf)
    {
      SetError(line_number, error, "Index %u is after index %u in track %u", index_number - 1, index_number,
               m_current_track->number);
      return false;
    }*/
  }

  const MSF* index0 = m_current_track->GetIndex(0);
  if (index0 && m_current_track->zero_pregap.has_value())
  {
    //Log_WarningPrintf("Zero pregap and index 0 specified in track %u, ignoring zero pregap", m_current_track->number);
    m_current_track->zero_pregap.reset();
  }

  m_current_track->start = *index1;

  m_tracks.push_back(std::move(m_current_track.value()));
  m_current_track.reset();
  return true;
}

bool File::SetTrackLengths(u32 line_number, Common::Error* error)
{
  for (const Track& track : m_tracks)
  {
    if (track.number > 1)
    {
      // set the length of the previous track based on this track's start, if they're the same file
      Track* previous_track = GetMutableTrack(track.number - 1);
      if (previous_track && previous_track->file == track.file)
      {
        /*if (previous_track->start > track.start)
        {
          SetError(line_number, error, "Track %u start greater than track %u start", previous_track->number,
                   track.number);
          return false;
        }*/

        // Use index 0, otherwise index 1.
        const MSF* start_index = track.GetIndex(0);
        if (!start_index)
          start_index = track.GetIndex(1);

          //previous_track->length.emplace((u32)CDVDapi_Disc.readSubQ);

        //previous_track->length = MSF::FromLBA(start_index->ToLBA() - previous_track->start.ToLBA());

      }
    }
  }

  return true;
}

const CueParser::MSF* Track::GetIndex(u32 n) const
{
  for (const auto& it : indices)
  {
    if (it.first == n)
      return &it.second;
  }

  return nullptr;
}

} // namespace CueParser
