/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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

#include "PrecompiledHeader.h"
#include "Common.h"

#include <cmath>

#define DOUBLE_FPU

// Helper Macros
//****************************************************************

const double d_DMAX = 6.8056469327705772e+38;
const double d_DMIN = 1.1754943508222875e-38;

const u32 u32_FMAX = 0x7FFFFFFFu;
const u32 u32_FMIN = 0x00800000u;

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

// Zero Check
#define IsZeroFloat(reg)  !((reg.UL)     & 0x7F800000)
#define IsZeroDouble(reg) !((reg.UL_[1]) & 0x7FF80000)
#define _FtIsZero_     IsZeroFloat(fpuRegs.fpr[ _Ft_ ])
#define _FsIsZero_     IsZeroFloat(fpuRegs.fpr[ _Fs_ ])
#define _FdIsZero_     IsZeroFloat(fpuRegs.fpr[ _Fd_ ])
#define _FAIsZero_     IsZeroFloat(fpuRegs.ACC)

// Ref
#define _FtRef_     fpuRegs.fpr[ _Ft_ ]
#define _FsRef_     fpuRegs.fpr[ _Fs_ ]
#define _FdRef_     fpuRegs.fpr[ _Fd_ ]
#define _FARef_     fpuRegs.ACC

// Double
#define _FtVald_     fpuRegs.fpr[ _Ft_ ].d
#define _FsVald_     fpuRegs.fpr[ _Fs_ ].d
#define _FdVald_     fpuRegs.fpr[ _Fd_ ].d
#define _FAVald_     fpuRegs.ACC.d

// U64's
#define _FtValUd_    fpuRegs.fpr[ _Ft_ ].UD
#define _FsValUd_    fpuRegs.fpr[ _Fs_ ].UD
#define _FdValUd_    fpuRegs.fpr[ _Fd_ ].UD
#define _FAValUd_    fpuRegs.ACC.UD

// S64's - useful for ensuring sign extension when needed.
#define _FtValSd_    fpuRegs.fpr[ _Ft_ ].SD
#define _FsValSd_    fpuRegs.fpr[ _Fs_ ].SD
#define _FdValSd_    fpuRegs.fpr[ _Fd_ ].SD
#define _FAValSd_    fpuRegs.ACC.SD

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
	if ( (xReg & ~0x80000000) == PosInfinity ) {
		/*Console.Warning( "FPU OVERFLOW!: Changing to +/-Fmax!!!!!!!!!!!!\n" );*/
		xReg = (xReg & 0x80000000) | posFmax;
		_ContVal_ |= (cFlagsToSet);
		return true;
	}

	return false;
}

// If we have a denormal value, then Underflow has occured.
bool checkUnderflow(u32& xReg, u32 cFlagsToSet) {
	if ( ( (xReg & 0x7F800000) == 0 ) && ( (xReg & 0x007FFFFF) != 0 ) ) {
		/*Console.Warning( "FPU UNDERFLOW!: Changing to +/-0!!!!!!!!!!!!\n" );*/
		xReg &= 0x80000000;
		_ContVal_ |= (cFlagsToSet);
		return true;
	}

	return false;
}

/*	Checks if Divide by Zero will occur. (z/y = x)
	cFlagsToSet1 = Flags to set if (z != 0)
	cFlagsToSet2 = Flags to set if (z == 0)
	( Denormals are counted as "0" )
*/
bool checkDivideByZero(u32& xReg, u32 yDivisorReg, u32 zDividendReg, u32 cFlagsToSet1, u32 cFlagsToSet2) {

	if ( (yDivisorReg & 0x7F800000) == 0 ) {
		_ContVal_ |= ( (zDividendReg & 0x7F800000) == 0 ) ? cFlagsToSet2 : cFlagsToSet1;
		xReg = ( (yDivisorReg ^ zDividendReg) & 0x80000000 ) | posFmax;
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

#ifdef comparePrecision
// This compare discards the least-significant bit(s) in order to solve some rounding issues.
	#define C_cond_S(cond) {  \
		FPRreg tempA, tempB;  \
		tempA.UL = _FsValUl_ & comparePrecision;  \
		tempB.UL = _FtValUl_ & comparePrecision;  \
		_ContVal_ = ( ( tempA.f ) cond ( tempB.f ) ) ?  \
					( _ContVal_ | FPUflagC ) :  \
					( _ContVal_ & ~FPUflagC );  \
	}
#else
// Used for Comparing; This compares if the floats are exactly the same.
	#define C_cond_S(cond) {  \
	   _ContVal_ = ( _FsValf_ cond _FtValf_ ) ?  \
				   ( _ContVal_ | FPUflagC ) :  \
				   ( _ContVal_ & ~FPUflagC );  \
	}
