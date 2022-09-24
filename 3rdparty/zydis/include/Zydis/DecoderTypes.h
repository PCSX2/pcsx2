/***************************************************************************************************

  Zyan Disassembler Library (Zydis)

  Original Author : Florian Bernd

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.

***************************************************************************************************/

/**
 * @file
 * Defines the basic `ZydisDecodedInstruction` and `ZydisDecodedOperand` structs.
 */

#ifndef ZYDIS_INSTRUCTIONINFO_H
#define ZYDIS_INSTRUCTIONINFO_H

#include <Zycore/Types.h>
#include <Zydis/MetaInfo.h>
#include <Zydis/Mnemonic.h>
#include <Zydis/Register.h>
#include <Zydis/SharedTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================================== */
/* Decoded operand                                                                                */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* Operand attributes                                                                             */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Defines the `ZydisOperandAttributes` data-type.
 */
typedef ZyanU8 ZydisOperandAttributes;

/**
 * The operand is a `MULTISOURCE4` register operand.
 *
 * This is a special register operand-type used by `4FMAPS` instructions where the given register
 * points to the first register of a register range (4 registers in total).
 *
 * Example: ZMM3 -> [ZMM3..ZMM6]
 */
#define ZYDIS_OATTRIB_IS_MULTISOURCE4   0x01 // (1 <<  0)

/* ---------------------------------------------------------------------------------------------- */
/* Memory type                                                                                    */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Defines the `ZydisMemoryOperandType` enum.
 */
typedef enum ZydisMemoryOperandType_
{
    ZYDIS_MEMOP_TYPE_INVALID,
    /**
     * Normal memory operand.
     */
    ZYDIS_MEMOP_TYPE_MEM,
    /**
     * The memory operand is only used for address-generation. No real memory-access is
     * caused.
     */
    ZYDIS_MEMOP_TYPE_AGEN,
    /**
     * A memory operand using `SIB` addressing form, where the index register is not used
     * in address calculation and scale is ignored. No real memory-access is caused.
     */
    ZYDIS_MEMOP_TYPE_MIB,

    /**
     * Maximum value of this enum.
     */
    ZYDIS_MEMOP_TYPE_MAX_VALUE = ZYDIS_MEMOP_TYPE_MIB,
    /**
     * The minimum number of bits required to represent all values of this enum.
     */
    ZYDIS_MEMOP_TYPE_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_MEMOP_TYPE_MAX_VALUE)
} ZydisMemoryOperandType;

/* ---------------------------------------------------------------------------------------------- */
/* Decoded operand                                                                                */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Defines the `ZydisDecodedOperand` struct.
 */
typedef struct ZydisDecodedOperand_
{
    /**
     * The operand-id.
     */
    ZyanU8 id;
    /**
     * The type of the operand.
     */
    ZydisOperandType type;
    /**
     * The visibility of the operand.
     */
    ZydisOperandVisibility visibility;
    /**
     * The operand-actions.
     */
    ZydisOperandActions actions;
    /**
     * The operand-encoding.
     */
    ZydisOperandEncoding encoding;
    /**
     * The logical size of the operand (in bits).
     */
    ZyanU16 size;
    /**
     * The element-type.
     */
    ZydisElementType element_type;
    /**
     * The size of a single element.
     */
    ZydisElementSize element_size;
    /**
     * The number of elements.
     */
    ZyanU16 element_count;
    /*
     * Additional operand attributes.
     */
    ZydisOperandAttributes attributes;
    /**
     * Extended info for register-operands.
     */
    struct ZydisDecodedOperandReg_
    {
        /**
         * The register value.
         */
        ZydisRegister value;
        // TODO: AVX512_4VNNIW MULTISOURCE registers
    } reg;
    /**
     * Extended info for memory-operands.
     */
    struct ZydisDecodedOperandMem_
    {
        /**
         * The type of the memory operand.
         */
        ZydisMemoryOperandType type;
        /**
         * The segment register.
         */
        ZydisRegister segment;
        /**
         * The base register.
         */
        ZydisRegister base;
        /**
         * The index register.
         */
        ZydisRegister index;
        /**
         * The scale factor.
         */
        ZyanU8 scale;
        /**
         * Extended info for memory-operands with displacement.
         */
        struct ZydisDecodedOperandMemDisp_
        {
            /**
             * Signals, if the displacement value is used.
             */
            ZyanBool has_displacement;
            /**
             * The displacement value
             */
            ZyanI64 value;
        } disp;
    } mem;
    /**
     * Extended info for pointer-operands.
     */
    struct ZydisDecodedOperandPtr_
    {
        ZyanU16 segment;
        ZyanU32 offset;
    } ptr;
    /**
     * Extended info for immediate-operands.
     */
    struct ZydisDecodedOperandImm_
    {
        /**
         * Signals, if the immediate value is signed.
         */
        ZyanBool is_signed;
        /**
         * Signals, if the immediate value contains a relative offset. You can use
         * `ZydisCalcAbsoluteAddress` to determine the absolute address value.
         */
        ZyanBool is_relative;
        /**
         * The immediate value.
         */
        union ZydisDecodedOperandImmValue_
        {
            ZyanU64 u;
            ZyanI64 s;
        } value;
    } imm;
} ZydisDecodedOperand;

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */
/* Decoded instruction                                                                            */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* Instruction attributes                                                                         */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Defines the `ZydisInstructionAttributes` data-type.
 */
typedef ZyanU64 ZydisInstructionAttributes;

/**
 * The instruction has the `ModRM` byte.
 */
#define ZYDIS_ATTRIB_HAS_MODRM                  0x0000000000000001 // (1 <<  0)
/**
 * The instruction has the `SIB` byte.
 */
#define ZYDIS_ATTRIB_HAS_SIB                    0x0000000000000002 // (1 <<  1)
/**
 * The instruction has the `REX` prefix.
 */
#define ZYDIS_ATTRIB_HAS_REX                    0x0000000000000004 // (1 <<  2)
/**
 * The instruction has the `XOP` prefix.
 */
#define ZYDIS_ATTRIB_HAS_XOP                    0x0000000000000008 // (1 <<  3)
/**
 * The instruction has the `VEX` prefix.
 */
