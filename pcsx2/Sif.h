// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

static const int FIFO_SIF_W = 128;

// Despite its name, this is actually the IOP's DMAtag, which itself also contains
// the EE's DMAtag in its upper 64 bits.  Note that only the lower 24 bits of 'data' is
// the IOP's chain transfer address (loaded into MADR).  Bits 30 and 31 are transfer stop
// bits of some sort.
struct sifData
{
	s32 data;
	s32 words;

	tDMA_TAG	tag_lo;		// EE DMA tag
	tDMA_TAG	tag_hi;		// EE DMA tag
};

struct sifFifo
{
	u32 data[FIFO_SIF_W];
	u32 junk[4];
	s32 readPos;
	s32 writePos;
	s32 size;

	s32 sif_free()
	{
		return FIFO_SIF_W - size;
	}

	void write(u32 *from, int words)
	{
		if (words > 0)
		{
			if ((FIFO_SIF_W - size) < words)
				DevCon.Warning("Not enough space in SIF0 FIFO!\n");

			const int wP0 = std::min((FIFO_SIF_W - writePos), words);
			const int wP1 = words - wP0;

			memcpy(&data[writePos], from, wP0 << 2);
			memcpy(&data[0], &from[wP0], wP1 << 2);

			writePos = (writePos + words) & (FIFO_SIF_W - 1);
			size += words;
		}
		SIF_LOG("  SIF + %d = %d (pos=%d)", words, size, writePos);
	}

	// Junk data writing
	// 
	// If there is not enough data produced from the IOP, it will always use the previous full quad word to
	// fill in the missing data.
	// One thing to note, when the IOP transfers the EE tag, it transfers a whole QW of data, which will include
	// the EE Tag and the next IOP tag, since the EE reads 1QW of data for DMA tags.
	//
	// So the data used will be as follows:
	// Less than 1QW = Junk data is made up of the EE tag + address (64 bits) and the following IOP tag (64 bits).
	// More than 1QW = Junk data is made up of the last complete QW of data that was transferred in this packet.
	//
	// Data is always offset in to the junk by the amount the IOP actually transferred, so if it sent 2 words
	// it will read words 3 and 4 out of the junk to fill the space.
	//
	// PS2 test results:
	//
	// Example of less than 1QW being sent with the only data being set being 0x69
	//
	//	addr 0x1500a0 value 0x69        <-- actual data (junk behind this would be the EE tag)
	//	addr 0x1500a4 value 0x1500a0    <-- EE address
	//	addr 0x1500a8 value 0x8001a170  <-- following IOP tag
	//	addr 0x1500ac value 0x10        <-- following IOP tag word count
	//
	// Example of more than 1QW being sent with the data going from 0x20 to 0x25
	//
	//	addr 0x150080 value 0x21 <-- start of previously completed QW
	//	addr 0x150084 value 0x22
	//	addr 0x150088 value 0x23
	//	addr 0x15008c value 0x24 <-- end of previously completed QW
	//	addr 0x150090 value 0x25 <-- end of recorded data
	//	addr 0x150094 value 0x22 <-- from position 2 of the previously completed quadword
	//	addr 0x150098 value 0x23 <-- from position 3 of the previously completed quadword
	//	addr 0x15009c value 0x24 <-- from position 4 of the previously completed quadword

	void writeJunk(int words)
	{
		if (words > 0)
		{
			// Get the start position of the previously completed whole QW.
			// Position is in word (32bit) units.
			const int transferredWords = 4 - words;
			const int prevQWPos = (writePos - (4 + transferredWords)) & (FIFO_SIF_W - 1);

			// Read the old data in to our junk array in case of wrapping.
			const int rP0 = std::min((FIFO_SIF_W - prevQWPos), 4);
			const int rP1 = 4 - rP0;
			memcpy(&junk[0], &data[prevQWPos], rP0 << 2);
			memcpy(&junk[rP0], &data[0], rP1 << 2);

			// Fill the missing words to fill the QW.
			const int wP0 = std::min((FIFO_SIF_W - writePos), words);
			const int wP1 = words - wP0;
			memcpy(&data[writePos], &junk[4- wP0], wP0 << 2);
			memcpy(&data[0], &junk[wP0], wP1 << 2);

			writePos = (writePos + words) & (FIFO_SIF_W - 1);
			size += words;

			SIF_LOG("  SIF + %d = %d Junk (pos=%d)", words, size, writePos);
		}
	}

	void read(u32 *to, int words)
	{
		if (words > 0)
		{
			const int wP0 = std::min((FIFO_SIF_W - readPos), words);
			const int wP1 = words - wP0;

			memcpy(to, &data[readPos], wP0 << 2);
			memcpy(&to[wP0], &data[0], wP1 << 2);

			readPos = (readPos + words) & (FIFO_SIF_W - 1);
			size -= words;
		}
		SIF_LOG("  SIF - %d = %d (pos=%d)", words, size, readPos);
	}
	void clear()
	{
		std::memset(data, 0, sizeof(data));
		readPos = 0;
		writePos = 0;
		size = 0;
	}
};

struct old_sif_structure
{
	sifFifo fifo; // Used in both.
	s32 chain; // Not used.
	s32 end; // Only used for EE.
	s32 tagMode; // No longer used.
	s32 counter; // Used to keep track of how much is left in IOP.
	struct sifData data; // Only used in IOP.
};

struct sif_ee
{
	bool end; // Only used for EE.
	bool busy;

	s32 cycles;
};

struct sif_iop
{
	bool end;
	bool busy;

	s32 cycles;
	s32 writeJunk;

	s32 counter; // Used to keep track of how much is left in IOP.
	struct sifData data; // Only used in IOP.
};

struct _sif
{
	sifFifo fifo; // Used in both.
	sif_ee ee;
	sif_iop iop;
};

extern _sif sif0, sif1, sif2;

extern void sifReset();

extern void SIF0Dma();
extern void SIF1Dma();
extern void SIF2Dma();

extern void dmaSIF0();
extern void dmaSIF1();
extern void dmaSIF2();

extern void EEsif0Interrupt();
extern void EEsif1Interrupt();
extern void EEsif2Interrupt();

extern void sif0Interrupt();
extern void sif1Interrupt();
extern void sif2Interrupt();

extern bool ReadFifoSingleWord();
extern bool WriteFifoSingleWord();

#define sif0data sif0.iop.data.data
#define sif1data sif1.iop.data.data
#define sif2data sif2.iop.data.data

#define sif0words sif0.iop.data.words
#define sif1words sif1.iop.data.words
#define sif2words sif2.iop.data.words

#define sif0tag DMA_TAG(sif0data)
#define sif1tag DMA_TAG(sif1data)
#define sif2tag DMA_TAG(sif2data)
