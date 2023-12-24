// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "CDVDcommon.h"

#include <memory>
#include <string>
#include <string_view>

class Error;
class ElfObject;
class IsoReader;

#define btoi(b) ((b) / 16 * 10 + (b) % 16) /* BCD to u_char */
#define itob(i) ((i) / 10 * 16 + (i) % 10) /* u_char to BCD */

static __fi s32 msf_to_lsn(const u8* Time) noexcept
{
	u32 lsn;

	lsn = Time[2];
	lsn += (Time[1] - 2) * 75;
	lsn += Time[0] * 75 * 60;
	return lsn;
}

static __fi s32 msf_to_lba(const u8 m, const u8 s, const u8 f) noexcept
{
	u32 lsn;
	lsn = f;
	lsn += (s - 2) * 75;
	lsn += m * 75 * 60;
	return lsn;
}

static __fi void lsn_to_msf(u8* Time, s32 lsn) noexcept
{
	u8 m, s, f;

	lsn += 150;
	m = lsn / 4500;       // minuten
	lsn = lsn - m * 4500; // minuten rest
	s = lsn / 75;         // sekunden
	f = lsn - (s * 75);   // sekunden rest
	Time[0] = itob(m);
	Time[1] = itob(s);
	Time[2] = itob(f);
}

static __fi void lba_to_msf(s32 lba, u8* m, u8* s, u8* f) noexcept
{
	lba += 150;
	*m = lba / (60 * 75);
	*s = (lba / 75) % 60;
	*f = lba % 75;
}

struct cdvdRTC
{
	u8 status;
	u8 second;
	u8 minute;
	u8 hour;
	u8 pad;
	u8 day;
	u8 month;
	u8 year;
};

enum class CDVDDiscType : u8
{
	Other,
	PS1Disc,
	PS2Disc
};

enum TrayStates
{
	CDVD_DISC_ENGAGED,
	CDVD_DISC_DETECTING,
	CDVD_DISC_SEEKING,
	CDVD_DISC_EJECT,
	CDVD_DISC_OPEN
};

struct cdvdTrayTimer
{
	u32 cdvdActionSeconds;
	TrayStates trayState;
};

enum MECHA_STATE
{
	MECHA_STATE_0 = 0,
	MECHA_STATE_READY = 1,
	MECHA_STATE_KEY_INDEXES_SET = 2,
	MECHA_STATE_CARD_IV_SEED_SET = 3,
	MECHA_STATE_CARD_NONCE_SET = 4,
	MECHA_STATE_CARD_CHALLANGE_GENERATED = 5,
	MECHA_STATE_CARD_CHALLENGE12_SENT = 6,
	MECHA_STATE_CARD_CHALLENGE23_SENT = 7,
	MECHA_STATE_CARD_RESPONSE12_RECEIVED = 8,
	MECHA_STATE_CARD_RESPONSE3_RECEIVED = 9,
	MECHA_STATE_CARD_VERIFIED = 10,
	MECHA_STATE_KELF_HEADER_PARAMS_SET = 11,
	MECHA_STATE_KELF_HEADER_RECEIVED = 12,
	MECHA_STATE_KELF_HEADER_VERIFED = 13,
	MECHA_STATE_BIT_LENGTH_SENT = 14,
	MECHA_STATE_KELF_CONTENT_DECRYPT_IN_PROGRESS = 15,
	MECHA_STATE_DATA_IN_LENGTH_SET = 16,
	MECHA_STATE_UNK17 = 17,
	MECHA_STATE_DATA_OUT_LENGTH_SET = 18,
	MECHA_STATE_KELF_CONTENT_RECEIVED = 19,
	MECHA_STATE_KELF_CONTENT_DECRYPT_DONE = 20,
	MECHA_STATE_KBIT1_SENT = 21,
	MECHA_STATE_KBIT2_SENT = 22,
	MECHA_STATE_KC1_SENT = 23,
	MECHA_STATE_KC2_SENT = 24,
	MECHA_STATE_CRYPTO_IV_SET = 25,
	MECHA_STATE_CRYPTO_KEY_RECEIVED = 26,
	MECHA_STATE_CRYPTO_KEYGEN_DONE = 27,
	MECHA_STATE_CRYPTO_DATA_IN_SIZE_SET = 28,
	MECHA_STATE_CRYPTO_CRYPT_DONE = 29,
	MECHA_STATE_CRYPTO_DATA_OUT_SIZE_SET = 30,
	MECHA_STATE_CRYPTO_DATA_RECVED = 31,
};

enum MECHA_RESULT
{
	MECHA_RESULT_0 = 0x0,
	MECHA_RESULT_0x100 = 0x100,
	MECHA_RESULT_CARD_CHALLANGE_GENERATED = 0x204,
	MECHA_RESULT_CARD_VERIFIED = 0x209,
	MECHA_RESULT_KELF_HEADER_VERIFED = 0x20C,
	MECHA_RESULT_KELF_CONTENT_DECRYPTED = 0x213,
	MECHA_RESULT_CRYPTO_KEYGEN_DONE = 0x21A,
	MECHA_RESULT_CRYPTO_CRYPT_DONE = 0x21F,
	MECHA_RESULT_FAILED = 0x300,
};

#pragma pack(push, 1)
struct KELFHeader
{
	uint8_t Nonce[16];
	uint32_t ContentSize; // Sometimes not...
	uint16_t HeaderSize;
	uint8_t SystemType;
	uint8_t ApplicationType;
	uint16_t Flags;
	uint16_t BanCount;
	uint32_t RegionFlags;
};

