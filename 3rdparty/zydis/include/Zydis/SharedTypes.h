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
 * Defines decoder/encoder-shared macros and types.
 */

#ifndef ZYDIS_SHAREDTYPES_H
#define ZYDIS_SHAREDTYPES_H

#include <Zycore/Types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================================== */
/* Macros                                                                                         */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* Constants                                                                                      */
/* ---------------------------------------------------------------------------------------------- */

#define ZYDIS_MAX_INSTRUCTION_LENGTH 15
#define ZYDIS_MAX_OPERAND_COUNT      10

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */
/* Enums and types                                                                                */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* Machine mode                                                                                   */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Defines the `ZydisMachineMode` enum.
 */
typedef enum ZydisMachineMode_
{
    /**
     * 64 bit mode.
     */
    ZYDIS_MACHINE_MODE_LONG_64,
    /**
     * 32 bit protected mode.
     */
    ZYDIS_MACHINE_MODE_LONG_COMPAT_32,
    /**
     * 16 bit protected mode.
     */
    ZYDIS_MACHINE_MODE_LONG_COMPAT_16,
    /**
     * 32 bit protected mode.
     */
    ZYDIS_MACHINE_MODE_LEGACY_32,
    /**
     * 16 bit protected mode.
     */
    ZYDIS_MACHINE_MODE_LEGACY_16,
    /**
     * 16 bit real mode.
     */
    ZYDIS_MACHINE_MODE_REAL_16,

    /**
     * Maximum value of this enum.
     */
    ZYDIS_MACHINE_MODE_MAX_VALUE = ZYDIS_MACHINE_MODE_REAL_16,
    /**
     * The minimum number of bits required to represent all values of this enum.
     */
    ZYDIS_MACHINE_MODE_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_MACHINE_MODE_MAX_VALUE)
} ZydisMachineMode;

/* ---------------------------------------------------------------------------------------------- */
/* Address width                                                                                  */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Defines the `ZydisAddressWidth` enum.
 */
typedef enum ZydisAddressWidth_
{
    ZYDIS_ADDRESS_WIDTH_16,
    ZYDIS_ADDRESS_WIDTH_32,
    ZYDIS_ADDRESS_WIDTH_64,

    /**
     * Maximum value of this enum.
     */
    ZYDIS_ADDRESS_WIDTH_MAX_VALUE = ZYDIS_ADDRESS_WIDTH_64,
    /**
     * The minimum number of bits required to represent all values of this enum.
     */
    ZYDIS_ADDRESS_WIDTH_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_ADDRESS_WIDTH_MAX_VALUE)
} ZydisAddressWidth;

/* ---------------------------------------------------------------------------------------------- */
/* Element type                                                                                   */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Defines the `ZydisElementType` enum.
 */
typedef enum ZydisElementType_
{
    ZYDIS_ELEMENT_TYPE_INVALID,
    /**
     * A struct type.
     */
    ZYDIS_ELEMENT_TYPE_STRUCT,
    /**
     * Unsigned integer value.
     */
    ZYDIS_ELEMENT_TYPE_UINT,
    /**
     * Signed integer value.
     */
    ZYDIS_ELEMENT_TYPE_INT,
    /**
     * 16-bit floating point value (`half`).
     */
    ZYDIS_ELEMENT_TYPE_FLOAT16,
    /**
     * 32-bit floating point value (`single`).
     */
    ZYDIS_ELEMENT_TYPE_FLOAT32,
    /**
     * 64-bit floating point value (`double`).
     */
    ZYDIS_ELEMENT_TYPE_FLOAT64,
    /**
     * 80-bit floating point value (`extended`).
     */
    ZYDIS_ELEMENT_TYPE_FLOAT80,
    /**
     * Binary coded decimal value.
     */
    ZYDIS_ELEMENT_TYPE_LONGBCD,
    /**
     * A condition code (e.g. used by `CMPPD`, `VCMPPD`, ...).
     */
    ZYDIS_ELEMENT_TYPE_CC,

    /**
     * Maximum value of this enum.
     */
    ZYDIS_ELEMENT_TYPE_MAX_VALUE = ZYDIS_ELEMENT_TYPE_CC,
    /**
     * The minimum number of bits required to represent all values of this enum.
     */
    ZYDIS_ELEMENT_TYPE_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_ELEMENT_TYPE_MAX_VALUE)
} ZydisElementType;