#define ZYDIS_ATTRIB_HAS_VEX                    0x0000000000000010 // (1 <<  4)
/**
 * The instruction has the `EVEX` prefix.
 */
#define ZYDIS_ATTRIB_HAS_EVEX                   0x0000000000000020 // (1 <<  5)
/**
 * The instruction has the `MVEX` prefix.
 */
#define ZYDIS_ATTRIB_HAS_MVEX                   0x0000000000000040 // (1 <<  6)
/**
 * The instruction has one or more operands with position-relative offsets.
 */
#define ZYDIS_ATTRIB_IS_RELATIVE                0x0000000000000080 // (1 <<  7)
/**
 * The instruction is privileged.
 *
 * Privileged instructions are any instructions that require a current ring level below 3.
 */
#define ZYDIS_ATTRIB_IS_PRIVILEGED              0x0000000000000100 // (1 <<  8)

/**
 * The instruction accesses one or more CPU-flags.
 */
#define ZYDIS_ATTRIB_CPUFLAG_ACCESS             0x0000001000000000 // (1 << 36) // TODO: rebase

/**
 * The instruction may conditionally read the general CPU state.
 */
#define ZYDIS_ATTRIB_CPU_STATE_CR               0x0000002000000000 // (1 << 37) // TODO: rebase
/**
 * The instruction may conditionally write the general CPU state.
 */
#define ZYDIS_ATTRIB_CPU_STATE_CW               0x0000004000000000 // (1 << 38) // TODO: rebase
/**
 * The instruction may conditionally read the FPU state (X87, MMX).
 */
#define ZYDIS_ATTRIB_FPU_STATE_CR               0x0000008000000000 // (1 << 39) // TODO: rebase
/**
 * The instruction may conditionally write the FPU state (X87, MMX).
 */
#define ZYDIS_ATTRIB_FPU_STATE_CW               0x0000010000000000 // (1 << 40) // TODO: rebase
/**
 * The instruction may conditionally read the XMM state (AVX, AVX2, AVX-512).
 */
#define ZYDIS_ATTRIB_XMM_STATE_CR               0x0000020000000000 // (1 << 41) // TODO: rebase
/**
 * The instruction may conditionally write the XMM state (AVX, AVX2, AVX-512).
 */
#define ZYDIS_ATTRIB_XMM_STATE_CW               0x0000040000000000 // (1 << 42) // TODO: rebase

/**
 * The instruction accepts the `LOCK` prefix (`0xF0`).
 */
#define ZYDIS_ATTRIB_ACCEPTS_LOCK               0x0000000000000200 // (1 <<  9)
/**
 * The instruction accepts the `REP` prefix (`0xF3`).
 */
#define ZYDIS_ATTRIB_ACCEPTS_REP                0x0000000000000400 // (1 << 10)
/**
 * The instruction accepts the `REPE`/`REPZ` prefix (`0xF3`).
 */
#define ZYDIS_ATTRIB_ACCEPTS_REPE               0x0000000000000800 // (1 << 11)
/**
 * The instruction accepts the `REPE`/`REPZ` prefix (`0xF3`).
 */
#define ZYDIS_ATTRIB_ACCEPTS_REPZ               0x0000000000000800 // (1 << 11)
/**
 * The instruction accepts the `REPNE`/`REPNZ` prefix (`0xF2`).
 */
#define ZYDIS_ATTRIB_ACCEPTS_REPNE              0x0000000000001000 // (1 << 12)
/**
 * The instruction accepts the `REPNE`/`REPNZ` prefix (`0xF2`).
 */
#define ZYDIS_ATTRIB_ACCEPTS_REPNZ              0x0000000000001000 // (1 << 12)
/**
 * The instruction accepts the `BND` prefix (`0xF2`).
 */
#define ZYDIS_ATTRIB_ACCEPTS_BND                0x0000000000002000 // (1 << 13)
/**
 * The instruction accepts the `XACQUIRE` prefix (`0xF2`).
 */
#define ZYDIS_ATTRIB_ACCEPTS_XACQUIRE           0x0000000000004000 // (1 << 14)
/**
 * The instruction accepts the `XRELEASE` prefix (`0xF3`).
 */
#define ZYDIS_ATTRIB_ACCEPTS_XRELEASE           0x0000000000008000 // (1 << 15)
/**
 * The instruction accepts the `XACQUIRE`/`XRELEASE` prefixes (`0xF2`, `0xF3`)
 * without the `LOCK` prefix (`0x0F`).
 */
#define ZYDIS_ATTRIB_ACCEPTS_HLE_WITHOUT_LOCK   0x0000000000010000 // (1 << 16)
/**
 * The instruction accepts branch hints (0x2E, 0x3E).
 */
#define ZYDIS_ATTRIB_ACCEPTS_BRANCH_HINTS       0x0000000000020000 // (1 << 17)
/**
 * The instruction accepts segment prefixes (`0x2E`, `0x36`, `0x3E`, `0x26`,
 * `0x64`, `0x65`).
 */
#define ZYDIS_ATTRIB_ACCEPTS_SEGMENT            0x0000000000040000 // (1 << 18)
/**
 * The instruction has the `LOCK` prefix (`0xF0`).
 */
#define ZYDIS_ATTRIB_HAS_LOCK                   0x0000000000080000 // (1 << 19)
/**
 * The instruction has the `REP` prefix (`0xF3`).
 */
#define ZYDIS_ATTRIB_HAS_REP                    0x0000000000100000 // (1 << 20)
/**
 * The instruction has the `REPE`/`REPZ` prefix (`0xF3`).
 */
#define ZYDIS_ATTRIB_HAS_REPE                   0x0000000000200000 // (1 << 21)
/**
 * The instruction has the `REPE`/`REPZ` prefix (`0xF3`).
 */
#define ZYDIS_ATTRIB_HAS_REPZ                   0x0000000000200000 // (1 << 21)
/**
 * The instruction has the `REPNE`/`REPNZ` prefix (`0xF2`).
 */
#define ZYDIS_ATTRIB_HAS_REPNE                  0x0000000000400000 // (1 << 22)
/**
 * The instruction has the `REPNE`/`REPNZ` prefix (`0xF2`).
 */
