#pragma once

namespace GSDumpReplayer
{
bool IsReplayingDump();

bool Initialize(const char* filename);
void Reset();
void Shutdown();

std::string GetDumpSerial();
u32 GetDumpCRC();

void RenderUI();
}