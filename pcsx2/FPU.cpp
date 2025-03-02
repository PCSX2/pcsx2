// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Common.h"
#include "PS2Float.h"
#include <cmath>

// Helper Macros
//****************************************************************

// IEEE 754 Values
#define PosInfinity 0x7f800000
#define NegInfinity 0xff800000
#define posFmax 0x7F7FFFFF
#define negFmax 0xFF7FFFFF


/*	Used in compare function to compensate for differences between IEEE 754 and the FPU.
	Setting it to ~0x00000000 = Compares Exact Value. (comment out this macro for faster Exact Compare method)
	Setting it to ~0x00000001 = Discards the least significant bit when comparing.
	Setting it to ~0x00000003 = Discards the least 2 significant bits when comparing... etc..  */
//#define comparePrecision ~0x00000001

// Operands
#define _Ft_         ( ( cpuRegs.code >> 16 ) & 0x1F )
#define _Fs_         ( ( cpuRegs.code >> 11 ) & 0x1F )
#define _Fd_         ( ( cpuRegs.code >>  6 ) & 0x1F )

// Floats
#define _FtValf_     fpuRegs.fpr[ _Ft_ ].f
#define _FsValf_     fpuRegs.fpr[ _Fs_ ].f
#define _FdValf_     fpuRegs.fpr[ _Fd_ ].f
#define _FAValf_     fpuRegs.ACC.f

// U32's
#define _FtValUl_    fpuRegs.fpr[ _Ft_ ].UL
#define _FsValUl_    fpuRegs.fpr[ _Fs_ ].UL
#define _FdValUl_    fpuRegs.fpr[ _Fd_ ].UL
#define _FAValUl_    fpuRegs.ACC.UL

// S32's - useful for ensuring sign extension when needed.
#define _FtValSl_    fpuRegs.fpr[ _Ft_ ].SL
#define _FsValSl_    fpuRegs.fpr[ _Fs_ ].SL
#define _FdValSl_    fpuRegs.fpr[ _Fd_ ].SL
#define _FAValSl_    fpuRegs.ACC.SL

// FPU Control Reg (FCR31)
#define _ContVal_    fpuRegs.fprc[ 31 ]

// FCR31 Flags
#define FPUflagC	0X00800000
#define FPUflagI	0X00020000
#define FPUflagD	0X00010000
#define FPUflagO	0X00008000
#define FPUflagU	0X00004000
#define FPUflagSI	0X00000040
#define FPUflagSD	0X00000020
#define FPUflagSO	0X00000010
#define FPUflagSU	0X00000008

//****************************************************************

// If we have an infinity value, then Overflow has occured.
bool checkOverflow(u32& xReg, u32 cFlagsToSet)
{
	if (CHECK_FPU_SOFT_ADDSUB || CHECK_FPU_SOFT_MULDIV || CHECK_FPU_SOFT_SQRT)
	{
		if (xReg == PS2Float::MAX_FLOATING_POINT_VALUE || xReg == PS2Float::MIN_FLOATING_POINT_VALUE)
		{
			_ContVal_ |= (cFlagsToSet);
			return true;
		}
		else if (cFlagsToSet & FPUflagO)
			_ContVal_ &= ~FPUflagO;
	}
	else
	{
		if ((xReg & ~0x80000000) == PosInfinity)
		{
			/*Console.Warning( "FPU OVERFLOW!: Changing to +/-Fmax!!!!!!!!!!!!\n" );*/
			xReg = (xReg & 0x80000000) | posFmax;
			_ContVal_ |= (cFlagsToSet);
			return true;
		}
		else if (cFlagsToSet & FPUflagO)
			_ContVal_ &= ~FPUflagO;
	}

	return false;
}

// If we have a denormal value, then Underflow has occured.
bool checkUnderflow(u32& xReg, u32 cFlagsToSet) {

	if (CHECK_FPU_SOFT_ADDSUB || CHECK_FPU_SOFT_MULDIV || CHECK_FPU_SOFT_SQRT)
	{
		if (PS2Float(xReg).IsDenormalized())
		{
			_ContVal_ |= (cFlagsToSet);
			return true;
		}
		else if (cFlagsToSet & FPUflagU)
			_ContVal_ &= ~FPUflagU;
	}
	else
	{
		if (((xReg & 0x7F800000) == 0) && ((xReg & 0x007FFFFF) != 0))
		{
			/*Console.Warning( "FPU UNDERFLOW!: Changing to +/-0!!!!!!!!!!!!\n" );*/
			xReg &= 0x80000000;
			_ContVal_ |= (cFlagsToSet);
			return true;
		}
		else if (cFlagsToSet & FPUflagU)
			_ContVal_ &= ~FPUflagU;
	}

	return false;
}