#define ZYDIS_ATTRIB_HAS_REPNZ                  0x0000000000400000 // (1 << 22)
/**
 * The instruction has the `BND` prefix (`0xF2`).
 */
#define ZYDIS_ATTRIB_HAS_BND                    0x0000000000800000 // (1 << 23)
/**
 * The instruction has the `XACQUIRE` prefix (`0xF2`).
 */
#define ZYDIS_ATTRIB_HAS_XACQUIRE               0x0000000001000000 // (1 << 24)
/**
 * The instruction has the `XRELEASE` prefix (`0xF3`).
 */
#define ZYDIS_ATTRIB_HAS_XRELEASE               0x0000000002000000 // (1 << 25)
/**
 * The instruction has the branch-not-taken hint (`0x2E`).
 */
#define ZYDIS_ATTRIB_HAS_BRANCH_NOT_TAKEN       0x0000000004000000 // (1 << 26)
/**
 * The instruction has the branch-taken hint (`0x3E`).
 */
#define ZYDIS_ATTRIB_HAS_BRANCH_TAKEN           0x0000000008000000 // (1 << 27)
/**
 * The instruction has a segment modifier.
 */
#define ZYDIS_ATTRIB_HAS_SEGMENT                0x00000003F0000000
/**
 * The instruction has the `CS` segment modifier (`0x2E`).
 */
#define ZYDIS_ATTRIB_HAS_SEGMENT_CS             0x0000000010000000 // (1 << 28)
/**
 * The instruction has the `SS` segment modifier (`0x36`).
 */
#define ZYDIS_ATTRIB_HAS_SEGMENT_SS             0x0000000020000000 // (1 << 29)
/**
 * The instruction has the `DS` segment modifier (`0x3E`).
 */
#define ZYDIS_ATTRIB_HAS_SEGMENT_DS             0x0000000040000000 // (1 << 30)
/**
 * The instruction has the `ES` segment modifier (`0x26`).
 */
#define ZYDIS_ATTRIB_HAS_SEGMENT_ES             0x0000000080000000 // (1 << 31)
/**
 * The instruction has the `FS` segment modifier (`0x64`).
 */
#define ZYDIS_ATTRIB_HAS_SEGMENT_FS             0x0000000100000000 // (1 << 32)
/**
 * The instruction has the `GS` segment modifier (`0x65`).
 */
#define ZYDIS_ATTRIB_HAS_SEGMENT_GS             0x0000000200000000 // (1 << 33)
/**
 * The instruction has the operand-size override prefix (`0x66`).
 */
#define ZYDIS_ATTRIB_HAS_OPERANDSIZE            0x0000000400000000 // (1 << 34) // TODO: rename
/**
 * The instruction has the address-size override prefix (`0x67`).
 */
#define ZYDIS_ATTRIB_HAS_ADDRESSSIZE            0x0000000800000000 // (1 << 35) // TODO: rename
/**
 * The instruction accepts the `CET` `no-track` prefix (`0x3E`).
 */
#define ZYDIS_ATTRIB_ACCEPTS_NOTRACK            0x0000080000000000 // (1 << 43) // TODO: rebase
 /**
  * The instruction has the `CET` `no-track` prefix (`0x3E`).
  */
#define ZYDIS_ATTRIB_HAS_NOTRACK                0x0000100000000000 // (1 << 44) // TODO: rebase

/* ---------------------------------------------------------------------------------------------- */
/* R/E/FLAGS info                                                                                 */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Defines the `ZydisCPUFlags` data-type.
 */
typedef ZyanU32 ZydisCPUFlags;

/**
 * Defines the `ZydisCPUFlag` data-type.
 */
typedef ZyanU8 ZydisCPUFlag;

/**
 * Carry flag.
 */
#define ZYDIS_CPUFLAG_CF     0
/**
 * Parity flag.
 */
#define ZYDIS_CPUFLAG_PF     2
/**
 * Adjust flag.
 */
#define ZYDIS_CPUFLAG_AF     4
/**
 * Zero flag.
 */
#define ZYDIS_CPUFLAG_ZF     6
/**
 * Sign flag.
 */
#define ZYDIS_CPUFLAG_SF     7
/**
 * Trap flag.
 */
#define ZYDIS_CPUFLAG_TF     8
/**
 * Interrupt enable flag.
 */
#define ZYDIS_CPUFLAG_IF     9
/**
 * Direction flag.
 */
#define ZYDIS_CPUFLAG_DF    10
/**
 * Overflow flag.
 */
#define ZYDIS_CPUFLAG_OF    11
/**
 * I/O privilege level flag.
 */
#define ZYDIS_CPUFLAG_IOPL  12
/**
 * Nested task flag.
 */
#define ZYDIS_CPUFLAG_NT    14
/**
 * Resume flag.
 */
#define ZYDIS_CPUFLAG_RF    16
/**
 * Virtual 8086 mode flag.
 */
#define ZYDIS_CPUFLAG_VM    17
/**
 * Alignment check.
 */
#define ZYDIS_CPUFLAG_AC    18
/**
 * Virtual interrupt flag.
 */
#define ZYDIS_CPUFLAG_VIF   19
/**
 * Virtual interrupt pending.
 */
#define ZYDIS_CPUFLAG_VIP   20
/**
 * Able to use CPUID instruction.
 */
#define ZYDIS_CPUFLAG_ID    21

///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * FPU condition-code flag 0.
 *
 * DEPRECATED. This flag is not actually part of `FLAGS/EFLAGS/RFLAGS` and will be removed in the
 * next major release. Please refer to the `fpu_flags_read`/`fpu_flags_written` field instead and
 * use one of the `ZYDIS_FPUFLAG_XXX` masks to check for specific a flag.
 */
#define ZYDIS_CPUFLAG_C0    22
/**
 * FPU condition-code flag 1.
 *
 * DEPRECATED. This flag is not actually part of `FLAGS/EFLAGS/RFLAGS` and will be removed in the
 * next major release. Please refer to the `fpu_flags_read`/`fpu_flags_written` field instead and
 * use one of the `ZYDIS_FPUFLAG_XXX` masks to check for specific a flag.
 */