/* ---------------------------------------------------------------------------------------------- */
/* Element size                                                                                   */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Defines the `ZydisElementSize` datatype.
 */
typedef ZyanU16 ZydisElementSize;

/* ---------------------------------------------------------------------------------------------- */
/* Operand type                                                                                   */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Defines the `ZydisOperandType` enum.
 */
typedef enum ZydisOperandType_
{
    /**
     * The operand is not used.
     */
    ZYDIS_OPERAND_TYPE_UNUSED,
    /**
     * The operand is a register operand.
     */
    ZYDIS_OPERAND_TYPE_REGISTER,
    /**
     * The operand is a memory operand.
     */
    ZYDIS_OPERAND_TYPE_MEMORY,
    /**
     * The operand is a pointer operand with a segment:offset lvalue.
     */
    ZYDIS_OPERAND_TYPE_POINTER,
    /**
     * The operand is an immediate operand.
     */
    ZYDIS_OPERAND_TYPE_IMMEDIATE,

    /**
     * Maximum value of this enum.
     */
    ZYDIS_OPERAND_TYPE_MAX_VALUE = ZYDIS_OPERAND_TYPE_IMMEDIATE,
    /**
     * The minimum number of bits required to represent all values of this enum.
     */
    ZYDIS_OPERAND_TYPE_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_OPERAND_TYPE_MAX_VALUE)
} ZydisOperandType;

/* ---------------------------------------------------------------------------------------------- */
/* Operand encoding                                                                               */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Defines the `ZydisOperandEncoding` enum.
 */
typedef enum ZydisOperandEncoding_
{
    ZYDIS_OPERAND_ENCODING_NONE,
    ZYDIS_OPERAND_ENCODING_MODRM_REG,
    ZYDIS_OPERAND_ENCODING_MODRM_RM,
    ZYDIS_OPERAND_ENCODING_OPCODE,
    ZYDIS_OPERAND_ENCODING_NDSNDD,
    ZYDIS_OPERAND_ENCODING_IS4,
    ZYDIS_OPERAND_ENCODING_MASK,
    ZYDIS_OPERAND_ENCODING_DISP8,
    ZYDIS_OPERAND_ENCODING_DISP16,
    ZYDIS_OPERAND_ENCODING_DISP32,
    ZYDIS_OPERAND_ENCODING_DISP64,
    ZYDIS_OPERAND_ENCODING_DISP16_32_64,
    ZYDIS_OPERAND_ENCODING_DISP32_32_64,
    ZYDIS_OPERAND_ENCODING_DISP16_32_32,
    ZYDIS_OPERAND_ENCODING_UIMM8,
    ZYDIS_OPERAND_ENCODING_UIMM16,
    ZYDIS_OPERAND_ENCODING_UIMM32,
    ZYDIS_OPERAND_ENCODING_UIMM64,
    ZYDIS_OPERAND_ENCODING_UIMM16_32_64,
    ZYDIS_OPERAND_ENCODING_UIMM32_32_64,
    ZYDIS_OPERAND_ENCODING_UIMM16_32_32,
    ZYDIS_OPERAND_ENCODING_SIMM8,
    ZYDIS_OPERAND_ENCODING_SIMM16,
    ZYDIS_OPERAND_ENCODING_SIMM32,
    ZYDIS_OPERAND_ENCODING_SIMM64,
    ZYDIS_OPERAND_ENCODING_SIMM16_32_64,
    ZYDIS_OPERAND_ENCODING_SIMM32_32_64,
    ZYDIS_OPERAND_ENCODING_SIMM16_32_32,
    ZYDIS_OPERAND_ENCODING_JIMM8,
    ZYDIS_OPERAND_ENCODING_JIMM16,
    ZYDIS_OPERAND_ENCODING_JIMM32,
    ZYDIS_OPERAND_ENCODING_JIMM64,
    ZYDIS_OPERAND_ENCODING_JIMM16_32_64,
    ZYDIS_OPERAND_ENCODING_JIMM32_32_64,
    ZYDIS_OPERAND_ENCODING_JIMM16_32_32,

    /**
     * Maximum value of this enum.
     */
    ZYDIS_OPERAND_ENCODING_MAX_VALUE = ZYDIS_OPERAND_ENCODING_JIMM16_32_32,
    /**
     * The minimum number of bits required to represent all values of this enum.
     */
    ZYDIS_OPERAND_ENCODING_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_OPERAND_ENCODING_MAX_VALUE)
} ZydisOperandEncoding;