#endif

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

static inline void u32_to_reg(FPRreg& reg)
{
#ifdef __x86_64__
	pxAssert(0);
#else
	const u32 exp_correction = ((1023-127)<<20);
	const u32 msb_mask = 0x8FFFFFFF;

	u32 exp  = reg.UL & 0x7F800000;

	if (exp == 0) {
		reg.UL_[1] = reg.UL & 0x80000000;
		reg.UL_[0] = 0;
	} else {
		u32 rot = _rotr(reg.UL, 3);

		reg.UL_[1] = _pdep_u32(rot, msb_mask) + exp_correction;
		reg.UL_[0] = rot & 0xE0000000; // Inject 29 '0' in the mantissa
	}

#if 0
  u32 exman0 = rotate_right(source, 3);
  u32 exman_high = exman0 & 0x0FFFFFFF;
  u32 exman_low = exman0 & 0xE0000000;
  result_high = sign_high + exman_high + EXPONENT_CORRECTION_HIGH;
  result_low = exman_low;

#endif

#if 0
[15:16:13] <gigaherz> u32 sign_high = source&0x80000000;
[15:16:25] <gigaherz> u32 exman_high = (source&0x7fffffff) >> 3;
[15:16:47] <gigaherz> u32 exman_low = (source&0x7fffffff) << 29;
[15:16:57] <gigaherz> u32 result_high = sign_high
[15:17:14] <gigaherz> u32 result_high = sign_high + exman_high + EXPONENT_CORRECTION_HIGH;
[15:17:30] <gigaherz> u32 result_low = exman_low;

	reg.UL_[1] = _pdep_u32(f >> 3, msb_mask); // Inject three '0' in exponent
	reg.UL_[0] = f << 29; // Inject 29 '0' in the mantissa
#endif

#endif
}

static inline u32 reg_to_u32(const FPRreg& reg)
{
#ifdef __x86_64__
	pxAssert(0);
#else
	const u32 exp_correction = ((1023-127)<<20);
	const u32 msb_mask = 0x8FFFFFFF;

	u32 exp  = reg.UL_[1] & 0x7FF80000;

	if (exp == 0) {
		return reg.UL_[1] & 0x80000000;
	} else {
		u32 msb = _pext_u32(reg.UL_[1] - exp_correction, msb_mask) << 3;
		u32 lsb = reg.UL_[0] >> 29;
		return msb | lsb;
	}
#endif
}

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

void upcast_reg(FPRreg& reg)
{
	if (!reg.IsDoubleCached) {
		reg.IsDoubleCached = 1;
		u32_to_reg(reg); // FIXME inline code
	}
}

// FIXME use template for flags
void downcast_reg(FPRreg& reg, u32 flags_to_set)
{
	reg.IsDoubleCached = 1;

	// Save and delete the sign
	u32 sign = reg.UL_[1] & 0x80000000;
	reg.UL_[1] &= ~0x80000000;

	if (IsZeroDouble(reg)) {
		reg.f = 0; // Flush denormal to 0
		reg.UD = 0;
	} else {
		if (reg.d > d_DMAX) { // overflow
			//Console.Warning( "FPU OVERFLOW !!!!!!!!!!!!!" );
			reg.d = d_DMAX;
			reg.UL = u32_FMAX;
			_ContVal_ |= (FPUflagSO | FPUflagO) & flags_to_set;
		} else if (reg.d < d_DMIN) { // underflow
			//Console.Warning( "FPU UNDERFLOW !!!!!!!!!!!!!" );
			reg.UL = 0;
			reg.UD = 0;
			_ContVal_ |= (FPUflagSU | FPUflagU) & flags_to_set;
		} else { // Normal number
			reg.UL = reg_to_u32(reg); // FIXME inline code
		}
	}

	// Restore the sign
	reg.UL_[1] |= sign;
	reg.UL |= sign;
}

void ABS_S() {
#ifdef DOUBLE_FPU
	pxAssert(0);
#else
	_FdValUl_ = _FsValUl_ & 0x7fffffff;
	clearFPUFlags( FPUflagO | FPUflagU );
#endif
}

void ADD_S() {
#ifdef DOUBLE_FPU
	pxAssert(0);
#else
	_FdValf_  = fpuDouble( _FsValUl_ ) + fpuDouble( _FtValUl_ );
	if (checkOverflow( _FdValUl_, FPUflagO | FPUflagSO)) return;
	checkUnderflow( _FdValUl_, FPUflagU | FPUflagSU);
#endif
}