#define ZYDIS_CPUFLAG_C1    23
/**
 * FPU condition-code flag 2.
 *
 * DEPRECATED. This flag is not actually part of `FLAGS/EFLAGS/RFLAGS` and will be removed in the
 * next major release. Please refer to the `fpu_flags_read`/`fpu_flags_written` field instead and
 * use one of the `ZYDIS_FPUFLAG_XXX` masks to check for specific a flag.
 */
#define ZYDIS_CPUFLAG_C2    24
/**
 * FPU condition-code flag 3.
 *
 * DEPRECATED. This flag is not actually part of `FLAGS/EFLAGS/RFLAGS` and will be removed in the
 * next major release. Please refer to the `fpu_flags_read`/`fpu_flags_written` field instead and
 * use one of the `ZYDIS_FPUFLAG_XXX` masks to check for specific a flag.
 */
#define ZYDIS_CPUFLAG_C3    25

/**
 * DEPRECATED. This define will be removed in the next major release.
 */
#define ZYDIS_CPUFLAG_MAX_VALUE     ZYDIS_CPUFLAG_C3

 ///////////////////////////////////////////////////////////////////////////////////////////////////

 /**
  * Defines the `ZydisFPUFlags` data-type.
  */
typedef ZyanU8 ZydisFPUFlags;

/**
 * FPU condition-code flag 0.
 */
#define ZYDIS_FPUFLAG_C0    0x00 // (1 << 0)
/**
 * FPU condition-code flag 1.
 */
#define ZYDIS_FPUFLAG_C1    0x01 // (1 << 1)
 /**
  * FPU condition-code flag 2.
  */
#define ZYDIS_FPUFLAG_C2    0x02 // (1 << 2)
/**
 * FPU condition-code flag 3.
 */
#define ZYDIS_FPUFLAG_C3    0x04 // (1 << 3)

/**
 * Defines the `ZydisCPUFlagAction` enum.
 *
 * DEPRECATED. This enum will be removed in the next major release.
 */
typedef enum ZydisCPUFlagAction_
{
    /**
     * The CPU flag is not touched by the instruction.
     */
    ZYDIS_CPUFLAG_ACTION_NONE,
    /**
     * The CPU flag is tested (read).
     */
    ZYDIS_CPUFLAG_ACTION_TESTED,
    /**
     * The CPU flag is tested and modified afterwards (read-write).
     */
    ZYDIS_CPUFLAG_ACTION_TESTED_MODIFIED,
    /**
     * The CPU flag is modified (write).
     */
    ZYDIS_CPUFLAG_ACTION_MODIFIED,
    /**
     * The CPU flag is set to 0 (write).
     */
    ZYDIS_CPUFLAG_ACTION_SET_0,
    /**
     * The CPU flag is set to 1 (write).
     */
    ZYDIS_CPUFLAG_ACTION_SET_1,
    /**
     * The CPU flag is undefined (write).
     */
    ZYDIS_CPUFLAG_ACTION_UNDEFINED,

    /**
     * Maximum value of this enum.
     */
    ZYDIS_CPUFLAG_ACTION_MAX_VALUE = ZYDIS_CPUFLAG_ACTION_UNDEFINED,
    /**
     * The minimum number of bits required to represent all values of this enum.
     */
    ZYDIS_CPUFLAG_ACTION_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_CPUFLAG_ACTION_MAX_VALUE)
} ZydisCPUFlagAction;

/* ---------------------------------------------------------------------------------------------- */
/* Branch types                                                                                   */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Defines the `ZydisBranchType` enum.
 */
typedef enum ZydisBranchType_
{
    /**
     * The instruction is not a branch instruction.
     */
    ZYDIS_BRANCH_TYPE_NONE,
    /**
     * The instruction is a short (8-bit) branch instruction.
     */
    ZYDIS_BRANCH_TYPE_SHORT,
    /**
     * The instruction is a near (16-bit or 32-bit) branch instruction.
     */
    ZYDIS_BRANCH_TYPE_NEAR,
    /**
     * The instruction is a far (inter-segment) branch instruction.
     */
    ZYDIS_BRANCH_TYPE_FAR,

    /**
     * Maximum value of this enum.
     */
    ZYDIS_BRANCH_TYPE_MAX_VALUE = ZYDIS_BRANCH_TYPE_FAR,
    /**
     * The minimum number of bits required to represent all values of this enum.
     */
    ZYDIS_BRANCH_TYPE_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_BRANCH_TYPE_MAX_VALUE)
} ZydisBranchType;

/* ---------------------------------------------------------------------------------------------- */
/* SSE/AVX exception-class                                                                        */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Defines the `ZydisExceptionClass` enum.
 */