/* ---------------------------------------------------------------------------------------------- */
/* Operand visibility                                                                             */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Defines the `ZydisOperandVisibility` enum.
 */
typedef enum ZydisOperandVisibility_
{
    ZYDIS_OPERAND_VISIBILITY_INVALID,
    /**
     * The operand is explicitly encoded in the instruction.
     */
    ZYDIS_OPERAND_VISIBILITY_EXPLICIT,
    /**
     * The operand is part of the opcode, but listed as an operand.
     */
    ZYDIS_OPERAND_VISIBILITY_IMPLICIT,
    /**
     * The operand is part of the opcode, and not typically listed as an operand.
     */
    ZYDIS_OPERAND_VISIBILITY_HIDDEN,

    /**
     * Maximum value of this enum.
     */
    ZYDIS_OPERAND_VISIBILITY_MAX_VALUE = ZYDIS_OPERAND_VISIBILITY_HIDDEN,
    /**
     * The minimum number of bits required to represent all values of this enum.
     */
    ZYDIS_OPERAND_VISIBILITY_REQUIRED_BITS =
        ZYAN_BITS_TO_REPRESENT(ZYDIS_OPERAND_VISIBILITY_MAX_VALUE)
} ZydisOperandVisibility;

/* ---------------------------------------------------------------------------------------------- */
/* Operand action                                                                                 */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Defines the `ZydisOperandAction` enum.
 */
typedef enum ZydisOperandAction_
{
    /* ------------------------------------------------------------------------------------------ */
    /* Elemental actions                                                                          */
    /* ------------------------------------------------------------------------------------------ */

    /**
     * The operand is read by the instruction.
     */
    ZYDIS_OPERAND_ACTION_READ       = 0x01,
    /**
     * The operand is written by the instruction (must write).
     */
    ZYDIS_OPERAND_ACTION_WRITE      = 0x02,
    /**
     * The operand is conditionally read by the instruction.
     */
    ZYDIS_OPERAND_ACTION_CONDREAD   = 0x04,
    /**
     * The operand is conditionally written by the instruction (may write).
     */
    ZYDIS_OPERAND_ACTION_CONDWRITE  = 0x08,

    /* ------------------------------------------------------------------------------------------ */
    /* Combined actions                                                                           */
    /* ------------------------------------------------------------------------------------------ */

    /**
     * The operand is read (must read) and written by the instruction (must write).
     */
    ZYDIS_OPERAND_ACTION_READWRITE = ZYDIS_OPERAND_ACTION_READ | ZYDIS_OPERAND_ACTION_WRITE,
    /**
     * The operand is conditionally read (may read) and conditionally written by
     * the instruction (may write).
     */
    ZYDIS_OPERAND_ACTION_CONDREAD_CONDWRITE =
        ZYDIS_OPERAND_ACTION_CONDREAD | ZYDIS_OPERAND_ACTION_CONDWRITE,
    /**
     * The operand is read (must read) and conditionally written by the
     * instruction (may write).
     */
    ZYDIS_OPERAND_ACTION_READ_CONDWRITE =
        ZYDIS_OPERAND_ACTION_READ | ZYDIS_OPERAND_ACTION_CONDWRITE,
    /**
     * The operand is written (must write) and conditionally read by the
     * instruction (may read).
     */
    ZYDIS_OPERAND_ACTION_CONDREAD_WRITE =
        ZYDIS_OPERAND_ACTION_CONDREAD | ZYDIS_OPERAND_ACTION_WRITE,

    /**
     * Mask combining all reading access flags.
     */
    ZYDIS_OPERAND_ACTION_MASK_READ  = ZYDIS_OPERAND_ACTION_READ | ZYDIS_OPERAND_ACTION_CONDREAD,
    /**
     * Mask combining all writing access flags.
     */
    ZYDIS_OPERAND_ACTION_MASK_WRITE = ZYDIS_OPERAND_ACTION_WRITE | ZYDIS_OPERAND_ACTION_CONDWRITE,

    /* ------------------------------------------------------------------------------------------ */

    /**
     * The minimum number of bits required to represent all values of this bitset.
     */
    ZYDIS_OPERAND_ACTION_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_OPERAND_ACTION_CONDWRITE)
} ZydisOperandAction;