void ADDA_S() {
#ifdef DOUBLE_FPU
	upcast_reg(_FtRef_);
	upcast_reg(_FsRef_);

	_FAVald_  = _FsVald_ + _FtVald_;

	downcast_reg(_FARef_, FPUflagO | FPUflagSO | FPUflagU | FPUflagSU);

#else
	_FAValf_  = fpuDouble( _FsValUl_ ) + fpuDouble( _FtValUl_ );
	if (checkOverflow( _FAValUl_, FPUflagO | FPUflagSO)) return;
	checkUnderflow( _FAValUl_, FPUflagU | FPUflagSU);
#endif
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
#ifdef DOUBLE_FPU
	pxAssert(0);
#else
	C_cond_S(==);
#endif
}

void C_F() {
#ifdef DOUBLE_FPU
	pxAssert(0);
#else
	clearFPUFlags( FPUflagC ); //clears C regardless
#endif
}

void C_LE() {
#ifdef DOUBLE_FPU
	pxAssert(0);
#else
	C_cond_S(<=);
#endif
}

void C_LT() {
#ifdef DOUBLE_FPU
	pxAssert(0);
#else
	C_cond_S(<);
#endif
}

void CFC1() {
	if ( !_Rt_ ) return;

	if (_Fs_ >= 16)
		cpuRegs.GPR.r[_Rt_].SD[0] = (s32)fpuRegs.fprc[31];	// force sign extension to 64 bit
	else
		cpuRegs.GPR.r[_Rt_].SD[0] = (s32)fpuRegs.fprc[0];	// force sign extension to 64 bit
}

void CTC1() {
	if ( _Fs_ < 16 ) return;

	u32 v = cpuRegs.GPR.r[_Rt_].UL[0];
	v &= 0x0083c078; // set always-zero bits
	v |= 0x01000001; // set always-one bits
	fpuRegs.fprc[31] = v;
}

void CVT_S() {
#ifdef DOUBLE_FPU
	pxAssert(0);
#else
	_FdValf_ = (float)_FsValSl_;
	_FdValf_ = fpuDouble( _FdValUl_ );
#endif
}

void CVT_W() {
#ifdef DOUBLE_FPU
	pxAssert(0);
#else
	if ( ( _FsValUl_ & 0x7F800000 ) <= 0x4E800000 ) { _FdValSl_ = (s32)_FsValf_; }
	else if ( ( _FsValUl_ & 0x80000000 ) == 0 ) { _FdValUl_ = 0x7fffffff; }
	else { _FdValUl_ = 0x80000000; }
#endif
}

void DIV_S() {
#ifdef DOUBLE_FPU
	if (_FtIsZero_) { // division by 0
		if (_FsIsZero_) { // but operand is 0
			_ContVal_ |= FPUflagI | FPUflagSI;
		} else {
			_ContVal_ |= FPUflagD | FPUflagSD;
		}

		_FdValUl_ = ( ( _FsValUl_ ^ _FtValUl_) & 0x80000000 ) | u32_FMAX;
		_FdRef_.IsDoubleCached = 0; // Or I can set the max value in double too

		return;
	}

	upcast_reg(_FsRef_);
	upcast_reg(_FtRef_);

	_FdVald_ = _FsVald_ / _FtVald_;

	downcast_reg(_FdRef_, 0);

#else
	if (checkDivideByZero( _FdValUl_, _FtValUl_, _FsValUl_, FPUflagD | FPUflagSD, FPUflagI | FPUflagSI)) return;
	_FdValf_ = fpuDouble( _FsValUl_ ) / fpuDouble( _FtValUl_ );
	if (checkOverflow( _FdValUl_, 0)) return;
	checkUnderflow( _FdValUl_, 0);
#endif
}

/*	The Instruction Set manual has an overly complicated way of
	determining the flags that are set. Hopefully this shorter
	method provides a similar outcome and is faster. (cottonvibes)
*/
void MADD_S() {
#ifdef DOUBLE_FPU
	upcast_reg(_FARef_);
	upcast_reg(_FtRef_);
	upcast_reg(_FsRef_);

	double mult = _FsVald_ * _FtVald_;

	_FdVald_ = mult + _FAVald_;

	downcast_reg(_FdRef_, FPUflagO | FPUflagSO | FPUflagU | FPUflagSU);

#else
	FPRreg temp;
	temp.f = fpuDouble( _FsValUl_ ) * fpuDouble( _FtValUl_ );
	_FdValf_  = fpuDouble( _FAValUl_ ) + fpuDouble( temp.UL );
	if (checkOverflow( _FdValUl_, FPUflagO | FPUflagSO)) return;
	checkUnderflow( _FdValUl_, FPUflagU | FPUflagSU);
#endif
}

