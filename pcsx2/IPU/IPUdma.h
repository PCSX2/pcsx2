// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "IPU.h"

struct IPUDMAStatus {
	bool InProgress;
	bool DMAFinished;
};

struct IPUStatus {
	bool DataRequested;
	bool WaitingOnIPUFrom;
	bool WaitingOnIPUTo;
};

extern void ipuCMDProcess();
extern void ipu0Interrupt();
extern void ipu1Interrupt();

extern void dmaIPU0();
extern void dmaIPU1();
extern void IPU0dma();
extern void IPU1dma();

extern void ipuDmaReset();
extern IPUDMAStatus IPU1Status;
extern IPUStatus IPUCoreStatus;