__fi u32 fp_max(u32 a, u32 b)
{
	return ((s32)a < 0 && (s32)b < 0) ? std::min<s32>(a, b) : std::max<s32>(a, b);
}

__fi u32 fp_min(u32 a, u32 b)
{
	return ((s32)a < 0 && (s32)b < 0) ? std::max<s32>(a, b) : std::min<s32>(a, b);
}

/*	Checks if Divide by Zero will occur. (z/y = x)
	cFlagsToSet1 = Flags to set if (z != 0)
	cFlagsToSet2 = Flags to set if (z == 0)
	( Denormals are counted as "0" )
*/
bool checkDivideByZero(u32& xReg, u32 yDivisorReg, u32 zDividendReg, u32 cFlagsToSet1, u32 cFlagsToSet2) {

	if (CHECK_FPU_SOFT_ADDSUB || CHECK_FPU_SOFT_MULDIV || CHECK_FPU_SOFT_SQRT)
	{
		PS2Float yMatrix = PS2Float(yDivisorReg);
		PS2Float zMatrix = PS2Float(zDividendReg);

		// Check Final Fantasy X controls and Klonoa 2 to test this code, they send a bunch of denormals which are often hack-fixed on the game code.
		if (zMatrix.IsDenormalized() || yMatrix.IsDenormalized())
		{
			_ContVal_ |= 0;
			xReg = PS2Float::SolveDivisionDenormalizedOperation(zMatrix, yMatrix).raw;
			return true;
		}

		if (zMatrix.IsZero())
		{
			bool dividendZero = yMatrix.IsZero();

			_ContVal_ |= dividendZero ? cFlagsToSet2 : cFlagsToSet1;

			bool IsSigned = zMatrix.Sign() ^ yMatrix.Sign();

			if (dividendZero)
				xReg = IsSigned ? PS2Float::MIN_FLOATING_POINT_VALUE : PS2Float::MAX_FLOATING_POINT_VALUE;
			else
			{
				xReg = PS2Float(IsSigned, 0, 0).raw;
			}

			return true;
		}
	}
	else if ((yDivisorReg & 0x7F800000) == 0)
	{
		_ContVal_ |= ((zDividendReg & 0x7F800000) == 0) ? cFlagsToSet2 : cFlagsToSet1;
		xReg = ((yDivisorReg ^ zDividendReg) & 0x80000000) | posFmax;
		return true;
	}

	return false;
}

/*	Clears the "Cause Flags" of the Control/Status Reg
	The "EE Core Users Manual" implies that all the Cause flags are cleared every instruction...
	But, the "EE Core Instruction Set Manual" says that only certain Cause Flags are cleared
	for specific instructions... I'm just setting them to clear when the Instruction Set Manual
	says to... (cottonvibes)
*/
#define clearFPUFlags(cFlags) {  \
	_ContVal_ &= ~( cFlags ) ;  \
}

// Conditional Branch
#define BC1(cond)                               \
   if ( ( _ContVal_ & FPUflagC ) cond 0 ) {   \
      intDoBranch( _BranchTarget_ );            \
   }

// Conditional Branch
#define BC1L(cond)                              \
   if ( ( _ContVal_ & FPUflagC ) cond 0 ) {   \
      intDoBranch( _BranchTarget_ );            \
   } else cpuRegs.pc += 4;