void MADDA_S() {
#ifdef DOUBLE_FPU
	upcast_reg(_FARef_);
	upcast_reg(_FtRef_);
	upcast_reg(_FsRef_);

	double mult = _FsVald_ * _FtVald_;

	_FAVald_ = mult + _FAVald_;

	downcast_reg(_FARef_, FPUflagO | FPUflagSO | FPUflagU | FPUflagSU);
#else
	_FAValf_ += fpuDouble( _FsValUl_ ) * fpuDouble( _FtValUl_ );
	if (checkOverflow( _FAValUl_, FPUflagO | FPUflagSO)) return;
	checkUnderflow( _FAValUl_, FPUflagU | FPUflagSU);
#endif
}

void MAX_S() {
#ifdef DOUBLE_FPU
	pxAssert(0);
#else
	_FdValf_  = std::max( _FsValf_, _FtValf_ );
	clearFPUFlags( FPUflagO | FPUflagU );
#endif
}

void MFC1() {
	if ( !_Rt_ ) return;

	cpuRegs.GPR.r[_Rt_].SD[0] = _FsValSl_;		// sign extension into 64bit
}

void MIN_S() {
#ifdef DOUBLE_FPU
	pxAssert(0);
#else
	_FdValf_  = std::min( _FsValf_, _FtValf_ );
	clearFPUFlags( FPUflagO | FPUflagU );
#endif
}

void MOV_S() {
#ifdef DOUBLE_FPU
	_FdRef_ = _FsRef_;
#else
	_FdValUl_ = _FsValUl_;
#endif
}

void MSUB_S() {
#ifdef DOUBLE_FPU
	upcast_reg(_FARef_);
	upcast_reg(_FtRef_);
	upcast_reg(_FsRef_);

	double mult = _FsVald_ * _FtVald_;

	_FdVald_ = _FAVald_ - mult;

	downcast_reg(_FdRef_, FPUflagO | FPUflagSO | FPUflagU | FPUflagSU);
#else
	FPRreg temp;
	temp.f = fpuDouble( _FsValUl_ ) * fpuDouble( _FtValUl_ );
	_FdValf_  = fpuDouble( _FAValUl_ ) - fpuDouble( temp.UL );
	if (checkOverflow( _FdValUl_, FPUflagO | FPUflagSO)) return;
	checkUnderflow( _FdValUl_, FPUflagU | FPUflagSU);
#endif
}

void MSUBA_S() {
#ifdef DOUBLE_FPU
	upcast_reg(_FARef_);
	upcast_reg(_FtRef_);
	upcast_reg(_FsRef_);

	double mult = _FsVald_ * _FtVald_;

	_FAVald_ = _FAVald_ - mult;

	downcast_reg(_FARef_, FPUflagO | FPUflagSO | FPUflagU | FPUflagSU);
#else
	_FAValf_ -= fpuDouble( _FsValUl_ ) * fpuDouble( _FtValUl_ );
	if (checkOverflow( _FAValUl_, FPUflagO | FPUflagSO)) return;
	checkUnderflow( _FAValUl_, FPUflagU | FPUflagSU);
#endif
}

void MTC1() {
	_FsValUl_ = cpuRegs.GPR.r[_Rt_].UL[0];
#ifdef DOUBLE_FPU
	fpuRegs.fpr[_Rs_].IsDoubleCached = 0;
#endif
}

void MUL_S() {
#ifdef DOUBLE_FPU
	upcast_reg(_FtRef_);
	upcast_reg(_FsRef_);

	_FdVald_ = _FsVald_ * _FtVald_;

	downcast_reg(_FdRef_, FPUflagO | FPUflagSO | FPUflagU | FPUflagSU);
#else
	_FdValf_  = fpuDouble( _FsValUl_ ) * fpuDouble( _FtValUl_ );
	if (checkOverflow( _FdValUl_, FPUflagO | FPUflagSO)) return;
	checkUnderflow( _FdValUl_, FPUflagU | FPUflagSU);
#endif
}

void MULA_S() {
#ifdef DOUBLE_FPU
	upcast_reg(_FtRef_);
	upcast_reg(_FsRef_);

	_FAVald_ = _FsVald_ * _FtVald_;

	downcast_reg(_FARef_, FPUflagO | FPUflagSO | FPUflagU | FPUflagSU);
#else
	_FAValf_  = fpuDouble( _FsValUl_ ) * fpuDouble( _FtValUl_ );
	if (checkOverflow( _FAValUl_, FPUflagO | FPUflagSO)) return;
	checkUnderflow( _FAValUl_, FPUflagU | FPUflagSU);
#endif
}