typedef enum ZydisExceptionClass_
{
    ZYDIS_EXCEPTION_CLASS_NONE,
    // TODO: FP Exceptions
    ZYDIS_EXCEPTION_CLASS_SSE1,
    ZYDIS_EXCEPTION_CLASS_SSE2,
    ZYDIS_EXCEPTION_CLASS_SSE3,
    ZYDIS_EXCEPTION_CLASS_SSE4,
    ZYDIS_EXCEPTION_CLASS_SSE5,
    ZYDIS_EXCEPTION_CLASS_SSE7,
    ZYDIS_EXCEPTION_CLASS_AVX1,
    ZYDIS_EXCEPTION_CLASS_AVX2,
    ZYDIS_EXCEPTION_CLASS_AVX3,
    ZYDIS_EXCEPTION_CLASS_AVX4,
    ZYDIS_EXCEPTION_CLASS_AVX5,
    ZYDIS_EXCEPTION_CLASS_AVX6,
    ZYDIS_EXCEPTION_CLASS_AVX7,
    ZYDIS_EXCEPTION_CLASS_AVX8,
    ZYDIS_EXCEPTION_CLASS_AVX11,
    ZYDIS_EXCEPTION_CLASS_AVX12,
    ZYDIS_EXCEPTION_CLASS_E1,
    ZYDIS_EXCEPTION_CLASS_E1NF,
    ZYDIS_EXCEPTION_CLASS_E2,
    ZYDIS_EXCEPTION_CLASS_E2NF,
    ZYDIS_EXCEPTION_CLASS_E3,
    ZYDIS_EXCEPTION_CLASS_E3NF,
    ZYDIS_EXCEPTION_CLASS_E4,
    ZYDIS_EXCEPTION_CLASS_E4NF,
    ZYDIS_EXCEPTION_CLASS_E5,
    ZYDIS_EXCEPTION_CLASS_E5NF,
    ZYDIS_EXCEPTION_CLASS_E6,
    ZYDIS_EXCEPTION_CLASS_E6NF,
    ZYDIS_EXCEPTION_CLASS_E7NM,
    ZYDIS_EXCEPTION_CLASS_E7NM128,
    ZYDIS_EXCEPTION_CLASS_E9NF,
    ZYDIS_EXCEPTION_CLASS_E10,
    ZYDIS_EXCEPTION_CLASS_E10NF,
    ZYDIS_EXCEPTION_CLASS_E11,
    ZYDIS_EXCEPTION_CLASS_E11NF,
    ZYDIS_EXCEPTION_CLASS_E12,
    ZYDIS_EXCEPTION_CLASS_E12NP,
    ZYDIS_EXCEPTION_CLASS_K20,
    ZYDIS_EXCEPTION_CLASS_K21,
    ZYDIS_EXCEPTION_CLASS_AMXE1,
    ZYDIS_EXCEPTION_CLASS_AMXE2,
    ZYDIS_EXCEPTION_CLASS_AMXE3,
    ZYDIS_EXCEPTION_CLASS_AMXE4,
    ZYDIS_EXCEPTION_CLASS_AMXE5,
    ZYDIS_EXCEPTION_CLASS_AMXE6,

    /**
     * Maximum value of this enum.
     */
    ZYDIS_EXCEPTION_CLASS_MAX_VALUE = ZYDIS_EXCEPTION_CLASS_AMXE6,
    /**
     * The minimum number of bits required to represent all values of this enum.
     */
    ZYDIS_EXCEPTION_CLASS_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_EXCEPTION_CLASS_MAX_VALUE)
} ZydisExceptionClass;

/* ---------------------------------------------------------------------------------------------- */
/* AVX mask mode                                                                                  */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Defines the `ZydisMaskMode` enum.
 */
typedef enum ZydisMaskMode_
{
    ZYDIS_MASK_MODE_INVALID,
    /**
     * Masking is disabled for the current instruction (`K0` register is used).
     */
    ZYDIS_MASK_MODE_DISABLED,
    /**
     * The embedded mask register is used as a merge-mask.
     */
    ZYDIS_MASK_MODE_MERGING,
    /**
     * The embedded mask register is used as a zero-mask.
     */
    ZYDIS_MASK_MODE_ZEROING,
    /**
     * The embedded mask register is used as a control-mask (element selector).
     */
    ZYDIS_MASK_MODE_CONTROL,
    /**
     * The embedded mask register is used as a zeroing control-mask (element selector).
     */
    ZYDIS_MASK_MODE_CONTROL_ZEROING,

    /**
     * Maximum value of this enum.
     */
    ZYDIS_MASK_MODE_MAX_VALUE = ZYDIS_MASK_MODE_CONTROL_ZEROING,
    /**
     * The minimum number of bits required to represent all values of this enum.
     */
    ZYDIS_MASK_MODE_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_MASK_MODE_MAX_VALUE)
} ZydisMaskMode;

/* ---------------------------------------------------------------------------------------------- */
/* AVX broadcast-mode                                                                             */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Defines the `ZydisBroadcastMode` enum.
 */
typedef enum ZydisBroadcastMode_
{
    ZYDIS_BROADCAST_MODE_INVALID,
    ZYDIS_BROADCAST_MODE_1_TO_2,
    ZYDIS_BROADCAST_MODE_1_TO_4,
    ZYDIS_BROADCAST_MODE_1_TO_8,
    ZYDIS_BROADCAST_MODE_1_TO_16,
    ZYDIS_BROADCAST_MODE_1_TO_32,
    ZYDIS_BROADCAST_MODE_1_TO_64,
    ZYDIS_BROADCAST_MODE_2_TO_4,
    ZYDIS_BROADCAST_MODE_2_TO_8,
    ZYDIS_BROADCAST_MODE_2_TO_16,
    ZYDIS_BROADCAST_MODE_4_TO_8,
    ZYDIS_BROADCAST_MODE_4_TO_16,
    ZYDIS_BROADCAST_MODE_8_TO_16,

    /**
     * Maximum value of this enum.
     */
    ZYDIS_BROADCAST_MODE_MAX_VALUE = ZYDIS_BROADCAST_MODE_8_TO_16,
    /**
     * The minimum number of bits required to represent all values of this enum.
     */
    ZYDIS_BROADCAST_MODE_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_BROADCAST_MODE_MAX_VALUE)
} ZydisBroadcastMode;

/* ---------------------------------------------------------------------------------------------- */
/* AVX rounding-mode                                                                              */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Defines the `ZydisRoundingMode` enum.
 */
typedef enum ZydisRoundingMode_
{
    ZYDIS_ROUNDING_MODE_INVALID,
    /**
     * Round to nearest.
     */
    ZYDIS_ROUNDING_MODE_RN,
    /**
     * Round down.
     */
    ZYDIS_ROUNDING_MODE_RD,
    /**
     * Round up.
     */
    ZYDIS_ROUNDING_MODE_RU,
    /**
     * Round towards zero.
     */
    ZYDIS_ROUNDING_MODE_RZ,

    /**
     * Maximum value of this enum.
     */
    ZYDIS_ROUNDING_MODE_MAX_VALUE = ZYDIS_ROUNDING_MODE_RZ,
    /**
     * The minimum number of bits required to represent all values of this enum.
     */
    ZYDIS_ROUNDING_MODE_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_ROUNDING_MODE_MAX_VALUE)
} ZydisRoundingMode;

/* ---------------------------------------------------------------------------------------------- */
/* KNC swizzle-mode                                                                               */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Defines the `ZydisSwizzleMode` enum.
 */
