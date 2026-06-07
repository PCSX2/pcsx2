// Copyright 2015, VIXL authors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of ARM Limited nor the names of its contributors may be
//     used to endorse or promote products derived from this software without
//     specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "disasm-aarch64.h"

#include <bitset>
#include <cstdlib>
#include <regex>
#include <sstream>
#include <stack>

namespace vixl {
namespace aarch64 {

std::string Disassembler::GetMnemonicAlias(const Instruction *instr) {
  // Representation of a simple condition for an alias to apply. Mask and
  // post-mask value are combined into a single 64-bit field (similar to
  // unallocated instruction detection elsewhere) such that an alias
  // should be applied if (i & mask_value >> 32) == (mask_value & 0xffffffff).
  using MaskAlias = struct {
    uint64_t mask_value;
    std::string alias;
  };

  // "Simple" alias detection. For each form, one or more masking tests are
  // independently applied to determine if the alias is used.
  using MaskAliasMap = std::unordered_map<uint32_t, std::vector<MaskAlias>>;

  const uint64_t kAllCases = 0x00000000'00000000;
  const uint64_t kRdIsZROrSP = 0x0000001f'0000001f;
  const uint64_t kRnIsZROrSP = 0x000003e0'000003e0;
  const uint64_t kRaIsZROrSP = 0x00007c00'00007c00;
  const uint64_t kAddSubImmZero = 0x003ffc00'00000000;
  const uint64_t kLogImmIsZeroLSL = 0x00c0fc00'00000000;
  const uint64_t kBFMr0s7 = 0x003ffc00'00001c00;
  const uint64_t kBFMr0s15 = 0x003ffc00'00003c00;
  const uint64_t kBFMr0s31 = 0x003ffc00'00007c00;
  const uint64_t kBFMs31 = 0x0000fc00'00007c00;
  const uint64_t kBFMs63 = 0x0000fc00'0000fc00;
  const uint64_t kUMOVIsS = 0x00070000'00040000;
  const uint64_t kNEONQSet = 0x40000000'40000000;
  const uint64_t kSHLLImmh1 = 0x003f0000'00080000;
  const uint64_t kSHLLImmh2 = 0x003f0000'00100000;
  const uint64_t kSHLLImmh4 = 0x003f0000'00200000;
  const uint64_t kSysIsIC = 0x0007ffe0'00037520;
  const uint64_t kSysIsGCSSS1 = 0x0007ffe0'00037740;
  const uint64_t kSysIsGCSPUSHM = 0x0007ffe0'00037700;
  const uint64_t kSyslIsGCSPOPM = 0x0007ffe0'00037720;
  const uint64_t kSyslIsGCSSS2 = 0x0007ffe0'00037760;
  const uint64_t kCrmIs0 = 0x00000f00'00000000;
  const uint64_t kCrmIs4 = 0x00000f00'00000400;

  static const MaskAliasMap maskmap =
      {{"adds_32s_addsub_imm"_h, {{kRdIsZROrSP, "cmn"}}},
       {"adds_64s_addsub_imm"_h, {{kRdIsZROrSP, "cmn"}}},
       {"subs_32s_addsub_imm"_h, {{kRdIsZROrSP, "cmp"}}},
       {"subs_64s_addsub_imm"_h, {{kRdIsZROrSP, "cmp"}}},
       {"add_32_addsub_imm"_h,
        {{kRdIsZROrSP | kAddSubImmZero, "mov"},
         {kRnIsZROrSP | kAddSubImmZero, "mov"}}},
       {"add_64_addsub_imm"_h,
        {{kRdIsZROrSP | kAddSubImmZero, "mov"},
         {kRnIsZROrSP | kAddSubImmZero, "mov"}}},
       {"adds_32_addsub_shift"_h, {{kRdIsZROrSP, "cmn"}}},
       {"adds_64_addsub_shift"_h, {{kRdIsZROrSP, "cmn"}}},
       {"sub_32_addsub_shift"_h, {{kRnIsZROrSP, "neg"}}},
       {"sub_64_addsub_shift"_h, {{kRnIsZROrSP, "neg"}}},
       {"subs_32_addsub_shift"_h,
        {{kRdIsZROrSP, "cmp"}, {kRnIsZROrSP, "negs"}}},
       {"subs_64_addsub_shift"_h,
        {{kRdIsZROrSP, "cmp"}, {kRnIsZROrSP, "negs"}}},
       {"sbc_32_addsub_carry"_h, {{kRnIsZROrSP, "ngc"}}},
       {"sbc_64_addsub_carry"_h, {{kRnIsZROrSP, "ngc"}}},
       {"sbcs_32_addsub_carry"_h, {{kRnIsZROrSP, "ngcs"}}},
       {"sbcs_64_addsub_carry"_h, {{kRnIsZROrSP, "ngcs"}}},
       {"adds_32s_addsub_ext"_h, {{kRdIsZROrSP, "cmn"}}},
       {"adds_64s_addsub_ext"_h, {{kRdIsZROrSP, "cmn"}}},
       {"subs_32s_addsub_ext"_h, {{kRdIsZROrSP, "cmp"}}},
       {"subs_64s_addsub_ext"_h, {{kRdIsZROrSP, "cmp"}}},
       {"adds_32s_addsub_ext"_h, {{kRnIsZROrSP, "add_lsl"}}},
       {"adds_64s_addsub_ext"_h, {{kRnIsZROrSP, "add_lsl"}}},
       {"subs_32s_addsub_ext"_h, {{kRnIsZROrSP, "sub_lsl"}}},
       {"subs_64s_addsub_ext"_h, {{kRnIsZROrSP, "sub_lsl"}}},
       {"add_32_addsub_ext"_h,
        {{kRdIsZROrSP, "add_lsl"}, {kRnIsZROrSP, "add_lsl"}}},
       {"add_64_addsub_ext"_h,
        {{kRdIsZROrSP, "add_lsl"}, {kRnIsZROrSP, "add_lsl"}}},
       {"sub_32_addsub_ext"_h,
        {{kRdIsZROrSP, "sub_lsl"}, {kRnIsZROrSP, "sub_lsl"}}},
       {"sub_64_addsub_ext"_h,
        {{kRdIsZROrSP, "sub_lsl"}, {kRnIsZROrSP, "sub_lsl"}}},
       {"ands_32_log_shift"_h, {{kRdIsZROrSP, "tst"}}},
       {"ands_64_log_shift"_h, {{kRdIsZROrSP, "tst"}}},
       {"orr_32_log_shift"_h, {{kRnIsZROrSP | kLogImmIsZeroLSL, "mov"}}},
       {"orr_64_log_shift"_h, {{kRnIsZROrSP | kLogImmIsZeroLSL, "mov"}}},
       {"orn_32_log_shift"_h, {{kRnIsZROrSP, "mvn"}}},
       {"orn_64_log_shift"_h, {{kRnIsZROrSP, "mvn"}}},
       {"ands_32s_log_imm"_h, {{kRdIsZROrSP, "tst"}}},
       {"ands_64s_log_imm"_h, {{kRdIsZROrSP, "tst"}}},
       {"madd_32a_dp_3src"_h, {{kRaIsZROrSP, "mul"}}},
       {"madd_64a_dp_3src"_h, {{kRaIsZROrSP, "mul"}}},
       {"msub_32a_dp_3src"_h, {{kRaIsZROrSP, "mneg"}}},
       {"msub_64a_dp_3src"_h, {{kRaIsZROrSP, "mneg"}}},
       {"smaddl_64wa_dp_3src"_h, {{kRaIsZROrSP, "smull"}}},
       {"smsubl_64wa_dp_3src"_h, {{kRaIsZROrSP, "smnegl"}}},
       {"umaddl_64wa_dp_3src"_h, {{kRaIsZROrSP, "umull"}}},
       {"umsubl_64wa_dp_3src"_h, {{kRaIsZROrSP, "umnegl"}}},
       {"asrv_32_dp_2src"_h, {{kAllCases, "asr"}}},
       {"asrv_64_dp_2src"_h, {{kAllCases, "asr"}}},
       {"lslv_32_dp_2src"_h, {{kAllCases, "lsl"}}},
       {"lslv_64_dp_2src"_h, {{kAllCases, "lsl"}}},
       {"lsrv_32_dp_2src"_h, {{kAllCases, "lsr"}}},
       {"lsrv_64_dp_2src"_h, {{kAllCases, "lsr"}}},
       {"rorv_32_dp_2src"_h, {{kAllCases, "ror"}}},
       {"rorv_64_dp_2src"_h, {{kAllCases, "ror"}}},
       {"b_only_condbranch"_h, {{kAllCases, "b.'[condb]"}}},
       {"bc_only_condbranch"_h, {{kAllCases, "bc.'[condb]"}}},
       {"not_asimdmisc_r"_h, {{kAllCases, "mvn"}}},
       {"dup_z_i"_h, {{kAllCases, "mov"}}},
       {"fdup_z_i"_h, {{kAllCases, "fmov"}}},
       {"dup_z_r"_h, {{kAllCases, "mov"}}},
       {"cpy_z_p_r"_h, {{kAllCases, "mov"}}},
       {"cpy_z_o_i"_h, {{kAllCases, "mov"}}},
       {"cpy_z_p_i"_h, {{kAllCases, "mov"}}},
       {"cpy_z_p_v"_h, {{kAllCases, "mov"}}},
       {"fcpy_z_p_i"_h, {{kAllCases, "fmov"}}},
       {"ins_asimdins_ir_r"_h, {{kAllCases, "mov"}}},
       {"ins_asimdins_iv_v"_h, {{kAllCases, "mov"}}},
       {"umov_asimdins_x_x"_h, {{kAllCases, "mov"}}},
       {"dup_asisdone_only"_h, {{kAllCases, "mov"}}},
       {"umov_asimdins_w_w"_h, {{kUMOVIsS, "mov"}}},
       {"shrn_asimdshf_n"_h, {{kNEONQSet, "shrn2"}}},
       {"rshrn_asimdshf_n"_h, {{kNEONQSet, "rshrn2"}}},
       {"sqshrn_asimdshf_n"_h, {{kNEONQSet, "sqshrn2"}}},
       {"sqrshrn_asimdshf_n"_h, {{kNEONQSet, "sqrshrn2"}}},
       {"sqshrun_asimdshf_n"_h, {{kNEONQSet, "sqshrun2"}}},
       {"sqrshrun_asimdshf_n"_h, {{kNEONQSet, "sqrshrun2"}}},
       {"uqshrn_asimdshf_n"_h, {{kNEONQSet, "uqshrn2"}}},
       {"uqrshrn_asimdshf_n"_h, {{kNEONQSet, "uqrshrn2"}}},
       {"shll_asimdmisc_s"_h, {{kNEONQSet, "shll2"}}},
       {"xtn_asimdmisc_n"_h, {{kNEONQSet, "xtn2"}}},
       {"sqxtn_asimdmisc_n"_h, {{kNEONQSet, "sqxtn2"}}},
       {"uqxtn_asimdmisc_n"_h, {{kNEONQSet, "uqxtn2"}}},
       {"sqxtun_asimdmisc_n"_h, {{kNEONQSet, "sqxtun2"}}},
       {"smlal_asimdelem_l"_h, {{kNEONQSet, "smlal2"}}},
       {"smlsl_asimdelem_l"_h, {{kNEONQSet, "smlsl2"}}},
       {"smull_asimdelem_l"_h, {{kNEONQSet, "smull2"}}},
       {"umlal_asimdelem_l"_h, {{kNEONQSet, "umlal2"}}},
       {"umlsl_asimdelem_l"_h, {{kNEONQSet, "umlsl2"}}},
       {"umull_asimdelem_l"_h, {{kNEONQSet, "umull2"}}},
       {"sqdmull_asimdelem_l"_h, {{kNEONQSet, "sqdmull2"}}},
       {"sqdmlal_asimdelem_l"_h, {{kNEONQSet, "sqdmlal2"}}},
       {"sqdmlsl_asimdelem_l"_h, {{kNEONQSet, "sqdmlsl2"}}},
       {"bfcvtn_asimdmisc_4s"_h, {{kNEONQSet, "bfcvtn2"}}},
       {"fcvtxn_asimdmisc_n"_h, {{kNEONQSet, "fcvtxn2"}}},
       {"sabal_asimddiff_l"_h, {{kNEONQSet, "sabal2"}}},
       {"sabdl_asimddiff_l"_h, {{kNEONQSet, "sabdl2"}}},
       {"saddl_asimddiff_l"_h, {{kNEONQSet, "saddl2"}}},
       {"smlal_asimddiff_l"_h, {{kNEONQSet, "smlal2"}}},
       {"smlsl_asimddiff_l"_h, {{kNEONQSet, "smlsl2"}}},
       {"smull_asimddiff_l"_h, {{kNEONQSet, "smull2"}}},
       {"ssubl_asimddiff_l"_h, {{kNEONQSet, "ssubl2"}}},
       {"uabal_asimddiff_l"_h, {{kNEONQSet, "uabal2"}}},
       {"uabdl_asimddiff_l"_h, {{kNEONQSet, "uabdl2"}}},
       {"uaddl_asimddiff_l"_h, {{kNEONQSet, "uaddl2"}}},
       {"umlal_asimddiff_l"_h, {{kNEONQSet, "umlal2"}}},
       {"umlsl_asimddiff_l"_h, {{kNEONQSet, "umlsl2"}}},
       {"umull_asimddiff_l"_h, {{kNEONQSet, "umull2"}}},
       {"usubl_asimddiff_l"_h, {{kNEONQSet, "usubl2"}}},
       {"saddw_asimddiff_w"_h, {{kNEONQSet, "saddw2"}}},
       {"ssubw_asimddiff_w"_h, {{kNEONQSet, "ssubw2"}}},
       {"uaddw_asimddiff_w"_h, {{kNEONQSet, "uaddw2"}}},
       {"usubw_asimddiff_w"_h, {{kNEONQSet, "usubw2"}}},
       {"addhn_asimddiff_n"_h, {{kNEONQSet, "addhn2"}}},
       {"raddhn_asimddiff_n"_h, {{kNEONQSet, "raddhn2"}}},
       {"rsubhn_asimddiff_n"_h, {{kNEONQSet, "rsubhn2"}}},
       {"subhn_asimddiff_n"_h, {{kNEONQSet, "subhn2"}}},
       {"sqdmlal_asimddiff_l"_h, {{kNEONQSet, "sqdmlal2"}}},
       {"sqdmlsl_asimddiff_l"_h, {{kNEONQSet, "sqdmlsl2"}}},
       {"sqdmull_asimddiff_l"_h, {{kNEONQSet, "sqdmull2"}}},
       {"pmull_asimddiff_l"_h, {{kNEONQSet, "pmull2"}}},
       {"subps_64s_dp_2src"_h, {{kRdIsZROrSP, "cmpp"}}},
       {"sbfm_32m_bitfield"_h,
        {{kBFMr0s7, "sxtb"}, {kBFMr0s15, "sxth"}, {kBFMs31, "asr"}}},
       {"sbfm_64m_bitfield"_h,
        {{kBFMr0s7, "sxtb"},
         {kBFMr0s15, "sxth"},
         {kBFMr0s31, "sxtw"},
         {kBFMs63, "asr"}}},
       {"ubfm_32m_bitfield"_h,
        {{kBFMr0s7, "uxtb"}, {kBFMr0s15, "uxth"}, {kBFMs31, "lsr"}}},
       {"ubfm_64m_bitfield"_h,
        {{kBFMr0s7, "uxtb"}, {kBFMr0s15, "uxth"}, {kBFMs63, "lsr"}}},
       {"sshll_asimdshf_l"_h,
        {{kSHLLImmh1 | kNEONQSet, "sxtl2"},
         {kSHLLImmh2 | kNEONQSet, "sxtl2"},
         {kSHLLImmh4 | kNEONQSet, "sxtl2"},
         {kSHLLImmh1, "sxtl"},
         {kSHLLImmh2, "sxtl"},
         {kSHLLImmh4, "sxtl"},
         {kNEONQSet, "sshll2"},
         {kNEONQSet, "sshll2"},
         {kNEONQSet, "sshll2"}}},
       {"ushll_asimdshf_l"_h,
        {{kSHLLImmh1 | kNEONQSet, "uxtl2"},
         {kSHLLImmh2 | kNEONQSet, "uxtl2"},
         {kSHLLImmh4 | kNEONQSet, "uxtl2"},
         {kSHLLImmh1, "uxtl"},
         {kSHLLImmh2, "uxtl"},
         {kSHLLImmh4, "uxtl"},
         {kNEONQSet, "ushll2"},
         {kNEONQSet, "ushll2"},
         {kNEONQSet, "ushll2"}}},
       {"fcvtl_asimdmisc_l"_h, {{kNEONQSet, "fcvtl2"}}},
       {"fcvtn_asimdmisc_n"_h, {{kNEONQSet, "fcvtn2"}}},
       {"sys_cr_systeminstrs"_h,
        {{kSysIsIC, "ic"},
         {kSysIsGCSSS1, "gcsss1"},
         {kSysIsGCSPUSHM, "gcspushm"}}},
       {"sysl_rc_systeminstrs"_h,
        {{kSyslIsGCSPOPM, "gcspopm"}, {kSyslIsGCSSS2, "gcsss2"}}},
       {"dsb_bo_barriers"_h, {{kCrmIs0, "ssbb"}, {kCrmIs4, "pssbb"}}},
       {"ldaddb_32_memop"_h, {{kRdIsZROrSP, "staddb"}}},
       {"ldaddh_32_memop"_h, {{kRdIsZROrSP, "staddh"}}},
       {"ldaddlb_32_memop"_h, {{kRdIsZROrSP, "staddlb"}}},
       {"ldaddlh_32_memop"_h, {{kRdIsZROrSP, "staddlh"}}},
       {"ldaddl_32_memop"_h, {{kRdIsZROrSP, "staddl"}}},
       {"ldaddl_64_memop"_h, {{kRdIsZROrSP, "staddl"}}},
       {"ldadd_32_memop"_h, {{kRdIsZROrSP, "stadd"}}},
       {"ldadd_64_memop"_h, {{kRdIsZROrSP, "stadd"}}},
       {"ldclrb_32_memop"_h, {{kRdIsZROrSP, "stclrb"}}},
       {"ldclrh_32_memop"_h, {{kRdIsZROrSP, "stclrh"}}},
       {"ldclrlb_32_memop"_h, {{kRdIsZROrSP, "stclrlb"}}},
       {"ldclrlh_32_memop"_h, {{kRdIsZROrSP, "stclrlh"}}},
       {"ldclrl_32_memop"_h, {{kRdIsZROrSP, "stclrl"}}},
       {"ldclrl_64_memop"_h, {{kRdIsZROrSP, "stclrl"}}},
       {"ldclr_32_memop"_h, {{kRdIsZROrSP, "stclr"}}},
       {"ldclr_64_memop"_h, {{kRdIsZROrSP, "stclr"}}},
       {"ldeorb_32_memop"_h, {{kRdIsZROrSP, "steorb"}}},
       {"ldeorh_32_memop"_h, {{kRdIsZROrSP, "steorh"}}},
       {"ldeorlb_32_memop"_h, {{kRdIsZROrSP, "steorlb"}}},
       {"ldeorlh_32_memop"_h, {{kRdIsZROrSP, "steorlh"}}},
       {"ldeorl_32_memop"_h, {{kRdIsZROrSP, "steorl"}}},
       {"ldeorl_64_memop"_h, {{kRdIsZROrSP, "steorl"}}},
       {"ldeor_32_memop"_h, {{kRdIsZROrSP, "steor"}}},
       {"ldeor_64_memop"_h, {{kRdIsZROrSP, "steor"}}},
       {"ldsetb_32_memop"_h, {{kRdIsZROrSP, "stsetb"}}},
       {"ldseth_32_memop"_h, {{kRdIsZROrSP, "stseth"}}},
       {"ldsetlb_32_memop"_h, {{kRdIsZROrSP, "stsetlb"}}},
       {"ldsetlh_32_memop"_h, {{kRdIsZROrSP, "stsetlh"}}},
       {"ldsetl_32_memop"_h, {{kRdIsZROrSP, "stsetl"}}},
       {"ldsetl_64_memop"_h, {{kRdIsZROrSP, "stsetl"}}},
       {"ldset_32_memop"_h, {{kRdIsZROrSP, "stset"}}},
       {"ldset_64_memop"_h, {{kRdIsZROrSP, "stset"}}},
       {"ldsmaxb_32_memop"_h, {{kRdIsZROrSP, "stsmaxb"}}},
       {"ldsmaxh_32_memop"_h, {{kRdIsZROrSP, "stsmaxh"}}},
       {"ldsmaxlb_32_memop"_h, {{kRdIsZROrSP, "stsmaxlb"}}},
       {"ldsmaxlh_32_memop"_h, {{kRdIsZROrSP, "stsmaxlh"}}},
       {"ldsmaxl_32_memop"_h, {{kRdIsZROrSP, "stsmaxl"}}},
       {"ldsmaxl_64_memop"_h, {{kRdIsZROrSP, "stsmaxl"}}},
       {"ldsmax_32_memop"_h, {{kRdIsZROrSP, "stsmax"}}},
       {"ldsmax_64_memop"_h, {{kRdIsZROrSP, "stsmax"}}},
       {"ldsminb_32_memop"_h, {{kRdIsZROrSP, "stsminb"}}},
       {"ldsminh_32_memop"_h, {{kRdIsZROrSP, "stsminh"}}},
       {"ldsminlb_32_memop"_h, {{kRdIsZROrSP, "stsminlb"}}},
       {"ldsminlh_32_memop"_h, {{kRdIsZROrSP, "stsminlh"}}},
       {"ldsminl_32_memop"_h, {{kRdIsZROrSP, "stsminl"}}},
       {"ldsminl_64_memop"_h, {{kRdIsZROrSP, "stsminl"}}},
       {"ldsmin_32_memop"_h, {{kRdIsZROrSP, "stsmin"}}},
       {"ldsmin_64_memop"_h, {{kRdIsZROrSP, "stsmin"}}},
       {"ldumaxb_32_memop"_h, {{kRdIsZROrSP, "stumaxb"}}},
       {"ldumaxh_32_memop"_h, {{kRdIsZROrSP, "stumaxh"}}},
       {"ldumaxlb_32_memop"_h, {{kRdIsZROrSP, "stumaxlb"}}},
       {"ldumaxlh_32_memop"_h, {{kRdIsZROrSP, "stumaxlh"}}},
       {"ldumaxl_32_memop"_h, {{kRdIsZROrSP, "stumaxl"}}},
       {"ldumaxl_64_memop"_h, {{kRdIsZROrSP, "stumaxl"}}},
       {"ldumax_32_memop"_h, {{kRdIsZROrSP, "stumax"}}},
       {"ldumax_64_memop"_h, {{kRdIsZROrSP, "stumax"}}},
       {"lduminb_32_memop"_h, {{kRdIsZROrSP, "stuminb"}}},
       {"lduminh_32_memop"_h, {{kRdIsZROrSP, "stuminh"}}},
       {"lduminlb_32_memop"_h, {{kRdIsZROrSP, "stuminlb"}}},
       {"lduminlh_32_memop"_h, {{kRdIsZROrSP, "stuminlh"}}},
       {"lduminl_32_memop"_h, {{kRdIsZROrSP, "stuminl"}}},
       {"lduminl_64_memop"_h, {{kRdIsZROrSP, "stuminl"}}},
       {"ldumin_32_memop"_h, {{kRdIsZROrSP, "stumin"}}},
       {"ldumin_64_memop"_h, {{kRdIsZROrSP, "stumin"}}}};

  // "Complex" alias detection. For each form, one or more function groups are
  // applied to the encoding. If ALL of the functions in a group return true
  // (ie. intersection) then the alias for that group is used.
  using FuncAlias = struct {
    std::vector<std::function<bool(const Instruction *)>> conditions;
    std::string alias;
  };

  auto RnRmAliased = [](const Instruction *i) {
    return (i->GetRn() == i->GetRm());
  };

  auto RdRmAliased = [](const Instruction *i) {
    return (i->GetRd() == i->GetRm());
  };

  auto RdRnAliased = [](const Instruction *i) {
    return (i->GetRd() == i->GetRn());
  };

  auto PnPmAliased = [](const Instruction *i) {
    return (i->GetPn() == i->GetPm());
  };

  auto PgPmAliased = [](const Instruction *i) {
    return (i->ExtractBits(13, 10) == static_cast<uint32_t>(i->GetPm()));
  };

  auto PdPmAliased = [](const Instruction *i) {
    return (i->GetPd() == i->GetPm());
  };

  auto CondNotAlNv = [](const Instruction *i) {
    return (i->GetCondition() != al) && (i->GetCondition() != nv);
  };

  auto IsNotMovzMovnImmW = [this](const Instruction *i) {
    return !IsMovzMovnImm(kWRegSize, i->GetImmLogical());
  };

  auto IsNotMovzMovnImmX = [this](const Instruction *i) {
    return !IsMovzMovnImm(kXRegSize, i->GetImmLogical());
  };

  auto IsNonOnesMov = [](const Instruction *i) {
    return i->GetImmMoveWide() != 0xffff;
  };

  auto IsNonZeroNoShiftMov = [](const Instruction *i) {
    return i->GetImmMoveWide() || (i->GetShiftMoveWide() == 0);
  };

  auto BitfieldSLessThanR = [](const Instruction *i) {
    return i->GetImmS() < i->GetImmR();
  };

  auto BitfieldRIsSPlus1 = [](const Instruction *i) {
    return i->GetImmR() == (i->GetImmS() + 1);
  };

  auto DupHasOneSetBit = [](const Instruction *i) {
    return CountSetBits(i->ExtractBits(23, 22)) +
               CountSetBits(i->ExtractBits(20, 16)) ==
           1;
  };

  auto IsDCOperation = [](const Instruction *i) {
    std::unordered_set<uint32_t> ops = {CVAC,
                                        CVAU,
                                        CVAP,
                                        CVADP,
                                        CIVAC,
                                        ZVA,
                                        GVA,
                                        GZVA,
                                        CGVAC,
                                        CGDVAC,
                                        CGVAP,
                                        CGDVAP,
                                        CIGVAC,
                                        CIGDVAC};
    return ops.count(i->GetSysOp()) == 1;
  };

  auto AllCases = [](const Instruction *i) {
    USE(i);
    return true;
  };

  using FuncAliasMap = std::unordered_map<uint32_t, std::vector<FuncAlias>>;
  static const FuncAliasMap funcmap =
      {{"csinc_32_condsel"_h,
        {{{RnIsZROrSP, RmIsZROrSP, CondNotAlNv}, "cset"},
         {{RnRmAliased, CondNotAlNv}, "cinc"}}},
       {"csinc_64_condsel"_h,
        {{{RnIsZROrSP, RmIsZROrSP, CondNotAlNv}, "cset"},
         {{RnRmAliased, CondNotAlNv}, "cinc"}}},
       {"csinv_32_condsel"_h,
        {{{RnIsZROrSP, RmIsZROrSP, CondNotAlNv}, "csetm"},
         {{RnRmAliased, CondNotAlNv}, "cinv"}}},
       {"csinv_64_condsel"_h,
        {{{RnIsZROrSP, RmIsZROrSP, CondNotAlNv}, "csetm"},
         {{RnRmAliased, CondNotAlNv}, "cinv"}}},
       {"csneg_32_condsel"_h, {{{RnRmAliased, CondNotAlNv}, "cneg"}}},
       {"csneg_64_condsel"_h, {{{RnRmAliased, CondNotAlNv}, "cneg"}}},
       {"extr_32_extract"_h, {{{RnRmAliased}, "ror"}}},
       {"extr_64_extract"_h, {{{RnRmAliased}, "ror"}}},
       {"orr_32_log_imm"_h, {{{RnIsZROrSP, IsNotMovzMovnImmW}, "mov"}}},
       {"orr_64_log_imm"_h, {{{RnIsZROrSP, IsNotMovzMovnImmX}, "mov"}}},
       {"sbfm_32m_bitfield"_h,
        {{{BitfieldSLessThanR}, "sbfiz"}, {{AllCases}, "sbfx"}}},
       {"sbfm_64m_bitfield"_h,
        {{{BitfieldSLessThanR}, "sbfiz"}, {{AllCases}, "sbfx"}}},
       {"ubfm_32m_bitfield"_h,
        {{{BitfieldRIsSPlus1}, "lsl"},
         {{BitfieldSLessThanR}, "ubfiz"},
         {{AllCases}, "ubfx"}}},
       {"ubfm_64m_bitfield"_h,
        {{{BitfieldRIsSPlus1}, "lsl"},
         {{BitfieldSLessThanR}, "ubfiz"},
         {{AllCases}, "ubfx"}}},
       {"bfm_32m_bitfield"_h,
        {{{BitfieldSLessThanR, RnIsZROrSP}, "bfc"},
         {{BitfieldSLessThanR}, "bfi"},
         {{AllCases}, "bfxil"}}},
       {"bfm_64m_bitfield"_h,
        {{{BitfieldSLessThanR, RnIsZROrSP}, "bfc"},
         {{BitfieldSLessThanR}, "bfi"},
         {{AllCases}, "bfxil"}}},
       {"orr_asimdsame_only"_h, {{{RnRmAliased}, "mov"}}},
       {"orr_z_zz"_h, {{{RnRmAliased}, "mov"}}},
       {"sel_z_p_zz"_h, {{{RdRmAliased}, "mov"}}},
       {"ands_p_p_pp_z"_h, {{{PnPmAliased}, "movs"}}},
       {"and_p_p_pp_z"_h, {{{PnPmAliased}, "mov"}}},
       {"eors_p_p_pp_z"_h, {{{PgPmAliased}, "nots"}}},
       {"eor_p_p_pp_z"_h, {{{PgPmAliased}, "not"}}},
       {"orrs_p_p_pp_z"_h, {{{PnPmAliased, PgPmAliased}, "movs"}}},
       {"orr_p_p_pp_z"_h, {{{PnPmAliased, PgPmAliased}, "mov"}}},
       {"sel_p_p_pp"_h, {{{PdPmAliased}, "mov"}}},
       {"movz_32_movewide"_h, {{{IsNonZeroNoShiftMov}, "mov"}}},
       {"movz_64_movewide"_h, {{{IsNonZeroNoShiftMov}, "mov"}}},
       {"movn_32_movewide"_h, {{{IsNonZeroNoShiftMov, IsNonOnesMov}, "mov"}}},
       {"movn_64_movewide"_h, {{{IsNonZeroNoShiftMov}, "mov"}}},
       {"dup_z_zi"_h, {{{DupHasOneSetBit}, "mov_1"}, {{AllCases}, "mov"}}},
       {"sys_cr_systeminstrs"_h, {{{IsDCOperation}, "dc"}}},
       {"cpyen_cpy_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"cpyern_cpy_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"cpyewn_cpy_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"cpye_cpy_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"cpyfen_cpy_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"cpyfern_cpy_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"cpyfewn_cpy_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"cpyfe_cpy_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"cpyfmn_cpy_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"cpyfmrn_cpy_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"cpyfmwn_cpy_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"cpyfm_cpy_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"cpyfpn_cpy_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"cpyfprn_cpy_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"cpyfpwn_cpy_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"cpyfp_cpy_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"cpymn_cpy_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"cpymrn_cpy_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"cpymwn_cpy_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"cpym_cpy_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"cpypn_cpy_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"cpyprn_cpy_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"cpypwn_cpy_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"cpyp_cpy_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"seten_set_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"sete_set_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"setgen_set_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"setge_set_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"setgmn_set_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"setgm_set_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"setgpn_set_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"setgp_set_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"setmn_set_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"setm_set_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"setpn_set_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}},
       {"setp_set_memcms"_h,
        {{{RdRnAliased}, "unallocated"},
         {{RdRmAliased}, "unallocated"},
         {{RnRmAliased}, "unallocated"}}}};

  // Check simple aliases.
  std::string alias;
  MaskAliasMap::const_iterator ita = maskmap.find(form_hash_);
  if (ita != maskmap.end()) {
    for (auto rule : ita->second) {
      uint64_t mv = rule.mask_value;
      uint32_t mask = mv >> 32;
      uint32_t value = mv & 0xffffffff;
      if ((mask == 0) || (instr->Mask(mask) == value)) {
        alias = rule.alias;
        break;
      }
    }
  }

  // If there was no simple alias, check for a complex one.
  if (alias.length() == 0) {
    FuncAliasMap::const_iterator ita2 = funcmap.find(form_hash_);
    if (ita2 != funcmap.end()) {
      for (auto rule : ita2->second) {
        bool all = true;
        for (auto cond : rule.conditions) {
          all = all && cond(instr);
        }
        if (all == true) {
          alias = rule.alias;
          break;
        }
      }
    }
  }

  return alias;
}

void Disassembler::PopulateFormToStringMap(FormToStringMap *fts) {
  using StringToFormMap =
      std::unordered_map<std::string, std::unordered_set<uint32_t>>;

  // Map from disassembler format string to instruction form that uses it. On
  // object construction, this is used to build a map from instruction to
  // disassembler string, allowing fast lookup during disassembly.
  static const StringToFormMap forms = {
      {"", {"autia1716_hi_hints"_h,   "autiasp_hi_hints"_h,
            "autiaz_hi_hints"_h,      "autib1716_hi_hints"_h,
            "autibsp_hi_hints"_h,     "autibz_hi_hints"_h,
            "axflag_m_pstate"_h,      "cfinv_m_pstate"_h,
            "csdb_hi_hints"_h,        "dgh_hi_hints"_h,
            "ssbb_only_barriers"_h,   "esb_hi_hints"_h,
            "isb_bi_barriers"_h,      "nop_hi_hints"_h,
            "pacia1716_hi_hints"_h,   "paciasp_hi_hints"_h,
            "paciaz_hi_hints"_h,      "pacib1716_hi_hints"_h,
            "pacibsp_hi_hints"_h,     "pacibz_hi_hints"_h,
            "sb_only_barriers"_h,     "setffr_f"_h,
            "sev_hi_hints"_h,         "sevl_hi_hints"_h,
            "wfe_hi_hints"_h,         "wfi_hi_hints"_h,
            "xaflag_m_pstate"_h,      "xpaclri_hi_hints"_h,
            "yield_hi_hints"_h,       "retaa_64e_branch_reg"_h,
            "retab_64e_branch_reg"_h, "ssbb_dsb_bo_barriers"_h,
            "pssbb_dsb_bo_barriers"_h}},
      {"(Unallocated)",
       {"unallocated_cpyen_cpy_memcms"_h,   "unallocated_cpyern_cpy_memcms"_h,
        "unallocated_cpyewn_cpy_memcms"_h,  "unallocated_cpye_cpy_memcms"_h,
        "unallocated_cpyfen_cpy_memcms"_h,  "unallocated_cpyfern_cpy_memcms"_h,
        "unallocated_cpyfewn_cpy_memcms"_h, "unallocated_cpyfe_cpy_memcms"_h,
        "unallocated_cpyfmn_cpy_memcms"_h,  "unallocated_cpyfmrn_cpy_memcms"_h,
        "unallocated_cpyfmwn_cpy_memcms"_h, "unallocated_cpyfm_cpy_memcms"_h,
        "unallocated_cpyfpn_cpy_memcms"_h,  "unallocated_cpyfprn_cpy_memcms"_h,
        "unallocated_cpyfpwn_cpy_memcms"_h, "unallocated_cpyfp_cpy_memcms"_h,
        "unallocated_cpymn_cpy_memcms"_h,   "unallocated_cpymrn_cpy_memcms"_h,
        "unallocated_cpymwn_cpy_memcms"_h,  "unallocated_cpym_cpy_memcms"_h,
        "unallocated_cpypn_cpy_memcms"_h,   "unallocated_cpyprn_cpy_memcms"_h,
        "unallocated_cpypwn_cpy_memcms"_h,  "unallocated_cpyp_cpy_memcms"_h,
        "unallocated_seten_set_memcms"_h,   "unallocated_sete_set_memcms"_h,
        "unallocated_setgen_set_memcms"_h,  "unallocated_setge_set_memcms"_h,
        "unallocated_setgmn_set_memcms"_h,  "unallocated_setgm_set_memcms"_h,
        "unallocated_setgpn_set_memcms"_h,  "unallocated_setgp_set_memcms"_h,
        "unallocated_setmn_set_memcms"_h,   "unallocated_setm_set_memcms"_h,
        "unallocated_setpn_set_memcms"_h,   "unallocated_setp_set_memcms"_h}},
      {"['Xd]!, ['Xs]!, 'Xn!",
       {"cpyen_cpy_memcms"_h,   "cpyern_cpy_memcms"_h,  "cpyewn_cpy_memcms"_h,
        "cpye_cpy_memcms"_h,    "cpyfen_cpy_memcms"_h,  "cpyfern_cpy_memcms"_h,
        "cpyfewn_cpy_memcms"_h, "cpyfe_cpy_memcms"_h,   "cpyfmn_cpy_memcms"_h,
        "cpyfmrn_cpy_memcms"_h, "cpyfmwn_cpy_memcms"_h, "cpyfm_cpy_memcms"_h,
        "cpyfpn_cpy_memcms"_h,  "cpyfprn_cpy_memcms"_h, "cpyfpwn_cpy_memcms"_h,
        "cpyfp_cpy_memcms"_h,   "cpymn_cpy_memcms"_h,   "cpymrn_cpy_memcms"_h,
        "cpymwn_cpy_memcms"_h,  "cpym_cpy_memcms"_h,    "cpypn_cpy_memcms"_h,
        "cpyprn_cpy_memcms"_h,  "cpypwn_cpy_memcms"_h,  "cpyp_cpy_memcms"_h}},
      {"['Xd]!, 'Xn!, 'Xs",
       {"seten_set_memcms"_h,
        "sete_set_memcms"_h,
        "setgen_set_memcms"_h,
        "setge_set_memcms"_h,
        "setgmn_set_memcms"_h,
        "setgm_set_memcms"_h,
        "setgpn_set_memcms"_h,
        "setgp_set_memcms"_h,
        "setmn_set_memcms"_h,
        "setm_set_memcms"_h,
        "setpn_set_memcms"_h,
        "setp_set_memcms"_h}},
      {"#'u1105", {"hint_hm_hints"_h}},
      {"#'u1816, C'u1512, C'u1108, #'u0705'(0400=31?:, 'Xt)",
       {"sys_cr_systeminstrs"_h}},
      {"#0x'x2005",
       {"brk_ex_exception"_h,
        "hlt_ex_exception"_h,
        "hvc_ex_exception"_h,
        "smc_ex_exception"_h,
        "svc_ex_exception"_h}},
      {"'{dcop}, 'Xt", {"dc_sys_cr_systeminstrs"_h}},
      {"'[barrier]", {"dsb_bo_barriers"_h, "dmb_bo_barriers"_h}},
      {"'[sz]'u0400, '[sz]'u0905",
       {"sqabs_asisdmisc_r"_h,
        "sqneg_asisdmisc_r"_h,
        "suqadd_asisdmisc_r"_h,
        "usqadd_asisdmisc_r"_h}},
      {"'[sz]'u0400, '[nscall]'u0905",
       {"sqxtn_asisdmisc_n"_h, "sqxtun_asisdmisc_n"_h, "uqxtn_asisdmisc_n"_h}},
      {"'[sz]'u0400, '[sz]'u0905, '[sz]'u2016",
       {"sqadd_asisdsame_only"_h,
        "sqdmulh_asisdsame_only"_h,
        "sqrdmulh_asisdsame_only"_h,
        "sqrshl_asisdsame_only"_h,
        "sqshl_asisdsame_only"_h,
        "sqsub_asisdsame_only"_h,
        "srshl_asisdsame_only"_h,
        "sshl_asisdsame_only"_h,
        "uqadd_asisdsame_only"_h,
        "uqrshl_asisdsame_only"_h,
        "uqshl_asisdsame_only"_h,
        "uqsub_asisdsame_only"_h,
        "urshl_asisdsame_only"_h,
        "ushl_asisdsame_only"_h,
        "sqrdmlah_asisdsame2_only"_h,
        "sqrdmlsh_asisdsame2_only"_h}},
      {"'[sz]'u0400, '[sz]'u0905, 'Vf.'[sz]['<u11_2119 u2322 lsr>]",
       {"sqdmulh_asisdelem_r"_h,
        "sqrdmlah_asisdelem_r"_h,
        "sqrdmlsh_asisdelem_r"_h,
        "sqrdmulh_asisdelem_r"_h}},
      {"'[sz]'u0400, 'Vn.'[n]",
       {"addv_asimdall_only"_h,
        "smaxv_asimdall_only"_h,
        "sminv_asimdall_only"_h,
        "umaxv_asimdall_only"_h,
        "uminv_asimdall_only"_h}},
      {"'[nscall]'u0400, 'Vn.'[n]",
       {"saddlv_asimdall_only"_h, "uaddlv_asimdall_only"_h}},
      {"'[nscall]'u0400, '[sz]'u0905, '[sz]'u2016",
       {"sqdmlal_asisddiff_only"_h,
        "sqdmlsl_asisddiff_only"_h,
        "sqdmull_asisddiff_only"_h}},
      {"'[nscall]'u0400, '[sz]'u0905, 'Vf.'[sz]['<u11_2119 u2322 lsr>]",
       {"sqdmlal_asisdelem_l"_h,
        "sqdmlsl_asisdelem_l"_h,
        "sqdmull_asisdelem_l"_h}},
      {"'[nshiftscal]'u0400, '[nshiftscal]'u0905, #'<u2216 8 31 u2219 clz32 - "
       "lsl ->",
       {"sqshlu_asisdshf_r"_h, "sqshl_asisdshf_r"_h, "uqshl_asisdshf_r"_h}},
      {"'[nshiftscal]'u0400, '[nshiftscal]'u0905, #'<16 31 u2219 clz32 - lsl "
       "u2216 ->",
       {"fcvtzs_asisdshf_c"_h,
        "fcvtzu_asisdshf_c"_h,
        "scvtf_asisdshf_c"_h,
        "ucvtf_asisdshf_c"_h}},
      {"'[ntriscal]'u0400, 'Vn.'[ntriscal]['<u2016 dup ctz 1 + lsr>]",
       {"mov_dup_asisdone_only"_h}},
      {"'(0400=31?:'Xt)", {"gcspopm_sysl_rc_systeminstrs"_h}},
      {"'(07?j)'(06?c)", {"bti_hb_hints"_h}},
      {"'(0905=30?:'Xn)", {"ret_64r_branch_reg"_h}},
      {"'(1108=15?:#0x'x1108)", {"clrex_bn_barriers"_h}},
      {"'(21?s:'?20:hb)'u0400, '(21?d:'?20:sh)'u0905, #'<16 31 u2219 clz32 - "
       "lsl u2216 ->",
       {"sqrshrn_asisdshf_n"_h,
        "sqrshrun_asisdshf_n"_h,
        "sqshrn_asisdshf_n"_h,
        "sqshrun_asisdshf_n"_h,
        "uqrshrn_asisdshf_n"_h,
        "uqshrn_asisdshf_n"_h}},
      {"'(2322=3?'Xd:'Wd), 'Pgl, '(2322=3?'Xd:'Wd), 'Zn.'[sz]",
       {"clasta_r_p_z"_h, "clastb_r_p_z"_h}},
      {"'(2322=3?'Xd:'Wd), 'Pgl, 'Zn.'[sz]",
       {"lasta_r_p_z"_h, "lastb_r_p_z"_h}},
      {"'Wt, ['Xns]", {"ldaprb_32l_memop"_h,     "ldaprh_32l_memop"_h,
                       "ldapr_32l_memop"_h,      "ldarb_lr32_ldstexcl"_h,
                       "ldarh_lr32_ldstexcl"_h,  "ldar_lr32_ldstexcl"_h,
                       "ldaxrb_lr32_ldstexcl"_h, "ldaxrh_lr32_ldstexcl"_h,
                       "ldaxr_lr32_ldstexcl"_h,  "ldlarb_lr32_ldstexcl"_h,
                       "ldlarh_lr32_ldstexcl"_h, "ldlar_lr32_ldstexcl"_h,
                       "ldxrb_lr32_ldstexcl"_h,  "ldxrh_lr32_ldstexcl"_h,
                       "ldxr_lr32_ldstexcl"_h,   "stllrb_sl32_ldstexcl"_h,
                       "stllrh_sl32_ldstexcl"_h, "stllr_sl32_ldstexcl"_h,
                       "stlrb_sl32_ldstexcl"_h,  "stlrh_sl32_ldstexcl"_h,
                       "stlr_sl32_ldstexcl"_h}},
      {"'Xt, ['Xns]",
       {"ldapr_64l_memop"_h,
        "ldxr_lr64_ldstexcl"_h,
        "ldaxr_lr64_ldstexcl"_h,
        "ldar_lr64_ldstexcl"_h,
        "ldlar_lr64_ldstexcl"_h,
        "stlr_sl64_ldstexcl"_h,
        "stllr_sl64_ldstexcl"_h}},
      {"'Ws, ['Xns]",
       {"staddb_ldaddb_32_memop"_h,     "staddh_ldaddh_32_memop"_h,
        "staddlb_ldaddlb_32_memop"_h,   "staddlh_ldaddlh_32_memop"_h,
        "staddl_ldaddl_32_memop"_h,     "stadd_ldadd_32_memop"_h,
        "stclrb_ldclrb_32_memop"_h,     "stclrh_ldclrh_32_memop"_h,
        "stclrlb_ldclrlb_32_memop"_h,   "stclrlh_ldclrlh_32_memop"_h,
        "stclrl_ldclrl_32_memop"_h,     "stclr_ldclr_32_memop"_h,
        "steorb_ldeorb_32_memop"_h,     "steorh_ldeorh_32_memop"_h,
        "steorlb_ldeorlb_32_memop"_h,   "steorlh_ldeorlh_32_memop"_h,
        "steorl_ldeorl_32_memop"_h,     "steor_ldeor_32_memop"_h,
        "stsetb_ldsetb_32_memop"_h,     "stseth_ldseth_32_memop"_h,
        "stsetlb_ldsetlb_32_memop"_h,   "stsetlh_ldsetlh_32_memop"_h,
        "stsetl_ldsetl_32_memop"_h,     "stset_ldset_32_memop"_h,
        "stsmaxb_ldsmaxb_32_memop"_h,   "stsmaxh_ldsmaxh_32_memop"_h,
        "stsmaxlb_ldsmaxlb_32_memop"_h, "stsmaxlh_ldsmaxlh_32_memop"_h,
        "stsmaxl_ldsmaxl_32_memop"_h,   "stsmax_ldsmax_32_memop"_h,
        "stsminb_ldsminb_32_memop"_h,   "stsminh_ldsminh_32_memop"_h,
        "stsminlb_ldsminlb_32_memop"_h, "stsminlh_ldsminlh_32_memop"_h,
        "stsminl_ldsminl_32_memop"_h,   "stsmin_ldsmin_32_memop"_h,
        "stumaxb_ldumaxb_32_memop"_h,   "stumaxh_ldumaxh_32_memop"_h,
        "stumaxlb_ldumaxlb_32_memop"_h, "stumaxlh_ldumaxlh_32_memop"_h,
        "stumaxl_ldumaxl_32_memop"_h,   "stumax_ldumax_32_memop"_h,
        "stuminb_lduminb_32_memop"_h,   "stuminh_lduminh_32_memop"_h,
        "stuminlb_lduminlb_32_memop"_h, "stuminlh_lduminlh_32_memop"_h,
        "stuminl_lduminl_32_memop"_h,   "stumin_ldumin_32_memop"_h}},
      {"'Xs, ['Xns]",
       {"staddl_ldaddl_64_memop"_h,
        "stadd_ldadd_64_memop"_h,
        "stclrl_ldclrl_64_memop"_h,
        "stclr_ldclr_64_memop"_h,
        "steorl_ldeorl_64_memop"_h,
        "steor_ldeor_64_memop"_h,
        "stsetl_ldsetl_64_memop"_h,
        "stset_ldset_64_memop"_h,
        "stsmaxl_ldsmaxl_64_memop"_h,
        "stsmax_ldsmax_64_memop"_h,
        "stsminl_ldsminl_64_memop"_h,
        "stsmin_ldsmin_64_memop"_h,
        "stumaxl_ldumaxl_64_memop"_h,
        "stumax_ldumax_64_memop"_h,
        "stuminl_lduminl_64_memop"_h,
        "stumin_ldumin_64_memop"_h}},
      {"'Ws, 'Ws+, 'Wt, 'Wt+, ['Xns]",
       {"casp_cp32_ldstexcl"_h,
        "caspa_cp32_ldstexcl"_h,
        "caspl_cp32_ldstexcl"_h,
        "caspal_cp32_ldstexcl"_h}},
      {"'Ws, 'Wt, 'Wt2, ['Xns]",
       {"stxp_sp32_ldstexcl"_h, "stlxp_sp32_ldstexcl"_h}},
      {"'Ws, 'Wt, ['Xns]", {"ldaddab_32_memop"_h,     "ldaddah_32_memop"_h,
                            "ldaddalb_32_memop"_h,    "ldaddalh_32_memop"_h,
                            "ldaddal_32_memop"_h,     "ldadda_32_memop"_h,
                            "ldaddb_32_memop"_h,      "ldaddh_32_memop"_h,
                            "ldaddlb_32_memop"_h,     "ldaddlh_32_memop"_h,
                            "ldaddl_32_memop"_h,      "ldadd_32_memop"_h,
                            "ldclrab_32_memop"_h,     "ldclrah_32_memop"_h,
                            "ldclralb_32_memop"_h,    "ldclralh_32_memop"_h,
                            "ldclral_32_memop"_h,     "ldclra_32_memop"_h,
                            "ldclrb_32_memop"_h,      "ldclrh_32_memop"_h,
                            "ldclrlb_32_memop"_h,     "ldclrlh_32_memop"_h,
                            "ldclrl_32_memop"_h,      "ldclr_32_memop"_h,
                            "ldeorab_32_memop"_h,     "ldeorah_32_memop"_h,
                            "ldeoralb_32_memop"_h,    "ldeoralh_32_memop"_h,
                            "ldeoral_32_memop"_h,     "ldeora_32_memop"_h,
                            "ldeorb_32_memop"_h,      "ldeorh_32_memop"_h,
                            "ldeorlb_32_memop"_h,     "ldeorlh_32_memop"_h,
                            "ldeorl_32_memop"_h,      "ldeor_32_memop"_h,
                            "ldsetab_32_memop"_h,     "ldsetah_32_memop"_h,
                            "ldsetalb_32_memop"_h,    "ldsetalh_32_memop"_h,
                            "ldsetal_32_memop"_h,     "ldseta_32_memop"_h,
                            "ldsetb_32_memop"_h,      "ldseth_32_memop"_h,
                            "ldsetlb_32_memop"_h,     "ldsetlh_32_memop"_h,
                            "ldsetl_32_memop"_h,      "ldset_32_memop"_h,
                            "ldsmaxab_32_memop"_h,    "ldsmaxah_32_memop"_h,
                            "ldsmaxalb_32_memop"_h,   "ldsmaxalh_32_memop"_h,
                            "ldsmaxal_32_memop"_h,    "ldsmaxa_32_memop"_h,
                            "ldsmaxb_32_memop"_h,     "ldsmaxh_32_memop"_h,
                            "ldsmaxlb_32_memop"_h,    "ldsmaxlh_32_memop"_h,
                            "ldsmaxl_32_memop"_h,     "ldsmax_32_memop"_h,
                            "ldsminab_32_memop"_h,    "ldsminah_32_memop"_h,
                            "ldsminalb_32_memop"_h,   "ldsminalh_32_memop"_h,
                            "ldsminal_32_memop"_h,    "ldsmina_32_memop"_h,
                            "ldsminb_32_memop"_h,     "ldsminh_32_memop"_h,
                            "ldsminlb_32_memop"_h,    "ldsminlh_32_memop"_h,
                            "ldsminl_32_memop"_h,     "ldsmin_32_memop"_h,
                            "ldumaxab_32_memop"_h,    "ldumaxah_32_memop"_h,
                            "ldumaxalb_32_memop"_h,   "ldumaxalh_32_memop"_h,
                            "ldumaxal_32_memop"_h,    "ldumaxa_32_memop"_h,
                            "ldumaxb_32_memop"_h,     "ldumaxh_32_memop"_h,
                            "ldumaxlb_32_memop"_h,    "ldumaxlh_32_memop"_h,
                            "ldumaxl_32_memop"_h,     "ldumax_32_memop"_h,
                            "lduminab_32_memop"_h,    "lduminah_32_memop"_h,
                            "lduminalb_32_memop"_h,   "lduminalh_32_memop"_h,
                            "lduminal_32_memop"_h,    "ldumina_32_memop"_h,
                            "lduminb_32_memop"_h,     "lduminh_32_memop"_h,
                            "lduminlb_32_memop"_h,    "lduminlh_32_memop"_h,
                            "lduminl_32_memop"_h,     "ldumin_32_memop"_h,
                            "swpab_32_memop"_h,       "swpah_32_memop"_h,
                            "swpalb_32_memop"_h,      "swpalh_32_memop"_h,
                            "swpal_32_memop"_h,       "swpa_32_memop"_h,
                            "swpb_32_memop"_h,        "swph_32_memop"_h,
                            "swplb_32_memop"_h,       "swplh_32_memop"_h,
                            "swpl_32_memop"_h,        "swp_32_memop"_h,
                            "cas_c32_ldstexcl"_h,     "casa_c32_ldstexcl"_h,
                            "casl_c32_ldstexcl"_h,    "casal_c32_ldstexcl"_h,
                            "casb_c32_ldstexcl"_h,    "casab_c32_ldstexcl"_h,
                            "caslb_c32_ldstexcl"_h,   "casalb_c32_ldstexcl"_h,
                            "cash_c32_ldstexcl"_h,    "casah_c32_ldstexcl"_h,
                            "caslh_c32_ldstexcl"_h,   "casalh_c32_ldstexcl"_h,
                            "stxrb_sr32_ldstexcl"_h,  "stxrh_sr32_ldstexcl"_h,
                            "stxr_sr32_ldstexcl"_h,   "stlxrb_sr32_ldstexcl"_h,
                            "stlxrh_sr32_ldstexcl"_h, "stlxr_sr32_ldstexcl"_h}},
      {"'Ws, 'Xt, 'Xt2, ['Xns]",
       {"stxp_sp64_ldstexcl"_h, "stlxp_sp64_ldstexcl"_h}},
      {"'Ws, 'Xt, ['Xns]", {"stxr_sr64_ldstexcl"_h, "stlxr_sr64_ldstexcl"_h}},
      {"'Xs, 'Xs+, 'Xt, 'Xt+, ['Xns]",
       {"casp_cp64_ldstexcl"_h,
        "caspa_cp64_ldstexcl"_h,
        "caspl_cp64_ldstexcl"_h,
        "caspal_cp64_ldstexcl"_h}},
      {"'Xs, 'Xt, ['Xns]",
       {"ldaddal_64_memop"_h,  "ldadda_64_memop"_h,   "ldaddl_64_memop"_h,
        "ldadd_64_memop"_h,    "ldclral_64_memop"_h,  "ldclra_64_memop"_h,
        "ldclrl_64_memop"_h,   "ldclr_64_memop"_h,    "ldeoral_64_memop"_h,
        "ldeora_64_memop"_h,   "ldeorl_64_memop"_h,   "ldeor_64_memop"_h,
        "ldsetal_64_memop"_h,  "ldseta_64_memop"_h,   "ldsetl_64_memop"_h,
        "ldset_64_memop"_h,    "ldsmaxal_64_memop"_h, "ldsmaxa_64_memop"_h,
        "ldsmaxl_64_memop"_h,  "ldsmax_64_memop"_h,   "ldsminal_64_memop"_h,
        "ldsmina_64_memop"_h,  "ldsminl_64_memop"_h,  "ldsmin_64_memop"_h,
        "ldumaxal_64_memop"_h, "ldumaxa_64_memop"_h,  "ldumaxl_64_memop"_h,
        "ldumax_64_memop"_h,   "lduminal_64_memop"_h, "ldumina_64_memop"_h,
        "lduminl_64_memop"_h,  "ldumin_64_memop"_h,   "swpal_64_memop"_h,
        "swpa_64_memop"_h,     "swpl_64_memop"_h,     "swp_64_memop"_h,
        "cas_c64_ldstexcl"_h,  "casa_c64_ldstexcl"_h, "casl_c64_ldstexcl"_h,
        "casal_c64_ldstexcl"_h}},
      {"'?22:ds'u0400, '?22:ds'u0905",
       {"fcvtas_asisdmisc_r"_h,
        "fcvtau_asisdmisc_r"_h,
        "fcvtms_asisdmisc_r"_h,
        "fcvtmu_asisdmisc_r"_h,
        "fcvtns_asisdmisc_r"_h,
        "fcvtnu_asisdmisc_r"_h,
        "fcvtps_asisdmisc_r"_h,
        "fcvtpu_asisdmisc_r"_h,
        "fcvtzs_asisdmisc_r"_h,
        "fcvtzu_asisdmisc_r"_h,
        "frecpe_asisdmisc_r"_h,
        "frecpx_asisdmisc_r"_h,
        "frsqrte_asisdmisc_r"_h,
        "scvtf_asisdmisc_r"_h,
        "ucvtf_asisdmisc_r"_h}},
      {"'?22:ds'u0400, '?22:ds'u0905, #0.0",
       {"fcmeq_asisdmisc_fz"_h,
        "fcmge_asisdmisc_fz"_h,
        "fcmgt_asisdmisc_fz"_h,
        "fcmle_asisdmisc_fz"_h,
        "fcmlt_asisdmisc_fz"_h}},
      {"'?22:ds'u0400, '?22:ds'u0905, '?22:ds'u2016",
       {"fabd_asisdsame_only"_h,
        "facge_asisdsame_only"_h,
        "facgt_asisdsame_only"_h,
        "fcmeq_asisdsame_only"_h,
        "fcmge_asisdsame_only"_h,
        "fcmgt_asisdsame_only"_h,
        "fmulx_asisdsame_only"_h,
        "frecps_asisdsame_only"_h,
        "frsqrts_asisdsame_only"_h}},
      {"'?22:ds'u0400, '?22:ds'u0905, 'Vf.'?22:ds['<u11_2119 u2322 lsr>]",
       {"fmla_asisdelem_r_sd"_h,
        "fmls_asisdelem_r_sd"_h,
        "fmul_asisdelem_r_sd"_h,
        "fmulx_asisdelem_r_sd"_h}},
      {"'?22:ds'u0400, 'Vn.2'?22:ds",
       {"faddp_asisdpair_only_sd"_h,
        "fmaxnmp_asisdpair_only_sd"_h,
        "fmaxp_asisdpair_only_sd"_h,
        "fminnmp_asisdpair_only_sd"_h,
        "fminp_asisdpair_only_sd"_h}},
      {"'Bt, ['Xns'(2012?, #'s2012)]",
       {"ldur_b_ldst_unscaled"_h, "stur_b_ldst_unscaled"_h}},
      {"'Bt, ['Xns'(2110?, #'u2110)]",
       {"ldr_b_ldst_pos"_h, "str_b_ldst_pos"_h}},
      {"'Bt, ['Xns, #'s2012]!", {"ldr_b_ldst_immpre"_h, "str_b_ldst_immpre"_h}},
      {"'Bt, ['Xns, 'R13m'(1512=6?]'$), '[extmem]'(12? #0)]",
       {"ldr_b_ldst_regoff"_h,
        "ldr_bl_ldst_regoff"_h,
        "str_b_ldst_regoff"_h,
        "str_bl_ldst_regoff"_h}},
      {"'Bt, ['Xns], #'s2012",
       {"ldr_b_ldst_immpost"_h, "str_b_ldst_immpost"_h}},
      {"'Dd, 'Dn", {"abs_asisdmisc_r"_h, "neg_asisdmisc_r"_h}},
      {"'Dd, 'Dn, #0",
       {"cmeq_asisdmisc_z"_h,
        "cmge_asisdmisc_z"_h,
        "cmgt_asisdmisc_z"_h,
        "cmle_asisdmisc_z"_h,
        "cmlt_asisdmisc_z"_h}},
      {"'Dd, 'Dn, 'Dm",
       {"cmeq_asisdsame_only"_h,
        "cmge_asisdsame_only"_h,
        "cmgt_asisdsame_only"_h,
        "cmhi_asisdsame_only"_h,
        "cmhs_asisdsame_only"_h,
        "cmtst_asisdsame_only"_h,
        "add_asisdsame_only"_h,
        "sub_asisdsame_only"_h}},
      {"'Dd, 'Dn, #'<16 31 u2219 clz32 - lsl u2216 ->",
       {"sri_asisdshf_r"_h,
        "srshr_asisdshf_r"_h,
        "srsra_asisdshf_r"_h,
        "sshr_asisdshf_r"_h,
        "ssra_asisdshf_r"_h,
        "urshr_asisdshf_r"_h,
        "ursra_asisdshf_r"_h,
        "ushr_asisdshf_r"_h,
        "usra_asisdshf_r"_h}},
      {"'Dd, 'Dn, #'<u2216 8 31 u2219 clz32 - lsl ->",
       {"shl_asisdshf_r"_h, "sli_asisdshf_r"_h}},
      {"'Dd, 'Hn", {"fcvt_dh_floatdp1"_h}},
      {"'Dd, #'f2013", {"fmov_d_floatimm"_h}},
      {"'Dd, #0x'<0xff 56 lsl u18 * 0xff 48 lsl u17 * + 0xff 40 lsl u16 * + "
       "0xff 32 lsl u09 * + 0xff 24 lsl u08 * + 0xff0000 u07 * + 0xff00 u06 * "
       "+ 0xff u05 * + hex>",
       {"movi_asimdimm_d_ds"_h}},
      {"'Dd, 'Pgl, 'Zn.'[sz]", {"saddv_r_p_z"_h, "uaddv_r_p_z"_h}},
      {"'Dd, 'Sn", {"fcvt_ds_floatdp1"_h}},
      {"'Dd, 'Vn.2d", {"addp_asisdpair_only"_h}},
      {"'Dt, 'Dt2, ['Xns'(2115?, #'<s2115 8 *>)]",
       {"ldnp_d_ldstnapair_offs"_h,
        "ldp_d_ldstpair_off"_h,
        "stnp_d_ldstnapair_offs"_h,
        "stp_d_ldstpair_off"_h}},
      {"'Dt, 'Dt2, ['Xns, #'<s2115 8 *>]!",
       {"ldp_d_ldstpair_pre"_h, "stp_d_ldstpair_pre"_h}},
      {"'Dt, 'Dt2, ['Xns], #'<s2115 8 *>",
       {"ldp_d_ldstpair_post"_h, "stp_d_ldstpair_post"_h}},
      {"'Dt, pc'(23?:+)'<s2305 4 *> 'LValue", {"ldr_d_loadlit"_h}},
      {"'Dt, ['Xns'(2012?, #'s2012)]",
       {"ldur_d_ldst_unscaled"_h, "stur_d_ldst_unscaled"_h}},
      {"'Dt, ['Xns'(2110?, #'<u2110 8 *>)]",
       {"ldr_d_ldst_pos"_h, "str_d_ldst_pos"_h}},
      {"'Dt, ['Xns, #'s2012]!", {"ldr_d_ldst_immpre"_h, "str_d_ldst_immpre"_h}},
      {"'Dt, ['Xns, 'R13m'(1512=6?]'$), '[extmem]'(12? #3)]",
       {"ldr_d_ldst_regoff"_h, "str_d_ldst_regoff"_h}},
      {"'Dt, ['Xns], #'s2012",
       {"ldr_d_ldst_immpost"_h, "str_d_ldst_immpost"_h}},
      {"'Fd, 'Fn", {"fabs_d_floatdp1"_h,     "fabs_h_floatdp1"_h,
                    "fabs_s_floatdp1"_h,     "fmov_d_floatdp1"_h,
                    "fmov_h_floatdp1"_h,     "fmov_s_floatdp1"_h,
                    "fneg_d_floatdp1"_h,     "fneg_h_floatdp1"_h,
                    "fneg_s_floatdp1"_h,     "frint32x_d_floatdp1"_h,
                    "frint32x_s_floatdp1"_h, "frint32z_d_floatdp1"_h,
                    "frint32z_s_floatdp1"_h, "frint64x_d_floatdp1"_h,
                    "frint64x_s_floatdp1"_h, "frint64z_d_floatdp1"_h,
                    "frint64z_s_floatdp1"_h, "frinta_d_floatdp1"_h,
                    "frinta_h_floatdp1"_h,   "frinta_s_floatdp1"_h,
                    "frinti_d_floatdp1"_h,   "frinti_h_floatdp1"_h,
                    "frinti_s_floatdp1"_h,   "frintm_d_floatdp1"_h,
                    "frintm_h_floatdp1"_h,   "frintm_s_floatdp1"_h,
                    "frintn_d_floatdp1"_h,   "frintn_h_floatdp1"_h,
                    "frintn_s_floatdp1"_h,   "frintp_d_floatdp1"_h,
                    "frintp_h_floatdp1"_h,   "frintp_s_floatdp1"_h,
                    "frintx_d_floatdp1"_h,   "frintx_h_floatdp1"_h,
                    "frintx_s_floatdp1"_h,   "frintz_d_floatdp1"_h,
                    "frintz_h_floatdp1"_h,   "frintz_s_floatdp1"_h,
                    "fsqrt_d_floatdp1"_h,    "fsqrt_h_floatdp1"_h,
                    "fsqrt_s_floatdp1"_h}},
      {"'Fd, 'Fn, 'Fm",
       {"fadd_d_floatdp2"_h,   "fadd_h_floatdp2"_h,   "fadd_s_floatdp2"_h,
        "fdiv_d_floatdp2"_h,   "fdiv_h_floatdp2"_h,   "fdiv_s_floatdp2"_h,
        "fmax_d_floatdp2"_h,   "fmax_h_floatdp2"_h,   "fmax_s_floatdp2"_h,
        "fmaxnm_d_floatdp2"_h, "fmaxnm_h_floatdp2"_h, "fmaxnm_s_floatdp2"_h,
        "fmin_d_floatdp2"_h,   "fmin_h_floatdp2"_h,   "fmin_s_floatdp2"_h,
        "fminnm_d_floatdp2"_h, "fminnm_h_floatdp2"_h, "fminnm_s_floatdp2"_h,
        "fmul_d_floatdp2"_h,   "fmul_h_floatdp2"_h,   "fmul_s_floatdp2"_h,
        "fnmul_d_floatdp2"_h,  "fnmul_h_floatdp2"_h,  "fnmul_s_floatdp2"_h,
        "fsub_d_floatdp2"_h,   "fsub_h_floatdp2"_h,   "fsub_s_floatdp2"_h}},
      {"'Fd, 'Fn, 'Fm, '[cond]",
       {"fcsel_d_floatsel"_h, "fcsel_h_floatsel"_h, "fcsel_s_floatsel"_h}},
      {"'Fd, 'Fn, 'Fm, 'Fa",
       {"fmadd_d_floatdp3"_h,
        "fmadd_h_floatdp3"_h,
        "fmadd_s_floatdp3"_h,
        "fmsub_d_floatdp3"_h,
        "fmsub_h_floatdp3"_h,
        "fmsub_s_floatdp3"_h,
        "fnmadd_d_floatdp3"_h,
        "fnmadd_h_floatdp3"_h,
        "fnmadd_s_floatdp3"_h,
        "fnmsub_d_floatdp3"_h,
        "fnmsub_h_floatdp3"_h,
        "fnmsub_s_floatdp3"_h}},
      {"'Fd, 'Rn",
       {"fmov_d64_float2int"_h,
        "fmov_h32_float2int"_h,
        "fmov_h64_float2int"_h,
        "fmov_s32_float2int"_h,
        "scvtf_d32_float2int"_h,
        "scvtf_d64_float2int"_h,
        "scvtf_h32_float2int"_h,
        "scvtf_h64_float2int"_h,
        "scvtf_s32_float2int"_h,
        "scvtf_s64_float2int"_h,
        "ucvtf_d32_float2int"_h,
        "ucvtf_d64_float2int"_h,
        "ucvtf_h32_float2int"_h,
        "ucvtf_h64_float2int"_h,
        "ucvtf_s32_float2int"_h,
        "ucvtf_s64_float2int"_h}},
      {"'Fd, 'Rn, #'<64 u1510 ->",
       {"scvtf_d32_float2fix"_h,
        "scvtf_d64_float2fix"_h,
        "scvtf_h32_float2fix"_h,
        "scvtf_h64_float2fix"_h,
        "scvtf_s32_float2fix"_h,
        "scvtf_s64_float2fix"_h,
        "ucvtf_d32_float2fix"_h,
        "ucvtf_d64_float2fix"_h,
        "ucvtf_h32_float2fix"_h,
        "ucvtf_h64_float2fix"_h,
        "ucvtf_s32_float2fix"_h,
        "ucvtf_s64_float2fix"_h}},
      {"'Fn, #0.0",
       {"fcmp_dz_floatcmp"_h,
        "fcmp_hz_floatcmp"_h,
        "fcmp_sz_floatcmp"_h,
        "fcmpe_dz_floatcmp"_h,
        "fcmpe_hz_floatcmp"_h,
        "fcmpe_sz_floatcmp"_h}},
      {"'Fn, 'Fm",
       {"fcmp_d_floatcmp"_h,
        "fcmp_h_floatcmp"_h,
        "fcmp_s_floatcmp"_h,
        "fcmpe_d_floatcmp"_h,
        "fcmpe_h_floatcmp"_h,
        "fcmpe_s_floatcmp"_h}},
      {"'Fn, 'Fm, #'[nzcv], '[cond]",
       {"fccmp_d_floatccmp"_h,
        "fccmp_h_floatccmp"_h,
        "fccmp_s_floatccmp"_h,
        "fccmpe_d_floatccmp"_h,
        "fccmpe_h_floatccmp"_h,
        "fccmpe_s_floatccmp"_h}},
      {"'Hd, 'Dn", {"fcvt_hd_floatdp1"_h}},
      {"'Hd, 'Hn",
       {"fcvtas_asisdmiscfp16_r"_h,
        "fcvtau_asisdmiscfp16_r"_h,
        "fcvtms_asisdmiscfp16_r"_h,
        "fcvtmu_asisdmiscfp16_r"_h,
        "fcvtns_asisdmiscfp16_r"_h,
        "fcvtnu_asisdmiscfp16_r"_h,
        "fcvtps_asisdmiscfp16_r"_h,
        "fcvtpu_asisdmiscfp16_r"_h,
        "fcvtzs_asisdmiscfp16_r"_h,
        "fcvtzu_asisdmiscfp16_r"_h,
        "frecpe_asisdmiscfp16_r"_h,
        "frecpx_asisdmiscfp16_r"_h,
        "frsqrte_asisdmiscfp16_r"_h,
        "scvtf_asisdmiscfp16_r"_h,
        "ucvtf_asisdmiscfp16_r"_h}},
      {"'Hd, 'Hn, #0.0",
       {"fcmeq_asisdmiscfp16_fz"_h,
        "fcmge_asisdmiscfp16_fz"_h,
        "fcmgt_asisdmiscfp16_fz"_h,
        "fcmle_asisdmiscfp16_fz"_h,
        "fcmlt_asisdmiscfp16_fz"_h}},
      {"'Hd, 'Hn, 'Hm",
       {"fabd_asisdsamefp16_only"_h,
        "facge_asisdsamefp16_only"_h,
        "facgt_asisdsamefp16_only"_h,
        "fcmeq_asisdsamefp16_only"_h,
        "fcmge_asisdsamefp16_only"_h,
        "fcmgt_asisdsamefp16_only"_h,
        "fmulx_asisdsamefp16_only"_h,
        "frecps_asisdsamefp16_only"_h,
        "frsqrts_asisdsamefp16_only"_h}},
      {"'Hd, 'Hn, 'Vf.h['<u11_2119 u2322 lsr>]",
       {"fmla_asisdelem_rh_h"_h,
        "fmls_asisdelem_rh_h"_h,
        "fmul_asisdelem_rh_h"_h,
        "fmulx_asisdelem_rh_h"_h}},
      {"'Hd, #'f2013", {"fmov_h_floatimm"_h}},
      {"'Hd, 'Sn", {"bfcvt_bs_floatdp1"_h, "fcvt_hs_floatdp1"_h}},
      {"'Hd, 'Vn.'?30:84h",
       {"fmaxnmv_asimdall_only_h"_h,
        "fmaxv_asimdall_only_h"_h,
        "fminnmv_asimdall_only_h"_h,
        "fminv_asimdall_only_h"_h}},
      {"'Hd, 'Vn.2h",
       {"faddp_asisdpair_only_h"_h,
        "fmaxnmp_asisdpair_only_h"_h,
        "fmaxp_asisdpair_only_h"_h,
        "fminnmp_asisdpair_only_h"_h,
        "fminp_asisdpair_only_h"_h}},
      {"'Ht, ['Xns'(2012?, #'s2012)]",
       {"ldur_h_ldst_unscaled"_h, "stur_h_ldst_unscaled"_h}},
      {"'Ht, ['Xns'(2110?, #'<u2110 2 *>)]",
       {"ldr_h_ldst_pos"_h, "str_h_ldst_pos"_h}},
      {"'Ht, ['Xns, #'s2012]!", {"ldr_h_ldst_immpre"_h, "str_h_ldst_immpre"_h}},
      {"'Ht, ['Xns, 'R13m'(1512=6?]'$), '[extmem]'(12? #1)]",
       {"ldr_h_ldst_regoff"_h, "str_h_ldst_regoff"_h}},
      {"'Ht, ['Xns], #'s2012",
       {"ldr_h_ldst_immpost"_h, "str_h_ldst_immpost"_h}},
      {"'IY, 'Xt", {"msr_sr_systemmove"_h}},
      {"'[barrier]", {"dsb_bo_barriers"_h}},
      {"'Pd, ['Xns'(2110?, #'s2116_1210, mul vl)]",
       {"ldr_p_bi"_h, "str_p_bi"_h}},
      {"'Pd.'[sz]'(0905=31?:, '[mulpat])", {"ptrue_p_s"_h, "ptrues_p_s"_h}},
      {"'Pd.'[sz], 'Pgl/z, 'Zn.'[sz], #'s2016",
       {"cmpeq_p_p_zi"_h,
        "cmpge_p_p_zi"_h,
        "cmpgt_p_p_zi"_h,
        "cmple_p_p_zi"_h,
        "cmplt_p_p_zi"_h,
        "cmpne_p_p_zi"_h}},
      {"'Pd.'[sz], 'Pgl/z, 'Zn.'[sz], #'u2014",
       {"cmphi_p_p_zi"_h,
        "cmphs_p_p_zi"_h,
        "cmplo_p_p_zi"_h,
        "cmpls_p_p_zi"_h}},
      {"'Pd.'[sz], 'Pgl/z, 'Zn.'[sz], #0.0",
       {"fcmeq_p_p_z0"_h,
        "fcmge_p_p_z0"_h,
        "fcmgt_p_p_z0"_h,
        "fcmle_p_p_z0"_h,
        "fcmlt_p_p_z0"_h,
        "fcmne_p_p_z0"_h}},
      {"'Pd.'[sz], 'Pgl/z, 'Zn.'[sz], 'Zm.'[sz]",
       {"cmpeq_p_p_zz"_h,
        "cmpge_p_p_zz"_h,
        "cmpgt_p_p_zz"_h,
        "cmphi_p_p_zz"_h,
        "cmphs_p_p_zz"_h,
        "cmpne_p_p_zz"_h,
        "facge_p_p_zz"_h,
        "facgt_p_p_zz"_h,
        "fcmeq_p_p_zz"_h,
        "fcmge_p_p_zz"_h,
        "fcmgt_p_p_zz"_h,
        "fcmne_p_p_zz"_h,
        "fcmuo_p_p_zz"_h,
        "match_p_p_zz"_h,
        "nmatch_p_p_zz"_h}},
      {"'Pd.'[sz], 'Pgl/z, 'Zn.'[sz], 'Zm.d",
       {"cmpeq_p_p_zw"_h,
        "cmpge_p_p_zw"_h,
        "cmpgt_p_p_zw"_h,
        "cmphi_p_p_zw"_h,
        "cmphs_p_p_zw"_h,
        "cmple_p_p_zw"_h,
        "cmplo_p_p_zw"_h,
        "cmpls_p_p_zw"_h,
        "cmplt_p_p_zw"_h,
        "cmpne_p_p_zw"_h}},
      {"'Pd.'[sz], 'Pn, 'Pd.'[sz]", {"pnext_p_p_p"_h}},
      {"'Pd.'[sz], 'Pn.'[sz]", {"rev_p_p"_h}},
      {"'Pd.'[sz], 'Pn.'[sz], 'Pm.'[sz]",
       {"trn1_p_pp"_h,
        "trn2_p_pp"_h,
        "uzp1_p_pp"_h,
        "uzp2_p_pp"_h,
        "zip1_p_pp"_h,
        "zip2_p_pp"_h}},
      {"'Pd.'[sz], 'R12n, 'R12m",
       {"whilege_p_p_rr"_h,
        "whilegt_p_p_rr"_h,
        "whilehi_p_p_rr"_h,
        "whilehs_p_p_rr"_h,
        "whilele_p_p_rr"_h,
        "whilelo_p_p_rr"_h,
        "whilels_p_p_rr"_h,
        "whilelt_p_p_rr"_h,
        "whilerw_p_rr"_h,
        "whilewr_p_rr"_h}},
      {"'Pd.b", {"pfalse_p"_h, "rdffr_p_f"_h}},
      {"'Pd.b, 'Pn.b", {"movs_orrs_p_p_pp_z"_h, "mov_orr_p_p_pp_z"_h}},
      {"'Pd.b, 'Pn, 'Pd.b", {"pfirst_p_p_p"_h}},
      {"'Pd.b, 'Pn/z", {"rdffrs_p_p_f"_h, "rdffr_p_p_f"_h}},
      {"'Pd.b, 'Pm/z, 'Pn.b", {"nots_eors_p_p_pp_z"_h, "not_eor_p_p_pp_z"_h}},
      {"'Pd.b, p'u1310, 'Pn.b, 'Pm.b", {"sel_p_p_pp"_h}},
      {"'Pd.b, p'u1310/'?04:mz, 'Pn.b",
       {"brka_p_p_p"_h,
        "brkas_p_p_p_z"_h,
        "brkb_p_p_p"_h,
        "brkbs_p_p_p_z"_h,
        "movs_ands_p_p_pp_z"_h,
        "mov_and_p_p_pp_z"_h,
        "mov_sel_p_p_pp"_h}},
      {"'Pd.b, p'u1310/z, 'Pn.b, 'Pd.b", {"brkn_p_p_pp"_h, "brkns_p_p_pp"_h}},
      {"'Pd.b, p'u1310/z, 'Pn.b, 'Pm.b",
       {"brkpas_p_p_pp"_h,
        "brkpa_p_p_pp"_h,
        "brkpbs_p_p_pp"_h,
        "brkpb_p_p_pp"_h,
        "ands_p_p_pp_z"_h,
        "and_p_p_pp_z"_h,
        "bics_p_p_pp_z"_h,
        "bic_p_p_pp_z"_h,
        "eors_p_p_pp_z"_h,
        "eor_p_p_pp_z"_h,
        "nands_p_p_pp_z"_h,
        "nand_p_p_pp_z"_h,
        "nors_p_p_pp_z"_h,
        "nor_p_p_pp_z"_h,
        "orns_p_p_pp_z"_h,
        "orn_p_p_pp_z"_h,
        "orrs_p_p_pp_z"_h,
        "orr_p_p_pp_z"_h}},
      {"'Pd.h, 'Pn.b", {"punpkhi_p_p"_h, "punpklo_p_p"_h}},
      {"'Pn.b", {"wrffr_f_p"_h}},
      {"'Qd, 'Qn, 'Vm.2d",
       {"sha512h2_qqv_cryptosha512_3"_h, "sha512h_qqv_cryptosha512_3"_h}},
      {"'Qd, 'Qn, 'Vm.4s",
       {"sha256h2_qqv_cryptosha3"_h, "sha256h_qqv_cryptosha3"_h}},
      {"'Qd, 'Sn, 'Vm.4s",
       {"sha1c_qsv_cryptosha3"_h,
        "sha1m_qsv_cryptosha3"_h,
        "sha1p_qsv_cryptosha3"_h}},
      {"'Qt, pc'(23?:+)'<s2305 4 *> 'LValue", {"ldr_q_loadlit"_h}},
      {"'Qt, 'Qt2, ['Xns'(2115?, #'<s2115 16 *>)]",
       {"ldnp_q_ldstnapair_offs"_h,
        "ldp_q_ldstpair_off"_h,
        "stnp_q_ldstnapair_offs"_h,
        "stp_q_ldstpair_off"_h}},
      {"'Qt, 'Qt2, ['Xns, #'<s2115 16 *>]!",
       {"ldp_q_ldstpair_pre"_h, "stp_q_ldstpair_pre"_h}},
      {"'Qt, 'Qt2, ['Xns], #'<s2115 16 *>",
       {"ldp_q_ldstpair_post"_h, "stp_q_ldstpair_post"_h}},
      {"'Qt, ['Xns'(2012?, #'s2012)]",
       {"ldur_q_ldst_unscaled"_h, "stur_q_ldst_unscaled"_h}},
      {"'Qt, ['Xns'(2110?, #'<u2110 16 *>)]",
       {"ldr_q_ldst_pos"_h, "str_q_ldst_pos"_h}},
      {"'Qt, ['Xns, #'s2012]!", {"ldr_q_ldst_immpre"_h, "str_q_ldst_immpre"_h}},
      {"'Qt, ['Xns, 'R13m'(1512=6?]'$), '[extmem]'(12? #4)]",
       {"ldr_q_ldst_regoff"_h, "str_q_ldst_regoff"_h}},
      {"'Qt, ['Xns], #'s2012",
       {"ldr_q_ldst_immpost"_h, "str_q_ldst_immpost"_h}},
      {"'R20d'(1916?, '[mulpat], mul #'<u1916 1 +>'$)'(0905=31?:, '[mulpat])",
       {"sqdecb_r_rs_x"_h,  "sqdecd_r_rs_x"_h,  "sqdech_r_rs_x"_h,
        "sqdecw_r_rs_x"_h,  "sqincb_r_rs_x"_h,  "sqincd_r_rs_x"_h,
        "sqinch_r_rs_x"_h,  "sqincw_r_rs_x"_h,  "uqdecb_r_rs_uw"_h,
        "uqdecb_r_rs_x"_h,  "uqdecd_r_rs_uw"_h, "uqdecd_r_rs_x"_h,
        "uqdech_r_rs_uw"_h, "uqdech_r_rs_x"_h,  "uqdecw_r_rs_uw"_h,
        "uqdecw_r_rs_x"_h,  "uqincb_r_rs_uw"_h, "uqincb_r_rs_x"_h,
        "uqincd_r_rs_uw"_h, "uqincd_r_rs_x"_h,  "uqinch_r_rs_uw"_h,
        "uqinch_r_rs_x"_h,  "uqincw_r_rs_uw"_h, "uqincw_r_rs_x"_h}},
      {"'R22n, 'R22m", {"ctermeq_rr"_h, "ctermne_rr"_h}},
      {"'Rd, #0x'x2005'(2221?, lsl #'<u2221 16 *>)",
       {"movk_32_movewide"_h, "movk_64_movewide"_h}},
      {"'Rd, '[condinv]",
       {"cset_csinc_32_condsel"_h,
        "cset_csinc_64_condsel"_h,
        "csetm_csinv_32_condsel"_h,
        "csetm_csinv_64_condsel"_h}},
      {"'Rd, 'Fn", {"fcvtas_32d_float2int"_h,  "fcvtas_32h_float2int"_h,
                    "fcvtas_32s_float2int"_h,  "fcvtas_64d_float2int"_h,
                    "fcvtas_64h_float2int"_h,  "fcvtas_64s_float2int"_h,
                    "fcvtau_32d_float2int"_h,  "fcvtau_32h_float2int"_h,
                    "fcvtau_32s_float2int"_h,  "fcvtau_64d_float2int"_h,
                    "fcvtau_64h_float2int"_h,  "fcvtau_64s_float2int"_h,
                    "fcvtms_32d_float2int"_h,  "fcvtms_32h_float2int"_h,
                    "fcvtms_32s_float2int"_h,  "fcvtms_64d_float2int"_h,
                    "fcvtms_64h_float2int"_h,  "fcvtms_64s_float2int"_h,
                    "fcvtmu_32d_float2int"_h,  "fcvtmu_32h_float2int"_h,
                    "fcvtmu_32s_float2int"_h,  "fcvtmu_64d_float2int"_h,
                    "fcvtmu_64h_float2int"_h,  "fcvtmu_64s_float2int"_h,
                    "fcvtns_32d_float2int"_h,  "fcvtns_32h_float2int"_h,
                    "fcvtns_32s_float2int"_h,  "fcvtns_64d_float2int"_h,
                    "fcvtns_64h_float2int"_h,  "fcvtns_64s_float2int"_h,
                    "fcvtnu_32d_float2int"_h,  "fcvtnu_32h_float2int"_h,
                    "fcvtnu_32s_float2int"_h,  "fcvtnu_64d_float2int"_h,
                    "fcvtnu_64h_float2int"_h,  "fcvtnu_64s_float2int"_h,
                    "fcvtps_32d_float2int"_h,  "fcvtps_32h_float2int"_h,
                    "fcvtps_32s_float2int"_h,  "fcvtps_64d_float2int"_h,
                    "fcvtps_64h_float2int"_h,  "fcvtps_64s_float2int"_h,
                    "fcvtpu_32d_float2int"_h,  "fcvtpu_32h_float2int"_h,
                    "fcvtpu_32s_float2int"_h,  "fcvtpu_64d_float2int"_h,
                    "fcvtpu_64h_float2int"_h,  "fcvtpu_64s_float2int"_h,
                    "fcvtzs_32d_float2int"_h,  "fcvtzs_32h_float2int"_h,
                    "fcvtzs_32s_float2int"_h,  "fcvtzs_64d_float2int"_h,
                    "fcvtzs_64h_float2int"_h,  "fcvtzs_64s_float2int"_h,
                    "fcvtzu_32d_float2int"_h,  "fcvtzu_32h_float2int"_h,
                    "fcvtzu_32s_float2int"_h,  "fcvtzu_64d_float2int"_h,
                    "fcvtzu_64h_float2int"_h,  "fcvtzu_64s_float2int"_h,
                    "fjcvtzs_32d_float2int"_h, "fmov_32h_float2int"_h,
                    "fmov_32s_float2int"_h,    "fmov_64d_float2int"_h,
                    "fmov_64h_float2int"_h}},
      {"'Rd, 'Fn, #'<64 u1510 ->",
       {"fcvtzs_32d_float2fix"_h,
        "fcvtzs_32h_float2fix"_h,
        "fcvtzs_32s_float2fix"_h,
        "fcvtzs_64d_float2fix"_h,
        "fcvtzs_64h_float2fix"_h,
        "fcvtzs_64s_float2fix"_h,
        "fcvtzu_32d_float2fix"_h,
        "fcvtzu_32h_float2fix"_h,
        "fcvtzu_32s_float2fix"_h,
        "fcvtzu_64d_float2fix"_h,
        "fcvtzu_64h_float2fix"_h,
        "fcvtzu_64s_float2fix"_h}},
      {"'Rd, #'<32 u2116 ->, #'<s1510 1 +>", {"bfc_bfm_32m_bitfield"_h}},
      {"'Rd, #'<64 u2116 ->, #'<s1510 1 +>", {"bfc_bfm_64m_bitfield"_h}},
      {"'Rd, #0x'<u2005 u2221 16 * lsl hex>",
       {"movz_32_movewide"_h,
        "movz_64_movewide"_h,
        "movn_32_movewide"_h,
        "movn_64_movewide"_h,
        "mov_movz_32_movewide"_h,
        "mov_movz_64_movewide"_h}},
      {"'Rd, #0x'<u2005 u2221 16 * lsl not 32 lsl 32 lsr hex>",
       {"mov_movn_32_movewide"_h}},
      {"'Rd, #0x'<u2005 u2221 16 * lsl not hex>", {"mov_movn_64_movewide"_h}},
      {"'Rd, 'Rm",
       {"ngc_sbc_32_addsub_carry"_h,
        "ngc_sbc_64_addsub_carry"_h,
        "ngcs_sbcs_32_addsub_carry"_h,
        "ngcs_sbcs_64_addsub_carry"_h,
        "mov_orr_32_log_shift"_h,
        "mov_orr_64_log_shift"_h}},
      {"'Rd, 'Rm'(1510?, '[shift] #'u1510)",
       {"neg_sub_32_addsub_shift"_h,
        "neg_sub_64_addsub_shift"_h,
        "negs_subs_32_addsub_shift"_h,
        "negs_subs_64_addsub_shift"_h,
        "mvn_orn_32_log_shift"_h,
        "mvn_orn_64_log_shift"_h}},
      {"'Rd, 'Rn",
       {"abs_32_dp_1src"_h,
        "abs_64_dp_1src"_h,
        "cls_32_dp_1src"_h,
        "cls_64_dp_1src"_h,
        "clz_32_dp_1src"_h,
        "clz_64_dp_1src"_h,
        "cnt_32_dp_1src"_h,
        "cnt_64_dp_1src"_h,
        "ctz_32_dp_1src"_h,
        "ctz_64_dp_1src"_h,
        "rbit_32_dp_1src"_h,
        "rbit_64_dp_1src"_h,
        "rev16_32_dp_1src"_h,
        "rev16_64_dp_1src"_h,
        "rev32_64_dp_1src"_h,
        "rev_32_dp_1src"_h,
        "rev_64_dp_1src"_h}},
      {"'Rd, 'Rn, #'s1710",
       {"smax_32_minmax_imm"_h,
        "smax_64_minmax_imm"_h,
        "smin_32_minmax_imm"_h,
        "smin_64_minmax_imm"_h}},
      {"'Rd, 'Rn, #'u1510", {"ror_extr_32_extract"_h, "ror_extr_64_extract"_h}},
      {"'Rd, 'Rn, #'u1710",
       {"umax_32u_minmax_imm"_h,
        "umax_64u_minmax_imm"_h,
        "umin_32u_minmax_imm"_h,
        "umin_64u_minmax_imm"_h}},
      {"'Rd, 'Rn, '[condinv]",
       {"cinc_csinc_32_condsel"_h,
        "cinc_csinc_64_condsel"_h,
        "cinv_csinv_32_condsel"_h,
        "cinv_csinv_64_condsel"_h,
        "cneg_csneg_32_condsel"_h,
        "cneg_csneg_64_condsel"_h}},
      {"'Rd, 'Rn, #'u2116",
       {"asr_sbfm_32m_bitfield"_h,
        "asr_sbfm_64m_bitfield"_h,
        "lsr_ubfm_32m_bitfield"_h,
        "lsr_ubfm_64m_bitfield"_h}},
      {"'Rd, 'Rn, #'u2116, #'<1 u1510 u2116 - +>",
       {"sbfx_sbfm_32m_bitfield"_h,
        "sbfx_sbfm_64m_bitfield"_h,
        "ubfx_ubfm_32m_bitfield"_h,
        "ubfx_ubfm_64m_bitfield"_h,
        "bfxil_bfm_32m_bitfield"_h,
        "bfxil_bfm_64m_bitfield"_h}},
      {"'Rd, 'Rn, #'<32 u2116 ->", {"lsl_ubfm_32m_bitfield"_h}},
      {"'Rd, 'Rn, #'<64 u2116 ->", {"lsl_ubfm_64m_bitfield"_h}},
      {"'Rd, 'Rn, #'<32 u2116 ->, #'<s1510 1 +>",
       {"sbfiz_sbfm_32m_bitfield"_h,
        "ubfiz_ubfm_32m_bitfield"_h,
        "bfi_bfm_32m_bitfield"_h}},
      {"'Rd, 'Rn, #'<64 u2116 ->, #'<s1510 1 +>",
       {"sbfiz_sbfm_64m_bitfield"_h,
        "ubfiz_ubfm_64m_bitfield"_h,
        "bfi_bfm_64m_bitfield"_h}},
      {"'Rd, 'Rn, 'Rm", {"crc32b_32c_dp_2src"_h,    "crc32cb_32c_dp_2src"_h,
                         "crc32ch_32c_dp_2src"_h,   "crc32cw_32c_dp_2src"_h,
                         "crc32h_32c_dp_2src"_h,    "crc32w_32c_dp_2src"_h,
                         "sdiv_32_dp_2src"_h,       "sdiv_64_dp_2src"_h,
                         "smax_32_dp_2src"_h,       "smax_64_dp_2src"_h,
                         "smin_32_dp_2src"_h,       "smin_64_dp_2src"_h,
                         "udiv_32_dp_2src"_h,       "udiv_64_dp_2src"_h,
                         "umax_32_dp_2src"_h,       "umax_64_dp_2src"_h,
                         "umin_32_dp_2src"_h,       "umin_64_dp_2src"_h,
                         "adcs_32_addsub_carry"_h,  "adcs_64_addsub_carry"_h,
                         "adc_32_addsub_carry"_h,   "adc_64_addsub_carry"_h,
                         "sbcs_32_addsub_carry"_h,  "sbcs_64_addsub_carry"_h,
                         "sbc_32_addsub_carry"_h,   "sbc_64_addsub_carry"_h,
                         "mul_madd_32a_dp_3src"_h,  "mul_madd_64a_dp_3src"_h,
                         "mneg_msub_32a_dp_3src"_h, "mneg_msub_64a_dp_3src"_h,
                         "asr_asrv_32_dp_2src"_h,   "asr_asrv_64_dp_2src"_h,
                         "lsl_lslv_32_dp_2src"_h,   "lsl_lslv_64_dp_2src"_h,
                         "lsr_lsrv_32_dp_2src"_h,   "lsr_lsrv_64_dp_2src"_h,
                         "ror_rorv_32_dp_2src"_h,   "ror_rorv_64_dp_2src"_h}},
      {"'Rd, 'Rn, 'Rm, #'u1510", {"extr_32_extract"_h, "extr_64_extract"_h}},
      {"'Rd, 'Rn, 'Rm, '[cond]",
       {"csel_32_condsel"_h,
        "csel_64_condsel"_h,
        "csinc_32_condsel"_h,
        "csinc_64_condsel"_h,
        "csinv_32_condsel"_h,
        "csinv_64_condsel"_h,
        "csneg_32_condsel"_h,
        "csneg_64_condsel"_h}},
      {"'Rd, 'Rn, 'Rm, 'Ra",
       {"madd_32a_dp_3src"_h,
        "madd_64a_dp_3src"_h,
        "msub_32a_dp_3src"_h,
        "msub_64a_dp_3src"_h}},
      {"'Rd, 'Rn, 'Rm'(1510?, '[shift] #'u1510)",
       {"adds_32_addsub_shift"_h, "adds_64_addsub_shift"_h,
        "add_32_addsub_shift"_h,  "add_64_addsub_shift"_h,
        "subs_32_addsub_shift"_h, "subs_64_addsub_shift"_h,
        "sub_32_addsub_shift"_h,  "sub_64_addsub_shift"_h,
        "ands_32_log_shift"_h,    "ands_64_log_shift"_h,
        "and_32_log_shift"_h,     "and_64_log_shift"_h,
        "bics_32_log_shift"_h,    "bics_64_log_shift"_h,
        "bic_32_log_shift"_h,     "bic_64_log_shift"_h,
        "eon_32_log_shift"_h,     "eon_64_log_shift"_h,
        "eor_32_log_shift"_h,     "eor_64_log_shift"_h,
        "orn_32_log_shift"_h,     "orn_64_log_shift"_h,
        "orr_32_log_shift"_h,     "orr_64_log_shift"_h}},
      {"'Rd, 'Vn.D[1]", {"fmov_64vx_float2int"_h}},
      {"'Rd, 'Wn",
       {"sxtb_sbfm_32m_bitfield"_h,
        "sxtb_sbfm_64m_bitfield"_h,
        "sxth_sbfm_32m_bitfield"_h,
        "sxth_sbfm_64m_bitfield"_h,
        "sxtw_sbfm_64m_bitfield"_h,
        "uxtb_ubfm_32m_bitfield"_h,
        "uxtb_ubfm_64m_bitfield"_h,
        "uxth_ubfm_32m_bitfield"_h,
        "uxth_ubfm_64m_bitfield"_h}},
      {"'Rd, 'Xns, 'Rm", {"gmi_64g_dp_2src"_h}},
      {"'Rds, 'ITri", {"mov_orr_32_log_imm"_h, "mov_orr_64_log_imm"_h}},
      {"'Rds, 'Rn, 'ITri",
       {"ands_32s_log_imm"_h,
        "ands_64s_log_imm"_h,
        "and_32_log_imm"_h,
        "and_64_log_imm"_h,
        "eor_32_log_imm"_h,
        "eor_64_log_imm"_h,
        "orr_32_log_imm"_h,
        "orr_64_log_imm"_h}},
      {"'Rds, 'Rns", {"mov_add_32_addsub_imm"_h, "mov_add_64_addsub_imm"_h}},
      {"'Rds, 'Rns, '(1413=3?x:w)'(2016=31?zr:'u2016), '[ext]'(1210? #'u1210)",
       {"adds_32s_addsub_ext"_h,
        "add_32_addsub_ext"_h,
        "subs_32s_addsub_ext"_h,
        "sub_32_addsub_ext"_h,
        "adds_32s_addsub_ext"_h,
        "adds_64s_addsub_ext"_h,
        "add_64_addsub_ext"_h,
        "subs_64s_addsub_ext"_h,
        "sub_64_addsub_ext"_h}},
      {"'Rds, 'Rns, "
       "'(1413=3?x:w)'(2016=31?zr:'u2016)'(1510=16?:, '[ext32])'(1210? "
       "#'u1210)",
       {"adds_lsl_adds_32s_addsub_ext"_h,
        "add_lsl_add_32_addsub_ext"_h,
        "subs_lsl_subs_32s_addsub_ext"_h,
        "sub_lsl_sub_32_addsub_ext"_h}},
      {"'Rds, 'Rns, "
       "'(1413=3?x:w)'(2016=31?zr:'u2016)'(1510=24?:, '[ext64])'(1210? "
       "#'u1210)",
       {"adds_lsl_adds_64s_addsub_ext"_h,
        "add_lsl_add_64_addsub_ext"_h,
        "subs_lsl_subs_64s_addsub_ext"_h,
        "sub_lsl_sub_64_addsub_ext"_h}},
      {"'Rds, 'Rns, #0x'(22?'<u2110 12 lsl hex>:'x2110) ('(22?'<u2110 12 "
       "lsl>:'u2110))",
       {"adds_32s_addsub_imm"_h,
        "adds_64s_addsub_imm"_h,
        "add_32_addsub_imm"_h,
        "add_64_addsub_imm"_h,
        "subs_32s_addsub_imm"_h,
        "subs_64s_addsub_imm"_h,
        "sub_32_addsub_imm"_h,
        "sub_64_addsub_imm"_h}},
      {"'Rn, #'u2016, #'[nzcv], '[cond]",
       {"ccmn_32_condcmp_imm"_h,
        "ccmn_64_condcmp_imm"_h,
        "ccmp_32_condcmp_imm"_h,
        "ccmp_64_condcmp_imm"_h}},
      {"'Rn, 'ITri", {"tst_ands_32s_log_imm"_h, "tst_ands_64s_log_imm"_h}},
      {"'Rn, 'Rm, #'[nzcv], '[cond]",
       {"ccmn_32_condcmp_reg"_h,
        "ccmn_64_condcmp_reg"_h,
        "ccmp_32_condcmp_reg"_h,
        "ccmp_64_condcmp_reg"_h}},
      {"'Rn, 'Rm'(1510?, '[shift] #'u1510)",
       {"cmn_adds_32_addsub_shift"_h,
        "cmn_adds_64_addsub_shift"_h,
        "cmp_subs_32_addsub_shift"_h,
        "cmp_subs_64_addsub_shift"_h,
        "tst_ands_32_log_shift"_h,
        "tst_ands_64_log_shift"_h}},
      {"'Rns, '(1413=3?x:w)'(2016=31?zr:'u2016)'(1510=16?'$:, '[ext32])'(1210? "
       "#'u1210)",
       {"cmn_adds_32s_addsub_ext"_h, "cmp_subs_32s_addsub_ext"_h}},
      {"'Rns, '(1413=3?x:w)'(2016=31?zr:'u2016)'(1510=16?'$:, '[ext64])'(1210? "
       "#'u1210)",
       {"cmn_adds_64s_addsub_ext"_h, "cmp_subs_64s_addsub_ext"_h}},
      {"'Rns, #0x'(22?'<u2110 12 lsl hex>:'x2110) ('(22?'<u2110 12 "
       "lsl>:'u2110))",
       {"cmn_adds_32s_addsub_imm"_h,
        "cmn_adds_64s_addsub_imm"_h,
        "cmp_subs_32s_addsub_imm"_h,
        "cmp_subs_64s_addsub_imm"_h}},
      {"'Rt, #'u31_2319, 'TImmTest",
       {"tbnz_only_testbranch"_h, "tbz_only_testbranch"_h}},
      {"'Rt, 'TImmCmpa",
       {"cbnz_32_compbranch"_h,
        "cbnz_64_compbranch"_h,
        "cbz_32_compbranch"_h,
        "cbz_64_compbranch"_h}},
      {"'Sd, 'Dn", {"fcvt_sd_floatdp1"_h, "fcvtxn_asisdmisc_n"_h}},
      {"'Sd, 'Hn", {"fcvt_sh_floatdp1"_h}},
      {"'Sd, #'f2013", {"fmov_s_floatimm"_h}},
      {"'Sd, 'Sn", {"sha1h_ss_cryptosha2"_h}},
      {"'Sd, 'Vn.4s",
       {"fmaxnmv_asimdall_only_sd"_h,
        "fminnmv_asimdall_only_sd"_h,
        "fmaxv_asimdall_only_sd"_h,
        "fminv_asimdall_only_sd"_h}},
      {"'St, pc'(23?:+)'<s2305 4 *> 'LValue", {"ldr_s_loadlit"_h}},
      {"'St, 'St2, ['Xns'(2115?, #'<s2115 4 *>)]",
       {"ldnp_s_ldstnapair_offs"_h,
        "ldp_s_ldstpair_off"_h,
        "stnp_s_ldstnapair_offs"_h,
        "stp_s_ldstpair_off"_h}},
      {"'St, 'St2, ['Xns, #'<s2115 4 *>]!",
       {"ldp_s_ldstpair_pre"_h, "stp_s_ldstpair_pre"_h}},
      {"'St, 'St2, ['Xns], #'<s2115 4 *>",
       {"ldp_s_ldstpair_post"_h, "stp_s_ldstpair_post"_h}},
      {"'St, ['Xns'(2012?, #'s2012)]",
       {"ldur_s_ldst_unscaled"_h, "stur_s_ldst_unscaled"_h}},
      {"'St, ['Xns'(2110?, #'<u2110 4 *>)]",
       {"ldr_s_ldst_pos"_h, "str_s_ldst_pos"_h}},
      {"'St, ['Xns, #'s2012]!", {"ldr_s_ldst_immpre"_h, "str_s_ldst_immpre"_h}},
      {"'St, ['Xns, 'R13m'(1512=6?]'$), '[extmem]'(12? #2)]",
       {"ldr_s_ldst_regoff"_h, "str_s_ldst_regoff"_h}},
      {"'St, ['Xns], #'s2012",
       {"ldr_s_ldst_immpost"_h, "str_s_ldst_immpost"_h}},
      {"'TImmCond",
       {"b.'[condb]_b_only_condbranch"_h, "bc.'[condb]_bc_only_condbranch"_h}},
      {"'TImmUncn", {"b_only_branch_imm"_h, "bl_only_branch_imm"_h}},
      {"'Vd.'[nf], 'Vn.'[nf], 'Vm.'[nf]",
       {"fabd_asimdsame_only"_h,    "facge_asimdsame_only"_h,
        "facgt_asimdsame_only"_h,   "faddp_asimdsame_only"_h,
        "fadd_asimdsame_only"_h,    "fcmeq_asimdsame_only"_h,
        "fcmge_asimdsame_only"_h,   "fcmgt_asimdsame_only"_h,
        "fdiv_asimdsame_only"_h,    "fmaxnmp_asimdsame_only"_h,
        "fmaxnm_asimdsame_only"_h,  "fmaxp_asimdsame_only"_h,
        "fmax_asimdsame_only"_h,    "fminnmp_asimdsame_only"_h,
        "fminnm_asimdsame_only"_h,  "fminp_asimdsame_only"_h,
        "fmin_asimdsame_only"_h,    "fmla_asimdsame_only"_h,
        "fmls_asimdsame_only"_h,    "fmulx_asimdsame_only"_h,
        "fmul_asimdsame_only"_h,    "frecps_asimdsame_only"_h,
        "frsqrts_asimdsame_only"_h, "fsub_asimdsame_only"_h}},
      {"'Vd.'[nf], 'Vn.'[nf], 'Vf.'[sz]['<u11_2119 u2322 lsr>]",
       {"fmla_asimdelem_r_sd"_h,
        "fmls_asimdelem_r_sd"_h,
        "fmulx_asimdelem_r_sd"_h,
        "fmul_asimdelem_r_sd"_h}},
      {"'Vd.'[npair], 'Vn.'[n]",
       {"sadalp_asimdmisc_p"_h,
        "saddlp_asimdmisc_p"_h,
        "uadalp_asimdmisc_p"_h,
        "uaddlp_asimdmisc_p"_h}},
      {"'Vd.'[nshift], 'Vn.'[nshift], #'<u2216 8 31 u2219 clz32 - lsl ->",
       {"sqshlu_asimdshf_r"_h,
        "sqshl_asimdshf_r"_h,
        "uqshl_asimdshf_r"_h,
        "shl_asimdshf_r"_h,
        "sli_asimdshf_r"_h}},
      {"'Vd.'[nshift], 'Vn.'[nshift], #'<16 31 u2219 clz32 - lsl u2216 ->",
       {"sri_asimdshf_r"_h,
        "srshr_asimdshf_r"_h,
        "srsra_asimdshf_r"_h,
        "sshr_asimdshf_r"_h,
        "ssra_asimdshf_r"_h,
        "urshr_asimdshf_r"_h,
        "ursra_asimdshf_r"_h,
        "ushr_asimdshf_r"_h,
        "usra_asimdshf_r"_h,
        "scvtf_asimdshf_c"_h,
        "ucvtf_asimdshf_c"_h,
        "fcvtzs_asimdshf_c"_h,
        "fcvtzu_asimdshf_c"_h}},
      {"'Vd.'[nshift], 'Vn.'[nshiftln], #'<16 31 u2219 clz32 - lsl u2216 ->",
       {"shrn_asimdshf_n"_h,
        "rshrn_asimdshf_n"_h,
        "sqshrn_asimdshf_n"_h,
        "sqrshrn_asimdshf_n"_h,
        "sqshrun_asimdshf_n"_h,
        "sqrshrun_asimdshf_n"_h,
        "uqshrn_asimdshf_n"_h,
        "uqrshrn_asimdshf_n"_h,
        "shrn2_shrn_asimdshf_n"_h,
        "rshrn2_rshrn_asimdshf_n"_h,
        "sqshrn2_sqshrn_asimdshf_n"_h,
        "sqrshrn2_sqrshrn_asimdshf_n"_h,
        "sqshrun2_sqshrun_asimdshf_n"_h,
        "sqrshrun2_sqrshrun_asimdshf_n"_h,
        "uqshrn2_uqshrn_asimdshf_n"_h,
        "uqrshrn2_uqrshrn_asimdshf_n"_h}},
      {"'Vd.'[nshiftln], 'Vn.'[nshift]",
       {"sxtl_sshll_asimdshf_l"_h,
        "uxtl_ushll_asimdshf_l"_h,
        "sxtl2_sshll_asimdshf_l"_h,
        "uxtl2_ushll_asimdshf_l"_h}},
      {"'Vd.'[nshiftln], 'Vn.'[nshift], #'<u2216 8 31 u2219 clz32 - lsl ->",
       {"sshll_asimdshf_l"_h,
        "ushll_asimdshf_l"_h,
        "sshll2_sshll_asimdshf_l"_h,
        "ushll2_ushll_asimdshf_l"_h}},
      {"'Vd.'[ntri], '(1916=8?'Xn:'Wn)", {"dup_asimdins_dr_r"_h}},
      {"'Vd.'[ntri], 'Vn.'[ntriscal]['<u2016 dup ctz 1 + lsr>]",
       {"dup_asimdins_dv_v"_h}},
      {"'Vd.'[ntriscal]['<u2016 dup ctz 1 + lsr>], '(1916=8?'Xn:'Wn)",
       {"mov_ins_asimdins_ir_r"_h}},
      {"'Vd.'[ntriscal]['<u2016 dup ctz 1 + lsr>], 'Vn.'[ntriscal]['<u1411 "
       "u2016 ctz lsr>]",
       {"mov_ins_asimdins_iv_v"_h}},
      {"'Vd.'(22?1q:8h), 'Vn.'(22?'<u30 1 +>d:'<u3027 7 +>b), "
       "'Vm.'(22?'<u30 1 +>d:'<u3027 7 +>b)",
       {"pmull_asimddiff_l"_h, "pmull2_pmull_asimddiff_l"_h}},
      {"'Vd.'(22?2d:4s), 'Vn.'(22?2s:4h)", {"fcvtl_asimdmisc_l"_h}},
      {"'Vd.'(22?2d:4s), 'Vn.'(22?4s:8h)", {"fcvtl2_fcvtl_asimdmisc_l"_h}},
      {"'Vd.'(22?2s:4h), 'Vn.'(22?2d:4s)", {"fcvtn_asimdmisc_n"_h}},
      {"'Vd.'(22?4s:8h), 'Vn.'(22?2d:4s)", {"fcvtn2_fcvtn_asimdmisc_n"_h}},
      {"'Vd.'(22?4s:2d), 'Vn.'[n], 'Vf.'[sz]['<u11_2119 u2322 lsr>]",
       {"smlal_asimdelem_l"_h,
        "smlsl_asimdelem_l"_h,
        "smull_asimdelem_l"_h,
        "umlal_asimdelem_l"_h,
        "umlsl_asimdelem_l"_h,
        "umull_asimdelem_l"_h,
        "sqdmull_asimdelem_l"_h,
        "sqdmlal_asimdelem_l"_h,
        "sqdmlsl_asimdelem_l"_h,
        "smlal2_smlal_asimdelem_l"_h,
        "smlsl2_smlsl_asimdelem_l"_h,
        "smull2_smull_asimdelem_l"_h,
        "umlal2_umlal_asimdelem_l"_h,
        "umlsl2_umlsl_asimdelem_l"_h,
        "umull2_umull_asimdelem_l"_h,
        "sqdmull2_sqdmull_asimdelem_l"_h,
        "sqdmlal2_sqdmlal_asimdelem_l"_h,
        "sqdmlsl2_sqdmlsl_asimdelem_l"_h}},
      {"'Vd.'(2222=1?2d:'?30:42s), 'Vn.'(2222=1?2d:'?30:42s)",
       {"fabs_asimdmisc_r"_h,     "fcvtas_asimdmisc_r"_h,
        "fcvtau_asimdmisc_r"_h,   "fcvtms_asimdmisc_r"_h,
        "fcvtmu_asimdmisc_r"_h,   "fcvtns_asimdmisc_r"_h,
        "fcvtnu_asimdmisc_r"_h,   "fcvtps_asimdmisc_r"_h,
        "fcvtpu_asimdmisc_r"_h,   "fcvtzs_asimdmisc_r"_h,
        "fcvtzu_asimdmisc_r"_h,   "fneg_asimdmisc_r"_h,
        "frecpe_asimdmisc_r"_h,   "frint32x_asimdmisc_r"_h,
        "frint32z_asimdmisc_r"_h, "frint64x_asimdmisc_r"_h,
        "frint64z_asimdmisc_r"_h, "frinta_asimdmisc_r"_h,
        "frinti_asimdmisc_r"_h,   "frintm_asimdmisc_r"_h,
        "frintn_asimdmisc_r"_h,   "frintp_asimdmisc_r"_h,
        "frintx_asimdmisc_r"_h,   "frintz_asimdmisc_r"_h,
        "frsqrte_asimdmisc_r"_h,  "fsqrt_asimdmisc_r"_h,
        "scvtf_asimdmisc_r"_h,    "ucvtf_asimdmisc_r"_h}},
      {"'Vd.'(2222=1?2d:'?30:42s), 'Vn.'(2222=1?2d:'?30:42s), #0.0",
       {"fcmeq_asimdmisc_fz"_h,
        "fcmge_asimdmisc_fz"_h,
        "fcmgt_asimdmisc_fz"_h,
        "fcmle_asimdmisc_fz"_h,
        "fcmlt_asimdmisc_fz"_h}},
      {"'Vd.'(30?16:8)b, 'Vn.'(30?16:8)b",
       {"rbit_asimdmisc_r"_h,
        "cnt_asimdmisc_r"_h,
        "rev16_asimdmisc_r"_h,
        "mov_orr_asimdsame_only"_h,
        "mvn_not_asimdmisc_r"_h}},
      {"'Vd.'(30?16:8)b, 'Vn.'(30?16:8)b, 'Vm.'(30?16:8)b",
       {"and_asimdsame_only"_h,
        "bic_asimdsame_only"_h,
        "bif_asimdsame_only"_h,
        "bit_asimdsame_only"_h,
        "bsl_asimdsame_only"_h,
        "eor_asimdsame_only"_h,
        "orn_asimdsame_only"_h,
        "orr_asimdsame_only"_h,
        "pmul_asimdsame_only"_h}},
      {"'Vd.'(30?16:8)b, 'Vn.'(30?16:8)b, 'Vm.'(30?16:8)b, #'u1411",
       {"ext_asimdext_only"_h}},
      {"'Vd.'(30?16:8)b, {'Vn.16b, 'Vn2.16b, 'Vn3.16b, 'Vn4.16b}, "
       "'Vm.'(30?16:8)b",
       {"tbl_asimdtbl_l4_4"_h, "tbx_asimdtbl_l4_4"_h}},
      {"'Vd.'(30?16:8)b, {'Vn.16b, 'Vn2.16b, 'Vn3.16b}, 'Vm.'(30?16:8)b",
       {"tbl_asimdtbl_l3_3"_h, "tbx_asimdtbl_l3_3"_h}},
      {"'Vd.'(30?16:8)b, {'Vn.16b, 'Vn2.16b}, 'Vm.'(30?16:8)b",
       {"tbl_asimdtbl_l2_2"_h, "tbx_asimdtbl_l2_2"_h}},
      {"'Vd.'(30?16:8)b, {'Vn.16b}, 'Vm.'(30?16:8)b",
       {"tbl_asimdtbl_l1_1"_h, "tbx_asimdtbl_l1_1"_h}},
      {"'Vd.'?30:42s, 'Vn.'(30?16:8)b, 'Vm.4b['u11_21]",
       {"sdot_asimdelem_d"_h,
        "sudot_asimdelem_d"_h,
        "udot_asimdelem_d"_h,
        "usdot_asimdelem_d"_h}},
      {"'Vd.'?30:42s, 'Vn.'?30:42h, 'Ve.h['u11_2120]",
       {"fmlal2_asimdelem_lh"_h,
        "fmlal_asimdelem_lh"_h,
        "fmlsl2_asimdelem_lh"_h,
        "fmlsl_asimdelem_lh"_h}},
      {"'Vd.'?30:42s, 'Vn.'?30:42h, 'Vm.'?30:42h",
       {"fmlal2_asimdsame_f"_h,
        "fmlal_asimdsame_f"_h,
        "fmlsl2_asimdsame_f"_h,
        "fmlsl_asimdsame_f"_h}},
      {"'Vd.'?30:42s, 'Vn.'?30:84h, 'Vm.'?30:84h",
       {"bfdot_asimdsame2_d"_h, "bfmmla_asimdsame2_e"_h}},
      {"'Vd.'?30:42s, 'Vn.'?30:84h, 'Vm.2h['u11_21]", {"bfdot_asimdelem_e"_h}},
      {"'Vd.'?30:42s, 'Vn.'?30:42s",
       {"urecpe_asimdmisc_r"_h, "ursqrte_asimdmisc_r"_h}},
      {"'Vd.'?30:42s, 'Vn.'(30?16:8)b, 'Vm.'(30?16:8)b",
       {"sdot_asimdsame2_d"_h, "udot_asimdsame2_d"_h, "usdot_asimdsame2_d"_h}},
      {"'Vd.'?30:42s, 'Vn.2d",
       {"fcvtxn_asimdmisc_n"_h, "fcvtxn2_fcvtxn_asimdmisc_n"_h}},
      {"'Vd.'?30:84h, 'Vn.4s",
       {"bfcvtn_asimdmisc_4s"_h, "bfcvtn2_bfcvtn_asimdmisc_4s"_h}},
      {"'Vd.'?30:84h, 'Vn.'?30:84h",
       {"fabs_asimdmiscfp16_r"_h,    "fcvtas_asimdmiscfp16_r"_h,
        "fcvtau_asimdmiscfp16_r"_h,  "fcvtms_asimdmiscfp16_r"_h,
        "fcvtmu_asimdmiscfp16_r"_h,  "fcvtns_asimdmiscfp16_r"_h,
        "fcvtnu_asimdmiscfp16_r"_h,  "fcvtps_asimdmiscfp16_r"_h,
        "fcvtpu_asimdmiscfp16_r"_h,  "fcvtzs_asimdmiscfp16_r"_h,
        "fcvtzu_asimdmiscfp16_r"_h,  "fneg_asimdmiscfp16_r"_h,
        "frecpe_asimdmiscfp16_r"_h,  "frinta_asimdmiscfp16_r"_h,
        "frinti_asimdmiscfp16_r"_h,  "frintm_asimdmiscfp16_r"_h,
        "frintn_asimdmiscfp16_r"_h,  "frintp_asimdmiscfp16_r"_h,
        "frintx_asimdmiscfp16_r"_h,  "frintz_asimdmiscfp16_r"_h,
        "frsqrte_asimdmiscfp16_r"_h, "fsqrt_asimdmiscfp16_r"_h,
        "scvtf_asimdmiscfp16_r"_h,   "ucvtf_asimdmiscfp16_r"_h}},
      {"'Vd.'?30:84h, 'Vn.'?30:84h, #0.0",
       {"fcmeq_asimdmiscfp16_fz"_h,
        "fcmge_asimdmiscfp16_fz"_h,
        "fcmgt_asimdmiscfp16_fz"_h,
        "fcmle_asimdmiscfp16_fz"_h,
        "fcmlt_asimdmiscfp16_fz"_h}},
      {"'Vd.'?30:84h, 'Vn.'?30:84h, 'Ve.h['<u11_2120>]",
       {"fmla_asimdelem_rh_h"_h,
        "fmls_asimdelem_rh_h"_h,
        "fmulx_asimdelem_rh_h"_h,
        "fmul_asimdelem_rh_h"_h}},
      {"'Vd.'?30:84h, 'Vn.'?30:84h, 'Vm.'?30:84h",
       {"fabd_asimdsamefp16_only"_h,    "facge_asimdsamefp16_only"_h,
        "facgt_asimdsamefp16_only"_h,   "faddp_asimdsamefp16_only"_h,
        "fadd_asimdsamefp16_only"_h,    "fcmeq_asimdsamefp16_only"_h,
        "fcmge_asimdsamefp16_only"_h,   "fcmgt_asimdsamefp16_only"_h,
        "fdiv_asimdsamefp16_only"_h,    "fmaxnmp_asimdsamefp16_only"_h,
        "fmaxnm_asimdsamefp16_only"_h,  "fmaxp_asimdsamefp16_only"_h,
        "fmax_asimdsamefp16_only"_h,    "fminnmp_asimdsamefp16_only"_h,
        "fminnm_asimdsamefp16_only"_h,  "fminp_asimdsamefp16_only"_h,
        "fmin_asimdsamefp16_only"_h,    "fmla_asimdsamefp16_only"_h,
        "fmls_asimdsamefp16_only"_h,    "fmulx_asimdsamefp16_only"_h,
        "fmul_asimdsamefp16_only"_h,    "frecps_asimdsamefp16_only"_h,
        "frsqrts_asimdsamefp16_only"_h, "fsub_asimdsamefp16_only"_h}},
      {"'Vd.'?30:84h, 'Vn.'?30:84h, 'Vm.h['<u11_2120 u2322 lsr>], #'<u1413 "
       "90 *>",
       {"fcmla_asimdelem_c_h"_h}},
      {"'Vd.'[n], 'Vn.'[n]",
       {"abs_asimdmisc_r"_h,    "cls_asimdmisc_r"_h,    "clz_asimdmisc_r"_h,
        "neg_asimdmisc_r"_h,    "not_asimdmisc_r"_h,    "rev32_asimdmisc_r"_h,
        "rev64_asimdmisc_r"_h,  "sqabs_asimdmisc_r"_h,  "sqneg_asimdmisc_r"_h,
        "suqadd_asimdmisc_r"_h, "usqadd_asimdmisc_r"_h, "abs_asimdmisc_r"_h,
        "cls_asimdmisc_r"_h,    "clz_asimdmisc_r"_h,    "cnt_asimdmisc_r"_h,
        "neg_asimdmisc_r"_h,    "rev16_asimdmisc_r"_h,  "rev32_asimdmisc_r"_h,
        "rev64_asimdmisc_r"_h,  "sqabs_asimdmisc_r"_h,  "sqneg_asimdmisc_r"_h,
        "suqadd_asimdmisc_r"_h, "urecpe_asimdmisc_r"_h, "ursqrte_asimdmisc_r"_h,
        "usqadd_asimdmisc_r"_h}},
      {"'Vd.'[n], 'Vn.'[n], #0",
       {"cmeq_asimdmisc_z"_h,
        "cmge_asimdmisc_z"_h,
        "cmgt_asimdmisc_z"_h,
        "cmle_asimdmisc_z"_h,
        "cmlt_asimdmisc_z"_h}},
      {"'Vd.'[n], 'Vn.'[n], 'Vf.'[sz]['<u11_2119 u2322 lsr>]",
       {"mla_asimdelem_r"_h,
        "mls_asimdelem_r"_h,
        "mul_asimdelem_r"_h,
        "sqdmulh_asimdelem_r"_h,
        "sqrdmlah_asimdelem_r"_h,
        "sqrdmlsh_asimdelem_r"_h,
        "sqrdmulh_asimdelem_r"_h}},
      {"'Vd.'[n], 'Vn.'[n], 'Vm.'[n]",
       {"mla_asimdsame_only"_h,       "mls_asimdsame_only"_h,
        "mul_asimdsame_only"_h,       "saba_asimdsame_only"_h,
        "sabd_asimdsame_only"_h,      "shadd_asimdsame_only"_h,
        "shsub_asimdsame_only"_h,     "smaxp_asimdsame_only"_h,
        "smax_asimdsame_only"_h,      "sminp_asimdsame_only"_h,
        "smin_asimdsame_only"_h,      "srhadd_asimdsame_only"_h,
        "uaba_asimdsame_only"_h,      "uabd_asimdsame_only"_h,
        "uhadd_asimdsame_only"_h,     "uhsub_asimdsame_only"_h,
        "umaxp_asimdsame_only"_h,     "umax_asimdsame_only"_h,
        "uminp_asimdsame_only"_h,     "umin_asimdsame_only"_h,
        "urhadd_asimdsame_only"_h,    "addp_asimdsame_only"_h,
        "add_asimdsame_only"_h,       "cmeq_asimdsame_only"_h,
        "cmge_asimdsame_only"_h,      "cmgt_asimdsame_only"_h,
        "cmhi_asimdsame_only"_h,      "cmhs_asimdsame_only"_h,
        "cmtst_asimdsame_only"_h,     "sqadd_asimdsame_only"_h,
        "sqdmulh_asimdsame_only"_h,   "sqrdmulh_asimdsame_only"_h,
        "sqrshl_asimdsame_only"_h,    "sqshl_asimdsame_only"_h,
        "sqsub_asimdsame_only"_h,     "srshl_asimdsame_only"_h,
        "sshl_asimdsame_only"_h,      "sub_asimdsame_only"_h,
        "uqadd_asimdsame_only"_h,     "uqrshl_asimdsame_only"_h,
        "uqshl_asimdsame_only"_h,     "uqsub_asimdsame_only"_h,
        "urshl_asimdsame_only"_h,     "ushl_asimdsame_only"_h,
        "trn1_asimdperm_only"_h,      "trn2_asimdperm_only"_h,
        "uzp1_asimdperm_only"_h,      "uzp2_asimdperm_only"_h,
        "zip1_asimdperm_only"_h,      "zip2_asimdperm_only"_h,
        "sqrdmlah_asimdsame2_only"_h, "sqrdmlsh_asimdsame2_only"_h}},
      {"'Vd.'[n], 'Vn.'[n], 'Vm.'[n], #'<u1211 90 *>",
       {"fcmla_asimdsame2_c"_h}},
      {"'Vd.'[n], 'Vn.'[n], 'Vm.'[n], #'(12?270:90)", {"fcadd_asimdsame2_c"_h}},
      {"'Vd.'[n], 'Vn.'[nl]",
       {"xtn_asimdmisc_n"_h,
        "sqxtn_asimdmisc_n"_h,
        "uqxtn_asimdmisc_n"_h,
        "sqxtun_asimdmisc_n"_h,
        "xtn2_xtn_asimdmisc_n"_h,
        "sqxtn2_sqxtn_asimdmisc_n"_h,
        "uqxtn2_uqxtn_asimdmisc_n"_h,
        "sqxtun2_sqxtun_asimdmisc_n"_h}},
      {"'Vd.'[n], 'Vn.'[nl], 'Vm.'[nl]",
       {"addhn_asimddiff_n"_h,
        "raddhn_asimddiff_n"_h,
        "rsubhn_asimddiff_n"_h,
        "subhn_asimddiff_n"_h,
        "addhn2_addhn_asimddiff_n"_h,
        "raddhn2_raddhn_asimddiff_n"_h,
        "rsubhn2_rsubhn_asimddiff_n"_h,
        "subhn2_subhn_asimddiff_n"_h}},
      {"'Vd.'[nl], 'Vn.'[n], #'(2322?'<u2322 16 *>:8)",
       {"shll_asimdmisc_s"_h, "shll2_shll_asimdmisc_s"_h}},
      {"'Vd.'[nl], 'Vn.'[n], 'Vm.'[n]",
       {"sabal_asimddiff_l"_h,
        "sabdl_asimddiff_l"_h,
        "saddl_asimddiff_l"_h,
        "smlal_asimddiff_l"_h,
        "smlsl_asimddiff_l"_h,
        "smull_asimddiff_l"_h,
        "ssubl_asimddiff_l"_h,
        "uabal_asimddiff_l"_h,
        "uabdl_asimddiff_l"_h,
        "uaddl_asimddiff_l"_h,
        "umlal_asimddiff_l"_h,
        "umlsl_asimddiff_l"_h,
        "umull_asimddiff_l"_h,
        "usubl_asimddiff_l"_h,
        "sabal2_sabal_asimddiff_l"_h,
        "sabdl2_sabdl_asimddiff_l"_h,
        "saddl2_saddl_asimddiff_l"_h,
        "smlal2_smlal_asimddiff_l"_h,
        "smlsl2_smlsl_asimddiff_l"_h,
        "smull2_smull_asimddiff_l"_h,
        "ssubl2_ssubl_asimddiff_l"_h,
        "uabal2_uabal_asimddiff_l"_h,
        "uabdl2_uabdl_asimddiff_l"_h,
        "uaddl2_uaddl_asimddiff_l"_h,
        "umlal2_umlal_asimddiff_l"_h,
        "umlsl2_umlsl_asimddiff_l"_h,
        "umull2_umull_asimddiff_l"_h,
        "usubl2_usubl_asimddiff_l"_h,
        "sqdmlal_asimddiff_l"_h,
        "sqdmlsl_asimddiff_l"_h,
        "sqdmull_asimddiff_l"_h,
        "sqdmlal2_sqdmlal_asimddiff_l"_h,
        "sqdmlsl2_sqdmlsl_asimddiff_l"_h,
        "sqdmull2_sqdmull_asimddiff_l"_h}},
      {"'Vd.'[nl], 'Vn.'[nl], 'Vm.'[n]",
       {"saddw_asimddiff_w"_h,
        "ssubw_asimddiff_w"_h,
        "uaddw_asimddiff_w"_h,
        "usubw_asimddiff_w"_h,
        "saddw2_saddw_asimddiff_w"_h,
        "ssubw2_ssubw_asimddiff_w"_h,
        "uaddw2_uaddw_asimddiff_w"_h,
        "usubw2_usubw_asimddiff_w"_h}},
      {"'Vd.16b, 'Vn.16b",
       {"aesd_b_cryptoaes"_h,
        "aese_b_cryptoaes"_h,
        "aesimc_b_cryptoaes"_h,
        "aesmc_b_cryptoaes"_h}},
      {"'Vd.16b, 'Vn.16b, 'Vm.16b, 'Va.16b",
       {"bcax_vvv16_crypto4"_h, "eor3_vvv16_crypto4"_h}},
      {"'Vd.2d, 'Vn.2d", {"sha512su0_vv2_cryptosha512_2"_h}},
      {"'Vd.2d, 'Vn.2d, 'Vm.2d",
       {"rax1_vvv2_cryptosha512_3"_h, "sha512su1_vvv2_cryptosha512_3"_h}},
      {"'Vd.2d, 'Vn.2d, 'Vm.2d, #'u1510", {"xar_vvv2_crypto3_imm6"_h}},
      {"'Vd.4s, 'Vn.16b, 'Vm.16b",
       {"smmla_asimdsame2_g"_h,
        "ummla_asimdsame2_g"_h,
        "usmmla_asimdsame2_g"_h}},
      {"'Vd.4s, 'Vn.4s",
       {"sha1su1_vv_cryptosha2"_h,
        "sha256su0_vv_cryptosha2"_h,
        "sm4e_vv4_cryptosha512_2"_h}},
      {"'Vd.4s, 'Vn.4s, 'Vm.4s",
       {"sha1su0_vvv_cryptosha3"_h,
        "sha256su1_vvv_cryptosha3"_h,
        "sm3partw1_vvv4_cryptosha512_3"_h,
        "sm3partw2_vvv4_cryptosha512_3"_h,
        "sm4ekey_vvv4_cryptosha512_3"_h}},
      {"'Vd.4s, 'Vn.4s, 'Vm.4s, 'Va.4s", {"sm3ss1_vvv4_crypto4"_h}},
      {"'Vd.4s, 'Vn.4s, 'Vm.s['u1312]",
       {"sm3tt1a_vvv4_crypto3_imm2"_h,
        "sm3tt1b_vvv4_crypto3_imm2"_h,
        "sm3tt2a_vvv4_crypto3_imm2"_h,
        "sm3tt2b_vvv_crypto3_imm2"_h}},
      {"'Vd.4s, 'Vn.4s, 'Vm.s['<u11_2120 u2322 lsr>], #'<u1413 90 *>",
       {"fcmla_asimdelem_c_s"_h}},
      {"'Vd.D[1], 'Rn", {"fmov_v64i_float2int"_h}},
      {"'Vdv, 'Pgl, 'Zn.'[sz]",
       {"andv_r_p_z"_h,
        "eorv_r_p_z"_h,
        "orv_r_p_z"_h,
        "smaxv_r_p_z"_h,
        "sminv_r_p_z"_h,
        "umaxv_r_p_z"_h,
        "uminv_r_p_z"_h}},
      {"'Vt.'(30?16:8)b, #0x'x1816_0905", {"movi_asimdimm_n_b"_h}},
      {"'Vt.'?30:42s, #0x'x1816_0905'(1413?, lsl #'<u1413 8 *>)",
       {"bic_asimdimm_l_sl"_h,
        "movi_asimdimm_l_sl"_h,
        "mvni_asimdimm_l_sl"_h,
        "orr_asimdimm_l_sl"_h}},
      {"'Vt.'?30:42s, #0x'x1816_0905, msl #'(12?16:8)",
       {"movi_asimdimm_m_sm"_h, "mvni_asimdimm_m_sm"_h}},
      {"'Vt.'?30:42s, #'f1816_0905", {"fmov_asimdimm_s_s"_h}},
      {"'Vt.'?30:84h, #0x'x1816_0905'(1413?, lsl #'<u1413 8 *>)",
       {"bic_asimdimm_l_hl"_h,
        "movi_asimdimm_l_hl"_h,
        "mvni_asimdimm_l_hl"_h,
        "orr_asimdimm_l_hl"_h}},
      {"'Vt.'?30:84h, #'f1816_0905", {"fmov_asimdimm_h_h"_h}},
      {"'Vt.2d, #'f1816_0905", {"fmov_asimdimm_d2_d"_h}},
      {"'Vt.2d, #0x'<0xff 56 lsl u18 * 0xff 48 lsl u17 * + 0xff 40 lsl u16 * + "
       "0xff 32 lsl u09 * + 0xff 24 lsl u08 * + 0xff0000 u07 * + 0xff00 u06 * "
       "+ 0xff u05 * + hex>",
       {"movi_asimdimm_d2_d"_h}},
      {"'Wd, 'Pn.'[sz]", {"uqdecp_r_p_r_uw"_h, "uqincp_r_p_r_uw"_h}},
      {"'Wd, 'Wn, 'Xm", {"crc32cx_64c_dp_2src"_h, "crc32x_64c_dp_2src"_h}},
      {"'Wn", {"setf16_only_setf"_h, "setf8_only_setf"_h}},
      {"'Wt, pc'(23?:+)'<s2305 4 *> 'LValue", {"ldr_32_loadlit"_h}},
      {"'Wt, 'Wt2, ['Xns]", {"ldxp_lp32_ldstexcl"_h, "ldaxp_lp32_ldstexcl"_h}},
      {"'Wt, 'Wt2, ['Xns'(2115?, #'<s2115 4 *>)]",
       {"ldnp_32_ldstnapair_offs"_h,
        "ldp_32_ldstpair_off"_h,
        "stnp_32_ldstnapair_offs"_h,
        "stp_32_ldstpair_off"_h}},
      {"'Wt, 'Wt2, ['Xns, #'<s2115 4 *>]!",
       {"ldp_32_ldstpair_pre"_h, "stp_32_ldstpair_pre"_h}},
      {"'Wt, 'Wt2, ['Xns], #'<s2115 4 *>",
       {"ldp_32_ldstpair_post"_h, "stp_32_ldstpair_post"_h}},
      {"'Wt, ['Xns'(2012?, #'s2012)]",
       {"ldapur_32_ldapstl_unscaled"_h,   "ldapurb_32_ldapstl_unscaled"_h,
        "ldapurh_32_ldapstl_unscaled"_h,  "ldapursb_32_ldapstl_unscaled"_h,
        "ldapursh_32_ldapstl_unscaled"_h, "ldur_32_ldst_unscaled"_h,
        "ldurb_32_ldst_unscaled"_h,       "ldurh_32_ldst_unscaled"_h,
        "ldursb_32_ldst_unscaled"_h,      "ldursh_32_ldst_unscaled"_h,
        "stlur_32_ldapstl_unscaled"_h,    "stlurb_32_ldapstl_unscaled"_h,
        "stlurh_32_ldapstl_unscaled"_h,   "stur_32_ldst_unscaled"_h,
        "sturb_32_ldst_unscaled"_h,       "sturh_32_ldst_unscaled"_h,
        "ldtr_32_ldst_unpriv"_h,          "ldtrb_32_ldst_unpriv"_h,
        "ldtrh_32_ldst_unpriv"_h,         "ldtrsb_32_ldst_unpriv"_h,
        "ldtrsh_32_ldst_unpriv"_h,        "sttr_32_ldst_unpriv"_h,
        "sttrb_32_ldst_unpriv"_h,         "sttrh_32_ldst_unpriv"_h}},
      {"'Wt, ['Xns'(2110?, #'u2110)]",
       {"ldrb_32_ldst_pos"_h, "ldrsb_32_ldst_pos"_h, "strb_32_ldst_pos"_h}},
      {"'Wt, ['Xns'(2110?, #'<u2110 2 *>)]",
       {"ldrh_32_ldst_pos"_h, "ldrsh_32_ldst_pos"_h, "strh_32_ldst_pos"_h}},
      {"'Wt, ['Xns'(2110?, #'<u2110 4 *>)]",
       {"ldr_32_ldst_pos"_h, "str_32_ldst_pos"_h}},
      {"'Wt, ['Xns, #'s2012]!",
       {"ldr_32_ldst_immpre"_h,
        "ldrb_32_ldst_immpre"_h,
        "ldrh_32_ldst_immpre"_h,
        "ldrsb_32_ldst_immpre"_h,
        "ldrsh_32_ldst_immpre"_h,
        "str_32_ldst_immpre"_h,
        "strb_32_ldst_immpre"_h,
        "strh_32_ldst_immpre"_h}},
      {"'Wt, ['Xns, 'R13m'(1512=6?]'$), '[extmem]'(12? #'u3130)]",
       {"ldrb_32b_ldst_regoff"_h,
        "ldrb_32bl_ldst_regoff"_h,
        "ldrsb_32b_ldst_regoff"_h,
        "ldrsb_32bl_ldst_regoff"_h,
        "strb_32b_ldst_regoff"_h,
        "strb_32bl_ldst_regoff"_h,
        "ldrh_32_ldst_regoff"_h,
        "ldrsh_32_ldst_regoff"_h,
        "strh_32_ldst_regoff"_h,
        "ldr_32_ldst_regoff"_h,
        "str_32_ldst_regoff"_h}},
      {"'Wt, ['Xns], #'s2012",
       {"ldr_32_ldst_immpost"_h,
        "ldrb_32_ldst_immpost"_h,
        "ldrh_32_ldst_immpost"_h,
        "ldrsb_32_ldst_immpost"_h,
        "ldrsh_32_ldst_immpost"_h,
        "str_32_ldst_immpost"_h,
        "strb_32_ldst_immpost"_h,
        "strh_32_ldst_immpost"_h}},
      {"'Xd",
       {"autdza_64z_dp_1src"_h,
        "autdzb_64z_dp_1src"_h,
        "autiza_64z_dp_1src"_h,
        "autizb_64z_dp_1src"_h,
        "pacdza_64z_dp_1src"_h,
        "pacdzb_64z_dp_1src"_h,
        "paciza_64z_dp_1src"_h,
        "pacizb_64z_dp_1src"_h,
        "xpacd_64z_dp_1src"_h,
        "xpaci_64z_dp_1src"_h}},
      {"'Xd'(1916?, '[mulpat], mul #'<u1916 1 +>'$)'(0905=31?:, '[mulpat])",
       {"decb_r_rs"_h,
        "decd_r_rs"_h,
        "dech_r_rs"_h,
        "decw_r_rs"_h,
        "incb_r_rs"_h,
        "incd_r_rs"_h,
        "inch_r_rs"_h,
        "incw_r_rs"_h,
        "cntb_r_s"_h,
        "cntd_r_s"_h,
        "cnth_r_s"_h,
        "cntw_r_s"_h}},
      {"'Xd, #'s1005", {"rdvl_r_i"_h}},
      {"'Xd, 'AddrPCRelByte", {"adr_only_pcreladdr"_h}},
      {"'Xd, 'AddrPCRelPage", {"adrp_only_pcreladdr"_h}},
      {"'Xd, 'Pn.'[sz]",
       {"decp_r_p_r"_h,
        "incp_r_p_r"_h,
        "sqdecp_r_p_r_x"_h,
        "sqincp_r_p_r_x"_h,
        "uqdecp_r_p_r_x"_h,
        "uqincp_r_p_r_x"_h}},
      {"'Xd, 'Pn.'[sz], 'Wd", {"sqdecp_r_p_r_sx"_h, "sqincp_r_p_r_sx"_h}},
      {"'Xd, 'Vn.'[ntriscal]['<u2016 dup ctz 1 + lsr>]",
       {"mov_umov_asimdins_x_x"_h, "smov_asimdins_x_x"_h}},
      {"'Wd, 'Vn.'[ntriscal]['<u2016 dup ctz 1 + lsr>]",
       {"mov_umov_asimdins_w_w"_h,
        "umov_asimdins_w_w"_h,
        "smov_asimdins_w_w"_h}},
      {"'Xd, 'Wd'(1916?, '[mulpat], mul #'<u1916 1 +>'$)'(0905=31?:, "
       "'[mulpat])",
       {"sqdecb_r_rs_sx"_h,
        "sqdecd_r_rs_sx"_h,
        "sqdech_r_rs_sx"_h,
        "sqdecw_r_rs_sx"_h,
        "sqincb_r_rs_sx"_h,
        "sqincd_r_rs_sx"_h,
        "sqinch_r_rs_sx"_h,
        "sqincw_r_rs_sx"_h}},
      {"'Xd, 'Wn, 'Wm",
       {"smull_smaddl_64wa_dp_3src"_h,
        "smnegl_smsubl_64wa_dp_3src"_h,
        "umull_umaddl_64wa_dp_3src"_h,
        "umnegl_umsubl_64wa_dp_3src"_h}},
      {"'Xd, 'Wn, 'Wm, 'Xa",
       {"smaddl_64wa_dp_3src"_h,
        "smsubl_64wa_dp_3src"_h,
        "umaddl_64wa_dp_3src"_h,
        "umsubl_64wa_dp_3src"_h}},
      {"'Xd, 'Xn, 'Xm", {"smulh_64_dp_3src"_h, "umulh_64_dp_3src"_h}},
      {"'Xd, 'Xn, 'Xms", {"pacga_64p_dp_2src"_h}},
      {"'Xd, 'Xns",
       {"autda_64p_dp_1src"_h,
        "autdb_64p_dp_1src"_h,
        "autia_64p_dp_1src"_h,
        "autib_64p_dp_1src"_h,
        "pacda_64p_dp_1src"_h,
        "pacdb_64p_dp_1src"_h,
        "pacia_64p_dp_1src"_h,
        "pacib_64p_dp_1src"_h}},
      {"'Xd, 'Xns, 'Xms", {"subp_64s_dp_2src"_h, "subps_64s_dp_2src"_h}},
      {"'Xd, p'u1310, 'Pn.'[sz]", {"cntp_r_p_p"_h}},
      {"'Xds, 'Xms, #'s1005", {"addpl_r_ri"_h, "addvl_r_ri"_h}},
      {"'Xds, 'Xns'(2016=31?:, 'Xm)", {"irg_64i_dp_2src"_h}},
      {"'Xds, 'Xns, #'<u2116 16 *>, #'u1310",
       {"addg_64_addsub_immtags"_h, "subg_64_addsub_immtags"_h}},
      {"'Xds, ['Xns'(2012?, #'<s2012 16 *>)]",
       {"st2g_64soffset_ldsttags"_h,
        "stg_64soffset_ldsttags"_h,
        "stz2g_64soffset_ldsttags"_h,
        "stzg_64soffset_ldsttags"_h}},
      {"'Xds, ['Xns, #'<s2012 16 *>]!",
       {"st2g_64spre_ldsttags"_h,
        "stg_64spre_ldsttags"_h,
        "stz2g_64spre_ldsttags"_h,
        "stzg_64spre_ldsttags"_h}},
      {"'Xds, ['Xns], #'<s2012 16 *>",
       {"st2g_64spost_ldsttags"_h,
        "stg_64spost_ldsttags"_h,
        "stz2g_64spost_ldsttags"_h,
        "stzg_64spost_ldsttags"_h}},
      {"'Xn",
       {"blr_64_branch_reg"_h,
        "blraaz_64_branch_reg"_h,
        "blrabz_64_branch_reg"_h,
        "br_64_branch_reg"_h,
        "braaz_64_branch_reg"_h,
        "brabz_64_branch_reg"_h,
        "drps_64e_branch_reg"_h,
        "eret_64e_branch_reg"_h,
        "eretaa_64e_branch_reg"_h,
        "eretab_64e_branch_reg"_h}},
      {"'Xn, #'u2015, #'[nzcv]", {"rmif_only_rmif"_h}},
      {"'Xn, 'Xds",
       {"blraa_64p_branch_reg"_h,
        "blrab_64p_branch_reg"_h,
        "braa_64p_branch_reg"_h,
        "brab_64p_branch_reg"_h}},
      {"'Xns, 'Xms", {"cmpp_subps_64s_dp_2src"_h}},
      {"'Xt",
       {"gcsss1_sys_cr_systeminstrs"_h,
        "gcspushm_sys_cr_systeminstrs"_h,
        "gcsss2_sysl_rc_systeminstrs"_h}},
      {"'Xt, pc'(23?:+)'<s2305 4 *> 'LValue",
       {"ldr_64_loadlit"_h, "ldrsw_64_loadlit"_h}},
      {"'Xt, 'IY", {"mrs_rs_systemmove"_h}},
      {"'Xt, 'Xt2, ['Xns]", {"ldxp_lp64_ldstexcl"_h, "ldaxp_lp64_ldstexcl"_h}},
      {"'Xt, 'Xt2, ['Xns'(2115?, #'<s2115 16 *>)]", {"stgp_64_ldstpair_off"_h}},
      {"'Xt, 'Xt2, ['Xns'(2115?, #'<s2115 4 *>)]", {"ldpsw_64_ldstpair_off"_h}},
      {"'Xt, 'Xt2, ['Xns'(2115?, #'<s2115 8 *>)]",
       {"ldnp_64_ldstnapair_offs"_h,
        "ldp_64_ldstpair_off"_h,
        "stnp_64_ldstnapair_offs"_h,
        "stp_64_ldstpair_off"_h}},
      {"'Xt, 'Xt2, ['Xns, #'<s2115 16 *>]!", {"stgp_64_ldstpair_pre"_h}},
      {"'Xt, 'Xt2, ['Xns, #'<s2115 4 *>]!", {"ldpsw_64_ldstpair_pre"_h}},
      {"'Xt, 'Xt2, ['Xns, #'<s2115 8 *>]!",
       {"ldp_64_ldstpair_pre"_h, "stp_64_ldstpair_pre"_h}},
      {"'Xt, 'Xt2, ['Xns], #'<s2115 16 *>", {"stgp_64_ldstpair_post"_h}},
      {"'Xt, 'Xt2, ['Xns], #'<s2115 4 *>", {"ldpsw_64_ldstpair_post"_h}},
      {"'Xt, 'Xt2, ['Xns], #'<s2115 8 *>",
       {"ldp_64_ldstpair_post"_h, "stp_64_ldstpair_post"_h}},
      {"'Xt, ['Xns'(2012?, #'s2012)]",
       {"ldapur_64_ldapstl_unscaled"_h,
        "ldapursb_64_ldapstl_unscaled"_h,
        "ldapursh_64_ldapstl_unscaled"_h,
        "ldapursw_64_ldapstl_unscaled"_h,
        "ldur_64_ldst_unscaled"_h,
        "ldursb_64_ldst_unscaled"_h,
        "ldursh_64_ldst_unscaled"_h,
        "ldursw_64_ldst_unscaled"_h,
        "stlur_64_ldapstl_unscaled"_h,
        "stur_64_ldst_unscaled"_h,
        "ldtr_64_ldst_unpriv"_h,
        "ldtrsb_64_ldst_unpriv"_h,
        "ldtrsh_64_ldst_unpriv"_h,
        "ldtrsw_64_ldst_unpriv"_h,
        "sttr_64_ldst_unpriv"_h}},
      {"'Xt, ['Xns'(2012?, #'<s2012 16 *>)]", {"ldg_64loffset_ldsttags"_h}},
      {"'Xt, ['Xns'(2212=512?:, #'<s22_2012 8 *>)]!",
       {"ldraa_64w_ldst_pac"_h, "ldrab_64w_ldst_pac"_h}},
      {"'Xt, ['Xns'(2212=512?:, #'<s22_2012 8 *>)]",
       {"ldraa_64_ldst_pac"_h, "ldrab_64_ldst_pac"_h}},
      {"'Xt, ['Xns'(2110?, #'u2110)]", {"ldrsb_64_ldst_pos"_h}},
      {"'Xt, ['Xns'(2110?, #'<u2110 2 *>)]", {"ldrsh_64_ldst_pos"_h}},
      {"'Xt, ['Xns'(2110?, #'<u2110 4 *>)]", {"ldrsw_64_ldst_pos"_h}},
      {"'Xt, ['Xns'(2110?, #'<u2110 8 *>)]",
       {"ldr_64_ldst_pos"_h, "str_64_ldst_pos"_h}},
      {"'Xt, ['Xns, #'s2012]!",
       {"ldr_64_ldst_immpre"_h,
        "ldrsb_64_ldst_immpre"_h,
        "ldrsh_64_ldst_immpre"_h,
        "ldrsw_64_ldst_immpre"_h,
        "str_64_ldst_immpre"_h}},
      {"'Xt, ['Xns, 'R13m'(1512=6?]'$), '[extmem]'(12? #'u3130)]",
       {"ldrsb_64b_ldst_regoff"_h,
        "ldrsb_64bl_ldst_regoff"_h,
        "ldrsh_64_ldst_regoff"_h,
        "ldrsw_64_ldst_regoff"_h,
        "ldr_64_ldst_regoff"_h,
        "str_64_ldst_regoff"_h}},
      {"'Xt, ['Xns], #'s2012",
       {"ldr_64_ldst_immpost"_h,
        "ldrsb_64_ldst_immpost"_h,
        "ldrsh_64_ldst_immpost"_h,
        "ldrsw_64_ldst_immpost"_h,
        "str_64_ldst_immpost"_h}},
      {"'Xt, #'u1816, C'u1512, C'u1108, #'u0705", {"sysl_rc_systeminstrs"_h}},
      {"'Zd, 'Zn", {"movprfx_z_z"_h}},
      {"'Zd.'?22:ds, 'Zn.'?22:ds, 'Zm.'?22:ds",
       {"adclb_z_zzz"_h, "adclt_z_zzz"_h, "sbclb_z_zzz"_h, "sbclt_z_zzz"_h}},
      {"'Zd.'[sz]'(1916?, '[mulpat], mul #'<u1916 1 +>'$)'(0905=31?:, "
       "'[mulpat])",
       {"decd_z_zs"_h,
        "dech_z_zs"_h,
        "decw_z_zs"_h,
        "incd_z_zs"_h,
        "inch_z_zs"_h,
        "incw_z_zs"_h,
        "sqdecd_z_zs"_h,
        "sqdech_z_zs"_h,
        "sqdecw_z_zs"_h,
        "sqincd_z_zs"_h,
        "sqinch_z_zs"_h,
        "sqincw_z_zs"_h,
        "uqdecd_z_zs"_h,
        "uqdech_z_zs"_h,
        "uqdecw_z_zs"_h,
        "uqincd_z_zs"_h,
        "uqinch_z_zs"_h,
        "uqincw_z_zs"_h}},
      {"'Zd.'[sz], #'s0905, #'s2016", {"index_z_ii"_h}},
      {"'Zd.'[sz], #'s0905, '(2322=3?'Xm:'Wm)", {"index_z_ir"_h}},
      {"'Zd.'[sz], #'s1205'(13?, lsl #8)", {"mov_dup_z_i"_h}},
      {"'Zd.'[sz], '(2322=3?'Xn:'Wn)", {"insr_z_r"_h}},
      {"'Zd.'[sz], '(2322=3?'Xn:'Wn), #'s2016", {"index_z_ri"_h}},
      {"'Zd.'[sz], '(2322=3?'Xn:'Wn), '(2322=3?'Xm:'Wm)", {"index_z_rr"_h}},
      {"'Zd.'[sz], '(2322=3?'Xns:'Wns)", {"mov_dup_z_r"_h}},
      {"'Zd.'[sz], #'f1205", {"fmov_fdup_z_i"_h}},
      {"'Zd.'[sz], 'Pgl, 'Zd.'[sz], 'Zn.'[sz]",
       {"clasta_z_p_zz"_h, "clastb_z_p_zz"_h, "splice_z_p_zz_des"_h}},
      {"'Zd.'[sz], 'Pgl, 'Zn.'[sz]", {"compact_z_p_z"_h}},
      {"'Zd.'[sz], 'Pgl, {'Zn.'[sz], 'Zn2.'[sz]}", {"splice_z_p_zz_con"_h}},
      {"'Zd.'[sz], 'Pgl/'?16:mz, 'Zn.'[sz]", {"movprfx_z_p_z"_h}},
      {"'Zd.'[sz], 'Pgl/m, '(2322=3?'Xns:'Wns)", {"mov_cpy_z_p_r"_h}},
      {"'Zd.'[sz], 'Pgl/m, 'Vnv", {"mov_cpy_z_p_v"_h}},
      {"'Zd.'[sz], 'Pgl/m, 'Zd.'[sz], #'(05?1:0).0",
       {"fmaxnm_z_p_zs"_h,
        "fmax_z_p_zs"_h,
        "fminnm_z_p_zs"_h,
        "fmin_z_p_zs"_h}},
      {"'Zd.'[sz], 'Pgl/m, 'Zd.'[sz], #'(05?1.0:0.5)",
       {"fadd_z_p_zs"_h, "fsubr_z_p_zs"_h, "fsub_z_p_zs"_h}},
      {"'Zd.'[sz], 'Pgl/m, 'Zd.'[sz], #'(05?2.0:0.5)", {"fmul_z_p_zs"_h}},
      {"'Zd.'[sz], 'Pgl/m, 'Zd.'[sz], 'Zn.'[sz]",
       {"addp_z_p_zz"_h,    "shadd_z_p_zz"_h,   "shsub_z_p_zz"_h,
        "shsubr_z_p_zz"_h,  "smaxp_z_p_zz"_h,   "sminp_z_p_zz"_h,
        "sqadd_z_p_zz"_h,   "sqrshl_z_p_zz"_h,  "sqrshlr_z_p_zz"_h,
        "sqshl_z_p_zz"_h,   "sqshlr_z_p_zz"_h,  "sqsub_z_p_zz"_h,
        "sqsubr_z_p_zz"_h,  "srhadd_z_p_zz"_h,  "srshl_z_p_zz"_h,
        "srshlr_z_p_zz"_h,  "suqadd_z_p_zz"_h,  "uhadd_z_p_zz"_h,
        "uhsub_z_p_zz"_h,   "uhsubr_z_p_zz"_h,  "umaxp_z_p_zz"_h,
        "uminp_z_p_zz"_h,   "uqadd_z_p_zz"_h,   "uqrshl_z_p_zz"_h,
        "uqrshlr_z_p_zz"_h, "uqshl_z_p_zz"_h,   "uqshlr_z_p_zz"_h,
        "uqsub_z_p_zz"_h,   "uqsubr_z_p_zz"_h,  "urhadd_z_p_zz"_h,
        "urshl_z_p_zz"_h,   "urshlr_z_p_zz"_h,  "usqadd_z_p_zz"_h,
        "mul_z_p_zz"_h,     "smulh_z_p_zz"_h,   "umulh_z_p_zz"_h,
        "sabd_z_p_zz"_h,    "smax_z_p_zz"_h,    "smin_z_p_zz"_h,
        "uabd_z_p_zz"_h,    "umax_z_p_zz"_h,    "umin_z_p_zz"_h,
        "add_z_p_zz"_h,     "subr_z_p_zz"_h,    "sub_z_p_zz"_h,
        "and_z_p_zz"_h,     "bic_z_p_zz"_h,     "eor_z_p_zz"_h,
        "orr_z_p_zz"_h,     "asrr_z_p_zz"_h,    "asr_z_p_zz"_h,
        "lslr_z_p_zz"_h,    "lsl_z_p_zz"_h,     "lsrr_z_p_zz"_h,
        "lsr_z_p_zz"_h,     "faddp_z_p_zz"_h,   "fmaxnmp_z_p_zz"_h,
        "fmaxp_z_p_zz"_h,   "fminnmp_z_p_zz"_h, "fminp_z_p_zz"_h,
        "fabd_z_p_zz"_h,    "fadd_z_p_zz"_h,    "fdivr_z_p_zz"_h,
        "fdiv_z_p_zz"_h,    "fmaxnm_z_p_zz"_h,  "fmax_z_p_zz"_h,
        "fminnm_z_p_zz"_h,  "fmin_z_p_zz"_h,    "fmulx_z_p_zz"_h,
        "fmul_z_p_zz"_h,    "fscale_z_p_zz"_h,  "fsubr_z_p_zz"_h,
        "fsub_z_p_zz"_h,    "sdiv_z_p_zz"_h,    "sdivr_z_p_zz"_h,
        "udiv_z_p_zz"_h,    "udivr_z_p_zz"_h}},
      {"'Zd.'[sz], 'Pgl/m, 'Zd.'[sz], 'Zn.'[sz], #'<u1615 90 *>",
       {"fcadd_z_p_zz"_h}},
      {"'Zd.'[sz], 'Pgl/m, 'Zm.'[sz], 'Zn.'[sz]",
       {"mad_z_p_zzz"_h, "msb_z_p_zzz"_h}},
      {"'Zd.'[sz], 'Pgl/m, 'Zn.'[sz]",
       {"sqabs_z_p_z"_h,  "sqneg_z_p_z"_h,  "frinta_z_p_z"_h, "frinti_z_p_z"_h,
        "frintm_z_p_z"_h, "frintn_z_p_z"_h, "frintp_z_p_z"_h, "frintx_z_p_z"_h,
        "frintz_z_p_z"_h, "frecpx_z_p_z"_h, "fsqrt_z_p_z"_h,  "abs_z_p_z"_h,
        "cls_z_p_z"_h,    "clz_z_p_z"_h,    "cnot_z_p_z"_h,   "cnt_z_p_z"_h,
        "fabs_z_p_z"_h,   "fneg_z_p_z"_h,   "neg_z_p_z"_h,    "not_z_p_z"_h,
        "sxtb_z_p_z"_h,   "sxth_z_p_z"_h,   "sxtw_z_p_z"_h,   "uxtb_z_p_z"_h,
        "uxth_z_p_z"_h,   "uxtw_z_p_z"_h,   "rbit_z_p_z"_h,   "revb_z_z"_h,
        "revh_z_z"_h,     "revw_z_z"_h}},
      {"'Zd.'[sz], 'Pgl/m, 'Zn.'[sz], 'Zm.'[sz]",
       {"mla_z_p_zzz"_h,
        "mls_z_p_zzz"_h,
        "fmad_z_p_zzz"_h,
        "fmla_z_p_zzz"_h,
        "fmls_z_p_zzz"_h,
        "fmsb_z_p_zzz"_h,
        "fnmad_z_p_zzz"_h,
        "fnmla_z_p_zzz"_h,
        "fnmls_z_p_zzz"_h,
        "fnmsb_z_p_zzz"_h}},
      {"'Zd.'[sz], 'Pgl/m, 'Zn.'[sz], 'Zm.'[sz], #'<u1413 90 *>",
       {"fcmla_z_p_zzz"_h}},
      {"'Zd.'[sz], 'Pgl/m, 'Zn.'[sszh]", {"sadalp_z_p_z"_h, "uadalp_z_p_z"_h}},
      {"'Zd.'[sz], 'Pgl/z, 'Zn.'[sz], 'Zm.'[sz]", {"histcnt_z_p_zz"_h}},
      {"'Zd.'[sz], 'Pm/'?14:mz, #'s1205'(13?, lsl #8)",
       {"mov_cpy_z_o_i"_h, "mov_cpy_z_p_i"_h}},
      {"'Zd.'[sz], 'Pm/m, #'f1205", {"fmov_fcpy_z_p_i"_h}},
      {"'Zd.'[sz], 'Pn",
       {"decp_z_p_z"_h,
        "incp_z_p_z"_h,
        "sqdecp_z_p_z"_h,
        "sqincp_z_p_z"_h,
        "uqdecp_z_p_z"_h,
        "uqincp_z_p_z"_h}},
      {"'Zd.'[sz], 'Vnv", {"insr_z_v"_h}},
      {"'Zd.'[sz], 'Zd.'[sz], #'s1205",
       {"mul_z_zi"_h, "smax_z_zi"_h, "smin_z_zi"_h}},
      {"'Zd.'[sz], 'Zd.'[sz], #'u1205", {"umax_z_zi"_h, "umin_z_zi"_h}},
      {"'Zd.'[sz], 'Zd.'[sz], #'u1205'(13?, lsl #8)",
       {"add_z_zi"_h,
        "sqadd_z_zi"_h,
        "sqsub_z_zi"_h,
        "sub_z_zi"_h,
        "subr_z_zi"_h,
        "uqadd_z_zi"_h,
        "uqsub_z_zi"_h}},
      {"'Zd.'[sz], 'Zd.'[sz], 'Zn.'[sz], #'(10?27:9)0",
       {"cadd_z_zz"_h, "sqcadd_z_zz"_h}},
      {"'Zd.'[sz], 'Zd.'[sz], 'Zn.'[sz], #'u1816", {"ftmad_z_zzi"_h}},
      {"'Zd.'[sz], 'Zn.'[sz]",
       {"frecpe_z_z"_h, "frsqrte_z_z"_h, "rev_z_z"_h, "fexpa_z_z"_h}},
      {"'Zd.'[sz], 'Zn.'[sz], 'Zm.'[sz]",
       {"bdep_z_zz"_h,      "bext_z_zz"_h,      "bgrp_z_zz"_h,
        "eorbt_z_zz"_h,     "eortb_z_zz"_h,     "mul_z_zz"_h,
        "smulh_z_zz"_h,     "sqdmulh_z_zz"_h,   "sqrdmulh_z_zz"_h,
        "tbx_z_zz"_h,       "umulh_z_zz"_h,     "saba_z_zzz"_h,
        "sqrdmlah_z_zzz"_h, "sqrdmlsh_z_zzz"_h, "uaba_z_zzz"_h,
        "fmmla_z_zzz_s"_h,  "fmmla_z_zzz_d"_h,  "trn1_z_zz"_h,
        "trn2_z_zz"_h,      "uzp1_z_zz"_h,      "uzp2_z_zz"_h,
        "zip1_z_zz"_h,      "zip2_z_zz"_h,      "add_z_zz"_h,
        "sqadd_z_zz"_h,     "sqsub_z_zz"_h,     "sub_z_zz"_h,
        "uqadd_z_zz"_h,     "uqsub_z_zz"_h,     "fadd_z_zz"_h,
        "fmul_z_zz"_h,      "frecps_z_zz"_h,    "frsqrts_z_zz"_h,
        "fsub_z_zz"_h,      "ftsmul_z_zz"_h,    "ftssel_z_zz"_h}},
      {"'Zd.'[sz], 'Zn.'[sz], 'Zm.'[sz], #'<u1110 90 *>",
       {"cmla_z_zzz"_h, "sqrdcmlah_z_zzz"_h}},
      {"'Zd.'[sz], 'Zn.'[sz], 'Zm.d",
       {"asr_z_zw"_h, "lsl_z_zw"_h, "lsr_z_zw"_h}},
      {"'Zd.'[sz], 'Zn.'[sszq], 'Zm.'[sszq]", {"sdot_z_zzz"_h, "udot_z_zzz"_h}},
      {"'Zd.'[sz], 'Zn.'[sszq], 'Zm.'[sszq], #'<u1110 90 *>", {"cdot_z_zzz"_h}},
      {"'Zd.'[sz], ['Zn.'[sz], 'Zm.'[sz]'(1110?, lsl #'u1110)]",
       {"adr_z_az_sd_same_scaled"_h}},
      {"'Zd.'[sz], {'Zn.'[sz], 'Zn2.'[sz]}, 'Zm.'[sz]", {"tbl_z_zz_2"_h}},
      {"'Zd.'[sz], {'Zn.'[sz]}, 'Zm.'[sz]", {"tbl_z_zz_1"_h}},
      {"'Zd.'[sz], p'u1310, 'Zn.'[sz], 'Zm.'[sz]", {"sel_z_p_zz"_h}},
      {"'Zd.'[sz], p'u1310/m, 'Zn.'[sz]", {"mov_sel_z_p_zz"_h}},
      {"'Zd.'[sszdup], '[sszdup]'u0905", {"mov_1_dup_z_zi"_h}},
      {"'Zd.'[sszdup], 'Zn.'[sszdup]['<u2322_2016 dup ctz 1 + lsr>]",
       {"mov_dup_z_zi"_h}},
      {"'Zd.'[flogbsz], 'Pgl/m, 'Zn.'[flogbsz]", {"flogb_z_p_z"_h}},
      {"'Zd.'[sszh], 'Zn.'[sz], 'Zm.'[sz]",
       {"addhnb_z_zz"_h,
        "addhnt_z_zz"_h,
        "raddhnb_z_zz"_h,
        "raddhnt_z_zz"_h,
        "rsubhnb_z_zz"_h,
        "rsubhnt_z_zz"_h,
        "subhnb_z_zz"_h,
        "subhnt_z_zz"_h}},
      {"'Zd.'(17?d:'[sszlog]), 'Zd.'(17?d:'[sszlog]), 'ITriSvel",
       {"and_z_zi"_h, "eor_z_zi"_h, "orr_z_zi"_h}},
      {"'Zd.'[sszshd], 'Zn.'[sszshs], #'<u2322_2016 1 34 u2322_2019 clz32 - "
       "lsl ->",
       {"sshllb_z_zi"_h, "sshllt_z_zi"_h, "ushllb_z_zi"_h, "ushllt_z_zi"_h}},
      {"'Zd.'[sszshu], 'Pgl/m, 'Zd.'[sszshu], #'<u2322_0905 1 34 u2322_0908 "
       "clz32 - lsl ->",
       {"lsl_z_p_zi"_h, "sqshl_z_p_zi"_h, "sqshlu_z_p_zi"_h, "uqshl_z_p_zi"_h}},
      {"'Zd.'[sszshu], 'Pgl/m, 'Zd.'[sszshu], #'<1 35 u2322_0908 clz32 - lsl "
       "u2322_0905 ->",
       {"asrd_z_p_zi"_h,
        "asr_z_p_zi"_h,
        "lsr_z_p_zi"_h,
        "srshr_z_p_zi"_h,
        "urshr_z_p_zi"_h}},
      {"'Zd.'[sszshs], 'Zn.'[sszshd]",
       {"sqxtnb_z_zz"_h,
        "sqxtnt_z_zz"_h,
        "sqxtunb_z_zz"_h,
        "sqxtunt_z_zz"_h,
        "uqxtnb_z_zz"_h,
        "uqxtnt_z_zz"_h}},
      {"'Zd.'[sszshs], 'Zn.'[sszshd], #'<1 35 u2322_2019 clz32 - lsl "
       "u2322_2016 ->",
       {"rshrnb_z_zi"_h,
        "rshrnt_z_zi"_h,
        "shrnb_z_zi"_h,
        "shrnt_z_zi"_h,
        "sqrshrnb_z_zi"_h,
        "sqrshrnt_z_zi"_h,
        "sqrshrunb_z_zi"_h,
        "sqrshrunt_z_zi"_h,
        "sqshrnb_z_zi"_h,
        "sqshrnt_z_zi"_h,
        "sqshrunb_z_zi"_h,
        "sqshrunt_z_zi"_h,
        "uqrshrnb_z_zi"_h,
        "uqrshrnt_z_zi"_h,
        "uqshrnb_z_zi"_h,
        "uqshrnt_z_zi"_h}},
      {"'Zd.'[sszshs], 'Zn.'[sszshs], #'<u2322_2016 1 34 u2322_2019 clz32 - "
       "lsl ->",
       {"lsl_z_zi"_h, "sli_z_zzi"_h}},
      {"'Zd.'[sszshs], 'Zn.'[sszshs], #'<1 35 u2322_2019 clz32 - lsl "
       "u2322_2016 ->",
       {"asr_z_zi"_h,
        "lsr_z_zi"_h,
        "sri_z_zzi"_h,
        "srsra_z_zi"_h,
        "ssra_z_zi"_h,
        "ursra_z_zi"_h,
        "usra_z_zi"_h}},
      {"'Zd.'[sszshs], 'Zd.'[sszshs], 'Zn.'[sszshs], #'<1 35 u2322_2019 clz32 "
       "- lsl u2322_2016 ->",
       {"xar_z_zzi"_h}},
      {"'Zd.b, 'Zd.b", {"aesimc_z_z"_h, "aesmc_z_z"_h}},
      {"'Zd.b, 'Zd.b, 'Zn.b", {"aesd_z_zz"_h, "aese_z_zz"_h}},
      {"'Zd.b, 'Zd.b, 'Zn.b, #'u2016_1210", {"ext_z_zi_des"_h}},
      {"'Zd.b, 'Zn.b, 'Zm.b", {"histseg_z_zz"_h, "pmul_z_zz"_h}},
      {"'Zd.b, {'Zn.b, 'Zn2.b}, #'u2016_1210", {"ext_z_zi_con"_h}},
      {"'Zd.d, 'Pgl/m, 'Zn.d",
       {"fcvtzs_z_p_z_d2x"_h,
        "fcvtzu_z_p_z_d2x"_h,
        "scvtf_z_p_z_x2d"_h,
        "ucvtf_z_p_z_x2d"_h}},
      {"'Zd.d, 'Pgl/m, 'Zn.h",
       {"fcvt_z_p_z_h2d"_h, "fcvtzs_z_p_z_fp162x"_h, "fcvtzu_z_p_z_fp162x"_h}},
      {"'Zd.d, 'Pgl/m, 'Zn.s",
       {"fcvt_z_p_z_s2d"_h,
        "fcvtlt_z_p_z_s2d"_h,
        "fcvtzs_z_p_z_s2x"_h,
        "fcvtzu_z_p_z_s2x"_h,
        "scvtf_z_p_z_w2d"_h,
        "ucvtf_z_p_z_w2d"_h}},
      {"'Zd.d, 'Zd.d, 'Zm.d, 'Zn.d",
       {"bcax_z_zzz"_h,
        "bsl1n_z_zzz"_h,
        "bsl2n_z_zzz"_h,
        "bsl_z_zzz"_h,
        "eor3_z_zzz"_h,
        "nbsl_z_zzz"_h}},
      {"'Zd.d, 'Zn.d", {"mov_orr_z_zz"_h}},
      {"'Zd.d, 'Zn.d, 'Zm.d",
       {"rax1_z_zz"_h, "and_z_zz"_h, "bic_z_zz"_h, "eor_z_zz"_h, "orr_z_zz"_h}},
      {"'Zd.d, 'Zn.d, z'u1916.d['u20]",
       {"fmla_z_zzzi_d"_h,
        "fmls_z_zzzi_d"_h,
        "fmul_z_zzi_d"_h,
        "mla_z_zzzi_d"_h,
        "mls_z_zzzi_d"_h,
        "mul_z_zzi_d"_h,
        "sqdmulh_z_zzi_d"_h,
        "sqrdmulh_z_zzi_d"_h,
        "sqrdmlah_z_zzzi_d"_h,
        "sqrdmlsh_z_zzzi_d"_h}},
      {"'Zd.d, 'Zn.h, z'u1916.h['u20]", {"sdot_z_zzzi_d"_h, "udot_z_zzzi_d"_h}},
      {"'Zd.d, 'Zn.h, z'u1916.h['u20], #'<u1110 90 *>", {"cdot_z_zzzi_d"_h}},
      {"'Zd.d, 'Zn.s, z'u1916.s['u20_11]",
       {"smlalb_z_zzzi_d"_h,
        "smlalt_z_zzzi_d"_h,
        "smlslb_z_zzzi_d"_h,
        "smlslt_z_zzzi_d"_h,
        "smullb_z_zzi_d"_h,
        "smullt_z_zzi_d"_h,
        "sqdmullb_z_zzi_d"_h,
        "sqdmullt_z_zzi_d"_h,
        "sqdmlalb_z_zzzi_d"_h,
        "sqdmlalt_z_zzzi_d"_h,
        "sqdmlslb_z_zzzi_d"_h,
        "sqdmlslt_z_zzzi_d"_h,
        "umlalb_z_zzzi_d"_h,
        "umlalt_z_zzzi_d"_h,
        "umlslb_z_zzzi_d"_h,
        "umlslt_z_zzzi_d"_h,
        "umullb_z_zzi_d"_h,
        "umullt_z_zzi_d"_h}},
      {"'Zd.d, ['Zn.d, 'Zm.d, sxtw'(1110? #'u1110)]",
       {"adr_z_az_d_s32_scaled"_h}},
      {"'Zd.d, ['Zn.d, 'Zm.d, uxtw'(1110? #'u1110)]",
       {"adr_z_az_d_u32_scaled"_h}},
      {"'Zd.h, 'Pgl/m, 'Zn.d",
       {"fcvt_z_p_z_d2h"_h, "scvtf_z_p_z_x2fp16"_h, "ucvtf_z_p_z_x2fp16"_h}},
      {"'Zd.h, 'Pgl/m, 'Zn.h",
       {"fcvtzs_z_p_z_fp162h"_h,
        "fcvtzu_z_p_z_fp162h"_h,
        "scvtf_z_p_z_h2fp16"_h,
        "ucvtf_z_p_z_h2fp16"_h}},
      {"'Zd.h, 'Pgl/m, 'Zn.s",
       {"fcvt_z_p_z_s2h"_h,
        "fcvtnt_z_p_z_s2h"_h,
        "bfcvt_z_p_z_s2bf"_h,
        "bfcvtnt_z_p_z_s2bf"_h,
        "scvtf_z_p_z_w2fp16"_h,
        "ucvtf_z_p_z_w2fp16"_h}},
      {"'Zd.h, 'Zn.h, z'u1816.h['u2019], #'<u1110 90 *>",
       {"cmla_z_zzzi_h"_h, "fcmla_z_zzzi_h"_h, "sqrdcmlah_z_zzzi_h"_h}},
      {"'Zd.h, 'Zn.h, z'u1816.h['u22_2019]",
       {"fmla_z_zzzi_h"_h,
        "fmls_z_zzzi_h"_h,
        "fmul_z_zzi_h"_h,
        "mla_z_zzzi_h"_h,
        "mls_z_zzzi_h"_h,
        "mul_z_zzi_h"_h,
        "sqdmulh_z_zzi_h"_h,
        "sqrdmulh_z_zzi_h"_h,
        "sqrdmlah_z_zzzi_h"_h,
        "sqrdmlsh_z_zzzi_h"_h}},
      {"'Zd.q, 'Zn.d, 'Zm.d", {"pmullb_z_zz_q"_h, "pmullt_z_zz_q"_h}},
      {"'Zd.s, 'Pgl/m, 'Zn.d",
       {"fcvt_z_p_z_d2s"_h,
        "fcvtnt_z_p_z_d2s"_h,
        "fcvtx_z_p_z_d2s"_h,
        "fcvtxnt_z_p_z_d2s"_h,
        "fcvtzs_z_p_z_d2w"_h,
        "fcvtzu_z_p_z_d2w"_h,
        "scvtf_z_p_z_x2s"_h,
        "ucvtf_z_p_z_x2s"_h}},
      {"'Zd.s, 'Pgl/m, 'Zn.h",
       {"fcvt_z_p_z_h2s"_h,
        "fcvtlt_z_p_z_h2s"_h,
        "fcvtzs_z_p_z_fp162w"_h,
        "fcvtzu_z_p_z_fp162w"_h}},
      {"'Zd.s, 'Pgl/m, 'Zn.s",
       {"fcvtzs_z_p_z_s2w"_h,
        "fcvtzu_z_p_z_s2w"_h,
        "urecpe_z_p_z"_h,
        "ursqrte_z_p_z"_h,
        "scvtf_z_p_z_w2s"_h,
        "ucvtf_z_p_z_w2s"_h}},
      {"'Zd.s, 'Zd.s, 'Zn.s", {"sm4e_z_zz"_h}},
      {"'Zd.s, 'Zn.b, 'Zm.b",
       {"smmla_z_zzz"_h, "ummla_z_zzz"_h, "usmmla_z_zzz"_h, "usdot_z_zzz_s"_h}},
      {"'Zd.s, 'Zn.b, z'u1816.b['u2019]",
       {"sdot_z_zzzi_s"_h,
        "sudot_z_zzzi_s"_h,
        "udot_z_zzzi_s"_h,
        "usdot_z_zzzi_s"_h}},
      {"'Zd.s, 'Zn.b, z'u1816.b['u2019], #'<u1110 90 *>", {"cdot_z_zzzi_s"_h}},
      {"'Zd.s, 'Zn.h, 'Zm.h",
       {"fmlalb_z_zzz"_h,
        "fmlalt_z_zzz"_h,
        "fmlslb_z_zzz"_h,
        "fmlslt_z_zzz"_h,
        "bfdot_z_zzz"_h,
        "bfmlalb_z_zzz"_h,
        "bfmlalt_z_zzz"_h,
        "bfmmla_z_zzz"_h}},
      {"'Zd.s, 'Zn.h, z'u1816.h['u2019_11]",
       {"fmlalb_z_zzzi_s"_h,   "fmlalt_z_zzzi_s"_h,   "fmlslb_z_zzzi_s"_h,
        "fmlslt_z_zzzi_s"_h,   "sqdmlalb_z_zzzi_s"_h, "sqdmlalt_z_zzzi_s"_h,
        "sqdmlslb_z_zzzi_s"_h, "sqdmlslt_z_zzzi_s"_h, "bfmlalb_z_zzzi"_h,
        "bfmlalt_z_zzzi"_h,    "smlalb_z_zzzi_s"_h,   "smlalt_z_zzzi_s"_h,
        "smlslb_z_zzzi_s"_h,   "smlslt_z_zzzi_s"_h,   "smullb_z_zzi_s"_h,
        "smullt_z_zzi_s"_h,    "sqdmullb_z_zzi_s"_h,  "sqdmullt_z_zzi_s"_h,
        "umlalb_z_zzzi_s"_h,   "umlalt_z_zzzi_s"_h,   "umlslb_z_zzzi_s"_h,
        "umlslt_z_zzzi_s"_h,   "umullb_z_zzi_s"_h,    "umullt_z_zzi_s"_h}},
      {"'Zd.s, 'Zn.h, z'u1816.h['u2019]", {"bfdot_z_zzzi"_h}},
      {"'Zd.s, 'Zn.s, 'Zm.s", {"sm4ekey_z_zz"_h}},
      {"'Zd.s, 'Zn.s, z'u1816.s['u2019]",
       {"fmla_z_zzzi_s"_h,
        "fmls_z_zzzi_s"_h,
        "fmul_z_zzi_s"_h,
        "mla_z_zzzi_s"_h,
        "mls_z_zzzi_s"_h,
        "mul_z_zzi_s"_h,
        "sqdmulh_z_zzi_s"_h,
        "sqrdmulh_z_zzi_s"_h,
        "sqrdmlah_z_zzzi_s"_h,
        "sqrdmlsh_z_zzzi_s"_h}},
      {"'Zd.s, 'Zn.s, z'u1916.s['u20], #'<u1110 90 *>",
       {"cmla_z_zzzi_s"_h, "fcmla_z_zzzi_s"_h, "sqrdcmlah_z_zzzi_s"_h}},
      {"'[prefop], pc'(23?:+)'<s2305 4 *> 'LValue", {"prfm_p_loadlit"_h}},
      {"'[prefop], ['Xns'(2012?, #'s2012)]", {"prfum_p_ldst_unscaled"_h}},
      {"'[prefop], ['Xns'(2110?, #'<u2110 8 *>)]", {"prfm_p_ldst_pos"_h}},
      {"'[prefop], ['Xns, 'R13m'(1512=6?]'$), '[extmem]'(12? #3)]",
       {"prfm_p_ldst_regoff"_h}},
      {"'[prefsveop], 'Pgl, ['Xns'(2116?, #'s2116, mul vl)]",
       {"prfb_i_p_bi_s"_h,
        "prfd_i_p_bi_s"_h,
        "prfh_i_p_bi_s"_h,
        "prfw_i_p_bi_s"_h}},
      {"'[prefsveop], 'Pgl, ['Xns, 'Rm'(2423?, lsl #'u2423)]",
       {"prfb_i_p_br_s"_h,
        "prfd_i_p_br_s"_h,
        "prfh_i_p_br_s"_h,
        "prfw_i_p_br_s"_h}},
      {"'[prefsveop], 'Pgl, ['Xns, 'Zm.d'(1413?, lsl #'u1413)]",
       {"prfb_i_p_bz_d_64_scaled"_h,
        "prfd_i_p_bz_d_64_scaled"_h,
        "prfh_i_p_bz_d_64_scaled"_h,
        "prfw_i_p_bz_d_64_scaled"_h}},
      {"'[prefsveop], 'Pgl, ['Zn.d'(2016?, #'u2016)]",
       {"prfb_i_p_ai_d"_h,
        "prfd_i_p_ai_d"_h,
        "prfh_i_p_ai_d"_h,
        "prfw_i_p_ai_d"_h}},
      {"'[prefsveop], 'Pgl, ['Zn.s'(2016?, #'u2016)]",
       {"prfb_i_p_ai_s"_h,
        "prfd_i_p_ai_s"_h,
        "prfh_i_p_ai_s"_h,
        "prfw_i_p_ai_s"_h}},
      {"'[prefsveop], 'Pgl, ['Xns, 'Zm.d, '?22:suxtw'(2423? #'u2423)]",
       {"prfb_i_p_bz_d_x32_scaled"_h,
        "prfd_i_p_bz_d_x32_scaled"_h,
        "prfh_i_p_bz_d_x32_scaled"_h,
        "prfw_i_p_bz_d_x32_scaled"_h}},
      {"'[prefsveop], 'Pgl, ['Xns, 'Zm.s, '?22:suxtw #1]",
       {"prfh_i_p_bz_s_x32_scaled"_h}},
      {"'[prefsveop], 'Pgl, ['Xns, 'Zm.s, '?22:suxtw #2]",
       {"prfw_i_p_bz_s_x32_scaled"_h}},
      {"'[prefsveop], 'Pgl, ['Xns, 'Zm.s, '?22:suxtw #3]",
       {"prfd_i_p_bz_s_x32_scaled"_h}},
      {"'[prefsveop], 'Pgl, ['Xns, 'Zm.s, '?22:suxtw]",
       {"prfb_i_p_bz_s_x32_scaled"_h}},
      {"'[sz]'u0400, 'Pgl, 'Zn.'[sz]",
       {"lasta_v_p_z"_h,
        "lastb_v_p_z"_h,
        "faddv_v_p_z"_h,
        "fmaxnmv_v_p_z"_h,
        "fmaxv_v_p_z"_h,
        "fminnmv_v_p_z"_h,
        "fminv_v_p_z"_h}},
      {"'[sz]'u0400, 'Pgl, '[sz]'u0400, 'Zn.'[sz]",
       {"clasta_v_p_z"_h, "clastb_v_p_z"_h, "fadda_v_p_z"_h}},
      {"p'u1310, 'Pn.b", {"ptest_p_p"_h}},
      {"{#0x'x2005}",
       {"dcps1_dc_exception"_h,
        "dcps2_dc_exception"_h,
        "dcps3_dc_exception"_h}},
      {"{'Vt.'[nload]}, ['Xns]'(23?, 'Xmr1)",
       {"ld1_asisdlse_r1_1v"_h,
        "ld1_asisdlsep_i1_i1"_h,
        "ld1_asisdlsep_r1_r1"_h,
        "st1_asisdlse_r1_1v"_h,
        "st1_asisdlsep_i1_i1"_h,
        "st1_asisdlsep_r1_r1"_h}},
      {"{'Vt.'[nload]}, ['Xns]'(23?, 'Xmz1)",
       {"ld1r_asisdlsop_r1_i"_h,
        "ld1r_asisdlsop_rx1_r"_h,
        "ld1r_asisdlso_r1"_h}},
      {"{'Vt.'[nload], 'Vt2.'[nload]}, ['Xns]'(23?, 'Xmr2)",
       {"ld2_asisdlse_r2"_h,
        "ld2_asisdlsep_i2_i"_h,
        "ld2_asisdlsep_r2_r"_h,
        "st2_asisdlse_r2"_h,
        "st2_asisdlsep_i2_i"_h,
        "st2_asisdlsep_r2_r"_h,
        "ld1_asisdlse_r2_2v"_h,
        "ld1_asisdlsep_i2_i2"_h,
        "ld1_asisdlsep_r2_r2"_h,
        "st1_asisdlse_r2_2v"_h,
        "st1_asisdlsep_i2_i2"_h,
        "st1_asisdlsep_r2_r2"_h}},
      {"{'Vt.'[nload], 'Vt2.'[nload]}, ['Xns]'(23?, 'Xmz2)",
       {"ld2r_asisdlsop_r2_i"_h,
        "ld2r_asisdlsop_rx2_r"_h,
        "ld2r_asisdlso_r2"_h}},
      {"{'Vt.'[nload], 'Vt2.'[nload], 'Vt3.'[nload]}, ['Xns]'(23?, 'Xmr3)",
       {"ld3_asisdlse_r3"_h,
        "ld3_asisdlsep_i3_i"_h,
        "ld3_asisdlsep_r3_r"_h,
        "st3_asisdlse_r3"_h,
        "st3_asisdlsep_i3_i"_h,
        "st3_asisdlsep_r3_r"_h,
        "ld1_asisdlse_r3_3v"_h,
        "ld1_asisdlsep_i3_i3"_h,
        "ld1_asisdlsep_r3_r3"_h,
        "st1_asisdlse_r3_3v"_h,
        "st1_asisdlsep_i3_i3"_h,
        "st1_asisdlsep_r3_r3"_h}},
      {"{'Vt.'[nload], 'Vt2.'[nload], 'Vt3.'[nload]}, ['Xns]'(23?, 'Xmz3)",
       {"ld3r_asisdlsop_r3_i"_h,
        "ld3r_asisdlsop_rx3_r"_h,
        "ld3r_asisdlso_r3"_h}},
      {"{'Vt.'[nload], 'Vt2.'[nload], 'Vt3.'[nload], 'Vt4.'[nload]}, "
       "['Xns]'(23?, 'Xmr4)",
       {"ld4_asisdlse_r4"_h,
        "ld4_asisdlsep_i4_i"_h,
        "ld4_asisdlsep_r4_r"_h,
        "st4_asisdlse_r4"_h,
        "st4_asisdlsep_i4_i"_h,
        "st4_asisdlsep_r4_r"_h,
        "ld1_asisdlse_r4_4v"_h,
        "ld1_asisdlsep_i4_i4"_h,
        "ld1_asisdlsep_r4_r4"_h,
        "st1_asisdlse_r4_4v"_h,
        "st1_asisdlsep_i4_i4"_h,
        "st1_asisdlsep_r4_r4"_h}},
      {"{'Vt.'[nload], 'Vt2.'[nload], 'Vt3.'[nload], 'Vt4.'[nload]}, "
       "['Xns]'(23?, 'Xmz4)",
       {"ld4r_asisdlsop_r4_i"_h,
        "ld4r_asisdlsop_rx4_r"_h,
        "ld4r_asisdlso_r4"_h}},
      {"{'Vt.b, 'Vt2.b, 'Vt3.b, 'Vt4.b}['u30_1210], ['Xns]'(23?, 'Xmb4)",
       {"ld4_asisdlsop_b4_i4b"_h,
        "ld4_asisdlsop_bx4_r4b"_h,
        "st4_asisdlsop_b4_i4b"_h,
        "st4_asisdlsop_bx4_r4b"_h,
        "ld4_asisdlso_b4_4b"_h,
        "st4_asisdlso_b4_4b"_h}},
      {"{'Vt.b, 'Vt2.b, 'Vt3.b}['u30_1210], ['Xns]'(23?, 'Xmb3)",
       {"ld3_asisdlsop_b3_i3b"_h,
        "ld3_asisdlsop_bx3_r3b"_h,
        "st3_asisdlsop_b3_i3b"_h,
        "st3_asisdlsop_bx3_r3b"_h,
        "ld3_asisdlso_b3_3b"_h,
        "st3_asisdlso_b3_3b"_h}},
      {"{'Vt.b, 'Vt2.b}['u30_1210], ['Xns]'(23?, 'Xmb2)",
       {"ld2_asisdlsop_b2_i2b"_h,
        "ld2_asisdlsop_bx2_r2b"_h,
        "st2_asisdlsop_b2_i2b"_h,
        "st2_asisdlsop_bx2_r2b"_h,
        "ld2_asisdlso_b2_2b"_h,
        "st2_asisdlso_b2_2b"_h}},
      {"{'Vt.b}['u30_1210], ['Xns]'(23?, 'Xmb1)",
       {"ld1_asisdlsop_b1_i1b"_h,
        "ld1_asisdlsop_bx1_r1b"_h,
        "st1_asisdlsop_b1_i1b"_h,
        "st1_asisdlsop_bx1_r1b"_h,
        "ld1_asisdlso_b1_1b"_h,
        "st1_asisdlso_b1_1b"_h}},
      {"{'Vt.d, 'Vt2.d, 'Vt3.d, 'Vt4.d}['u30], ['Xns]'(23?, 'Xmb32)",
       {"ld4_asisdlsop_d4_i4d"_h,
        "ld4_asisdlsop_dx4_r4d"_h,
        "st4_asisdlsop_d4_i4d"_h,
        "st4_asisdlsop_dx4_r4d"_h,
        "ld4_asisdlso_d4_4d"_h,
        "st4_asisdlso_d4_4d"_h}},
      {"{'Vt.d, 'Vt2.d, 'Vt3.d}['u30], ['Xns]'(23?, 'Xmb24)",
       {"ld3_asisdlsop_d3_i3d"_h,
        "ld3_asisdlsop_dx3_r3d"_h,
        "st3_asisdlsop_d3_i3d"_h,
        "st3_asisdlsop_dx3_r3d"_h,
        "ld3_asisdlso_d3_3d"_h,
        "st3_asisdlso_d3_3d"_h}},
      {"{'Vt.d, 'Vt2.d}['u30], ['Xns]'(23?, 'Xmb16)",
       {"ld2_asisdlsop_d2_i2d"_h,
        "ld2_asisdlsop_dx2_r2d"_h,
        "st2_asisdlsop_d2_i2d"_h,
        "st2_asisdlsop_dx2_r2d"_h,
        "ld2_asisdlso_d2_2d"_h,
        "st2_asisdlso_d2_2d"_h}},
      {"{'Vt.d}['u30], ['Xns]'(23?, 'Xmb8)",
       {"ld1_asisdlsop_d1_i1d"_h,
        "ld1_asisdlsop_dx1_r1d"_h,
        "st1_asisdlsop_d1_i1d"_h,
        "st1_asisdlsop_dx1_r1d"_h,
        "ld1_asisdlso_d1_1d"_h,
        "st1_asisdlso_d1_1d"_h}},
      {"{'Vt.h, 'Vt2.h, 'Vt3.h, 'Vt4.h}['u30_1211], ['Xns]'(23?, 'Xmb8)",
       {"ld4_asisdlso_h4_4h"_h,
        "ld4_asisdlsop_h4_i4h"_h,
        "ld4_asisdlsop_hx4_r4h"_h,
        "st4_asisdlso_h4_4h"_h,
        "st4_asisdlsop_h4_i4h"_h,
        "st4_asisdlsop_hx4_r4h"_h}},
      {"{'Vt.h, 'Vt2.h, 'Vt3.h}['u30_1211], ['Xns]'(23?, 'Xmb6)",
       {"ld3_asisdlso_h3_3h"_h,
        "ld3_asisdlsop_h3_i3h"_h,
        "ld3_asisdlsop_hx3_r3h"_h,
        "st3_asisdlso_h3_3h"_h,
        "st3_asisdlsop_h3_i3h"_h,
        "st3_asisdlsop_hx3_r3h"_h}},
      {"{'Vt.h, 'Vt2.h}['u30_1211], ['Xns]'(23?, 'Xmb4)",
       {"ld2_asisdlso_h2_2h"_h,
        "ld2_asisdlsop_h2_i2h"_h,
        "ld2_asisdlsop_hx2_r2h"_h,
        "st2_asisdlso_h2_2h"_h,
        "st2_asisdlsop_h2_i2h"_h,
        "st2_asisdlsop_hx2_r2h"_h}},
      {"{'Vt.h}['u30_1211], ['Xns]'(23?, 'Xmb2)",
       {"ld1_asisdlso_h1_1h"_h,
        "ld1_asisdlsop_h1_i1h"_h,
        "ld1_asisdlsop_hx1_r1h"_h,
        "st1_asisdlso_h1_1h"_h,
        "st1_asisdlsop_h1_i1h"_h,
        "st1_asisdlsop_hx1_r1h"_h}},
      {"{'Vt.s, 'Vt2.s, 'Vt3.s, 'Vt4.s}['u30_12], ['Xns]'(23?, 'Xmb16)",
       {"ld4_asisdlsop_s4_i4s"_h,
        "ld4_asisdlsop_sx4_r4s"_h,
        "st4_asisdlsop_s4_i4s"_h,
        "st4_asisdlsop_sx4_r4s"_h,
        "ld4_asisdlso_s4_4s"_h,
        "st4_asisdlso_s4_4s"_h}},
      {"{'Vt.s, 'Vt2.s, 'Vt3.s}['u30_12], ['Xns]'(23?, 'Xmb12)",
       {"ld3_asisdlsop_s3_i3s"_h,
        "ld3_asisdlsop_sx3_r3s"_h,
        "st3_asisdlsop_s3_i3s"_h,
        "st3_asisdlsop_sx3_r3s"_h,
        "ld3_asisdlso_s3_3s"_h,
        "st3_asisdlso_s3_3s"_h}},
      {"{'Vt.s, 'Vt2.s}['u30_12], ['Xns]'(23?, 'Xmb8)",
       {"ld2_asisdlsop_s2_i2s"_h,
        "ld2_asisdlsop_sx2_r2s"_h,
        "st2_asisdlsop_s2_i2s"_h,
        "st2_asisdlsop_sx2_r2s"_h,
        "ld2_asisdlso_s2_2s"_h,
        "st2_asisdlso_s2_2s"_h}},
      {"{'Vt.s}['u30_12], ['Xns]'(23?, 'Xmb4)",
       {"ld1_asisdlsop_s1_i1s"_h,
        "ld1_asisdlsop_sx1_r1s"_h,
        "st1_asisdlsop_s1_i1s"_h,
        "st1_asisdlsop_sx1_r1s"_h,
        "ld1_asisdlso_s1_1s"_h,
        "st1_asisdlso_s1_1s"_h}},
      {"{'Zt.'[sszld]}, 'Pgl/z, ['Xns'(1916?, #'s1916, mul vl)]",
       {"ld1b_z_p_bi_u16"_h,    "ld1b_z_p_bi_u32"_h,    "ld1b_z_p_bi_u64"_h,
        "ld1b_z_p_bi_u8"_h,     "ld1d_z_p_bi_u64"_h,    "ld1h_z_p_bi_u16"_h,
        "ld1h_z_p_bi_u32"_h,    "ld1h_z_p_bi_u64"_h,    "ld1sb_z_p_bi_s16"_h,
        "ld1sb_z_p_bi_s32"_h,   "ld1sb_z_p_bi_s64"_h,   "ld1sh_z_p_bi_s32"_h,
        "ld1sh_z_p_bi_s64"_h,   "ld1sw_z_p_bi_s64"_h,   "ld1w_z_p_bi_u32"_h,
        "ld1w_z_p_bi_u64"_h,    "ldnf1b_z_p_bi_u16"_h,  "ldnf1b_z_p_bi_u32"_h,
        "ldnf1b_z_p_bi_u64"_h,  "ldnf1b_z_p_bi_u8"_h,   "ldnf1d_z_p_bi_u64"_h,
        "ldnf1h_z_p_bi_u16"_h,  "ldnf1h_z_p_bi_u32"_h,  "ldnf1h_z_p_bi_u64"_h,
        "ldnf1sb_z_p_bi_s16"_h, "ldnf1sb_z_p_bi_s32"_h, "ldnf1sb_z_p_bi_s64"_h,
        "ldnf1sh_z_p_bi_s32"_h, "ldnf1sh_z_p_bi_s64"_h, "ldnf1sw_z_p_bi_s64"_h,
        "ldnf1w_z_p_bi_u32"_h,  "ldnf1w_z_p_bi_u64"_h}},
      {"{'Zt.'[sszld]}, 'Pgl/z, ['Xns'(2016=31?:, 'Xm)]",
       {"ldff1b_z_p_br_u16"_h,
        "ldff1b_z_p_br_u32"_h,
        "ldff1b_z_p_br_u64"_h,
        "ldff1b_z_p_br_u8"_h,
        "ldff1sb_z_p_br_s16"_h,
        "ldff1sb_z_p_br_s32"_h,
        "ldff1sb_z_p_br_s64"_h}},
      {"{'Zt.'[sszld]}, 'Pgl/z, ['Xns'(2016=31?:, 'Xm, lsl #1)]",
       {"ldff1h_z_p_br_u16"_h,
        "ldff1h_z_p_br_u32"_h,
        "ldff1h_z_p_br_u64"_h,
        "ldff1sh_z_p_br_s32"_h,
        "ldff1sh_z_p_br_s64"_h}},
      {"{'Zt.'[sszld]}, 'Pgl/z, ['Xns'(2016=31?:, 'Xm, lsl #2)]",
       {"ldff1w_z_p_br_u32"_h, "ldff1w_z_p_br_u64"_h, "ldff1sw_z_p_br_s64"_h}},
      {"{'Zt.'[sszld]}, 'Pgl/z, ['Xns'(2016=31?:, 'Xm, lsl #3)]",
       {"ldff1d_z_p_br_u64"_h}},
      {"{'Zt.'[sszld]}, 'Pgl/z, ['Xns, 'Xm, lsl #'u2423]",
       {"ld1d_z_p_br_u64"_h,
        "ld1h_z_p_br_u16"_h,
        "ld1h_z_p_br_u32"_h,
        "ld1h_z_p_br_u64"_h,
        "ld1w_z_p_br_u32"_h,
        "ld1w_z_p_br_u64"_h}},
      {"{'Zt.'[sszld]}, 'Pgl/z, ['Xns, 'Xm, lsl #1]",
       {"ld1sh_z_p_br_s32"_h, "ld1sh_z_p_br_s64"_h}},
      {"{'Zt.'[sszld]}, 'Pgl/z, ['Xns, 'Xm, lsl #2]", {"ld1sw_z_p_br_s64"_h}},
      {"{'Zt.'[sszld]}, 'Pgl/z, ['Xns, 'Xm]",
       {"ld1b_z_p_br_u16"_h,
        "ld1b_z_p_br_u32"_h,
        "ld1b_z_p_br_u64"_h,
        "ld1b_z_p_br_u8"_h,
        "ld1sb_z_p_br_s16"_h,
        "ld1sb_z_p_br_s32"_h,
        "ld1sb_z_p_br_s64"_h}},
      {"{'Zt.'[sszst]}, 'Pgl, ['Xns'(1916?, #'s1916, mul vl)]",
       {"st1b_z_p_bi"_h, "st1d_z_p_bi"_h, "st1h_z_p_bi"_h, "st1w_z_p_bi"_h}},
      {"{'Zt.'[sszst]}, 'Pgl, ['Xns, 'Xm'(2423?, lsl #'u2423)]",
       {"st1b_z_p_br"_h, "st1d_z_p_br"_h, "st1h_z_p_br"_h, "st1w_z_p_br"_h}},
      {"{'Zt.'[sszmem], 'Zt2.'[sszmem], 'Zt3.'[sszmem], 'Zt4.'[sszmem]}, "
       "'Pgl'(30?:/z), ['Xns'(1916?, #'<s1916 4 *>, mul vl)]",
       {"st4b_z_p_bi_contiguous"_h,
        "st4d_z_p_bi_contiguous"_h,
        "st4h_z_p_bi_contiguous"_h,
        "st4w_z_p_bi_contiguous"_h,
        "ld4b_z_p_bi_contiguous"_h,
        "ld4d_z_p_bi_contiguous"_h,
        "ld4h_z_p_bi_contiguous"_h,
        "ld4w_z_p_bi_contiguous"_h}},
      {"{'Zt.'[sszmem], 'Zt2.'[sszmem], 'Zt3.'[sszmem], 'Zt4.'[sszmem]}, "
       "'Pgl'(30?:/z), ['Xns, 'Xm'(2423?, lsl #'u2423)]",
       {"st4b_z_p_br_contiguous"_h,
        "st4d_z_p_br_contiguous"_h,
        "st4h_z_p_br_contiguous"_h,
        "st4w_z_p_br_contiguous"_h,
        "ld4b_z_p_br_contiguous"_h,
        "ld4d_z_p_br_contiguous"_h,
        "ld4h_z_p_br_contiguous"_h,
        "ld4w_z_p_br_contiguous"_h}},
      {"{'Zt.'[sszmem], 'Zt2.'[sszmem], 'Zt3.'[sszmem]}, 'Pgl'(30?:/z), "
       "['Xns'(1916?, #'<s1916 3 *>, mul vl)]",
       {"st3b_z_p_bi_contiguous"_h,
        "st3d_z_p_bi_contiguous"_h,
        "st3h_z_p_bi_contiguous"_h,
        "st3w_z_p_bi_contiguous"_h,
        "ld3b_z_p_bi_contiguous"_h,
        "ld3d_z_p_bi_contiguous"_h,
        "ld3h_z_p_bi_contiguous"_h,
        "ld3w_z_p_bi_contiguous"_h}},
      {"{'Zt.'[sszmem], 'Zt2.'[sszmem], 'Zt3.'[sszmem]}, 'Pgl'(30?:/z), ['Xns, "
       "'Xm'(2423?, lsl #'u2423)]",
       {"st3b_z_p_br_contiguous"_h,
        "st3d_z_p_br_contiguous"_h,
        "st3h_z_p_br_contiguous"_h,
        "st3w_z_p_br_contiguous"_h,
        "ld3b_z_p_br_contiguous"_h,
        "ld3d_z_p_br_contiguous"_h,
        "ld3h_z_p_br_contiguous"_h,
        "ld3w_z_p_br_contiguous"_h}},
      {"{'Zt.'[sszmem], 'Zt2.'[sszmem]}, 'Pgl'(30?:/z), ['Xns'(1916?, "
       "#'<s1916 2 *>, mul vl)]",
       {"st2b_z_p_bi_contiguous"_h,
        "st2d_z_p_bi_contiguous"_h,
        "st2h_z_p_bi_contiguous"_h,
        "st2w_z_p_bi_contiguous"_h,
        "ld2b_z_p_bi_contiguous"_h,
        "ld2d_z_p_bi_contiguous"_h,
        "ld2h_z_p_bi_contiguous"_h,
        "ld2w_z_p_bi_contiguous"_h}},
      {"{'Zt.'[sszmem], 'Zt2.'[sszmem]}, 'Pgl'(30?:/z), ['Xns, 'Xm'(2423?, lsl "
       "#'u2423)]",
       {"st2b_z_p_br_contiguous"_h,
        "st2d_z_p_br_contiguous"_h,
        "st2h_z_p_br_contiguous"_h,
        "st2w_z_p_br_contiguous"_h,
        "ld2b_z_p_br_contiguous"_h,
        "ld2d_z_p_br_contiguous"_h,
        "ld2h_z_p_br_contiguous"_h,
        "ld2w_z_p_br_contiguous"_h}},
      {"{'Zt.'[sszmem]}, 'Pgl/z, ['Xns'(1916?, #'<s1916 16 *>)]",
       {"ld1rqb_z_p_bi_u8"_h,
        "ld1rqd_z_p_bi_u64"_h,
        "ld1rqh_z_p_bi_u16"_h,
        "ld1rqw_z_p_bi_u32"_h}},
      {"{'Zt.'[sszmem]}, 'Pgl/z, ['Xns'(1916?, #'<s1916 32 *>)]",
       {"ld1rob_z_p_bi_u8"_h,
        "ld1rod_z_p_bi_u64"_h,
        "ld1roh_z_p_bi_u16"_h,
        "ld1row_z_p_bi_u32"_h}},
      {"{'Zt.'[sszmem]}, 'Pgl/z, ['Xns, 'Rm, lsl #'u2423]",
       {"ld1rqd_z_p_br_contiguous"_h,
        "ld1rqh_z_p_br_contiguous"_h,
        "ld1rqw_z_p_br_contiguous"_h,
        "ld1rod_z_p_br_contiguous"_h,
        "ld1roh_z_p_br_contiguous"_h,
        "ld1row_z_p_br_contiguous"_h}},
      {"{'Zt.'[sszmem]}, 'Pgl/z, ['Xns, 'Rm]",
       {"ld1rqb_z_p_br_contiguous"_h, "ld1rob_z_p_br_contiguous"_h}},
      {"{'Zt.b}, 'Pgl, ['Xns, 'Rm]", {"stnt1b_z_p_br_contiguous"_h}},
      {"{'Zt.b}, 'Pgl/z, ['Xns, 'Rm]", {"ldnt1b_z_p_br_contiguous"_h}},
      {"{'Zt.b}, 'Pgl/z, ['Xns'(2116?, #'u2116)]", {"ld1rb_z_p_bi_u8"_h}},
      {"{'Zt.b}, 'Pgl'(20?:/z), ['Xns'(1916?, #'s1916, mul vl)]",
       {"ldnt1b_z_p_bi_contiguous"_h, "stnt1b_z_p_bi_contiguous"_h}},
      {"{'Zt.d}, 'Pgl'(20?:/z), ['Xns'(1916?, #'s1916, mul vl)]",
       {"ldnt1d_z_p_bi_contiguous"_h, "stnt1d_z_p_bi_contiguous"_h}},
      {"{'Zt.d}, 'Pgl'(29?:/z), ['Zn.d'(2016=31?:, 'Xm)]",
       {"stnt1b_z_p_ar_d_64_unscaled"_h,
        "stnt1d_z_p_ar_d_64_unscaled"_h,
        "stnt1h_z_p_ar_d_64_unscaled"_h,
        "stnt1w_z_p_ar_d_64_unscaled"_h,
        "ldnt1b_z_p_ar_d_64_unscaled"_h,
        "ldnt1d_z_p_ar_d_64_unscaled"_h,
        "ldnt1h_z_p_ar_d_64_unscaled"_h,
        "ldnt1sb_z_p_ar_d_64_unscaled"_h,
        "ldnt1sh_z_p_ar_d_64_unscaled"_h,
        "ldnt1sw_z_p_ar_d_64_unscaled"_h,
        "ldnt1w_z_p_ar_d_64_unscaled"_h}},
      {"{'Zt.d}, 'Pgl, ['Xns, 'Rm, lsl #3]", {"stnt1d_z_p_br_contiguous"_h}},
      {"{'Zt.d}, 'Pgl, ['Xns, 'Zm.d, '?14:suxtw #'u2423]",
       {"st1d_z_p_bz_d_x32_scaled"_h,
        "st1h_z_p_bz_d_x32_scaled"_h,
        "st1w_z_p_bz_d_x32_scaled"_h}},
      {"{'Zt.d}, 'Pgl, ['Xns, 'Zm.d, '?14:suxtw]",
       {"st1b_z_p_bz_d_x32_unscaled"_h,
        "st1d_z_p_bz_d_x32_unscaled"_h,
        "st1h_z_p_bz_d_x32_unscaled"_h,
        "st1w_z_p_bz_d_x32_unscaled"_h}},
      {"{'Zt.d}, 'Pgl, ['Xns, 'Zm.d, lsl #'u2423]",
       {"st1d_z_p_bz_d_64_scaled"_h,
        "st1h_z_p_bz_d_64_scaled"_h,
        "st1w_z_p_bz_d_64_scaled"_h}},
      {"{'Zt.d}, 'Pgl, ['Xns, 'Zm.d]",
       {"st1b_z_p_bz_d_64_unscaled"_h,
        "st1d_z_p_bz_d_64_unscaled"_h,
        "st1h_z_p_bz_d_64_unscaled"_h,
        "st1w_z_p_bz_d_64_unscaled"_h}},
      {"{'Zt.d}, 'Pgl, ['Zn.d'(2016?, #'u2016)]", {"st1b_z_p_ai_d"_h}},
      {"{'Zt.d}, 'Pgl, ['Zn.d'(2016?, #'<u2016 2 *>)]", {"st1h_z_p_ai_d"_h}},
      {"{'Zt.d}, 'Pgl, ['Zn.d'(2016?, #'<u2016 4 *>)]", {"st1w_z_p_ai_d"_h}},
      {"{'Zt.d}, 'Pgl, ['Zn.d'(2016?, #'<u2016 8 *>)]", {"st1d_z_p_ai_d"_h}},
      {"{'Zt.d}, 'Pgl/z, ['Xns'(2116?, #'u2116)]",
       {"ld1rb_z_p_bi_u64"_h, "ld1rsb_z_p_bi_s64"_h}},
      {"{'Zt.d}, 'Pgl/z, ['Xns'(2116?, #'<u2116 2 *>)]",
       {"ld1rh_z_p_bi_u64"_h, "ld1rsh_z_p_bi_s64"_h}},
      {"{'Zt.d}, 'Pgl/z, ['Xns'(2116?, #'<u2116 4 *>)]",
       {"ld1rw_z_p_bi_u64"_h, "ld1rsw_z_p_bi_s64"_h}},
      {"{'Zt.d}, 'Pgl/z, ['Xns'(2116?, #'<u2116 8 *>)]",
       {"ld1rd_z_p_bi_u64"_h}},
      {"{'Zt.d}, 'Pgl/z, ['Xns, 'Rm, lsl #3]", {"ldnt1d_z_p_br_contiguous"_h}},
      {"{'Zt.d}, 'Pgl/z, ['Xns, 'Zm.d, '?22:suxtw #'u2423]",
       {"ld1d_z_p_bz_d_x32_scaled"_h,
        "ld1h_z_p_bz_d_x32_scaled"_h,
        "ld1sh_z_p_bz_d_x32_scaled"_h,
        "ld1sw_z_p_bz_d_x32_scaled"_h,
        "ld1w_z_p_bz_d_x32_scaled"_h,
        "ldff1d_z_p_bz_d_x32_scaled"_h,
        "ldff1h_z_p_bz_d_x32_scaled"_h,
        "ldff1sh_z_p_bz_d_x32_scaled"_h,
        "ldff1sw_z_p_bz_d_x32_scaled"_h,
        "ldff1w_z_p_bz_d_x32_scaled"_h}},
      {"{'Zt.d}, 'Pgl/z, ['Xns, 'Zm.d, '?22:suxtw]",
       {"ld1b_z_p_bz_d_x32_unscaled"_h,
        "ld1d_z_p_bz_d_x32_unscaled"_h,
        "ld1h_z_p_bz_d_x32_unscaled"_h,
        "ld1sb_z_p_bz_d_x32_unscaled"_h,
        "ld1sh_z_p_bz_d_x32_unscaled"_h,
        "ld1sw_z_p_bz_d_x32_unscaled"_h,
        "ld1w_z_p_bz_d_x32_unscaled"_h,
        "ldff1b_z_p_bz_d_x32_unscaled"_h,
        "ldff1d_z_p_bz_d_x32_unscaled"_h,
        "ldff1h_z_p_bz_d_x32_unscaled"_h,
        "ldff1sb_z_p_bz_d_x32_unscaled"_h,
        "ldff1sh_z_p_bz_d_x32_unscaled"_h,
        "ldff1sw_z_p_bz_d_x32_unscaled"_h,
        "ldff1w_z_p_bz_d_x32_unscaled"_h}},
      {"{'Zt.d}, 'Pgl/z, ['Xns, 'Zm.d, lsl #'u2423]",
       {"ld1d_z_p_bz_d_64_scaled"_h,
        "ld1h_z_p_bz_d_64_scaled"_h,
        "ld1sh_z_p_bz_d_64_scaled"_h,
        "ld1sw_z_p_bz_d_64_scaled"_h,
        "ld1w_z_p_bz_d_64_scaled"_h,
        "ldff1d_z_p_bz_d_64_scaled"_h,
        "ldff1h_z_p_bz_d_64_scaled"_h,
        "ldff1sh_z_p_bz_d_64_scaled"_h,
        "ldff1sw_z_p_bz_d_64_scaled"_h,
        "ldff1w_z_p_bz_d_64_scaled"_h}},
      {"{'Zt.d}, 'Pgl/z, ['Xns, 'Zm.d]",
       {"ld1b_z_p_bz_d_64_unscaled"_h,
        "ld1d_z_p_bz_d_64_unscaled"_h,
        "ld1h_z_p_bz_d_64_unscaled"_h,
        "ld1sb_z_p_bz_d_64_unscaled"_h,
        "ld1sh_z_p_bz_d_64_unscaled"_h,
        "ld1sw_z_p_bz_d_64_unscaled"_h,
        "ld1w_z_p_bz_d_64_unscaled"_h,
        "ldff1b_z_p_bz_d_64_unscaled"_h,
        "ldff1d_z_p_bz_d_64_unscaled"_h,
        "ldff1h_z_p_bz_d_64_unscaled"_h,
        "ldff1sb_z_p_bz_d_64_unscaled"_h,
        "ldff1sh_z_p_bz_d_64_unscaled"_h,
        "ldff1sw_z_p_bz_d_64_unscaled"_h,
        "ldff1w_z_p_bz_d_64_unscaled"_h}},
      {"{'Zt.d}, 'Pgl/z, ['Zn.d'(2016?, #'u2016)]",
       {"ld1b_z_p_ai_d"_h,
        "ld1sb_z_p_ai_d"_h,
        "ldff1b_z_p_ai_d"_h,
        "ldff1sb_z_p_ai_d"_h}},
      {"{'Zt.d}, 'Pgl/z, ['Zn.d'(2016?, #'<u2016 2 *>)]",
       {"ld1h_z_p_ai_d"_h,
        "ld1sh_z_p_ai_d"_h,
        "ldff1h_z_p_ai_d"_h,
        "ldff1sh_z_p_ai_d"_h}},
      {"{'Zt.d}, 'Pgl/z, ['Zn.d'(2016?, #'<u2016 4 *>)]",
       {"ld1sw_z_p_ai_d"_h,
        "ld1w_z_p_ai_d"_h,
        "ldff1sw_z_p_ai_d"_h,
        "ldff1w_z_p_ai_d"_h}},
      {"{'Zt.d}, 'Pgl/z, ['Zn.d'(2016?, #'<u2016 8 *>)]",
       {"ld1d_z_p_ai_d"_h, "ldff1d_z_p_ai_d"_h}},
      {"{'Zt.h}, 'Pgl'(30?:/z), ['Xns, 'Rm, lsl #1]",
       {"ldnt1h_z_p_br_contiguous"_h, "stnt1h_z_p_br_contiguous"_h}},
      {"{'Zt.h}, 'Pgl'(20?:/z), ['Xns'(1916?, #'s1916, mul vl)]",
       {"ldnt1h_z_p_bi_contiguous"_h, "stnt1h_z_p_bi_contiguous"_h}},
      {"{'Zt.h}, 'Pgl/z, ['Xns'(2116?, #'<u2116 2 *>)]",
       {"ld1rb_z_p_bi_u16"_h, "ld1rsb_z_p_bi_s16"_h}},
      {"{'Zt.h}, 'Pgl/z, ['Xns'(2116?, #'<u2116 4 *>)]",
       {"ld1rh_z_p_bi_u16"_h}},
      {"{'Zt.s}, 'Pgl'(29?:/z), ['Zn.s'(2016=31?:, 'Xm)]",
       {"stnt1b_z_p_ar_s_x32_unscaled"_h,
        "stnt1h_z_p_ar_s_x32_unscaled"_h,
        "stnt1w_z_p_ar_s_x32_unscaled"_h,
        "ldnt1b_z_p_ar_s_x32_unscaled"_h,
        "ldnt1h_z_p_ar_s_x32_unscaled"_h,
        "ldnt1sb_z_p_ar_s_x32_unscaled"_h,
        "ldnt1sh_z_p_ar_s_x32_unscaled"_h,
        "ldnt1w_z_p_ar_s_x32_unscaled"_h}},
      {"{'Zt.s}, 'Pgl, ['Xns, 'Rm, lsl #2]", {"stnt1w_z_p_br_contiguous"_h}},
      {"{'Zt.s}, 'Pgl, ['Xns, 'Zm.s, '?14:suxtw #'u2423]",
       {"st1h_z_p_bz_s_x32_scaled"_h, "st1w_z_p_bz_s_x32_scaled"_h}},
      {"{'Zt.s}, 'Pgl, ['Xns, 'Zm.s, '?14:suxtw]",
       {"st1b_z_p_bz_s_x32_unscaled"_h,
        "st1h_z_p_bz_s_x32_unscaled"_h,
        "st1w_z_p_bz_s_x32_unscaled"_h}},
      {"{'Zt.s}, 'Pgl/z, ['Xns'(2116?, #'u2116)]",
       {"ld1rb_z_p_bi_u32"_h, "ld1rsb_z_p_bi_s32"_h}},
      {"{'Zt.s}, 'Pgl/z, ['Xns'(2116?, #'<u2116 2 *>)]",
       {"ld1rh_z_p_bi_u32"_h, "ld1rsh_z_p_bi_s32"_h}},
      {"{'Zt.s}, 'Pgl/z, ['Xns'(2116?, #'<u2116 4 *>)]",
       {"ld1rw_z_p_bi_u32"_h, "ld1rsw_z_p_bi_s32"_h}},
      {"{'Zt.s}, 'Pgl'(20?:/z), ['Xns'(1916?, #'s1916, mul vl)]",
       {"ldnt1w_z_p_bi_contiguous"_h, "stnt1w_z_p_bi_contiguous"_h}},
      {"{'Zt.s}, 'Pgl/z, ['Xns, 'Rm, lsl #2]", {"ldnt1w_z_p_br_contiguous"_h}},
      {"{'Zt.s}, 'Pgl/z, ['Xns, 'Zm.s, '?22:suxtw #1]",
       {"ld1h_z_p_bz_s_x32_scaled"_h,
        "ld1sh_z_p_bz_s_x32_scaled"_h,
        "ldff1h_z_p_bz_s_x32_scaled"_h,
        "ldff1sh_z_p_bz_s_x32_scaled"_h}},
      {"{'Zt.s}, 'Pgl/z, ['Xns, 'Zm.s, '?22:suxtw #2]",
       {"ld1w_z_p_bz_s_x32_scaled"_h, "ldff1w_z_p_bz_s_x32_scaled"_h}},
      {"{'Zt.s}, 'Pgl/z, ['Xns, 'Zm.s, '?22:suxtw]",
       {"ld1b_z_p_bz_s_x32_unscaled"_h,
        "ld1h_z_p_bz_s_x32_unscaled"_h,
        "ld1sb_z_p_bz_s_x32_unscaled"_h,
        "ld1sh_z_p_bz_s_x32_unscaled"_h,
        "ld1w_z_p_bz_s_x32_unscaled"_h,
        "ldff1b_z_p_bz_s_x32_unscaled"_h,
        "ldff1h_z_p_bz_s_x32_unscaled"_h,
        "ldff1sb_z_p_bz_s_x32_unscaled"_h,
        "ldff1sh_z_p_bz_s_x32_unscaled"_h,
        "ldff1w_z_p_bz_s_x32_unscaled"_h}},
      {"{'Zt.s}, 'Pgl/z, ['Zn.s'(2016?, #'u2016)]",
       {"ld1b_z_p_ai_s"_h,
        "ld1sb_z_p_ai_s"_h,
        "ldff1b_z_p_ai_s"_h,
        "ldff1sb_z_p_ai_s"_h}},
      {"{'Zt.s}, 'Pgl, ['Zn.s'(2016?, #'u2016)]", {"st1b_z_p_ai_s"_h}},
      {"{'Zt.s}, 'Pgl/z, ['Zn.s'(2016?, #'<u2016 2 *>)]",
       {"ld1h_z_p_ai_s"_h,
        "ld1sh_z_p_ai_s"_h,
        "ldff1h_z_p_ai_s"_h,
        "ldff1sh_z_p_ai_s"_h}},
      {"{'Zt.s}, 'Pgl, ['Zn.s'(2016?, #'<u2016 2 *>)]", {"st1h_z_p_ai_s"_h}},
      {"{'Zt.s}, 'Pgl/z, ['Zn.s'(2016?, #'<u2016 4 *>)]",
       {"ld1w_z_p_ai_s"_h, "ldff1w_z_p_ai_s"_h}},
      {"{'Zt.s}, 'Pgl, ['Zn.s'(2016?, #'<u2016 4 *>)]", {"st1w_z_p_ai_s"_h}},
      {"'Zd.'[sz], 'Pgl/m, 'Zd.'[sz], 'Zn.d",
       {"asr_z_p_zw"_h, "lsl_z_p_zw"_h, "lsr_z_p_zw"_h}},
      {"'Zd.'[sz], 'Zn.'[sz], 'Zm.'[sszh]",
       {"saddwb_z_zz"_h,
        "saddwt_z_zz"_h,
        "ssubwb_z_zz"_h,
        "ssubwt_z_zz"_h,
        "uaddwb_z_zz"_h,
        "uaddwt_z_zz"_h,
        "usubwb_z_zz"_h,
        "usubwt_z_zz"_h}},
      {"'Zd.'[sz], 'Zn.'[sszh]",
       {"sunpkhi_z_z"_h, "sunpklo_z_z"_h, "uunpkhi_z_z"_h, "uunpklo_z_z"_h}},
      {"'Zd.'[sz], 'Zn.'[sszh], 'Zm.'[sszh]",
       {"smlalb_z_zzz"_h,   "smlalt_z_zzz"_h,   "smlslb_z_zzz"_h,
        "smlslt_z_zzz"_h,   "sqdmlalb_z_zzz"_h, "sqdmlalbt_z_zzz"_h,
        "sqdmlalt_z_zzz"_h, "sqdmlslb_z_zzz"_h, "sqdmlslbt_z_zzz"_h,
        "sqdmlslt_z_zzz"_h, "umlalb_z_zzz"_h,   "umlalt_z_zzz"_h,
        "umlslb_z_zzz"_h,   "umlslt_z_zzz"_h,   "sabalb_z_zzz"_h,
        "sabalt_z_zzz"_h,   "sabdlb_z_zz"_h,    "sabdlt_z_zz"_h,
        "saddlb_z_zz"_h,    "saddlbt_z_zz"_h,   "saddlt_z_zz"_h,
        "smullb_z_zz"_h,    "smullt_z_zz"_h,    "sqdmullb_z_zz"_h,
        "sqdmullt_z_zz"_h,  "ssublb_z_zz"_h,    "ssublbt_z_zz"_h,
        "ssublt_z_zz"_h,    "ssubltb_z_zz"_h,   "uabalb_z_zzz"_h,
        "uabalt_z_zzz"_h,   "uabdlb_z_zz"_h,    "uabdlt_z_zz"_h,
        "uaddlb_z_zz"_h,    "uaddlt_z_zz"_h,    "umullb_z_zz"_h,
        "umullt_z_zz"_h,    "usublb_z_zz"_h,    "usublt_z_zz"_h,
        "pmullb_z_zz"_h,    "pmullt_z_zz"_h}},
      {"'Zt, ['Xns'(2110=16?:, #'s2116_1210, mul vl)]",
       {"ldr_z_bi"_h, "str_z_bi"_h}},
      {"ivau, 'Xt", {"ic_sys_cr_systeminstrs"_h}},
      {"'{dcop}, 'Xt", {"dc_sys_cr_systeminstrs"_h}},
      {"'{pstatefield}, #'u1108", {"msr_si_pstate"_h}},
      {"csync", {"psb_c_hints"_h, "tsb_hc_hints"_h}},
      {"x16", {"chkfeat_hf_hints"_h}}};

  for (auto &itm : forms) {
    const std::unordered_set<uint32_t> &s = forms.at(itm.first);
    for (const uint32_t &its : s) {
      fts->insert(std::make_pair(its, itm.first.c_str()));
    }
  }
}

const Disassembler::FormToVisitorFnMap *Disassembler::GetFormToVisitorFnMap() {
  static const FormToVisitorFnMap form_to_visitor = {
      DEFAULT_FORM_TO_VISITOR_MAP(Disassembler),
  };
  return &form_to_visitor;
}  // NOLINT(readability/fn_size)

Disassembler::Disassembler() {
  buffer_size_ = 256;
  buffer_ = reinterpret_cast<char *>(malloc(buffer_size_));
  buffer_pos_ = 0;
  own_buffer_ = true;
  code_address_offset_ = 0;

  PopulateFormToStringMap(&form_to_string_);
}

Disassembler::Disassembler(char *text_buffer, int buffer_size) {
  buffer_size_ = buffer_size;
  buffer_ = text_buffer;
  buffer_pos_ = 0;
  own_buffer_ = false;
  code_address_offset_ = 0;

  PopulateFormToStringMap(&form_to_string_);
}

Disassembler::~Disassembler() {
  if (own_buffer_) {
    free(buffer_);
  }
}

char *Disassembler::GetOutput() { return buffer_; }

bool Disassembler::IsMovzMovnImm(unsigned reg_size, uint64_t value) {
  VIXL_ASSERT((reg_size == kXRegSize) ||
              ((reg_size == kWRegSize) && (value <= 0xffffffff)));

  // Test for movz: 16 bits set at positions 0, 16, 32 or 48.
  if (((value & UINT64_C(0xffffffffffff0000)) == 0) ||
      ((value & UINT64_C(0xffffffff0000ffff)) == 0) ||
      ((value & UINT64_C(0xffff0000ffffffff)) == 0) ||
      ((value & UINT64_C(0x0000ffffffffffff)) == 0)) {
    return true;
  }

  // Test for movn: NOT(16 bits set at positions 0, 16, 32 or 48).
  if ((reg_size == kXRegSize) &&
      (((~value & UINT64_C(0xffffffffffff0000)) == 0) ||
       ((~value & UINT64_C(0xffffffff0000ffff)) == 0) ||
       ((~value & UINT64_C(0xffff0000ffffffff)) == 0) ||
       ((~value & UINT64_C(0x0000ffffffffffff)) == 0))) {
    return true;
  }
  if ((reg_size == kWRegSize) && (((value & 0xffff0000) == 0xffff0000) ||
                                  ((value & 0x0000ffff) == 0x0000ffff))) {
    return true;
  }
  return false;
}

static bool SVEMoveMaskPreferred(uint64_t value, int lane_bytes_log2) {
  VIXL_ASSERT(IsUintN(8 << lane_bytes_log2, value));

  // Duplicate lane-sized value across double word.
  switch (lane_bytes_log2) {
    case 0:
      value *= 0x0101010101010101;
      break;
    case 1:
      value *= 0x0001000100010001;
      break;
    case 2:
      value *= 0x0000000100000001;
      break;
    case 3:  // Nothing to do
      break;
    default:
      VIXL_UNREACHABLE();
  }

  if ((value & 0xff) == 0) {
    // mov z.d, #signed_16bit_imm
    if (value == SignExtend(value, 16)) {
      return false;
    }

    // mov z.s, #signed_16bit_imm
    uint32_t value32 = static_cast<uint32_t>(value);
    if (AllWordsMatch(value) && (value32 == SignExtend(value32, 16))) {
      return false;
    }

    // mov z.h, #signed_16bit_imm
    if (AllHalfwordsMatch(value)) {
      return false;
    }
  } else {
    // mov z.d, #signed_8bit_imm
    if (value == SignExtend(value, 8)) {
      return false;
    }

    // mov z.s, #signed_8bit_imm
    uint32_t value32 = static_cast<uint32_t>(value);
    if (AllWordsMatch(value) && (value32 == SignExtend(value32, 8))) {
      return false;
    }

    // mov z.h, #signed_8bit_imm
    uint16_t value16 = static_cast<uint16_t>(value);
    if (AllHalfwordsMatch(value) && (value16 == SignExtend(value16, 8))) {
      return false;
    }

    // mov z.b, #signed_8bit_imm
    if (AllBytesMatch(value)) {
      return false;
    }
  }
  return true;
}

void Disassembler::VisitSVEBroadcastBitmaskImm(const Instruction *instr) {
  uint64_t imm = instr->GetSVEImmLogical();
  if (imm == 0) {
    VisitUnallocated(instr);
  } else {
    int lane_size = instr->GetSVEBitwiseImmLaneSizeInBytesLog2();
    const char *mnemonic =
        SVEMoveMaskPreferred(imm, lane_size) ? "mov" : "dupm";
    const char *form = "'Zd.'(17?d:'[sszlog]), 'ITriSvel";
    Format(instr, mnemonic, form);
  }
}

void Disassembler::VisitReserved(const Instruction *instr) {
  FormatWithDecodedMnemonic(instr, "#0x'x1500");
}

void Disassembler::VisitUnimplemented(const Instruction *instr) {
  Format(instr, "unimplemented", "(Unimplemented)");
}

void Disassembler::VisitUnallocated(const Instruction *instr) {
  Format(instr, "unallocated", "(Unallocated)");
}

void Disassembler::Visit(Metadata *metadata, const Instruction *instr) {
  VIXL_ASSERT(metadata->count("form") > 0);
  // Check for unallocated encodings.
  if (metadata->count("unallocated") > 0) {
    VisitUnallocated(instr);
    return;
  }

  std::string form = (*metadata)["form"];
  form_hash_ = Hash(form.c_str());

  // Find the alias of the decoded instruction, if any.
  std::string alias = GetMnemonicAlias(instr);
  if (alias.length() > 0) {
    form = alias + "_" + form;
    form_hash_ = Hash(form.c_str());
  }

  // Get the disassembly string for this form or alias.
  FormToStringMap::const_iterator its = form_to_string_.find(form_hash_);
  if (its != form_to_string_.end()) {
    SetMnemonicFromForm(form);
    FormatWithDecodedMnemonic(instr, its->second);
    return;
  }

  if (alias.length() > 0) {
    printf("Unhandled %s\n", form.c_str());
  }
  // All aliases should be handled by this point.
  VIXL_ASSERT(alias.length() == 0);

  // Call a legacy visitor-based handler.
  const FormToVisitorFnMap *fv = Disassembler::GetFormToVisitorFnMap();
  FormToVisitorFnMap::const_iterator it = fv->find(form_hash_);
  if (it == fv->end()) {
    VisitUnimplemented(instr);
  } else {
    SetMnemonicFromForm(form);
    (it->second)(this, instr);
  }
}

void Disassembler::ProcessOutput(const Instruction * /*instr*/) {
  // The base disasm does nothing more than disassembling into a buffer.
}

void Disassembler::AppendRegisterNameToOutput(const Instruction *instr,
                                              const CPURegister &reg) {
  USE(instr);
  VIXL_ASSERT(reg.IsValid());
  char reg_char;

  if (reg.IsRegister()) {
    reg_char = reg.Is64Bits() ? 'x' : 'w';
  } else {
    VIXL_ASSERT(reg.IsVRegister());
    switch (reg.GetSizeInBits()) {
      case kBRegSize:
        reg_char = 'b';
        break;
      case kHRegSize:
        reg_char = 'h';
        break;
      case kSRegSize:
        reg_char = 's';
        break;
      case kDRegSize:
        reg_char = 'd';
        break;
      default:
        VIXL_ASSERT(reg.Is128Bits());
        reg_char = 'q';
    }
  }

  if (reg.IsVRegister() || !(reg.Aliases(sp) || reg.Aliases(xzr))) {
    // A core or scalar/vector register: [wx]0 - 30, [bhsdq]0 - 31.
    AppendToOutput("%c%d", reg_char, reg.GetCode());
  } else if (reg.Aliases(sp)) {
    // Disassemble w31/x31 as stack pointer wsp/sp.
    AppendToOutput("%s", reg.Is64Bits() ? "sp" : "wsp");
  } else {
    // Disassemble w31/x31 as zero register wzr/xzr.
    AppendToOutput("%czr", reg_char);
  }
}

void Disassembler::AppendPCRelativeOffsetToOutput(const Instruction *instr,
                                                  int64_t offset) {
  USE(instr);
  if (offset < 0) {
    // Cast to uint64_t so that INT64_MIN is handled in a well-defined way.
    uint64_t abs_offset = UnsignedNegate(static_cast<uint64_t>(offset));
    AppendToOutput("#-0x%" PRIx64, abs_offset);
  } else {
    AppendToOutput("#+0x%" PRIx64, offset);
  }
}

void Disassembler::AppendAddressToOutput(const Instruction *instr,
                                         const void *addr) {
  USE(instr);
  AppendToOutput("(addr 0x%" PRIxPTR ")", reinterpret_cast<uintptr_t>(addr));
}

void Disassembler::AppendCodeAddressToOutput(const Instruction *instr,
                                             const void *addr) {
  AppendAddressToOutput(instr, addr);
}

void Disassembler::AppendDataAddressToOutput(const Instruction *instr,
                                             const void *addr) {
  AppendAddressToOutput(instr, addr);
}

void Disassembler::AppendCodeRelativeAddressToOutput(const Instruction *instr,
                                                     const void *addr) {
  USE(instr);
  int64_t rel_addr = CodeRelativeAddress(addr);
  if (rel_addr >= 0) {
    AppendToOutput("(addr 0x%" PRIx64 ")", rel_addr);
  } else {
    AppendToOutput("(addr -0x%" PRIx64 ")", -rel_addr);
  }
}

void Disassembler::AppendCodeRelativeCodeAddressToOutput(
    const Instruction *instr, const void *addr) {
  AppendCodeRelativeAddressToOutput(instr, addr);
}

void Disassembler::AppendCodeRelativeDataAddressToOutput(
    const Instruction *instr, const void *addr) {
  AppendCodeRelativeAddressToOutput(instr, addr);
}

void Disassembler::MapCodeAddress(int64_t base_address,
                                  const Instruction *instr_address) {
  set_code_address_offset(base_address -
                          reinterpret_cast<intptr_t>(instr_address));
}
int64_t Disassembler::CodeRelativeAddress(const void *addr) {
  return reinterpret_cast<intptr_t>(addr) + code_address_offset();
}

void Disassembler::Format(const Instruction *instr,
                          const char *mnemonic,
                          const char *format0,
                          const char *format1) {
  if ((mnemonic == NULL) || (format0 == NULL)) {
    VisitUnallocated(instr);
  } else {
    ResetOutput();
    Substitute(instr, mnemonic);
    if (format0[0] != 0) {  // Not a zero-length string.
      VIXL_ASSERT(buffer_pos_ < buffer_size_);
      buffer_[buffer_pos_++] = ' ';
      int chars_written = Substitute(instr, format0);
      // TODO: consider using a zero-length string here, too.
      if (format1 != NULL) {
        chars_written += Substitute(instr, format1);
      }

      if (chars_written == 0) {
        // Erase the space written earlier, as there are no arguments to the
        // instruction.
        buffer_pos_--;
      }
    }
    VIXL_ASSERT(buffer_pos_ < buffer_size_);
    buffer_[buffer_pos_] = 0;
    ProcessOutput(instr);
  }
}

void Disassembler::FormatWithDecodedMnemonic(const Instruction *instr,
                                             const char *format0,
                                             const char *format1) {
  Format(instr, mnemonic_.c_str(), format0, format1);
}

int Disassembler::Substitute(const Instruction *instr, const char *string) {
  uint32_t buffer_pos_init = buffer_pos_;
  char chr = *string++;
  while (chr != '\0') {
    if (chr == '\'') {
      int offset = SubstituteField(instr, string);
      if (offset == 0) break;
      string += offset;
    } else {
      VIXL_ASSERT(buffer_pos_ < buffer_size_);
      buffer_[buffer_pos_++] = chr;
    }
    chr = *string++;
  }
  return static_cast<int>(buffer_pos_ - buffer_pos_init);
}

int Disassembler::SubstituteField(const Instruction *instr,
                                  const char *format) {
  switch (format[0]) {
    case 'R':  // Register. X or W, selected by sf (or alternative) bit.
    case 'F':  // FP register. S or D, selected by type field.
    case 'V':  // Vector register, V, vector format.
    case 'Z':  // Scalable vector register.
    case 'W':
    case 'X':
    case 'B':
    case 'H':
    case 'S':
    case 'D':
    case 'Q':
      return SubstituteRegisterField(instr, format);
    case 'P':
      return SubstitutePredicateRegisterField(instr, format);
    case 'I':
      return SubstituteImmediateField(instr, format);
    case 'L':
      return SubstituteLiteralField(instr, format);
    case 'A':
      return SubstitutePCRelAddressField(instr, format);
    case 'T':
      return SubstituteBranchTargetField(instr, format);
    case 'u':
    case 's':
    case 'x':
    case 'n':
      return SubstituteIntField(instr, format);
    case 'f':
      return SubstituteFPField(instr, format);
    case '?':
      return SubstituteTernary(instr, format);
    case '(':
      return SubstituteConditionalBlock(instr, format);
    case '[':
      return SubstituteGenericArray(instr, format);
    case '{':
      return SubstituteGenericHash(instr, format);
    case '<':
      return SubstituteExpression(instr, format);
    case '$':
      return SubstituteEnd(instr, format);
    default: {
      VIXL_UNREACHABLE();
      return 1;
    }
  }
}

std::pair<unsigned, unsigned> Disassembler::GetRegNumForField(
    const Instruction *instr, char reg_prefix, const char *field) {
  unsigned reg_num = UINT_MAX;
  unsigned field_len = 1;

  switch (field[0]) {
    case 'd':
      reg_num = instr->GetRd();
      break;
    case 'n':
      reg_num = instr->GetRn();
      break;
    case 'm':
      reg_num = instr->GetRm();
      break;
    case 'e':
      // This is register Rm, but using a 4-bit specifier. Used in NEON
      // by-element instructions.
      reg_num = instr->GetRmLow16();
      break;
    case 'f':
      // This is register Rm, but using an element size dependent number of bits
      // in the register specifier.
      reg_num =
          (instr->GetNEONSize() < 2) ? instr->GetRmLow16() : instr->GetRm();
      break;
    case 'a':
      reg_num = instr->GetRa();
      break;
    case 's':
      reg_num = instr->GetRs();
      break;
    case 't':
      reg_num = instr->GetRt();
      break;
    default:
      VIXL_UNREACHABLE();
  }

  switch (field[1]) {
    case '2':
    case '3':
    case '4':
      if ((reg_prefix == 'V') || (reg_prefix == 'Z')) {  // t2/3/4, n2/3/4
        VIXL_ASSERT((field[0] == 't') || (field[0] == 'n'));
        reg_num = (reg_num + field[1] - '1') % 32;
        field_len++;
      } else {
        VIXL_ASSERT((field[0] == 't') && (field[1] == '2'));
        reg_num = instr->GetRt2();
        field_len++;
      }
      break;
    case '+':  // Rt+, Rs+ (ie. Rt + 1, Rs + 1)
      VIXL_ASSERT((reg_prefix == 'W') || (reg_prefix == 'X'));
      VIXL_ASSERT((field[0] == 's') || (field[0] == 't'));
      reg_num++;
      field_len++;
      break;
    case 's':  // Core registers that are (w)sp rather than zr.
      VIXL_ASSERT((reg_prefix == 'W') || (reg_prefix == 'X'));
      reg_num = (reg_num == kZeroRegCode) ? kSPRegInternalCode : reg_num;
      field_len++;
      break;
  }

  VIXL_ASSERT(reg_num != UINT_MAX);
  return std::make_pair(reg_num, field_len);
}

int BitPositionFromString(const char *c) {
  VIXL_ASSERT(strspn(c, "0123456789") >= 2);
  int pos = ((c[0] - '0') * 10) + (c[1] - '0');
  VIXL_ASSERT(pos <= 31);
  return pos;
}

int Disassembler::SubstituteRegisterField(const Instruction *instr,
                                          const char *format) {
  unsigned field_len = 1;  // Initially, count only the first character.

  // The first character of the register format field, eg R, X, S, etc.
  char reg_prefix = format[0];

  // Pointer to the character after the prefix. This may be one of the standard
  // symbols representing a register encoding, or a two digit bit position,
  // handled by the following code.
  const char *reg_field = &format[1];

  if (reg_prefix == 'R') {
    bool is_x = instr->GetSixtyFourBits() == 1;
    if (strspn(reg_field, "0123456789") == 2) {  // r20d, r31n, etc.
      // Core W or X registers where the type is determined by a specified bit
      // position, eg. 'R20d, 'R05n. This is like the 'Rd syntax, where bit 31
      // is implicitly used to select between W and X.
      int bitpos = BitPositionFromString(reg_field);
      is_x = (instr->ExtractBit(bitpos) == 1);
      reg_field = &format[3];
      field_len += 2;
    }
    reg_prefix = is_x ? 'X' : 'W';
  }

  std::pair<unsigned, unsigned> rn =
      GetRegNumForField(instr, reg_prefix, reg_field);
  unsigned reg_num = rn.first;
  field_len += rn.second;

  if (reg_field[0] == 'm') {
    switch (reg_field[1]) {
      // Handle registers tagged with b (bytes), z (instruction), or
      // r (registers), used for address updates in NEON load/store
      // instructions.
      case 'r':
      case 'b':
      case 'z': {
        VIXL_ASSERT(reg_prefix == 'X');
        field_len = 3;
        char *eimm;
        int imm = static_cast<int>(strtol(&reg_field[2], &eimm, 10));
        field_len += static_cast<unsigned>(eimm - &reg_field[2]);
        if (reg_num == 31) {
          switch (reg_field[1]) {
            case 'z':
              imm *= (1 << instr->GetNEONLSSize());
              break;
            case 'r':
              imm *= (instr->GetNEONQ() == 0) ? kDRegSizeInBytes
                                              : kQRegSizeInBytes;
              break;
            case 'b':
              break;
          }
          AppendToOutput("#%d", imm);
          return field_len;
        }
        break;
      }
    }
  }

  CPURegister::RegisterType reg_type = CPURegister::kRegister;
  unsigned reg_size = kXRegSize;

  if (reg_prefix == 'F') {
    switch (instr->GetFPType()) {
      case 3:
        reg_prefix = 'H';
        break;
      case 0:
        reg_prefix = 'S';
        break;
      default:
        reg_prefix = 'D';
    }
  }

  switch (reg_prefix) {
    case 'W':
      reg_type = CPURegister::kRegister;
      reg_size = kWRegSize;
      break;
    case 'X':
      reg_type = CPURegister::kRegister;
      reg_size = kXRegSize;
      break;
    case 'B':
      reg_type = CPURegister::kVRegister;
      reg_size = kBRegSize;
      break;
    case 'H':
      reg_type = CPURegister::kVRegister;
      reg_size = kHRegSize;
      break;
    case 'S':
      reg_type = CPURegister::kVRegister;
      reg_size = kSRegSize;
      break;
    case 'D':
      reg_type = CPURegister::kVRegister;
      reg_size = kDRegSize;
      break;
    case 'Q':
      reg_type = CPURegister::kVRegister;
      reg_size = kQRegSize;
      break;
    case 'V':
      if (reg_field[1] == 'v') {
        reg_type = CPURegister::kVRegister;
        reg_size = 1 << (instr->GetSVESize() + 3);
        field_len++;
        break;
      }
      AppendToOutput("v%d", reg_num);
      return field_len;
    case 'Z':
      AppendToOutput("z%d", reg_num);
      return field_len;
    default:
      VIXL_UNREACHABLE();
  }

  AppendRegisterNameToOutput(instr, CPURegister(reg_num, reg_size, reg_type));

  return field_len;
}

int Disassembler::SubstitutePredicateRegisterField(const Instruction *instr,
                                                   const char *format) {
  VIXL_ASSERT(format[0] == 'P');
  switch (format[1]) {
    // This field only supports P register that are always encoded in the same
    // position.
    case 'd':
    case 't':
      AppendToOutput("p%u", instr->GetPt());
      break;
    case 'n':
      AppendToOutput("p%u", instr->GetPn());
      break;
    case 'm':
      AppendToOutput("p%u", instr->GetPm());
      break;
    case 'g':
      VIXL_ASSERT(format[2] == 'l');
      AppendToOutput("p%u", instr->GetPgLow8());
      return 3;
    default:
      VIXL_UNREACHABLE();
  }
  return 2;
}

int Disassembler::SubstituteImmediateField(const Instruction *instr,
                                           const char *format) {
  VIXL_ASSERT(format[0] == 'I');

  switch (format[1]) {
    case 'T': {  // ITri - Immediate Triangular Encoded.
      if (format[4] == 'S') {
        VIXL_ASSERT((format[5] == 'v') && (format[6] == 'e'));
        switch (format[7]) {
          case 'l':
            // SVE logical immediate encoding.
            AppendToOutput("#0x%" PRIx64, instr->GetSVEImmLogical());
            return 8;
          default:
            VIXL_UNREACHABLE();
            return 0;
        }
      } else {
        AppendToOutput("#0x%" PRIx64, instr->GetImmLogical());
        return 4;
      }
    }
    case 'Y': {  // IY - system register immediate.
      switch (instr->GetImmSystemRegister()) {
        case NZCV:
          AppendToOutput("nzcv");
          break;
        case FPCR:
          AppendToOutput("fpcr");
          break;
        case RNDR:
          AppendToOutput("rndr");
          break;
        case RNDRRS:
          AppendToOutput("rndrrs");
          break;
        case DCZID_EL0:
          AppendToOutput("dczid_el0");
          break;
        default:
          AppendToOutput("S%d_%d_c%d_c%d_%d",
                         instr->GetSysOp0(),
                         instr->GetSysOp1(),
                         instr->GetCRn(),
                         instr->GetCRm(),
                         instr->GetSysOp2());
          break;
      }
      return 2;
    }
    default: {
      VIXL_UNIMPLEMENTED();
      return 0;
    }
  }
}

int Disassembler::SubstituteLiteralField(const Instruction *instr,
                                         const char *format) {
  VIXL_ASSERT(strncmp(format, "LValue", 6) == 0);
  USE(format);

  const void *address = instr->GetLiteralAddress<const void *>();
  switch (instr->Mask(LoadLiteralMask)) {
    case LDR_w_lit:
    case LDR_x_lit:
    case LDRSW_x_lit:
    case LDR_s_lit:
    case LDR_d_lit:
    case LDR_q_lit:
      AppendCodeRelativeDataAddressToOutput(instr, address);
      break;
    case PRFM_lit: {
      // Use the prefetch hint to decide how to print the address.
      switch (instr->GetPrefetchHint()) {
        case 0x0:  // PLD: prefetch for load.
        case 0x2:  // PST: prepare for store.
          AppendCodeRelativeDataAddressToOutput(instr, address);
          break;
        case 0x1:  // PLI: preload instructions.
          AppendCodeRelativeCodeAddressToOutput(instr, address);
          break;
        case 0x3:  // Unallocated hint.
          AppendCodeRelativeAddressToOutput(instr, address);
          break;
      }
      break;
    }
    default:
      VIXL_UNREACHABLE();
  }

  return 6;
}

int Disassembler::SubstitutePCRelAddressField(const Instruction *instr,
                                              const char *format) {
  VIXL_ASSERT((strcmp(format, "AddrPCRelByte") == 0) ||  // Used by `adr`.
              (strcmp(format, "AddrPCRelPage") == 0));   // Used by `adrp`.

  int64_t offset = instr->GetImmPCRel();

  // Compute the target address based on the effective address (after applying
  // code_address_offset). This is required for correct behaviour of adrp.
  const Instruction *base = instr + code_address_offset();
  if (format[9] == 'P') {
    offset *= kPageSize;
    base = AlignDown(base, kPageSize);
  }
  // Strip code_address_offset before printing, so we can use the
  // semantically-correct AppendCodeRelativeAddressToOutput.
  const void *target =
      reinterpret_cast<const void *>(base + offset - code_address_offset());

  AppendPCRelativeOffsetToOutput(instr, offset);
  AppendToOutput(" ");
  AppendCodeRelativeAddressToOutput(instr, target);
  return 13;
}

int Disassembler::SubstituteBranchTargetField(const Instruction *instr,
                                              const char *format) {
  VIXL_ASSERT(strncmp(format, "TImm", 4) == 0);

  int64_t offset = 0;
  switch (format[5]) {
    // BImmUncn - unconditional branch immediate.
    case 'n':
      offset = instr->GetImmUncondBranch();
      break;
    // BImmCond - conditional branch immediate.
    case 'o':
      offset = instr->GetImmCondBranch();
      break;
    // BImmCmpa - compare and branch immediate.
    case 'm':
      offset = instr->GetImmCmpBranch();
      break;
    // BImmTest - test and branch immediate.
    case 'e':
      offset = instr->GetImmTestBranch();
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
  offset *= static_cast<int>(kInstructionSize);
  const void *target_address = reinterpret_cast<const void *>(instr + offset);
  VIXL_STATIC_ASSERT(sizeof(*instr) == 1);

  AppendPCRelativeOffsetToOutput(instr, offset);
  AppendToOutput(" ");
  AppendCodeRelativeCodeAddressToOutput(instr, target_address);

  return 8;
}

std::pair<int32_t, int> ExtractIntTerm(const Instruction *instr,
                                       const char *c) {
  int32_t bits = 0;
  int off = 0;
  if (std::isdigit(*c)) {
    // Parse decimal and hexadecimal constants.
    int base = ((c[0] == '0') && (c[1] == 'x')) ? 16 : 10;

    char *new_c;
    uint64_t value = strtoul(c, &new_c, base);
    off = static_cast<int>(new_c - c);
    VIXL_ASSERT(IsInt32(value));
    bits = static_cast<int32_t>(value);
  } else {
    VIXL_ASSERT(strchr("suxf", *c) != nullptr);
    int width = 0;
    do {
      if (strspn(&c[off + 1], "0123456789") == 2) {
        int pos = BitPositionFromString(&c[off + 1]);
        bits = (bits << 1) | instr->ExtractBit(pos);
        width++;
        off += 3;  // Skip [usx_] and the two character bit position.
      } else {
        VIXL_ASSERT(strspn(&c[off + 1], "0123456789") == 4);
        int msb = BitPositionFromString(&c[off + 1]);
        int lsb = BitPositionFromString(&c[off + 3]);
        int chunk_width = msb - lsb + 1;
        VIXL_ASSERT((chunk_width > 0) && (chunk_width < 32));
        width += chunk_width;
        VIXL_ASSERT(width <= 31);
        bits = (bits << chunk_width) | instr->ExtractBits(msb, lsb);
        off += 5;  // Skip [usx_] and the four character bit position.
      }
    } while (c[off] == '_');
    VIXL_ASSERT(IsUintN(width, bits));

    if (c[0] == 's') {
      bits = ExtractSignedBitfield32(width - 1, 0, bits);
    }
  }

  return {bits, off};
}

int Disassembler::SubstituteIntField(const Instruction *instr,
                                     const char *format) {
  VIXL_ASSERT((*format == 'u') || (*format == 's') || (*format == 'x'));

  // A generic signed or unsigned int field uses a placeholder of the form
  // 'sAABB and 'uAABB respectively where AA and BB are two digit bit positions
  // between 00 and 31, and AA >= BB. The placeholder is substituted with the
  // decimal integer represented by the bits in the instruction between
  // positions AA and BB inclusive.
  //
  // In addition, split fields can be represented using 'sAABB_CCDD, where CCDD
  // become the least-significant bits of the result, and bit AA is the sign bit
  // (if 's is used).
  //
  // For unsigned fields, 'u may be replaced with 'x to substitute the
  // hexadecimal representation instead of a decimal.
  auto [bits, advance] = ExtractIntTerm(instr, format);
  AppendToOutput(format[0] == 'x' ? "%x" : "%d", bits);
  return advance;
}

int Disassembler::SubstituteFPField(const Instruction *instr,
                                    const char *format) {
  VIXL_ASSERT(format[0] == 'f');
  // A generic floating-point field uses a placeholder of the form 'fAABB where
  // AA and BB are two-digit bit positions between 00 and 31, and AA >= BB. The
  // placeholder is substituted with the 8-bit extracted integer and floating
  // point value resulting from the conversion of those eight bits to FP format
  // using Imm8ToFP32().
  //
  // In addition, split fields can be represented using 'fAABB_CCDD, where CCDD
  // become the least-significant bits of the 8-bit value.
  auto [bits, advance] = ExtractIntTerm(instr, format);
  AppendToOutput("0x%" PRIx32 " (%.4f)", bits, Instruction::Imm8ToFP32(bits));
  return advance;
}

int Disassembler::SubstituteGenericHash(const Instruction *instr,
                                        const char *format) {
  VIXL_ASSERT(format[0] == '{');
  const char *close = strchr(format, '}');
  VIXL_ASSERT(close != nullptr);
  int keylen = static_cast<int>(close - format - 1);
  std::string key(format, 1, keylen);

  struct SubstList {
    std::vector<int> bit_positions;
    std::map<int, std::string> substitutions;
    std::string no_substitution;
  };

  // clang-format off
  static const std::map<std::string, SubstList> subst = {
    {"dcop", {{18, 17, 16, 11, 10, 9, 8, 7, 6, 5}, {
      {0b011'0100'001, "zva"},
      {0b011'0100'011, "gva"},
      {0b011'0100'100, "gzva"},
      {0b011'1010'001, "cvac"},
      {0b011'1010'011, "cgvac"},
      {0b011'1010'101, "cgdvac"},
      {0b011'1011'001, "cvau"},
      {0b011'1100'001, "cvap"},
      {0b011'1100'011, "cgvap"},
      {0b011'1100'101, "cgdvap"},
      {0b011'1101'001, "cvadp"},
      {0b011'1110'001, "civac"},
      {0b011'1110'011, "cigvac"},
      {0b011'1110'101, "cigdvac"},
     }, "undefined"}
   },
   {"pstatefield", {{18, 17, 16, 7, 6, 5}, {
      {0b011'110, "daifset"},
      {0b011'111, "daifclr"},
     }, "undefined"}
   }
  };
  // clang-format on

  VIXL_ASSERT(subst.count(key) == 1);
  auto x = subst.at(key);
  int index = 0;
  for (auto b : x.bit_positions) {
    index <<= 1;
    index |= instr->ExtractBit(b);
  }
  if (x.substitutions.count(index) == 1) {
    AppendToOutput("%s", x.substitutions.at(index).c_str());
  } else {
    AppendToOutput("%s", x.no_substitution.c_str());
  }
  return keylen + 2;  // +2 for the braces.
}

int Disassembler::SubstituteGenericArray(const Instruction *instr,
                                         const char *format) {
  VIXL_ASSERT(format[0] == '[');
  const char *close = strchr(format, ']');
  VIXL_ASSERT(close != nullptr);
  int keylen = static_cast<int>(close - format - 1);
  std::string key(format, 1, keylen);

  struct SubstList {
    std::vector<int> bit_positions;
    std::vector<std::string> substitutions;
  };

  // clang-format off
  static const std::map<std::string, SubstList> subst = {
      {"ext", {{15, 14, 13},
      {"uxtb", "uxth", "uxtw", "uxtx", "sxtb", "sxth", "sxtw", "sxtx"}}},
      {"ext32", {{15, 14, 13},
      {"uxtb", "uxth", "lsl", "uxtx", "sxtb", "sxth", "sxtw", "sxtx"}}},
      {"ext64", {{15, 14, 13},
      {"uxtb", "uxth", "uxtw", "lsl", "sxtb", "sxth", "sxtw", "sxtx"}}},
      {"extmem", {{15, 13}, {"uxtw", "lsl", "sxtw", "sxtx"}}},
      {"cond",
       {{15, 14, 13, 12},
        {"eq", "ne", "hs", "lo", "mi", "pl", "vs", "vc",
         "hi", "ls", "ge", "lt", "gt", "le", "al", "nv"}}},
      {"condinv",
       {{15, 14, 13, 12},
        {"ne", "eq", "lo", "hs", "pl", "mi", "vc", "vs",
         "ls", "hi", "lt", "ge", "le", "gt", "undefined", "undefined"}}},
      {"condb",
       {{3, 2, 1, 0},
        {"eq", "ne", "hs", "lo", "mi", "pl", "vs", "vc",
         "hi", "ls", "ge", "lt", "gt", "le", "al", "nv"}}},
      {"nzcv",
       {{3, 2, 1, 0},
        {"nzcv", "nzcV", "nzCv", "nzCV",
         "nZcv", "nZcV", "nZCv", "nZCV",
         "Nzcv", "NzcV", "NzCv", "NzCV",
         "NZcv", "NZcV", "NZCv", "NZCV"}}},
      {"prefop",
       {{4, 3, 2, 1, 0},
        {"pldl1keep",  "pldl1strm",  "pldl2keep",  "pldl2strm",  "pldl3keep",
         "pldl3strm",  "pldslckeep", "pldslcstrm", "plil1keep",  "plil1strm",
         "plil2keep",  "plil2strm",  "plil3keep",  "plil3strm",  "plislckeep",
         "plislcstrm", "pstl1keep",  "pstl1strm",  "pstl2keep",  "pstl2strm",
         "pstl3keep",  "pstl3strm",  "pstslckeep", "pstslcstrm", "#0b11000",
         "#0b11001",   "#0b11010",   "#0b11011",   "#0b11100",   "#0b11101",
         "#0b11110",   "#0b11111"}}},
      {"prefsveop",
       {{3, 2, 1, 0},
        {"pldl1keep", "pldl1strm",  "pldl2keep",  "pldl2strm",
         "pldl3keep", "pldl3strm",  "#0b0110",    "#0b0111",
         "pstl1keep", "pstl1strm",  "pstl2keep",  "pstl2strm",
         "pstl3keep", "pstl3strm",  "#0b1110",    "#0b1111"}}},
      {"n", {{30, 23, 22}, {"8b", "4h", "2s", "1d", "16b", "8h", "4s", "2d"}}},
      {"nl", {{23, 22}, {"8h", "4s", "2d", ""}}},
      {"nf", {{22, 30}, {"2s", "4s", "1d", "2d"}}},
      {"nload",
       {{30, 11, 10}, {"8b", "4h", "2s", "1d", "16b", "8h", "4s", "2d"}}},
      {"npair", {{30, 23, 22}, {"4h", "2s", "1d", "", "8h", "4s", "2d", ""}}},
      {"nscall", {{23, 22}, {"h", "s", "d", "q"}}},
      {"nshift",
       {{22, 21, 20, 19, 30},
        {"",   "",   "8b", "16b", "4h", "8h", "4h", "8h", "2s", "4s", "2s",
         "4s", "2s", "4s", "2s",  "4s", "",   "2d", "",   "2d", "",   "2d",
         "",   "2d", "",   "2d",  "",   "2d", "",   "2d", "",   "2d"}}},
      {"nshiftln",
       {{21, 20, 19}, {"", "8h", "4s", "4s", "2d", "2d", "2d", "2d"}}},
      {"nshiftscal",
       {{22, 21, 20, 19},
        {"",  "b", "h", "h", "s", "s", "s", "s",
         "d", "d", "d", "d", "d", "d", "d", "d"}}},
      {"sz", {{23, 22}, {"b", "h", "s", "d"}}},
      {"sszshu",
       {{23, 22, 9, 8},
        {"",  "b", "h", "h", "s", "s", "s", "s",
         "d", "d", "d", "d", "d", "d", "d", "d"}}},
      {"sszshs",
       {{23, 22, 20, 19},
        {"",  "b", "h", "h", "s", "s", "s", "s",
         "d", "d", "d", "d", "d", "d", "d", "d"}}},
      {"sszshd",
       {{23, 22, 20, 19},
        {"",  "h", "s", "s", "d", "d", "d", "d",
         "",  "",  "",  "",  "",  "",  "",  ""}}},
      {"sszld",
       {{24, 23, 22, 21},
        {"b", "h", "s", "d", "d", "h", "s", "d",
         "d", "s", "s", "d", "d", "s", "h", "d"}}},
      {"sszst",
       {{22, 21},
        {"b", "h", "s", "d"}}},
      {"sszdup",
       {{20, 19, 18, 17, 16},
        {"",  "b", "h", "b", "s", "b", "h", "b",
         "d", "b", "h", "b", "s", "b", "h", "b",
         "q", "b", "h", "b", "s", "b", "h", "b",
         "d", "b", "h", "b", "s", "b", "h", "b"}}},
      {"ntri",
       {{19, 18, 17, 16, 30},
        {"",   "",   "8b", "16b", "4h", "8h", "8b", "16b",
         "2s", "4s", "8b", "16b", "4h", "8h", "8b", "16b",
         "",   "2d", "8b", "16b", "4h", "8h", "8b", "16b",
         "2s", "4s", "8b", "16b", "4h", "8h", "8b", "16b"}}},
      {"ntriscal",
       {{19, 18, 17, 16},
        {"",  "b", "h", "b", "s", "b", "h", "b",
         "d", "b", "h", "b", "s", "b", "h", "b"}}},
      {"shift", {{23, 22}, {"lsl", "lsr", "asr", "ror"}}},
      {"sszlog",
       {{10, 9, 8, 7, 6},
        {"s", "s", "s", "s", "s", "s", "s", "s",
         "s", "s", "s", "s", "s", "s", "s", "s",
         "h", "h", "h", "h", "h", "h", "h", "h",
         "b", "b", "b", "b", "b", "b", "b", "",}}},
      {"sszmem", {{24, 23}, {"b", "h", "s", "d"}}},
      {"sszh", {{23, 22}, {"", "b", "h", "s"}}},
      {"sszq", {{23, 22}, {"", "", "b", "h"}}},
      {"flogbsz", {{18, 17}, {"b", "h", "s", "d"}}},
      {"mulpat",
       {{9, 8, 7, 6, 5},
        {"pow2",  "vl1",   "vl2",   "vl3",   "vl4",   "vl5",   "vl6",
         "vl7",   "vl8",   "vl16",  "vl32",  "vl64",  "vl128", "vl256",
         "#0xe",  "#0xf",  "#0x10", "#0x11", "#0x12", "#0x13", "#0x14",
         "#0x15", "#0x16", "#0x17", "#0x18", "#0x19", "#0x1a", "#0x1b",
         "#0x1c", "mul4",  "mul3",  "all"}}},
      {"barrier",
       {{11, 10, 9, 8},
        {"reserved (0b0000)", "oshld", "oshst", "osh",
         "reserved (0b0100)", "nshld", "nshst", "nsh",
         "reserved (0b1000)", "ishld", "ishst", "ish",
         "reserved (0b1100)", "ld",    "st",    "sy"}}},
  };
  // clang-format on

  VIXL_ASSERT(subst.count(key) == 1);
  auto x = subst.at(key);
  int index = 0;
  for (auto b : x.bit_positions) {
    index <<= 1;
    index |= instr->ExtractBit(b);
  }
  AppendToOutput("%s", x.substitutions.at(index).c_str());
  return keylen + 2;  // +2 for the braces.
}

using ExprStack = std::stack<uint64_t>;

bool HandleUnaryExpression(ExprStack *s, const std::string &op) {
  if (s->size() == 0) {
    return false;
  }

  uint64_t r = s->top();
  s->pop();

  if (op == "clz32") {
    s->push(CountLeadingZeros(r, 32));
  } else if (op == "ctz") {
    s->push(CountTrailingZeros(r, 32));
  } else if (op == "not") {
    s->push(~r);
  } else if (op == "dup") {
    s->push(r);
    s->push(r);
  } else {
    s->push(r);
    return false;
  }
  return true;
}

bool HandleBinaryExpression(ExprStack *s, const std::string &op) {
  if (s->size() < 2) {
    return false;
  }

  uint64_t r = s->top();
  s->pop();
  uint64_t b = s->top();

  if (op == "+") {
    r = b + r;
  } else if (op == "-") {
    r = b - r;
  } else if (op == "*") {
    r = b * r;
  } else if (op == "lsl") {
    r = b << r;
  } else if (op == "lsr") {
    r = b >> r;
  } else {
    s->push(r);
    return false;
  }

  s->pop();
  s->push(r);
  return true;
}

int Disassembler::SubstituteExpression(const Instruction *instr,
                                       const char *format) {
  USE(instr);
  VIXL_ASSERT(format[0] == '<');
  const char *close = strchr(format, '>');
  VIXL_ASSERT(close != nullptr);
  int exprlen = static_cast<int>(close - format - 1);
  std::string expr(format, 1, exprlen);

  ExprStack s;
  std::regex re(" ");
  std::sregex_token_iterator it(expr.begin(), expr.end(), re, -1);
  std::sregex_token_iterator end;

  const char *placeholder = "%ld";
  for (; it != end; it++) {
    std::string t = it->str();
    if (std::isdigit(t[0]) || (t[0] == 's') || (t[0] == 'u')) {
      auto [value, advance] = ExtractIntTerm(instr, t.c_str());
      VIXL_ASSERT(t.length() == static_cast<size_t>(advance));
      s.push(value);
    } else if (HandleBinaryExpression(&s, t)) {
      // Handled a binary operation - nothing else to do.
    } else if (HandleUnaryExpression(&s, t)) {
      // Handled a unary operation - nothing else to do.
    } else if (t == "hex") {
      // Print value on the stack as hexadecimal. This must be the result of the
      // computation and therefore the only value on the stack.
      VIXL_ASSERT(s.size() == 1);
      placeholder = "%lx";
    } else {
      printf("Unknown token: %s\n", t.c_str());
      VIXL_ABORT();
    }
  }
  VIXL_ASSERT(s.size() == 1);
  AppendToOutput(placeholder, s.top());

  return exprlen + 2;  // +2 for <>
}

int Disassembler::SubstituteEnd(const Instruction *instr, const char *format) {
  USE(instr);
  USE(format);
  VIXL_ASSERT(format[0] == '$');
  AppendToOutput("%c", '\0');
  return 0;
}

int Disassembler::SubstituteTernary(const Instruction *instr,
                                    const char *format) {
  VIXL_ASSERT((format[0] == '?') && (format[3] == ':'));

  // The ternary substitution of the format "'?bb:TF" is replaced by a single
  // character, either T or F, depending on the value of the bit at position
  // bb in the instruction. For example, "'?31:xw" is substituted with "x" if
  // bit 31 is true, and "w" otherwise.
  VIXL_ASSERT(strspn(&format[1], "0123456789") == 2);
  char *c;
  uint64_t value = strtoul(&format[1], &c, 10);
  VIXL_ASSERT(value < (kInstructionSize * kBitsPerByte));
  VIXL_ASSERT((*c == ':') && (strlen(c) >= 3));  // Minimum of ":TF"
  c++;
  AppendToOutput("%c", c[1 - instr->ExtractBit(static_cast<int>(value))]);
  return 6;
}

int Disassembler::SubstituteConditionalBlock(const Instruction *instr,
                                             const char *format) {
  VIXL_ASSERT(strlen(format) >= 6);
  VIXL_ASSERT((format[0] == '(') &&
              (format[3] == '?' || format[5] == '?' || (format[5] == '=')));
  VIXL_ASSERT(strchr(format, ')') != nullptr);

  // A conditional block uses the placeholder '(AABB?xxx:yyyy)' where AA and
  // BB are two digit bit positions between 00 and 31, and AA >= BB. If the
  // bits of the instruction in the range AA to BB are non-zero, the placeholder
  // is substituted with the string represented by xxx, else yyyy. The strings
  // are of variable length and may contain other placeholders for further
  // substitutions. The ':yyyy' section may be omitted, implying a zero-length
  // string is substituted if instruction bits in the range AA to BB are zero.
  //
  // Alternatively, a specific value for the bits in the range AA to BB can
  // be specified using the placeholder '(AABB=zzz?xxx:yyyy)'. If the bits in
  // the range AA to BB are equal to zzz, xxx is substitued, else yyyy. As
  // above, :yyyy may be omitted.
  const char *c = &format[1];
  uint32_t bits = 0;
  uint64_t value = 0;
  bool use_explicit_value = false;
  int msb = BitPositionFromString(&c[0]);
  if ((format[5] == '?') || (format[5] == '=')) {  // Extract a range of bits.
    int lsb = BitPositionFromString(&c[2]);
    bits = instr->ExtractBits(msb, lsb);

    if (format[5] == '=') {
      use_explicit_value = true;
      char *temp;
      VIXL_ASSERT(strspn(&format[6], "0123456789") > 0);
      value = strtoul(&format[6], &temp, 10);
      c = temp;
    } else {
      c += 4;  // Skip the bit positions we read above.
    }
  } else {
    // Extract a single bit.
    VIXL_ASSERT(format[3] == '?');
    bits = instr->ExtractBit(msb);
    c += 2;
  }

  // Skip '?'
  VIXL_ASSERT(*c == '?');
  c++;

  char temp[256] = {0};
  const char *close = strchr(format, ')');
  size_t subst_len = close - c;
  VIXL_ASSERT(subst_len < sizeof(temp));

  // Copy the substitution string into a temporary buffer and set up pointers
  // for the left-hand (true) and right-hand (false) sides.
  memcpy(temp, c, subst_len);

  char *lhs = temp;
  char *rhs = nullptr;
  char *colon = strchr(temp, ':');
  if (colon != nullptr) {
    // If there's a colon, set it to zero to act as the terminator for the
    // left-hand string.
    *colon = 0;
    rhs = colon + 1;
  }

  bool use_lhs;
  if (use_explicit_value) {
    use_lhs = (bits == value);
  } else {
    use_lhs = (bits != 0);
  }

  char *subst = use_lhs ? lhs : rhs;
  if ((subst != nullptr) && (strlen(subst) > 0)) {
    Substitute(instr, subst);
  }

  return static_cast<int>(1 + close - format);
}

void Disassembler::ResetOutput() {
  buffer_pos_ = 0;
  buffer_[buffer_pos_] = 0;
}

void Disassembler::AppendToOutput(const char *format, ...) {
  va_list args;
  va_start(args, format);
  buffer_pos_ += vsnprintf(&buffer_[buffer_pos_],
                           buffer_size_ - buffer_pos_,
                           format,
                           args);
  va_end(args);
}

void PrintDisassembler::Disassemble(const Instruction *instr) {
  Decoder decoder;
  if (cpu_features_auditor_ != NULL) {
    decoder.AppendVisitor(cpu_features_auditor_);
  }
  decoder.AppendVisitor(this);
  decoder.Decode(instr);
}

void PrintDisassembler::DisassembleBuffer(const Instruction *start,
                                          const Instruction *end) {
  Decoder decoder;
  if (cpu_features_auditor_ != NULL) {
    decoder.AppendVisitor(cpu_features_auditor_);
  }
  decoder.AppendVisitor(this);
  decoder.Decode(start, end);
}

void PrintDisassembler::DisassembleBuffer(const Instruction *start,
                                          uint64_t size) {
  DisassembleBuffer(start, start + size);
}

void PrintDisassembler::ProcessOutput(const Instruction *instr) {
  int64_t address = CodeRelativeAddress(instr);

  uint64_t abs_address;
  const char *sign;
  if (signed_addresses_) {
    if (address < 0) {
      sign = "-";
      abs_address = UnsignedNegate(static_cast<uint64_t>(address));
    } else {
      // Leave a leading space, to maintain alignment.
      sign = " ";
      abs_address = address;
    }
  } else {
    sign = "";
    abs_address = address;
  }

  int bytes_printed = fprintf(stream_,
                              "%s0x%016" PRIx64 "  %08" PRIx32 "\t\t%s",
                              sign,
                              abs_address,
                              instr->GetInstructionBits(),
                              GetOutput());
  if (cpu_features_auditor_ != NULL) {
    CPUFeatures needs = cpu_features_auditor_->GetInstructionFeatures();
    needs.Remove(cpu_features_auditor_->GetAvailableFeatures());
    if (needs != CPUFeatures::None()) {
      // Try to align annotations. This value is arbitrary, but based on looking
      // good with most instructions. Note that, for historical reasons, the
      // disassembly itself is printed with tab characters, so bytes_printed is
      // _not_ equivalent to the number of occupied screen columns. However, the
      // prefix before the tabs is always the same length, so the annotation
      // indentation does not change from one line to the next.
      const int indent_to = 70;
      // Always allow some space between the instruction and the annotation.
      const int min_pad = 2;

      int pad = std::max(min_pad, (indent_to - bytes_printed));
      fprintf(stream_, "%*s", pad, "");

      std::stringstream features;
      features << needs;
      fprintf(stream_,
              "%s%s%s",
              cpu_features_prefix_,
              features.str().c_str(),
              cpu_features_suffix_);
    }
  }
  fprintf(stream_, "\n");
}

}  // namespace aarch64
}  // namespace vixl