void NEG_S() {
#ifdef DOUBLE_FPU
	pxAssert(0);
#else
	_FdValUl_  = (_FsValUl_ ^ 0x80000000);
	clearFPUFlags( FPUflagO | FPUflagU );
#endif
}

void RSQRT_S() {
#ifdef DOUBLE_FPU
	if (_FtIsZero_) { // division by 0
		_ContVal_ |= FPUflagD | FPUflagSD;
		_FdValUl_ = ( _FsValUl_ & 0x80000000 ) | u32_FMAX;
		_FdRef_.IsDoubleCached = 0; // Or I can set the max value in double too
		return;
	}

	upcast_reg(_FsRef_);
	upcast_reg(_FtRef_);

	if ( _FtValUl_ & 0x80000000 ) { // Negative number
		_ContVal_ |= FPUflagI | FPUflagSI;
	}

	double temp = sqrt( fabs( _FtVald_ ) );

	_FdVald_ = _FsVald_ / temp;

	downcast_reg(_FdRef_, 0);

#else
	FPRreg temp;
	if ( ( _FtValUl_ & 0x7F800000 ) == 0 ) { // Ft is zero (Denormals are Zero)
		_ContVal_ |= FPUflagD | FPUflagSD;
		_FdValUl_ = ( ( _FsValUl_ ^ _FtValUl_ ) & 0x80000000 ) | posFmax;
		return;
	}
	else if ( _FtValUl_ & 0x80000000 ) { // Ft is negative
		_ContVal_ |= FPUflagI | FPUflagSI;
		temp.f = sqrt( fabs( fpuDouble( _FtValUl_ ) ) );
		_FdValf_ = fpuDouble( _FsValUl_ ) / fpuDouble( temp.UL );
	}
	else { _FdValf_ = fpuDouble( _FsValUl_ ) / sqrt( fpuDouble( _FtValUl_ ) ); } // Ft is positive and not zero

	if (checkOverflow( _FdValUl_, 0)) return;
	checkUnderflow( _FdValUl_, 0);
#endif
}

void SQRT_S() {
	// Note spec says that sqrt(-0) == -0 but tests show the contrary. Who trust !
#ifdef DOUBLE_FPU
	upcast_reg(_FtRef_);

	if ( _FtValUl_ & 0x80000000 ) { // Negative number
		_ContVal_ |= FPUflagI | FPUflagSI;
	}

	_FdVald_ = sqrt( fabs( _FtVald_ ) );

	downcast_reg(_FdRef_, 0);

#else
	if ( ( _FtValUl_ & 0x7F800000 ) == 0 ) // If Ft = +/-0
		_FdValUl_ = 0;// result is 0
	else if ( _FtValUl_ & 0x80000000 ) { // If Ft is Negative
		_ContVal_ |= FPUflagI | FPUflagSI;
		_FdValf_ = sqrt( fabs( fpuDouble( _FtValUl_ ) ) );
	} else
		_FdValf_ = sqrt( fpuDouble( _FtValUl_ ) ); // If Ft is Positive
#endif

	clearFPUFlags( FPUflagD );
}

void SUB_S() {
#ifdef DOUBLE_FPU
	pxAssert(0);
#else
	_FdValf_  = fpuDouble( _FsValUl_ ) - fpuDouble( _FtValUl_ );
	if (checkOverflow( _FdValUl_, FPUflagO | FPUflagSO)) return;
	checkUnderflow( _FdValUl_, FPUflagU | FPUflagSU);
#endif
}

void SUBA_S() {
#ifdef DOUBLE_FPU
	pxAssert(0);
#else
	_FAValf_  = fpuDouble( _FsValUl_ ) - fpuDouble( _FtValUl_ );
	if (checkOverflow( _FAValUl_, FPUflagO | FPUflagSO)) return;
	checkUnderflow( _FAValUl_, FPUflagU | FPUflagSU);
#endif
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
#ifdef DOUBLE_FPU
	fpuRegs.fpr[_Rt_].IsDoubleCached = 0;
#endif
}

void SWC1() {
	u32 addr;
	addr = cpuRegs.GPR.r[_Rs_].UL[0] + (s16)(cpuRegs.code & 0xffff);	// force sign extension to 32bit
	if (addr & 0x00000003) { Console.Error( "FPU (SWC1 Opcode): Invalid Unaligned Memory Address" ); return; }  // Should signal an exception?

	memWrite32(addr, fpuRegs.fpr[_Rt_].UL);
}

} } }
