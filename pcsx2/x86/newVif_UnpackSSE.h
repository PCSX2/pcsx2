// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "Common.h"
#include "Vif_Dma.h"
#include "newVif.h"

// --------------------------------------------------------------------------------------
//  VifUnpackSSE_Base
// --------------------------------------------------------------------------------------
class VifUnpackSSE_Base
{
public:
	bool usn;    // unsigned flag
	bool doMask; // masking write enable flag
	int  UnpkLoopIteration;
	int  UnpkNoOfIterations;
	int  IsAligned;


protected:
	xAddressVoid dstIndirect;
	xAddressVoid srcIndirect;
	xRegisterSSE zeroReg;
	xRegisterSSE workReg;
	xRegisterSSE destReg;

public:
	VifUnpackSSE_Base();
	virtual ~VifUnpackSSE_Base() = default;

	virtual void xUnpack(int upktype) const;
	virtual bool IsWriteProtectedOp() const = 0;
	virtual bool IsInputMasked() const = 0;
	virtual bool IsUnmaskedOp() const = 0;
	virtual void xMovDest() const;

protected:
	virtual void doMaskWrite(const xRegisterSSE& regX) const = 0;

	virtual void xShiftR(const xRegisterSSE& regX, int n) const;
	virtual void xPMOVXX8(const xRegisterSSE& regX) const;
	virtual void xPMOVXX16(const xRegisterSSE& regX) const;

	virtual void xUPK_S_32() const;
	virtual void xUPK_S_16() const;
	virtual void xUPK_S_8() const;

	virtual void xUPK_V2_32() const;
	virtual void xUPK_V2_16() const;
	virtual void xUPK_V2_8() const;

	virtual void xUPK_V3_32() const;
	virtual void xUPK_V3_16() const;
	virtual void xUPK_V3_8() const;

	virtual void xUPK_V4_32() const;
	virtual void xUPK_V4_16() const;
	virtual void xUPK_V4_8() const;
	virtual void xUPK_V4_5() const;
};

// --------------------------------------------------------------------------------------
//  VifUnpackSSE_Simple
// --------------------------------------------------------------------------------------
class VifUnpackSSE_Simple : public VifUnpackSSE_Base
{
	typedef VifUnpackSSE_Base _parent;

public:
	int curCycle;

public:
	VifUnpackSSE_Simple(bool usn_, bool domask_, int curCycle_);
	virtual ~VifUnpackSSE_Simple() = default;

	virtual bool IsWriteProtectedOp() const { return false; }
	virtual bool IsInputMasked() const { return false; }
	virtual bool IsUnmaskedOp() const { return !doMask; }

protected:
	virtual void doMaskWrite(const xRegisterSSE& regX) const;
};

// --------------------------------------------------------------------------------------
//  VifUnpackSSE_Dynarec
// --------------------------------------------------------------------------------------
class VifUnpackSSE_Dynarec : public VifUnpackSSE_Base
{
	typedef VifUnpackSSE_Base _parent;

public:
	bool isFill;
	int  doMode; // two bit value representing difference mode
	bool skipProcessing;
	bool inputMasked;

protected:
	const nVifStruct& v;   // vif0 or vif1
	const nVifBlock&  vB;  // some pre-collected data from VifStruct
	int               vCL; // internal copy of vif->cl

public:
	VifUnpackSSE_Dynarec(const nVifStruct& vif_, const nVifBlock& vifBlock_);
	VifUnpackSSE_Dynarec(const VifUnpackSSE_Dynarec& src) // copy constructor
		: _parent(src)
		, v(src.v)
		, vB(src.vB)
	{
		isFill = src.isFill;
		vCL    = src.vCL;
	}

	virtual ~VifUnpackSSE_Dynarec() = default;

	virtual bool IsWriteProtectedOp() const { return skipProcessing; }
	virtual bool IsInputMasked() const { return inputMasked; }
	virtual bool IsUnmaskedOp() const { return !doMode && !doMask; }

	void ModUnpack(int upknum, bool PostOp);
	void ProcessMasks();
	void CompileRoutine();


protected:
	virtual void doMaskWrite(const xRegisterSSE& regX) const;
	void SetMasks(int cS) const;
	void writeBackRow() const;

	static VifUnpackSSE_Dynarec FillingWrite(const VifUnpackSSE_Dynarec& src)
	{
		VifUnpackSSE_Dynarec fillingWrite(src);
		fillingWrite.doMask = true;
		fillingWrite.doMode = 0;
		return fillingWrite;
	}
};