typedef enum ZydisSwizzleMode_
{
    ZYDIS_SWIZZLE_MODE_INVALID,
    ZYDIS_SWIZZLE_MODE_DCBA,
    ZYDIS_SWIZZLE_MODE_CDAB,
    ZYDIS_SWIZZLE_MODE_BADC,
    ZYDIS_SWIZZLE_MODE_DACB,
    ZYDIS_SWIZZLE_MODE_AAAA,
    ZYDIS_SWIZZLE_MODE_BBBB,
    ZYDIS_SWIZZLE_MODE_CCCC,
    ZYDIS_SWIZZLE_MODE_DDDD,

    /**
     * Maximum value of this enum.
     */
    ZYDIS_SWIZZLE_MODE_MAX_VALUE = ZYDIS_SWIZZLE_MODE_DDDD,
    /**
     * The minimum number of bits required to represent all values of this enum.
     */
    ZYDIS_SWIZZLE_MODE_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_SWIZZLE_MODE_MAX_VALUE)
} ZydisSwizzleMode;

/* ---------------------------------------------------------------------------------------------- */
/* KNC conversion-mode                                                                            */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Defines the `ZydisConversionMode` enum.
 */
typedef enum ZydisConversionMode_
{
    ZYDIS_CONVERSION_MODE_INVALID,
    ZYDIS_CONVERSION_MODE_FLOAT16,
    ZYDIS_CONVERSION_MODE_SINT8,
    ZYDIS_CONVERSION_MODE_UINT8,
    ZYDIS_CONVERSION_MODE_SINT16,
    ZYDIS_CONVERSION_MODE_UINT16,

    /**
     * Maximum value of this enum.
     */
    ZYDIS_CONVERSION_MODE_MAX_VALUE = ZYDIS_CONVERSION_MODE_UINT16,
    /**
     * The minimum number of bits required to represent all values of this enum.
     */
    ZYDIS_CONVERSION_MODE_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_CONVERSION_MODE_MAX_VALUE)
} ZydisConversionMode;

/* ---------------------------------------------------------------------------------------------- */
/* Legacy prefix type                                                                             */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Defines the `ZydisPrefixType` enum.
 */
typedef enum ZydisPrefixType_
{
    /**
     * The prefix is ignored by the instruction.
     *
     * This applies to all prefixes that are not accepted by the instruction in general or the
     * ones that are overwritten by a prefix of the same group closer to the instruction opcode.
     */
    ZYDIS_PREFIX_TYPE_IGNORED,
    /**
     * The prefix is effectively used by the instruction.
     */
    ZYDIS_PREFIX_TYPE_EFFECTIVE,
    /**
     * The prefix is used as a mandatory prefix.
     *
     * A mandatory prefix is interpreted as an opcode extension and has no further effect on the
     * instruction.
     */
    ZYDIS_PREFIX_TYPE_MANDATORY,

    /**
     * Maximum value of this enum.
     */
    ZYDIS_PREFIX_TYPE_MAX_VALUE = ZYDIS_PREFIX_TYPE_MANDATORY,
    /**
     * The minimum number of bits required to represent all values of this enum.
     */
    ZYDIS_PREFIX_TYPE_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_PREFIX_TYPE_MAX_VALUE)
} ZydisPrefixType;

// TODO: Check effective for 66/67 prefixes (currently defaults to EFFECTIVE)

/* ---------------------------------------------------------------------------------------------- */
/* Decoded instruction                                                                            */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Information about a decoded instruction.
 */