struct ConsoleBan
{
	uint8_t iLinkID[8];
	uint8_t consoleID[8];
};

#define BIT_BLOCK_ENCRYPTED 1
#define BIT_BLOCK_SIGNED 2

struct BitBlock
{
	uint32_t Size;
	uint32_t Flags;
	uint8_t Signature[8];
};

struct BitTable
{
	uint32_t HeaderSize;
	uint8_t BlockCount;
	uint8_t gap[3];

	struct BitBlock Blocks[256];
};
#pragma pack(pop)


struct BitBlockProccessed
{
	uint8_t Flags;
	uint32_t Size;
	uint8_t Signature[8];
};

struct cdvdStruct
{
	u8 nCommand;
	u8 Ready;
	u8 Error;
	u8 IntrStat;
	u8 Status;
	u8 StatusSticky;
	u8 DiscType;
	u8 sCommand;
	u8 sDataIn;
	u8 sDataOut;
	u8 HowTo;

	u8 NCMDParamBuff[16];
	u8 SCMDParamBuff[16];
	u8 SCMDResultBuff[16];

	u8 NCMDParamCnt;
	u8 NCMDParamPos;
	u8 SCMDParamCnt;
	u8 SCMDParamPos;
	u8 SCMDResultCnt;
	u8 SCMDResultPos;

	u8 CBlockIndex;
	u8 COffset;
	u8 CReadWrite;
	u8 CNumBlocks;

	// Calculates the number of Vsyncs and once it reaches a total number of Vsyncs worth a second with respect to
	// the videomode's vertical frequency, it updates the real time clock.
	int RTCcount;
	cdvdRTC RTC;

	u32 CurrentSector;
	int SectorCnt;
	int SeekCompleted;  // change to bool. --arcum42
	int Reading; // same here.
	int WaitingDMA;
	int ReadMode;
	int BlockSize; // Total bytes transfered at 1x speed
	int Speed;
	int RetryCntMax;
	int CurrentRetryCnt;
	int ReadErr;
	int SpindlCtrl;

	u8 Key[16];
	u8 KeyXor;
	u8 decSet;

	uint8_t icvps2Key[16];
	MECHA_STATE mecha_state;
	MECHA_RESULT mecha_result;
	uint8_t mecha_errorcode;
	uint8_t cardKeySlot;
	uint8_t cardKeyIndex;
	uint8_t mode3KeyIndex;
	uint8_t memcard_iv[8];
	uint8_t memcard_seed[8];
	uint8_t memcard_nonce[8];
	uint8_t memcard_key[16];
	uint8_t memcard_random[8];
	uint8_t memcard_challenge1[8];
	uint8_t memcard_challenge2[8];
	uint8_t memcard_challenge3[8];
	uint8_t memcard_reponse1[8];
	uint8_t memcard_reponse2[8];
	uint8_t memcard_reponse3[8];
	uint8_t CardKey[16][8];
	uint8_t mode;
	uint16_t DataSize;
	uint16_t data_buffer_offset;
	uint8_t data_buffer[0x80000];
	uint16_t bit_length;
	uint16_t data_out_offset;
	uint8_t Kc[16];
	BitTable *bitTablePtr;
	uint8_t *data_out_ptr;
	uint16_t lastBitTable;
	BitBlockProccessed bitBlocks[64];
	uint8_t pub_icvps2[8];
	uint8_t pub_Kbit[16];
	uint8_t pub_Kc[16];
	uint16_t DoneBlocks;
	uint16_t currentBlockIdx;
	KELFHeader verifiedKelfHeader;
	uint8_t ContentLastCiphertext[8];
	uint8_t SignatureLastCiphertext[8];

	u8 TrayTimeout;
	u8 Action;        // the currently scheduled emulated action
	u32 SeekToSector; // Holds the destination sector during seek operations.
	u32 MaxSector;    // Current disc max sector.
	u32 ReadTime;     // Avg. time to read one block of data (in Iop cycles)
	u32 RotSpeed;     // Rotational Speed
	bool Spinning;    // indicates if the Cdvd is spinning or needs a spinup delay
	cdvdTrayTimer Tray;
	u8 nextSectorsBuffered;
	bool AbortRequested;
};

extern cdvdStruct cdvd;

extern void cdvdReadLanguageParams(u8* config);

extern void cdvdReset();
extern void cdvdVsync();
extern void cdvdActionInterrupt();
extern void cdvdSectorReady();
extern void cdvdReadInterrupt();

// We really should not have a function with the exact same name as a callback except for case!
extern void cdvdNewDiskCB();
extern u8 cdvdRead(u8 key);
extern void cdvdWrite(u8 key, u8 rt);

extern void cdvdGetDiscInfo(std::string* out_serial, std::string* out_elf_path, std::string* out_version, u32* out_crc,
	CDVDDiscType* out_disc_type);
extern u32 cdvdGetElfCRC(const std::string& path);
extern bool cdvdLoadElf(ElfObject* elfo, const std::string_view& elfpath, bool isPSXElf, Error* error);
extern bool cdvdLoadDiscElf(ElfObject* elfo, IsoReader& isor, const std::string_view& elfpath, bool isPSXElf, Error* error);

extern s32 cdvdCtrlTrayOpen();
extern s32 cdvdCtrlTrayClose();