namespace R5900 {
namespace Interpreter {
namespace OpcodeImpl {
namespace COP1 {

//****************************************************************
// FPU Opcodes
//****************************************************************

float fpuDouble(u32 f)
{
	switch(f & 0x7f800000){
		case 0x0:
			f &= 0x80000000;
			return *(float*)&f;
			break;
		case 0x7f800000:
			f = (f & 0x80000000)|0x7f7fffff;
			return *(float*)&f;
			break;
		default:
			return *(float*)&f;
			break;
	}
}

static __fi u32 fpuAccurateAdd(u32 a, u32 b)
{
	if (CHECK_FPU_SOFT_ADDSUB) return PS2Float(a).Add(PS2Float(b)).raw;

	return std::bit_cast<u32>(fpuDouble(a) + fpuDouble(b));
}

static __fi u32 fpuAccurateSub(u32 a, u32 b)
{
	if (CHECK_FPU_SOFT_ADDSUB) return PS2Float(a).Sub(PS2Float(b)).raw;

	return std::bit_cast<u32>(fpuDouble(a) - fpuDouble(b));
}

static __fi u32 fpuAccurateMul(u32 a, u32 b)
{
	if (CHECK_FPU_SOFT_MULDIV) return PS2Float(a).Mul(PS2Float(b)).raw;

	return std::bit_cast<u32>(fpuDouble(a) * fpuDouble(b));
}

static __fi u32 fpuAccurateDiv(u32 a, u32 b)
{
	if (CHECK_FPU_SOFT_MULDIV) return PS2Float(a).Div(PS2Float(b)).raw;

	return std::bit_cast<u32>(fpuDouble(a) / fpuDouble(b));
}

static __fi void C_cond_S(uint8_t mode)
{
	switch (mode)
	{
		case 0: // ==
			if (CHECK_FPU_SOFT_ADDSUB || CHECK_FPU_SOFT_MULDIV || CHECK_FPU_SOFT_SQRT)
			{
				_ContVal_ = (PS2Float(_FsValUl_).CompareToSign(PS2Float(_FtValUl_)) == 0) ? (_ContVal_ | FPUflagC) : (_ContVal_ & ~FPUflagC); 
			}
			else
			{
				_ContVal_ = (fpuDouble(_FsValUl_) == fpuDouble(_FtValUl_)) ? (_ContVal_ | FPUflagC) : (_ContVal_ & ~FPUflagC); 
			}
			break;
		case 1: // <=
			if (CHECK_FPU_SOFT_ADDSUB || CHECK_FPU_SOFT_MULDIV || CHECK_FPU_SOFT_SQRT)
			{
				int32_t cmpResult = PS2Float(_FsValUl_).CompareToSign(PS2Float(_FtValUl_));
				_ContVal_ = (cmpResult == 0 || cmpResult == -1) ? (_ContVal_ | FPUflagC) : (_ContVal_ & ~FPUflagC);
			}
			else
			{
				_ContVal_ = (fpuDouble(_FsValUl_) <= fpuDouble(_FtValUl_)) ? (_ContVal_ | FPUflagC) : (_ContVal_ & ~FPUflagC);
			}
			break;
		case 2: // <
			if (CHECK_FPU_SOFT_ADDSUB || CHECK_FPU_SOFT_MULDIV || CHECK_FPU_SOFT_SQRT)
			{
				_ContVal_ = (PS2Float(_FsValUl_).CompareToSign(PS2Float(_FtValUl_)) == -1) ? (_ContVal_ | FPUflagC) : (_ContVal_ & ~FPUflagC);
			}
			else
			{
				_ContVal_ = (fpuDouble(_FsValUl_) < fpuDouble(_FtValUl_)) ? (_ContVal_ | FPUflagC) : (_ContVal_ & ~FPUflagC);
			}
			break;
	}
}

void ABS_S() {
	_FdValUl_ = _FsValUl_ & 0x7fffffff;
	clearFPUFlags( FPUflagO | FPUflagU );
}

void ADD_S() {
	_FdValUl_ = fpuAccurateAdd(_FsValUl_, _FtValUl_);
	if (checkOverflow( _FdValUl_, FPUflagO | FPUflagSO)) return;
	checkUnderflow( _FdValUl_, FPUflagU | FPUflagSU);
}

void ADDA_S() {
	_FAValUl_ = fpuAccurateAdd(_FsValUl_, _FtValUl_);
	if (checkOverflow( _FAValUl_, FPUflagO | FPUflagSO)) return;
	checkUnderflow( _FAValUl_, FPUflagU | FPUflagSU);
}

void BC1F() {
	BC1(==);
}

void BC1FL() {
	BC1L(==); // Equal to 0
}

void BC1T() {
	BC1(!=);
}

void BC1TL() {
	BC1L(!=); // different from 0
}

void C_EQ() {
	C_cond_S(0);
}

void C_F() {
	clearFPUFlags( FPUflagC ); //clears C regardless
}

void C_LE() {
	C_cond_S(1);
}

void C_LT() {
	C_cond_S(2);
}

void CFC1() {
	if (!_Rt_) return;

	if (_Fs_ == 31)
		cpuRegs.GPR.r[_Rt_].SD[0] = (s32)fpuRegs.fprc[31];	// force sign extension to 64 bit
	else if (_Fs_ == 0)
		cpuRegs.GPR.r[_Rt_].SD[0] = 0x2E00;
	else
		cpuRegs.GPR.r[_Rt_].SD[0] = 0;
}

void CTC1() {
	if ( _Fs_ != 31 ) return;
	fpuRegs.fprc[_Fs_] = cpuRegs.GPR.r[_Rt_].UL[0];
}

void CVT_S() {
	if (CHECK_FPU_SOFT_ADDSUB || CHECK_FPU_SOFT_MULDIV || CHECK_FPU_SOFT_SQRT)
	{
		_FdValUl_ = PS2Float::Itof(0, _FsValSl_).raw;
	}
	else
	{
		_FdValf_ = (float)_FsValSl_;
		_FdValf_ = fpuDouble(_FdValUl_);
	}
}

void CVT_W() {
	if (CHECK_FPU_SOFT_ADDSUB || CHECK_FPU_SOFT_MULDIV || CHECK_FPU_SOFT_SQRT)
	{
		_FdValSl_ = PS2Float::Ftoi(0, _FsValUl_);
	}
	else
	{
		if ((_FsValUl_ & 0x7F800000) <= 0x4E800000)
		{
			_FdValSl_ = (s32)_FsValf_;
		}
		else if ((_FsValUl_ & 0x80000000) == 0)
		{
			_FdValUl_ = 0x7fffffff;
		}
		else
		{
			_FdValUl_ = 0x80000000;
		}
	}
}

void DIV_S() {
	if (checkDivideByZero( _FdValUl_, _FtValUl_, _FsValUl_, FPUflagD | FPUflagSD, FPUflagI | FPUflagSI)) return;
	_FdValUl_ = fpuAccurateDiv(_FsValUl_, _FtValUl_);
	if (checkOverflow( _FdValUl_, 0)) return;
	checkUnderflow( _FdValUl_, 0);
}

/*	The Instruction Set manual has an overly complicated way of
	determining the flags that are set. Hopefully this shorter
	method provides a similar outcome and is faster. (cottonvibes)
*/
void MADD_S() {
	_FdValUl_ = fpuAccurateAdd(_FAValUl_, fpuAccurateMul(_FsValUl_, _FtValUl_));
	if (checkOverflow( _FdValUl_, FPUflagO | FPUflagSO)) return;
	checkUnderflow( _FdValUl_, FPUflagU | FPUflagSU);
}

void MADDA_S() {
	_FAValUl_ = fpuAccurateAdd(_FAValUl_, fpuAccurateMul(_FsValUl_, _FtValUl_));
	if (checkOverflow( _FAValUl_, FPUflagO | FPUflagSO)) return;
	checkUnderflow( _FAValUl_, FPUflagU | FPUflagSU);
}

void MAX_S() {
	_FdValUl_  = fp_max( _FsValUl_, _FtValUl_ );
	clearFPUFlags( FPUflagO | FPUflagU );
}

void MFC1() {
	if ( !_Rt_ ) return;
	cpuRegs.GPR.r[_Rt_].SD[0] = _FsValSl_;		// sign extension into 64bit
}

void MIN_S() {
	_FdValUl_ = fp_min(_FsValUl_, _FtValUl_);
	clearFPUFlags( FPUflagO | FPUflagU );
}

void MOV_S() {
	_FdValUl_ = _FsValUl_;
}

void MSUB_S() {
	_FdValUl_ = fpuAccurateSub(_FAValUl_, fpuAccurateMul(_FsValUl_, _FtValUl_));
	if (checkOverflow( _FdValUl_, FPUflagO | FPUflagSO)) return;
	checkUnderflow( _FdValUl_, FPUflagU | FPUflagSU);
}

void MSUBA_S() {
	_FAValUl_ = fpuAccurateSub(_FAValUl_, fpuAccurateMul(_FsValUl_, _FtValUl_));
	if (checkOverflow( _FAValUl_, FPUflagO | FPUflagSO)) return;
	checkUnderflow( _FAValUl_, FPUflagU | FPUflagSU);
}

void MTC1() {
	_FsValUl_ = cpuRegs.GPR.r[_Rt_].UL[0];
}

void MUL_S() {
	_FdValUl_ = fpuAccurateMul(_FsValUl_, _FtValUl_);
	if (checkOverflow( _FdValUl_, FPUflagO | FPUflagSO)) return;
	checkUnderflow( _FdValUl_, FPUflagU | FPUflagSU);
}

void MULA_S() {
	_FAValUl_ = fpuAccurateMul(_FsValUl_, _FtValUl_);
	if (checkOverflow( _FAValUl_, FPUflagO | FPUflagSO)) return;
	checkUnderflow( _FAValUl_, FPUflagU | FPUflagSU);
}

void NEG_S() {
	_FdValUl_  = (_FsValUl_ ^ 0x80000000);
	clearFPUFlags( FPUflagO | FPUflagU );
}

void RSQRT_S() {
	FPRreg temp;
	clearFPUFlags(FPUflagD | FPUflagI);

	if (CHECK_FPU_SOFT_SQRT)
	{
		PS2Float value = PS2Float(_FtValUl_);

		if (value.IsDenormalized())
		{
			_ContVal_ |= FPUflagD | FPUflagSD;
			_FdValUl_ = value.Sign() ? PS2Float::MIN_FLOATING_POINT_VALUE : PS2Float::MAX_FLOATING_POINT_VALUE;
			return;
		}
		else if (_FtValUl_ & 0x80000000) // Ft is negative
		{
			_ContVal_ |= FPUflagI | FPUflagSI;
			_FdValUl_ = PS2Float(_FsValUl_).Rsqrt(PS2Float(value.Abs())).raw;
		}
		else // Ft is positive and not zero
		{
			_FdValUl_ = PS2Float(_FsValUl_).Rsqrt(value).raw;
		}
	}
	else
	{
		if ((_FtValUl_ & 0x7F800000) == 0) // Ft is zero (Denormals are Zero)
		{
			_ContVal_ |= FPUflagD | FPUflagSD;
			_FdValUl_ = (_FtValUl_ & 0x80000000) | posFmax;
			return;
		}
		else if (_FtValUl_ & 0x80000000) // Ft is negative
		{
			_ContVal_ |= FPUflagI | FPUflagSI;
			temp.f = sqrt(fabs(fpuDouble(_FtValUl_)));
			_FdValf_ = fpuDouble(_FsValUl_) / fpuDouble(temp.UL);
		}
		else // Ft is positive and not zero
		{
			_FdValf_ = fpuDouble(_FsValUl_) / sqrt(fpuDouble(_FtValUl_));
		}
	}

	if (checkOverflow( _FdValUl_, 0)) return;
	checkUnderflow( _FdValUl_, 0);
}

void SQRT_S() {
	clearFPUFlags(FPUflagI | FPUflagD);

	if (CHECK_FPU_SOFT_SQRT)
	{
		PS2Float value = PS2Float(_FtValUl_);

		if (_FtValUl_ & 0x80000000) // If Ft is Negative
		{
			_ContVal_ |= FPUflagI | FPUflagSI;
			_FdValUl_ = PS2Float(value.Abs()).Sqrt().raw;
		}
		else
			_FdValUl_ = value.Sqrt().raw; // If Ft is Positive
	}
	else
	{
		if ((_FtValUl_ & 0x7F800000) == 0) // If Ft = +/-0
			_FdValUl_ = _FtValUl_ & 0x80000000; // result is 0
		else if (_FtValUl_ & 0x80000000) // If Ft is Negative
		{
			_ContVal_ |= FPUflagI | FPUflagSI;
			_FdValf_ = sqrt(fabs(fpuDouble(_FtValUl_)));
		}
		else
			_FdValf_ = sqrt(fpuDouble(_FtValUl_)); // If Ft is Positive
	}
}

void SUB_S() {
	_FdValUl_ = fpuAccurateSub(_FsValUl_, _FtValUl_);
	if (checkOverflow( _FdValUl_, FPUflagO | FPUflagSO)) return;
	checkUnderflow( _FdValUl_, FPUflagU | FPUflagSU);
}

void SUBA_S() {
	_FAValUl_ = fpuAccurateSub(_FsValUl_, _FtValUl_);
	if (checkOverflow( _FAValUl_, FPUflagO | FPUflagSO)) return;
	checkUnderflow( _FAValUl_, FPUflagU | FPUflagSU);
}

}	// End Namespace COP1

/////////////////////////////////////////////////////////////////////
// COP1 (FPU)  Load/Store Instructions

// These are actually EE opcodes but since they're related to FPU registers and such they
// seem more appropriately located here.

void LWC1() {
	u32 addr;
	addr = cpuRegs.GPR.r[_Rs_].UL[0] + (s16)(cpuRegs.code & 0xffff);	// force sign extension to 32bit
	if (addr & 0x00000003) { Console.Error( "FPU (LWC1 Opcode): Invalid Unaligned Memory Address" ); return; }  // Should signal an exception?
	fpuRegs.fpr[_Rt_].UL = memRead32(addr);
}

void SWC1() {
	u32 addr;
	addr = cpuRegs.GPR.r[_Rs_].UL[0] + (s16)(cpuRegs.code & 0xffff);	// force sign extension to 32bit
	if (addr & 0x00000003) { Console.Error( "FPU (SWC1 Opcode): Invalid Unaligned Memory Address" ); return; }  // Should signal an exception?
	memWrite32(addr, fpuRegs.fpr[_Rt_].UL);
}

} } }