typedef struct ZydisDecodedInstruction_
{
    /**
     * The machine mode used to decode this instruction.
     */
    ZydisMachineMode machine_mode;
    /**
     * The instruction-mnemonic.
     */
    ZydisMnemonic mnemonic;
    /**
     * The length of the decoded instruction.
     */
    ZyanU8 length;
    /**
     * The instruction-encoding (`LEGACY`, `3DNOW`, `VEX`, `EVEX`, `XOP`).
     */
    ZydisInstructionEncoding encoding;
    /**
     * The opcode-map.
     */
    ZydisOpcodeMap opcode_map;
    /**
     * The instruction-opcode.
     */
    ZyanU8 opcode;
    /**
     * The stack width.
     */
    ZyanU8 stack_width;
    /**
     * The effective operand width.
     */
    ZyanU8 operand_width;
    /**
     * The effective address width.
     */
    ZyanU8 address_width;
    /**
     * The number of instruction-operands.
     */
    ZyanU8 operand_count;
    /**
     * Detailed info for all instruction operands.
     *
     * Explicit operands are guaranteed to be in the front and ordered as they are printed
     * by the formatter in Intel mode. No assumptions can be made about the order of hidden
     * operands, except that they always located behind the explicit operands.
     */
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
    /**
     * Instruction attributes.
     */
    ZydisInstructionAttributes attributes;
    /**
     * Information about accessed CPU flags.
     *
     * DEPRECATED. This field will be removed in the next major release. Please use the
     * `cpu_flags_read`/`cpu_flags_written` or `fpu_flags_read`/`fpu_flags_written` fields
     * instead.
     */
    struct ZydisDecodedInstructionAccessedFlags_
    {
        /**
         * The CPU-flag action.
         *
         * Use `ZydisGetAccessedFlagsByAction` to get a mask with all flags matching a specific
         * action.
         */
        ZydisCPUFlagAction action;
    } accessed_flags[ZYDIS_CPUFLAG_MAX_VALUE + 1];
    /**
     * A mask containing the CPU flags read by the instruction.
     *
     * The bits in this mask correspond to the actual bits in the `FLAGS/EFLAGS/RFLAGS`
     * register.
     *
     * This mask includes the actions `TESTED` and `TESTED_MODIFIED`.
     */
    ZydisCPUFlags cpu_flags_read;
    /**
     * A mask containing the CPU flags written by the instruction.
     *
     * The bits in this mask correspond to the actual bits in the `FLAGS/EFLAGS/RFLAGS`
     * register.
     *
     * This mask includes the actions `TESTED_MODIFIED`, `SET_0`, `SET_1` and `UNDEFINED`.
     */
    ZydisCPUFlags cpu_flags_written;
    /**
     * A mask containing the FPU flags read by the instruction.
     */
    ZydisFPUFlags fpu_flags_read;
    /**
     * A mask containing the FPU flags written by the instruction.
     */
    ZydisFPUFlags fpu_flags_written;
    /**
     * Extended info for `AVX` instructions.
     */
    struct ZydisDecodedInstructionAvx_
    {
        /**
         * The `AVX` vector-length.
         */
        ZyanU16 vector_length;
        /**
         * Info about the embedded writemask-register (`AVX-512` and `KNC` only).
         */
        struct ZydisDecodedInstructionAvxMask_
        {
            /**
             * The masking mode.
             */
            ZydisMaskMode mode;
            /**
             * The mask register.
             */
            ZydisRegister reg;
        } mask;
        /**
         * Contains info about the `AVX` broadcast.
         */
        struct ZydisDecodedInstructionAvxBroadcast_
        {
            /**
             * Signals, if the broadcast is a static broadcast.
             *
             * This is the case for instructions with inbuilt broadcast functionality, which is
             * always active and not controlled by the `EVEX/MVEX.RC` bits.
             */
            ZyanBool is_static;
            /**
             * The `AVX` broadcast-mode.
             */
            ZydisBroadcastMode mode;
        } broadcast;
        /**
         * Contains info about the `AVX` rounding.
         */
        struct ZydisDecodedInstructionAvxRounding_
        {
            /**
             * The `AVX` rounding-mode.
             */
            ZydisRoundingMode mode;
        } rounding;
        /**
         * Contains info about the `AVX` register-swizzle (`KNC` only).
         */
        struct ZydisDecodedInstructionAvxSwizzle_
        {
            /**
             * The `AVX` register-swizzle mode.
             */
            ZydisSwizzleMode mode;
        } swizzle;
        /**
         * Contains info about the `AVX` data-conversion (`KNC` only).
         */
        struct ZydisDecodedInstructionAvxConversion_
        {
            /**
             * The `AVX` data-conversion mode.
             */
            ZydisConversionMode mode;
        } conversion;
        /**
         * Signals, if the `SAE` (suppress-all-exceptions) functionality is
         * enabled for the instruction.
         */
        ZyanBool has_sae;
        /**
         * Signals, if the instruction has a memory-eviction-hint (`KNC` only).
         */
        ZyanBool has_eviction_hint;
        // TODO: publish EVEX tuple-type and MVEX functionality
    } avx;
    /**
     * Meta info.
     */
    struct ZydisDecodedInstructionMeta_
    {
        /**
         * The instruction category.
         */
        ZydisInstructionCategory category;
        /**
         * The ISA-set.
         */
        ZydisISASet isa_set;
        /**
         * The ISA-set extension.
         */
        ZydisISAExt isa_ext;
        /**
         * The branch type.
         */
        ZydisBranchType branch_type;
        /**
         * The exception class.
         */
        ZydisExceptionClass exception_class;
    } meta;
    /**
     * Detailed info about different instruction-parts like `ModRM`, `SIB` or
     * encoding-prefixes.
     */
    struct ZydisDecodedInstructionRaw_
    {
        /**
         * The number of legacy prefixes.
         */
        ZyanU8 prefix_count;
        /**
         * Detailed info about the legacy prefixes (including `REX`).
         */
        struct ZydisDecodedInstructionRawPrefixes_
        {
            /**
             * The prefix type.
             */
            ZydisPrefixType type;
            /**
             * The prefix byte.
             */
            ZyanU8 value;
        } prefixes[ZYDIS_MAX_INSTRUCTION_LENGTH];
        /**
         * Detailed info about the `REX` prefix.
         */
        struct ZydisDecodedInstructionRawRex_
        {
            /**
             * 64-bit operand-size promotion.
             */
            ZyanU8 W;
            /**
             * Extension of the `ModRM.reg` field.
             */
            ZyanU8 R;
            /**
             * Extension of the `SIB.index` field.
             */
            ZyanU8 X;
            /**
             * Extension of the `ModRM.rm`, `SIB.base`, or `opcode.reg` field.
             */
            ZyanU8 B;
            /**
             * The offset of the effective `REX` byte, relative to the beginning of the
             * instruction, in bytes.
             *
             * This offset always points to the "effective" `REX` prefix (the one closest to the
             * instruction opcode), if multiple `REX` prefixes are present.
             *
             * Note that the `REX` byte can be the first byte of the instruction, which would lead
             * to an offset of `0`. Please refer to the instruction attributes to check for the
             * presence of the `REX` prefix.
             */
            ZyanU8 offset;
        } rex;
        /**
         * Detailed info about the `XOP` prefix.
         */
        struct ZydisDecodedInstructionRawXop_
        {
            /**
             * Extension of the `ModRM.reg` field (inverted).
             */
            ZyanU8 R;
            /**
             * Extension of the `SIB.index` field (inverted).
             */
            ZyanU8 X;
            /**
             * Extension of the `ModRM.rm`, `SIB.base`, or `opcode.reg` field (inverted).
             */
            ZyanU8 B;
            /**
             * Opcode-map specifier.
             */
            ZyanU8 m_mmmm;
            /**
             * 64-bit operand-size promotion or opcode-extension.
             */
            ZyanU8 W;
            /**
             * `NDS`/`NDD` (non-destructive-source/destination) register
             * specifier (inverted).
             */
            ZyanU8 vvvv;
            /**
             * Vector-length specifier.
             */
            ZyanU8 L;
            /**
             * Compressed legacy prefix.
             */
            ZyanU8 pp;
            /**
             * The offset of the first xop byte, relative to the beginning of
             * the instruction, in bytes.
             */
            ZyanU8 offset;
        } xop;
        /**
         * Detailed info about the `VEX` prefix.
         */
        struct ZydisDecodedInstructionRawVex_
        {
            /**
             * Extension of the `ModRM.reg` field (inverted).
             */
            ZyanU8 R;
            /**
             * Extension of the `SIB.index` field (inverted).
             */
            ZyanU8 X;
            /**
             * Extension of the `ModRM.rm`, `SIB.base`, or `opcode.reg` field (inverted).
             */
            ZyanU8 B;
            /**
             * Opcode-map specifier.
             */
            ZyanU8 m_mmmm;
            /**
             * 64-bit operand-size promotion or opcode-extension.
             */
            ZyanU8 W;
            /**
             * `NDS`/`NDD` (non-destructive-source/destination) register specifier
             *  (inverted).
             */
            ZyanU8 vvvv;
            /**
             * Vector-length specifier.
             */
            ZyanU8 L;
            /**
             * Compressed legacy prefix.
             */
            ZyanU8 pp;
            /**
             * The offset of the first `VEX` byte, relative to the beginning of the instruction, in
             * bytes.
             */
            ZyanU8 offset;
            /**
             * The size of the `VEX` prefix, in bytes.
             */
            ZyanU8 size;
        } vex;
        /**
         * Detailed info about the `EVEX` prefix.
         */
        struct ZydisDecodedInstructionRawEvex_
        {
            /**
             * Extension of the `ModRM.reg` field (inverted).
             */
            ZyanU8 R;
            /**
             * Extension of the `SIB.index/vidx` field (inverted).
             */
            ZyanU8 X;
            /**
             * Extension of the `ModRM.rm` or `SIB.base` field (inverted).
             */
            ZyanU8 B;
            /**
             * High-16 register specifier modifier (inverted).
             */
            ZyanU8 R2;
            /**
             * Opcode-map specifier.
             */
            ZyanU8 mmm;
            /**
             * 64-bit operand-size promotion or opcode-extension.
             */
            ZyanU8 W;
            /**
             * `NDS`/`NDD` (non-destructive-source/destination) register specifier
             * (inverted).
             */
            ZyanU8 vvvv;
            /**
             * Compressed legacy prefix.
             */
            ZyanU8 pp;
            /**
             * Zeroing/Merging.
             */
            ZyanU8 z;
            /**
             * Vector-length specifier or rounding-control (most significant bit).
             */
            ZyanU8 L2;
            /**
             * Vector-length specifier or rounding-control (least significant bit).
             */
            ZyanU8 L;
            /**
             * Broadcast/RC/SAE context.
             */
            ZyanU8 b;
            /**
             * High-16 `NDS`/`VIDX` register specifier.
             */
            ZyanU8 V2;
            /**
             * Embedded opmask register specifier.
             */
            ZyanU8 aaa;
            /**
             * The offset of the first evex byte, relative to the beginning of the
             * instruction, in bytes.
             */
            ZyanU8 offset;
        } evex;
        /**
        * Detailed info about the `MVEX` prefix.
        */
        struct ZydisDecodedInstructionRawMvex_
        {
            /**
             * Extension of the `ModRM.reg` field (inverted).
             */
            ZyanU8 R;
            /**
             * Extension of the `SIB.index/vidx` field (inverted).
             */
            ZyanU8 X;
            /**
             * Extension of the `ModRM.rm` or `SIB.base` field (inverted).
             */
            ZyanU8 B;
            /**
             * High-16 register specifier modifier (inverted).
             */
            ZyanU8 R2;
            /**
             * Opcode-map specifier.
             */
            ZyanU8 mmmm;
            /**
             * 64-bit operand-size promotion or opcode-extension.
             */
            ZyanU8 W;
            /**
             * `NDS`/`NDD` (non-destructive-source/destination) register specifier
             *  (inverted).
             */
            ZyanU8 vvvv;
            /**
             * Compressed legacy prefix.
             */
            ZyanU8 pp;
            /**
             * Non-temporal/eviction hint.
             */
            ZyanU8 E;
            /**
             * Swizzle/broadcast/up-convert/down-convert/static-rounding controls.
             */
            ZyanU8 SSS;
            /**
             * High-16 `NDS`/`VIDX` register specifier.
             */
            ZyanU8 V2;
            /**
             * Embedded opmask register specifier.
             */
            ZyanU8 kkk;
            /**
             * The offset of the first mvex byte, relative to the beginning of the
             * instruction, in bytes.
             */
            ZyanU8 offset;
        } mvex;
        /**
         * Detailed info about the `ModRM` byte.
         */
        struct ZydisDecodedInstructionModRm_
        {
            /**
             * The addressing mode.
             */
            ZyanU8 mod;
            /**
             * Register specifier or opcode-extension.
             */
            ZyanU8 reg;
            /**
             * Register specifier or opcode-extension.
             */
            ZyanU8 rm;
            /**
             * The offset of the `ModRM` byte, relative to the beginning of the
             * instruction, in bytes.
             */
            ZyanU8 offset;
        } modrm;
        /**
         * Detailed info about the `SIB` byte.
         */
        struct ZydisDecodedInstructionRawSib_
        {
            /**
             * The scale factor.
             */
            ZyanU8 scale;
            /**
             * The index-register specifier.
             */
            ZyanU8 index;
            /**
             * The base-register specifier.
             */
            ZyanU8 base;
            /**
             * The offset of the `SIB` byte, relative to the beginning of the
             * instruction, in bytes.
             */
            ZyanU8 offset;
        } sib;
        /**
         * Detailed info about displacement-bytes.
         */
        struct ZydisDecodedInstructionRawDisp_
        {
            /**
             * The displacement value
             */
            ZyanI64 value;
            /**
             * The physical displacement size, in bits.
             */
            ZyanU8 size;
            // TODO: publish cd8 scale
            /**
             * The offset of the displacement data, relative to the beginning of the
             * instruction, in bytes.
             */
            ZyanU8 offset;
        } disp;
        /**
         * Detailed info about immediate-bytes.
         */
        struct ZydisDecodedInstructionRawImm_
        {
            /**
             * Signals, if the immediate value is signed.
             */
            ZyanBool is_signed;
            /**
             * Signals, if the immediate value contains a relative offset. You can use
             * `ZydisCalcAbsoluteAddress` to determine the absolute address value.
             */
            ZyanBool is_relative;
            /**
             * The immediate value.
             */
            union ZydisDecodedInstructionRawImmValue_
            {
                ZyanU64 u;
                ZyanI64 s;
            } value;
            /**
             * The physical immediate size, in bits.
             */
            ZyanU8 size;
            /**
             * The offset of the immediate data, relative to the beginning of the
             * instruction, in bytes.
             */
            ZyanU8 offset;
        } imm[2];
    } raw;
} ZydisDecodedInstruction;

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* ZYDIS_INSTRUCTIONINFO_H */