/**
 * Defines the `ZydisOperandActions` data-type.
 */
typedef ZyanU8 ZydisOperandActions;

/* ---------------------------------------------------------------------------------------------- */
/* Instruction encoding                                                                           */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Defines the `ZydisInstructionEncoding` enum.
 */
typedef enum ZydisInstructionEncoding_
{
    /**
     * The instruction uses the legacy encoding.
     */
    ZYDIS_INSTRUCTION_ENCODING_LEGACY,
    /**
     * The instruction uses the AMD 3DNow-encoding.
     */
    ZYDIS_INSTRUCTION_ENCODING_3DNOW,
    /**
     * The instruction uses the AMD XOP-encoding.
     */
    ZYDIS_INSTRUCTION_ENCODING_XOP,
    /**
     * The instruction uses the VEX-encoding.
     */
    ZYDIS_INSTRUCTION_ENCODING_VEX,
    /**
     * The instruction uses the EVEX-encoding.
     */
    ZYDIS_INSTRUCTION_ENCODING_EVEX,
    /**
     * The instruction uses the MVEX-encoding.
     */
    ZYDIS_INSTRUCTION_ENCODING_MVEX,

    /**
     * Maximum value of this enum.
     */
    ZYDIS_INSTRUCTION_ENCODING_MAX_VALUE = ZYDIS_INSTRUCTION_ENCODING_MVEX,
    /**
     * The minimum number of bits required to represent all values of this enum.
     */
    ZYDIS_INSTRUCTION_ENCODING_REQUIRED_BITS =
        ZYAN_BITS_TO_REPRESENT(ZYDIS_INSTRUCTION_ENCODING_MAX_VALUE)
} ZydisInstructionEncoding;

/* ---------------------------------------------------------------------------------------------- */
/* Opcode map                                                                                     */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Defines the `ZydisOpcodeMap` enum.
 */
typedef enum ZydisOpcodeMap_
{
    ZYDIS_OPCODE_MAP_DEFAULT,
    ZYDIS_OPCODE_MAP_0F,
    ZYDIS_OPCODE_MAP_0F38,
    ZYDIS_OPCODE_MAP_0F3A,
    ZYDIS_OPCODE_MAP_MAP4, // not used
    ZYDIS_OPCODE_MAP_MAP5,
    ZYDIS_OPCODE_MAP_MAP6,
    ZYDIS_OPCODE_MAP_MAP7, // not used
    ZYDIS_OPCODE_MAP_0F0F,
    ZYDIS_OPCODE_MAP_XOP8,
    ZYDIS_OPCODE_MAP_XOP9,
    ZYDIS_OPCODE_MAP_XOPA,

    /**
     * Maximum value of this enum.
     */
    ZYDIS_OPCODE_MAP_MAX_VALUE = ZYDIS_OPCODE_MAP_XOPA,
    /**
     * The minimum number of bits required to represent all values of this enum.
     */
    ZYDIS_OPCODE_MAP_REQUIRED_BITS = ZYAN_BITS_TO_REPRESENT(ZYDIS_OPCODE_MAP_MAX_VALUE)
} ZydisOpcodeMap;

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* ZYDIS_SHAREDTYPES_H */
