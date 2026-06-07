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

#ifdef VIXL_INCLUDE_SIMULATOR_AARCH64

#include "simulator-aarch64.h"

#include <cmath>
#include <cstring>
#include <errno.h>
#include <limits>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#undef MultiplyHigh
#include <Memoryapi.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#ifdef _MSC_VER
#define VIXL_SYNC() MemoryBarrier()
#else
#define VIXL_SYNC() __sync_synchronize()
#endif

namespace vixl {
namespace aarch64 {

using vixl::internal::SimFloat16;

const Instruction* Simulator::kEndOfSimAddress = NULL;

MemoryAccessResult TryMemoryAccess(uintptr_t address, uintptr_t access_size) {
#ifdef VIXL_ENABLE_IMPLICIT_CHECKS
  for (uintptr_t i = 0; i < access_size; i++) {
    if (_vixl_internal_ReadMemory(address, i) == MemoryAccessResult::Failure) {
      // The memory access failed.
      return MemoryAccessResult::Failure;
    }
  }

  // Either the memory access did not raise a signal or the signal handler did
  // not correctly return MemoryAccessResult::Failure.
  return MemoryAccessResult::Success;
#else
  USE(address);
  USE(access_size);
  return MemoryAccessResult::Success;
#endif  // VIXL_ENABLE_IMPLICIT_CHECKS
}

bool MetaDataDepot::MetaDataMTE::is_active = false;

void SimSystemRegister::SetBits(int msb, int lsb, uint32_t bits) {
  int width = msb - lsb + 1;
  VIXL_ASSERT(IsUintN(width, bits) || IsIntN(width, bits));

  bits <<= lsb;
  uint32_t mask = ((1 << width) - 1) << lsb;
  VIXL_ASSERT((mask & write_ignore_mask_) == 0);

  value_ = (value_ & ~mask) | (bits & mask);
}


SimSystemRegister SimSystemRegister::DefaultValueFor(SystemRegister id) {
  switch (id) {
    case NZCV:
      return SimSystemRegister(0x00000000, NZCVWriteIgnoreMask);
    case FPCR:
      return SimSystemRegister(0x00000000, FPCRWriteIgnoreMask);
    default:
      VIXL_UNREACHABLE();
      return SimSystemRegister();
  }
}

const Simulator::FormToVisitorFnMap* Simulator::GetFormToVisitorFnMap() {
  static const FormToVisitorFnMap form_to_visitor = {
      DEFAULT_FORM_TO_VISITOR_MAP(Simulator),
      SIM_AUD_VISITOR_MAP(Simulator),
      {"fmov_h_floatdp1"_h, &Simulator::SimulateFPConvert},
      {"fmov_s_floatdp1"_h, &Simulator::SimulateFPConvert},
      {"fmov_d_floatdp1"_h, &Simulator::SimulateFPConvert},
      {"fcvt_ds_floatdp1"_h, &Simulator::SimulateFPConvert},
      {"fcvt_sd_floatdp1"_h, &Simulator::SimulateFPConvert},
      {"fcvt_hs_floatdp1"_h, &Simulator::SimulateFPConvert},
      {"fcvt_sh_floatdp1"_h, &Simulator::SimulateFPConvert},
      {"fcvt_dh_floatdp1"_h, &Simulator::SimulateFPConvert},
      {"fcvt_hd_floatdp1"_h, &Simulator::SimulateFPConvert},
      {"bfcvt_bs_floatdp1"_h, &Simulator::SimulateFPConvert},
      {"frint32x_d_floatdp1"_h, &Simulator::SimulateFPRoundIntToSize},
      {"frint32x_s_floatdp1"_h, &Simulator::SimulateFPRoundIntToSize},
      {"frint32z_d_floatdp1"_h, &Simulator::SimulateFPRoundIntToSize},
      {"frint32z_s_floatdp1"_h, &Simulator::SimulateFPRoundIntToSize},
      {"frint64x_d_floatdp1"_h, &Simulator::SimulateFPRoundIntToSize},
      {"frint64x_s_floatdp1"_h, &Simulator::SimulateFPRoundIntToSize},
      {"frint64z_d_floatdp1"_h, &Simulator::SimulateFPRoundIntToSize},
      {"frint64z_s_floatdp1"_h, &Simulator::SimulateFPRoundIntToSize},
      {"frinta_d_floatdp1"_h, &Simulator::SimulateFPRoundInt},
      {"frinta_h_floatdp1"_h, &Simulator::SimulateFPRoundInt},
      {"frinta_s_floatdp1"_h, &Simulator::SimulateFPRoundInt},
      {"frinti_d_floatdp1"_h, &Simulator::SimulateFPRoundInt},
      {"frinti_h_floatdp1"_h, &Simulator::SimulateFPRoundInt},
      {"frinti_s_floatdp1"_h, &Simulator::SimulateFPRoundInt},
      {"frintm_d_floatdp1"_h, &Simulator::SimulateFPRoundInt},
      {"frintm_h_floatdp1"_h, &Simulator::SimulateFPRoundInt},
      {"frintm_s_floatdp1"_h, &Simulator::SimulateFPRoundInt},
      {"frintn_d_floatdp1"_h, &Simulator::SimulateFPRoundInt},
      {"frintn_h_floatdp1"_h, &Simulator::SimulateFPRoundInt},
      {"frintn_s_floatdp1"_h, &Simulator::SimulateFPRoundInt},
      {"frintp_d_floatdp1"_h, &Simulator::SimulateFPRoundInt},
      {"frintp_h_floatdp1"_h, &Simulator::SimulateFPRoundInt},
      {"frintp_s_floatdp1"_h, &Simulator::SimulateFPRoundInt},
      {"frintx_d_floatdp1"_h, &Simulator::SimulateFPRoundInt},
      {"frintx_h_floatdp1"_h, &Simulator::SimulateFPRoundInt},
      {"frintx_s_floatdp1"_h, &Simulator::SimulateFPRoundInt},
      {"frintz_d_floatdp1"_h, &Simulator::SimulateFPRoundInt},
      {"frintz_h_floatdp1"_h, &Simulator::SimulateFPRoundInt},
      {"frintz_s_floatdp1"_h, &Simulator::SimulateFPRoundInt},
      {"fcvtas_asimdmisc_r"_h, &Simulator::SimulateNEONFPConvert},
      {"fcvtau_asimdmisc_r"_h, &Simulator::SimulateNEONFPConvert},
      {"fcvtl_asimdmisc_l"_h, &Simulator::SimulateNEONFPConvert},
      {"fcvtms_asimdmisc_r"_h, &Simulator::SimulateNEONFPConvert},
      {"fcvtmu_asimdmisc_r"_h, &Simulator::SimulateNEONFPConvert},
      {"fcvtns_asimdmisc_r"_h, &Simulator::SimulateNEONFPConvert},
      {"fcvtnu_asimdmisc_r"_h, &Simulator::SimulateNEONFPConvert},
      {"fcvtn_asimdmisc_n"_h, &Simulator::SimulateNEONFPConvert},
      {"fcvtps_asimdmisc_r"_h, &Simulator::SimulateNEONFPConvert},
      {"fcvtpu_asimdmisc_r"_h, &Simulator::SimulateNEONFPConvert},
      {"fcvtxn_asimdmisc_n"_h, &Simulator::SimulateNEONFPConvert},
      {"fcvtzs_asimdmisc_r"_h, &Simulator::SimulateNEONFPConvert},
      {"fcvtzu_asimdmisc_r"_h, &Simulator::SimulateNEONFPConvert},
      {"bfcvtn_asimdmisc_4s"_h, &Simulator::SimulateNEONFPConvert},
      {"frint32x_asimdmisc_r"_h, &Simulator::SimulateNEONRoundIntToSize},
      {"frint32z_asimdmisc_r"_h, &Simulator::SimulateNEONRoundIntToSize},
      {"frint64x_asimdmisc_r"_h, &Simulator::SimulateNEONRoundIntToSize},
      {"frint64z_asimdmisc_r"_h, &Simulator::SimulateNEONRoundIntToSize},
      {"frinta_asimdmisc_r"_h, &Simulator::SimulateNEONRoundInt},
      {"frinti_asimdmisc_r"_h, &Simulator::SimulateNEONRoundInt},
      {"frintm_asimdmisc_r"_h, &Simulator::SimulateNEONRoundInt},
      {"frintn_asimdmisc_r"_h, &Simulator::SimulateNEONRoundInt},
      {"frintp_asimdmisc_r"_h, &Simulator::SimulateNEONRoundInt},
      {"frintx_asimdmisc_r"_h, &Simulator::SimulateNEONRoundInt},
      {"frintz_asimdmisc_r"_h, &Simulator::SimulateNEONRoundInt},
      {"fabs_asimdmisc_r"_h, &Simulator::SimulateNEONFP2RegMisc},
      {"fcmeq_asimdmisc_fz"_h, &Simulator::SimulateNEONFP2RegMisc},
      {"fcmge_asimdmisc_fz"_h, &Simulator::SimulateNEONFP2RegMisc},
      {"fcmgt_asimdmisc_fz"_h, &Simulator::SimulateNEONFP2RegMisc},
      {"fcmle_asimdmisc_fz"_h, &Simulator::SimulateNEONFP2RegMisc},
      {"fcmlt_asimdmisc_fz"_h, &Simulator::SimulateNEONFP2RegMisc},
      {"fneg_asimdmisc_r"_h, &Simulator::SimulateNEONFP2RegMisc},
      {"frecpe_asimdmisc_r"_h, &Simulator::SimulateNEONFP2RegMisc},
      {"frsqrte_asimdmisc_r"_h, &Simulator::SimulateNEONFP2RegMisc},
      {"fsqrt_asimdmisc_r"_h, &Simulator::SimulateNEONFP2RegMisc},
      {"scvtf_asimdmisc_r"_h, &Simulator::SimulateNEONFP2RegMisc},
      {"ucvtf_asimdmisc_r"_h, &Simulator::SimulateNEONFP2RegMisc},
      {"urecpe_asimdmisc_r"_h, &Simulator::SimulateNEONFP2RegMisc},
      {"ursqrte_asimdmisc_r"_h, &Simulator::SimulateNEONFP2RegMisc},
      {"smlal_asimdelem_l"_h, &Simulator::SimulateNEONMulByElementLong},
      {"smlsl_asimdelem_l"_h, &Simulator::SimulateNEONMulByElementLong},
      {"smull_asimdelem_l"_h, &Simulator::SimulateNEONMulByElementLong},
      {"sqdmlal_asimdelem_l"_h, &Simulator::SimulateNEONMulByElementLong},
      {"sqdmlsl_asimdelem_l"_h, &Simulator::SimulateNEONMulByElementLong},
      {"sqdmull_asimdelem_l"_h, &Simulator::SimulateNEONMulByElementLong},
      {"umlal_asimdelem_l"_h, &Simulator::SimulateNEONMulByElementLong},
      {"umlsl_asimdelem_l"_h, &Simulator::SimulateNEONMulByElementLong},
      {"umull_asimdelem_l"_h, &Simulator::SimulateNEONMulByElementLong},
      {"fcmla_asimdelem_c_h"_h, &Simulator::SimulateNEONComplexMulByElement},
      {"fcmla_asimdelem_c_s"_h, &Simulator::SimulateNEONComplexMulByElement},
      {"fmlal2_asimdelem_lh"_h, &Simulator::SimulateNEONFPMulByElementLong},
      {"fmlal_asimdelem_lh"_h, &Simulator::SimulateNEONFPMulByElementLong},
      {"fmlsl2_asimdelem_lh"_h, &Simulator::SimulateNEONFPMulByElementLong},
      {"fmlsl_asimdelem_lh"_h, &Simulator::SimulateNEONFPMulByElementLong},
      {"fmla_asimdelem_rh_h"_h, &Simulator::SimulateNEONFPMulByElement},
      {"fmls_asimdelem_rh_h"_h, &Simulator::SimulateNEONFPMulByElement},
      {"fmulx_asimdelem_rh_h"_h, &Simulator::SimulateNEONFPMulByElement},
      {"fmul_asimdelem_rh_h"_h, &Simulator::SimulateNEONFPMulByElement},
      {"fmla_asimdelem_r_sd"_h, &Simulator::SimulateNEONFPMulByElement},
      {"fmls_asimdelem_r_sd"_h, &Simulator::SimulateNEONFPMulByElement},
      {"fmulx_asimdelem_r_sd"_h, &Simulator::SimulateNEONFPMulByElement},
      {"fmul_asimdelem_r_sd"_h, &Simulator::SimulateNEONFPMulByElement},
      {"sdot_asimdelem_d"_h, &Simulator::SimulateNEONDotProdByElement},
      {"udot_asimdelem_d"_h, &Simulator::SimulateNEONDotProdByElement},
      {"adclb_z_zzz"_h, &Simulator::SimulateSVEAddSubCarry},
      {"adclt_z_zzz"_h, &Simulator::SimulateSVEAddSubCarry},
      {"addhnb_z_zz"_h, &Simulator::SimulateSVEAddSubHigh},
      {"addhnt_z_zz"_h, &Simulator::SimulateSVEAddSubHigh},
      {"addp_z_p_zz"_h, &Simulator::SimulateSVEIntArithPair},
      {"bcax_z_zzz"_h, &Simulator::SimulateSVEBitwiseTernary},
      {"bdep_z_zz"_h, &Simulator::Simulate_ZdT_ZnT_ZmT},
      {"bext_z_zz"_h, &Simulator::Simulate_ZdT_ZnT_ZmT},
      {"bgrp_z_zz"_h, &Simulator::Simulate_ZdT_ZnT_ZmT},
      {"bsl1n_z_zzz"_h, &Simulator::SimulateSVEBitwiseTernary},
      {"bsl2n_z_zzz"_h, &Simulator::SimulateSVEBitwiseTernary},
      {"bsl_z_zzz"_h, &Simulator::SimulateSVEBitwiseTernary},
      {"cadd_z_zz"_h, &Simulator::Simulate_ZdnT_ZdnT_ZmT_const},
      {"cdot_z_zzz"_h, &Simulator::SimulateSVEComplexDotProduct},
      {"cdot_z_zzzi_d"_h, &Simulator::SimulateSVEComplexDotProduct},
      {"cdot_z_zzzi_s"_h, &Simulator::SimulateSVEComplexDotProduct},
      {"cmla_z_zzz"_h, &Simulator::SimulateSVEComplexIntMulAdd},
      {"cmla_z_zzzi_h"_h, &Simulator::SimulateSVEComplexIntMulAdd},
      {"cmla_z_zzzi_s"_h, &Simulator::SimulateSVEComplexIntMulAdd},
      {"eor3_z_zzz"_h, &Simulator::SimulateSVEBitwiseTernary},
      {"eorbt_z_zz"_h, &Simulator::Simulate_ZdT_ZnT_ZmT},
      {"eortb_z_zz"_h, &Simulator::Simulate_ZdT_ZnT_ZmT},
      {"ext_z_zi_con"_h, &Simulator::Simulate_ZdB_Zn1B_Zn2B_imm},
      {"faddp_z_p_zz"_h, &Simulator::Simulate_ZdnT_PgM_ZdnT_ZmT},
      {"fcvtlt_z_p_z_h2s"_h, &Simulator::SimulateSVEFPConvertLong},
      {"fcvtlt_z_p_z_s2d"_h, &Simulator::SimulateSVEFPConvertLong},
      {"fcvtnt_z_p_z_d2s"_h, &Simulator::Simulate_ZdS_PgM_ZnD},
      {"bfcvt_z_p_z_s2bf"_h, &Simulator::Simulate_ZdH_PgM_ZnS},
      {"bfcvtnt_z_p_z_s2bf"_h, &Simulator::Simulate_ZdH_PgM_ZnS},
      {"fcvtnt_z_p_z_s2h"_h, &Simulator::Simulate_ZdH_PgM_ZnS},
      {"fcvtx_z_p_z_d2s"_h, &Simulator::Simulate_ZdS_PgM_ZnD},
      {"fcvtxnt_z_p_z_d2s"_h, &Simulator::Simulate_ZdS_PgM_ZnD},
      {"flogb_z_p_z"_h, &Simulator::Simulate_ZdT_PgM_ZnT},
      {"fmaxnmp_z_p_zz"_h, &Simulator::Simulate_ZdnT_PgM_ZdnT_ZmT},
      {"fmaxp_z_p_zz"_h, &Simulator::Simulate_ZdnT_PgM_ZdnT_ZmT},
      {"fminnmp_z_p_zz"_h, &Simulator::Simulate_ZdnT_PgM_ZdnT_ZmT},
      {"fminp_z_p_zz"_h, &Simulator::Simulate_ZdnT_PgM_ZdnT_ZmT},
      {"fmlalb_z_zzz"_h, &Simulator::Simulate_ZdaS_ZnH_ZmH},
      {"fmlalb_z_zzzi_s"_h, &Simulator::Simulate_ZdaS_ZnH_ZmH_imm},
      {"fmlalt_z_zzz"_h, &Simulator::Simulate_ZdaS_ZnH_ZmH},
      {"fmlalt_z_zzzi_s"_h, &Simulator::Simulate_ZdaS_ZnH_ZmH_imm},
      {"fmlslb_z_zzz"_h, &Simulator::Simulate_ZdaS_ZnH_ZmH},
      {"fmlslb_z_zzzi_s"_h, &Simulator::Simulate_ZdaS_ZnH_ZmH_imm},
      {"fmlslt_z_zzz"_h, &Simulator::Simulate_ZdaS_ZnH_ZmH},
      {"fmlslt_z_zzzi_s"_h, &Simulator::Simulate_ZdaS_ZnH_ZmH_imm},
      {"histcnt_z_p_zz"_h, &Simulator::Simulate_ZdT_PgZ_ZnT_ZmT},
      {"histseg_z_zz"_h, &Simulator::Simulate_ZdB_ZnB_ZmB},
      {"ldnt1b_z_p_ar_d_64_unscaled"_h, &Simulator::Simulate_ZtD_PgZ_ZnD_Xm},
      {"ldnt1b_z_p_ar_s_x32_unscaled"_h, &Simulator::Simulate_ZtS_PgZ_ZnS_Xm},
      {"ldnt1d_z_p_ar_d_64_unscaled"_h, &Simulator::Simulate_ZtD_PgZ_ZnD_Xm},
      {"ldnt1h_z_p_ar_d_64_unscaled"_h, &Simulator::Simulate_ZtD_PgZ_ZnD_Xm},
      {"ldnt1h_z_p_ar_s_x32_unscaled"_h, &Simulator::Simulate_ZtS_PgZ_ZnS_Xm},
      {"ldnt1sb_z_p_ar_d_64_unscaled"_h, &Simulator::Simulate_ZtD_PgZ_ZnD_Xm},
      {"ldnt1sb_z_p_ar_s_x32_unscaled"_h, &Simulator::Simulate_ZtS_PgZ_ZnS_Xm},
      {"ldnt1sh_z_p_ar_d_64_unscaled"_h, &Simulator::Simulate_ZtD_PgZ_ZnD_Xm},
      {"ldnt1sh_z_p_ar_s_x32_unscaled"_h, &Simulator::Simulate_ZtS_PgZ_ZnS_Xm},
      {"ldnt1sw_z_p_ar_d_64_unscaled"_h, &Simulator::Simulate_ZtD_PgZ_ZnD_Xm},
      {"ldnt1w_z_p_ar_d_64_unscaled"_h, &Simulator::Simulate_ZtD_PgZ_ZnD_Xm},
      {"ldnt1w_z_p_ar_s_x32_unscaled"_h, &Simulator::Simulate_ZtS_PgZ_ZnS_Xm},
      {"match_p_p_zz"_h, &Simulator::Simulate_PdT_PgZ_ZnT_ZmT},
      {"mla_z_zzzi_d"_h, &Simulator::SimulateSVEMlaMlsIndex},
      {"mla_z_zzzi_h"_h, &Simulator::SimulateSVEMlaMlsIndex},
      {"mla_z_zzzi_s"_h, &Simulator::SimulateSVEMlaMlsIndex},
      {"mls_z_zzzi_d"_h, &Simulator::SimulateSVEMlaMlsIndex},
      {"mls_z_zzzi_h"_h, &Simulator::SimulateSVEMlaMlsIndex},
      {"mls_z_zzzi_s"_h, &Simulator::SimulateSVEMlaMlsIndex},
      {"mul_z_zz"_h, &Simulator::Simulate_ZdT_ZnT_ZmT},
      {"mul_z_zzi_d"_h, &Simulator::SimulateSVEMulIndex},
      {"mul_z_zzi_h"_h, &Simulator::SimulateSVEMulIndex},
      {"mul_z_zzi_s"_h, &Simulator::SimulateSVEMulIndex},
      {"nbsl_z_zzz"_h, &Simulator::SimulateSVEBitwiseTernary},
      {"nmatch_p_p_zz"_h, &Simulator::Simulate_PdT_PgZ_ZnT_ZmT},
      {"pmul_z_zz"_h, &Simulator::Simulate_ZdB_ZnB_ZmB},
      {"pmullb_z_zz"_h, &Simulator::SimulateSVEIntMulLongVec},
      {"pmullt_z_zz"_h, &Simulator::SimulateSVEIntMulLongVec},
      {"raddhnb_z_zz"_h, &Simulator::SimulateSVEAddSubHigh},
      {"raddhnt_z_zz"_h, &Simulator::SimulateSVEAddSubHigh},
      {"rshrnb_z_zi"_h, &Simulator::SimulateSVENarrow},
      {"rshrnt_z_zi"_h, &Simulator::SimulateSVENarrow},
      {"rsubhnb_z_zz"_h, &Simulator::SimulateSVEAddSubHigh},
      {"rsubhnt_z_zz"_h, &Simulator::SimulateSVEAddSubHigh},
      {"saba_z_zzz"_h, &Simulator::Simulate_ZdaT_ZnT_ZmT},
      {"sabalb_z_zzz"_h, &Simulator::SimulateSVEInterleavedArithLong},
      {"sabalt_z_zzz"_h, &Simulator::SimulateSVEInterleavedArithLong},
      {"sabdlb_z_zz"_h, &Simulator::SimulateSVEInterleavedArithLong},
      {"sabdlt_z_zz"_h, &Simulator::SimulateSVEInterleavedArithLong},
      {"sadalp_z_p_z"_h, &Simulator::Simulate_ZdaT_PgM_ZnTb},
      {"saddlb_z_zz"_h, &Simulator::SimulateSVEInterleavedArithLong},
      {"saddlbt_z_zz"_h, &Simulator::SimulateSVEInterleavedArithLong},
      {"saddlt_z_zz"_h, &Simulator::SimulateSVEInterleavedArithLong},
      {"saddwb_z_zz"_h, &Simulator::Simulate_ZdT_ZnT_ZmTb},
      {"saddwt_z_zz"_h, &Simulator::Simulate_ZdT_ZnT_ZmTb},
      {"sbclb_z_zzz"_h, &Simulator::SimulateSVEAddSubCarry},
      {"sbclt_z_zzz"_h, &Simulator::SimulateSVEAddSubCarry},
      {"shadd_z_p_zz"_h, &Simulator::SimulateSVEHalvingAddSub},
      {"shrnb_z_zi"_h, &Simulator::SimulateSVENarrow},
      {"shrnt_z_zi"_h, &Simulator::SimulateSVENarrow},
      {"shsub_z_p_zz"_h, &Simulator::SimulateSVEHalvingAddSub},
      {"shsubr_z_p_zz"_h, &Simulator::SimulateSVEHalvingAddSub},
      {"sli_z_zzi"_h, &Simulator::Simulate_ZdT_ZnT_const},
      {"smaxp_z_p_zz"_h, &Simulator::SimulateSVEIntArithPair},
      {"sminp_z_p_zz"_h, &Simulator::SimulateSVEIntArithPair},
      {"smlalb_z_zzz"_h, &Simulator::Simulate_ZdaT_ZnTb_ZmTb},
      {"smlalb_z_zzzi_d"_h, &Simulator::SimulateSVESaturatingIntMulLongIdx},
      {"smlalb_z_zzzi_s"_h, &Simulator::SimulateSVESaturatingIntMulLongIdx},
      {"smlalt_z_zzz"_h, &Simulator::Simulate_ZdaT_ZnTb_ZmTb},
      {"smlalt_z_zzzi_d"_h, &Simulator::SimulateSVESaturatingIntMulLongIdx},
      {"smlalt_z_zzzi_s"_h, &Simulator::SimulateSVESaturatingIntMulLongIdx},
      {"smlslb_z_zzz"_h, &Simulator::Simulate_ZdaT_ZnTb_ZmTb},
      {"smlslb_z_zzzi_d"_h, &Simulator::SimulateSVESaturatingIntMulLongIdx},
      {"smlslb_z_zzzi_s"_h, &Simulator::SimulateSVESaturatingIntMulLongIdx},
      {"smlslt_z_zzz"_h, &Simulator::Simulate_ZdaT_ZnTb_ZmTb},
      {"smlslt_z_zzzi_d"_h, &Simulator::SimulateSVESaturatingIntMulLongIdx},
      {"smlslt_z_zzzi_s"_h, &Simulator::SimulateSVESaturatingIntMulLongIdx},
      {"smulh_z_zz"_h, &Simulator::Simulate_ZdT_ZnT_ZmT},
      {"smullb_z_zz"_h, &Simulator::SimulateSVEIntMulLongVec},
      {"smullb_z_zzi_d"_h, &Simulator::SimulateSVESaturatingIntMulLongIdx},
      {"smullb_z_zzi_s"_h, &Simulator::SimulateSVESaturatingIntMulLongIdx},
      {"smullt_z_zz"_h, &Simulator::SimulateSVEIntMulLongVec},
      {"smullt_z_zzi_d"_h, &Simulator::SimulateSVESaturatingIntMulLongIdx},
      {"smullt_z_zzi_s"_h, &Simulator::SimulateSVESaturatingIntMulLongIdx},
      {"splice_z_p_zz_con"_h, &Simulator::VisitSVEVectorSplice},
      {"sqabs_z_p_z"_h, &Simulator::Simulate_ZdT_PgM_ZnT},
      {"sqadd_z_p_zz"_h, &Simulator::SimulateSVESaturatingArithmetic},
      {"sqcadd_z_zz"_h, &Simulator::Simulate_ZdnT_ZdnT_ZmT_const},
      {"sqdmlalb_z_zzz"_h, &Simulator::Simulate_ZdaT_ZnTb_ZmTb},
      {"sqdmlalb_z_zzzi_d"_h, &Simulator::Simulate_ZdaD_ZnS_ZmS_imm},
      {"sqdmlalb_z_zzzi_s"_h, &Simulator::Simulate_ZdaS_ZnH_ZmH_imm},
      {"sqdmlalbt_z_zzz"_h, &Simulator::Simulate_ZdaT_ZnTb_ZmTb},
      {"sqdmlalt_z_zzz"_h, &Simulator::Simulate_ZdaT_ZnTb_ZmTb},
      {"sqdmlalt_z_zzzi_d"_h, &Simulator::Simulate_ZdaD_ZnS_ZmS_imm},
      {"sqdmlalt_z_zzzi_s"_h, &Simulator::Simulate_ZdaS_ZnH_ZmH_imm},
      {"sqdmlslb_z_zzz"_h, &Simulator::Simulate_ZdaT_ZnTb_ZmTb},
      {"sqdmlslb_z_zzzi_d"_h, &Simulator::Simulate_ZdaD_ZnS_ZmS_imm},
      {"sqdmlslb_z_zzzi_s"_h, &Simulator::Simulate_ZdaS_ZnH_ZmH_imm},
      {"sqdmlslbt_z_zzz"_h, &Simulator::Simulate_ZdaT_ZnTb_ZmTb},
      {"sqdmlslt_z_zzz"_h, &Simulator::Simulate_ZdaT_ZnTb_ZmTb},
      {"sqdmlslt_z_zzzi_d"_h, &Simulator::Simulate_ZdaD_ZnS_ZmS_imm},
      {"sqdmlslt_z_zzzi_s"_h, &Simulator::Simulate_ZdaS_ZnH_ZmH_imm},
      {"sqdmulh_z_zz"_h, &Simulator::Simulate_ZdT_ZnT_ZmT},
      {"sqdmulh_z_zzi_d"_h, &Simulator::SimulateSVESaturatingMulHighIndex},
      {"sqdmulh_z_zzi_h"_h, &Simulator::SimulateSVESaturatingMulHighIndex},
      {"sqdmulh_z_zzi_s"_h, &Simulator::SimulateSVESaturatingMulHighIndex},
      {"sqdmullb_z_zz"_h, &Simulator::SimulateSVEIntMulLongVec},
      {"sqdmullb_z_zzi_d"_h, &Simulator::SimulateSVESaturatingIntMulLongIdx},
      {"sqdmullb_z_zzi_s"_h, &Simulator::SimulateSVESaturatingIntMulLongIdx},
      {"sqdmullt_z_zz"_h, &Simulator::SimulateSVEIntMulLongVec},
      {"sqdmullt_z_zzi_d"_h, &Simulator::SimulateSVESaturatingIntMulLongIdx},
      {"sqdmullt_z_zzi_s"_h, &Simulator::SimulateSVESaturatingIntMulLongIdx},
      {"sqneg_z_p_z"_h, &Simulator::Simulate_ZdT_PgM_ZnT},
      {"sqrdcmlah_z_zzz"_h, &Simulator::SimulateSVEComplexIntMulAdd},
      {"sqrdcmlah_z_zzzi_h"_h, &Simulator::SimulateSVEComplexIntMulAdd},
      {"sqrdcmlah_z_zzzi_s"_h, &Simulator::SimulateSVEComplexIntMulAdd},
      {"sqrdmlah_z_zzz"_h, &Simulator::SimulateSVESaturatingMulAddHigh},
      {"sqrdmlah_z_zzzi_d"_h, &Simulator::SimulateSVESaturatingMulAddHigh},
      {"sqrdmlah_z_zzzi_h"_h, &Simulator::SimulateSVESaturatingMulAddHigh},
      {"sqrdmlah_z_zzzi_s"_h, &Simulator::SimulateSVESaturatingMulAddHigh},
      {"sqrdmlsh_z_zzz"_h, &Simulator::SimulateSVESaturatingMulAddHigh},
      {"sqrdmlsh_z_zzzi_d"_h, &Simulator::SimulateSVESaturatingMulAddHigh},
      {"sqrdmlsh_z_zzzi_h"_h, &Simulator::SimulateSVESaturatingMulAddHigh},
      {"sqrdmlsh_z_zzzi_s"_h, &Simulator::SimulateSVESaturatingMulAddHigh},
      {"sqrdmulh_z_zz"_h, &Simulator::Simulate_ZdT_ZnT_ZmT},
      {"sqrdmulh_z_zzi_d"_h, &Simulator::SimulateSVESaturatingMulHighIndex},
      {"sqrdmulh_z_zzi_h"_h, &Simulator::SimulateSVESaturatingMulHighIndex},
      {"sqrdmulh_z_zzi_s"_h, &Simulator::SimulateSVESaturatingMulHighIndex},
      {"sqrshl_z_p_zz"_h, &Simulator::VisitSVEBitwiseShiftByVector_Predicated},
      {"sqrshlr_z_p_zz"_h, &Simulator::VisitSVEBitwiseShiftByVector_Predicated},
      {"sqrshrnb_z_zi"_h, &Simulator::SimulateSVENarrow},
      {"sqrshrnt_z_zi"_h, &Simulator::SimulateSVENarrow},
      {"sqrshrunb_z_zi"_h, &Simulator::SimulateSVENarrow},
      {"sqrshrunt_z_zi"_h, &Simulator::SimulateSVENarrow},
      {"sqshl_z_p_zi"_h, &Simulator::Simulate_ZdnT_PgM_ZdnT_const},
      {"sqshl_z_p_zz"_h, &Simulator::VisitSVEBitwiseShiftByVector_Predicated},
      {"sqshlr_z_p_zz"_h, &Simulator::VisitSVEBitwiseShiftByVector_Predicated},
      {"sqshlu_z_p_zi"_h, &Simulator::Simulate_ZdnT_PgM_ZdnT_const},
      {"sqshrnb_z_zi"_h, &Simulator::SimulateSVENarrow},
      {"sqshrnt_z_zi"_h, &Simulator::SimulateSVENarrow},
      {"sqshrunb_z_zi"_h, &Simulator::SimulateSVENarrow},
      {"sqshrunt_z_zi"_h, &Simulator::SimulateSVENarrow},
      {"sqsub_z_p_zz"_h, &Simulator::SimulateSVESaturatingArithmetic},
      {"sqsubr_z_p_zz"_h, &Simulator::SimulateSVESaturatingArithmetic},
      {"sqxtnb_z_zz"_h, &Simulator::SimulateSVENarrow},
      {"sqxtnt_z_zz"_h, &Simulator::SimulateSVENarrow},
      {"sqxtunb_z_zz"_h, &Simulator::SimulateSVENarrow},
      {"sqxtunt_z_zz"_h, &Simulator::SimulateSVENarrow},
      {"srhadd_z_p_zz"_h, &Simulator::SimulateSVEHalvingAddSub},
      {"sri_z_zzi"_h, &Simulator::Simulate_ZdT_ZnT_const},
      {"srshl_z_p_zz"_h, &Simulator::VisitSVEBitwiseShiftByVector_Predicated},
      {"srshlr_z_p_zz"_h, &Simulator::VisitSVEBitwiseShiftByVector_Predicated},
      {"srshr_z_p_zi"_h, &Simulator::Simulate_ZdnT_PgM_ZdnT_const},
      {"srsra_z_zi"_h, &Simulator::Simulate_ZdaT_ZnT_const},
      {"sshllb_z_zi"_h, &Simulator::SimulateSVEShiftLeftImm},
      {"sshllt_z_zi"_h, &Simulator::SimulateSVEShiftLeftImm},
      {"ssra_z_zi"_h, &Simulator::Simulate_ZdaT_ZnT_const},
      {"ssublb_z_zz"_h, &Simulator::SimulateSVEInterleavedArithLong},
      {"ssublbt_z_zz"_h, &Simulator::SimulateSVEInterleavedArithLong},
      {"ssublt_z_zz"_h, &Simulator::SimulateSVEInterleavedArithLong},
      {"ssubltb_z_zz"_h, &Simulator::SimulateSVEInterleavedArithLong},
      {"ssubwb_z_zz"_h, &Simulator::Simulate_ZdT_ZnT_ZmTb},
      {"ssubwt_z_zz"_h, &Simulator::Simulate_ZdT_ZnT_ZmTb},
      {"stnt1b_z_p_ar_d_64_unscaled"_h, &Simulator::Simulate_ZtD_Pg_ZnD_Xm},
      {"stnt1b_z_p_ar_s_x32_unscaled"_h, &Simulator::Simulate_ZtS_Pg_ZnS_Xm},
      {"stnt1d_z_p_ar_d_64_unscaled"_h, &Simulator::Simulate_ZtD_Pg_ZnD_Xm},
      {"stnt1h_z_p_ar_d_64_unscaled"_h, &Simulator::Simulate_ZtD_Pg_ZnD_Xm},
      {"stnt1h_z_p_ar_s_x32_unscaled"_h, &Simulator::Simulate_ZtS_Pg_ZnS_Xm},
      {"stnt1w_z_p_ar_d_64_unscaled"_h, &Simulator::Simulate_ZtD_Pg_ZnD_Xm},
      {"stnt1w_z_p_ar_s_x32_unscaled"_h, &Simulator::Simulate_ZtS_Pg_ZnS_Xm},
      {"subhnb_z_zz"_h, &Simulator::SimulateSVEAddSubHigh},
      {"subhnt_z_zz"_h, &Simulator::SimulateSVEAddSubHigh},
      {"suqadd_z_p_zz"_h, &Simulator::SimulateSVESaturatingArithmetic},
      {"tbl_z_zz_2"_h, &Simulator::VisitSVETableLookup},
      {"tbx_z_zz"_h, &Simulator::VisitSVETableLookup},
      {"uaba_z_zzz"_h, &Simulator::Simulate_ZdaT_ZnT_ZmT},
      {"uabalb_z_zzz"_h, &Simulator::SimulateSVEInterleavedArithLong},
      {"uabalt_z_zzz"_h, &Simulator::SimulateSVEInterleavedArithLong},
      {"uabdlb_z_zz"_h, &Simulator::SimulateSVEInterleavedArithLong},
      {"uabdlt_z_zz"_h, &Simulator::SimulateSVEInterleavedArithLong},
      {"uadalp_z_p_z"_h, &Simulator::Simulate_ZdaT_PgM_ZnTb},
      {"uaddlb_z_zz"_h, &Simulator::SimulateSVEInterleavedArithLong},
      {"uaddlt_z_zz"_h, &Simulator::SimulateSVEInterleavedArithLong},
      {"uaddwb_z_zz"_h, &Simulator::Simulate_ZdT_ZnT_ZmTb},
      {"uaddwt_z_zz"_h, &Simulator::Simulate_ZdT_ZnT_ZmTb},
      {"uhadd_z_p_zz"_h, &Simulator::SimulateSVEHalvingAddSub},
      {"uhsub_z_p_zz"_h, &Simulator::SimulateSVEHalvingAddSub},
      {"uhsubr_z_p_zz"_h, &Simulator::SimulateSVEHalvingAddSub},
      {"umaxp_z_p_zz"_h, &Simulator::SimulateSVEIntArithPair},
      {"uminp_z_p_zz"_h, &Simulator::SimulateSVEIntArithPair},
      {"umlalb_z_zzz"_h, &Simulator::Simulate_ZdaT_ZnTb_ZmTb},
      {"umlalb_z_zzzi_d"_h, &Simulator::SimulateSVESaturatingIntMulLongIdx},
      {"umlalb_z_zzzi_s"_h, &Simulator::SimulateSVESaturatingIntMulLongIdx},
      {"umlalt_z_zzz"_h, &Simulator::Simulate_ZdaT_ZnTb_ZmTb},
      {"umlalt_z_zzzi_d"_h, &Simulator::SimulateSVESaturatingIntMulLongIdx},
      {"umlalt_z_zzzi_s"_h, &Simulator::SimulateSVESaturatingIntMulLongIdx},
      {"umlslb_z_zzz"_h, &Simulator::Simulate_ZdaT_ZnTb_ZmTb},
      {"umlslb_z_zzzi_d"_h, &Simulator::SimulateSVESaturatingIntMulLongIdx},
      {"umlslb_z_zzzi_s"_h, &Simulator::SimulateSVESaturatingIntMulLongIdx},
      {"umlslt_z_zzz"_h, &Simulator::Simulate_ZdaT_ZnTb_ZmTb},
      {"umlslt_z_zzzi_d"_h, &Simulator::SimulateSVESaturatingIntMulLongIdx},
      {"umlslt_z_zzzi_s"_h, &Simulator::SimulateSVESaturatingIntMulLongIdx},
      {"umulh_z_zz"_h, &Simulator::Simulate_ZdT_ZnT_ZmT},
      {"umullb_z_zz"_h, &Simulator::SimulateSVEIntMulLongVec},
      {"umullb_z_zzi_d"_h, &Simulator::SimulateSVESaturatingIntMulLongIdx},
      {"umullb_z_zzi_s"_h, &Simulator::SimulateSVESaturatingIntMulLongIdx},
      {"umullt_z_zz"_h, &Simulator::SimulateSVEIntMulLongVec},
      {"umullt_z_zzi_d"_h, &Simulator::SimulateSVESaturatingIntMulLongIdx},
      {"umullt_z_zzi_s"_h, &Simulator::SimulateSVESaturatingIntMulLongIdx},
      {"uqadd_z_p_zz"_h, &Simulator::SimulateSVESaturatingArithmetic},
      {"uqrshl_z_p_zz"_h, &Simulator::VisitSVEBitwiseShiftByVector_Predicated},
      {"uqrshlr_z_p_zz"_h, &Simulator::VisitSVEBitwiseShiftByVector_Predicated},
      {"uqrshrnb_z_zi"_h, &Simulator::SimulateSVENarrow},
      {"uqrshrnt_z_zi"_h, &Simulator::SimulateSVENarrow},
      {"uqshl_z_p_zi"_h, &Simulator::Simulate_ZdnT_PgM_ZdnT_const},
      {"uqshl_z_p_zz"_h, &Simulator::VisitSVEBitwiseShiftByVector_Predicated},
      {"uqshlr_z_p_zz"_h, &Simulator::VisitSVEBitwiseShiftByVector_Predicated},
      {"uqshrnb_z_zi"_h, &Simulator::SimulateSVENarrow},
      {"uqshrnt_z_zi"_h, &Simulator::SimulateSVENarrow},
      {"uqsub_z_p_zz"_h, &Simulator::SimulateSVESaturatingArithmetic},
      {"uqsubr_z_p_zz"_h, &Simulator::SimulateSVESaturatingArithmetic},
      {"uqxtnb_z_zz"_h, &Simulator::SimulateSVENarrow},
      {"uqxtnt_z_zz"_h, &Simulator::SimulateSVENarrow},
      {"urecpe_z_p_z"_h, &Simulator::Simulate_ZdS_PgM_ZnS},
      {"urhadd_z_p_zz"_h, &Simulator::SimulateSVEHalvingAddSub},
      {"urshl_z_p_zz"_h, &Simulator::VisitSVEBitwiseShiftByVector_Predicated},
      {"urshlr_z_p_zz"_h, &Simulator::VisitSVEBitwiseShiftByVector_Predicated},
      {"urshr_z_p_zi"_h, &Simulator::Simulate_ZdnT_PgM_ZdnT_const},
      {"ursqrte_z_p_z"_h, &Simulator::Simulate_ZdS_PgM_ZnS},
      {"ursra_z_zi"_h, &Simulator::Simulate_ZdaT_ZnT_const},
      {"ushllb_z_zi"_h, &Simulator::SimulateSVEShiftLeftImm},
      {"ushllt_z_zi"_h, &Simulator::SimulateSVEShiftLeftImm},
      {"usqadd_z_p_zz"_h, &Simulator::SimulateSVESaturatingArithmetic},
      {"usra_z_zi"_h, &Simulator::Simulate_ZdaT_ZnT_const},
      {"usublb_z_zz"_h, &Simulator::SimulateSVEInterleavedArithLong},
      {"usublt_z_zz"_h, &Simulator::SimulateSVEInterleavedArithLong},
      {"usubwb_z_zz"_h, &Simulator::Simulate_ZdT_ZnT_ZmTb},
      {"usubwt_z_zz"_h, &Simulator::Simulate_ZdT_ZnT_ZmTb},
      {"whilege_p_p_rr"_h, &Simulator::VisitSVEIntCompareScalarCountAndLimit},
      {"whilegt_p_p_rr"_h, &Simulator::VisitSVEIntCompareScalarCountAndLimit},
      {"whilehi_p_p_rr"_h, &Simulator::VisitSVEIntCompareScalarCountAndLimit},
      {"whilehs_p_p_rr"_h, &Simulator::VisitSVEIntCompareScalarCountAndLimit},
      {"whilerw_p_rr"_h, &Simulator::Simulate_PdT_Xn_Xm},
      {"whilewr_p_rr"_h, &Simulator::Simulate_PdT_Xn_Xm},
      {"xar_z_zzi"_h, &Simulator::SimulateSVEExclusiveOrRotate},
      {"smmla_z_zzz"_h, &Simulator::SimulateMatrixMul},
      {"ummla_z_zzz"_h, &Simulator::SimulateMatrixMul},
      {"usmmla_z_zzz"_h, &Simulator::SimulateMatrixMul},
      {"smmla_asimdsame2_g"_h, &Simulator::SimulateMatrixMul},
      {"ummla_asimdsame2_g"_h, &Simulator::SimulateMatrixMul},
      {"usmmla_asimdsame2_g"_h, &Simulator::SimulateMatrixMul},
      {"fmmla_z_zzz_s"_h, &Simulator::SimulateSVEFPMatrixMul},
      {"fmmla_z_zzz_d"_h, &Simulator::SimulateSVEFPMatrixMul},
      {"ld1row_z_p_bi_u32"_h,
       &Simulator::VisitSVELoadAndBroadcastQOWord_ScalarPlusImm},
      {"ld1row_z_p_br_contiguous"_h,
       &Simulator::VisitSVELoadAndBroadcastQOWord_ScalarPlusScalar},
      {"ld1rod_z_p_bi_u64"_h,
       &Simulator::VisitSVELoadAndBroadcastQOWord_ScalarPlusImm},
      {"ld1rod_z_p_br_contiguous"_h,
       &Simulator::VisitSVELoadAndBroadcastQOWord_ScalarPlusScalar},
      {"ld1rob_z_p_bi_u8"_h,
       &Simulator::VisitSVELoadAndBroadcastQOWord_ScalarPlusImm},
      {"ld1rob_z_p_br_contiguous"_h,
       &Simulator::VisitSVELoadAndBroadcastQOWord_ScalarPlusScalar},
      {"ld1roh_z_p_bi_u16"_h,
       &Simulator::VisitSVELoadAndBroadcastQOWord_ScalarPlusImm},
      {"ld1roh_z_p_br_contiguous"_h,
       &Simulator::VisitSVELoadAndBroadcastQOWord_ScalarPlusScalar},
      {"usdot_z_zzz_s"_h, &Simulator::VisitSVEIntMulAddUnpredicated},
      {"sudot_z_zzzi_s"_h, &Simulator::VisitSVEMulIndex},
      {"usdot_z_zzzi_s"_h, &Simulator::VisitSVEMulIndex},
      {"usdot_asimdsame2_d"_h, &Simulator::VisitNEON3SameExtra},
      {"sudot_asimdelem_d"_h, &Simulator::SimulateNEONDotProdByElement},
      {"usdot_asimdelem_d"_h, &Simulator::SimulateNEONDotProdByElement},
      {"addg_64_addsub_immtags"_h, &Simulator::SimulateMTEAddSubTag},
      {"gmi_64g_dp_2src"_h, &Simulator::SimulateMTETagMaskInsert},
      {"irg_64i_dp_2src"_h, &Simulator::Simulate_XdSP_XnSP_Xm},
      {"ldg_64loffset_ldsttags"_h, &Simulator::SimulateMTELoadTag},
      {"st2g_64soffset_ldsttags"_h, &Simulator::Simulator::SimulateMTEStoreTag},
      {"st2g_64spost_ldsttags"_h, &Simulator::Simulator::SimulateMTEStoreTag},
      {"st2g_64spre_ldsttags"_h, &Simulator::Simulator::SimulateMTEStoreTag},
      {"stgp_64_ldstpair_off"_h, &Simulator::SimulateMTEStoreTagPair},
      {"stgp_64_ldstpair_post"_h, &Simulator::SimulateMTEStoreTagPair},
      {"stgp_64_ldstpair_pre"_h, &Simulator::SimulateMTEStoreTagPair},
      {"stg_64soffset_ldsttags"_h, &Simulator::Simulator::SimulateMTEStoreTag},
      {"stg_64spost_ldsttags"_h, &Simulator::Simulator::SimulateMTEStoreTag},
      {"stg_64spre_ldsttags"_h, &Simulator::Simulator::SimulateMTEStoreTag},
      {"stz2g_64soffset_ldsttags"_h,
       &Simulator::Simulator::SimulateMTEStoreTag},
      {"stz2g_64spost_ldsttags"_h, &Simulator::Simulator::SimulateMTEStoreTag},
      {"stz2g_64spre_ldsttags"_h, &Simulator::Simulator::SimulateMTEStoreTag},
      {"stzg_64soffset_ldsttags"_h, &Simulator::Simulator::SimulateMTEStoreTag},
      {"stzg_64spost_ldsttags"_h, &Simulator::Simulator::SimulateMTEStoreTag},
      {"stzg_64spre_ldsttags"_h, &Simulator::Simulator::SimulateMTEStoreTag},
      {"subg_64_addsub_immtags"_h, &Simulator::SimulateMTEAddSubTag},
      {"subps_64s_dp_2src"_h, &Simulator::SimulateMTESubPointer},
      {"subp_64s_dp_2src"_h, &Simulator::SimulateMTESubPointer},
      {"cpyen_cpy_memcms"_h, &Simulator::SimulateCpyE},
      {"cpyern_cpy_memcms"_h, &Simulator::SimulateCpyE},
      {"cpyewn_cpy_memcms"_h, &Simulator::SimulateCpyE},
      {"cpye_cpy_memcms"_h, &Simulator::SimulateCpyE},
      {"cpyfen_cpy_memcms"_h, &Simulator::SimulateCpyE},
      {"cpyfern_cpy_memcms"_h, &Simulator::SimulateCpyE},
      {"cpyfewn_cpy_memcms"_h, &Simulator::SimulateCpyE},
      {"cpyfe_cpy_memcms"_h, &Simulator::SimulateCpyE},
      {"cpyfmn_cpy_memcms"_h, &Simulator::SimulateCpyM},
      {"cpyfmrn_cpy_memcms"_h, &Simulator::SimulateCpyM},
      {"cpyfmwn_cpy_memcms"_h, &Simulator::SimulateCpyM},
      {"cpyfm_cpy_memcms"_h, &Simulator::SimulateCpyM},
      {"cpyfpn_cpy_memcms"_h, &Simulator::SimulateCpyFP},
      {"cpyfprn_cpy_memcms"_h, &Simulator::SimulateCpyFP},
      {"cpyfpwn_cpy_memcms"_h, &Simulator::SimulateCpyFP},
      {"cpyfp_cpy_memcms"_h, &Simulator::SimulateCpyFP},
      {"cpymn_cpy_memcms"_h, &Simulator::SimulateCpyM},
      {"cpymrn_cpy_memcms"_h, &Simulator::SimulateCpyM},
      {"cpymwn_cpy_memcms"_h, &Simulator::SimulateCpyM},
      {"cpym_cpy_memcms"_h, &Simulator::SimulateCpyM},
      {"cpypn_cpy_memcms"_h, &Simulator::SimulateCpyP},
      {"cpyprn_cpy_memcms"_h, &Simulator::SimulateCpyP},
      {"cpypwn_cpy_memcms"_h, &Simulator::SimulateCpyP},
      {"cpyp_cpy_memcms"_h, &Simulator::SimulateCpyP},
      {"setp_set_memcms"_h, &Simulator::SimulateSetP},
      {"setpn_set_memcms"_h, &Simulator::SimulateSetP},
      {"setgp_set_memcms"_h, &Simulator::SimulateSetGP},
      {"setgpn_set_memcms"_h, &Simulator::SimulateSetGP},
      {"setm_set_memcms"_h, &Simulator::SimulateSetM},
      {"setmn_set_memcms"_h, &Simulator::SimulateSetM},
      {"setgm_set_memcms"_h, &Simulator::SimulateSetGM},
      {"setgmn_set_memcms"_h, &Simulator::SimulateSetGM},
      {"sete_set_memcms"_h, &Simulator::SimulateSetE},
      {"seten_set_memcms"_h, &Simulator::SimulateSetE},
      {"setge_set_memcms"_h, &Simulator::SimulateSetE},
      {"setgen_set_memcms"_h, &Simulator::SimulateSetE},
      {"abs_32_dp_1src"_h, &Simulator::VisitDataProcessing1Source},
      {"abs_64_dp_1src"_h, &Simulator::VisitDataProcessing1Source},
      {"cnt_32_dp_1src"_h, &Simulator::VisitDataProcessing1Source},
      {"cnt_64_dp_1src"_h, &Simulator::VisitDataProcessing1Source},
      {"ctz_32_dp_1src"_h, &Simulator::VisitDataProcessing1Source},
      {"ctz_64_dp_1src"_h, &Simulator::VisitDataProcessing1Source},
      {"smax_32_dp_2src"_h, &Simulator::SimulateSignedMinMax},
      {"smax_64_dp_2src"_h, &Simulator::SimulateSignedMinMax},
      {"smin_32_dp_2src"_h, &Simulator::SimulateSignedMinMax},
      {"smin_64_dp_2src"_h, &Simulator::SimulateSignedMinMax},
      {"smax_32_minmax_imm"_h, &Simulator::SimulateSignedMinMax},
      {"smax_64_minmax_imm"_h, &Simulator::SimulateSignedMinMax},
      {"smin_32_minmax_imm"_h, &Simulator::SimulateSignedMinMax},
      {"smin_64_minmax_imm"_h, &Simulator::SimulateSignedMinMax},
      {"umax_32_dp_2src"_h, &Simulator::SimulateUnsignedMinMax},
      {"umax_64_dp_2src"_h, &Simulator::SimulateUnsignedMinMax},
      {"umin_32_dp_2src"_h, &Simulator::SimulateUnsignedMinMax},
      {"umin_64_dp_2src"_h, &Simulator::SimulateUnsignedMinMax},
      {"umax_32u_minmax_imm"_h, &Simulator::SimulateUnsignedMinMax},
      {"umax_64u_minmax_imm"_h, &Simulator::SimulateUnsignedMinMax},
      {"umin_32u_minmax_imm"_h, &Simulator::SimulateUnsignedMinMax},
      {"umin_64u_minmax_imm"_h, &Simulator::SimulateUnsignedMinMax},
      {"bcax_vvv16_crypto4"_h, &Simulator::SimulateNEONSHA3},
      {"eor3_vvv16_crypto4"_h, &Simulator::SimulateNEONSHA3},
      {"rax1_vvv2_cryptosha512_3"_h, &Simulator::SimulateNEONSHA3},
      {"xar_vvv2_crypto3_imm6"_h, &Simulator::SimulateNEONSHA3},
      {"sha512h_qqv_cryptosha512_3"_h, &Simulator::SimulateSHA512},
      {"sha512h2_qqv_cryptosha512_3"_h, &Simulator::SimulateSHA512},
      {"sha512su0_vv2_cryptosha512_2"_h, &Simulator::SimulateSHA512},
      {"sha512su1_vvv2_cryptosha512_3"_h, &Simulator::SimulateSHA512},
      {"pmullb_z_zz_q"_h, &Simulator::SimulateSVEPmull128},
      {"pmullt_z_zz_q"_h, &Simulator::SimulateSVEPmull128},
  };
  return &form_to_visitor;
}

// Try to access the piece of memory given by the address passed in RDI and the
// offset passed in RSI, using testb. If a signal is raised then the signal
// handler should set RIP to _vixl_internal_AccessMemory_continue and RAX to
// MemoryAccessResult::Failure. If no signal is raised then zero RAX before
// returning.
#ifdef VIXL_ENABLE_IMPLICIT_CHECKS
#ifdef __x86_64__
asm(R"(
  .globl _vixl_internal_ReadMemory
  _vixl_internal_ReadMemory:
    testb (%rdi, %rsi), %al
    xorq %rax, %rax
    ret
  .globl _vixl_internal_AccessMemory_continue
  _vixl_internal_AccessMemory_continue:
    ret
)");
#else
asm(R"(
  .globl _vixl_internal_ReadMemory
  _vixl_internal_ReadMemory:
    ret
)");
#endif  // __x86_64__
#endif  // VIXL_ENABLE_IMPLICIT_CHECKS

Simulator::Simulator(Decoder* decoder, FILE* stream, SimStack::Allocated stack)
    : memory_(std::move(stack)),
      last_instr_(NULL),
      cpu_features_auditor_(decoder, CPUFeatures::All()),
      gcs_({nullptr, kGCSNoStack}),
      gcs_enabled_(false) {
  // Ensure that shift operations act as the simulator expects.
  VIXL_ASSERT((static_cast<int32_t>(-1) >> 1) == -1);
  VIXL_ASSERT((static_cast<uint32_t>(-1) >> 1) == 0x7fffffff);

  // Set up a placeholder pipe for CanReadMemory.
#ifndef _WIN32
  VIXL_CHECK(pipe(placeholder_pipe_fd_) == 0);
#endif

  // Set up the decoder.
  decoder_ = decoder;
  decoder_->AppendVisitor(this);

  stream_ = stream;

  print_disasm_ = new PrintDisassembler(stream_);

  memory_.AppendMetaData(&meta_data_);

  // The Simulator and Disassembler share the same available list, held by the
  // auditor. The Disassembler only annotates instructions with features that
  // are _not_ available, so registering the auditor should have no effect
  // unless the simulator is about to abort (due to missing features). In
  // practice, this means that with trace enabled, the simulator will crash just
  // after the disassembler prints the instruction, with the missing features
  // enumerated.
  print_disasm_->RegisterCPUFeaturesAuditor(&cpu_features_auditor_);

  SetColouredTrace(false);
  trace_parameters_ = LOG_NONE;

  // We have to configure the SVE vector register length before calling
  // ResetState().
  SetVectorLengthInBits(kZRegMinSize);

  ResetState();

  // Print a warning about exclusive-access instructions, but only the first
  // time they are encountered. This warning can be silenced using
  // SilenceExclusiveAccessWarning().
  print_exclusive_access_warning_ = true;

  guard_pages_ = false;

  // Initialize the common state of RNDR and RNDRRS.
  uint64_t seed = (11 + (22 << 16) + (static_cast<uint64_t>(33) << 32));
  rand_gen_.seed(seed);

  // Initialize all bits of pseudo predicate register to true.
  LogicPRegister ones(pregister_all_true_);
  ones.SetAllBits();

  // Initialize the debugger but disable it by default.
  SetDebuggerEnabled(false);
  debugger_ = std::make_unique<Debugger>(this);
}

void Simulator::ResetSystemRegisters() {
  // Reset the system registers.
  nzcv_ = SimSystemRegister::DefaultValueFor(NZCV);
  fpcr_ = SimSystemRegister::DefaultValueFor(FPCR);
  ResetFFR();
}

void Simulator::ResetRegisters() {
  for (unsigned i = 0; i < kNumberOfRegisters; i++) {
    WriteXRegister(i, 0xbadbeef);
  }
  // Returning to address 0 exits the Simulator.
  WriteLr(kEndOfSimAddress);
}

void Simulator::ResetVRegisters() {
  // Set SVE/FP registers to a value that is a NaN in both 32-bit and 64-bit FP.
  VIXL_ASSERT((GetVectorLengthInBytes() % kDRegSizeInBytes) == 0);
  int lane_count = GetVectorLengthInBytes() / kDRegSizeInBytes;
  for (unsigned i = 0; i < kNumberOfZRegisters; i++) {
    VIXL_ASSERT(vregisters_[i].GetSizeInBytes() == GetVectorLengthInBytes());
    vregisters_[i].NotifyAccessAsZ();
    for (int lane = 0; lane < lane_count; lane++) {
      // Encode the register number and (D-sized) lane into each NaN, to
      // make them easier to trace.
      uint64_t nan_bits = 0x7ff0f0007f80f000 | (0x0000000100000000 * i) |
                          (0x0000000000000001 * lane);
      VIXL_ASSERT(IsSignallingNaN(RawbitsToDouble(nan_bits & kDRegMask)));
      VIXL_ASSERT(IsSignallingNaN(RawbitsToFloat(nan_bits & kSRegMask)));
      vregisters_[i].Insert(lane, nan_bits);
    }
  }
}

void Simulator::ResetPRegisters() {
  VIXL_ASSERT((GetPredicateLengthInBytes() % kHRegSizeInBytes) == 0);
  int lane_count = GetPredicateLengthInBytes() / kHRegSizeInBytes;
  // Ensure the register configuration fits in this bit encoding.
  VIXL_STATIC_ASSERT(kNumberOfPRegisters <= UINT8_MAX);
  VIXL_ASSERT(lane_count <= UINT8_MAX);
  for (unsigned i = 0; i < kNumberOfPRegisters; i++) {
    VIXL_ASSERT(pregisters_[i].GetSizeInBytes() == GetPredicateLengthInBytes());
    for (int lane = 0; lane < lane_count; lane++) {
      // Encode the register number and (H-sized) lane into each lane slot.
      uint16_t bits = (0x0100 * lane) | i;
      pregisters_[i].Insert(lane, bits);
    }
  }
}

void Simulator::ResetFFR() {
  VIXL_ASSERT((GetPredicateLengthInBytes() % kHRegSizeInBytes) == 0);
  int default_active_lanes = GetPredicateLengthInBytes() / kHRegSizeInBytes;
  ffr_register_.Write(static_cast<uint16_t>(GetUintMask(default_active_lanes)));
}

void Simulator::ResetState() {
  ResetSystemRegisters();
  ResetRegisters();
  ResetVRegisters();
  ResetPRegisters();

  WriteSp(memory_.GetStack().GetBase());
  ResetGCSState();
  EnableGCSCheck();

  pc_ = NULL;
  pc_modified_ = false;

  // BTI state.
  btype_ = DefaultBType;
  next_btype_ = DefaultBType;

  meta_data_.ResetState();
}

void Simulator::SetVectorLengthInBits(unsigned vector_length) {
  VIXL_ASSERT((vector_length >= kZRegMinSize) &&
              (vector_length <= kZRegMaxSize));
  VIXL_ASSERT(IsPowerOf2(vector_length));
  vector_length_ = vector_length;

  for (unsigned i = 0; i < kNumberOfZRegisters; i++) {
    vregisters_[i].SetSizeInBytes(GetVectorLengthInBytes());
  }
  for (unsigned i = 0; i < kNumberOfPRegisters; i++) {
    pregisters_[i].SetSizeInBytes(GetPredicateLengthInBytes());
  }

  ffr_register_.SetSizeInBytes(GetPredicateLengthInBytes());

  ResetVRegisters();
  ResetPRegisters();
  ResetFFR();
}

Simulator::~Simulator() {
  // The decoder may outlive the simulator.
  decoder_->RemoveVisitor(print_disasm_);
  delete print_disasm_;
#ifndef _WIN32
  close(placeholder_pipe_fd_[0]);
  close(placeholder_pipe_fd_[1]);
#endif
  GetGCSManager().FreeStack(GetGCSToken());
}


void Simulator::Run() {
  // Flush any written registers before executing anything, so that
  // manually-set registers are logged _before_ the first instruction.
  LogAllWrittenRegisters();

  if (debugger_enabled_) {
    // Slow path to check for breakpoints only if the debugger is enabled.
    Debugger* debugger = GetDebugger();
    while (!IsSimulationFinished()) {
      if (debugger->IsAtBreakpoint()) {
        fprintf(stream_, "Debugger hit breakpoint, breaking...\n");
        debugger->Debug();
      } else {
        ExecuteInstruction();
      }
    }
  } else {
    while (!IsSimulationFinished()) {
      ExecuteInstruction();
    }
  }
}


void Simulator::RunFrom(const Instruction* first) {
  WritePc(first, NoBranchLog);
  Run();
}


// clang-format off
const char* Simulator::xreg_names[] = {"x0",  "x1",  "x2",  "x3",  "x4",  "x5",
                                       "x6",  "x7",  "x8",  "x9",  "x10", "x11",
                                       "x12", "x13", "x14", "x15", "x16", "x17",
                                       "x18", "x19", "x20", "x21", "x22", "x23",
                                       "x24", "x25", "x26", "x27", "x28", "x29",
                                       "lr",  "xzr", "sp"};

const char* Simulator::wreg_names[] = {"w0",  "w1",  "w2",  "w3",  "w4",  "w5",
                                       "w6",  "w7",  "w8",  "w9",  "w10", "w11",
                                       "w12", "w13", "w14", "w15", "w16", "w17",
                                       "w18", "w19", "w20", "w21", "w22", "w23",
                                       "w24", "w25", "w26", "w27", "w28", "w29",
                                       "w30", "wzr", "wsp"};

const char* Simulator::breg_names[] = {"b0",  "b1",  "b2",  "b3",  "b4",  "b5",
                                       "b6",  "b7",  "b8",  "b9",  "b10", "b11",
                                       "b12", "b13", "b14", "b15", "b16", "b17",
                                       "b18", "b19", "b20", "b21", "b22", "b23",
                                       "b24", "b25", "b26", "b27", "b28", "b29",
                                       "b30", "b31"};

const char* Simulator::hreg_names[] = {"h0",  "h1",  "h2",  "h3",  "h4",  "h5",
                                       "h6",  "h7",  "h8",  "h9",  "h10", "h11",
                                       "h12", "h13", "h14", "h15", "h16", "h17",
                                       "h18", "h19", "h20", "h21", "h22", "h23",
                                       "h24", "h25", "h26", "h27", "h28", "h29",
                                       "h30", "h31"};

const char* Simulator::sreg_names[] = {"s0",  "s1",  "s2",  "s3",  "s4",  "s5",
                                       "s6",  "s7",  "s8",  "s9",  "s10", "s11",
                                       "s12", "s13", "s14", "s15", "s16", "s17",
                                       "s18", "s19", "s20", "s21", "s22", "s23",
                                       "s24", "s25", "s26", "s27", "s28", "s29",
                                       "s30", "s31"};

const char* Simulator::dreg_names[] = {"d0",  "d1",  "d2",  "d3",  "d4",  "d5",
                                       "d6",  "d7",  "d8",  "d9",  "d10", "d11",
                                       "d12", "d13", "d14", "d15", "d16", "d17",
                                       "d18", "d19", "d20", "d21", "d22", "d23",
                                       "d24", "d25", "d26", "d27", "d28", "d29",
                                       "d30", "d31"};

const char* Simulator::vreg_names[] = {"v0",  "v1",  "v2",  "v3",  "v4",  "v5",
                                       "v6",  "v7",  "v8",  "v9",  "v10", "v11",
                                       "v12", "v13", "v14", "v15", "v16", "v17",
                                       "v18", "v19", "v20", "v21", "v22", "v23",
                                       "v24", "v25", "v26", "v27", "v28", "v29",
                                       "v30", "v31"};

const char* Simulator::zreg_names[] = {"z0",  "z1",  "z2",  "z3",  "z4",  "z5",
                                       "z6",  "z7",  "z8",  "z9",  "z10", "z11",
                                       "z12", "z13", "z14", "z15", "z16", "z17",
                                       "z18", "z19", "z20", "z21", "z22", "z23",
                                       "z24", "z25", "z26", "z27", "z28", "z29",
                                       "z30", "z31"};

const char* Simulator::preg_names[] = {"p0",  "p1",  "p2",  "p3",  "p4",  "p5",
                                       "p6",  "p7",  "p8",  "p9",  "p10", "p11",
                                       "p12", "p13", "p14", "p15"};
// clang-format on


const char* Simulator::WRegNameForCode(unsigned code, Reg31Mode mode) {
  // If the code represents the stack pointer, index the name after zr.
  if ((code == kSPRegInternalCode) ||
      ((code == kZeroRegCode) && (mode == Reg31IsStackPointer))) {
    code = kZeroRegCode + 1;
  }
  VIXL_ASSERT(code < ArrayLength(wreg_names));
  return wreg_names[code];
}


const char* Simulator::XRegNameForCode(unsigned code, Reg31Mode mode) {
  // If the code represents the stack pointer, index the name after zr.
  if ((code == kSPRegInternalCode) ||
      ((code == kZeroRegCode) && (mode == Reg31IsStackPointer))) {
    code = kZeroRegCode + 1;
  }
  VIXL_ASSERT(code < ArrayLength(xreg_names));
  return xreg_names[code];
}


const char* Simulator::BRegNameForCode(unsigned code) {
  VIXL_ASSERT(code < kNumberOfVRegisters);
  return breg_names[code];
}


const char* Simulator::HRegNameForCode(unsigned code) {
  VIXL_ASSERT(code < kNumberOfVRegisters);
  return hreg_names[code];
}


const char* Simulator::SRegNameForCode(unsigned code) {
  VIXL_ASSERT(code < kNumberOfVRegisters);
  return sreg_names[code];
}


const char* Simulator::DRegNameForCode(unsigned code) {
  VIXL_ASSERT(code < kNumberOfVRegisters);
  return dreg_names[code];
}


const char* Simulator::VRegNameForCode(unsigned code) {
  VIXL_ASSERT(code < kNumberOfVRegisters);
  return vreg_names[code];
}


const char* Simulator::ZRegNameForCode(unsigned code) {
  VIXL_ASSERT(code < kNumberOfZRegisters);
  return zreg_names[code];
}


const char* Simulator::PRegNameForCode(unsigned code) {
  VIXL_ASSERT(code < kNumberOfPRegisters);
  return preg_names[code];
}

SimVRegister Simulator::ExpandToSimVRegister(const SimPRegister& pg) {
  SimVRegister ones, result;
  dup_immediate(kFormatVnB, ones, 0xff);
  mov_zeroing(kFormatVnB, result, pg, ones);
  return result;
}

void Simulator::ExtractFromSimVRegister(VectorFormat vform,
                                        SimPRegister& pd,
                                        SimVRegister vreg) {
  SimVRegister zero;
  dup_immediate(kFormatVnB, zero, 0);
  SVEIntCompareVectorsHelper(ne,
                             vform,
                             pd,
                             GetPTrue(),
                             vreg,
                             zero,
                             false,
                             LeaveFlags);
}

#define COLOUR(colour_code) "\033[0;" colour_code "m"
#define COLOUR_BOLD(colour_code) "\033[1;" colour_code "m"
#define COLOUR_HIGHLIGHT "\033[43m"
#define NORMAL ""
#define GREY "30"
#define RED "31"
#define GREEN "32"
#define YELLOW "33"
#define BLUE "34"
#define MAGENTA "35"
#define CYAN "36"
#define WHITE "37"
void Simulator::SetColouredTrace(bool value) {
  coloured_trace_ = value;

  clr_normal = value ? COLOUR(NORMAL) : "";
  clr_flag_name = value ? COLOUR_BOLD(WHITE) : "";
  clr_flag_value = value ? COLOUR(NORMAL) : "";
  clr_reg_name = value ? COLOUR_BOLD(CYAN) : "";
  clr_reg_value = value ? COLOUR(CYAN) : "";
  clr_vreg_name = value ? COLOUR_BOLD(MAGENTA) : "";
  clr_vreg_value = value ? COLOUR(MAGENTA) : "";
  clr_preg_name = value ? COLOUR_BOLD(GREEN) : "";
  clr_preg_value = value ? COLOUR(GREEN) : "";
  clr_memory_address = value ? COLOUR_BOLD(BLUE) : "";
  clr_warning = value ? COLOUR_BOLD(YELLOW) : "";
  clr_warning_message = value ? COLOUR(YELLOW) : "";
  clr_printf = value ? COLOUR(GREEN) : "";
  clr_branch_marker = value ? COLOUR(GREY) COLOUR_HIGHLIGHT : "";

  if (value) {
    print_disasm_->SetCPUFeaturesPrefix("// Needs: " COLOUR_BOLD(RED));
    print_disasm_->SetCPUFeaturesSuffix(COLOUR(NORMAL));
  } else {
    print_disasm_->SetCPUFeaturesPrefix("// Needs: ");
    print_disasm_->SetCPUFeaturesSuffix("");
  }
}


void Simulator::SetTraceParameters(int parameters) {
  bool disasm_before = trace_parameters_ & LOG_DISASM;
  trace_parameters_ = parameters;
  bool disasm_after = trace_parameters_ & LOG_DISASM;

  if (disasm_before != disasm_after) {
    if (disasm_after) {
      decoder_->InsertVisitorBefore(print_disasm_, this);
    } else {
      decoder_->RemoveVisitor(print_disasm_);
    }
  }
}

// Helpers ---------------------------------------------------------------------
uint64_t Simulator::AddWithCarry(unsigned reg_size,
                                 bool set_flags,
                                 uint64_t left,
                                 uint64_t right,
                                 int carry_in) {
  std::pair<uint64_t, uint8_t> result_and_flags =
      AddWithCarry(reg_size, left, right, carry_in);
  if (set_flags) {
    uint8_t flags = result_and_flags.second;
    ReadNzcv().SetN((flags >> 3) & 1);
    ReadNzcv().SetZ((flags >> 2) & 1);
    ReadNzcv().SetC((flags >> 1) & 1);
    ReadNzcv().SetV((flags >> 0) & 1);
    LogSystemRegister(NZCV);
  }
  return result_and_flags.first;
}

std::pair<uint64_t, uint8_t> Simulator::AddWithCarry(unsigned reg_size,
                                                     uint64_t left,
                                                     uint64_t right,
                                                     int carry_in) {
  VIXL_ASSERT((carry_in == 0) || (carry_in == 1));
  VIXL_ASSERT((reg_size == kXRegSize) || (reg_size == kWRegSize));

  uint64_t max_uint = (reg_size == kWRegSize) ? kWMaxUInt : kXMaxUInt;
  uint64_t reg_mask = (reg_size == kWRegSize) ? kWRegMask : kXRegMask;
  uint64_t sign_mask = (reg_size == kWRegSize) ? kWSignMask : kXSignMask;

  left &= reg_mask;
  right &= reg_mask;
  uint64_t result = (left + right + carry_in) & reg_mask;

  // NZCV bits, ordered N in bit 3 to V in bit 0.
  uint8_t nzcv = CalcNFlag(result, reg_size) ? 8 : 0;
  nzcv |= CalcZFlag(result) ? 4 : 0;

  // Compute the C flag by comparing the result to the max unsigned integer.
  uint64_t max_uint_2op = max_uint - carry_in;
  bool C = (left > max_uint_2op) || ((max_uint_2op - left) < right);
  nzcv |= C ? 2 : 0;

  // Overflow iff the sign bit is the same for the two inputs and different
  // for the result.
  uint64_t left_sign = left & sign_mask;
  uint64_t right_sign = right & sign_mask;
  uint64_t result_sign = result & sign_mask;
  bool V = (left_sign == right_sign) && (left_sign != result_sign);
  nzcv |= V ? 1 : 0;

  return std::make_pair(result, nzcv);
}

using vixl_uint128_t = std::pair<uint64_t, uint64_t>;

vixl_uint128_t Simulator::Add128(vixl_uint128_t x, vixl_uint128_t y) {
  std::pair<uint64_t, uint8_t> sum_lo =
      AddWithCarry(kXRegSize, x.second, y.second, 0);
  int carry_in = (sum_lo.second & 0x2) >> 1;  // C flag in NZCV result.
  std::pair<uint64_t, uint8_t> sum_hi =
      AddWithCarry(kXRegSize, x.first, y.first, carry_in);
  return std::make_pair(sum_hi.first, sum_lo.first);
}

vixl_uint128_t Simulator::Lsl128(vixl_uint128_t x, unsigned shift) const {
  VIXL_ASSERT(shift <= 64);
  if (shift == 0) return x;
  if (shift == 64) return std::make_pair(x.second, 0);
  uint64_t lo = x.second << shift;
  uint64_t hi = (x.first << shift) | (x.second >> (64 - shift));
  return std::make_pair(hi, lo);
}

vixl_uint128_t Simulator::Eor128(vixl_uint128_t x, vixl_uint128_t y) const {
  return std::make_pair(x.first ^ y.first, x.second ^ y.second);
}

vixl_uint128_t Simulator::Neg128(vixl_uint128_t x) {
  // Negate the integer value. Throw an assertion when the input is INT128_MIN.
  VIXL_ASSERT((x.first != GetSignMask(64)) || (x.second != 0));
  x.first = ~x.first;
  x.second = ~x.second;
  return Add128(x, {0, 1});
}

vixl_uint128_t Simulator::Mul64(uint64_t x, uint64_t y) {
  bool neg_result = false;
  if ((x >> 63) == 1) {
    x = UnsignedNegate(x);
    neg_result = !neg_result;
  }
  if ((y >> 63) == 1) {
    y = UnsignedNegate(y);
    neg_result = !neg_result;
  }

  uint64_t x_lo = x & 0xffffffff;
  uint64_t x_hi = x >> 32;
  uint64_t y_lo = y & 0xffffffff;
  uint64_t y_hi = y >> 32;

  uint64_t t1 = x_lo * y_hi;
  uint64_t t2 = x_hi * y_lo;
  vixl_uint128_t a = std::make_pair(0, x_lo * y_lo);
  vixl_uint128_t b = std::make_pair(t1 >> 32, t1 << 32);
  vixl_uint128_t c = std::make_pair(t2 >> 32, t2 << 32);
  vixl_uint128_t d = std::make_pair(x_hi * y_hi, 0);

  vixl_uint128_t result = Add128(a, b);
  result = Add128(result, c);
  result = Add128(result, d);
  return neg_result ? std::make_pair(UnsignedNegate(result.first) - 1,
                                     UnsignedNegate(result.second))
                    : result;
}

vixl_uint128_t Simulator::PolynomialMult128(uint64_t op1,
                                            uint64_t op2,
                                            int lane_size_in_bits) const {
  VIXL_ASSERT(static_cast<unsigned>(lane_size_in_bits) <= kDRegSize);
  vixl_uint128_t result = std::make_pair(0, 0);
  vixl_uint128_t op2q = std::make_pair(0, op2);
  for (int i = 0; i < lane_size_in_bits; i++) {
    if ((op1 >> i) & 1) {
      result = Eor128(result, Lsl128(op2q, i));
    }
  }
  return result;
}

int64_t Simulator::ShiftOperand(unsigned reg_size,
                                uint64_t uvalue,
                                Shift shift_type,
                                unsigned amount) const {
  VIXL_ASSERT((reg_size == kBRegSize) || (reg_size == kHRegSize) ||
              (reg_size == kSRegSize) || (reg_size == kDRegSize));
  if (amount > 0) {
    uint64_t mask = GetUintMask(reg_size);
    bool is_negative = (uvalue & GetSignMask(reg_size)) != 0;
    // The behavior is undefined in c++ if the shift amount greater than or
    // equal to the register lane size. Work out the shifted result based on
    // architectural behavior before performing the c++ type shift operations.
    switch (shift_type) {
      case LSL:
        if (amount >= reg_size) {
          return UINT64_C(0);
        }
        uvalue <<= amount;
        break;
      case LSR:
        if (amount >= reg_size) {
          return UINT64_C(0);
        }
        uvalue >>= amount;
        break;
      case ASR:
        if (amount >= reg_size) {
          return is_negative ? ~UINT64_C(0) : UINT64_C(0);
        }
        uvalue >>= amount;
        if (is_negative) {
          // Simulate sign-extension to 64 bits.
          uvalue |= ~UINT64_C(0) << (reg_size - amount);
        }
        break;
      case ROR: {
        uvalue = RotateRight(uvalue, amount, reg_size);
        break;
      }
      default:
        VIXL_UNIMPLEMENTED();
        return 0;
    }
    uvalue &= mask;
  }

  int64_t result;
  memcpy(&result, &uvalue, sizeof(result));
  return result;
}


int64_t Simulator::ExtendValue(unsigned reg_size,
                               int64_t value,
                               Extend extend_type,
                               unsigned left_shift) const {
  switch (extend_type) {
    case UXTB:
      value &= kByteMask;
      break;
    case UXTH:
      value &= kHalfWordMask;
      break;
    case UXTW:
      value &= kWordMask;
      break;
    case SXTB:
      value &= kByteMask;
      if ((value & 0x80) != 0) {
        value |= ~UINT64_C(0) << 8;
      }
      break;
    case SXTH:
      value &= kHalfWordMask;
      if ((value & 0x8000) != 0) {
        value |= ~UINT64_C(0) << 16;
      }
      break;
    case SXTW:
      value &= kWordMask;
      if ((value & 0x80000000) != 0) {
        value |= ~UINT64_C(0) << 32;
      }
      break;
    case UXTX:
    case SXTX:
      break;
    default:
      VIXL_UNREACHABLE();
  }
  return ShiftOperand(reg_size, value, LSL, left_shift);
}


void Simulator::FPCompare(double val0, double val1, FPTrapFlags trap) {
  AssertSupportedFPCR();

  // TODO: This assumes that the C++ implementation handles comparisons in the
  // way that we expect (as per AssertSupportedFPCR()).
  bool process_exception = false;
  if ((IsNaN(val0) != 0) || (IsNaN(val1) != 0)) {
    ReadNzcv().SetRawValue(FPUnorderedFlag);
    if (IsSignallingNaN(val0) || IsSignallingNaN(val1) ||
        (trap == EnableTrap)) {
      process_exception = true;
    }
  } else if (val0 < val1) {
    ReadNzcv().SetRawValue(FPLessThanFlag);
  } else if (val0 > val1) {
    ReadNzcv().SetRawValue(FPGreaterThanFlag);
  } else if (val0 == val1) {
    ReadNzcv().SetRawValue(FPEqualFlag);
  } else {
    VIXL_UNREACHABLE();
  }
  LogSystemRegister(NZCV);
  if (process_exception) FPProcessException();
}


uint64_t Simulator::ComputeMemOperandAddress(const MemOperand& mem_op) const {
  VIXL_ASSERT(mem_op.IsValid());
  int64_t base = ReadRegister<int64_t>(mem_op.GetBaseRegister());
  if (mem_op.IsImmediateOffset()) {
    return base + mem_op.GetOffset();
  } else {
    VIXL_ASSERT(mem_op.GetRegisterOffset().IsValid());
    int64_t offset = ReadRegister<int64_t>(mem_op.GetRegisterOffset());
    unsigned shift_amount = mem_op.GetShiftAmount();
    if (mem_op.GetShift() != NO_SHIFT) {
      offset = ShiftOperand(kXRegSize, offset, mem_op.GetShift(), shift_amount);
    }
    if (mem_op.GetExtend() != NO_EXTEND) {
      offset = ExtendValue(kXRegSize, offset, mem_op.GetExtend(), shift_amount);
    }
    return static_cast<uint64_t>(base + offset);
  }
}


Simulator::PrintRegisterFormat Simulator::GetPrintRegisterFormatForSize(
    unsigned reg_size, unsigned lane_size) {
  VIXL_ASSERT(reg_size >= lane_size);

  uint32_t format = 0;
  if (reg_size != lane_size) {
    switch (reg_size) {
      default:
        VIXL_UNREACHABLE();
        break;
      case kQRegSizeInBytes:
        format = kPrintRegAsQVector;
        break;
      case kDRegSizeInBytes:
        format = kPrintRegAsDVector;
        break;
    }
  }

  switch (lane_size) {
    default:
      VIXL_UNREACHABLE();
      break;
    case kQRegSizeInBytes:
      format |= kPrintReg1Q;
      break;
    case kDRegSizeInBytes:
      format |= kPrintReg1D;
      break;
    case kSRegSizeInBytes:
      format |= kPrintReg1S;
      break;
    case kHRegSizeInBytes:
      format |= kPrintReg1H;
      break;
    case kBRegSizeInBytes:
      format |= kPrintReg1B;
      break;
  }
  // These sizes would be duplicate case labels.
  VIXL_STATIC_ASSERT(kXRegSizeInBytes == kDRegSizeInBytes);
  VIXL_STATIC_ASSERT(kWRegSizeInBytes == kSRegSizeInBytes);
  VIXL_STATIC_ASSERT(kPrintXReg == kPrintReg1D);
  VIXL_STATIC_ASSERT(kPrintWReg == kPrintReg1S);

  return static_cast<PrintRegisterFormat>(format);
}


Simulator::PrintRegisterFormat Simulator::GetPrintRegisterFormat(
    VectorFormat vform) {
  switch (vform) {
    default:
      VIXL_UNREACHABLE();
      return kPrintReg16B;
    case kFormat16B:
      return kPrintReg16B;
    case kFormat8B:
      return kPrintReg8B;
    case kFormat8H:
      return kPrintReg8H;
    case kFormat4H:
      return kPrintReg4H;
    case kFormat4S:
      return kPrintReg4S;
    case kFormat2S:
      return kPrintReg2S;
    case kFormat2D:
      return kPrintReg2D;
    case kFormat1D:
      return kPrintReg1D;

    case kFormatB:
      return kPrintReg1B;
    case kFormatH:
      return kPrintReg1H;
    case kFormatS:
      return kPrintReg1S;
    case kFormatD:
      return kPrintReg1D;

    case kFormatVnB:
      return kPrintRegVnB;
    case kFormatVnH:
      return kPrintRegVnH;
    case kFormatVnS:
      return kPrintRegVnS;
    case kFormatVnD:
      return kPrintRegVnD;
  }
}


Simulator::PrintRegisterFormat Simulator::GetPrintRegisterFormatFP(
    VectorFormat vform) {
  switch (vform) {
    default:
      VIXL_UNREACHABLE();
      return kPrintReg16B;
    case kFormat8H:
      return kPrintReg8HFP;
    case kFormat4H:
      return kPrintReg4HFP;
    case kFormat4S:
      return kPrintReg4SFP;
    case kFormat2S:
      return kPrintReg2SFP;
    case kFormat2D:
      return kPrintReg2DFP;
    case kFormat1D:
      return kPrintReg1DFP;
    case kFormatH:
      return kPrintReg1HFP;
    case kFormatS:
      return kPrintReg1SFP;
    case kFormatD:
      return kPrintReg1DFP;
  }
}

void Simulator::PrintRegisters() {
  for (unsigned i = 0; i < kNumberOfRegisters; i++) {
    if (i == kSpRegCode) i = kSPRegInternalCode;
    PrintRegister(i);
  }
}

void Simulator::PrintVRegisters() {
  for (unsigned i = 0; i < kNumberOfVRegisters; i++) {
    PrintVRegister(i);
  }
}

void Simulator::PrintZRegisters() {
  for (unsigned i = 0; i < kNumberOfZRegisters; i++) {
    PrintZRegister(i);
  }
}

void Simulator::PrintWrittenRegisters() {
  for (unsigned i = 0; i < kNumberOfRegisters; i++) {
    if (registers_[i].WrittenSinceLastLog()) {
      if (i == kSpRegCode) i = kSPRegInternalCode;
      PrintRegister(i);
    }
  }
}

void Simulator::PrintWrittenVRegisters() {
  bool has_sve = GetCPUFeatures()->Has(CPUFeatures::kSVE);
  for (unsigned i = 0; i < kNumberOfVRegisters; i++) {
    if (vregisters_[i].WrittenSinceLastLog()) {
      // Z registers are initialised in the constructor before the user can
      // configure the CPU features, so we must also check for SVE here.
      if (vregisters_[i].AccessedAsZSinceLastLog() && has_sve) {
        PrintZRegister(i);
      } else {
        PrintVRegister(i);
      }
    }
  }
}

void Simulator::PrintWrittenPRegisters() {
  // P registers are initialised in the constructor before the user can
  // configure the CPU features, so we must check for SVE here.
  if (!GetCPUFeatures()->Has(CPUFeatures::kSVE)) return;
  for (unsigned i = 0; i < kNumberOfPRegisters; i++) {
    if (pregisters_[i].WrittenSinceLastLog()) {
      PrintPRegister(i);
    }
  }
  if (ReadFFR().WrittenSinceLastLog()) PrintFFR();
}

void Simulator::PrintSystemRegisters() {
  PrintSystemRegister(NZCV);
  PrintSystemRegister(FPCR);
}

void Simulator::PrintRegisterValue(const uint8_t* value,
                                   int value_size,
                                   PrintRegisterFormat format) {
  int print_width = GetPrintRegSizeInBytes(format);
  VIXL_ASSERT(print_width <= value_size);
  for (int i = value_size - 1; i >= print_width; i--) {
    // Pad with spaces so that values align vertically.
    fprintf(stream_, "  ");
    // If we aren't explicitly printing a partial value, ensure that the
    // unprinted bits are zero.
    VIXL_ASSERT(((format & kPrintRegPartial) != 0) || (value[i] == 0));
  }
  fprintf(stream_, "0x");
  for (int i = print_width - 1; i >= 0; i--) {
    fprintf(stream_, "%02x", value[i]);
  }
}

void Simulator::PrintRegisterValueFPAnnotations(const uint8_t* value,
                                                uint16_t lane_mask,
                                                PrintRegisterFormat format) {
  VIXL_ASSERT((format & kPrintRegAsFP) != 0);
  int lane_size = GetPrintRegLaneSizeInBytes(format);
  fprintf(stream_, " (");
  bool last_inactive = false;
  const char* sep = "";
  for (int i = GetPrintRegLaneCount(format) - 1; i >= 0; i--, sep = ", ") {
    bool access = (lane_mask & (1 << (i * lane_size))) != 0;
    if (access) {
      // Read the lane as a double, so we can format all FP types in the same
      // way. We squash NaNs, and a double can exactly represent any other value
      // that the smaller types can represent, so this is lossless.
      double element;
      switch (lane_size) {
        case kHRegSizeInBytes: {
          Float16 element_fp16;
          VIXL_STATIC_ASSERT(sizeof(element_fp16) == kHRegSizeInBytes);
          memcpy(&element_fp16, &value[i * lane_size], sizeof(element_fp16));
          element = FPToDouble(element_fp16, kUseDefaultNaN);
          break;
        }
        case kSRegSizeInBytes: {
          float element_fp32;
          memcpy(&element_fp32, &value[i * lane_size], sizeof(element_fp32));
          element = static_cast<double>(element_fp32);
          break;
        }
        case kDRegSizeInBytes: {
          memcpy(&element, &value[i * lane_size], sizeof(element));
          break;
        }
        default:
          VIXL_UNREACHABLE();
          fprintf(stream_, "{UnknownFPValue}");
          continue;
      }
      if (IsNaN(element)) {
        // The fprintf behaviour for NaNs is implementation-defined. Always
        // print "nan", so that traces are consistent.
        fprintf(stream_, "%s%snan%s", sep, clr_vreg_value, clr_normal);
      } else {
        fprintf(stream_,
                "%s%s%#.4g%s",
                sep,
                clr_vreg_value,
                element,
                clr_normal);
      }
      last_inactive = false;
    } else if (!last_inactive) {
      // Replace each contiguous sequence of inactive lanes with "...".
      fprintf(stream_, "%s...", sep);
      last_inactive = true;
    }
  }
  fprintf(stream_, ")");
}

void Simulator::PrintRegister(int code,
                              PrintRegisterFormat format,
                              const char* suffix) {
  VIXL_ASSERT((static_cast<unsigned>(code) < kNumberOfRegisters) ||
              (static_cast<unsigned>(code) == kSPRegInternalCode));
  VIXL_ASSERT((format & kPrintRegAsVectorMask) == kPrintRegAsScalar);
  VIXL_ASSERT((format & kPrintRegAsFP) == 0);

  SimRegister* reg;
  SimRegister zero;
  if (code == kZeroRegCode) {
    reg = &zero;
  } else {
    // registers_[31] holds the SP.
    VIXL_STATIC_ASSERT((kSPRegInternalCode % kNumberOfRegisters) == 31);
    reg = &registers_[code % kNumberOfRegisters];
  }

  // We trace register writes as whole register values, implying that any
  // unprinted bits are all zero:
  //   "#       x{code}: 0x{-----value----}"
  //   "#       w{code}:         0x{-value}"
  // Stores trace partial register values, implying nothing about the unprinted
  // bits:
  //   "# x{code}<63:0>: 0x{-----value----}"
  //   "# x{code}<31:0>:         0x{-value}"
  //   "# x{code}<15:0>:             0x{--}"
  //   "#  x{code}<7:0>:               0x{}"

  bool is_partial = (format & kPrintRegPartial) != 0;
  unsigned print_reg_size = GetPrintRegSizeInBits(format);
  std::stringstream name;
  if (is_partial) {
    name << XRegNameForCode(code) << GetPartialRegSuffix(format);
  } else {
    // Notify the register that it has been logged, but only if we're printing
    // all of it.
    reg->NotifyRegisterLogged();
    switch (print_reg_size) {
      case kWRegSize:
        name << WRegNameForCode(code);
        break;
      case kXRegSize:
        name << XRegNameForCode(code);
        break;
      default:
        VIXL_UNREACHABLE();
        return;
    }
  }

  fprintf(stream_,
          "# %s%*s: %s",
          clr_reg_name,
          kPrintRegisterNameFieldWidth,
          name.str().c_str(),
          clr_reg_value);
  PrintRegisterValue(*reg, format);
  fprintf(stream_, "%s%s", clr_normal, suffix);
}

void Simulator::PrintVRegister(int code,
                               PrintRegisterFormat format,
                               const char* suffix) {
  VIXL_ASSERT(static_cast<unsigned>(code) < kNumberOfVRegisters);
  VIXL_ASSERT(((format & kPrintRegAsVectorMask) == kPrintRegAsScalar) ||
              ((format & kPrintRegAsVectorMask) == kPrintRegAsDVector) ||
              ((format & kPrintRegAsVectorMask) == kPrintRegAsQVector));

  // We trace register writes as whole register values, implying that any
  // unprinted bits are all zero:
  //   "#        v{code}: 0x{-------------value------------}"
  //   "#        d{code}:                 0x{-----value----}"
  //   "#        s{code}:                         0x{-value}"
  //   "#        h{code}:                             0x{--}"
  //   "#        b{code}:                               0x{}"
  // Stores trace partial register values, implying nothing about the unprinted
  // bits:
  //   "# v{code}<127:0>: 0x{-------------value------------}"
  //   "#  v{code}<63:0>:                 0x{-----value----}"
  //   "#  v{code}<31:0>:                         0x{-value}"
  //   "#  v{code}<15:0>:                             0x{--}"
  //   "#   v{code}<7:0>:                               0x{}"

  bool is_partial = ((format & kPrintRegPartial) != 0);
  std::stringstream name;
  unsigned print_reg_size = GetPrintRegSizeInBits(format);
  if (is_partial) {
    name << VRegNameForCode(code) << GetPartialRegSuffix(format);
  } else {
    // Notify the register that it has been logged, but only if we're printing
    // all of it.
    vregisters_[code].NotifyRegisterLogged();
    switch (print_reg_size) {
      case kBRegSize:
        name << BRegNameForCode(code);
        break;
      case kHRegSize:
        name << HRegNameForCode(code);
        break;
      case kSRegSize:
        name << SRegNameForCode(code);
        break;
      case kDRegSize:
        name << DRegNameForCode(code);
        break;
      case kQRegSize:
        name << VRegNameForCode(code);
        break;
      default:
        VIXL_UNREACHABLE();
        return;
    }
  }

  fprintf(stream_,
          "# %s%*s: %s",
          clr_vreg_name,
          kPrintRegisterNameFieldWidth,
          name.str().c_str(),
          clr_vreg_value);
  PrintRegisterValue(vregisters_[code], format);
  fprintf(stream_, "%s", clr_normal);
  if ((format & kPrintRegAsFP) != 0) {
    PrintRegisterValueFPAnnotations(vregisters_[code], format);
  }
  fprintf(stream_, "%s", suffix);
}

void Simulator::PrintVRegistersForStructuredAccess(int rt_code,
                                                   int reg_count,
                                                   uint16_t focus_mask,
                                                   PrintRegisterFormat format) {
  bool print_fp = (format & kPrintRegAsFP) != 0;
  // Suppress FP formatting, so we can specify the lanes we're interested in.
  PrintRegisterFormat format_no_fp =
      static_cast<PrintRegisterFormat>(format & ~kPrintRegAsFP);

  for (int r = 0; r < reg_count; r++) {
    int code = (rt_code + r) % kNumberOfVRegisters;
    PrintVRegister(code, format_no_fp, "");
    if (print_fp) {
      PrintRegisterValueFPAnnotations(vregisters_[code], focus_mask, format);
    }
    fprintf(stream_, "\n");
  }
}

void Simulator::PrintZRegistersForStructuredAccess(int rt_code,
                                                   int q_index,
                                                   int reg_count,
                                                   uint16_t focus_mask,
                                                   PrintRegisterFormat format) {
  bool print_fp = (format & kPrintRegAsFP) != 0;
  // Suppress FP formatting, so we can specify the lanes we're interested in.
  PrintRegisterFormat format_no_fp =
      static_cast<PrintRegisterFormat>(format & ~kPrintRegAsFP);

  PrintRegisterFormat format_q = GetPrintRegAsQChunkOfSVE(format);

  const unsigned size = kQRegSizeInBytes;
  unsigned byte_index = q_index * size;
  const uint8_t* value = vregisters_[rt_code].GetBytes() + byte_index;
  VIXL_ASSERT((byte_index + size) <= vregisters_[rt_code].GetSizeInBytes());

  for (int r = 0; r < reg_count; r++) {
    int code = (rt_code + r) % kNumberOfZRegisters;
    PrintPartialZRegister(code, q_index, format_no_fp, "");
    if (print_fp) {
      PrintRegisterValueFPAnnotations(value, focus_mask, format_q);
    }
    fprintf(stream_, "\n");
  }
}

void Simulator::PrintZRegister(int code, PrintRegisterFormat format) {
  // We're going to print the register in parts, so force a partial format.
  format = GetPrintRegPartial(format);
  VIXL_ASSERT((format & kPrintRegAsVectorMask) == kPrintRegAsSVEVector);
  int vl = GetVectorLengthInBits();
  VIXL_ASSERT((vl % kQRegSize) == 0);
  for (unsigned i = 0; i < (vl / kQRegSize); i++) {
    PrintPartialZRegister(code, i, format);
  }
  vregisters_[code].NotifyRegisterLogged();
}

void Simulator::PrintPRegister(int code, PrintRegisterFormat format) {
  // We're going to print the register in parts, so force a partial format.
  format = GetPrintRegPartial(format);
  VIXL_ASSERT((format & kPrintRegAsVectorMask) == kPrintRegAsSVEVector);
  int vl = GetVectorLengthInBits();
  VIXL_ASSERT((vl % kQRegSize) == 0);
  for (unsigned i = 0; i < (vl / kQRegSize); i++) {
    PrintPartialPRegister(code, i, format);
  }
  pregisters_[code].NotifyRegisterLogged();
}

void Simulator::PrintFFR(PrintRegisterFormat format) {
  // We're going to print the register in parts, so force a partial format.
  format = GetPrintRegPartial(format);
  VIXL_ASSERT((format & kPrintRegAsVectorMask) == kPrintRegAsSVEVector);
  int vl = GetVectorLengthInBits();
  VIXL_ASSERT((vl % kQRegSize) == 0);
  SimPRegister& ffr = ReadFFR();
  for (unsigned i = 0; i < (vl / kQRegSize); i++) {
    PrintPartialPRegister("FFR", ffr, i, format);
  }
  ffr.NotifyRegisterLogged();
}

void Simulator::PrintPartialZRegister(int code,
                                      int q_index,
                                      PrintRegisterFormat format,
                                      const char* suffix) {
  VIXL_ASSERT(static_cast<unsigned>(code) < kNumberOfZRegisters);
  VIXL_ASSERT((format & kPrintRegAsVectorMask) == kPrintRegAsSVEVector);
  VIXL_ASSERT((format & kPrintRegPartial) != 0);
  VIXL_ASSERT((q_index * kQRegSize) < GetVectorLengthInBits());

  // We _only_ trace partial Z register values in Q-sized chunks, because
  // they're often too large to reasonably fit on a single line. Each line
  // implies nothing about the unprinted bits.
  //   "# z{code}<127:0>: 0x{-------------value------------}"

  format = GetPrintRegAsQChunkOfSVE(format);

  const unsigned size = kQRegSizeInBytes;
  unsigned byte_index = q_index * size;
  const uint8_t* value = vregisters_[code].GetBytes() + byte_index;
  VIXL_ASSERT((byte_index + size) <= vregisters_[code].GetSizeInBytes());

  int lsb = q_index * kQRegSize;
  int msb = lsb + kQRegSize - 1;
  std::stringstream name;
  name << ZRegNameForCode(code) << '<' << msb << ':' << lsb << '>';

  fprintf(stream_,
          "# %s%*s: %s",
          clr_vreg_name,
          kPrintRegisterNameFieldWidth,
          name.str().c_str(),
          clr_vreg_value);
  PrintRegisterValue(value, size, format);
  fprintf(stream_, "%s", clr_normal);
  if ((format & kPrintRegAsFP) != 0) {
    PrintRegisterValueFPAnnotations(value, GetPrintRegLaneMask(format), format);
  }
  fprintf(stream_, "%s", suffix);
}

void Simulator::PrintPartialPRegister(const char* name,
                                      const SimPRegister& reg,
                                      int q_index,
                                      PrintRegisterFormat format,
                                      const char* suffix) {
  VIXL_ASSERT((format & kPrintRegAsVectorMask) == kPrintRegAsSVEVector);
  VIXL_ASSERT((format & kPrintRegPartial) != 0);
  VIXL_ASSERT((q_index * kQRegSize) < GetVectorLengthInBits());

  // We don't currently use the format for anything here.
  USE(format);

  // We _only_ trace partial P register values, because they're often too large
  // to reasonably fit on a single line. Each line implies nothing about the
  // unprinted bits.
  //
  // We print values in binary, with spaces between each bit, in order for the
  // bits to align with the Z register bytes that they predicate.
  //   "# {name}<15:0>: 0b{-------------value------------}"

  int print_size_in_bits = kQRegSize / kZRegBitsPerPRegBit;
  int lsb = q_index * print_size_in_bits;
  int msb = lsb + print_size_in_bits - 1;
  std::stringstream prefix;
  prefix << name << '<' << msb << ':' << lsb << '>';

  fprintf(stream_,
          "# %s%*s: %s0b",
          clr_preg_name,
          kPrintRegisterNameFieldWidth,
          prefix.str().c_str(),
          clr_preg_value);
  for (int i = msb; i >= lsb; i--) {
    fprintf(stream_, " %c", reg.GetBit(i) ? '1' : '0');
  }
  fprintf(stream_, "%s%s", clr_normal, suffix);
}

void Simulator::PrintPartialPRegister(int code,
                                      int q_index,
                                      PrintRegisterFormat format,
                                      const char* suffix) {
  VIXL_ASSERT(static_cast<unsigned>(code) < kNumberOfPRegisters);
  PrintPartialPRegister(PRegNameForCode(code),
                        pregisters_[code],
                        q_index,
                        format,
                        suffix);
}

void Simulator::PrintSystemRegister(SystemRegister id) {
  switch (id) {
    case NZCV:
      fprintf(stream_,
              "# %sNZCV: %sN:%d Z:%d C:%d V:%d%s\n",
              clr_flag_name,
              clr_flag_value,
              ReadNzcv().GetN(),
              ReadNzcv().GetZ(),
              ReadNzcv().GetC(),
              ReadNzcv().GetV(),
              clr_normal);
      break;
    case FPCR: {
      static const char* rmode[] = {"0b00 (Round to Nearest)",
                                    "0b01 (Round towards Plus Infinity)",
                                    "0b10 (Round towards Minus Infinity)",
                                    "0b11 (Round towards Zero)"};
      VIXL_ASSERT(ReadFpcr().GetRMode() < ArrayLength(rmode));
      fprintf(stream_,
              "# %sFPCR: %sAHP:%d DN:%d FZ:%d RMode:%s%s\n",
              clr_flag_name,
              clr_flag_value,
              ReadFpcr().GetAHP(),
              ReadFpcr().GetDN(),
              ReadFpcr().GetFZ(),
              rmode[ReadFpcr().GetRMode()],
              clr_normal);
      break;
    }
    default:
      VIXL_UNREACHABLE();
  }
}

void Simulator::PrintGCS(bool is_push, uint64_t addr, size_t entry) {
  const char* arrow = is_push ? "<-" : "->";
  fprintf(stream_,
          "# %sgcs0x%04" PRIu64 "[%zx]: %s %s 0x%016" PRIx64 "\n",
          clr_flag_name,
          GCSManager::GetGCSIndexFromToken(GetGCSToken()),
          entry,
          clr_normal,
          arrow,
          addr);
}

uint16_t Simulator::PrintPartialAccess(uint16_t access_mask,
                                       uint16_t future_access_mask,
                                       int struct_element_count,
                                       int lane_size_in_bytes,
                                       const char* op,
                                       uintptr_t address,
                                       int reg_size_in_bytes) {
  // We want to assume that we'll access at least one lane.
  VIXL_ASSERT(access_mask != 0);
  VIXL_ASSERT((reg_size_in_bytes == kXRegSizeInBytes) ||
              (reg_size_in_bytes == kQRegSizeInBytes));
  bool started_annotation = false;
  // Indent to match the register field, the fixed formatting, and the value
  // prefix ("0x"): "# {name}: 0x"
  fprintf(stream_, "# %*s    ", kPrintRegisterNameFieldWidth, "");
  // First, annotate the lanes (byte by byte).
  for (int lane = reg_size_in_bytes - 1; lane >= 0; lane--) {
    bool access = (access_mask & (1 << lane)) != 0;
    bool future = (future_access_mask & (1 << lane)) != 0;
    if (started_annotation) {
      // If we've started an annotation, draw a horizontal line in addition to
      // any other symbols.
      if (access) {
        fprintf(stream_, "─╨");
      } else if (future) {
        fprintf(stream_, "─║");
      } else {
        fprintf(stream_, "──");
      }
    } else {
      if (access) {
        started_annotation = true;
        fprintf(stream_, " ╙");
      } else if (future) {
        fprintf(stream_, " ║");
      } else {
        fprintf(stream_, "  ");
      }
    }
  }
  VIXL_ASSERT(started_annotation);
  fprintf(stream_, "─ 0x");
  int lane_size_in_nibbles = lane_size_in_bytes * 2;
  // Print the most-significant struct element first.
  const char* sep = "";
  for (int i = struct_element_count - 1; i >= 0; i--) {
    int offset = lane_size_in_bytes * i;
    auto nibble = MemReadUint(lane_size_in_bytes, address + offset);
    VIXL_ASSERT(nibble);
    fprintf(stream_, "%s%0*" PRIx64, sep, lane_size_in_nibbles, *nibble);
    sep = "'";
  }
  fprintf(stream_,
          " %s %s0x%016" PRIxPTR "%s\n",
          op,
          clr_memory_address,
          address,
          clr_normal);
  return future_access_mask & ~access_mask;
}

void Simulator::PrintAccess(int code,
                            PrintRegisterFormat format,
                            const char* op,
                            uintptr_t address) {
  VIXL_ASSERT(GetPrintRegLaneCount(format) == 1);
  VIXL_ASSERT((strcmp(op, "->") == 0) || (strcmp(op, "<-") == 0));
  if ((format & kPrintRegPartial) == 0) {
    if (code != kZeroRegCode) {
      registers_[code].NotifyRegisterLogged();
    }
  }
  // Scalar-format accesses use a simple format:
  //   "# {reg}: 0x{value} -> {address}"

  // Suppress the newline, so the access annotation goes on the same line.
  PrintRegister(code, format, "");
  fprintf(stream_,
          " %s %s0x%016" PRIxPTR "%s\n",
          op,
          clr_memory_address,
          address,
          clr_normal);
}

void Simulator::PrintVAccess(int code,
                             PrintRegisterFormat format,
                             const char* op,
                             uintptr_t address) {
  VIXL_ASSERT((strcmp(op, "->") == 0) || (strcmp(op, "<-") == 0));

  // Scalar-format accesses use a simple format:
  //   "# v{code}: 0x{value} -> {address}"

  // Suppress the newline, so the access annotation goes on the same line.
  PrintVRegister(code, format, "");
  fprintf(stream_,
          " %s %s0x%016" PRIxPTR "%s\n",
          op,
          clr_memory_address,
          address,
          clr_normal);
}

void Simulator::PrintVStructAccess(int rt_code,
                                   int reg_count,
                                   PrintRegisterFormat format,
                                   const char* op,
                                   uintptr_t address) {
  VIXL_ASSERT((strcmp(op, "->") == 0) || (strcmp(op, "<-") == 0));

  // For example:
  //   "# v{code}: 0x{value}"
  //   "#     ...: 0x{value}"
  //   "#              ║   ╙─ {struct_value} -> {lowest_address}"
  //   "#              ╙───── {struct_value} -> {highest_address}"

  uint16_t lane_mask = GetPrintRegLaneMask(format);
  PrintVRegistersForStructuredAccess(rt_code, reg_count, lane_mask, format);

  int reg_size_in_bytes = GetPrintRegSizeInBytes(format);
  int lane_size_in_bytes = GetPrintRegLaneSizeInBytes(format);
  for (int i = 0; i < reg_size_in_bytes; i += lane_size_in_bytes) {
    uint16_t access_mask = 1 << i;
    VIXL_ASSERT((lane_mask & access_mask) != 0);
    lane_mask = PrintPartialAccess(access_mask,
                                   lane_mask,
                                   reg_count,
                                   lane_size_in_bytes,
                                   op,
                                   address + (i * reg_count));
  }
}

void Simulator::PrintVSingleStructAccess(int rt_code,
                                         int reg_count,
                                         int lane,
                                         PrintRegisterFormat format,
                                         const char* op,
                                         uintptr_t address) {
  VIXL_ASSERT((strcmp(op, "->") == 0) || (strcmp(op, "<-") == 0));

  // For example:
  //   "# v{code}: 0x{value}"
  //   "#     ...: 0x{value}"
  //   "#              ╙───── {struct_value} -> {address}"

  int lane_size_in_bytes = GetPrintRegLaneSizeInBytes(format);
  uint16_t lane_mask = 1 << (lane * lane_size_in_bytes);
  PrintVRegistersForStructuredAccess(rt_code, reg_count, lane_mask, format);
  PrintPartialAccess(lane_mask, 0, reg_count, lane_size_in_bytes, op, address);
}

void Simulator::PrintVReplicatingStructAccess(int rt_code,
                                              int reg_count,
                                              PrintRegisterFormat format,
                                              const char* op,
                                              uintptr_t address) {
  VIXL_ASSERT((strcmp(op, "->") == 0) || (strcmp(op, "<-") == 0));

  // For example:
  //   "# v{code}: 0x{value}"
  //   "#     ...: 0x{value}"
  //   "#            ╙─╨─╨─╨─ {struct_value} -> {address}"

  int lane_size_in_bytes = GetPrintRegLaneSizeInBytes(format);
  uint16_t lane_mask = GetPrintRegLaneMask(format);
  PrintVRegistersForStructuredAccess(rt_code, reg_count, lane_mask, format);
  PrintPartialAccess(lane_mask, 0, reg_count, lane_size_in_bytes, op, address);
}

void Simulator::PrintZAccess(int rt_code, const char* op, uintptr_t address) {
  VIXL_ASSERT((strcmp(op, "->") == 0) || (strcmp(op, "<-") == 0));

  // Scalar-format accesses are split into separate chunks, each of which uses a
  // simple format:
  //   "#   z{code}<127:0>: 0x{value} -> {address}"
  //   "# z{code}<255:128>: 0x{value} -> {address + 16}"
  //   "# z{code}<383:256>: 0x{value} -> {address + 32}"
  // etc

  int vl = GetVectorLengthInBits();
  VIXL_ASSERT((vl % kQRegSize) == 0);
  for (unsigned q_index = 0; q_index < (vl / kQRegSize); q_index++) {
    // Suppress the newline, so the access annotation goes on the same line.
    PrintPartialZRegister(rt_code, q_index, kPrintRegVnQPartial, "");
    fprintf(stream_,
            " %s %s0x%016" PRIxPTR "%s\n",
            op,
            clr_memory_address,
            address,
            clr_normal);
    address += kQRegSizeInBytes;
  }
}

void Simulator::PrintZStructAccess(int rt_code,
                                   int reg_count,
                                   const LogicPRegister& pg,
                                   PrintRegisterFormat format,
                                   int msize_in_bytes,
                                   const char* op,
                                   const LogicSVEAddressVector& addr) {
  VIXL_ASSERT((strcmp(op, "->") == 0) || (strcmp(op, "<-") == 0));

  // For example:
  //   "# z{code}<255:128>: 0x{value}"
  //   "#     ...<255:128>: 0x{value}"
  //   "#                       ║   ╙─ {struct_value} -> {first_address}"
  //   "#                       ╙───── {struct_value} -> {last_address}"

  // We're going to print the register in parts, so force a partial format.
  bool skip_inactive_chunks = (format & kPrintRegPartial) != 0;
  format = GetPrintRegPartial(format);

  int esize_in_bytes = GetPrintRegLaneSizeInBytes(format);
  int vl = GetVectorLengthInBits();
  VIXL_ASSERT((vl % kQRegSize) == 0);
  int lanes_per_q = kQRegSizeInBytes / esize_in_bytes;
  for (unsigned q_index = 0; q_index < (vl / kQRegSize); q_index++) {
    uint16_t pred =
        pg.GetActiveMask<uint16_t>(q_index) & GetPrintRegLaneMask(format);
    if ((pred == 0) && skip_inactive_chunks) continue;

    PrintZRegistersForStructuredAccess(rt_code,
                                       q_index,
                                       reg_count,
                                       pred,
                                       format);
    if (pred == 0) {
      // This register chunk has no active lanes. The loop below would print
      // nothing, so leave a blank line to keep structures grouped together.
      fprintf(stream_, "#\n");
      continue;
    }
    for (int i = 0; i < lanes_per_q; i++) {
      uint16_t access = 1 << (i * esize_in_bytes);
      int lane = (q_index * lanes_per_q) + i;
      // Skip inactive lanes.
      if ((pred & access) == 0) continue;
      pred = PrintPartialAccess(access,
                                pred,
                                reg_count,
                                msize_in_bytes,
                                op,
                                addr.GetStructAddress(lane));
    }
  }

  // We print the whole register, even for stores.
  for (int i = 0; i < reg_count; i++) {
    vregisters_[(rt_code + i) % kNumberOfZRegisters].NotifyRegisterLogged();
  }
}

void Simulator::PrintPAccess(int code, const char* op, uintptr_t address) {
  VIXL_ASSERT((strcmp(op, "->") == 0) || (strcmp(op, "<-") == 0));

  // Scalar-format accesses are split into separate chunks, each of which uses a
  // simple format:
  //   "#  p{code}<15:0>: 0b{value} -> {address}"
  //   "# p{code}<31:16>: 0b{value} -> {address + 2}"
  //   "# p{code}<47:32>: 0b{value} -> {address + 4}"
  // etc

  int vl = GetVectorLengthInBits();
  VIXL_ASSERT((vl % kQRegSize) == 0);
  for (unsigned q_index = 0; q_index < (vl / kQRegSize); q_index++) {
    // Suppress the newline, so the access annotation goes on the same line.
    PrintPartialPRegister(code, q_index, kPrintRegVnQPartial, "");
    fprintf(stream_,
            " %s %s0x%016" PRIxPTR "%s\n",
            op,
            clr_memory_address,
            address,
            clr_normal);
    address += kQRegSizeInBytes;
  }
}

void Simulator::PrintMemTransfer(uintptr_t dst, uintptr_t src, uint8_t value) {
  fprintf(stream_,
          "#               %s: %s0x%016" PRIxPTR " %s<- %s0x%02x%s",
          clr_reg_name,
          clr_memory_address,
          dst,
          clr_normal,
          clr_reg_value,
          value,
          clr_normal);

  fprintf(stream_,
          " <- %s0x%016" PRIxPTR "%s\n",
          clr_memory_address,
          src,
          clr_normal);
}

void Simulator::PrintRead(int rt_code,
                          PrintRegisterFormat format,
                          uintptr_t address) {
  VIXL_ASSERT(GetPrintRegLaneCount(format) == 1);
  if (rt_code != kZeroRegCode) {
    registers_[rt_code].NotifyRegisterLogged();
  }
  PrintAccess(rt_code, format, "<-", address);
}

void Simulator::PrintExtendingRead(int rt_code,
                                   PrintRegisterFormat format,
                                   int access_size_in_bytes,
                                   uintptr_t address) {
  int reg_size_in_bytes = GetPrintRegSizeInBytes(format);
  if (access_size_in_bytes == reg_size_in_bytes) {
    // There is no extension here, so print a simple load.
    PrintRead(rt_code, format, address);
    return;
  }
  VIXL_ASSERT(access_size_in_bytes < reg_size_in_bytes);

  // For sign- and zero-extension, make it clear that the resulting register
  // value is different from what is loaded from memory.
  VIXL_ASSERT(GetPrintRegLaneCount(format) == 1);
  if (rt_code != kZeroRegCode) {
    registers_[rt_code].NotifyRegisterLogged();
  }
  PrintRegister(rt_code, format);
  PrintPartialAccess(1,
                     0,
                     1,
                     access_size_in_bytes,
                     "<-",
                     address,
                     kXRegSizeInBytes);
}

void Simulator::PrintVRead(int rt_code,
                           PrintRegisterFormat format,
                           uintptr_t address) {
  VIXL_ASSERT(GetPrintRegLaneCount(format) == 1);
  vregisters_[rt_code].NotifyRegisterLogged();
  PrintVAccess(rt_code, format, "<-", address);
}

void Simulator::PrintWrite(int rt_code,
                           PrintRegisterFormat format,
                           uintptr_t address) {
  // Because this trace doesn't represent a change to the source register's
  // value, only print the relevant part of the value.
  format = GetPrintRegPartial(format);
  VIXL_ASSERT(GetPrintRegLaneCount(format) == 1);
  if (rt_code != kZeroRegCode) {
    registers_[rt_code].NotifyRegisterLogged();
  }
  PrintAccess(rt_code, format, "->", address);
}

void Simulator::PrintVWrite(int rt_code,
                            PrintRegisterFormat format,
                            uintptr_t address) {
  // Because this trace doesn't represent a change to the source register's
  // value, only print the relevant part of the value.
  format = GetPrintRegPartial(format);
  // It only makes sense to write scalar values here. Vectors are handled by
  // PrintVStructAccess.
  VIXL_ASSERT(GetPrintRegLaneCount(format) == 1);
  PrintVAccess(rt_code, format, "->", address);
}

void Simulator::PrintTakenBranch(const Instruction* target) {
  fprintf(stream_,
          "# %sBranch%s to 0x%016" PRIx64 ".\n",
          clr_branch_marker,
          clr_normal,
          reinterpret_cast<uint64_t>(target));
}

// Visitors---------------------------------------------------------------------


void Simulator::Visit(Metadata* metadata, const Instruction* instr) {
  VIXL_ASSERT(metadata->count("form") > 0);
  // Check for unallocated encodings.
  if (metadata->count("unallocated") > 0) {
    VisitUnallocated(instr);
    return;
  }

  std::string form = (*metadata)["form"];
  form_hash_ = Hash(form.c_str());
  const FormToVisitorFnMap* fv = Simulator::GetFormToVisitorFnMap();
  FormToVisitorFnMap::const_iterator it = fv->find(form_hash_);
  if (it == fv->end()) {
    VisitUnimplemented(instr);
  } else {
    (it->second)(this, instr);
  }
}

void Simulator::Simulate_PdT_PgZ_ZnT_ZmT(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimPRegister& pd = ReadPRegister(instr->GetPd());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  SimVRegister& zm = ReadVRegister(instr->GetRm());
  SimVRegister& zn = ReadVRegister(instr->GetRn());

  switch (form_hash_) {
    case "match_p_p_zz"_h:
      match(vform, pd, zn, zm, /* negate_match = */ false);
      break;
    case "nmatch_p_p_zz"_h:
      match(vform, pd, zn, zm, /* negate_match = */ true);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
  mov_zeroing(pd, pg, pd);
  PredTest(vform, pg, pd);
}

void Simulator::Simulate_PdT_Xn_Xm(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimPRegister& pd = ReadPRegister(instr->GetPd());
  uint64_t src1 = ReadXRegister(instr->GetRn());
  uint64_t src2 = ReadXRegister(instr->GetRm());

  uint64_t absdiff = (src1 > src2) ? (src1 - src2) : (src2 - src1);
  absdiff >>= LaneSizeInBytesLog2FromFormat(vform);

  bool no_conflict = false;
  switch (form_hash_) {
    case "whilerw_p_rr"_h:
      no_conflict = (absdiff == 0);
      break;
    case "whilewr_p_rr"_h:
      no_conflict = (absdiff == 0) || (src2 <= src1);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }

  LogicPRegister dst(pd);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    dst.SetActive(vform,
                  i,
                  no_conflict || (static_cast<uint64_t>(i) < absdiff));
  }

  PredTest(vform, GetPTrue(), pd);
}

void Simulator::Simulate_ZdB_Zn1B_Zn2B_imm(const Instruction* instr) {
  VIXL_ASSERT(form_hash_ == "ext_z_zi_con"_h);

  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimVRegister& zn2 = ReadVRegister((instr->GetRn() + 1) % kNumberOfZRegisters);

  int index = instr->GetSVEExtractImmediate();
  int vl = GetVectorLengthInBytes();
  index = (index >= vl) ? 0 : index;

  ext(kFormatVnB, zd, zn, zn2, index);
}

void Simulator::Simulate_ZdB_ZnB_ZmB(const Instruction* instr) {
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRm());
  SimVRegister& zn = ReadVRegister(instr->GetRn());

  switch (form_hash_) {
    case "histseg_z_zz"_h:
      if (instr->GetSVEVectorFormat() == kFormatVnB) {
        histogram(kFormatVnB,
                  zd,
                  GetPTrue(),
                  zn,
                  zm,
                  /* do_segmented = */ true);
      } else {
        VIXL_UNIMPLEMENTED();
      }
      break;
    case "pmul_z_zz"_h:
      pmul(kFormatVnB, zd, zn, zm);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
}

void Simulator::SimulateSVEMulIndex(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());

  // The encoding for B and H-sized lanes are redefined to encode the most
  // significant bit of index for H-sized lanes. B-sized lanes are not
  // supported.
  if (vform == kFormatVnB) vform = kFormatVnH;

  VIXL_ASSERT((form_hash_ == "mul_z_zzi_d"_h) ||
              (form_hash_ == "mul_z_zzi_h"_h) ||
              (form_hash_ == "mul_z_zzi_s"_h));

  SimVRegister temp;
  dup_elements_to_segments(vform, temp, instr->GetSVEMulZmAndIndex());
  mul(vform, zd, zn, temp);
}

void Simulator::SimulateSVEMlaMlsIndex(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zda = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());

  // The encoding for B and H-sized lanes are redefined to encode the most
  // significant bit of index for H-sized lanes. B-sized lanes are not
  // supported.
  if (vform == kFormatVnB) vform = kFormatVnH;

  VIXL_ASSERT(
      (form_hash_ == "mla_z_zzzi_d"_h) || (form_hash_ == "mla_z_zzzi_h"_h) ||
      (form_hash_ == "mla_z_zzzi_s"_h) || (form_hash_ == "mls_z_zzzi_d"_h) ||
      (form_hash_ == "mls_z_zzzi_h"_h) || (form_hash_ == "mls_z_zzzi_s"_h));

  SimVRegister temp;
  dup_elements_to_segments(vform, temp, instr->GetSVEMulZmAndIndex());
  if (instr->ExtractBit(10) == 0) {
    mla(vform, zda, zda, zn, temp);
  } else {
    mls(vform, zda, zda, zn, temp);
  }
}

void Simulator::SimulateSVESaturatingMulHighIndex(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());

  // The encoding for B and H-sized lanes are redefined to encode the most
  // significant bit of index for H-sized lanes. B-sized lanes are not
  // supported.
  if (vform == kFormatVnB) {
    vform = kFormatVnH;
  }

  SimVRegister temp;
  dup_elements_to_segments(vform, temp, instr->GetSVEMulZmAndIndex());
  switch (form_hash_) {
    case "sqdmulh_z_zzi_h"_h:
    case "sqdmulh_z_zzi_s"_h:
    case "sqdmulh_z_zzi_d"_h:
      sqdmulh(vform, zd, zn, temp);
      break;
    case "sqrdmulh_z_zzi_h"_h:
    case "sqrdmulh_z_zzi_s"_h:
    case "sqrdmulh_z_zzi_d"_h:
      sqrdmulh(vform, zd, zn, temp);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
}

void Simulator::SimulateSVESaturatingIntMulLongIdx(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());

  SimVRegister temp, zm_idx, zn_b, zn_t;
  // Instead of calling the indexed form of the instruction logic, we call the
  // vector form, which can reuse existing function logic without modification.
  // Select the specified elements based on the index input and than pack them
  // to the corresponding position.
  VectorFormat vform_half = VectorFormatHalfWidth(vform);
  dup_elements_to_segments(vform_half, temp, instr->GetSVEMulLongZmAndIndex());
  pack_even_elements(vform_half, zm_idx, temp);

  pack_even_elements(vform_half, zn_b, zn);
  pack_odd_elements(vform_half, zn_t, zn);

  switch (form_hash_) {
    case "smullb_z_zzi_s"_h:
    case "smullb_z_zzi_d"_h:
      smull(vform, zd, zn_b, zm_idx);
      break;
    case "smullt_z_zzi_s"_h:
    case "smullt_z_zzi_d"_h:
      smull(vform, zd, zn_t, zm_idx);
      break;
    case "sqdmullb_z_zzi_d"_h:
      sqdmull(vform, zd, zn_b, zm_idx);
      break;
    case "sqdmullt_z_zzi_d"_h:
      sqdmull(vform, zd, zn_t, zm_idx);
      break;
    case "umullb_z_zzi_s"_h:
    case "umullb_z_zzi_d"_h:
      umull(vform, zd, zn_b, zm_idx);
      break;
    case "umullt_z_zzi_s"_h:
    case "umullt_z_zzi_d"_h:
      umull(vform, zd, zn_t, zm_idx);
      break;
    case "sqdmullb_z_zzi_s"_h:
      sqdmull(vform, zd, zn_b, zm_idx);
      break;
    case "sqdmullt_z_zzi_s"_h:
      sqdmull(vform, zd, zn_t, zm_idx);
      break;
    case "smlalb_z_zzzi_s"_h:
    case "smlalb_z_zzzi_d"_h:
      smlal(vform, zd, zn_b, zm_idx);
      break;
    case "smlalt_z_zzzi_s"_h:
    case "smlalt_z_zzzi_d"_h:
      smlal(vform, zd, zn_t, zm_idx);
      break;
    case "smlslb_z_zzzi_s"_h:
    case "smlslb_z_zzzi_d"_h:
      smlsl(vform, zd, zn_b, zm_idx);
      break;
    case "smlslt_z_zzzi_s"_h:
    case "smlslt_z_zzzi_d"_h:
      smlsl(vform, zd, zn_t, zm_idx);
      break;
    case "umlalb_z_zzzi_s"_h:
    case "umlalb_z_zzzi_d"_h:
      umlal(vform, zd, zn_b, zm_idx);
      break;
    case "umlalt_z_zzzi_s"_h:
    case "umlalt_z_zzzi_d"_h:
      umlal(vform, zd, zn_t, zm_idx);
      break;
    case "umlslb_z_zzzi_s"_h:
    case "umlslb_z_zzzi_d"_h:
      umlsl(vform, zd, zn_b, zm_idx);
      break;
    case "umlslt_z_zzzi_s"_h:
    case "umlslt_z_zzzi_d"_h:
      umlsl(vform, zd, zn_t, zm_idx);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
}

void Simulator::Simulate_ZdH_PgM_ZnS(const Instruction* instr) {
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimVRegister result, zd_b, zero;

  zero.Clear();
  pack_even_elements(kFormatVnH, zd_b, zd);

  switch (form_hash_) {
    case "fcvtnt_z_p_z_s2h"_h:
      fcvt(kFormatVnH, kFormatVnS, result, pg, zn);
      pack_even_elements(kFormatVnH, result, result);
      zip1(kFormatVnH, result, zd_b, result);
      break;
    case "bfcvt_z_p_z_s2bf"_h:
      bfcvtn(kFormatVnH, result, zn);
      zip1(kFormatVnH, result, result, zero);
      break;
    case "bfcvtnt_z_p_z_s2bf"_h:
      bfcvtn(kFormatVnH, result, zn);
      zip1(kFormatVnH, result, zd_b, result);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
  mov_merging(kFormatVnS, zd, pg, result);
}

void Simulator::Simulate_ZdS_PgM_ZnD(const Instruction* instr) {
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimVRegister result, zero, zd_b;

  zero.Clear();
  pack_even_elements(kFormatVnS, zd_b, zd);

  switch (form_hash_) {
    case "fcvtnt_z_p_z_d2s"_h:
      fcvt(kFormatVnS, kFormatVnD, result, pg, zn);
      pack_even_elements(kFormatVnS, result, result);
      zip1(kFormatVnS, result, zd_b, result);
      break;
    case "fcvtx_z_p_z_d2s"_h:
      fcvtxn(kFormatVnS, result, zn);
      zip1(kFormatVnS, result, result, zero);
      break;
    case "fcvtxnt_z_p_z_d2s"_h:
      fcvtxn(kFormatVnS, result, zn);
      zip1(kFormatVnS, result, zd_b, result);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
  mov_merging(kFormatVnD, zd, pg, result);
}

void Simulator::SimulateSVEFPConvertLong(const Instruction* instr) {
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimVRegister result;

  switch (form_hash_) {
    case "fcvtlt_z_p_z_h2s"_h:
      ext(kFormatVnB, result, zn, zn, kHRegSizeInBytes);
      fcvt(kFormatVnS, kFormatVnH, zd, pg, result);
      break;
    case "fcvtlt_z_p_z_s2d"_h:
      ext(kFormatVnB, result, zn, zn, kSRegSizeInBytes);
      fcvt(kFormatVnD, kFormatVnS, zd, pg, result);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
}

void Simulator::Simulate_ZdS_PgM_ZnS(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimVRegister result;

  if (vform != kFormatVnS) {
    VIXL_UNIMPLEMENTED();
  }

  switch (form_hash_) {
    case "urecpe_z_p_z"_h:
      urecpe(vform, result, zn);
      break;
    case "ursqrte_z_p_z"_h:
      ursqrte(vform, result, zn);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
  mov_merging(vform, zd, pg, result);
}

void Simulator::Simulate_ZdT_PgM_ZnT(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimVRegister result;

  switch (form_hash_) {
    case "flogb_z_p_z"_h:
      vform = instr->GetSVEVectorFormat(17);
      flogb(vform, result, zn);
      break;
    case "sqabs_z_p_z"_h:
      abs(vform, result, zn).SignedSaturate(vform);
      break;
    case "sqneg_z_p_z"_h:
      neg(vform, result, zn).SignedSaturate(vform);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
  mov_merging(vform, zd, pg, result);
}

void Simulator::Simulate_ZdT_PgZ_ZnT_ZmT(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRm());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimVRegister result;

  VIXL_ASSERT(form_hash_ == "histcnt_z_p_zz"_h);
  if ((vform == kFormatVnS) || (vform == kFormatVnD)) {
    histogram(vform, result, pg, zn, zm);
    mov_zeroing(vform, zd, pg, result);
  } else {
    VIXL_UNIMPLEMENTED();
  }
}

void Simulator::Simulate_ZdT_ZnT_ZmT(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRm());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimVRegister result;
  bool do_bext = false;

  switch (form_hash_) {
    case "bdep_z_zz"_h:
      bdep(vform, zd, zn, zm);
      break;
    case "bext_z_zz"_h:
      do_bext = true;
      VIXL_FALLTHROUGH();
    case "bgrp_z_zz"_h:
      bgrp(vform, zd, zn, zm, do_bext);
      break;
    case "eorbt_z_zz"_h:
      rotate_elements_right(vform, result, zm, 1);
      SVEBitwiseLogicalUnpredicatedHelper(EOR, kFormatVnD, result, zn, result);
      mov_alternating(vform, zd, result, 0);
      break;
    case "eortb_z_zz"_h:
      rotate_elements_right(vform, result, zm, -1);
      SVEBitwiseLogicalUnpredicatedHelper(EOR, kFormatVnD, result, zn, result);
      mov_alternating(vform, zd, result, 1);
      break;
    case "mul_z_zz"_h:
      mul(vform, zd, zn, zm);
      break;
    case "smulh_z_zz"_h:
      smulh(vform, zd, zn, zm);
      break;
    case "sqdmulh_z_zz"_h:
      sqdmulh(vform, zd, zn, zm);
      break;
    case "sqrdmulh_z_zz"_h:
      sqrdmulh(vform, zd, zn, zm);
      break;
    case "umulh_z_zz"_h:
      umulh(vform, zd, zn, zm);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
}

void Simulator::Simulate_ZdT_ZnT_ZmTb(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRm());
  SimVRegister& zn = ReadVRegister(instr->GetRn());

  SimVRegister zm_b, zm_t;
  VectorFormat vform_half = VectorFormatHalfWidth(vform);
  pack_even_elements(vform_half, zm_b, zm);
  pack_odd_elements(vform_half, zm_t, zm);

  switch (form_hash_) {
    case "saddwb_z_zz"_h:
      saddw(vform, zd, zn, zm_b);
      break;
    case "saddwt_z_zz"_h:
      saddw(vform, zd, zn, zm_t);
      break;
    case "ssubwb_z_zz"_h:
      ssubw(vform, zd, zn, zm_b);
      break;
    case "ssubwt_z_zz"_h:
      ssubw(vform, zd, zn, zm_t);
      break;
    case "uaddwb_z_zz"_h:
      uaddw(vform, zd, zn, zm_b);
      break;
    case "uaddwt_z_zz"_h:
      uaddw(vform, zd, zn, zm_t);
      break;
    case "usubwb_z_zz"_h:
      usubw(vform, zd, zn, zm_b);
      break;
    case "usubwt_z_zz"_h:
      usubw(vform, zd, zn, zm_t);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
}

void Simulator::Simulate_ZdT_ZnT_const(const Instruction* instr) {
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());

  std::pair<int, int> shift_and_lane_size =
      instr->GetSVEImmShiftAndLaneSizeLog2(/* is_predicated = */ false);
  int lane_size = shift_and_lane_size.second;
  VIXL_ASSERT((lane_size >= 0) &&
              (static_cast<unsigned>(lane_size) <= kDRegSizeInBytesLog2));
  VectorFormat vform = SVEFormatFromLaneSizeInBytesLog2(lane_size);
  int shift_dist = shift_and_lane_size.first;

  switch (form_hash_) {
    case "sli_z_zzi"_h:
      // Shift distance is computed differently for left shifts. Convert the
      // result.
      shift_dist = (8 << lane_size) - shift_dist;
      sli(vform, zd, zn, shift_dist);
      break;
    case "sri_z_zzi"_h:
      sri(vform, zd, zn, shift_dist);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
}

void Simulator::SimulateSVENarrow(const Instruction* instr) {
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimVRegister result;

  std::pair<int, int> shift_and_lane_size =
      instr->GetSVEImmShiftAndLaneSizeLog2(/* is_predicated = */ false);
  int lane_size = shift_and_lane_size.second;
  VIXL_ASSERT((lane_size >= static_cast<int>(kBRegSizeInBytesLog2)) &&
              (lane_size <= static_cast<int>(kSRegSizeInBytesLog2)));
  VectorFormat vform = SVEFormatFromLaneSizeInBytesLog2(lane_size);
  int right_shift_dist = shift_and_lane_size.first;
  bool top = false;

  switch (form_hash_) {
    case "sqxtnt_z_zz"_h:
      top = true;
      VIXL_FALLTHROUGH();
    case "sqxtnb_z_zz"_h:
      sqxtn(vform, result, zn);
      break;
    case "sqxtunt_z_zz"_h:
      top = true;
      VIXL_FALLTHROUGH();
    case "sqxtunb_z_zz"_h:
      sqxtun(vform, result, zn);
      break;
    case "uqxtnt_z_zz"_h:
      top = true;
      VIXL_FALLTHROUGH();
    case "uqxtnb_z_zz"_h:
      uqxtn(vform, result, zn);
      break;
    case "rshrnt_z_zi"_h:
      top = true;
      VIXL_FALLTHROUGH();
    case "rshrnb_z_zi"_h:
      rshrn(vform, result, zn, right_shift_dist);
      break;
    case "shrnt_z_zi"_h:
      top = true;
      VIXL_FALLTHROUGH();
    case "shrnb_z_zi"_h:
      shrn(vform, result, zn, right_shift_dist);
      break;
    case "sqrshrnt_z_zi"_h:
      top = true;
      VIXL_FALLTHROUGH();
    case "sqrshrnb_z_zi"_h:
      sqrshrn(vform, result, zn, right_shift_dist);
      break;
    case "sqrshrunt_z_zi"_h:
      top = true;
      VIXL_FALLTHROUGH();
    case "sqrshrunb_z_zi"_h:
      sqrshrun(vform, result, zn, right_shift_dist);
      break;
    case "sqshrnt_z_zi"_h:
      top = true;
      VIXL_FALLTHROUGH();
    case "sqshrnb_z_zi"_h:
      sqshrn(vform, result, zn, right_shift_dist);
      break;
    case "sqshrunt_z_zi"_h:
      top = true;
      VIXL_FALLTHROUGH();
    case "sqshrunb_z_zi"_h:
      sqshrun(vform, result, zn, right_shift_dist);
      break;
    case "uqrshrnt_z_zi"_h:
      top = true;
      VIXL_FALLTHROUGH();
    case "uqrshrnb_z_zi"_h:
      uqrshrn(vform, result, zn, right_shift_dist);
      break;
    case "uqshrnt_z_zi"_h:
      top = true;
      VIXL_FALLTHROUGH();
    case "uqshrnb_z_zi"_h:
      uqshrn(vform, result, zn, right_shift_dist);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }

  if (top) {
    // Keep even elements, replace odd elements with the results.
    xtn(vform, zd, zd);
    zip1(vform, zd, zd, result);
  } else {
    // Zero odd elements, replace even elements with the results.
    SimVRegister zero;
    zero.Clear();
    zip1(vform, zd, result, zero);
  }
}

void Simulator::SimulateSVEInterleavedArithLong(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRm());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimVRegister temp, zn_b, zm_b, zn_t, zm_t;

  // Construct temporary registers containing the even (bottom) and odd (top)
  // elements.
  VectorFormat vform_half = VectorFormatHalfWidth(vform);
  pack_even_elements(vform_half, zn_b, zn);
  pack_even_elements(vform_half, zm_b, zm);
  pack_odd_elements(vform_half, zn_t, zn);
  pack_odd_elements(vform_half, zm_t, zm);

  switch (form_hash_) {
    case "sabdlb_z_zz"_h:
      sabdl(vform, zd, zn_b, zm_b);
      break;
    case "sabdlt_z_zz"_h:
      sabdl(vform, zd, zn_t, zm_t);
      break;
    case "saddlb_z_zz"_h:
      saddl(vform, zd, zn_b, zm_b);
      break;
    case "saddlbt_z_zz"_h:
      saddl(vform, zd, zn_b, zm_t);
      break;
    case "saddlt_z_zz"_h:
      saddl(vform, zd, zn_t, zm_t);
      break;
    case "ssublb_z_zz"_h:
      ssubl(vform, zd, zn_b, zm_b);
      break;
    case "ssublbt_z_zz"_h:
      ssubl(vform, zd, zn_b, zm_t);
      break;
    case "ssublt_z_zz"_h:
      ssubl(vform, zd, zn_t, zm_t);
      break;
    case "ssubltb_z_zz"_h:
      ssubl(vform, zd, zn_t, zm_b);
      break;
    case "uabdlb_z_zz"_h:
      uabdl(vform, zd, zn_b, zm_b);
      break;
    case "uabdlt_z_zz"_h:
      uabdl(vform, zd, zn_t, zm_t);
      break;
    case "uaddlb_z_zz"_h:
      uaddl(vform, zd, zn_b, zm_b);
      break;
    case "uaddlt_z_zz"_h:
      uaddl(vform, zd, zn_t, zm_t);
      break;
    case "usublb_z_zz"_h:
      usubl(vform, zd, zn_b, zm_b);
      break;
    case "usublt_z_zz"_h:
      usubl(vform, zd, zn_t, zm_t);
      break;
    case "sabalb_z_zzz"_h:
      sabal(vform, zd, zn_b, zm_b);
      break;
    case "sabalt_z_zzz"_h:
      sabal(vform, zd, zn_t, zm_t);
      break;
    case "uabalb_z_zzz"_h:
      uabal(vform, zd, zn_b, zm_b);
      break;
    case "uabalt_z_zzz"_h:
      uabal(vform, zd, zn_t, zm_t);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
}

void Simulator::SimulateSVEPmull128(const Instruction* instr) {
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRm());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimVRegister zn_temp, zm_temp;

  if (form_hash_ == "pmullb_z_zz_q"_h) {
    pack_even_elements(kFormatVnD, zn_temp, zn);
    pack_even_elements(kFormatVnD, zm_temp, zm);
  } else {
    VIXL_ASSERT(form_hash_ == "pmullt_z_zz_q"_h);
    pack_odd_elements(kFormatVnD, zn_temp, zn);
    pack_odd_elements(kFormatVnD, zm_temp, zm);
  }
  pmull(kFormatVnQ, zd, zn_temp, zm_temp);
}

void Simulator::SimulateSVEIntMulLongVec(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRm());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimVRegister temp, zn_b, zm_b, zn_t, zm_t;
  VectorFormat vform_half = VectorFormatHalfWidth(vform);
  pack_even_elements(vform_half, zn_b, zn);
  pack_even_elements(vform_half, zm_b, zm);
  pack_odd_elements(vform_half, zn_t, zn);
  pack_odd_elements(vform_half, zm_t, zm);

  switch (form_hash_) {
    case "pmullb_z_zz"_h:
      // Size '10' is undefined.
      if (vform == kFormatVnS) {
        VIXL_UNIMPLEMENTED();
      }
      pmull(vform, zd, zn_b, zm_b);
      break;
    case "pmullt_z_zz"_h:
      // Size '10' is undefined.
      if (vform == kFormatVnS) {
        VIXL_UNIMPLEMENTED();
      }
      pmull(vform, zd, zn_t, zm_t);
      break;
    case "smullb_z_zz"_h:
      smull(vform, zd, zn_b, zm_b);
      break;
    case "smullt_z_zz"_h:
      smull(vform, zd, zn_t, zm_t);
      break;
    case "sqdmullb_z_zz"_h:
      sqdmull(vform, zd, zn_b, zm_b);
      break;
    case "sqdmullt_z_zz"_h:
      sqdmull(vform, zd, zn_t, zm_t);
      break;
    case "umullb_z_zz"_h:
      umull(vform, zd, zn_b, zm_b);
      break;
    case "umullt_z_zz"_h:
      umull(vform, zd, zn_t, zm_t);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
}

void Simulator::SimulateSVEAddSubHigh(const Instruction* instr) {
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRm());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimVRegister result;
  bool top = false;

  VectorFormat vform_src = instr->GetSVEVectorFormat();
  if (vform_src == kFormatVnB) {
    VIXL_UNIMPLEMENTED();
  }
  VectorFormat vform = VectorFormatHalfWidth(vform_src);

  switch (form_hash_) {
    case "addhnt_z_zz"_h:
      top = true;
      VIXL_FALLTHROUGH();
    case "addhnb_z_zz"_h:
      addhn(vform, result, zn, zm);
      break;
    case "raddhnt_z_zz"_h:
      top = true;
      VIXL_FALLTHROUGH();
    case "raddhnb_z_zz"_h:
      raddhn(vform, result, zn, zm);
      break;
    case "rsubhnt_z_zz"_h:
      top = true;
      VIXL_FALLTHROUGH();
    case "rsubhnb_z_zz"_h:
      rsubhn(vform, result, zn, zm);
      break;
    case "subhnt_z_zz"_h:
      top = true;
      VIXL_FALLTHROUGH();
    case "subhnb_z_zz"_h:
      subhn(vform, result, zn, zm);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }

  if (top) {
    // Keep even elements, replace odd elements with the results.
    xtn(vform, zd, zd);
    zip1(vform, zd, zd, result);
  } else {
    // Zero odd elements, replace even elements with the results.
    SimVRegister zero;
    zero.Clear();
    zip1(vform, zd, result, zero);
  }
}

void Simulator::SimulateSVEShiftLeftImm(const Instruction* instr) {
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimVRegister zn_b, zn_t;

  std::pair<int, int> shift_and_lane_size =
      instr->GetSVEImmShiftAndLaneSizeLog2(/* is_predicated = */ false);
  int lane_size = shift_and_lane_size.second;
  VIXL_ASSERT((lane_size >= 0) &&
              (static_cast<unsigned>(lane_size) <= kDRegSizeInBytesLog2));
  VectorFormat vform = SVEFormatFromLaneSizeInBytesLog2(lane_size + 1);
  int right_shift_dist = shift_and_lane_size.first;
  int left_shift_dist = (8 << lane_size) - right_shift_dist;

  // Construct temporary registers containing the even (bottom) and odd (top)
  // elements.
  VectorFormat vform_half = VectorFormatHalfWidth(vform);
  pack_even_elements(vform_half, zn_b, zn);
  pack_odd_elements(vform_half, zn_t, zn);

  switch (form_hash_) {
    case "sshllb_z_zi"_h:
      sshll(vform, zd, zn_b, left_shift_dist);
      break;
    case "sshllt_z_zi"_h:
      sshll(vform, zd, zn_t, left_shift_dist);
      break;
    case "ushllb_z_zi"_h:
      ushll(vform, zd, zn_b, left_shift_dist);
      break;
    case "ushllt_z_zi"_h:
      ushll(vform, zd, zn_t, left_shift_dist);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
}

void Simulator::SimulateSVESaturatingMulAddHigh(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zda = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  unsigned zm_code = instr->GetRm();
  int index = -1;
  bool is_mla = false;

  switch (form_hash_) {
    case "sqrdmlah_z_zzz"_h:
      is_mla = true;
      VIXL_FALLTHROUGH();
    case "sqrdmlsh_z_zzz"_h:
      // Nothing to do.
      break;
    case "sqrdmlah_z_zzzi_h"_h:
      is_mla = true;
      VIXL_FALLTHROUGH();
    case "sqrdmlsh_z_zzzi_h"_h:
      vform = kFormatVnH;
      index = (instr->ExtractBit(22) << 2) | instr->ExtractBits(20, 19);
      zm_code = instr->ExtractBits(18, 16);
      break;
    case "sqrdmlah_z_zzzi_s"_h:
      is_mla = true;
      VIXL_FALLTHROUGH();
    case "sqrdmlsh_z_zzzi_s"_h:
      vform = kFormatVnS;
      index = instr->ExtractBits(20, 19);
      zm_code = instr->ExtractBits(18, 16);
      break;
    case "sqrdmlah_z_zzzi_d"_h:
      is_mla = true;
      VIXL_FALLTHROUGH();
    case "sqrdmlsh_z_zzzi_d"_h:
      vform = kFormatVnD;
      index = instr->ExtractBit(20);
      zm_code = instr->ExtractBits(19, 16);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }

  SimVRegister& zm = ReadVRegister(zm_code);
  SimVRegister zm_idx;
  if (index >= 0) {
    dup_elements_to_segments(vform, zm_idx, zm, index);
  }

  if (is_mla) {
    sqrdmlah(vform, zda, zn, (index >= 0) ? zm_idx : zm);
  } else {
    sqrdmlsh(vform, zda, zn, (index >= 0) ? zm_idx : zm);
  }
}

void Simulator::Simulate_ZdaD_ZnS_ZmS_imm(const Instruction* instr) {
  SimVRegister& zda = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimVRegister& zm = ReadVRegister(instr->ExtractBits(19, 16));

  SimVRegister temp, zm_idx, zn_b, zn_t;
  Instr index = (instr->ExtractBit(20) << 1) | instr->ExtractBit(11);
  dup_elements_to_segments(kFormatVnS, temp, zm, index);
  pack_even_elements(kFormatVnS, zm_idx, temp);
  pack_even_elements(kFormatVnS, zn_b, zn);
  pack_odd_elements(kFormatVnS, zn_t, zn);

  switch (form_hash_) {
    case "sqdmlalb_z_zzzi_d"_h:
      sqdmlal(kFormatVnD, zda, zn_b, zm_idx);
      break;
    case "sqdmlalt_z_zzzi_d"_h:
      sqdmlal(kFormatVnD, zda, zn_t, zm_idx);
      break;
    case "sqdmlslb_z_zzzi_d"_h:
      sqdmlsl(kFormatVnD, zda, zn_b, zm_idx);
      break;
    case "sqdmlslt_z_zzzi_d"_h:
      sqdmlsl(kFormatVnD, zda, zn_t, zm_idx);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
}

void Simulator::Simulate_ZdaS_ZnH_ZmH(const Instruction* instr) {
  SimVRegister& zda = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRm());
  SimVRegister& zn = ReadVRegister(instr->GetRn());

  SimVRegister temp, zn_b, zm_b, zn_t, zm_t;
  pack_even_elements(kFormatVnH, zn_b, zn);
  pack_even_elements(kFormatVnH, zm_b, zm);
  pack_odd_elements(kFormatVnH, zn_t, zn);
  pack_odd_elements(kFormatVnH, zm_t, zm);

  switch (form_hash_) {
    case "fmlalb_z_zzz"_h:
      fmlal(kFormatVnS, zda, zn_b, zm_b);
      break;
    case "fmlalt_z_zzz"_h:
      fmlal(kFormatVnS, zda, zn_t, zm_t);
      break;
    case "fmlslb_z_zzz"_h:
      fmlsl(kFormatVnS, zda, zn_b, zm_b);
      break;
    case "fmlslt_z_zzz"_h:
      fmlsl(kFormatVnS, zda, zn_t, zm_t);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
}

void Simulator::Simulate_ZdaS_ZnH_ZmH_imm(const Instruction* instr) {
  SimVRegister& zda = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimVRegister& zm = ReadVRegister(instr->ExtractBits(18, 16));

  SimVRegister temp, zm_idx, zn_b, zn_t;
  Instr index = (instr->ExtractBits(20, 19) << 1) | instr->ExtractBit(11);
  dup_elements_to_segments(kFormatVnH, temp, zm, index);
  pack_even_elements(kFormatVnH, zm_idx, temp);
  pack_even_elements(kFormatVnH, zn_b, zn);
  pack_odd_elements(kFormatVnH, zn_t, zn);

  switch (form_hash_) {
    case "fmlalb_z_zzzi_s"_h:
      fmlal(kFormatVnS, zda, zn_b, zm_idx);
      break;
    case "fmlalt_z_zzzi_s"_h:
      fmlal(kFormatVnS, zda, zn_t, zm_idx);
      break;
    case "fmlslb_z_zzzi_s"_h:
      fmlsl(kFormatVnS, zda, zn_b, zm_idx);
      break;
    case "fmlslt_z_zzzi_s"_h:
      fmlsl(kFormatVnS, zda, zn_t, zm_idx);
      break;
    case "sqdmlalb_z_zzzi_s"_h:
      sqdmlal(kFormatVnS, zda, zn_b, zm_idx);
      break;
    case "sqdmlalt_z_zzzi_s"_h:
      sqdmlal(kFormatVnS, zda, zn_t, zm_idx);
      break;
    case "sqdmlslb_z_zzzi_s"_h:
      sqdmlsl(kFormatVnS, zda, zn_b, zm_idx);
      break;
    case "sqdmlslt_z_zzzi_s"_h:
      sqdmlsl(kFormatVnS, zda, zn_t, zm_idx);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
}

void Simulator::Simulate_ZdaT_PgM_ZnTb(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  SimVRegister& zda = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimVRegister result;

  switch (form_hash_) {
    case "sadalp_z_p_z"_h:
      sadalp(vform, result, zn);
      break;
    case "uadalp_z_p_z"_h:
      uadalp(vform, result, zn);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
  mov_merging(vform, zda, pg, result);
}

void Simulator::SimulateSVEAddSubCarry(const Instruction* instr) {
  VectorFormat vform = (instr->ExtractBit(22) == 0) ? kFormatVnS : kFormatVnD;
  SimVRegister& zda = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRm());
  SimVRegister& zn = ReadVRegister(instr->GetRn());

  SimVRegister not_zn;
  not_(vform, not_zn, zn);

  switch (form_hash_) {
    case "adclb_z_zzz"_h:
      adcl(vform, zda, zn, zm, /* top = */ false);
      break;
    case "adclt_z_zzz"_h:
      adcl(vform, zda, zn, zm, /* top = */ true);
      break;
    case "sbclb_z_zzz"_h:
      adcl(vform, zda, not_zn, zm, /* top = */ false);
      break;
    case "sbclt_z_zzz"_h:
      adcl(vform, zda, not_zn, zm, /* top = */ true);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
}

void Simulator::Simulate_ZdaT_ZnT_ZmT(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zda = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRm());
  SimVRegister& zn = ReadVRegister(instr->GetRn());

  switch (form_hash_) {
    case "saba_z_zzz"_h:
      saba(vform, zda, zn, zm);
      break;
    case "uaba_z_zzz"_h:
      uaba(vform, zda, zn, zm);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
}

void Simulator::SimulateSVEComplexIntMulAdd(const Instruction* instr) {
  SimVRegister& zda = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  int rot = instr->ExtractBits(11, 10) * 90;
  // vform and zm are only valid for the vector form of instruction.
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zm = ReadVRegister(instr->GetRm());

  // Inputs for indexed form of instruction.
  SimVRegister& zm_h = ReadVRegister(instr->ExtractBits(18, 16));
  SimVRegister& zm_s = ReadVRegister(instr->ExtractBits(19, 16));
  int idx_h = instr->ExtractBits(20, 19);
  int idx_s = instr->ExtractBit(20);

  switch (form_hash_) {
    case "cmla_z_zzz"_h:
      cmla(vform, zda, zda, zn, zm, rot);
      break;
    case "cmla_z_zzzi_h"_h:
      cmla(kFormatVnH, zda, zda, zn, zm_h, idx_h, rot);
      break;
    case "cmla_z_zzzi_s"_h:
      cmla(kFormatVnS, zda, zda, zn, zm_s, idx_s, rot);
      break;
    case "sqrdcmlah_z_zzz"_h:
      sqrdcmlah(vform, zda, zda, zn, zm, rot);
      break;
    case "sqrdcmlah_z_zzzi_h"_h:
      sqrdcmlah(kFormatVnH, zda, zda, zn, zm_h, idx_h, rot);
      break;
    case "sqrdcmlah_z_zzzi_s"_h:
      sqrdcmlah(kFormatVnS, zda, zda, zn, zm_s, idx_s, rot);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
}

void Simulator::Simulate_ZdaT_ZnT_const(const Instruction* instr) {
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());

  std::pair<int, int> shift_and_lane_size =
      instr->GetSVEImmShiftAndLaneSizeLog2(/* is_predicated = */ false);
  int lane_size = shift_and_lane_size.second;
  VIXL_ASSERT((lane_size >= 0) &&
              (static_cast<unsigned>(lane_size) <= kDRegSizeInBytesLog2));
  VectorFormat vform = SVEFormatFromLaneSizeInBytesLog2(lane_size);
  int shift_dist = shift_and_lane_size.first;

  switch (form_hash_) {
    case "srsra_z_zi"_h:
      srsra(vform, zd, zn, shift_dist);
      break;
    case "ssra_z_zi"_h:
      ssra(vform, zd, zn, shift_dist);
      break;
    case "ursra_z_zi"_h:
      ursra(vform, zd, zn, shift_dist);
      break;
    case "usra_z_zi"_h:
      usra(vform, zd, zn, shift_dist);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
}

void Simulator::Simulate_ZdaT_ZnTb_ZmTb(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zda = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRm());
  SimVRegister& zn = ReadVRegister(instr->GetRn());

  SimVRegister zero, zn_b, zm_b, zn_t, zm_t;
  zero.Clear();

  VectorFormat vform_half = VectorFormatHalfWidth(vform);
  uzp1(vform_half, zn_b, zn, zero);
  uzp1(vform_half, zm_b, zm, zero);
  uzp2(vform_half, zn_t, zn, zero);
  uzp2(vform_half, zm_t, zm, zero);

  switch (form_hash_) {
    case "smlalb_z_zzz"_h:
      smlal(vform, zda, zn_b, zm_b);
      break;
    case "smlalt_z_zzz"_h:
      smlal(vform, zda, zn_t, zm_t);
      break;
    case "smlslb_z_zzz"_h:
      smlsl(vform, zda, zn_b, zm_b);
      break;
    case "smlslt_z_zzz"_h:
      smlsl(vform, zda, zn_t, zm_t);
      break;
    case "sqdmlalb_z_zzz"_h:
      sqdmlal(vform, zda, zn_b, zm_b);
      break;
    case "sqdmlalbt_z_zzz"_h:
      sqdmlal(vform, zda, zn_b, zm_t);
      break;
    case "sqdmlalt_z_zzz"_h:
      sqdmlal(vform, zda, zn_t, zm_t);
      break;
    case "sqdmlslb_z_zzz"_h:
      sqdmlsl(vform, zda, zn_b, zm_b);
      break;
    case "sqdmlslbt_z_zzz"_h:
      sqdmlsl(vform, zda, zn_b, zm_t);
      break;
    case "sqdmlslt_z_zzz"_h:
      sqdmlsl(vform, zda, zn_t, zm_t);
      break;
    case "umlalb_z_zzz"_h:
      umlal(vform, zda, zn_b, zm_b);
      break;
    case "umlalt_z_zzz"_h:
      umlal(vform, zda, zn_t, zm_t);
      break;
    case "umlslb_z_zzz"_h:
      umlsl(vform, zda, zn_b, zm_b);
      break;
    case "umlslt_z_zzz"_h:
      umlsl(vform, zda, zn_t, zm_t);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
}

void Simulator::SimulateSVEComplexDotProduct(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zda = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  int rot = instr->ExtractBits(11, 10) * 90;
  unsigned zm_code = instr->GetRm();
  int index = -1;

  switch (form_hash_) {
    case "cdot_z_zzz"_h:
      // Nothing to do.
      break;
    case "cdot_z_zzzi_s"_h:
      index = zm_code >> 3;
      zm_code &= 0x7;
      break;
    case "cdot_z_zzzi_d"_h:
      index = zm_code >> 4;
      zm_code &= 0xf;
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }

  SimVRegister temp;
  SimVRegister& zm = ReadVRegister(zm_code);
  if (index >= 0) dup_elements_to_segments(vform, temp, zm, index);
  cdot(vform, zda, zda, zn, (index >= 0) ? temp : zm, rot);
}

void Simulator::SimulateSVEBitwiseTernary(const Instruction* instr) {
  VectorFormat vform = kFormatVnD;
  SimVRegister& zdn = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRm());
  SimVRegister& zk = ReadVRegister(instr->GetRn());
  SimVRegister temp;

  switch (form_hash_) {
    case "bcax_z_zzz"_h:
      bic(vform, temp, zm, zk);
      eor(vform, zdn, temp, zdn);
      break;
    case "bsl1n_z_zzz"_h:
      not_(vform, temp, zdn);
      bsl(vform, zdn, zk, temp, zm);
      break;
    case "bsl2n_z_zzz"_h:
      not_(vform, temp, zm);
      bsl(vform, zdn, zk, zdn, temp);
      break;
    case "bsl_z_zzz"_h:
      bsl(vform, zdn, zk, zdn, zm);
      break;
    case "eor3_z_zzz"_h:
      eor(vform, temp, zdn, zm);
      eor(vform, zdn, temp, zk);
      break;
    case "nbsl_z_zzz"_h:
      bsl(vform, zdn, zk, zdn, zm);
      not_(vform, zdn, zdn);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
}

void Simulator::SimulateSVEHalvingAddSub(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  SimVRegister& zdn = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRn());
  SimVRegister result;

  switch (form_hash_) {
    case "shadd_z_p_zz"_h:
      add(vform, result, zdn, zm).Halve(vform);
      break;
    case "shsub_z_p_zz"_h:
      sub(vform, result, zdn, zm).Halve(vform);
      break;
    case "shsubr_z_p_zz"_h:
      sub(vform, result, zm, zdn).Halve(vform);
      break;
    case "srhadd_z_p_zz"_h:
      add(vform, result, zdn, zm).Halve(vform).Round(vform);
      break;
    case "uhadd_z_p_zz"_h:
      add(vform, result, zdn, zm).Uhalve(vform);
      break;
    case "uhsub_z_p_zz"_h:
      sub(vform, result, zdn, zm).Uhalve(vform);
      break;
    case "uhsubr_z_p_zz"_h:
      sub(vform, result, zm, zdn).Uhalve(vform);
      break;
    case "urhadd_z_p_zz"_h:
      add(vform, result, zdn, zm).Uhalve(vform).Round(vform);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
  mov_merging(vform, zdn, pg, result);
}

void Simulator::SimulateSVESaturatingArithmetic(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zdn = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRn());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  SimVRegister result;

  switch (form_hash_) {
    case "sqadd_z_p_zz"_h:
      add(vform, result, zdn, zm).SignedSaturate(vform);
      break;
    case "sqsub_z_p_zz"_h:
      sub(vform, result, zdn, zm).SignedSaturate(vform);
      break;
    case "sqsubr_z_p_zz"_h:
      sub(vform, result, zm, zdn).SignedSaturate(vform);
      break;
    case "suqadd_z_p_zz"_h:
      suqadd(vform, result, zdn, zm);
      break;
    case "uqadd_z_p_zz"_h:
      add(vform, result, zdn, zm).UnsignedSaturate(vform);
      break;
    case "uqsub_z_p_zz"_h:
      sub(vform, result, zdn, zm).UnsignedSaturate(vform);
      break;
    case "uqsubr_z_p_zz"_h:
      sub(vform, result, zm, zdn).UnsignedSaturate(vform);
      break;
    case "usqadd_z_p_zz"_h:
      usqadd(vform, result, zdn, zm);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
  mov_merging(vform, zdn, pg, result);
}

void Simulator::SimulateSVEIntArithPair(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  SimVRegister& zdn = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRn());
  SimVRegister result;

  switch (form_hash_) {
    case "addp_z_p_zz"_h:
      addp(vform, result, zdn, zm);
      break;
    case "smaxp_z_p_zz"_h:
      smaxp(vform, result, zdn, zm);
      break;
    case "sminp_z_p_zz"_h:
      sminp(vform, result, zdn, zm);
      break;
    case "umaxp_z_p_zz"_h:
      umaxp(vform, result, zdn, zm);
      break;
    case "uminp_z_p_zz"_h:
      uminp(vform, result, zdn, zm);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
  mov_merging(vform, zdn, pg, result);
}

void Simulator::Simulate_ZdnT_PgM_ZdnT_ZmT(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  SimVRegister& zdn = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRn());
  SimVRegister result;

  switch (form_hash_) {
    case "faddp_z_p_zz"_h:
      faddp(vform, result, zdn, zm);
      break;
    case "fmaxnmp_z_p_zz"_h:
      fmaxnmp(vform, result, zdn, zm);
      break;
    case "fmaxp_z_p_zz"_h:
      fmaxp(vform, result, zdn, zm);
      break;
    case "fminnmp_z_p_zz"_h:
      fminnmp(vform, result, zdn, zm);
      break;
    case "fminp_z_p_zz"_h:
      fminp(vform, result, zdn, zm);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
  mov_merging(vform, zdn, pg, result);
}

void Simulator::Simulate_ZdnT_PgM_ZdnT_const(const Instruction* instr) {
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  SimVRegister& zdn = ReadVRegister(instr->GetRd());

  std::pair<int, int> shift_and_lane_size =
      instr->GetSVEImmShiftAndLaneSizeLog2(/* is_predicated = */ true);
  unsigned lane_size = shift_and_lane_size.second;
  VectorFormat vform = SVEFormatFromLaneSizeInBytesLog2(lane_size);
  int right_shift_dist = shift_and_lane_size.first;
  int left_shift_dist = (8 << lane_size) - right_shift_dist;
  SimVRegister result;

  switch (form_hash_) {
    case "sqshl_z_p_zi"_h:
      sqshl(vform, result, zdn, left_shift_dist);
      break;
    case "sqshlu_z_p_zi"_h:
      sqshlu(vform, result, zdn, left_shift_dist);
      break;
    case "srshr_z_p_zi"_h:
      sshr(vform, result, zdn, right_shift_dist).Round(vform);
      break;
    case "uqshl_z_p_zi"_h:
      uqshl(vform, result, zdn, left_shift_dist);
      break;
    case "urshr_z_p_zi"_h:
      ushr(vform, result, zdn, right_shift_dist).Round(vform);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
  mov_merging(vform, zdn, pg, result);
}

void Simulator::SimulateSVEExclusiveOrRotate(const Instruction* instr) {
  VIXL_ASSERT(form_hash_ == "xar_z_zzi"_h);

  SimVRegister& zdn = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRn());

  std::pair<int, int> shift_and_lane_size =
      instr->GetSVEImmShiftAndLaneSizeLog2(/* is_predicated = */ false);
  unsigned lane_size = shift_and_lane_size.second;
  VIXL_ASSERT(lane_size <= kDRegSizeInBytesLog2);
  VectorFormat vform = SVEFormatFromLaneSizeInBytesLog2(lane_size);
  int shift_dist = shift_and_lane_size.first;
  eor(vform, zdn, zdn, zm);
  ror(vform, zdn, zdn, shift_dist);
}

void Simulator::Simulate_ZdnT_ZdnT_ZmT_const(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zdn = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRn());
  int rot = (instr->ExtractBit(10) == 0) ? 90 : 270;

  switch (form_hash_) {
    case "cadd_z_zz"_h:
      cadd(vform, zdn, zdn, zm, rot);
      break;
    case "sqcadd_z_zz"_h:
      cadd(vform, zdn, zdn, zm, rot, /* saturate = */ true);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
}

void Simulator::Simulate_ZtD_PgZ_ZnD_Xm(const Instruction* instr) {
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  uint64_t xm = ReadXRegister(instr->GetRm());

  LogicSVEAddressVector addr(xm, &zn, kFormatVnD);
  int msize = -1;
  bool is_signed = false;

  switch (form_hash_) {
    case "ldnt1b_z_p_ar_d_64_unscaled"_h:
      msize = 0;
      break;
    case "ldnt1d_z_p_ar_d_64_unscaled"_h:
      msize = 3;
      break;
    case "ldnt1h_z_p_ar_d_64_unscaled"_h:
      msize = 1;
      break;
    case "ldnt1sb_z_p_ar_d_64_unscaled"_h:
      msize = 0;
      is_signed = true;
      break;
    case "ldnt1sh_z_p_ar_d_64_unscaled"_h:
      msize = 1;
      is_signed = true;
      break;
    case "ldnt1sw_z_p_ar_d_64_unscaled"_h:
      msize = 2;
      is_signed = true;
      break;
    case "ldnt1w_z_p_ar_d_64_unscaled"_h:
      msize = 2;
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
  addr.SetMsizeInBytesLog2(msize);
  SVEStructuredLoadHelper(kFormatVnD, pg, instr->GetRt(), addr, is_signed);
}

void Simulator::Simulate_ZtD_Pg_ZnD_Xm(const Instruction* instr) {
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  uint64_t xm = ReadXRegister(instr->GetRm());

  LogicSVEAddressVector addr(xm, &zn, kFormatVnD);
  VIXL_ASSERT((form_hash_ == "stnt1b_z_p_ar_d_64_unscaled"_h) ||
              (form_hash_ == "stnt1d_z_p_ar_d_64_unscaled"_h) ||
              (form_hash_ == "stnt1h_z_p_ar_d_64_unscaled"_h) ||
              (form_hash_ == "stnt1w_z_p_ar_d_64_unscaled"_h));

  addr.SetMsizeInBytesLog2(
      instr->GetSVEMsizeFromDtype(/* is_signed = */ false));
  SVEStructuredStoreHelper(kFormatVnD, pg, instr->GetRt(), addr);
}

void Simulator::Simulate_ZtS_PgZ_ZnS_Xm(const Instruction* instr) {
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  uint64_t xm = ReadXRegister(instr->GetRm());

  LogicSVEAddressVector addr(xm, &zn, kFormatVnS);
  int msize = -1;
  bool is_signed = false;

  switch (form_hash_) {
    case "ldnt1b_z_p_ar_s_x32_unscaled"_h:
      msize = 0;
      break;
    case "ldnt1h_z_p_ar_s_x32_unscaled"_h:
      msize = 1;
      break;
    case "ldnt1sb_z_p_ar_s_x32_unscaled"_h:
      msize = 0;
      is_signed = true;
      break;
    case "ldnt1sh_z_p_ar_s_x32_unscaled"_h:
      msize = 1;
      is_signed = true;
      break;
    case "ldnt1w_z_p_ar_s_x32_unscaled"_h:
      msize = 2;
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
  addr.SetMsizeInBytesLog2(msize);
  SVEStructuredLoadHelper(kFormatVnS, pg, instr->GetRt(), addr, is_signed);
}

void Simulator::Simulate_ZtS_Pg_ZnS_Xm(const Instruction* instr) {
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  uint64_t xm = ReadXRegister(instr->GetRm());

  LogicSVEAddressVector addr(xm, &zn, kFormatVnS);
  VIXL_ASSERT((form_hash_ == "stnt1b_z_p_ar_s_x32_unscaled"_h) ||
              (form_hash_ == "stnt1h_z_p_ar_s_x32_unscaled"_h) ||
              (form_hash_ == "stnt1w_z_p_ar_s_x32_unscaled"_h));

  addr.SetMsizeInBytesLog2(
      instr->GetSVEMsizeFromDtype(/* is_signed = */ false));
  SVEStructuredStoreHelper(kFormatVnS, pg, instr->GetRt(), addr);
}

void Simulator::VisitReserved(const Instruction* instr) {
  // UDF is the only instruction in this group, and the Decoder is precise here.
  VIXL_ASSERT(instr->Mask(ReservedMask) == UDF);

  printf("UDF (permanently undefined) instruction at %p: 0x%08" PRIx32 "\n",
         reinterpret_cast<const void*>(instr),
         instr->GetInstructionBits());
  VIXL_ABORT_WITH_MSG("UNDEFINED (UDF)\n");
}


void Simulator::VisitUnimplemented(const Instruction* instr) {
  printf("Unimplemented instruction at %p: 0x%08" PRIx32 "\n",
         reinterpret_cast<const void*>(instr),
         instr->GetInstructionBits());
  VIXL_UNIMPLEMENTED();
}


void Simulator::VisitUnallocated(const Instruction* instr) {
  printf("Unallocated instruction at %p: 0x%08" PRIx32 "\n",
         reinterpret_cast<const void*>(instr),
         instr->GetInstructionBits());
  VIXL_UNIMPLEMENTED();
}


void Simulator::VisitPCRelAddressing(const Instruction* instr) {
  VIXL_ASSERT((instr->Mask(PCRelAddressingMask) == ADR) ||
              (instr->Mask(PCRelAddressingMask) == ADRP));

  WriteRegister(instr->GetRd(), instr->GetImmPCOffsetTarget());
}


void Simulator::VisitUnconditionalBranch(const Instruction* instr) {
  switch (instr->Mask(UnconditionalBranchMask)) {
    case BL:
      WriteLr(instr->GetNextInstruction());
      GCSPush(reinterpret_cast<uint64_t>(instr->GetNextInstruction()));
      VIXL_FALLTHROUGH();
    case B:
      WritePc(instr->GetImmPCOffsetTarget());
      break;
    default:
      VIXL_UNREACHABLE();
  }
}


void Simulator::VisitConditionalBranch(const Instruction* instr) {
  VIXL_ASSERT((form_hash_ == "b_only_condbranch"_h) ||
              (form_hash_ == "bc_only_condbranch"_h));
  if (ConditionPassed(instr->GetConditionBranch())) {
    WritePc(instr->GetImmPCOffsetTarget());
  }
}

BType Simulator::GetBTypeFromInstruction(const Instruction* instr) const {
  switch (instr->Mask(UnconditionalBranchToRegisterMask)) {
    case BLR:
    case BLRAA:
    case BLRAB:
    case BLRAAZ:
    case BLRABZ:
      return BranchAndLink;
    case BR:
    case BRAA:
    case BRAB:
    case BRAAZ:
    case BRABZ:
      if ((instr->GetRn() == 16) || (instr->GetRn() == 17) ||
          !PcIsInGuardedPage()) {
        return BranchFromUnguardedOrToIP;
      }
      return BranchFromGuardedNotToIP;
  }
  return DefaultBType;
}

void Simulator::VisitUnconditionalBranchToRegister(const Instruction* instr) {
  bool authenticate = false;
  bool link = false;
  bool ret = false;
  bool compare_gcs = false;
  uint64_t addr = ReadXRegister(instr->GetRn());
  uint64_t context = 0;

  switch (instr->Mask(UnconditionalBranchToRegisterMask)) {
    case BLR:
      link = true;
      VIXL_FALLTHROUGH();
    case BR:
      break;

    case BLRAAZ:
    case BLRABZ:
      link = true;
      VIXL_FALLTHROUGH();
    case BRAAZ:
    case BRABZ:
      authenticate = true;
      break;

    case BLRAA:
    case BLRAB:
      link = true;
      VIXL_FALLTHROUGH();
    case BRAA:
    case BRAB:
      authenticate = true;
      context = ReadXRegister(instr->GetRd());
      break;

    case RETAA:
    case RETAB:
      authenticate = true;
      addr = ReadXRegister(kLinkRegCode);
      context = ReadXRegister(31, Reg31IsStackPointer);
      VIXL_FALLTHROUGH();
    case RET:
      compare_gcs = true;
      ret = true;
      break;
    default:
      VIXL_UNREACHABLE();
  }

  if (authenticate) {
    PACKey key = (instr->ExtractBit(10) == 0) ? kPACKeyIA : kPACKeyIB;
    addr = AuthPAC(addr, context, key, kInstructionPointer);

    int error_lsb = GetTopPACBit(addr, kInstructionPointer) - 2;
    if (((addr >> error_lsb) & 0x3) != 0x0) {
      VIXL_ABORT_WITH_MSG("Failed to authenticate pointer.");
    }
  }

  if (compare_gcs) {
    uint64_t expected_lr = GCSPeek();
    char msg[128];
    if (expected_lr != 0) {
      if ((expected_lr & 0x3) != 0) {
        snprintf(msg,
                 sizeof(msg),
                 "GCS contains misaligned return address: 0x%016" PRIx64 "\n",
                 expected_lr);
        ReportGCSFailure(msg);
      } else if ((addr != 0) && (addr != expected_lr)) {
        snprintf(msg,
                 sizeof(msg),
                 "GCS mismatch: lr = 0x%016" PRIx64 ", gcs = 0x%016" PRIx64
                 "\n",
                 addr,
                 expected_lr);
        ReportGCSFailure(msg);
      }
      GCSPop();
    }
  }

  if (link) {
    WriteLr(instr->GetNextInstruction());
    GCSPush(reinterpret_cast<uint64_t>(instr->GetNextInstruction()));
  }

  if (!ret) {
    // Check for interceptions to the target address, if one is found, call it.
    MetaDataDepot::BranchInterceptionAbstract* interception =
        meta_data_.FindBranchInterception(addr);

    if (interception != nullptr) {
      // Instead of writing the address of the function to the PC, call the
      // function's interception directly. We change the address that will be
      // branched to so that afterwards we continue execution from
      // the address in the LR. Note: the interception may modify the LR so
      // store it before calling the interception.
      addr = ReadRegister<uint64_t>(kLinkRegCode);
      (*interception)(this);
    }
  }

  WriteNextBType(GetBTypeFromInstruction(instr));
  WritePc(Instruction::Cast(addr));
}


void Simulator::VisitTestBranch(const Instruction* instr) {
  unsigned bit_pos =
      (instr->GetImmTestBranchBit5() << 5) | instr->GetImmTestBranchBit40();
  bool bit_zero = ((ReadXRegister(instr->GetRt()) >> bit_pos) & 1) == 0;
  bool take_branch = false;
  switch (instr->Mask(TestBranchMask)) {
    case TBZ:
      take_branch = bit_zero;
      break;
    case TBNZ:
      take_branch = !bit_zero;
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
  if (take_branch) {
    WritePc(instr->GetImmPCOffsetTarget());
  }
}


void Simulator::VisitCompareBranch(const Instruction* instr) {
  unsigned rt = instr->GetRt();
  bool take_branch = false;
  switch (instr->Mask(CompareBranchMask)) {
    case CBZ_w:
      take_branch = (ReadWRegister(rt) == 0);
      break;
    case CBZ_x:
      take_branch = (ReadXRegister(rt) == 0);
      break;
    case CBNZ_w:
      take_branch = (ReadWRegister(rt) != 0);
      break;
    case CBNZ_x:
      take_branch = (ReadXRegister(rt) != 0);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
  if (take_branch) {
    WritePc(instr->GetImmPCOffsetTarget());
  }
}


void Simulator::AddSubHelper(const Instruction* instr, int64_t op2) {
  unsigned reg_size = instr->GetSixtyFourBits() ? kXRegSize : kWRegSize;
  bool set_flags = instr->GetFlagsUpdate();
  int64_t new_val = 0;
  Instr operation = instr->Mask(AddSubOpMask);

  switch (operation) {
    case ADD:
    case ADDS: {
      new_val = AddWithCarry(reg_size,
                             set_flags,
                             ReadRegister(reg_size,
                                          instr->GetRn(),
                                          instr->GetRnMode()),
                             op2);
      break;
    }
    case SUB:
    case SUBS: {
      new_val = AddWithCarry(reg_size,
                             set_flags,
                             ReadRegister(reg_size,
                                          instr->GetRn(),
                                          instr->GetRnMode()),
                             ~op2,
                             1);
      break;
    }
    default:
      VIXL_UNREACHABLE();
  }

  WriteRegister(reg_size,
                instr->GetRd(),
                new_val,
                LogRegWrites,
                instr->GetRdMode());
}


void Simulator::VisitAddSubShifted(const Instruction* instr) {
  // Add/sub/adds/subs don't allow ROR as a shift mode.
  VIXL_ASSERT(instr->GetShiftDP() != ROR);

  unsigned reg_size = instr->GetSixtyFourBits() ? kXRegSize : kWRegSize;
  int64_t op2 = ShiftOperand(reg_size,
                             ReadRegister(reg_size, instr->GetRm()),
                             static_cast<Shift>(instr->GetShiftDP()),
                             instr->GetImmDPShift());
  AddSubHelper(instr, op2);
}


void Simulator::VisitAddSubImmediate(const Instruction* instr) {
  int64_t op2 = instr->GetImmAddSub()
                << ((instr->GetImmAddSubShift() == 1) ? 12 : 0);
  AddSubHelper(instr, op2);
}


void Simulator::VisitAddSubExtended(const Instruction* instr) {
  unsigned reg_size = instr->GetSixtyFourBits() ? kXRegSize : kWRegSize;
  int64_t op2 = ExtendValue(reg_size,
                            ReadRegister(reg_size, instr->GetRm()),
                            static_cast<Extend>(instr->GetExtendMode()),
                            instr->GetImmExtendShift());
  AddSubHelper(instr, op2);
}


void Simulator::VisitAddSubWithCarry(const Instruction* instr) {
  unsigned reg_size = instr->GetSixtyFourBits() ? kXRegSize : kWRegSize;
  int64_t op2 = ReadRegister(reg_size, instr->GetRm());
  int64_t new_val;

  if ((instr->Mask(AddSubOpMask) == SUB) ||
      (instr->Mask(AddSubOpMask) == SUBS)) {
    op2 = ~op2;
  }

  new_val = AddWithCarry(reg_size,
                         instr->GetFlagsUpdate(),
                         ReadRegister(reg_size, instr->GetRn()),
                         op2,
                         ReadC());

  WriteRegister(reg_size, instr->GetRd(), new_val);
}


void Simulator::VisitRotateRightIntoFlags(const Instruction* instr) {
  switch (instr->Mask(RotateRightIntoFlagsMask)) {
    case RMIF: {
      uint64_t value = ReadRegister<uint64_t>(instr->GetRn());
      unsigned shift = instr->GetImmRMIFRotation();
      unsigned mask = instr->GetNzcv();
      uint64_t rotated = RotateRight(value, shift, kXRegSize);

      ReadNzcv().SetFlags((rotated & mask) | (ReadNzcv().GetFlags() & ~mask));
      LogSystemRegister(NZCV);
      break;
    }
  }
}


void Simulator::VisitEvaluateIntoFlags(const Instruction* instr) {
  uint32_t value = ReadRegister<uint32_t>(instr->GetRn());
  unsigned msb = (instr->Mask(EvaluateIntoFlagsMask) == SETF16) ? 15 : 7;

  unsigned sign_bit = (value >> msb) & 1;
  unsigned overflow_bit = (value >> (msb + 1)) & 1;
  ReadNzcv().SetN(sign_bit);
  ReadNzcv().SetZ((value << (31 - msb)) == 0);
  ReadNzcv().SetV(sign_bit ^ overflow_bit);
  LogSystemRegister(NZCV);
}


void Simulator::VisitLogicalShifted(const Instruction* instr) {
  unsigned reg_size = instr->GetSixtyFourBits() ? kXRegSize : kWRegSize;
  Shift shift_type = static_cast<Shift>(instr->GetShiftDP());
  unsigned shift_amount = instr->GetImmDPShift();
  int64_t op2 = ShiftOperand(reg_size,
                             ReadRegister(reg_size, instr->GetRm()),
                             shift_type,
                             shift_amount);
  if (instr->Mask(NOT) == NOT) {
    op2 = ~op2;
  }
  LogicalHelper(instr, op2);
}


void Simulator::VisitLogicalImmediate(const Instruction* instr) {
  if (instr->GetImmLogical() == 0) {
    VIXL_UNIMPLEMENTED();
  } else {
    LogicalHelper(instr, instr->GetImmLogical());
  }
}


void Simulator::LogicalHelper(const Instruction* instr, int64_t op2) {
  unsigned reg_size = instr->GetSixtyFourBits() ? kXRegSize : kWRegSize;
  int64_t op1 = ReadRegister(reg_size, instr->GetRn());
  int64_t result = 0;
  bool update_flags = false;

  // Switch on the logical operation, stripping out the NOT bit, as it has a
  // different meaning for logical immediate instructions.
  switch (instr->Mask(LogicalOpMask & ~NOT)) {
    case ANDS:
      update_flags = true;
      VIXL_FALLTHROUGH();
    case AND:
      result = op1 & op2;
      break;
    case ORR:
      result = op1 | op2;
      break;
    case EOR:
      result = op1 ^ op2;
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }

  if (update_flags) {
    ReadNzcv().SetN(CalcNFlag(result, reg_size));
    ReadNzcv().SetZ(CalcZFlag(result));
    ReadNzcv().SetC(0);
    ReadNzcv().SetV(0);
    LogSystemRegister(NZCV);
  }

  WriteRegister(reg_size,
                instr->GetRd(),
                result,
                LogRegWrites,
                instr->GetRdMode());
}


void Simulator::VisitConditionalCompareRegister(const Instruction* instr) {
  unsigned reg_size = instr->GetSixtyFourBits() ? kXRegSize : kWRegSize;
  ConditionalCompareHelper(instr, ReadRegister(reg_size, instr->GetRm()));
}


void Simulator::VisitConditionalCompareImmediate(const Instruction* instr) {
  ConditionalCompareHelper(instr, instr->GetImmCondCmp());
}


void Simulator::ConditionalCompareHelper(const Instruction* instr,
                                         int64_t op2) {
  unsigned reg_size = instr->GetSixtyFourBits() ? kXRegSize : kWRegSize;
  int64_t op1 = ReadRegister(reg_size, instr->GetRn());

  if (ConditionPassed(instr->GetCondition())) {
    // If the condition passes, set the status flags to the result of comparing
    // the operands.
    if (instr->Mask(ConditionalCompareMask) == CCMP) {
      AddWithCarry(reg_size, true, op1, ~op2, 1);
    } else {
      VIXL_ASSERT(instr->Mask(ConditionalCompareMask) == CCMN);
      AddWithCarry(reg_size, true, op1, op2, 0);
    }
  } else {
    // If the condition fails, set the status flags to the nzcv immediate.
    ReadNzcv().SetFlags(instr->GetNzcv());
    LogSystemRegister(NZCV);
  }
}


void Simulator::VisitLoadStoreUnsignedOffset(const Instruction* instr) {
  int offset = instr->GetImmLSUnsigned() << instr->GetSizeLS();
  LoadStoreHelper(instr, offset, Offset);
}


void Simulator::VisitLoadStoreUnscaledOffset(const Instruction* instr) {
  LoadStoreHelper(instr, instr->GetImmLS(), Offset);
}


void Simulator::VisitLoadStorePreIndex(const Instruction* instr) {
  LoadStoreHelper(instr, instr->GetImmLS(), PreIndex);
}


void Simulator::VisitLoadStorePostIndex(const Instruction* instr) {
  LoadStoreHelper(instr, instr->GetImmLS(), PostIndex);
}


template <typename T1, typename T2>
void Simulator::LoadAcquireRCpcUnscaledOffsetHelper(const Instruction* instr) {
  unsigned rt = instr->GetRt();
  unsigned rn = instr->GetRn();

  unsigned element_size = sizeof(T2);
  uint64_t address = ReadRegister<uint64_t>(rn, Reg31IsStackPointer);
  int offset = instr->GetImmLS();
  address += offset;

  // Verify that the address is available to the host.
  VIXL_ASSERT(address == static_cast<uintptr_t>(address));

  // Check the alignment of `address`.
  if (AlignDown(address, 16) != AlignDown(address + element_size - 1, 16)) {
    VIXL_ALIGNMENT_EXCEPTION();
  }

  VIXL_DEFINE_OR_RETURN(value, MemRead<T2>(address));

  WriteRegister<T1>(rt, static_cast<T1>(value));

  // Approximate load-acquire by issuing a full barrier after the load.
  VIXL_SYNC();

  LogRead(rt, GetPrintRegisterFormat(element_size), address);
}


template <typename T>
void Simulator::StoreReleaseUnscaledOffsetHelper(const Instruction* instr) {
  unsigned rt = instr->GetRt();
  unsigned rn = instr->GetRn();

  unsigned element_size = sizeof(T);
  uint64_t address = ReadRegister<uint64_t>(rn, Reg31IsStackPointer);
  int offset = instr->GetImmLS();
  address += offset;

  // Verify that the address is available to the host.
  VIXL_ASSERT(address == static_cast<uintptr_t>(address));

  // Check the alignment of `address`.
  if (AlignDown(address, 16) != AlignDown(address + element_size - 1, 16)) {
    VIXL_ALIGNMENT_EXCEPTION();
  }

  // Approximate store-release by issuing a full barrier after the load.
  VIXL_SYNC();

  if (!MemWrite<T>(address, ReadRegister<T>(rt))) return;

  LogWrite(rt, GetPrintRegisterFormat(element_size), address);
}


void Simulator::VisitLoadStoreRCpcUnscaledOffset(const Instruction* instr) {
  switch (instr->Mask(LoadStoreRCpcUnscaledOffsetMask)) {
    case LDAPURB:
      LoadAcquireRCpcUnscaledOffsetHelper<uint8_t, uint8_t>(instr);
      break;
    case LDAPURH:
      LoadAcquireRCpcUnscaledOffsetHelper<uint16_t, uint16_t>(instr);
      break;
    case LDAPUR_w:
      LoadAcquireRCpcUnscaledOffsetHelper<uint32_t, uint32_t>(instr);
      break;
    case LDAPUR_x:
      LoadAcquireRCpcUnscaledOffsetHelper<uint64_t, uint64_t>(instr);
      break;
    case LDAPURSB_w:
      LoadAcquireRCpcUnscaledOffsetHelper<int32_t, int8_t>(instr);
      break;
    case LDAPURSB_x:
      LoadAcquireRCpcUnscaledOffsetHelper<int64_t, int8_t>(instr);
      break;
    case LDAPURSH_w:
      LoadAcquireRCpcUnscaledOffsetHelper<int32_t, int16_t>(instr);
      break;
    case LDAPURSH_x:
      LoadAcquireRCpcUnscaledOffsetHelper<int64_t, int16_t>(instr);
      break;
    case LDAPURSW:
      LoadAcquireRCpcUnscaledOffsetHelper<int64_t, int32_t>(instr);
      break;
    case STLURB:
      StoreReleaseUnscaledOffsetHelper<uint8_t>(instr);
      break;
    case STLURH:
      StoreReleaseUnscaledOffsetHelper<uint16_t>(instr);
      break;
    case STLUR_w:
      StoreReleaseUnscaledOffsetHelper<uint32_t>(instr);
      break;
    case STLUR_x:
      StoreReleaseUnscaledOffsetHelper<uint64_t>(instr);
      break;
  }
}


void Simulator::VisitLoadStorePAC(const Instruction* instr) {
  unsigned dst = instr->GetRt();
  unsigned addr_reg = instr->GetRn();

  uint64_t address = ReadXRegister(addr_reg, Reg31IsStackPointer);

  PACKey key = (instr->ExtractBit(23) == 0) ? kPACKeyDA : kPACKeyDB;
  address = AuthPAC(address, 0, key, kDataPointer);

  int error_lsb = GetTopPACBit(address, kInstructionPointer) - 2;
  if (((address >> error_lsb) & 0x3) != 0x0) {
    VIXL_ABORT_WITH_MSG("Failed to authenticate pointer.");
  }


  if ((addr_reg == 31) && ((address % 16) != 0)) {
    // When the base register is SP the stack pointer is required to be
    // quadword aligned prior to the address calculation and write-backs.
    // Misalignment will cause a stack alignment fault.
    VIXL_ALIGNMENT_EXCEPTION();
  }

  int64_t offset = instr->GetImmLSPAC();
  address += offset;

  if (instr->Mask(LoadStorePACPreBit) == LoadStorePACPreBit) {
    // Pre-index mode.
    VIXL_ASSERT(offset != 0);
    WriteXRegister(addr_reg, address, LogRegWrites, Reg31IsStackPointer);
  }

  uintptr_t addr_ptr = static_cast<uintptr_t>(address);

  // Verify that the calculated address is available to the host.
  VIXL_ASSERT(address == addr_ptr);

  VIXL_DEFINE_OR_RETURN(value, MemRead<uint64_t>(addr_ptr));

  WriteXRegister(dst, value, NoRegLog);
  unsigned access_size = 1 << 3;
  LogRead(dst, GetPrintRegisterFormatForSize(access_size), addr_ptr);
}


void Simulator::VisitLoadStoreRegisterOffset(const Instruction* instr) {
  Extend ext = static_cast<Extend>(instr->GetExtendMode());
  VIXL_ASSERT((ext == UXTW) || (ext == UXTX) || (ext == SXTW) || (ext == SXTX));
  unsigned shift_amount = instr->GetImmShiftLS() * instr->GetSizeLS();

  int64_t offset =
      ExtendValue(kXRegSize, ReadXRegister(instr->GetRm()), ext, shift_amount);
  LoadStoreHelper(instr, offset, Offset);
}


void Simulator::LoadStoreHelper(const Instruction* instr,
                                int64_t offset,
                                AddrMode addrmode) {
  unsigned srcdst = instr->GetRt();
  uintptr_t address = AddressModeHelper(instr->GetRn(), offset, addrmode);

  bool rt_is_vreg = false;
  int extend_to_size = 0;
  LoadStoreOp op = static_cast<LoadStoreOp>(instr->Mask(LoadStoreMask));
  switch (op) {
    case LDRB_w: {
      VIXL_DEFINE_OR_RETURN(value, MemRead<uint8_t>(address));
      WriteWRegister(srcdst, value, NoRegLog);
      extend_to_size = kWRegSizeInBytes;
      break;
    }
    case LDRH_w: {
      VIXL_DEFINE_OR_RETURN(value, MemRead<uint16_t>(address));
      WriteWRegister(srcdst, value, NoRegLog);
      extend_to_size = kWRegSizeInBytes;
      break;
    }
    case LDR_w: {
      VIXL_DEFINE_OR_RETURN(value, MemRead<uint32_t>(address));
      WriteWRegister(srcdst, value, NoRegLog);
      extend_to_size = kWRegSizeInBytes;
      break;
    }
    case LDR_x: {
      VIXL_DEFINE_OR_RETURN(value, MemRead<uint64_t>(address));
      WriteXRegister(srcdst, value, NoRegLog);
      extend_to_size = kXRegSizeInBytes;
      break;
    }
    case LDRSB_w: {
      VIXL_DEFINE_OR_RETURN(value, MemRead<int8_t>(address));
      WriteWRegister(srcdst, value, NoRegLog);
      extend_to_size = kWRegSizeInBytes;
      break;
    }
    case LDRSH_w: {
      VIXL_DEFINE_OR_RETURN(value, MemRead<int16_t>(address));
      WriteWRegister(srcdst, value, NoRegLog);
      extend_to_size = kWRegSizeInBytes;
      break;
    }
    case LDRSB_x: {
      VIXL_DEFINE_OR_RETURN(value, MemRead<int8_t>(address));
      WriteXRegister(srcdst, value, NoRegLog);
      extend_to_size = kXRegSizeInBytes;
      break;
    }
    case LDRSH_x: {
      VIXL_DEFINE_OR_RETURN(value, MemRead<int16_t>(address));
      WriteXRegister(srcdst, value, NoRegLog);
      extend_to_size = kXRegSizeInBytes;
      break;
    }
    case LDRSW_x: {
      VIXL_DEFINE_OR_RETURN(value, MemRead<int32_t>(address));
      WriteXRegister(srcdst, value, NoRegLog);
      extend_to_size = kXRegSizeInBytes;
      break;
    }
    case LDR_b: {
      VIXL_DEFINE_OR_RETURN(value, MemRead<uint8_t>(address));
      WriteBRegister(srcdst, value, NoRegLog);
      rt_is_vreg = true;
      break;
    }
    case LDR_h: {
      VIXL_DEFINE_OR_RETURN(value, MemRead<uint16_t>(address));
      WriteHRegister(srcdst, value, NoRegLog);
      rt_is_vreg = true;
      break;
    }
    case LDR_s: {
      VIXL_DEFINE_OR_RETURN(value, MemRead<float>(address));
      WriteSRegister(srcdst, value, NoRegLog);
      rt_is_vreg = true;
      break;
    }
    case LDR_d: {
      VIXL_DEFINE_OR_RETURN(value, MemRead<double>(address));
      WriteDRegister(srcdst, value, NoRegLog);
      rt_is_vreg = true;
      break;
    }
    case LDR_q: {
      VIXL_DEFINE_OR_RETURN(value, MemRead<qreg_t>(address));
      WriteQRegister(srcdst, value, NoRegLog);
      rt_is_vreg = true;
      break;
    }

    case STRB_w:
      if (!MemWrite<uint8_t>(address, ReadWRegister(srcdst))) return;
      break;
    case STRH_w:
      if (!MemWrite<uint16_t>(address, ReadWRegister(srcdst))) return;
      break;
    case STR_w:
      if (!MemWrite<uint32_t>(address, ReadWRegister(srcdst))) return;
      break;
    case STR_x:
      if (!MemWrite<uint64_t>(address, ReadXRegister(srcdst))) return;
      break;
    case STR_b:
      if (!MemWrite<uint8_t>(address, ReadBRegister(srcdst))) return;
      rt_is_vreg = true;
      break;
    case STR_h:
      if (!MemWrite<uint16_t>(address, ReadHRegisterBits(srcdst))) return;
      rt_is_vreg = true;
      break;
    case STR_s:
      if (!MemWrite<float>(address, ReadSRegister(srcdst))) return;
      rt_is_vreg = true;
      break;
    case STR_d:
      if (!MemWrite<double>(address, ReadDRegister(srcdst))) return;
      rt_is_vreg = true;
      break;
    case STR_q:
      if (!MemWrite<qreg_t>(address, ReadQRegister(srcdst))) return;
      rt_is_vreg = true;
      break;

    // Ignore prfm hint instructions.
    case PRFM:
      break;

    default:
      VIXL_UNIMPLEMENTED();
  }

  // Print a detailed trace (including the memory address).
  bool extend = (extend_to_size != 0);
  unsigned access_size = 1 << instr->GetSizeLS();
  unsigned result_size = extend ? extend_to_size : access_size;
  PrintRegisterFormat print_format =
      rt_is_vreg ? GetPrintRegisterFormatForSizeTryFP(result_size)
                 : GetPrintRegisterFormatForSize(result_size);

  if (instr->IsLoad()) {
    if (rt_is_vreg) {
      LogVRead(srcdst, print_format, address);
    } else {
      LogExtendingRead(srcdst, print_format, access_size, address);
    }
  } else if (instr->IsStore()) {
    if (rt_is_vreg) {
      LogVWrite(srcdst, print_format, address);
    } else {
      LogWrite(srcdst, GetPrintRegisterFormatForSize(result_size), address);
    }
  } else {
    VIXL_ASSERT(op == PRFM);
  }

  local_monitor_.MaybeClear();
}


void Simulator::VisitLoadStorePairOffset(const Instruction* instr) {
  LoadStorePairHelper(instr, Offset);
}


void Simulator::VisitLoadStorePairPreIndex(const Instruction* instr) {
  LoadStorePairHelper(instr, PreIndex);
}


void Simulator::VisitLoadStorePairPostIndex(const Instruction* instr) {
  LoadStorePairHelper(instr, PostIndex);
}


void Simulator::VisitLoadStorePairNonTemporal(const Instruction* instr) {
  LoadStorePairHelper(instr, Offset);
}


void Simulator::LoadStorePairHelper(const Instruction* instr,
                                    AddrMode addrmode) {
  unsigned rt = instr->GetRt();
  unsigned rt2 = instr->GetRt2();
  int element_size = 1 << instr->GetSizeLSPair();
  int64_t offset = instr->GetImmLSPair() * element_size;
  uintptr_t address = AddressModeHelper(instr->GetRn(), offset, addrmode);
  uintptr_t address2 = address + element_size;

  LoadStorePairOp op =
      static_cast<LoadStorePairOp>(instr->Mask(LoadStorePairMask));

  // 'rt' and 'rt2' can only be aliased for stores.
  VIXL_ASSERT(((op & LoadStorePairLBit) == 0) || (rt != rt2));

  bool rt_is_vreg = false;
  bool sign_extend = false;
  switch (op) {
    // Use NoRegLog to suppress the register trace (LOG_REGS, LOG_FP_REGS). We
    // will print a more detailed log.
    case LDP_w: {
      VIXL_DEFINE_OR_RETURN(value, MemRead<uint32_t>(address));
      VIXL_DEFINE_OR_RETURN(value2, MemRead<uint32_t>(address2));
      WriteWRegister(rt, value, NoRegLog);
      WriteWRegister(rt2, value2, NoRegLog);
      break;
    }
    case LDP_s: {
      VIXL_DEFINE_OR_RETURN(value, MemRead<float>(address));
      VIXL_DEFINE_OR_RETURN(value2, MemRead<float>(address2));
      WriteSRegister(rt, value, NoRegLog);
      WriteSRegister(rt2, value2, NoRegLog);
      rt_is_vreg = true;
      break;
    }
    case LDP_x: {
      VIXL_DEFINE_OR_RETURN(value, MemRead<uint64_t>(address));
      VIXL_DEFINE_OR_RETURN(value2, MemRead<uint64_t>(address2));
      WriteXRegister(rt, value, NoRegLog);
      WriteXRegister(rt2, value2, NoRegLog);
      break;
    }
    case LDP_d: {
      VIXL_DEFINE_OR_RETURN(value, MemRead<double>(address));
      VIXL_DEFINE_OR_RETURN(value2, MemRead<double>(address2));
      WriteDRegister(rt, value, NoRegLog);
      WriteDRegister(rt2, value2, NoRegLog);
      rt_is_vreg = true;
      break;
    }
    case LDP_q: {
      VIXL_DEFINE_OR_RETURN(value, MemRead<qreg_t>(address));
      VIXL_DEFINE_OR_RETURN(value2, MemRead<qreg_t>(address2));
      WriteQRegister(rt, value, NoRegLog);
      WriteQRegister(rt2, value2, NoRegLog);
      rt_is_vreg = true;
      break;
    }
    case LDPSW_x: {
      VIXL_DEFINE_OR_RETURN(value, MemRead<int32_t>(address));
      VIXL_DEFINE_OR_RETURN(value2, MemRead<int32_t>(address2));
      WriteXRegister(rt, value, NoRegLog);
      WriteXRegister(rt2, value2, NoRegLog);
      sign_extend = true;
      break;
    }
    case STP_w: {
      if (!MemWrite<uint32_t>(address, ReadWRegister(rt))) return;
      if (!MemWrite<uint32_t>(address2, ReadWRegister(rt2))) return;
      break;
    }
    case STP_s: {
      if (!MemWrite<float>(address, ReadSRegister(rt))) return;
      if (!MemWrite<float>(address2, ReadSRegister(rt2))) return;
      rt_is_vreg = true;
      break;
    }
    case STP_x: {
      if (!MemWrite<uint64_t>(address, ReadXRegister(rt))) return;
      if (!MemWrite<uint64_t>(address2, ReadXRegister(rt2))) return;
      break;
    }
    case STP_d: {
      if (!MemWrite<double>(address, ReadDRegister(rt))) return;
      if (!MemWrite<double>(address2, ReadDRegister(rt2))) return;
      rt_is_vreg = true;
      break;
    }
    case STP_q: {
      if (!MemWrite<qreg_t>(address, ReadQRegister(rt))) return;
      if (!MemWrite<qreg_t>(address2, ReadQRegister(rt2))) return;
      rt_is_vreg = true;
      break;
    }
    default:
      VIXL_UNREACHABLE();
  }

  // Print a detailed trace (including the memory address).
  unsigned result_size = sign_extend ? kXRegSizeInBytes : element_size;
  PrintRegisterFormat print_format =
      rt_is_vreg ? GetPrintRegisterFormatForSizeTryFP(result_size)
                 : GetPrintRegisterFormatForSize(result_size);

  if (instr->IsLoad()) {
    if (rt_is_vreg) {
      LogVRead(rt, print_format, address);
      LogVRead(rt2, print_format, address2);
    } else if (sign_extend) {
      LogExtendingRead(rt, print_format, element_size, address);
      LogExtendingRead(rt2, print_format, element_size, address2);
    } else {
      LogRead(rt, print_format, address);
      LogRead(rt2, print_format, address2);
    }
  } else {
    if (rt_is_vreg) {
      LogVWrite(rt, print_format, address);
      LogVWrite(rt2, print_format, address2);
    } else {
      LogWrite(rt, print_format, address);
      LogWrite(rt2, print_format, address2);
    }
  }

  local_monitor_.MaybeClear();
}


template <typename T>
void Simulator::CompareAndSwapHelper(const Instruction* instr) {
  unsigned rs = instr->GetRs();
  unsigned rt = instr->GetRt();
  unsigned rn = instr->GetRn();

  unsigned element_size = sizeof(T);
  uint64_t address = ReadRegister<uint64_t>(rn, Reg31IsStackPointer);

  CheckIsValidUnalignedAtomicAccess(rn, address, element_size);

  bool is_acquire = instr->ExtractBit(22) == 1;
  bool is_release = instr->ExtractBit(15) == 1;

  T comparevalue = ReadRegister<T>(rs);
  T newvalue = ReadRegister<T>(rt);

  // The architecture permits that the data read clears any exclusive monitors
  // associated with that location, even if the compare subsequently fails.
  local_monitor_.Clear();

  VIXL_DEFINE_OR_RETURN(data, MemRead<T>(address));

  if (is_acquire) {
    // Approximate load-acquire by issuing a full barrier after the load.
    VIXL_SYNC();
  }

  if (data == comparevalue) {
    if (is_release) {
      // Approximate store-release by issuing a full barrier before the store.
      VIXL_SYNC();
    }
    if (!MemWrite<T>(address, newvalue)) return;
    LogWrite(rt, GetPrintRegisterFormatForSize(element_size), address);
  }
  WriteRegister<T>(rs, data, NoRegLog);
  LogRead(rs, GetPrintRegisterFormatForSize(element_size), address);
}


template <typename T>
void Simulator::CompareAndSwapPairHelper(const Instruction* instr) {
  VIXL_ASSERT((sizeof(T) == 4) || (sizeof(T) == 8));
  unsigned rs = instr->GetRs();
  unsigned rt = instr->GetRt();
  unsigned rn = instr->GetRn();

  VIXL_ASSERT((rs % 2 == 0) && (rt % 2 == 0));

  unsigned element_size = sizeof(T);
  uint64_t address = ReadRegister<uint64_t>(rn, Reg31IsStackPointer);

  CheckIsValidUnalignedAtomicAccess(rn, address, element_size * 2);

  uint64_t address2 = address + element_size;

  bool is_acquire = instr->ExtractBit(22) == 1;
  bool is_release = instr->ExtractBit(15) == 1;

  T comparevalue_high = ReadRegister<T>(rs + 1);
  T comparevalue_low = ReadRegister<T>(rs);
  T newvalue_high = ReadRegister<T>(rt + 1);
  T newvalue_low = ReadRegister<T>(rt);

  // The architecture permits that the data read clears any exclusive monitors
  // associated with that location, even if the compare subsequently fails.
  local_monitor_.Clear();

  VIXL_DEFINE_OR_RETURN(data_low, MemRead<T>(address));
  VIXL_DEFINE_OR_RETURN(data_high, MemRead<T>(address2));

  if (is_acquire) {
    // Approximate load-acquire by issuing a full barrier after the load.
    VIXL_SYNC();
  }

  bool same =
      (data_high == comparevalue_high) && (data_low == comparevalue_low);
  if (same) {
    if (is_release) {
      // Approximate store-release by issuing a full barrier before the store.
      VIXL_SYNC();
    }

    if (!MemWrite<T>(address, newvalue_low)) return;
    if (!MemWrite<T>(address2, newvalue_high)) return;
  }

  WriteRegister<T>(rs + 1, data_high, NoRegLog);
  WriteRegister<T>(rs, data_low, NoRegLog);

  PrintRegisterFormat format = GetPrintRegisterFormatForSize(element_size);
  LogRead(rs, format, address);
  LogRead(rs + 1, format, address2);

  if (same) {
    LogWrite(rt, format, address);
    LogWrite(rt + 1, format, address2);
  }
}

bool Simulator::CanReadMemory(uintptr_t address, size_t size) {
#ifndef _WIN32
  // To simulate fault-tolerant loads, we need to know what host addresses we
  // can access without generating a real fault. One way to do that is to
  // attempt to `write()` the memory to a placeholder pipe[1]. This is more
  // portable and less intrusive than using (global) signal handlers.
  //
  // [1]: https://stackoverflow.com/questions/7134590

  size_t written = 0;
  bool can_read = true;
  // `write` will normally return after one invocation, but it is allowed to
  // handle only part of the operation, so wrap it in a loop.
  while (can_read && (written < size)) {
    ssize_t result = write(placeholder_pipe_fd_[1],
                           reinterpret_cast<void*>(address + written),
                           size - written);
    if (result > 0) {
      written += result;
    } else {
      switch (result) {
        case -EPERM:
        case -EFAULT:
          // The address range is not accessible.
          // `write` is supposed to return -EFAULT in this case, but in practice
          // it seems to return -EPERM, so we accept that too.
          can_read = false;
          break;
        case -EINTR:
          // The call was interrupted by a signal. Just try again.
          break;
        default:
          // Any other error is fatal.
          VIXL_ABORT();
      }
    }
  }
  // Drain the read side of the pipe. If we don't do this, we'll leak memory as
  // the placeholder data is buffered. As before, we expect to drain the whole
  // write in one invocation, but cannot guarantee that, so we wrap it in a
  // loop. This function is primarily intended to implement SVE fault-tolerant
  // loads, so the maximum Z register size is a good default buffer size.
  char buffer[kZRegMaxSizeInBytes];
  while (written > 0) {
    ssize_t result = read(placeholder_pipe_fd_[0],
                          reinterpret_cast<void*>(buffer),
                          sizeof(buffer));
    // `read` blocks, and returns 0 only at EOF. We should not hit EOF until
    // we've read everything that was written, so treat 0 as an error.
    if (result > 0) {
      VIXL_ASSERT(static_cast<size_t>(result) <= written);
      written -= result;
    } else {
      // For -EINTR, just try again. We can't handle any other error.
      VIXL_CHECK(result == -EINTR);
    }
  }

  return can_read;
#else
  // To simulate fault-tolerant loads, we need to know what host addresses we
  // can access without generating a real fault
  // The pipe code above is almost but not fully compatible with Windows
  // Instead, use the platform specific API VirtualQuery()
  //
  // [2]: https://stackoverflow.com/a/18395247/9109981

  bool can_read = true;
  MEMORY_BASIC_INFORMATION pageInfo;

  size_t checked = 0;
  while (can_read && (checked < size)) {
    size_t result = VirtualQuery(reinterpret_cast<void*>(address + checked),
                                 &pageInfo,
                                 sizeof(pageInfo));

    if (result < 0) {
      can_read = false;
      break;
    }

    if (pageInfo.State != MEM_COMMIT) {
      can_read = false;
      break;
    }

    if (pageInfo.Protect == PAGE_NOACCESS || pageInfo.Protect == PAGE_EXECUTE) {
      can_read = false;
      break;
    }
    checked += pageInfo.RegionSize -
               ((address + checked) -
                reinterpret_cast<uintptr_t>(pageInfo.BaseAddress));
  }

  return can_read;
#endif
}

void Simulator::PrintExclusiveAccessWarning() {
  if (print_exclusive_access_warning_) {
    fprintf(stderr,
            "%sWARNING:%s VIXL simulator support for "
            "load-/store-/clear-exclusive "
            "instructions is limited. Refer to the README for details.%s\n",
            clr_warning,
            clr_warning_message,
            clr_normal);
    print_exclusive_access_warning_ = false;
  }
}

void Simulator::VisitLoadStoreExclusive(const Instruction* instr) {
  LoadStoreExclusive op =
      static_cast<LoadStoreExclusive>(instr->Mask(LoadStoreExclusiveMask));

  switch (op) {
    case CAS_w:
    case CASA_w:
    case CASL_w:
    case CASAL_w:
      CompareAndSwapHelper<uint32_t>(instr);
      break;
    case CAS_x:
    case CASA_x:
    case CASL_x:
    case CASAL_x:
      CompareAndSwapHelper<uint64_t>(instr);
      break;
    case CASB:
    case CASAB:
    case CASLB:
    case CASALB:
      CompareAndSwapHelper<uint8_t>(instr);
      break;
    case CASH:
    case CASAH:
    case CASLH:
    case CASALH:
      CompareAndSwapHelper<uint16_t>(instr);
      break;
    case CASP_w:
    case CASPA_w:
    case CASPL_w:
    case CASPAL_w:
      CompareAndSwapPairHelper<uint32_t>(instr);
      break;
    case CASP_x:
    case CASPA_x:
    case CASPL_x:
    case CASPAL_x:
      CompareAndSwapPairHelper<uint64_t>(instr);
      break;
    default:
      PrintExclusiveAccessWarning();

      unsigned rs = instr->GetRs();
      unsigned rt = instr->GetRt();
      unsigned rt2 = instr->GetRt2();
      unsigned rn = instr->GetRn();

      bool is_exclusive = !instr->GetLdStXNotExclusive();
      bool is_acquire_release =
          !is_exclusive || instr->GetLdStXAcquireRelease();
      bool is_load = instr->GetLdStXLoad();
      bool is_pair = instr->GetLdStXPair();

      unsigned element_size = 1 << instr->GetLdStXSizeLog2();
      unsigned access_size = is_pair ? element_size * 2 : element_size;
      uint64_t address = ReadRegister<uint64_t>(rn, Reg31IsStackPointer);

      CheckIsValidUnalignedAtomicAccess(rn, address, access_size);

      if (is_load) {
        if (is_exclusive) {
          local_monitor_.MarkExclusive(address, access_size);
        } else {
          // Any non-exclusive load can clear the local monitor as a side
          // effect. We don't need to do this, but it is useful to stress the
          // simulated code.
          local_monitor_.Clear();
        }

        // Use NoRegLog to suppress the register trace (LOG_REGS, LOG_FP_REGS).
        // We will print a more detailed log.
        unsigned reg_size = 0;
        switch (op) {
          case LDXRB_w:
          case LDAXRB_w:
          case LDARB_w:
          case LDLARB: {
            VIXL_DEFINE_OR_RETURN(value, MemRead<uint8_t>(address));
            WriteWRegister(rt, value, NoRegLog);
            reg_size = kWRegSizeInBytes;
            break;
          }
          case LDXRH_w:
          case LDAXRH_w:
          case LDARH_w:
          case LDLARH: {
            VIXL_DEFINE_OR_RETURN(value, MemRead<uint16_t>(address));
            WriteWRegister(rt, value, NoRegLog);
            reg_size = kWRegSizeInBytes;
            break;
          }
          case LDXR_w:
          case LDAXR_w:
          case LDAR_w:
          case LDLAR_w: {
            VIXL_DEFINE_OR_RETURN(value, MemRead<uint32_t>(address));
            WriteWRegister(rt, value, NoRegLog);
            reg_size = kWRegSizeInBytes;
            break;
          }
          case LDXR_x:
          case LDAXR_x:
          case LDAR_x:
          case LDLAR_x: {
            VIXL_DEFINE_OR_RETURN(value, MemRead<uint64_t>(address));
            WriteXRegister(rt, value, NoRegLog);
            reg_size = kXRegSizeInBytes;
            break;
          }
          case LDXP_w:
          case LDAXP_w: {
            VIXL_DEFINE_OR_RETURN(value, MemRead<uint32_t>(address));
            VIXL_DEFINE_OR_RETURN(value2,
                                  MemRead<uint32_t>(address + element_size));
            WriteWRegister(rt, value, NoRegLog);
            WriteWRegister(rt2, value2, NoRegLog);
            reg_size = kWRegSizeInBytes;
            break;
          }
          case LDXP_x:
          case LDAXP_x: {
            VIXL_DEFINE_OR_RETURN(value, MemRead<uint64_t>(address));
            VIXL_DEFINE_OR_RETURN(value2,
                                  MemRead<uint64_t>(address + element_size));
            WriteXRegister(rt, value, NoRegLog);
            WriteXRegister(rt2, value2, NoRegLog);
            reg_size = kXRegSizeInBytes;
            break;
          }
          default:
            VIXL_UNREACHABLE();
        }

        if (is_acquire_release) {
          // Approximate load-acquire by issuing a full barrier after the load.
          VIXL_SYNC();
        }

        PrintRegisterFormat format = GetPrintRegisterFormatForSize(reg_size);
        LogExtendingRead(rt, format, element_size, address);
        if (is_pair) {
          LogExtendingRead(rt2, format, element_size, address + element_size);
        }
      } else {
        if (is_acquire_release) {
          // Approximate store-release by issuing a full barrier before the
          // store.
          VIXL_SYNC();
        }

        bool do_store = true;
        if (is_exclusive) {
          do_store = local_monitor_.IsExclusive(address, access_size) &&
                     global_monitor_.IsExclusive(address, access_size);
          WriteWRegister(rs, do_store ? 0 : 1);

          //  - All exclusive stores explicitly clear the local monitor.
          local_monitor_.Clear();
        } else {
          //  - Any other store can clear the local monitor as a side effect.
          local_monitor_.MaybeClear();
        }

        if (do_store) {
          switch (op) {
            case STXRB_w:
            case STLXRB_w:
            case STLRB_w:
            case STLLRB:
              if (!MemWrite<uint8_t>(address, ReadWRegister(rt))) return;
              break;
            case STXRH_w:
            case STLXRH_w:
            case STLRH_w:
            case STLLRH:
              if (!MemWrite<uint16_t>(address, ReadWRegister(rt))) return;
              break;
            case STXR_w:
            case STLXR_w:
            case STLR_w:
            case STLLR_w:
              if (!MemWrite<uint32_t>(address, ReadWRegister(rt))) return;
              break;
            case STXR_x:
            case STLXR_x:
            case STLR_x:
            case STLLR_x:
              if (!MemWrite<uint64_t>(address, ReadXRegister(rt))) return;
              break;
            case STXP_w:
            case STLXP_w:
              if (!MemWrite<uint32_t>(address, ReadWRegister(rt))) return;
              if (!MemWrite<uint32_t>(address + element_size,
                                      ReadWRegister(rt2))) {
                return;
              }
              break;
            case STXP_x:
            case STLXP_x:
              if (!MemWrite<uint64_t>(address, ReadXRegister(rt))) return;
              if (!MemWrite<uint64_t>(address + element_size,
                                      ReadXRegister(rt2))) {
                return;
              }
              break;
            default:
              VIXL_UNREACHABLE();
          }

          PrintRegisterFormat format =
              GetPrintRegisterFormatForSize(element_size);
          LogWrite(rt, format, address);
          if (is_pair) {
            LogWrite(rt2, format, address + element_size);
          }
        }
      }
  }
}

template <typename T>
void Simulator::AtomicMemorySimpleHelper(const Instruction* instr) {
  unsigned rs = instr->GetRs();
  unsigned rt = instr->GetRt();
  unsigned rn = instr->GetRn();

  bool is_acquire = (instr->ExtractBit(23) == 1) && (rt != kZeroRegCode);
  bool is_release = instr->ExtractBit(22) == 1;

  unsigned element_size = sizeof(T);
  uint64_t address = ReadRegister<uint64_t>(rn, Reg31IsStackPointer);

  CheckIsValidUnalignedAtomicAccess(rn, address, element_size);

  T value = ReadRegister<T>(rs);

  VIXL_DEFINE_OR_RETURN(data, MemRead<T>(address));

  if (is_acquire) {
    // Approximate load-acquire by issuing a full barrier after the load.
    VIXL_SYNC();
  }

  T result = 0;
  switch (instr->Mask(AtomicMemorySimpleOpMask)) {
    case LDADDOp:
      result = data + value;
      break;
    case LDCLROp:
      VIXL_ASSERT(!std::numeric_limits<T>::is_signed);
      result = data & ~value;
      break;
    case LDEOROp:
      VIXL_ASSERT(!std::numeric_limits<T>::is_signed);
      result = data ^ value;
      break;
    case LDSETOp:
      VIXL_ASSERT(!std::numeric_limits<T>::is_signed);
      result = data | value;
      break;

    // Signed/Unsigned difference is done via the templated type T.
    case LDSMAXOp:
    case LDUMAXOp:
      result = (data > value) ? data : value;
      break;
    case LDSMINOp:
    case LDUMINOp:
      result = (data > value) ? value : data;
      break;
  }

  if (is_release) {
    // Approximate store-release by issuing a full barrier before the store.
    VIXL_SYNC();
  }

  WriteRegister<T>(rt, data, NoRegLog);

  unsigned register_size = element_size;
  if (element_size < kXRegSizeInBytes) {
    register_size = kWRegSizeInBytes;
  }
  PrintRegisterFormat format = GetPrintRegisterFormatForSize(register_size);
  LogExtendingRead(rt, format, element_size, address);

  if (!MemWrite<T>(address, result)) return;
  format = GetPrintRegisterFormatForSize(element_size);
  LogWrite(rs, format, address);
}

template <typename T>
void Simulator::AtomicMemorySwapHelper(const Instruction* instr) {
  unsigned rs = instr->GetRs();
  unsigned rt = instr->GetRt();
  unsigned rn = instr->GetRn();

  bool is_acquire = (instr->ExtractBit(23) == 1) && (rt != kZeroRegCode);
  bool is_release = instr->ExtractBit(22) == 1;

  unsigned element_size = sizeof(T);
  uint64_t address = ReadRegister<uint64_t>(rn, Reg31IsStackPointer);

  CheckIsValidUnalignedAtomicAccess(rn, address, element_size);

  VIXL_DEFINE_OR_RETURN(data, MemRead<T>(address));

  if (is_acquire) {
    // Approximate load-acquire by issuing a full barrier after the load.
    VIXL_SYNC();
  }

  if (is_release) {
    // Approximate store-release by issuing a full barrier before the store.
    VIXL_SYNC();
  }
  if (!MemWrite<T>(address, ReadRegister<T>(rs))) return;

  WriteRegister<T>(rt, data);

  PrintRegisterFormat format = GetPrintRegisterFormatForSize(element_size);
  LogRead(rt, format, address);
  LogWrite(rs, format, address);
}

template <typename T>
void Simulator::LoadAcquireRCpcHelper(const Instruction* instr) {
  unsigned rt = instr->GetRt();
  unsigned rn = instr->GetRn();

  unsigned element_size = sizeof(T);
  uint64_t address = ReadRegister<uint64_t>(rn, Reg31IsStackPointer);

  CheckIsValidUnalignedAtomicAccess(rn, address, element_size);

  VIXL_DEFINE_OR_RETURN(value, MemRead<T>(address));

  WriteRegister<T>(rt, value);

  // Approximate load-acquire by issuing a full barrier after the load.
  VIXL_SYNC();

  LogRead(rt, GetPrintRegisterFormatForSize(element_size), address);
}

#define ATOMIC_MEMORY_SIMPLE_UINT_LIST(V) \
  V(LDADD)                                \
  V(LDCLR)                                \
  V(LDEOR)                                \
  V(LDSET)                                \
  V(LDUMAX)                               \
  V(LDUMIN)

#define ATOMIC_MEMORY_SIMPLE_INT_LIST(V) \
  V(LDSMAX)                              \
  V(LDSMIN)

void Simulator::VisitAtomicMemory(const Instruction* instr) {
  switch (instr->Mask(AtomicMemoryMask)) {
// clang-format off
#define SIM_FUNC_B(A) \
    case A##B:        \
    case A##AB:       \
    case A##LB:       \
    case A##ALB:
#define SIM_FUNC_H(A) \
    case A##H:        \
    case A##AH:       \
    case A##LH:       \
    case A##ALH:
#define SIM_FUNC_w(A) \
    case A##_w:       \
    case A##A_w:      \
    case A##L_w:      \
    case A##AL_w:
#define SIM_FUNC_x(A) \
    case A##_x:       \
    case A##A_x:      \
    case A##L_x:      \
    case A##AL_x:

    ATOMIC_MEMORY_SIMPLE_UINT_LIST(SIM_FUNC_B)
      AtomicMemorySimpleHelper<uint8_t>(instr);
      break;
    ATOMIC_MEMORY_SIMPLE_INT_LIST(SIM_FUNC_B)
      AtomicMemorySimpleHelper<int8_t>(instr);
      break;
    ATOMIC_MEMORY_SIMPLE_UINT_LIST(SIM_FUNC_H)
      AtomicMemorySimpleHelper<uint16_t>(instr);
      break;
    ATOMIC_MEMORY_SIMPLE_INT_LIST(SIM_FUNC_H)
      AtomicMemorySimpleHelper<int16_t>(instr);
      break;
    ATOMIC_MEMORY_SIMPLE_UINT_LIST(SIM_FUNC_w)
      AtomicMemorySimpleHelper<uint32_t>(instr);
      break;
    ATOMIC_MEMORY_SIMPLE_INT_LIST(SIM_FUNC_w)
      AtomicMemorySimpleHelper<int32_t>(instr);
      break;
    ATOMIC_MEMORY_SIMPLE_UINT_LIST(SIM_FUNC_x)
      AtomicMemorySimpleHelper<uint64_t>(instr);
      break;
    ATOMIC_MEMORY_SIMPLE_INT_LIST(SIM_FUNC_x)
      AtomicMemorySimpleHelper<int64_t>(instr);
      break;
      // clang-format on

    case SWPB:
    case SWPAB:
    case SWPLB:
    case SWPALB:
      AtomicMemorySwapHelper<uint8_t>(instr);
      break;
    case SWPH:
    case SWPAH:
    case SWPLH:
    case SWPALH:
      AtomicMemorySwapHelper<uint16_t>(instr);
      break;
    case SWP_w:
    case SWPA_w:
    case SWPL_w:
    case SWPAL_w:
      AtomicMemorySwapHelper<uint32_t>(instr);
      break;
    case SWP_x:
    case SWPA_x:
    case SWPL_x:
    case SWPAL_x:
      AtomicMemorySwapHelper<uint64_t>(instr);
      break;
    case LDAPRB:
      LoadAcquireRCpcHelper<uint8_t>(instr);
      break;
    case LDAPRH:
      LoadAcquireRCpcHelper<uint16_t>(instr);
      break;
    case LDAPR_w:
      LoadAcquireRCpcHelper<uint32_t>(instr);
      break;
    case LDAPR_x:
      LoadAcquireRCpcHelper<uint64_t>(instr);
      break;
  }
}


void Simulator::VisitLoadLiteral(const Instruction* instr) {
  unsigned rt = instr->GetRt();
  uint64_t address = instr->GetLiteralAddress<uint64_t>();

  // Verify that the calculated address is available to the host.
  VIXL_ASSERT(address == static_cast<uintptr_t>(address));

  switch (instr->Mask(LoadLiteralMask)) {
    // Use NoRegLog to suppress the register trace (LOG_REGS, LOG_VREGS), then
    // print a more detailed log.
    case LDR_w_lit: {
      VIXL_DEFINE_OR_RETURN(value, MemRead<uint32_t>(address));
      WriteWRegister(rt, value, NoRegLog);
      LogRead(rt, kPrintWReg, address);
      break;
    }
    case LDR_x_lit: {
      VIXL_DEFINE_OR_RETURN(value, MemRead<uint64_t>(address));
      WriteXRegister(rt, value, NoRegLog);
      LogRead(rt, kPrintXReg, address);
      break;
    }
    case LDR_s_lit: {
      VIXL_DEFINE_OR_RETURN(value, MemRead<float>(address));
      WriteSRegister(rt, value, NoRegLog);
      LogVRead(rt, kPrintSRegFP, address);
      break;
    }
    case LDR_d_lit: {
      VIXL_DEFINE_OR_RETURN(value, MemRead<double>(address));
      WriteDRegister(rt, value, NoRegLog);
      LogVRead(rt, kPrintDRegFP, address);
      break;
    }
    case LDR_q_lit: {
      VIXL_DEFINE_OR_RETURN(value, MemRead<qreg_t>(address));
      WriteQRegister(rt, value, NoRegLog);
      LogVRead(rt, kPrintReg1Q, address);
      break;
    }
    case LDRSW_x_lit: {
      VIXL_DEFINE_OR_RETURN(value, MemRead<int32_t>(address));
      WriteXRegister(rt, value, NoRegLog);
      LogExtendingRead(rt, kPrintXReg, kWRegSizeInBytes, address);
      break;
    }

    // Ignore prfm hint instructions.
    case PRFM_lit:
      break;

    default:
      VIXL_UNREACHABLE();
  }

  local_monitor_.MaybeClear();
}


uintptr_t Simulator::AddressModeHelper(unsigned addr_reg,
                                       int64_t offset,
                                       AddrMode addrmode) {
  uint64_t address = ReadXRegister(addr_reg, Reg31IsStackPointer);

  if ((addr_reg == 31) && ((address % 16) != 0)) {
    // When the base register is SP the stack pointer is required to be
    // quadword aligned prior to the address calculation and write-backs.
    // Misalignment will cause a stack alignment fault.
    VIXL_ALIGNMENT_EXCEPTION();
  }

  if ((addrmode == PreIndex) || (addrmode == PostIndex)) {
    VIXL_ASSERT(offset != 0);
    // Only preindex should log the register update here. For Postindex, the
    // update will be printed automatically by LogWrittenRegisters _after_ the
    // memory access itself is logged.
    RegLogMode log_mode = (addrmode == PreIndex) ? LogRegWrites : NoRegLog;
    WriteXRegister(addr_reg, address + offset, log_mode, Reg31IsStackPointer);
  }

  if ((addrmode == Offset) || (addrmode == PreIndex)) {
    address += offset;
  }

  // Verify that the calculated address is available to the host.
  VIXL_ASSERT(address == static_cast<uintptr_t>(address));

  return static_cast<uintptr_t>(address);
}


void Simulator::VisitMoveWideImmediate(const Instruction* instr) {
  MoveWideImmediateOp mov_op =
      static_cast<MoveWideImmediateOp>(instr->Mask(MoveWideImmediateMask));
  int64_t new_xn_val = 0;

  bool is_64_bits = instr->GetSixtyFourBits() == 1;
  // Shift is limited for W operations.
  VIXL_ASSERT(is_64_bits || (instr->GetShiftMoveWide() < 2));

  // Get the shifted immediate.
  int64_t shift = instr->GetShiftMoveWide() * 16;
  int64_t shifted_imm16 = static_cast<int64_t>(instr->GetImmMoveWide())
                          << shift;

  // Compute the new value.
  switch (mov_op) {
    case MOVN_w:
    case MOVN_x: {
      new_xn_val = ~shifted_imm16;
      if (!is_64_bits) new_xn_val &= kWRegMask;
      break;
    }
    case MOVK_w:
    case MOVK_x: {
      unsigned reg_code = instr->GetRd();
      int64_t prev_xn_val =
          is_64_bits ? ReadXRegister(reg_code) : ReadWRegister(reg_code);
      new_xn_val = (prev_xn_val & ~(INT64_C(0xffff) << shift)) | shifted_imm16;
      break;
    }
    case MOVZ_w:
    case MOVZ_x: {
      new_xn_val = shifted_imm16;
      break;
    }
    default:
      VIXL_UNREACHABLE();
  }

  // Update the destination register.
  WriteXRegister(instr->GetRd(), new_xn_val);
}


void Simulator::VisitConditionalSelect(const Instruction* instr) {
  uint64_t new_val = ReadXRegister(instr->GetRn());

  if (ConditionFailed(static_cast<Condition>(instr->GetCondition()))) {
    new_val = ReadXRegister(instr->GetRm());
    switch (instr->Mask(ConditionalSelectMask)) {
      case CSEL_w:
      case CSEL_x:
        break;
      case CSINC_w:
      case CSINC_x:
        new_val++;
        break;
      case CSINV_w:
      case CSINV_x:
        new_val = ~new_val;
        break;
      case CSNEG_w:
      case CSNEG_x:
        new_val = UnsignedNegate(new_val);
        break;
      default:
        VIXL_UNIMPLEMENTED();
    }
  }
  unsigned reg_size = instr->GetSixtyFourBits() ? kXRegSize : kWRegSize;
  WriteRegister(reg_size, instr->GetRd(), new_val);
}


#define PAUTH_MODES_REGISTER_CONTEXT(V)   \
  V(i, a, kPACKeyIA, kInstructionPointer) \
  V(i, b, kPACKeyIB, kInstructionPointer) \
  V(d, a, kPACKeyDA, kDataPointer)        \
  V(d, b, kPACKeyDB, kDataPointer)

void Simulator::VisitDataProcessing1Source(const Instruction* instr) {
  unsigned dst = instr->GetRd();
  unsigned src = instr->GetRn();
  Reg31Mode r31_pac = Reg31IsStackPointer;

  switch (form_hash_) {
#define DEFINE_PAUTH_FUNCS(SUF0, SUF1, KEY, D)      \
  case "pac" #SUF0 "z" #SUF1 "_64z_dp_1src"_h:      \
    VIXL_ASSERT(src == kZeroRegCode);               \
    r31_pac = Reg31IsZeroRegister;                  \
    VIXL_FALLTHROUGH();                             \
  case "pac" #SUF0 #SUF1 "_64p_dp_1src"_h: {        \
    uint64_t mod = ReadXRegister(src, r31_pac);     \
    uint64_t ptr = ReadXRegister(dst);              \
    WriteXRegister(dst, AddPAC(ptr, mod, KEY, D));  \
    break;                                          \
  }                                                 \
  case "aut" #SUF0 "z" #SUF1 "_64z_dp_1src"_h:      \
    VIXL_ASSERT(src == kZeroRegCode);               \
    r31_pac = Reg31IsZeroRegister;                  \
    VIXL_FALLTHROUGH();                             \
  case "aut" #SUF0 #SUF1 "_64p_dp_1src"_h: {        \
    uint64_t mod = ReadXRegister(src, r31_pac);     \
    uint64_t ptr = ReadXRegister(dst);              \
    WriteXRegister(dst, AuthPAC(ptr, mod, KEY, D)); \
    break;                                          \
  }
    PAUTH_MODES_REGISTER_CONTEXT(DEFINE_PAUTH_FUNCS)
#undef DEFINE_PAUTH_FUNCS

    case "xpaci_64z_dp_1src"_h:
      WriteXRegister(dst, StripPAC(ReadXRegister(dst), kInstructionPointer));
      break;
    case "xpacd_64z_dp_1src"_h:
      WriteXRegister(dst, StripPAC(ReadXRegister(dst), kDataPointer));
      break;
    case "rbit_32_dp_1src"_h:
      WriteWRegister(dst, ReverseBits(ReadWRegister(src)));
      break;
    case "rbit_64_dp_1src"_h:
      WriteXRegister(dst, ReverseBits(ReadXRegister(src)));
      break;
    case "rev16_32_dp_1src"_h:
      WriteWRegister(dst, ReverseBytes(ReadWRegister(src), 1));
      break;
    case "rev16_64_dp_1src"_h:
      WriteXRegister(dst, ReverseBytes(ReadXRegister(src), 1));
      break;
    case "rev_32_dp_1src"_h:
      WriteWRegister(dst, ReverseBytes(ReadWRegister(src), 2));
      break;
    case "rev32_64_dp_1src"_h:
      WriteXRegister(dst, ReverseBytes(ReadXRegister(src), 2));
      break;
    case "rev_64_dp_1src"_h:
      WriteXRegister(dst, ReverseBytes(ReadXRegister(src), 3));
      break;
    case "clz_32_dp_1src"_h:
      WriteWRegister(dst, CountLeadingZeros(ReadWRegister(src)));
      break;
    case "clz_64_dp_1src"_h:
      WriteXRegister(dst, CountLeadingZeros(ReadXRegister(src)));
      break;
    case "cls_32_dp_1src"_h:
      WriteWRegister(dst, CountLeadingSignBits(ReadWRegister(src)));
      break;
    case "cls_64_dp_1src"_h:
      WriteXRegister(dst, CountLeadingSignBits(ReadXRegister(src)));
      break;
    case "abs_32_dp_1src"_h:
      WriteWRegister(dst, Abs(ReadWRegister(src)));
      break;
    case "abs_64_dp_1src"_h:
      WriteXRegister(dst, Abs(ReadXRegister(src)));
      break;
    case "cnt_32_dp_1src"_h:
      WriteWRegister(dst, CountSetBits(ReadWRegister(src)));
      break;
    case "cnt_64_dp_1src"_h:
      WriteXRegister(dst, CountSetBits(ReadXRegister(src)));
      break;
    case "ctz_32_dp_1src"_h:
      WriteWRegister(dst, CountTrailingZeros(ReadWRegister(src)));
      break;
    case "ctz_64_dp_1src"_h:
      WriteXRegister(dst, CountTrailingZeros(ReadXRegister(src)));
      break;
  }
}

uint32_t Simulator::Poly32Mod2(unsigned n, uint64_t data, uint32_t poly) {
  VIXL_ASSERT((n > 32) && (n <= 64));
  for (unsigned i = (n - 1); i >= 32; i--) {
    if (((data >> i) & 1) != 0) {
      uint64_t polysh32 = (uint64_t)poly << (i - 32);
      uint64_t mask = (UINT64_C(1) << i) - 1;
      data = ((data & mask) ^ polysh32);
    }
  }
  return data & 0xffffffff;
}


template <typename T>
uint32_t Simulator::Crc32Checksum(uint32_t acc, T val, uint32_t poly) {
  unsigned size = sizeof(val) * 8;  // Number of bits in type T.
  VIXL_ASSERT((size == 8) || (size == 16) || (size == 32));
  uint64_t tempacc = static_cast<uint64_t>(ReverseBits(acc)) << size;
  uint64_t tempval = static_cast<uint64_t>(ReverseBits(val)) << 32;
  return ReverseBits(Poly32Mod2(32 + size, tempacc ^ tempval, poly));
}


uint32_t Simulator::Crc32Checksum(uint32_t acc, uint64_t val, uint32_t poly) {
  // Poly32Mod2 cannot handle inputs with more than 32 bits, so compute
  // the CRC of each 32-bit word sequentially.
  acc = Crc32Checksum(acc, (uint32_t)(val & 0xffffffff), poly);
  return Crc32Checksum(acc, (uint32_t)(val >> 32), poly);
}


void Simulator::VisitDataProcessing2Source(const Instruction* instr) {
  Shift shift_op = NO_SHIFT;
  int64_t result = 0;
  unsigned reg_size = instr->GetSixtyFourBits() ? kXRegSize : kWRegSize;

  switch (instr->Mask(DataProcessing2SourceMask)) {
    case SDIV_w: {
      int32_t rn = ReadWRegister(instr->GetRn());
      int32_t rm = ReadWRegister(instr->GetRm());
      if ((rn == kWMinInt) && (rm == -1)) {
        result = kWMinInt;
      } else if (rm == 0) {
        // Division by zero can be trapped, but not on A-class processors.
        result = 0;
      } else {
        result = rn / rm;
      }
      break;
    }
    case SDIV_x: {
      int64_t rn = ReadXRegister(instr->GetRn());
      int64_t rm = ReadXRegister(instr->GetRm());
      if ((rn == kXMinInt) && (rm == -1)) {
        result = kXMinInt;
      } else if (rm == 0) {
        // Division by zero can be trapped, but not on A-class processors.
        result = 0;
      } else {
        result = rn / rm;
      }
      break;
    }
    case UDIV_w: {
      uint32_t rn = static_cast<uint32_t>(ReadWRegister(instr->GetRn()));
      uint32_t rm = static_cast<uint32_t>(ReadWRegister(instr->GetRm()));
      if (rm == 0) {
        // Division by zero can be trapped, but not on A-class processors.
        result = 0;
      } else {
        result = rn / rm;
      }
      break;
    }
    case UDIV_x: {
      uint64_t rn = static_cast<uint64_t>(ReadXRegister(instr->GetRn()));
      uint64_t rm = static_cast<uint64_t>(ReadXRegister(instr->GetRm()));
      if (rm == 0) {
        // Division by zero can be trapped, but not on A-class processors.
        result = 0;
      } else {
        result = rn / rm;
      }
      break;
    }
    case LSLV_w:
    case LSLV_x:
      shift_op = LSL;
      break;
    case LSRV_w:
    case LSRV_x:
      shift_op = LSR;
      break;
    case ASRV_w:
    case ASRV_x:
      shift_op = ASR;
      break;
    case RORV_w:
    case RORV_x:
      shift_op = ROR;
      break;
    case PACGA: {
      uint64_t dst = static_cast<uint64_t>(ReadXRegister(instr->GetRn()));
      uint64_t src = static_cast<uint64_t>(
          ReadXRegister(instr->GetRm(), Reg31IsStackPointer));
      uint64_t code = ComputePAC(dst, src, kPACKeyGA);
      result = code & 0xffffffff00000000;
      break;
    }
    case CRC32B: {
      uint32_t acc = ReadRegister<uint32_t>(instr->GetRn());
      uint8_t val = ReadRegister<uint8_t>(instr->GetRm());
      result = Crc32Checksum(acc, val, CRC32_POLY);
      break;
    }
    case CRC32H: {
      uint32_t acc = ReadRegister<uint32_t>(instr->GetRn());
      uint16_t val = ReadRegister<uint16_t>(instr->GetRm());
      result = Crc32Checksum(acc, val, CRC32_POLY);
      break;
    }
    case CRC32W: {
      uint32_t acc = ReadRegister<uint32_t>(instr->GetRn());
      uint32_t val = ReadRegister<uint32_t>(instr->GetRm());
      result = Crc32Checksum(acc, val, CRC32_POLY);
      break;
    }
    case CRC32X: {
      uint32_t acc = ReadRegister<uint32_t>(instr->GetRn());
      uint64_t val = ReadRegister<uint64_t>(instr->GetRm());
      result = Crc32Checksum(acc, val, CRC32_POLY);
      reg_size = kWRegSize;
      break;
    }
    case CRC32CB: {
      uint32_t acc = ReadRegister<uint32_t>(instr->GetRn());
      uint8_t val = ReadRegister<uint8_t>(instr->GetRm());
      result = Crc32Checksum(acc, val, CRC32C_POLY);
      break;
    }
    case CRC32CH: {
      uint32_t acc = ReadRegister<uint32_t>(instr->GetRn());
      uint16_t val = ReadRegister<uint16_t>(instr->GetRm());
      result = Crc32Checksum(acc, val, CRC32C_POLY);
      break;
    }
    case CRC32CW: {
      uint32_t acc = ReadRegister<uint32_t>(instr->GetRn());
      uint32_t val = ReadRegister<uint32_t>(instr->GetRm());
      result = Crc32Checksum(acc, val, CRC32C_POLY);
      break;
    }
    case CRC32CX: {
      uint32_t acc = ReadRegister<uint32_t>(instr->GetRn());
      uint64_t val = ReadRegister<uint64_t>(instr->GetRm());
      result = Crc32Checksum(acc, val, CRC32C_POLY);
      reg_size = kWRegSize;
      break;
    }
    default:
      VIXL_UNIMPLEMENTED();
  }

  if (shift_op != NO_SHIFT) {
    // Shift distance encoded in the least-significant five/six bits of the
    // register.
    int mask = (instr->GetSixtyFourBits() == 1) ? 0x3f : 0x1f;
    unsigned shift = ReadWRegister(instr->GetRm()) & mask;
    result = ShiftOperand(reg_size,
                          ReadRegister(reg_size, instr->GetRn()),
                          shift_op,
                          shift);
  }
  WriteRegister(reg_size, instr->GetRd(), result);
}

void Simulator::SimulateSignedMinMax(const Instruction* instr) {
  int32_t wn = ReadWRegister(instr->GetRn());
  int32_t wm = ReadWRegister(instr->GetRm());
  int64_t xn = ReadXRegister(instr->GetRn());
  int64_t xm = ReadXRegister(instr->GetRm());
  int32_t imm = instr->ExtractSignedBits(17, 10);
  int dst = instr->GetRd();

  switch (form_hash_) {
    case "smax_64_minmax_imm"_h:
    case "smin_64_minmax_imm"_h:
      xm = imm;
      break;
    case "smax_32_minmax_imm"_h:
    case "smin_32_minmax_imm"_h:
      wm = imm;
      break;
  }

  switch (form_hash_) {
    case "smax_32_minmax_imm"_h:
    case "smax_32_dp_2src"_h:
      WriteWRegister(dst, std::max(wn, wm));
      break;
    case "smax_64_minmax_imm"_h:
    case "smax_64_dp_2src"_h:
      WriteXRegister(dst, std::max(xn, xm));
      break;
    case "smin_32_minmax_imm"_h:
    case "smin_32_dp_2src"_h:
      WriteWRegister(dst, std::min(wn, wm));
      break;
    case "smin_64_minmax_imm"_h:
    case "smin_64_dp_2src"_h:
      WriteXRegister(dst, std::min(xn, xm));
      break;
  }
}

void Simulator::SimulateUnsignedMinMax(const Instruction* instr) {
  uint64_t xn = ReadXRegister(instr->GetRn());
  uint64_t xm = ReadXRegister(instr->GetRm());
  uint32_t imm = instr->ExtractBits(17, 10);
  int dst = instr->GetRd();

  switch (form_hash_) {
    case "umax_64u_minmax_imm"_h:
    case "umax_32u_minmax_imm"_h:
    case "umin_64u_minmax_imm"_h:
    case "umin_32u_minmax_imm"_h:
      xm = imm;
      break;
  }

  switch (form_hash_) {
    case "umax_32u_minmax_imm"_h:
    case "umax_32_dp_2src"_h:
      xn &= 0xffff'ffff;
      xm &= 0xffff'ffff;
      VIXL_FALLTHROUGH();
    case "umax_64u_minmax_imm"_h:
    case "umax_64_dp_2src"_h:
      WriteXRegister(dst, std::max(xn, xm));
      break;
    case "umin_32u_minmax_imm"_h:
    case "umin_32_dp_2src"_h:
      xn &= 0xffff'ffff;
      xm &= 0xffff'ffff;
      VIXL_FALLTHROUGH();
    case "umin_64u_minmax_imm"_h:
    case "umin_64_dp_2src"_h:
      WriteXRegister(dst, std::min(xn, xm));
      break;
  }
}

void Simulator::VisitDataProcessing3Source(const Instruction* instr) {
  unsigned reg_size = instr->GetSixtyFourBits() ? kXRegSize : kWRegSize;

  uint64_t result = 0;
  // Extract and sign- or zero-extend 32-bit arguments for widening operations.
  uint64_t rn_u32 = ReadRegister<uint32_t>(instr->GetRn());
  uint64_t rm_u32 = ReadRegister<uint32_t>(instr->GetRm());
  int64_t rn_s32 = ReadRegister<int32_t>(instr->GetRn());
  int64_t rm_s32 = ReadRegister<int32_t>(instr->GetRm());
  uint64_t rn_u64 = ReadXRegister(instr->GetRn());
  uint64_t rm_u64 = ReadXRegister(instr->GetRm());
  switch (instr->Mask(DataProcessing3SourceMask)) {
    case MADD_w:
    case MADD_x:
      result = ReadXRegister(instr->GetRa()) + (rn_u64 * rm_u64);
      break;
    case MSUB_w:
    case MSUB_x:
      result = ReadXRegister(instr->GetRa()) - (rn_u64 * rm_u64);
      break;
    case SMADDL_x:
      result = ReadXRegister(instr->GetRa()) +
               static_cast<uint64_t>(rn_s32 * rm_s32);
      break;
    case SMSUBL_x:
      result = ReadXRegister(instr->GetRa()) -
               static_cast<uint64_t>(rn_s32 * rm_s32);
      break;
    case UMADDL_x:
      result = ReadXRegister(instr->GetRa()) + (rn_u32 * rm_u32);
      break;
    case UMSUBL_x:
      result = ReadXRegister(instr->GetRa()) - (rn_u32 * rm_u32);
      break;
    case UMULH_x:
      result =
          internal::MultiplyHigh<64>(ReadRegister<uint64_t>(instr->GetRn()),
                                     ReadRegister<uint64_t>(instr->GetRm()));
      break;
    case SMULH_x:
      result = internal::MultiplyHigh<64>(ReadXRegister(instr->GetRn()),
                                          ReadXRegister(instr->GetRm()));
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
  WriteRegister(reg_size, instr->GetRd(), result);
}


void Simulator::VisitBitfield(const Instruction* instr) {
  unsigned reg_size = instr->GetSixtyFourBits() ? kXRegSize : kWRegSize;
  int64_t reg_mask = instr->GetSixtyFourBits() ? kXRegMask : kWRegMask;
  int R = instr->GetImmR();
  int S = instr->GetImmS();

  int diff = S - R;
  uint64_t mask;
  if (diff >= 0) {
    mask = ~UINT64_C(0) >> (64 - (diff + 1));
    mask = (static_cast<unsigned>(diff) < (reg_size - 1)) ? mask : reg_mask;
  } else {
    mask = ~UINT64_C(0) >> (64 - (S + 1));
    mask = RotateRight(mask, R, reg_size);
    diff += reg_size;
  }

  // inzero indicates if the extracted bitfield is inserted into the
  // destination register value or in zero.
  // If extend is true, extend the sign of the extracted bitfield.
  bool inzero = false;
  bool extend = false;
  switch (instr->Mask(BitfieldMask)) {
    case BFM_x:
    case BFM_w:
      break;
    case SBFM_x:
    case SBFM_w:
      inzero = true;
      extend = true;
      break;
    case UBFM_x:
    case UBFM_w:
      inzero = true;
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }

  uint64_t dst = inzero ? 0 : ReadRegister(reg_size, instr->GetRd());
  uint64_t src = ReadRegister(reg_size, instr->GetRn());
  // Rotate source bitfield into place.
  uint64_t result = RotateRight(src, R, reg_size);
  // Determine the sign extension.
  uint64_t topbits = (diff == 63) ? 0 : (~UINT64_C(0) << (diff + 1));
  uint64_t signbits = extend && ((src >> S) & 1) ? topbits : 0;

  // Merge sign extension, dest/zero and bitfield.
  result = signbits | (result & mask) | (dst & ~mask);

  WriteRegister(reg_size, instr->GetRd(), result);
}


void Simulator::VisitExtract(const Instruction* instr) {
  unsigned lsb = instr->GetImmS();
  unsigned reg_size = (instr->GetSixtyFourBits() == 1) ? kXRegSize : kWRegSize;
  uint64_t low_res =
      static_cast<uint64_t>(ReadRegister(reg_size, instr->GetRm())) >> lsb;
  uint64_t high_res = (lsb == 0)
                          ? 0
                          : ReadRegister<uint64_t>(reg_size, instr->GetRn())
                                << (reg_size - lsb);
  WriteRegister(reg_size, instr->GetRd(), low_res | high_res);
}


void Simulator::VisitFPImmediate(const Instruction* instr) {
  AssertSupportedFPCR();
  unsigned dest = instr->GetRd();
  switch (instr->Mask(FPImmediateMask)) {
    case FMOV_h_imm:
      WriteHRegister(dest, Float16ToRawbits(instr->GetImmFP16()));
      break;
    case FMOV_s_imm:
      WriteSRegister(dest, instr->GetImmFP32());
      break;
    case FMOV_d_imm:
      WriteDRegister(dest, instr->GetImmFP64());
      break;
    default:
      VIXL_UNREACHABLE();
  }
}


void Simulator::VisitFPIntegerConvert(const Instruction* instr) {
  AssertSupportedFPCR();

  unsigned dst = instr->GetRd();
  unsigned src = instr->GetRn();

  FPRounding round = ReadRMode();

  switch (instr->Mask(FPIntegerConvertMask)) {
    case FCVTAS_wh:
      WriteWRegister(dst, FPToInt32(ReadHRegister(src), FPTieAway));
      break;
    case FCVTAS_xh:
      WriteXRegister(dst, FPToInt64(ReadHRegister(src), FPTieAway));
      break;
    case FCVTAS_ws:
      WriteWRegister(dst, FPToInt32(ReadSRegister(src), FPTieAway));
      break;
    case FCVTAS_xs:
      WriteXRegister(dst, FPToInt64(ReadSRegister(src), FPTieAway));
      break;
    case FCVTAS_wd:
      WriteWRegister(dst, FPToInt32(ReadDRegister(src), FPTieAway));
      break;
    case FCVTAS_xd:
      WriteXRegister(dst, FPToInt64(ReadDRegister(src), FPTieAway));
      break;
    case FCVTAU_wh:
      WriteWRegister(dst, FPToUInt32(ReadHRegister(src), FPTieAway));
      break;
    case FCVTAU_xh:
      WriteXRegister(dst, FPToUInt64(ReadHRegister(src), FPTieAway));
      break;
    case FCVTAU_ws:
      WriteWRegister(dst, FPToUInt32(ReadSRegister(src), FPTieAway));
      break;
    case FCVTAU_xs:
      WriteXRegister(dst, FPToUInt64(ReadSRegister(src), FPTieAway));
      break;
    case FCVTAU_wd:
      WriteWRegister(dst, FPToUInt32(ReadDRegister(src), FPTieAway));
      break;
    case FCVTAU_xd:
      WriteXRegister(dst, FPToUInt64(ReadDRegister(src), FPTieAway));
      break;
    case FCVTMS_wh:
      WriteWRegister(dst, FPToInt32(ReadHRegister(src), FPNegativeInfinity));
      break;
    case FCVTMS_xh:
      WriteXRegister(dst, FPToInt64(ReadHRegister(src), FPNegativeInfinity));
      break;
    case FCVTMS_ws:
      WriteWRegister(dst, FPToInt32(ReadSRegister(src), FPNegativeInfinity));
      break;
    case FCVTMS_xs:
      WriteXRegister(dst, FPToInt64(ReadSRegister(src), FPNegativeInfinity));
      break;
    case FCVTMS_wd:
      WriteWRegister(dst, FPToInt32(ReadDRegister(src), FPNegativeInfinity));
      break;
    case FCVTMS_xd:
      WriteXRegister(dst, FPToInt64(ReadDRegister(src), FPNegativeInfinity));
      break;
    case FCVTMU_wh:
      WriteWRegister(dst, FPToUInt32(ReadHRegister(src), FPNegativeInfinity));
      break;
    case FCVTMU_xh:
      WriteXRegister(dst, FPToUInt64(ReadHRegister(src), FPNegativeInfinity));
      break;
    case FCVTMU_ws:
      WriteWRegister(dst, FPToUInt32(ReadSRegister(src), FPNegativeInfinity));
      break;
    case FCVTMU_xs:
      WriteXRegister(dst, FPToUInt64(ReadSRegister(src), FPNegativeInfinity));
      break;
    case FCVTMU_wd:
      WriteWRegister(dst, FPToUInt32(ReadDRegister(src), FPNegativeInfinity));
      break;
    case FCVTMU_xd:
      WriteXRegister(dst, FPToUInt64(ReadDRegister(src), FPNegativeInfinity));
      break;
    case FCVTPS_wh:
      WriteWRegister(dst, FPToInt32(ReadHRegister(src), FPPositiveInfinity));
      break;
    case FCVTPS_xh:
      WriteXRegister(dst, FPToInt64(ReadHRegister(src), FPPositiveInfinity));
      break;
    case FCVTPS_ws:
      WriteWRegister(dst, FPToInt32(ReadSRegister(src), FPPositiveInfinity));
      break;
    case FCVTPS_xs:
      WriteXRegister(dst, FPToInt64(ReadSRegister(src), FPPositiveInfinity));
      break;
    case FCVTPS_wd:
      WriteWRegister(dst, FPToInt32(ReadDRegister(src), FPPositiveInfinity));
      break;
    case FCVTPS_xd:
      WriteXRegister(dst, FPToInt64(ReadDRegister(src), FPPositiveInfinity));
      break;
    case FCVTPU_wh:
      WriteWRegister(dst, FPToUInt32(ReadHRegister(src), FPPositiveInfinity));
      break;
    case FCVTPU_xh:
      WriteXRegister(dst, FPToUInt64(ReadHRegister(src), FPPositiveInfinity));
      break;
    case FCVTPU_ws:
      WriteWRegister(dst, FPToUInt32(ReadSRegister(src), FPPositiveInfinity));
      break;
    case FCVTPU_xs:
      WriteXRegister(dst, FPToUInt64(ReadSRegister(src), FPPositiveInfinity));
      break;
    case FCVTPU_wd:
      WriteWRegister(dst, FPToUInt32(ReadDRegister(src), FPPositiveInfinity));
      break;
    case FCVTPU_xd:
      WriteXRegister(dst, FPToUInt64(ReadDRegister(src), FPPositiveInfinity));
      break;
    case FCVTNS_wh:
      WriteWRegister(dst, FPToInt32(ReadHRegister(src), FPTieEven));
      break;
    case FCVTNS_xh:
      WriteXRegister(dst, FPToInt64(ReadHRegister(src), FPTieEven));
      break;
    case FCVTNS_ws:
      WriteWRegister(dst, FPToInt32(ReadSRegister(src), FPTieEven));
      break;
    case FCVTNS_xs:
      WriteXRegister(dst, FPToInt64(ReadSRegister(src), FPTieEven));
      break;
    case FCVTNS_wd:
      WriteWRegister(dst, FPToInt32(ReadDRegister(src), FPTieEven));
      break;
    case FCVTNS_xd:
      WriteXRegister(dst, FPToInt64(ReadDRegister(src), FPTieEven));
      break;
    case FCVTNU_wh:
      WriteWRegister(dst, FPToUInt32(ReadHRegister(src), FPTieEven));
      break;
    case FCVTNU_xh:
      WriteXRegister(dst, FPToUInt64(ReadHRegister(src), FPTieEven));
      break;
    case FCVTNU_ws:
      WriteWRegister(dst, FPToUInt32(ReadSRegister(src), FPTieEven));
      break;
    case FCVTNU_xs:
      WriteXRegister(dst, FPToUInt64(ReadSRegister(src), FPTieEven));
      break;
    case FCVTNU_wd:
      WriteWRegister(dst, FPToUInt32(ReadDRegister(src), FPTieEven));
      break;
    case FCVTNU_xd:
      WriteXRegister(dst, FPToUInt64(ReadDRegister(src), FPTieEven));
      break;
    case FCVTZS_wh:
      WriteWRegister(dst, FPToInt32(ReadHRegister(src), FPZero));
      break;
    case FCVTZS_xh:
      WriteXRegister(dst, FPToInt64(ReadHRegister(src), FPZero));
      break;
    case FCVTZS_ws:
      WriteWRegister(dst, FPToInt32(ReadSRegister(src), FPZero));
      break;
    case FCVTZS_xs:
      WriteXRegister(dst, FPToInt64(ReadSRegister(src), FPZero));
      break;
    case FCVTZS_wd:
      WriteWRegister(dst, FPToInt32(ReadDRegister(src), FPZero));
      break;
    case FCVTZS_xd:
      WriteXRegister(dst, FPToInt64(ReadDRegister(src), FPZero));
      break;
    case FCVTZU_wh:
      WriteWRegister(dst, FPToUInt32(ReadHRegister(src), FPZero));
      break;
    case FCVTZU_xh:
      WriteXRegister(dst, FPToUInt64(ReadHRegister(src), FPZero));
      break;
    case FCVTZU_ws:
      WriteWRegister(dst, FPToUInt32(ReadSRegister(src), FPZero));
      break;
    case FCVTZU_xs:
      WriteXRegister(dst, FPToUInt64(ReadSRegister(src), FPZero));
      break;
    case FCVTZU_wd:
      WriteWRegister(dst, FPToUInt32(ReadDRegister(src), FPZero));
      break;
    case FCVTZU_xd:
      WriteXRegister(dst, FPToUInt64(ReadDRegister(src), FPZero));
      break;
    case FJCVTZS:
      WriteWRegister(dst, FPToFixedJS(ReadDRegister(src)));
      break;
    case FMOV_hw:
      WriteHRegister(dst, ReadWRegister(src) & kHRegMask);
      break;
    case FMOV_wh:
      WriteWRegister(dst, ReadHRegisterBits(src));
      break;
    case FMOV_xh:
      WriteXRegister(dst, ReadHRegisterBits(src));
      break;
    case FMOV_hx:
      WriteHRegister(dst, ReadXRegister(src) & kHRegMask);
      break;
    case FMOV_ws:
      WriteWRegister(dst, ReadSRegisterBits(src));
      break;
    case FMOV_xd:
      WriteXRegister(dst, ReadDRegisterBits(src));
      break;
    case FMOV_sw:
      WriteSRegisterBits(dst, ReadWRegister(src));
      break;
    case FMOV_dx:
      WriteDRegisterBits(dst, ReadXRegister(src));
      break;
    case FMOV_d1_x:
      // Zero bits beyond the MSB of a Q register.
      mov(kFormat16B, ReadVRegister(dst), ReadVRegister(dst));
      LogicVRegister(ReadVRegister(dst))
          .SetUint(kFormatD, 1, ReadXRegister(src));
      break;
    case FMOV_x_d1:
      WriteXRegister(dst, LogicVRegister(ReadVRegister(src)).Uint(kFormatD, 1));
      break;

    // A 32-bit input can be handled in the same way as a 64-bit input, since
    // the sign- or zero-extension will not affect the conversion.
    case SCVTF_dx:
      WriteDRegister(dst, FixedToDouble(ReadXRegister(src), 0, round));
      break;
    case SCVTF_dw:
      WriteDRegister(dst, FixedToDouble(ReadWRegister(src), 0, round));
      break;
    case UCVTF_dx:
      WriteDRegister(dst, UFixedToDouble(ReadXRegister(src), 0, round));
      break;
    case UCVTF_dw: {
      WriteDRegister(dst,
                     UFixedToDouble(ReadRegister<uint32_t>(src), 0, round));
      break;
    }
    case SCVTF_sx:
      WriteSRegister(dst, FixedToFloat(ReadXRegister(src), 0, round));
      break;
    case SCVTF_sw:
      WriteSRegister(dst, FixedToFloat(ReadWRegister(src), 0, round));
      break;
    case UCVTF_sx:
      WriteSRegister(dst, UFixedToFloat(ReadXRegister(src), 0, round));
      break;
    case UCVTF_sw: {
      WriteSRegister(dst, UFixedToFloat(ReadRegister<uint32_t>(src), 0, round));
      break;
    }
    case SCVTF_hx:
      WriteHRegister(dst, FixedToFloat16(ReadXRegister(src), 0, round));
      break;
    case SCVTF_hw:
      WriteHRegister(dst, FixedToFloat16(ReadWRegister(src), 0, round));
      break;
    case UCVTF_hx:
      WriteHRegister(dst, UFixedToFloat16(ReadXRegister(src), 0, round));
      break;
    case UCVTF_hw: {
      WriteHRegister(dst,
                     UFixedToFloat16(ReadRegister<uint32_t>(src), 0, round));
      break;
    }

    default:
      VIXL_UNREACHABLE();
  }
}


void Simulator::VisitFPFixedPointConvert(const Instruction* instr) {
  AssertSupportedFPCR();

  unsigned dst = instr->GetRd();
  unsigned src = instr->GetRn();
  int fbits = 64 - instr->GetFPScale();

  FPRounding round = ReadRMode();

  switch (instr->Mask(FPFixedPointConvertMask)) {
    // A 32-bit input can be handled in the same way as a 64-bit input, since
    // the sign- or zero-extension will not affect the conversion.
    case SCVTF_dx_fixed:
      WriteDRegister(dst, FixedToDouble(ReadXRegister(src), fbits, round));
      break;
    case SCVTF_dw_fixed:
      WriteDRegister(dst, FixedToDouble(ReadWRegister(src), fbits, round));
      break;
    case UCVTF_dx_fixed:
      WriteDRegister(dst, UFixedToDouble(ReadXRegister(src), fbits, round));
      break;
    case UCVTF_dw_fixed: {
      WriteDRegister(dst,
                     UFixedToDouble(ReadRegister<uint32_t>(src), fbits, round));
      break;
    }
    case SCVTF_sx_fixed:
      WriteSRegister(dst, FixedToFloat(ReadXRegister(src), fbits, round));
      break;
    case SCVTF_sw_fixed:
      WriteSRegister(dst, FixedToFloat(ReadWRegister(src), fbits, round));
      break;
    case UCVTF_sx_fixed:
      WriteSRegister(dst, UFixedToFloat(ReadXRegister(src), fbits, round));
      break;
    case UCVTF_sw_fixed: {
      WriteSRegister(dst,
                     UFixedToFloat(ReadRegister<uint32_t>(src), fbits, round));
      break;
    }
    case SCVTF_hx_fixed:
      WriteHRegister(dst, FixedToFloat16(ReadXRegister(src), fbits, round));
      break;
    case SCVTF_hw_fixed:
      WriteHRegister(dst, FixedToFloat16(ReadWRegister(src), fbits, round));
      break;
    case UCVTF_hx_fixed:
      WriteHRegister(dst, UFixedToFloat16(ReadXRegister(src), fbits, round));
      break;
    case UCVTF_hw_fixed: {
      WriteHRegister(dst,
                     UFixedToFloat16(ReadRegister<uint32_t>(src),
                                     fbits,
                                     round));
      break;
    }
    case FCVTZS_xd_fixed:
      WriteXRegister(dst,
                     FPToInt64(ReadDRegister(src) * std::pow(2.0, fbits),
                               FPZero));
      break;
    case FCVTZS_wd_fixed:
      WriteWRegister(dst,
                     FPToInt32(ReadDRegister(src) * std::pow(2.0, fbits),
                               FPZero));
      break;
    case FCVTZU_xd_fixed:
      WriteXRegister(dst,
                     FPToUInt64(ReadDRegister(src) * std::pow(2.0, fbits),
                                FPZero));
      break;
    case FCVTZU_wd_fixed:
      WriteWRegister(dst,
                     FPToUInt32(ReadDRegister(src) * std::pow(2.0, fbits),
                                FPZero));
      break;
    case FCVTZS_xs_fixed:
      WriteXRegister(dst,
                     FPToInt64(ReadSRegister(src) * std::pow(2.0f, fbits),
                               FPZero));
      break;
    case FCVTZS_ws_fixed:
      WriteWRegister(dst,
                     FPToInt32(ReadSRegister(src) * std::pow(2.0f, fbits),
                               FPZero));
      break;
    case FCVTZU_xs_fixed:
      WriteXRegister(dst,
                     FPToUInt64(ReadSRegister(src) * std::pow(2.0f, fbits),
                                FPZero));
      break;
    case FCVTZU_ws_fixed:
      WriteWRegister(dst,
                     FPToUInt32(ReadSRegister(src) * std::pow(2.0f, fbits),
                                FPZero));
      break;
    case FCVTZS_xh_fixed: {
      double output =
          static_cast<double>(ReadHRegister(src)) * std::pow(2.0, fbits);
      WriteXRegister(dst, FPToInt64(output, FPZero));
      break;
    }
    case FCVTZS_wh_fixed: {
      double output =
          static_cast<double>(ReadHRegister(src)) * std::pow(2.0, fbits);
      WriteWRegister(dst, FPToInt32(output, FPZero));
      break;
    }
    case FCVTZU_xh_fixed: {
      double output =
          static_cast<double>(ReadHRegister(src)) * std::pow(2.0, fbits);
      WriteXRegister(dst, FPToUInt64(output, FPZero));
      break;
    }
    case FCVTZU_wh_fixed: {
      double output =
          static_cast<double>(ReadHRegister(src)) * std::pow(2.0, fbits);
      WriteWRegister(dst, FPToUInt32(output, FPZero));
      break;
    }
    default:
      VIXL_UNREACHABLE();
  }
}


void Simulator::VisitFPCompare(const Instruction* instr) {
  AssertSupportedFPCR();

  FPTrapFlags trap = DisableTrap;
  switch (instr->Mask(FPCompareMask)) {
    case FCMPE_h:
      trap = EnableTrap;
      VIXL_FALLTHROUGH();
    case FCMP_h:
      FPCompare(ReadHRegister(instr->GetRn()),
                ReadHRegister(instr->GetRm()),
                trap);
      break;
    case FCMPE_s:
      trap = EnableTrap;
      VIXL_FALLTHROUGH();
    case FCMP_s:
      FPCompare(ReadSRegister(instr->GetRn()),
                ReadSRegister(instr->GetRm()),
                trap);
      break;
    case FCMPE_d:
      trap = EnableTrap;
      VIXL_FALLTHROUGH();
    case FCMP_d:
      FPCompare(ReadDRegister(instr->GetRn()),
                ReadDRegister(instr->GetRm()),
                trap);
      break;
    case FCMPE_h_zero:
      trap = EnableTrap;
      VIXL_FALLTHROUGH();
    case FCMP_h_zero:
      FPCompare(ReadHRegister(instr->GetRn()), SimFloat16(0.0), trap);
      break;
    case FCMPE_s_zero:
      trap = EnableTrap;
      VIXL_FALLTHROUGH();
    case FCMP_s_zero:
      FPCompare(ReadSRegister(instr->GetRn()), 0.0f, trap);
      break;
    case FCMPE_d_zero:
      trap = EnableTrap;
      VIXL_FALLTHROUGH();
    case FCMP_d_zero:
      FPCompare(ReadDRegister(instr->GetRn()), 0.0, trap);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
}


void Simulator::VisitFPConditionalCompare(const Instruction* instr) {
  AssertSupportedFPCR();

  FPTrapFlags trap = DisableTrap;
  switch (instr->Mask(FPConditionalCompareMask)) {
    case FCCMPE_h:
      trap = EnableTrap;
      VIXL_FALLTHROUGH();
    case FCCMP_h:
      if (ConditionPassed(instr->GetCondition())) {
        FPCompare(ReadHRegister(instr->GetRn()),
                  ReadHRegister(instr->GetRm()),
                  trap);
      } else {
        ReadNzcv().SetFlags(instr->GetNzcv());
        LogSystemRegister(NZCV);
      }
      break;
    case FCCMPE_s:
      trap = EnableTrap;
      VIXL_FALLTHROUGH();
    case FCCMP_s:
      if (ConditionPassed(instr->GetCondition())) {
        FPCompare(ReadSRegister(instr->GetRn()),
                  ReadSRegister(instr->GetRm()),
                  trap);
      } else {
        ReadNzcv().SetFlags(instr->GetNzcv());
        LogSystemRegister(NZCV);
      }
      break;
    case FCCMPE_d:
      trap = EnableTrap;
      VIXL_FALLTHROUGH();
    case FCCMP_d:
      if (ConditionPassed(instr->GetCondition())) {
        FPCompare(ReadDRegister(instr->GetRn()),
                  ReadDRegister(instr->GetRm()),
                  trap);
      } else {
        ReadNzcv().SetFlags(instr->GetNzcv());
        LogSystemRegister(NZCV);
      }
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
}


void Simulator::VisitFPConditionalSelect(const Instruction* instr) {
  AssertSupportedFPCR();

  Instr selected;
  if (ConditionPassed(instr->GetCondition())) {
    selected = instr->GetRn();
  } else {
    selected = instr->GetRm();
  }

  switch (instr->Mask(FPConditionalSelectMask)) {
    case FCSEL_h:
      WriteHRegister(instr->GetRd(), ReadHRegister(selected));
      break;
    case FCSEL_s:
      WriteSRegister(instr->GetRd(), ReadSRegister(selected));
      break;
    case FCSEL_d:
      WriteDRegister(instr->GetRd(), ReadDRegister(selected));
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
}

void Simulator::SimulateFPRoundIntToSize(const Instruction* instr) {
  AssertSupportedFPCR();

  struct FPRoundInfo {
    VectorFormat vform;
    bool use_fpcr;
    FrintMode frint_mode;
  };

  std::unordered_map<uint32_t, FPRoundInfo> modes = {
      {"frint32x_d_floatdp1"_h, {kFormatD, true, kFrintToInt32}},
      {"frint32x_s_floatdp1"_h, {kFormatS, true, kFrintToInt32}},
      {"frint64x_d_floatdp1"_h, {kFormatD, true, kFrintToInt64}},
      {"frint64x_s_floatdp1"_h, {kFormatS, true, kFrintToInt64}},
      {"frint32z_d_floatdp1"_h, {kFormatD, false, kFrintToInt32}},
      {"frint32z_s_floatdp1"_h, {kFormatS, false, kFrintToInt32}},
      {"frint64z_d_floatdp1"_h, {kFormatD, false, kFrintToInt64}},
      {"frint64z_s_floatdp1"_h, {kFormatS, false, kFrintToInt64}},
  };
  VIXL_ASSERT(modes.count(form_hash_) == 1);

  auto [vform, use_fpcr, frint_mode] = modes[form_hash_];
  FPRounding rounding_mode =
      use_fpcr ? static_cast<FPRounding>(ReadFpcr().GetRMode()) : FPZero;
  bool inexact_exception = true;

  unsigned fd = instr->GetRd();
  SimVRegister& rd = ReadVRegister(fd);
  SimVRegister& rn = ReadVRegister(instr->GetRn());

  frint(vform, rd, rn, rounding_mode, inexact_exception, frint_mode);
  // Explicitly log the register update whilst we have type information.
  LogVRegister(fd, GetPrintRegisterFormatFP(vform));
}

void Simulator::SimulateFPRoundInt(const Instruction* instr) {
  AssertSupportedFPCR();

  struct FPRoundInfo {
    VectorFormat vform;
    bool use_fpcr;
    FPRounding rounding_mode;
    bool inexact_exception;
  };

  std::unordered_map<uint32_t, FPRoundInfo> modes =
      {{"frinta_d_floatdp1"_h, {kFormatD, false, FPTieAway, false}},
       {"frinta_h_floatdp1"_h, {kFormatH, false, FPTieAway, false}},
       {"frinta_s_floatdp1"_h, {kFormatS, false, FPTieAway, false}},
       {"frinti_d_floatdp1"_h, {kFormatD, true, FPZero, false}},
       {"frinti_h_floatdp1"_h, {kFormatH, true, FPZero, false}},
       {"frinti_s_floatdp1"_h, {kFormatS, true, FPZero, false}},
       {"frintm_d_floatdp1"_h, {kFormatD, false, FPNegativeInfinity, false}},
       {"frintm_h_floatdp1"_h, {kFormatH, false, FPNegativeInfinity, false}},
       {"frintm_s_floatdp1"_h, {kFormatS, false, FPNegativeInfinity, false}},
       {"frintn_d_floatdp1"_h, {kFormatD, false, FPTieEven, false}},
       {"frintn_h_floatdp1"_h, {kFormatH, false, FPTieEven, false}},
       {"frintn_s_floatdp1"_h, {kFormatS, false, FPTieEven, false}},
       {"frintp_d_floatdp1"_h, {kFormatD, false, FPPositiveInfinity, false}},
       {"frintp_h_floatdp1"_h, {kFormatH, false, FPPositiveInfinity, false}},
       {"frintp_s_floatdp1"_h, {kFormatS, false, FPPositiveInfinity, false}},
       {"frintx_d_floatdp1"_h, {kFormatD, true, FPZero, true}},
       {"frintx_h_floatdp1"_h, {kFormatH, true, FPZero, true}},
       {"frintx_s_floatdp1"_h, {kFormatS, true, FPZero, true}},
       {"frintz_d_floatdp1"_h, {kFormatD, false, FPZero, false}},
       {"frintz_h_floatdp1"_h, {kFormatH, false, FPZero, false}},
       {"frintz_s_floatdp1"_h, {kFormatS, false, FPZero, false}}};
  VIXL_ASSERT(modes.count(form_hash_) == 1);

  auto [vform, use_fpcr, rounding_mode, inexact_exception] = modes[form_hash_];

  rounding_mode =
      use_fpcr ? static_cast<FPRounding>(ReadFpcr().GetRMode()) : rounding_mode;

  unsigned fd = instr->GetRd();
  SimVRegister& rd = ReadVRegister(fd);
  SimVRegister& rn = ReadVRegister(instr->GetRn());

  frint(vform, rd, rn, rounding_mode, inexact_exception);
  // Explicitly log the register update whilst we have type information.
  LogVRegister(fd, GetPrintRegisterFormatFP(vform));
}

void Simulator::SimulateFPConvert(const Instruction* instr) {
  AssertSupportedFPCR();
  unsigned fd = instr->GetRd();
  unsigned fn = instr->GetRn();
  UseDefaultNaN nan = ReadDN();

  Float16 hn = ReadHRegister(fn);
  float sn = ReadSRegister(fn);
  double dn = ReadDRegister(fn);

  switch (form_hash_) {
    case "fmov_h_floatdp1"_h:
      WriteHRegister(fd, hn);
      break;
    case "fmov_s_floatdp1"_h:
      WriteSRegister(fd, sn);
      break;
    case "fmov_d_floatdp1"_h:
      WriteDRegister(fd, dn);
      break;
    case "fcvt_ds_floatdp1"_h:
      WriteDRegister(fd, FPToDouble(sn, nan));
      break;
    case "fcvt_sd_floatdp1"_h:
      WriteSRegister(fd, FPToFloat(dn, FPTieEven, nan));
      break;
    case "fcvt_hs_floatdp1"_h:
      WriteHRegister(fd, Float16ToRawbits(FPToFloat16(sn, FPTieEven, nan)));
      break;
    case "fcvt_sh_floatdp1"_h:
      WriteSRegister(fd, FPToFloat(hn, nan));
      break;
    case "fcvt_dh_floatdp1"_h:
      WriteDRegister(fd, FPToDouble(hn, nan));
      break;
    case "fcvt_hd_floatdp1"_h:
      WriteHRegister(fd, Float16ToRawbits(FPToFloat16(dn, FPTieEven, nan)));
      break;
    case "bfcvt_bs_floatdp1"_h:
      WriteHRegister(fd, BFloat16ToRawbits(FPToBFloat16(sn, FPTieEven, nan)));
      break;
  }
}

void Simulator::VisitFPDataProcessing1Source(const Instruction* instr) {
  AssertSupportedFPCR();

  VectorFormat vform = kFormatD;
  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());

  switch (form_hash_) {
    case "fabs_d_floatdp1"_h:
      fabs_(vform = kFormatD, rd, rn);
      break;
    case "fabs_h_floatdp1"_h:
      fabs_(vform = kFormatH, rd, rn);
      break;
    case "fabs_s_floatdp1"_h:
      fabs_(vform = kFormatS, rd, rn);
      break;
    case "fneg_d_floatdp1"_h:
      fneg(vform = kFormatD, rd, rn);
      break;
    case "fneg_h_floatdp1"_h:
      fneg(vform = kFormatH, rd, rn);
      break;
    case "fneg_s_floatdp1"_h:
      fneg(vform = kFormatS, rd, rn);
      break;
    case "fsqrt_d_floatdp1"_h:
      fsqrt(vform = kFormatD, rd, rn);
      break;
    case "fsqrt_h_floatdp1"_h:
      fsqrt(vform = kFormatH, rd, rn);
      break;
    case "fsqrt_s_floatdp1"_h:
      fsqrt(vform = kFormatS, rd, rn);
      break;
  }
  // Explicitly log the register update whilst we have type information.
  LogVRegister(instr->GetRd(), GetPrintRegisterFormatFP(vform));
}


void Simulator::VisitFPDataProcessing2Source(const Instruction* instr) {
  AssertSupportedFPCR();

  VectorFormat vform;
  switch (instr->Mask(FPTypeMask)) {
    default:
      VIXL_UNREACHABLE_OR_FALLTHROUGH();
    case FP64:
      vform = kFormatD;
      break;
    case FP32:
      vform = kFormatS;
      break;
    case FP16:
      vform = kFormatH;
      break;
  }
  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());
  SimVRegister& rm = ReadVRegister(instr->GetRm());

  switch (instr->Mask(FPDataProcessing2SourceMask)) {
    case FADD_h:
    case FADD_s:
    case FADD_d:
      fadd(vform, rd, rn, rm);
      break;
    case FSUB_h:
    case FSUB_s:
    case FSUB_d:
      fsub(vform, rd, rn, rm);
      break;
    case FMUL_h:
    case FMUL_s:
    case FMUL_d:
      fmul(vform, rd, rn, rm);
      break;
    case FNMUL_h:
    case FNMUL_s:
    case FNMUL_d:
      fnmul(vform, rd, rn, rm);
      break;
    case FDIV_h:
    case FDIV_s:
    case FDIV_d:
      fdiv(vform, rd, rn, rm);
      break;
    case FMAX_h:
    case FMAX_s:
    case FMAX_d:
      fmax(vform, rd, rn, rm);
      break;
    case FMIN_h:
    case FMIN_s:
    case FMIN_d:
      fmin(vform, rd, rn, rm);
      break;
    case FMAXNM_h:
    case FMAXNM_s:
    case FMAXNM_d:
      fmaxnm(vform, rd, rn, rm);
      break;
    case FMINNM_h:
    case FMINNM_s:
    case FMINNM_d:
      fminnm(vform, rd, rn, rm);
      break;
    default:
      VIXL_UNREACHABLE();
  }
  // Explicitly log the register update whilst we have type information.
  LogVRegister(instr->GetRd(), GetPrintRegisterFormatFP(vform));
}


void Simulator::VisitFPDataProcessing3Source(const Instruction* instr) {
  AssertSupportedFPCR();

  unsigned fd = instr->GetRd();
  unsigned fn = instr->GetRn();
  unsigned fm = instr->GetRm();
  unsigned fa = instr->GetRa();

  switch (instr->Mask(FPDataProcessing3SourceMask)) {
    // fd = fa +/- (fn * fm)
    case FMADD_h:
      WriteHRegister(fd,
                     FPMulAdd(ReadHRegister(fa),
                              ReadHRegister(fn),
                              ReadHRegister(fm)));
      break;
    case FMSUB_h:
      WriteHRegister(fd,
                     FPMulAdd(ReadHRegister(fa),
                              -ReadHRegister(fn),
                              ReadHRegister(fm)));
      break;
    case FMADD_s:
      WriteSRegister(fd,
                     FPMulAdd(ReadSRegister(fa),
                              ReadSRegister(fn),
                              ReadSRegister(fm)));
      break;
    case FMSUB_s:
      WriteSRegister(fd,
                     FPMulAdd(ReadSRegister(fa),
                              -ReadSRegister(fn),
                              ReadSRegister(fm)));
      break;
    case FMADD_d:
      WriteDRegister(fd,
                     FPMulAdd(ReadDRegister(fa),
                              ReadDRegister(fn),
                              ReadDRegister(fm)));
      break;
    case FMSUB_d:
      WriteDRegister(fd,
                     FPMulAdd(ReadDRegister(fa),
                              -ReadDRegister(fn),
                              ReadDRegister(fm)));
      break;
    // Negated variants of the above.
    case FNMADD_h:
      WriteHRegister(fd,
                     FPMulAdd(-ReadHRegister(fa),
                              -ReadHRegister(fn),
                              ReadHRegister(fm)));
      break;
    case FNMSUB_h:
      WriteHRegister(fd,
                     FPMulAdd(-ReadHRegister(fa),
                              ReadHRegister(fn),
                              ReadHRegister(fm)));
      break;
    case FNMADD_s:
      WriteSRegister(fd,
                     FPMulAdd(-ReadSRegister(fa),
                              -ReadSRegister(fn),
                              ReadSRegister(fm)));
      break;
    case FNMSUB_s:
      WriteSRegister(fd,
                     FPMulAdd(-ReadSRegister(fa),
                              ReadSRegister(fn),
                              ReadSRegister(fm)));
      break;
    case FNMADD_d:
      WriteDRegister(fd,
                     FPMulAdd(-ReadDRegister(fa),
                              -ReadDRegister(fn),
                              ReadDRegister(fm)));
      break;
    case FNMSUB_d:
      WriteDRegister(fd,
                     FPMulAdd(-ReadDRegister(fa),
                              ReadDRegister(fn),
                              ReadDRegister(fm)));
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
}


bool Simulator::FPProcessNaNs(const Instruction* instr) {
  unsigned fd = instr->GetRd();
  unsigned fn = instr->GetRn();
  unsigned fm = instr->GetRm();
  bool done = false;

  if (instr->Mask(FP64) == FP64) {
    double result = FPProcessNaNs(ReadDRegister(fn), ReadDRegister(fm));
    if (IsNaN(result)) {
      WriteDRegister(fd, result);
      done = true;
    }
  } else if (instr->Mask(FP32) == FP32) {
    float result = FPProcessNaNs(ReadSRegister(fn), ReadSRegister(fm));
    if (IsNaN(result)) {
      WriteSRegister(fd, result);
      done = true;
    }
  } else {
    VIXL_ASSERT(instr->Mask(FP16) == FP16);
    VIXL_UNIMPLEMENTED();
  }

  return done;
}


bool Simulator::SysOp_W(int op, int64_t val) {
  switch (op) {
    case IVAU:
    case CVAC:
    case CVAU:
    case CVAP:
    case CVADP:
    case CIVAC:
    case CGVAC:
    case CGDVAC:
    case CGVAP:
    case CGDVAP:
    case CIGVAC:
    case CIGDVAC: {
      // Perform a placeholder memory access to ensure that we have read access
      // to the specified address. The read access does not require a tag match,
      // so temporarily disable MTE.
      bool mte_enabled = MetaDataDepot::MetaDataMTE::IsActive();
      MetaDataDepot::MetaDataMTE::SetActive(false);
      volatile uint8_t y = *MemRead<uint8_t>(val);
      MetaDataDepot::MetaDataMTE::SetActive(mte_enabled);
      USE(y);
      break;
    }
    case ZVA: {
      if ((dczid_ & 0x10) != 0) {  // Check dc zva is enabled.
        return false;
      }
      int blocksize = (1 << (dczid_ & 0xf)) * kWRegSizeInBytes;
      VIXL_ASSERT(IsMultiple(blocksize, sizeof(uint64_t)));
      uintptr_t addr = AlignDown(val, blocksize);
      for (int i = 0; i < blocksize; i += sizeof(uint64_t)) {
        MemWrite<uint64_t>(addr + i, 0);
        LogWriteU64(0, addr + i);
      }
      break;
    }
    // TODO: Implement GVA, GZVA.
    default:
      VIXL_UNIMPLEMENTED();
      return false;
  }
  return true;
}

void Simulator::PACHelper(int dst,
                          int src,
                          PACKey key,
                          decltype(&Simulator::AddPAC) pac_fn) {
  VIXL_ASSERT((dst == 17) || (dst == 30));
  VIXL_ASSERT((src == -1) || (src == 16) || (src == 31));

  uint64_t modifier = (src == -1) ? 0 : ReadXRegister(src, Reg31IsStackPointer);
  uint64_t result =
      (this->*pac_fn)(ReadXRegister(dst), modifier, key, kInstructionPointer);
  WriteXRegister(dst, result);
}

void Simulator::VisitSystem(const Instruction* instr) {
  PACKey pac_key = kPACKeyIA;  // Default key for PAC/AUTH handling.

  switch (form_hash_) {
    case "cfinv_m_pstate"_h:
      ReadNzcv().SetC(!ReadC());
      LogSystemRegister(NZCV);
      break;
    case "axflag_m_pstate"_h:
      ReadNzcv().SetN(0);
      ReadNzcv().SetZ(ReadNzcv().GetZ() | ReadNzcv().GetV());
      ReadNzcv().SetC(ReadNzcv().GetC() & ~ReadNzcv().GetV());
      ReadNzcv().SetV(0);
      LogSystemRegister(NZCV);
      break;
    case "xaflag_m_pstate"_h: {
      // Can't set the flags in place due to the logical dependencies.
      uint32_t n = (~ReadNzcv().GetC() & ~ReadNzcv().GetZ()) & 1;
      uint32_t z = ReadNzcv().GetZ() & ReadNzcv().GetC();
      uint32_t c = ReadNzcv().GetC() | ReadNzcv().GetZ();
      uint32_t v = ~ReadNzcv().GetC() & ReadNzcv().GetZ();
      ReadNzcv().SetN(n);
      ReadNzcv().SetZ(z);
      ReadNzcv().SetC(c);
      ReadNzcv().SetV(v);
      LogSystemRegister(NZCV);
      break;
    }
    case "xpaclri_hi_hints"_h:
      WriteXRegister(30, StripPAC(ReadXRegister(30), kInstructionPointer));
      break;
    case "clrex_bn_barriers"_h:
      PrintExclusiveAccessWarning();
      ClearLocalMonitor();
      break;
    case "msr_sr_systemmove"_h:
      switch (instr->GetImmSystemRegister()) {
        case NZCV:
          ReadNzcv().SetRawValue(ReadWRegister(instr->GetRt()));
          LogSystemRegister(NZCV);
          break;
        case FPCR:
          ReadFpcr().SetRawValue(ReadWRegister(instr->GetRt()));
          LogSystemRegister(FPCR);
          break;
        default:
          VIXL_UNIMPLEMENTED();
      }
      break;
    case "mrs_rs_systemmove"_h:
      switch (instr->GetImmSystemRegister()) {
        case NZCV:
          WriteXRegister(instr->GetRt(), ReadNzcv().GetRawValue());
          break;
        case FPCR:
          WriteXRegister(instr->GetRt(), ReadFpcr().GetRawValue());
          break;
        case RNDR:
        case RNDRRS: {
          uint64_t high = rand_gen_();
          uint64_t low = rand_gen_();
          uint64_t rand_num = (high << 32) | (low & 0xffffffff);
          WriteXRegister(instr->GetRt(), rand_num);
          // Simulate successful random number generation.
          // TODO: Return failure occasionally as a random number cannot be
          // returned in a period of time.
          ReadNzcv().SetRawValue(NoFlag);
          LogSystemRegister(NZCV);
          break;
        }
        case DCZID_EL0:
          WriteXRegister(instr->GetRt(), dczid_);
          break;
        default:
          VIXL_UNIMPLEMENTED();
      }
      break;
    case "chkfeat_hf_hints"_h: {
      uint64_t feat_select = ReadXRegister(16);
      uint64_t gcs_enabled = IsGCSCheckEnabled() ? 1 : 0;
      feat_select &= ~gcs_enabled;
      WriteXRegister(16, feat_select);
      break;
    }
    case "hint_hm_hints"_h:
    case "nop_hi_hints"_h:
    case "yield_hi_hints"_h:
    case "esb_hi_hints"_h:
    case "csdb_hi_hints"_h:
      break;
    case "bti_hb_hints"_h:
      switch (instr->GetImmHint()) {
        case BTI_jc:
          break;
        case BTI:
          if (PcIsInGuardedPage() && (ReadBType() != DefaultBType)) {
            VIXL_ABORT_WITH_MSG("Executing BTI with wrong BType.");
          }
          break;
        case BTI_c:
          if (PcIsInGuardedPage() &&
              (ReadBType() == BranchFromGuardedNotToIP)) {
            VIXL_ABORT_WITH_MSG("Executing BTI c with wrong BType.");
          }
          break;
        case BTI_j:
          if (PcIsInGuardedPage() && (ReadBType() == BranchAndLink)) {
            VIXL_ABORT_WITH_MSG("Executing BTI j with wrong BType.");
          }
          break;
        default:
          VIXL_UNREACHABLE();
      }
      return;
    case "pacib1716_hi_hints"_h:
      pac_key = kPACKeyIB;
      VIXL_FALLTHROUGH();
    case "pacia1716_hi_hints"_h:
      PACHelper(17, 16, pac_key, &Simulator::AddPAC);
      break;
    case "pacibsp_hi_hints"_h:
      pac_key = kPACKeyIB;
      VIXL_FALLTHROUGH();
    case "paciasp_hi_hints"_h:
      PACHelper(30, 31, pac_key, &Simulator::AddPAC);

      // Check BType allows PACI[AB]SP instructions.
      if (PcIsInGuardedPage()) {
        switch (ReadBType()) {
          case BranchFromGuardedNotToIP:
          // TODO: This case depends on the value of SCTLR_EL1.BT0, which we
          // assume here to be zero. This allows execution of PACI[AB]SP when
          // BTYPE is BranchFromGuardedNotToIP (0b11).
          case DefaultBType:
          case BranchFromUnguardedOrToIP:
          case BranchAndLink:
            break;
        }
      }
      break;
    case "pacibz_hi_hints"_h:
      pac_key = kPACKeyIB;
      VIXL_FALLTHROUGH();
    case "paciaz_hi_hints"_h:
      PACHelper(30, -1, pac_key, &Simulator::AddPAC);
      break;
    case "autib1716_hi_hints"_h:
      pac_key = kPACKeyIB;
      VIXL_FALLTHROUGH();
    case "autia1716_hi_hints"_h:
      PACHelper(17, 16, pac_key, &Simulator::AuthPAC);
      break;
    case "autibsp_hi_hints"_h:
      pac_key = kPACKeyIB;
      VIXL_FALLTHROUGH();
    case "autiasp_hi_hints"_h:
      PACHelper(30, 31, pac_key, &Simulator::AuthPAC);
      break;
    case "autibz_hi_hints"_h:
      pac_key = kPACKeyIB;
      VIXL_FALLTHROUGH();
    case "autiaz_hi_hints"_h:
      PACHelper(30, -1, pac_key, &Simulator::AuthPAC);
      break;
    case "dsb_bo_barriers"_h:
    case "dmb_bo_barriers"_h:
    case "isb_bi_barriers"_h:
      VIXL_SYNC();
      break;
    case "sys_cr_systeminstrs"_h: {
      uint64_t rt = ReadXRegister(instr->GetRt());
      uint32_t sysop = instr->GetSysOp();
      if (sysop == GCSSS1) {
        uint64_t incoming_size = rt >> 32;
        // Drop upper 32 bits to get GCS index.
        uint64_t incoming_gcs = rt & 0xffffffff;
        GuardedControlStack outgoing_gcs = ActivateGCS(incoming_gcs);
        uint64_t incoming_seal = GCSPop();
        if (((incoming_seal ^ rt) != 1) ||
            (GetGCSStorage()->size() != incoming_size)) {
          char msg[128];
          snprintf(msg,
                   sizeof(msg),
                   "GCS: invalid incoming stack: 0x%016" PRIx64 "\n",
                   incoming_seal);
          ReportGCSFailure(msg);
        }
        GCSPush(outgoing_gcs.token + 5);
      } else if (sysop == GCSPUSHM) {
        GCSPush(ReadXRegister(instr->GetRt()));
      } else {
        if (!SysOp_W(sysop, rt)) {
          VisitUnallocated(instr);
        }
      }
      break;
    }
    case "sysl_rc_systeminstrs"_h: {
      uint32_t sysop = instr->GetSysOp();
      if (sysop == GCSPOPM) {
        uint64_t addr = GCSPop();
        WriteXRegister(instr->GetRt(), addr);
      } else if (sysop == GCSSS2) {
        uint64_t outgoing_gcs = GCSPop();
        // Check for token inserted by gcsss1.
        if ((outgoing_gcs & 7) != 5) {
          char msg[128];
          snprintf(msg,
                   sizeof(msg),
                   "GCS: outgoing stack has no token: 0x%016" PRIx64 "\n",
                   outgoing_gcs);
          ReportGCSFailure(msg);
        }
        outgoing_gcs &= ~UINT64_C(0x3ff);
        GuardedControlStack incoming_gcs = ActivateGCS(outgoing_gcs);

        // Encode the size into the outgoing stack seal, to check later.
        uint64_t size = GetGCSStorage()->size();
        VIXL_ASSERT(IsUint32(size));
        VIXL_ASSERT(IsUint32(outgoing_gcs + 1));
        uint64_t outgoing_seal = (size << 32) | (outgoing_gcs + 1);
        GCSPush(outgoing_seal);
        ActivateGCS(incoming_gcs);
        WriteXRegister(instr->GetRt(), outgoing_seal - 1);
      } else {
        VIXL_UNIMPLEMENTED();
      }
      break;
    }
    default:
      VIXL_UNIMPLEMENTED();
  }
}


void Simulator::VisitException(const Instruction* instr) {
  switch (instr->Mask(ExceptionMask)) {
    case HLT:
      switch (instr->GetImmException()) {
        case kUnreachableOpcode:
          DoUnreachable(instr);
          return;
        case kTraceOpcode:
          DoTrace(instr);
          return;
        case kLogOpcode:
          DoLog(instr);
          return;
        case kPrintfOpcode:
          DoPrintf(instr);
          return;
        case kRuntimeCallOpcode:
          DoRuntimeCall(instr);
          return;
        case kSetCPUFeaturesOpcode:
        case kEnableCPUFeaturesOpcode:
        case kDisableCPUFeaturesOpcode:
          DoConfigureCPUFeatures(instr);
          return;
        case kSaveCPUFeaturesOpcode:
          DoSaveCPUFeatures(instr);
          return;
        case kRestoreCPUFeaturesOpcode:
          DoRestoreCPUFeatures(instr);
          return;
        case kMTEActive:
          MetaDataDepot::MetaDataMTE::SetActive(true);
          return;
        case kMTEInactive:
          MetaDataDepot::MetaDataMTE::SetActive(false);
          return;
        default:
          HostBreakpoint();
          return;
      }
    case BRK:
      if (debugger_enabled_) {
        uint64_t next_instr =
            reinterpret_cast<uint64_t>(pc_->GetNextInstruction());
        if (!debugger_->IsBreakpoint(next_instr)) {
          debugger_->RegisterBreakpoint(next_instr);
        }
      } else {
        HostBreakpoint();
      }
      return;
    default:
      VIXL_UNIMPLEMENTED();
  }
}


void Simulator::VisitCrypto2RegSHA(const Instruction* instr) {
  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());

  switch (form_hash_) {
    case "sha1h_ss_cryptosha2"_h:
      ror(kFormatS, rd, rn, 2);
      break;
    case "sha1su1_vv_cryptosha2"_h: {
      SimVRegister temp;

      // temp = srcdst ^ (src >> 32);
      ext(kFormat16B, temp, rn, temp, 4);
      eor(kFormat16B, temp, rd, temp);

      // srcdst = ROL(temp, 1) ^ (ROL(temp, 2) << 96)
      rol(kFormat4S, rd, temp, 1);
      rol(kFormatS, temp, temp, 2);  // kFormatS will zero bits <127:32>
      ext(kFormat16B, temp, temp, temp, 4);
      eor(kFormat16B, rd, rd, temp);
      break;
    }
    case "sha256su0_vv_cryptosha2"_h:
      sha2su0(rd, rn);
      break;
  }
}


void Simulator::VisitCrypto3RegSHA(const Instruction* instr) {
  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());
  SimVRegister& rm = ReadVRegister(instr->GetRm());

  switch (form_hash_) {
    case "sha1c_qsv_cryptosha3"_h:
      sha1<"choose"_h>(rd, rn, rm);
      break;
    case "sha1m_qsv_cryptosha3"_h:
      sha1<"majority"_h>(rd, rn, rm);
      break;
    case "sha1p_qsv_cryptosha3"_h:
      sha1<"parity"_h>(rd, rn, rm);
      break;
    case "sha1su0_vvv_cryptosha3"_h: {
      SimVRegister temp;
      ext(kFormat16B, temp, rd, rn, 8);
      eor(kFormat16B, temp, temp, rd);
      eor(kFormat16B, rd, temp, rm);
      break;
    }
    case "sha256h_qqv_cryptosha3"_h:
      sha2h(rd, rn, rm, /* part1 = */ true);
      break;
    case "sha256h2_qqv_cryptosha3"_h:
      sha2h(rd, rn, rm, /* part1 = */ false);
      break;
    case "sha256su1_vvv_cryptosha3"_h:
      sha2su1(rd, rn, rm);
      break;
  }
}


void Simulator::VisitCryptoAES(const Instruction* instr) {
  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());
  SimVRegister temp;

  switch (form_hash_) {
    case "aesd_b_cryptoaes"_h:
      eor(kFormat16B, temp, rd, rn);
      aes(rd, temp, /* decrypt = */ true);
      break;
    case "aese_b_cryptoaes"_h:
      eor(kFormat16B, temp, rd, rn);
      aes(rd, temp, /* decrypt = */ false);
      break;
    case "aesimc_b_cryptoaes"_h:
      aesmix(rd, rn, /* inverse = */ true);
      break;
    case "aesmc_b_cryptoaes"_h:
      aesmix(rd, rn, /* inverse = */ false);
      break;
  }
}

void Simulator::VisitCryptoSM3(const Instruction* instr) {
  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());
  SimVRegister& rm = ReadVRegister(instr->GetRm());
  SimVRegister& ra = ReadVRegister(instr->GetRa());
  int index = instr->ExtractBits(13, 12);

  bool is_a = false;
  switch (form_hash_) {
    case "sm3partw1_vvv4_cryptosha512_3"_h:
      sm3partw1(rd, rn, rm);
      break;
    case "sm3partw2_vvv4_cryptosha512_3"_h:
      sm3partw2(rd, rn, rm);
      break;
    case "sm3ss1_vvv4_crypto4"_h:
      sm3ss1(rd, rn, rm, ra);
      break;
    case "sm3tt1a_vvv4_crypto3_imm2"_h:
      is_a = true;
      VIXL_FALLTHROUGH();
    case "sm3tt1b_vvv4_crypto3_imm2"_h:
      sm3tt1(rd, rn, rm, index, is_a);
      break;
    case "sm3tt2a_vvv4_crypto3_imm2"_h:
      is_a = true;
      VIXL_FALLTHROUGH();
    case "sm3tt2b_vvv_crypto3_imm2"_h:
      sm3tt2(rd, rn, rm, index, is_a);
      break;
  }
}

void Simulator::VisitCryptoSM4(const Instruction* instr) {
  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());
  SimVRegister& rm = ReadVRegister(instr->GetRm());

  bool is_key = false;
  switch (form_hash_) {
    case "sm4ekey_vvv4_cryptosha512_3"_h:
      is_key = true;
      VIXL_FALLTHROUGH();
    case "sm4e_vv4_cryptosha512_2"_h:
      sm4(rd, rn, rm, is_key);
      break;
  }
}

void Simulator::SimulateSHA512(const Instruction* instr) {
  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());
  SimVRegister& rm = ReadVRegister(instr->GetRm());

  switch (form_hash_) {
    case "sha512h_qqv_cryptosha512_3"_h:
      sha512h(rd, rn, rm);
      break;
    case "sha512h2_qqv_cryptosha512_3"_h:
      sha512h2(rd, rn, rm);
      break;
    case "sha512su0_vv2_cryptosha512_2"_h:
      sha512su0(rd, rn);
      break;
    case "sha512su1_vvv2_cryptosha512_3"_h:
      sha512su1(rd, rn, rm);
      break;
  }
}

void Simulator::SimulateNEONRoundIntToSize(const Instruction* instr) {
  NEONFormatDecoder nfd(instr);
  VectorFormat vform = nfd.GetVectorFormat(nfd.FPFormatMap());
  FPRounding rounding_mode = static_cast<FPRounding>(ReadFpcr().GetRMode());
  FrintMode frint_mode = kFrintToInt32;
  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());

  frint_mode = kFrintToInt32;
  switch (form_hash_) {
    case "frint32z_asimdmisc_r"_h:
      rounding_mode = FPZero;
      break;
    case "frint64z_asimdmisc_r"_h:
      rounding_mode = FPZero;
      VIXL_FALLTHROUGH();
    case "frint64x_asimdmisc_r"_h:
      frint_mode = kFrintToInt64;
      break;
  }
  frint(vform,
        rd,
        rn,
        rounding_mode,
        /* inexact_exception = */ true,
        frint_mode);
}

void Simulator::SimulateNEONRoundInt(const Instruction* instr) {
  NEONFormatDecoder nfd(instr);
  VectorFormat vform = nfd.GetVectorFormat(nfd.FPFormatMap());
  FPRounding rounding_mode = static_cast<FPRounding>(ReadFpcr().GetRMode());
  bool inexact_exception = false;
  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());

  switch (form_hash_) {
    case "frinta_asimdmisc_r"_h:
      rounding_mode = FPTieAway;
      break;
    case "frintm_asimdmisc_r"_h:
      rounding_mode = FPNegativeInfinity;
      break;
    case "frintn_asimdmisc_r"_h:
      rounding_mode = FPTieEven;
      break;
    case "frintp_asimdmisc_r"_h:
      rounding_mode = FPPositiveInfinity;
      break;
    case "frintx_asimdmisc_r"_h:
      inexact_exception = true;
      break;
    case "frintz_asimdmisc_r"_h:
      rounding_mode = FPZero;
      break;
    case "frinti_asimdmisc_r"_h:
      // Uses FPCR - nothing to do.
      break;
  }
  frint(vform, rd, rn, rounding_mode, inexact_exception);
}

void Simulator::SimulateNEONFPConvert(const Instruction* instr) {
  NEONFormatDecoder nfd(instr);
  VectorFormat vform = nfd.GetVectorFormat(nfd.FPFormatMap());
  static const NEONFormatMap map_fcvtl = {{22}, {NF_4S, NF_2D}};
  VectorFormat vf_fcvtl = nfd.GetVectorFormat(&map_fcvtl);

  static const NEONFormatMap map_fcvtn = {{22, 30},
                                          {NF_4H, NF_8H, NF_2S, NF_4S}};
  VectorFormat vf_fcvtn = nfd.GetVectorFormat(&map_fcvtn);

  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());
  bool is_q = instr->Mask(NEON_Q) != 0;
  std::function<LogicVRegister(
      Simulator*, VectorFormat, LogicVRegister, const LogicVRegister&)>
      handler = nullptr;

  switch (form_hash_) {
    case "fcvtas_asimdmisc_r"_h:
      fcvts(vform, rd, rn, FPTieAway);
      break;
    case "fcvtau_asimdmisc_r"_h:
      fcvtu(vform, rd, rn, FPTieAway);
      break;
    case "fcvtms_asimdmisc_r"_h:
      fcvts(vform, rd, rn, FPNegativeInfinity);
      break;
    case "fcvtmu_asimdmisc_r"_h:
      fcvtu(vform, rd, rn, FPNegativeInfinity);
      break;
    case "fcvtns_asimdmisc_r"_h:
      fcvts(vform, rd, rn, FPTieEven);
      break;
    case "fcvtnu_asimdmisc_r"_h:
      fcvtu(vform, rd, rn, FPTieEven);
      break;
    case "fcvtps_asimdmisc_r"_h:
      fcvts(vform, rd, rn, FPPositiveInfinity);
      break;
    case "fcvtpu_asimdmisc_r"_h:
      fcvtu(vform, rd, rn, FPPositiveInfinity);
      break;
    case "fcvtzs_asimdmisc_r"_h:
      fcvts(vform, rd, rn, FPZero);
      break;
    case "fcvtzu_asimdmisc_r"_h:
      fcvtu(vform, rd, rn, FPZero);
      break;
    case "fcvtl_asimdmisc_l"_h:
      handler = is_q ? &Simulator::fcvtl2 : &Simulator::fcvtl;
      handler(this, vf_fcvtl, rd, rn);
      break;
    case "fcvtn_asimdmisc_n"_h:
      handler = is_q ? &Simulator::fcvtn2 : &Simulator::fcvtn;
      handler(this, vf_fcvtn, rd, rn);
      break;
    case "fcvtxn_asimdmisc_n"_h:
      handler = is_q ? &Simulator::fcvtxn2 : &Simulator::fcvtxn;
      handler(this, vf_fcvtn, rd, rn);
      break;
    case "bfcvtn_asimdmisc_4s"_h:
      handler = is_q ? &Simulator::bfcvtn2 : &Simulator::bfcvtn;
      handler(this, is_q ? kFormat8H : kFormat4H, rd, rn);
      break;
  }
}

void Simulator::SimulateNEONFP2RegMisc(const Instruction* instr) {
  NEONFormatDecoder nfd(instr);
  VectorFormat vform = nfd.GetVectorFormat(nfd.FPFormatMap());
  FPRounding rounding_mode = static_cast<FPRounding>(ReadFpcr().GetRMode());

  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());

  switch (form_hash_) {
    case "fabs_asimdmisc_r"_h:
      fabs_(vform, rd, rn);
      break;
    case "fneg_asimdmisc_r"_h:
      fneg(vform, rd, rn);
      break;
    case "fsqrt_asimdmisc_r"_h:
      fsqrt(vform, rd, rn);
      break;
    case "scvtf_asimdmisc_r"_h:
      scvtf(vform, rd, rn, 0, rounding_mode);
      break;
    case "ucvtf_asimdmisc_r"_h:
      ucvtf(vform, rd, rn, 0, rounding_mode);
      break;
    case "ursqrte_asimdmisc_r"_h:
      ursqrte(vform, rd, rn);
      break;
    case "urecpe_asimdmisc_r"_h:
      urecpe(vform, rd, rn);
      break;
    case "frsqrte_asimdmisc_r"_h:
      frsqrte(vform, rd, rn);
      break;
    case "frecpe_asimdmisc_r"_h:
      frecpe(vform, rd, rn, rounding_mode);
      break;
    case "fcmgt_asimdmisc_fz"_h:
      fcmp_zero(vform, rd, rn, gt);
      break;
    case "fcmge_asimdmisc_fz"_h:
      fcmp_zero(vform, rd, rn, ge);
      break;
    case "fcmeq_asimdmisc_fz"_h:
      fcmp_zero(vform, rd, rn, eq);
      break;
    case "fcmle_asimdmisc_fz"_h:
      fcmp_zero(vform, rd, rn, le);
      break;
    case "fcmlt_asimdmisc_fz"_h:
      fcmp_zero(vform, rd, rn, lt);
      break;
  }
}

void Simulator::VisitNEON2RegMisc(const Instruction* instr) {
  NEONFormatDecoder nfd(instr);
  VectorFormat vf = nfd.GetVectorFormat();
  VectorFormat vf_log = nfd.GetVectorFormat(nfd.LogicalFormatMap());

  static const NEONFormatMap map_lp =
      {{23, 22, 30}, {NF_4H, NF_8H, NF_2S, NF_4S, NF_1D, NF_2D}};
  VectorFormat vf_lp = nfd.GetVectorFormat(&map_lp);

  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());
  bool is_q = instr->Mask(NEON_Q) != 0;

  switch (form_hash_) {
    case "rev64_asimdmisc_r"_h:
      rev64(vf, rd, rn);
      break;
    case "rev32_asimdmisc_r"_h:
      rev32(vf, rd, rn);
      break;
    case "rev16_asimdmisc_r"_h:
      rev16(vf, rd, rn);
      break;
    case "suqadd_asimdmisc_r"_h:
      suqadd(vf, rd, rd, rn);
      break;
    case "usqadd_asimdmisc_r"_h:
      usqadd(vf, rd, rd, rn);
      break;
    case "cls_asimdmisc_r"_h:
      cls(vf, rd, rn);
      break;
    case "clz_asimdmisc_r"_h:
      clz(vf, rd, rn);
      break;
    case "cnt_asimdmisc_r"_h:
      cnt(vf, rd, rn);
      break;
    case "sqabs_asimdmisc_r"_h:
      abs(vf, rd, rn).SignedSaturate(vf);
      break;
    case "sqneg_asimdmisc_r"_h:
      neg(vf, rd, rn).SignedSaturate(vf);
      break;
    case "cmgt_asimdmisc_z"_h:
      cmp(vf, rd, rn, 0, gt);
      break;
    case "cmge_asimdmisc_z"_h:
      cmp(vf, rd, rn, 0, ge);
      break;
    case "cmeq_asimdmisc_z"_h:
      cmp(vf, rd, rn, 0, eq);
      break;
    case "cmle_asimdmisc_z"_h:
      cmp(vf, rd, rn, 0, le);
      break;
    case "cmlt_asimdmisc_z"_h:
      cmp(vf, rd, rn, 0, lt);
      break;
    case "abs_asimdmisc_r"_h:
      abs(vf, rd, rn);
      break;
    case "neg_asimdmisc_r"_h:
      neg(vf, rd, rn);
      break;
    case "xtn_asimdmisc_n"_h:
      xtn(vf, rd, rn);
      break;
    case "sqxtn_asimdmisc_n"_h:
      sqxtn(vf, rd, rn);
      break;
    case "uqxtn_asimdmisc_n"_h:
      uqxtn(vf, rd, rn);
      break;
    case "sqxtun_asimdmisc_n"_h:
      sqxtun(vf, rd, rn);
      break;
    case "saddlp_asimdmisc_p"_h:
      saddlp(vf_lp, rd, rn);
      break;
    case "uaddlp_asimdmisc_p"_h:
      uaddlp(vf_lp, rd, rn);
      break;
    case "sadalp_asimdmisc_p"_h:
      sadalp(vf_lp, rd, rn);
      break;
    case "uadalp_asimdmisc_p"_h:
      uadalp(vf_lp, rd, rn);
      break;
    case "not_asimdmisc_r"_h:
      not_(vf_log, rd, rn);
      break;
    case "rbit_asimdmisc_r"_h:
      rbit(vf_log, rd, rn);
      break;
    case "shll_asimdmisc_s"_h:
      vf = nfd.GetVectorFormat(nfd.LongIntegerFormatMap());
      is_q ? shll2(vf, rd, rn) : shll(vf, rd, rn);
      break;
  }
}


void Simulator::VisitNEON2RegMiscFP16(const Instruction* instr) {
  static const NEONFormatMap map_half = {{30}, {NF_4H, NF_8H}};
  NEONFormatDecoder nfd(instr);
  VectorFormat fpf = nfd.GetVectorFormat(&map_half);

  FPRounding fpcr_rounding = static_cast<FPRounding>(ReadFpcr().GetRMode());

  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());

  switch (instr->Mask(NEON2RegMiscFP16Mask)) {
    case NEON_SCVTF_H:
      scvtf(fpf, rd, rn, 0, fpcr_rounding);
      return;
    case NEON_UCVTF_H:
      ucvtf(fpf, rd, rn, 0, fpcr_rounding);
      return;
    case NEON_FCVTNS_H:
      fcvts(fpf, rd, rn, FPTieEven);
      return;
    case NEON_FCVTNU_H:
      fcvtu(fpf, rd, rn, FPTieEven);
      return;
    case NEON_FCVTPS_H:
      fcvts(fpf, rd, rn, FPPositiveInfinity);
      return;
    case NEON_FCVTPU_H:
      fcvtu(fpf, rd, rn, FPPositiveInfinity);
      return;
    case NEON_FCVTMS_H:
      fcvts(fpf, rd, rn, FPNegativeInfinity);
      return;
    case NEON_FCVTMU_H:
      fcvtu(fpf, rd, rn, FPNegativeInfinity);
      return;
    case NEON_FCVTZS_H:
      fcvts(fpf, rd, rn, FPZero);
      return;
    case NEON_FCVTZU_H:
      fcvtu(fpf, rd, rn, FPZero);
      return;
    case NEON_FCVTAS_H:
      fcvts(fpf, rd, rn, FPTieAway);
      return;
    case NEON_FCVTAU_H:
      fcvtu(fpf, rd, rn, FPTieAway);
      return;
    case NEON_FRINTI_H:
      frint(fpf, rd, rn, fpcr_rounding, false);
      return;
    case NEON_FRINTX_H:
      frint(fpf, rd, rn, fpcr_rounding, true);
      return;
    case NEON_FRINTA_H:
      frint(fpf, rd, rn, FPTieAway, false);
      return;
    case NEON_FRINTM_H:
      frint(fpf, rd, rn, FPNegativeInfinity, false);
      return;
    case NEON_FRINTN_H:
      frint(fpf, rd, rn, FPTieEven, false);
      return;
    case NEON_FRINTP_H:
      frint(fpf, rd, rn, FPPositiveInfinity, false);
      return;
    case NEON_FRINTZ_H:
      frint(fpf, rd, rn, FPZero, false);
      return;
    case NEON_FABS_H:
      fabs_(fpf, rd, rn);
      return;
    case NEON_FNEG_H:
      fneg(fpf, rd, rn);
      return;
    case NEON_FSQRT_H:
      fsqrt(fpf, rd, rn);
      return;
    case NEON_FRSQRTE_H:
      frsqrte(fpf, rd, rn);
      return;
    case NEON_FRECPE_H:
      frecpe(fpf, rd, rn, fpcr_rounding);
      return;
    case NEON_FCMGT_H_zero:
      fcmp_zero(fpf, rd, rn, gt);
      return;
    case NEON_FCMGE_H_zero:
      fcmp_zero(fpf, rd, rn, ge);
      return;
    case NEON_FCMEQ_H_zero:
      fcmp_zero(fpf, rd, rn, eq);
      return;
    case NEON_FCMLE_H_zero:
      fcmp_zero(fpf, rd, rn, le);
      return;
    case NEON_FCMLT_H_zero:
      fcmp_zero(fpf, rd, rn, lt);
      return;
    default:
      VIXL_UNIMPLEMENTED();
      return;
  }
}


void Simulator::VisitNEON3Same(const Instruction* instr) {
  NEONFormatDecoder nfd(instr);
  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());
  SimVRegister& rm = ReadVRegister(instr->GetRm());

  if (instr->Mask(NEON3SameLogicalFMask) == NEON3SameLogicalFixed) {
    VectorFormat vf = nfd.GetVectorFormat(nfd.LogicalFormatMap());
    switch (instr->Mask(NEON3SameLogicalMask)) {
      case NEON_AND:
        and_(vf, rd, rn, rm);
        break;
      case NEON_ORR:
        orr(vf, rd, rn, rm);
        break;
      case NEON_ORN:
        orn(vf, rd, rn, rm);
        break;
      case NEON_EOR:
        eor(vf, rd, rn, rm);
        break;
      case NEON_BIC:
        bic(vf, rd, rn, rm);
        break;
      case NEON_BIF:
        bif(vf, rd, rn, rm);
        break;
      case NEON_BIT:
        bit(vf, rd, rn, rm);
        break;
      case NEON_BSL:
        bsl(vf, rd, rd, rn, rm);
        break;
      default:
        VIXL_UNIMPLEMENTED();
    }
  } else if (instr->Mask(NEON3SameFPFMask) == NEON3SameFPFixed) {
    VectorFormat vf = nfd.GetVectorFormat(nfd.FPFormatMap());
    switch (instr->Mask(NEON3SameFPMask)) {
      case NEON_FADD:
        fadd(vf, rd, rn, rm);
        break;
      case NEON_FSUB:
        fsub(vf, rd, rn, rm);
        break;
      case NEON_FMUL:
        fmul(vf, rd, rn, rm);
        break;
      case NEON_FDIV:
        fdiv(vf, rd, rn, rm);
        break;
      case NEON_FMAX:
        fmax(vf, rd, rn, rm);
        break;
      case NEON_FMIN:
        fmin(vf, rd, rn, rm);
        break;
      case NEON_FMAXNM:
        fmaxnm(vf, rd, rn, rm);
        break;
      case NEON_FMINNM:
        fminnm(vf, rd, rn, rm);
        break;
      case NEON_FMLA:
        fmla(vf, rd, rd, rn, rm);
        break;
      case NEON_FMLS:
        fmls(vf, rd, rd, rn, rm);
        break;
      case NEON_FMULX:
        fmulx(vf, rd, rn, rm);
        break;
      case NEON_FACGE:
        fabscmp(vf, rd, rn, rm, ge);
        break;
      case NEON_FACGT:
        fabscmp(vf, rd, rn, rm, gt);
        break;
      case NEON_FCMEQ:
        fcmp(vf, rd, rn, rm, eq);
        break;
      case NEON_FCMGE:
        fcmp(vf, rd, rn, rm, ge);
        break;
      case NEON_FCMGT:
        fcmp(vf, rd, rn, rm, gt);
        break;
      case NEON_FRECPS:
        frecps(vf, rd, rn, rm);
        break;
      case NEON_FRSQRTS:
        frsqrts(vf, rd, rn, rm);
        break;
      case NEON_FABD:
        fabd(vf, rd, rn, rm);
        break;
      case NEON_FADDP:
        faddp(vf, rd, rn, rm);
        break;
      case NEON_FMAXP:
        fmaxp(vf, rd, rn, rm);
        break;
      case NEON_FMAXNMP:
        fmaxnmp(vf, rd, rn, rm);
        break;
      case NEON_FMINP:
        fminp(vf, rd, rn, rm);
        break;
      case NEON_FMINNMP:
        fminnmp(vf, rd, rn, rm);
        break;
      default:
        // FMLAL{2} and FMLSL{2} have special-case encodings.
        switch (instr->Mask(NEON3SameFHMMask)) {
          case NEON_FMLAL:
            fmlal(vf, rd, rn, rm);
            break;
          case NEON_FMLAL2:
            fmlal2(vf, rd, rn, rm);
            break;
          case NEON_FMLSL:
            fmlsl(vf, rd, rn, rm);
            break;
          case NEON_FMLSL2:
            fmlsl2(vf, rd, rn, rm);
            break;
          default:
            VIXL_UNIMPLEMENTED();
        }
    }
  } else {
    VectorFormat vf = nfd.GetVectorFormat();
    switch (instr->Mask(NEON3SameMask)) {
      case NEON_ADD:
        add(vf, rd, rn, rm);
        break;
      case NEON_ADDP:
        addp(vf, rd, rn, rm);
        break;
      case NEON_CMEQ:
        cmp(vf, rd, rn, rm, eq);
        break;
      case NEON_CMGE:
        cmp(vf, rd, rn, rm, ge);
        break;
      case NEON_CMGT:
        cmp(vf, rd, rn, rm, gt);
        break;
      case NEON_CMHI:
        cmp(vf, rd, rn, rm, hi);
        break;
      case NEON_CMHS:
        cmp(vf, rd, rn, rm, hs);
        break;
      case NEON_CMTST:
        cmptst(vf, rd, rn, rm);
        break;
      case NEON_MLS:
        mls(vf, rd, rd, rn, rm);
        break;
      case NEON_MLA:
        mla(vf, rd, rd, rn, rm);
        break;
      case NEON_MUL:
        mul(vf, rd, rn, rm);
        break;
      case NEON_PMUL:
        pmul(vf, rd, rn, rm);
        break;
      case NEON_SMAX:
        smax(vf, rd, rn, rm);
        break;
      case NEON_SMAXP:
        smaxp(vf, rd, rn, rm);
        break;
      case NEON_SMIN:
        smin(vf, rd, rn, rm);
        break;
      case NEON_SMINP:
        sminp(vf, rd, rn, rm);
        break;
      case NEON_SUB:
        sub(vf, rd, rn, rm);
        break;
      case NEON_UMAX:
        umax(vf, rd, rn, rm);
        break;
      case NEON_UMAXP:
        umaxp(vf, rd, rn, rm);
        break;
      case NEON_UMIN:
        umin(vf, rd, rn, rm);
        break;
      case NEON_UMINP:
        uminp(vf, rd, rn, rm);
        break;
      case NEON_SSHL:
        sshl(vf, rd, rn, rm);
        break;
      case NEON_USHL:
        ushl(vf, rd, rn, rm);
        break;
      case NEON_SABD:
        absdiff(vf, rd, rn, rm, true);
        break;
      case NEON_UABD:
        absdiff(vf, rd, rn, rm, false);
        break;
      case NEON_SABA:
        saba(vf, rd, rn, rm);
        break;
      case NEON_UABA:
        uaba(vf, rd, rn, rm);
        break;
      case NEON_UQADD:
        add(vf, rd, rn, rm).UnsignedSaturate(vf);
        break;
      case NEON_SQADD:
        add(vf, rd, rn, rm).SignedSaturate(vf);
        break;
      case NEON_UQSUB:
        sub(vf, rd, rn, rm).UnsignedSaturate(vf);
        break;
      case NEON_SQSUB:
        sub(vf, rd, rn, rm).SignedSaturate(vf);
        break;
      case NEON_SQDMULH:
        sqdmulh(vf, rd, rn, rm);
        break;
      case NEON_SQRDMULH:
        sqrdmulh(vf, rd, rn, rm);
        break;
      case NEON_UQSHL:
        ushl(vf, rd, rn, rm).UnsignedSaturate(vf);
        break;
      case NEON_SQSHL:
        sshl(vf, rd, rn, rm).SignedSaturate(vf);
        break;
      case NEON_URSHL:
        ushl(vf, rd, rn, rm).Round(vf);
        break;
      case NEON_SRSHL:
        sshl(vf, rd, rn, rm).Round(vf);
        break;
      case NEON_UQRSHL:
        ushl(vf, rd, rn, rm).Round(vf).UnsignedSaturate(vf);
        break;
      case NEON_SQRSHL:
        sshl(vf, rd, rn, rm).Round(vf).SignedSaturate(vf);
        break;
      case NEON_UHADD:
        add(vf, rd, rn, rm).Uhalve(vf);
        break;
      case NEON_URHADD:
        add(vf, rd, rn, rm).Uhalve(vf).Round(vf);
        break;
      case NEON_SHADD:
        add(vf, rd, rn, rm).Halve(vf);
        break;
      case NEON_SRHADD:
        add(vf, rd, rn, rm).Halve(vf).Round(vf);
        break;
      case NEON_UHSUB:
        sub(vf, rd, rn, rm).Uhalve(vf);
        break;
      case NEON_SHSUB:
        sub(vf, rd, rn, rm).Halve(vf);
        break;
      default:
        VIXL_UNIMPLEMENTED();
    }
  }
}


void Simulator::VisitNEON3SameFP16(const Instruction* instr) {
  NEONFormatDecoder nfd(instr);
  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());
  SimVRegister& rm = ReadVRegister(instr->GetRm());

  VectorFormat vf = nfd.GetVectorFormat(nfd.FP16FormatMap());
  switch (instr->Mask(NEON3SameFP16Mask)) {
#define SIM_FUNC(A, B) \
  case NEON_##A##_H:   \
    B(vf, rd, rn, rm); \
    break;
    SIM_FUNC(FMAXNM, fmaxnm);
    SIM_FUNC(FADD, fadd);
    SIM_FUNC(FMULX, fmulx);
    SIM_FUNC(FMAX, fmax);
    SIM_FUNC(FRECPS, frecps);
    SIM_FUNC(FMINNM, fminnm);
    SIM_FUNC(FSUB, fsub);
    SIM_FUNC(FMIN, fmin);
    SIM_FUNC(FRSQRTS, frsqrts);
    SIM_FUNC(FMAXNMP, fmaxnmp);
    SIM_FUNC(FADDP, faddp);
    SIM_FUNC(FMUL, fmul);
    SIM_FUNC(FMAXP, fmaxp);
    SIM_FUNC(FDIV, fdiv);
    SIM_FUNC(FMINNMP, fminnmp);
    SIM_FUNC(FABD, fabd);
    SIM_FUNC(FMINP, fminp);
#undef SIM_FUNC
    case NEON_FMLA_H:
      fmla(vf, rd, rd, rn, rm);
      break;
    case NEON_FMLS_H:
      fmls(vf, rd, rd, rn, rm);
      break;
    case NEON_FCMEQ_H:
      fcmp(vf, rd, rn, rm, eq);
      break;
    case NEON_FCMGE_H:
      fcmp(vf, rd, rn, rm, ge);
      break;
    case NEON_FACGE_H:
      fabscmp(vf, rd, rn, rm, ge);
      break;
    case NEON_FCMGT_H:
      fcmp(vf, rd, rn, rm, gt);
      break;
    case NEON_FACGT_H:
      fabscmp(vf, rd, rn, rm, gt);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitNEON3SameExtra(const Instruction* instr) {
  NEONFormatDecoder nfd(instr);
  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());
  SimVRegister& rm = ReadVRegister(instr->GetRm());
  int rot = 0;
  VectorFormat vf = nfd.GetVectorFormat();

  switch (form_hash_) {
    case "fcmla_asimdsame2_c"_h:
      rot = instr->GetImmRotFcmlaVec();
      fcmla(vf, rd, rn, rm, rd, rot);
      break;
    case "fcadd_asimdsame2_c"_h:
      rot = instr->GetImmRotFcadd();
      fcadd(vf, rd, rn, rm, rot);
      break;
    case "sdot_asimdsame2_d"_h:
      sdot(vf, rd, rn, rm);
      break;
    case "udot_asimdsame2_d"_h:
      udot(vf, rd, rn, rm);
      break;
    case "usdot_asimdsame2_d"_h:
      usdot(vf, rd, rn, rm);
      break;
    case "sqrdmlah_asimdsame2_only"_h:
      sqrdmlah(vf, rd, rn, rm);
      break;
    case "sqrdmlsh_asimdsame2_only"_h:
      sqrdmlsh(vf, rd, rn, rm);
      break;
  }
}


void Simulator::VisitNEON3Different(const Instruction* instr) {
  NEONFormatDecoder nfd(instr);
  VectorFormat vf = nfd.GetVectorFormat();
  VectorFormat vf_l = nfd.GetVectorFormat(nfd.LongIntegerFormatMap());

  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());
  SimVRegister& rm = ReadVRegister(instr->GetRm());
  int size = instr->GetNEONSize();

  switch (instr->Mask(NEON3DifferentMask)) {
    case NEON_PMULL:
      if (size == 3) vf_l = kFormat1Q;
      pmull(vf_l, rd, rn, rm);
      break;
    case NEON_PMULL2:
      if (size == 3) vf_l = kFormat1Q;
      pmull2(vf_l, rd, rn, rm);
      break;
    case NEON_UADDL:
      uaddl(vf_l, rd, rn, rm);
      break;
    case NEON_UADDL2:
      uaddl2(vf_l, rd, rn, rm);
      break;
    case NEON_SADDL:
      saddl(vf_l, rd, rn, rm);
      break;
    case NEON_SADDL2:
      saddl2(vf_l, rd, rn, rm);
      break;
    case NEON_USUBL:
      usubl(vf_l, rd, rn, rm);
      break;
    case NEON_USUBL2:
      usubl2(vf_l, rd, rn, rm);
      break;
    case NEON_SSUBL:
      ssubl(vf_l, rd, rn, rm);
      break;
    case NEON_SSUBL2:
      ssubl2(vf_l, rd, rn, rm);
      break;
    case NEON_SABAL:
      sabal(vf_l, rd, rn, rm);
      break;
    case NEON_SABAL2:
      sabal2(vf_l, rd, rn, rm);
      break;
    case NEON_UABAL:
      uabal(vf_l, rd, rn, rm);
      break;
    case NEON_UABAL2:
      uabal2(vf_l, rd, rn, rm);
      break;
    case NEON_SABDL:
      sabdl(vf_l, rd, rn, rm);
      break;
    case NEON_SABDL2:
      sabdl2(vf_l, rd, rn, rm);
      break;
    case NEON_UABDL:
      uabdl(vf_l, rd, rn, rm);
      break;
    case NEON_UABDL2:
      uabdl2(vf_l, rd, rn, rm);
      break;
    case NEON_SMLAL:
      smlal(vf_l, rd, rn, rm);
      break;
    case NEON_SMLAL2:
      smlal2(vf_l, rd, rn, rm);
      break;
    case NEON_UMLAL:
      umlal(vf_l, rd, rn, rm);
      break;
    case NEON_UMLAL2:
      umlal2(vf_l, rd, rn, rm);
      break;
    case NEON_SMLSL:
      smlsl(vf_l, rd, rn, rm);
      break;
    case NEON_SMLSL2:
      smlsl2(vf_l, rd, rn, rm);
      break;
    case NEON_UMLSL:
      umlsl(vf_l, rd, rn, rm);
      break;
    case NEON_UMLSL2:
      umlsl2(vf_l, rd, rn, rm);
      break;
    case NEON_SMULL:
      smull(vf_l, rd, rn, rm);
      break;
    case NEON_SMULL2:
      smull2(vf_l, rd, rn, rm);
      break;
    case NEON_UMULL:
      umull(vf_l, rd, rn, rm);
      break;
    case NEON_UMULL2:
      umull2(vf_l, rd, rn, rm);
      break;
    case NEON_SQDMLAL:
      sqdmlal(vf_l, rd, rn, rm);
      break;
    case NEON_SQDMLAL2:
      sqdmlal2(vf_l, rd, rn, rm);
      break;
    case NEON_SQDMLSL:
      sqdmlsl(vf_l, rd, rn, rm);
      break;
    case NEON_SQDMLSL2:
      sqdmlsl2(vf_l, rd, rn, rm);
      break;
    case NEON_SQDMULL:
      sqdmull(vf_l, rd, rn, rm);
      break;
    case NEON_SQDMULL2:
      sqdmull2(vf_l, rd, rn, rm);
      break;
    case NEON_UADDW:
      uaddw(vf_l, rd, rn, rm);
      break;
    case NEON_UADDW2:
      uaddw2(vf_l, rd, rn, rm);
      break;
    case NEON_SADDW:
      saddw(vf_l, rd, rn, rm);
      break;
    case NEON_SADDW2:
      saddw2(vf_l, rd, rn, rm);
      break;
    case NEON_USUBW:
      usubw(vf_l, rd, rn, rm);
      break;
    case NEON_USUBW2:
      usubw2(vf_l, rd, rn, rm);
      break;
    case NEON_SSUBW:
      ssubw(vf_l, rd, rn, rm);
      break;
    case NEON_SSUBW2:
      ssubw2(vf_l, rd, rn, rm);
      break;
    case NEON_ADDHN:
      addhn(vf, rd, rn, rm);
      break;
    case NEON_ADDHN2:
      addhn2(vf, rd, rn, rm);
      break;
    case NEON_RADDHN:
      raddhn(vf, rd, rn, rm);
      break;
    case NEON_RADDHN2:
      raddhn2(vf, rd, rn, rm);
      break;
    case NEON_SUBHN:
      subhn(vf, rd, rn, rm);
      break;
    case NEON_SUBHN2:
      subhn2(vf, rd, rn, rm);
      break;
    case NEON_RSUBHN:
      rsubhn(vf, rd, rn, rm);
      break;
    case NEON_RSUBHN2:
      rsubhn2(vf, rd, rn, rm);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
}


void Simulator::VisitNEONAcrossLanes(const Instruction* instr) {
  NEONFormatDecoder nfd(instr);

  static const NEONFormatMap map_half = {{30}, {NF_4H, NF_8H}};

  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());

  if (instr->Mask(NEONAcrossLanesFP16FMask) == NEONAcrossLanesFP16Fixed) {
    VectorFormat vf = nfd.GetVectorFormat(&map_half);
    switch (instr->Mask(NEONAcrossLanesFP16Mask)) {
      case NEON_FMAXV_H:
        fmaxv(vf, rd, rn);
        break;
      case NEON_FMINV_H:
        fminv(vf, rd, rn);
        break;
      case NEON_FMAXNMV_H:
        fmaxnmv(vf, rd, rn);
        break;
      case NEON_FMINNMV_H:
        fminnmv(vf, rd, rn);
        break;
      default:
        VIXL_UNIMPLEMENTED();
    }
  } else if (instr->Mask(NEONAcrossLanesFPFMask) == NEONAcrossLanesFPFixed) {
    // The input operand's VectorFormat is passed for these instructions.
    VectorFormat vf = nfd.GetVectorFormat(nfd.FPFormatMap());

    switch (instr->Mask(NEONAcrossLanesFPMask)) {
      case NEON_FMAXV:
        fmaxv(vf, rd, rn);
        break;
      case NEON_FMINV:
        fminv(vf, rd, rn);
        break;
      case NEON_FMAXNMV:
        fmaxnmv(vf, rd, rn);
        break;
      case NEON_FMINNMV:
        fminnmv(vf, rd, rn);
        break;
      default:
        VIXL_UNIMPLEMENTED();
    }
  } else {
    VectorFormat vf = nfd.GetVectorFormat();

    switch (instr->Mask(NEONAcrossLanesMask)) {
      case NEON_ADDV:
        addv(vf, rd, rn);
        break;
      case NEON_SMAXV:
        smaxv(vf, rd, rn);
        break;
      case NEON_SMINV:
        sminv(vf, rd, rn);
        break;
      case NEON_UMAXV:
        umaxv(vf, rd, rn);
        break;
      case NEON_UMINV:
        uminv(vf, rd, rn);
        break;
      case NEON_SADDLV:
        saddlv(vf, rd, rn);
        break;
      case NEON_UADDLV:
        uaddlv(vf, rd, rn);
        break;
      default:
        VIXL_UNIMPLEMENTED();
    }
  }
}

void Simulator::SimulateNEONMulByElementLong(const Instruction* instr) {
  NEONFormatDecoder nfd(instr);
  VectorFormat vf = nfd.GetVectorFormat(nfd.LongIntegerFormatMap());
  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());

  std::pair<int, int> rm_and_index = instr->GetNEONMulRmAndIndex();
  SimVRegister temp;
  VectorFormat indexform =
      VectorFormatHalfWidthDoubleLanes(VectorFormatFillQ(vf));
  dup_elements_to_segments(indexform, temp, rm_and_index);

  bool is_2 = instr->Mask(NEON_Q) ? true : false;

  switch (form_hash_) {
    case "smull_asimdelem_l"_h:
      smull(vf, rd, rn, temp, is_2);
      break;
    case "umull_asimdelem_l"_h:
      umull(vf, rd, rn, temp, is_2);
      break;
    case "smlal_asimdelem_l"_h:
      smlal(vf, rd, rn, temp, is_2);
      break;
    case "umlal_asimdelem_l"_h:
      umlal(vf, rd, rn, temp, is_2);
      break;
    case "smlsl_asimdelem_l"_h:
      smlsl(vf, rd, rn, temp, is_2);
      break;
    case "umlsl_asimdelem_l"_h:
      umlsl(vf, rd, rn, temp, is_2);
      break;
    case "sqdmull_asimdelem_l"_h:
      sqdmull(vf, rd, rn, temp, is_2);
      break;
    case "sqdmlal_asimdelem_l"_h:
      sqdmlal(vf, rd, rn, temp, is_2);
      break;
    case "sqdmlsl_asimdelem_l"_h:
      sqdmlsl(vf, rd, rn, temp, is_2);
      break;
    default:
      VIXL_UNREACHABLE();
  }
}

void Simulator::SimulateNEONFPMulByElementLong(const Instruction* instr) {
  VectorFormat vform = instr->GetNEONQ() ? kFormat4S : kFormat2S;
  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());
  SimVRegister& rm = ReadVRegister(instr->GetRmLow16());

  int index =
      (instr->GetNEONH() << 2) | (instr->GetNEONL() << 1) | instr->GetNEONM();

  switch (form_hash_) {
    case "fmlal_asimdelem_lh"_h:
      fmlal(vform, rd, rn, rm, index);
      break;
    case "fmlal2_asimdelem_lh"_h:
      fmlal2(vform, rd, rn, rm, index);
      break;
    case "fmlsl_asimdelem_lh"_h:
      fmlsl(vform, rd, rn, rm, index);
      break;
    case "fmlsl2_asimdelem_lh"_h:
      fmlsl2(vform, rd, rn, rm, index);
      break;
    default:
      VIXL_UNREACHABLE();
  }
}

void Simulator::SimulateNEONFPMulByElement(const Instruction* instr) {
  NEONFormatDecoder nfd(instr);
  static const NEONFormatMap map =
      {{23, 22, 30},
       {NF_4H, NF_8H, NF_UNDEF, NF_UNDEF, NF_2S, NF_4S, NF_UNDEF, NF_2D}};
  VectorFormat vform = nfd.GetVectorFormat(&map);

  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());

  std::pair<int, int> rm_and_index = instr->GetNEONMulRmAndIndex();
  SimVRegister& rm = ReadVRegister(rm_and_index.first);
  int index = rm_and_index.second;

  switch (form_hash_) {
    case "fmul_asimdelem_rh_h"_h:
    case "fmul_asimdelem_r_sd"_h:
      fmul(vform, rd, rn, rm, index);
      break;
    case "fmla_asimdelem_rh_h"_h:
    case "fmla_asimdelem_r_sd"_h:
      fmla(vform, rd, rn, rm, index);
      break;
    case "fmls_asimdelem_rh_h"_h:
    case "fmls_asimdelem_r_sd"_h:
      fmls(vform, rd, rn, rm, index);
      break;
    case "fmulx_asimdelem_rh_h"_h:
    case "fmulx_asimdelem_r_sd"_h:
      fmulx(vform, rd, rn, rm, index);
      break;
    default:
      VIXL_UNREACHABLE();
  }
}

void Simulator::SimulateNEONComplexMulByElement(const Instruction* instr) {
  VectorFormat vform = instr->GetNEONQ() ? kFormat8H : kFormat4H;
  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());
  SimVRegister& rm = ReadVRegister(instr->GetRm());
  int index = (instr->GetNEONH() << 1) | instr->GetNEONL();

  switch (form_hash_) {
    case "fcmla_asimdelem_c_s"_h:
      vform = kFormat4S;
      index >>= 1;
      VIXL_FALLTHROUGH();
    case "fcmla_asimdelem_c_h"_h:
      fcmla(vform, rd, rn, rm, index, instr->GetImmRotFcmlaSca());
      break;
    default:
      VIXL_UNREACHABLE();
  }
}

void Simulator::SimulateNEONDotProdByElement(const Instruction* instr) {
  VectorFormat vform = instr->GetNEONQ() ? kFormat4S : kFormat2S;

  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());
  SimVRegister& rm = ReadVRegister(instr->GetRm());
  int index = (instr->GetNEONH() << 1) | instr->GetNEONL();

  SimVRegister temp;
  // NEON indexed `dot` allows the index value exceed the register size.
  // Promote the format to Q-sized vector format before the duplication.
  dup_elements_to_segments(VectorFormatFillQ(vform), temp, rm, index);

  switch (form_hash_) {
    case "sdot_asimdelem_d"_h:
      sdot(vform, rd, rn, temp);
      break;
    case "udot_asimdelem_d"_h:
      udot(vform, rd, rn, temp);
      break;
    case "sudot_asimdelem_d"_h:
      usdot(vform, rd, temp, rn);
      break;
    case "usdot_asimdelem_d"_h:
      usdot(vform, rd, rn, temp);
      break;
  }
}

void Simulator::VisitNEONByIndexedElement(const Instruction* instr) {
  NEONFormatDecoder nfd(instr);
  VectorFormat vform = nfd.GetVectorFormat();

  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());

  std::pair<int, int> rm_and_index = instr->GetNEONMulRmAndIndex();
  SimVRegister& rm = ReadVRegister(rm_and_index.first);
  int index = rm_and_index.second;

  switch (form_hash_) {
    case "mul_asimdelem_r"_h:
      mul(vform, rd, rn, rm, index);
      break;
    case "mla_asimdelem_r"_h:
      mla(vform, rd, rn, rm, index);
      break;
    case "mls_asimdelem_r"_h:
      mls(vform, rd, rn, rm, index);
      break;
    case "sqdmulh_asimdelem_r"_h:
      sqdmulh(vform, rd, rn, rm, index);
      break;
    case "sqrdmulh_asimdelem_r"_h:
      sqrdmulh(vform, rd, rn, rm, index);
      break;
    case "sqrdmlah_asimdelem_r"_h:
      sqrdmlah(vform, rd, rn, rm, index);
      break;
    case "sqrdmlsh_asimdelem_r"_h:
      sqrdmlsh(vform, rd, rn, rm, index);
      break;
  }
}


void Simulator::VisitNEONCopy(const Instruction* instr) {
  NEONFormatDecoder nfd(instr, NEONFormatDecoder::TriangularFormatMap());
  VectorFormat vf = nfd.GetVectorFormat();

  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());
  int imm5 = instr->GetImmNEON5();
  int tz = CountTrailingZeros(imm5, 32);
  int reg_index = ExtractSignedBitfield32(31, tz + 1, imm5);

  if (instr->Mask(NEONCopyInsElementMask) == NEON_INS_ELEMENT) {
    int imm4 = instr->GetImmNEON4();
    int rn_index = ExtractSignedBitfield32(31, tz, imm4);
    mov(kFormat16B, rd, rd);  // Zero bits beyond the MSB of a Q register.
    ins_element(vf, rd, reg_index, rn, rn_index);
  } else if (instr->Mask(NEONCopyInsGeneralMask) == NEON_INS_GENERAL) {
    mov(kFormat16B, rd, rd);  // Zero bits beyond the MSB of a Q register.
    ins_immediate(vf, rd, reg_index, ReadXRegister(instr->GetRn()));
  } else if (instr->Mask(NEONCopyUmovMask) == NEON_UMOV) {
    uint64_t value = LogicVRegister(rn).Uint(vf, reg_index);
    value &= MaxUintFromFormat(vf);
    WriteXRegister(instr->GetRd(), value);
  } else if (instr->Mask(NEONCopyUmovMask) == NEON_SMOV) {
    int64_t value = LogicVRegister(rn).Int(vf, reg_index);
    if (instr->GetNEONQ()) {
      WriteXRegister(instr->GetRd(), value);
    } else {
      WriteWRegister(instr->GetRd(), (int32_t)value);
    }
  } else if (instr->Mask(NEONCopyDupElementMask) == NEON_DUP_ELEMENT) {
    dup_element(vf, rd, rn, reg_index);
  } else if (instr->Mask(NEONCopyDupGeneralMask) == NEON_DUP_GENERAL) {
    dup_immediate(vf, rd, ReadXRegister(instr->GetRn()));
  } else {
    VIXL_UNIMPLEMENTED();
  }
}


void Simulator::VisitNEONExtract(const Instruction* instr) {
  NEONFormatDecoder nfd(instr, NEONFormatDecoder::LogicalFormatMap());
  VectorFormat vf = nfd.GetVectorFormat();
  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());
  SimVRegister& rm = ReadVRegister(instr->GetRm());
  if (instr->Mask(NEONExtractMask) == NEON_EXT) {
    int index = instr->GetImmNEONExt();
    ext(vf, rd, rn, rm, index);
  } else {
    VIXL_UNIMPLEMENTED();
  }
}


void Simulator::NEONLoadStoreMultiStructHelper(const Instruction* instr,
                                               AddrMode addr_mode) {
  NEONFormatDecoder nfd(instr, NEONFormatDecoder::LoadStoreFormatMap());
  VectorFormat vf = nfd.GetVectorFormat();

  uint64_t addr_base = ReadXRegister(instr->GetRn(), Reg31IsStackPointer);
  int reg_size = RegisterSizeInBytesFromFormat(vf);

  int reg[4];
  uint64_t addr[4];
  for (int i = 0; i < 4; i++) {
    reg[i] = (instr->GetRt() + i) % kNumberOfVRegisters;
    addr[i] = addr_base + (i * reg_size);
  }
  int struct_parts = 1;
  int reg_count = 1;
  bool log_read = true;

  // Bit 23 determines whether this is an offset or post-index addressing mode.
  // In offset mode, bits 20 to 16 should be zero; these bits encode the
  // register or immediate in post-index mode.
  if ((instr->ExtractBit(23) == 0) && (instr->ExtractBits(20, 16) != 0)) {
    VIXL_UNREACHABLE();
  }

  // We use the PostIndex mask here, as it works in this case for both Offset
  // and PostIndex addressing.
  switch (instr->Mask(NEONLoadStoreMultiStructPostIndexMask)) {
    case NEON_LD1_4v:
    case NEON_LD1_4v_post:
      if (!ld1(vf, ReadVRegister(reg[3]), addr[3])) {
        return;
      }
      reg_count++;
      VIXL_FALLTHROUGH();
    case NEON_LD1_3v:
    case NEON_LD1_3v_post:
      if (!ld1(vf, ReadVRegister(reg[2]), addr[2])) {
        return;
      }
      reg_count++;
      VIXL_FALLTHROUGH();
    case NEON_LD1_2v:
    case NEON_LD1_2v_post:
      if (!ld1(vf, ReadVRegister(reg[1]), addr[1])) {
        return;
      }
      reg_count++;
      VIXL_FALLTHROUGH();
    case NEON_LD1_1v:
    case NEON_LD1_1v_post:
      if (!ld1(vf, ReadVRegister(reg[0]), addr[0])) {
        return;
      }
      break;
    case NEON_ST1_4v:
    case NEON_ST1_4v_post:
      if (!st1(vf, ReadVRegister(reg[3]), addr[3])) return;
      reg_count++;
      VIXL_FALLTHROUGH();
    case NEON_ST1_3v:
    case NEON_ST1_3v_post:
      if (!st1(vf, ReadVRegister(reg[2]), addr[2])) return;
      reg_count++;
      VIXL_FALLTHROUGH();
    case NEON_ST1_2v:
    case NEON_ST1_2v_post:
      if (!st1(vf, ReadVRegister(reg[1]), addr[1])) return;
      reg_count++;
      VIXL_FALLTHROUGH();
    case NEON_ST1_1v:
    case NEON_ST1_1v_post:
      if (!st1(vf, ReadVRegister(reg[0]), addr[0])) return;
      log_read = false;
      break;
    case NEON_LD2_post:
    case NEON_LD2:
      if (!ld2(vf, ReadVRegister(reg[0]), ReadVRegister(reg[1]), addr[0])) {
        return;
      }
      struct_parts = 2;
      reg_count = 2;
      break;
    case NEON_ST2:
    case NEON_ST2_post:
      if (!st2(vf, ReadVRegister(reg[0]), ReadVRegister(reg[1]), addr[0])) {
        return;
      }
      struct_parts = 2;
      reg_count = 2;
      log_read = false;
      break;
    case NEON_LD3_post:
    case NEON_LD3:
      if (!ld3(vf,
               ReadVRegister(reg[0]),
               ReadVRegister(reg[1]),
               ReadVRegister(reg[2]),
               addr[0])) {
        return;
      }
      struct_parts = 3;
      reg_count = 3;
      break;
    case NEON_ST3:
    case NEON_ST3_post:
      if (!st3(vf,
               ReadVRegister(reg[0]),
               ReadVRegister(reg[1]),
               ReadVRegister(reg[2]),
               addr[0])) {
        return;
      }
      struct_parts = 3;
      reg_count = 3;
      log_read = false;
      break;
    case NEON_ST4:
    case NEON_ST4_post:
      if (!st4(vf,
               ReadVRegister(reg[0]),
               ReadVRegister(reg[1]),
               ReadVRegister(reg[2]),
               ReadVRegister(reg[3]),
               addr[0])) {
        return;
      }
      struct_parts = 4;
      reg_count = 4;
      log_read = false;
      break;
    case NEON_LD4_post:
    case NEON_LD4:
      if (!ld4(vf,
               ReadVRegister(reg[0]),
               ReadVRegister(reg[1]),
               ReadVRegister(reg[2]),
               ReadVRegister(reg[3]),
               addr[0])) {
        return;
      }
      struct_parts = 4;
      reg_count = 4;
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }

  bool do_trace = log_read ? ShouldTraceVRegs() : ShouldTraceWrites();
  if (do_trace) {
    PrintRegisterFormat print_format =
        GetPrintRegisterFormatTryFP(GetPrintRegisterFormat(vf));
    const char* op;
    if (log_read) {
      op = "<-";
    } else {
      op = "->";
      // Stores don't represent a change to the source register's value, so only
      // print the relevant part of the value.
      print_format = GetPrintRegPartial(print_format);
    }

    VIXL_ASSERT((struct_parts == reg_count) || (struct_parts == 1));
    for (int s = reg_count - struct_parts; s >= 0; s -= struct_parts) {
      uintptr_t address = addr_base + (s * RegisterSizeInBytesFromFormat(vf));
      PrintVStructAccess(reg[s], struct_parts, print_format, op, address);
    }
  }

  if (addr_mode == PostIndex) {
    int rm = instr->GetRm();
    // The immediate post index addressing mode is indicated by rm = 31.
    // The immediate is implied by the number of vector registers used.
    addr_base += (rm == 31) ? (RegisterSizeInBytesFromFormat(vf) * reg_count)
                            : ReadXRegister(rm);
    WriteXRegister(instr->GetRn(),
                   addr_base,
                   LogRegWrites,
                   Reg31IsStackPointer);
  } else {
    VIXL_ASSERT(addr_mode == Offset);
  }
}


void Simulator::VisitNEONLoadStoreMultiStruct(const Instruction* instr) {
  NEONLoadStoreMultiStructHelper(instr, Offset);
}


void Simulator::VisitNEONLoadStoreMultiStructPostIndex(
    const Instruction* instr) {
  NEONLoadStoreMultiStructHelper(instr, PostIndex);
}


void Simulator::NEONLoadStoreSingleStructHelper(const Instruction* instr,
                                                AddrMode addr_mode) {
  uint64_t addr = ReadXRegister(instr->GetRn(), Reg31IsStackPointer);
  int rt = instr->GetRt();

  // Bit 23 determines whether this is an offset or post-index addressing mode.
  // In offset mode, bits 20 to 16 should be zero; these bits encode the
  // register or immediate in post-index mode.
  if ((instr->ExtractBit(23) == 0) && (instr->ExtractBits(20, 16) != 0)) {
    VIXL_UNREACHABLE();
  }

  // We use the PostIndex mask here, as it works in this case for both Offset
  // and PostIndex addressing.
  bool do_load = false;

  bool replicating = false;

  NEONFormatDecoder nfd(instr, NEONFormatDecoder::LoadStoreFormatMap());
  VectorFormat vf_t = nfd.GetVectorFormat();

  VectorFormat vf = kFormat16B;
  switch (instr->Mask(NEONLoadStoreSingleStructPostIndexMask)) {
    case NEON_LD1_b:
    case NEON_LD1_b_post:
    case NEON_LD2_b:
    case NEON_LD2_b_post:
    case NEON_LD3_b:
    case NEON_LD3_b_post:
    case NEON_LD4_b:
    case NEON_LD4_b_post:
      do_load = true;
      VIXL_FALLTHROUGH();
    case NEON_ST1_b:
    case NEON_ST1_b_post:
    case NEON_ST2_b:
    case NEON_ST2_b_post:
    case NEON_ST3_b:
    case NEON_ST3_b_post:
    case NEON_ST4_b:
    case NEON_ST4_b_post:
      break;

    case NEON_LD1_h:
    case NEON_LD1_h_post:
    case NEON_LD2_h:
    case NEON_LD2_h_post:
    case NEON_LD3_h:
    case NEON_LD3_h_post:
    case NEON_LD4_h:
    case NEON_LD4_h_post:
      do_load = true;
      VIXL_FALLTHROUGH();
    case NEON_ST1_h:
    case NEON_ST1_h_post:
    case NEON_ST2_h:
    case NEON_ST2_h_post:
    case NEON_ST3_h:
    case NEON_ST3_h_post:
    case NEON_ST4_h:
    case NEON_ST4_h_post:
      vf = kFormat8H;
      break;
    case NEON_LD1_s:
    case NEON_LD1_s_post:
    case NEON_LD2_s:
    case NEON_LD2_s_post:
    case NEON_LD3_s:
    case NEON_LD3_s_post:
    case NEON_LD4_s:
    case NEON_LD4_s_post:
      do_load = true;
      VIXL_FALLTHROUGH();
    case NEON_ST1_s:
    case NEON_ST1_s_post:
    case NEON_ST2_s:
    case NEON_ST2_s_post:
    case NEON_ST3_s:
    case NEON_ST3_s_post:
    case NEON_ST4_s:
    case NEON_ST4_s_post: {
      VIXL_STATIC_ASSERT((NEON_LD1_s | (1 << NEONLSSize_offset)) == NEON_LD1_d);
      VIXL_STATIC_ASSERT((NEON_LD1_s_post | (1 << NEONLSSize_offset)) ==
                         NEON_LD1_d_post);
      VIXL_STATIC_ASSERT((NEON_ST1_s | (1 << NEONLSSize_offset)) == NEON_ST1_d);
      VIXL_STATIC_ASSERT((NEON_ST1_s_post | (1 << NEONLSSize_offset)) ==
                         NEON_ST1_d_post);
      vf = ((instr->GetNEONLSSize() & 1) == 0) ? kFormat4S : kFormat2D;
      break;
    }

    case NEON_LD1R:
    case NEON_LD1R_post:
    case NEON_LD2R:
    case NEON_LD2R_post:
    case NEON_LD3R:
    case NEON_LD3R_post:
    case NEON_LD4R:
    case NEON_LD4R_post:
      vf = vf_t;
      do_load = true;
      replicating = true;
      break;

    default:
      VIXL_UNIMPLEMENTED();
  }

  int index_shift = LaneSizeInBytesLog2FromFormat(vf);
  int lane = instr->GetNEONLSIndex(index_shift);
  int reg_count = 0;
  int rt2 = (rt + 1) % kNumberOfVRegisters;
  int rt3 = (rt2 + 1) % kNumberOfVRegisters;
  int rt4 = (rt3 + 1) % kNumberOfVRegisters;
  switch (instr->Mask(NEONLoadStoreSingleLenMask)) {
    case NEONLoadStoreSingle1:
      reg_count = 1;
      if (replicating) {
        VIXL_ASSERT(do_load);
        if (!ld1r(vf, ReadVRegister(rt), addr)) {
          return;
        }
      } else if (do_load) {
        if (!ld1(vf, ReadVRegister(rt), lane, addr)) {
          return;
        }
      } else {
        if (!st1(vf, ReadVRegister(rt), lane, addr)) return;
      }
      break;
    case NEONLoadStoreSingle2:
      reg_count = 2;
      if (replicating) {
        VIXL_ASSERT(do_load);
        if (!ld2r(vf, ReadVRegister(rt), ReadVRegister(rt2), addr)) {
          return;
        }
      } else if (do_load) {
        if (!ld2(vf, ReadVRegister(rt), ReadVRegister(rt2), lane, addr)) {
          return;
        }
      } else {
        if (!st2(vf, ReadVRegister(rt), ReadVRegister(rt2), lane, addr)) return;
      }
      break;
    case NEONLoadStoreSingle3:
      reg_count = 3;
      if (replicating) {
        VIXL_ASSERT(do_load);
        if (!ld3r(vf,
                  ReadVRegister(rt),
                  ReadVRegister(rt2),
                  ReadVRegister(rt3),
                  addr)) {
          return;
        }
      } else if (do_load) {
        if (!ld3(vf,
                 ReadVRegister(rt),
                 ReadVRegister(rt2),
                 ReadVRegister(rt3),
                 lane,
                 addr)) {
          return;
        }
      } else {
        if (!st3(vf,
                 ReadVRegister(rt),
                 ReadVRegister(rt2),
                 ReadVRegister(rt3),
                 lane,
                 addr)) {
          return;
        }
      }
      break;
    case NEONLoadStoreSingle4:
      reg_count = 4;
      if (replicating) {
        VIXL_ASSERT(do_load);
        if (!ld4r(vf,
                  ReadVRegister(rt),
                  ReadVRegister(rt2),
                  ReadVRegister(rt3),
                  ReadVRegister(rt4),
                  addr)) {
          return;
        }
      } else if (do_load) {
        if (!ld4(vf,
                 ReadVRegister(rt),
                 ReadVRegister(rt2),
                 ReadVRegister(rt3),
                 ReadVRegister(rt4),
                 lane,
                 addr)) {
          return;
        }
      } else {
        if (!st4(vf,
                 ReadVRegister(rt),
                 ReadVRegister(rt2),
                 ReadVRegister(rt3),
                 ReadVRegister(rt4),
                 lane,
                 addr)) {
          return;
        }
      }
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }

  // Trace registers and/or memory writes.
  PrintRegisterFormat print_format =
      GetPrintRegisterFormatTryFP(GetPrintRegisterFormat(vf));
  if (do_load) {
    if (ShouldTraceVRegs()) {
      if (replicating) {
        PrintVReplicatingStructAccess(rt, reg_count, print_format, "<-", addr);
      } else {
        PrintVSingleStructAccess(rt, reg_count, lane, print_format, "<-", addr);
      }
    }
  } else {
    if (ShouldTraceWrites()) {
      // Stores don't represent a change to the source register's value, so only
      // print the relevant part of the value.
      print_format = GetPrintRegPartial(print_format);
      PrintVSingleStructAccess(rt, reg_count, lane, print_format, "->", addr);
    }
  }

  if (addr_mode == PostIndex) {
    int rm = instr->GetRm();
    int lane_size = LaneSizeInBytesFromFormat(vf);
    WriteXRegister(instr->GetRn(),
                   addr + ((rm == 31) ? (reg_count * lane_size)
                                      : ReadXRegister(rm)),
                   LogRegWrites,
                   Reg31IsStackPointer);
  }
}


void Simulator::VisitNEONLoadStoreSingleStruct(const Instruction* instr) {
  NEONLoadStoreSingleStructHelper(instr, Offset);
}


void Simulator::VisitNEONLoadStoreSingleStructPostIndex(
    const Instruction* instr) {
  NEONLoadStoreSingleStructHelper(instr, PostIndex);
}


void Simulator::VisitNEONModifiedImmediate(const Instruction* instr) {
  SimVRegister& rd = ReadVRegister(instr->GetRd());
  int cmode = instr->GetNEONCmode();
  int cmode_3_1 = (cmode >> 1) & 7;
  int cmode_3 = (cmode >> 3) & 1;
  int cmode_2 = (cmode >> 2) & 1;
  int cmode_1 = (cmode >> 1) & 1;
  int cmode_0 = cmode & 1;
  int half_enc = instr->ExtractBit(11);
  int q = instr->GetNEONQ();
  int op_bit = instr->GetNEONModImmOp();
  uint64_t imm8 = instr->GetImmNEONabcdefgh();
  // Find the format and immediate value
  uint64_t imm = 0;
  VectorFormat vform = kFormatUndefined;
  switch (cmode_3_1) {
    case 0x0:
    case 0x1:
    case 0x2:
    case 0x3:
      vform = (q == 1) ? kFormat4S : kFormat2S;
      imm = imm8 << (8 * cmode_3_1);
      break;
    case 0x4:
    case 0x5:
      vform = (q == 1) ? kFormat8H : kFormat4H;
      imm = imm8 << (8 * cmode_1);
      break;
    case 0x6:
      vform = (q == 1) ? kFormat4S : kFormat2S;
      if (cmode_0 == 0) {
        imm = imm8 << 8 | 0x000000ff;
      } else {
        imm = imm8 << 16 | 0x0000ffff;
      }
      break;
    case 0x7:
      if (cmode_0 == 0 && op_bit == 0) {
        vform = q ? kFormat16B : kFormat8B;
        imm = imm8;
      } else if (cmode_0 == 0 && op_bit == 1) {
        vform = q ? kFormat2D : kFormat1D;
        imm = 0;
        for (int i = 0; i < 8; ++i) {
          if (imm8 & (uint64_t{1} << i)) {
            imm |= (UINT64_C(0xff) << (8 * i));
          }
        }
      } else {  // cmode_0 == 1, cmode == 0xf.
        if (half_enc == 1) {
          vform = q ? kFormat8H : kFormat4H;
          imm = Float16ToRawbits(instr->GetImmNEONFP16());
        } else if (op_bit == 0) {
          vform = q ? kFormat4S : kFormat2S;
          imm = FloatToRawbits(instr->GetImmNEONFP32());
        } else if (q == 1) {
          vform = kFormat2D;
          imm = DoubleToRawbits(instr->GetImmNEONFP64());
        }
      }
      break;
    default:
      VIXL_UNREACHABLE();
      break;
  }

  // Find the operation
  NEONModifiedImmediateOp op;
  if (cmode_3 == 0) {
    if (cmode_0 == 0) {
      op = op_bit ? NEONModifiedImmediate_MVNI : NEONModifiedImmediate_MOVI;
    } else {  // cmode<0> == '1'
      op = op_bit ? NEONModifiedImmediate_BIC : NEONModifiedImmediate_ORR;
    }
  } else {  // cmode<3> == '1'
    if (cmode_2 == 0) {
      if (cmode_0 == 0) {
        op = op_bit ? NEONModifiedImmediate_MVNI : NEONModifiedImmediate_MOVI;
      } else {  // cmode<0> == '1'
        op = op_bit ? NEONModifiedImmediate_BIC : NEONModifiedImmediate_ORR;
      }
    } else {  // cmode<2> == '1'
      if (cmode_1 == 0) {
        op = op_bit ? NEONModifiedImmediate_MVNI : NEONModifiedImmediate_MOVI;
      } else {  // cmode<1> == '1'
        if (cmode_0 == 0) {
          op = NEONModifiedImmediate_MOVI;
        } else {  // cmode<0> == '1'
          op = NEONModifiedImmediate_MOVI;
        }
      }
    }
  }

  // Call the logic function
  if (op == NEONModifiedImmediate_ORR) {
    orr(vform, rd, rd, imm);
  } else if (op == NEONModifiedImmediate_BIC) {
    bic(vform, rd, rd, imm);
  } else if (op == NEONModifiedImmediate_MOVI) {
    movi(vform, rd, imm);
  } else if (op == NEONModifiedImmediate_MVNI) {
    mvni(vform, rd, imm);
  } else {
    VisitUnimplemented(instr);
  }
}


void Simulator::VisitNEONScalar2RegMisc(const Instruction* instr) {
  NEONFormatDecoder nfd(instr, NEONFormatDecoder::ScalarFormatMap());
  VectorFormat vf = nfd.GetVectorFormat();

  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());

  if (instr->Mask(NEON2RegMiscOpcode) <= NEON_NEG_scalar_opcode) {
    // These instructions all use a two bit size field, except NOT and RBIT,
    // which use the field to encode the operation.
    switch (instr->Mask(NEONScalar2RegMiscMask)) {
      case NEON_CMEQ_zero_scalar:
        cmp(vf, rd, rn, 0, eq);
        break;
      case NEON_CMGE_zero_scalar:
        cmp(vf, rd, rn, 0, ge);
        break;
      case NEON_CMGT_zero_scalar:
        cmp(vf, rd, rn, 0, gt);
        break;
      case NEON_CMLT_zero_scalar:
        cmp(vf, rd, rn, 0, lt);
        break;
      case NEON_CMLE_zero_scalar:
        cmp(vf, rd, rn, 0, le);
        break;
      case NEON_ABS_scalar:
        abs(vf, rd, rn);
        break;
      case NEON_SQABS_scalar:
        abs(vf, rd, rn).SignedSaturate(vf);
        break;
      case NEON_NEG_scalar:
        neg(vf, rd, rn);
        break;
      case NEON_SQNEG_scalar:
        neg(vf, rd, rn).SignedSaturate(vf);
        break;
      case NEON_SUQADD_scalar:
        suqadd(vf, rd, rd, rn);
        break;
      case NEON_USQADD_scalar:
        usqadd(vf, rd, rd, rn);
        break;
      default:
        VIXL_UNIMPLEMENTED();
        break;
    }
  } else {
    VectorFormat fpf = nfd.GetVectorFormat(nfd.FPScalarFormatMap());
    FPRounding fpcr_rounding = static_cast<FPRounding>(ReadFpcr().GetRMode());

    // These instructions all use a one bit size field, except SQXTUN, SQXTN
    // and UQXTN, which use a two bit size field.
    switch (instr->Mask(NEONScalar2RegMiscFPMask)) {
      case NEON_FRECPE_scalar:
        frecpe(fpf, rd, rn, fpcr_rounding);
        break;
      case NEON_FRECPX_scalar:
        frecpx(fpf, rd, rn);
        break;
      case NEON_FRSQRTE_scalar:
        frsqrte(fpf, rd, rn);
        break;
      case NEON_FCMGT_zero_scalar:
        fcmp_zero(fpf, rd, rn, gt);
        break;
      case NEON_FCMGE_zero_scalar:
        fcmp_zero(fpf, rd, rn, ge);
        break;
      case NEON_FCMEQ_zero_scalar:
        fcmp_zero(fpf, rd, rn, eq);
        break;
      case NEON_FCMLE_zero_scalar:
        fcmp_zero(fpf, rd, rn, le);
        break;
      case NEON_FCMLT_zero_scalar:
        fcmp_zero(fpf, rd, rn, lt);
        break;
      case NEON_SCVTF_scalar:
        scvtf(fpf, rd, rn, 0, fpcr_rounding);
        break;
      case NEON_UCVTF_scalar:
        ucvtf(fpf, rd, rn, 0, fpcr_rounding);
        break;
      case NEON_FCVTNS_scalar:
        fcvts(fpf, rd, rn, FPTieEven);
        break;
      case NEON_FCVTNU_scalar:
        fcvtu(fpf, rd, rn, FPTieEven);
        break;
      case NEON_FCVTPS_scalar:
        fcvts(fpf, rd, rn, FPPositiveInfinity);
        break;
      case NEON_FCVTPU_scalar:
        fcvtu(fpf, rd, rn, FPPositiveInfinity);
        break;
      case NEON_FCVTMS_scalar:
        fcvts(fpf, rd, rn, FPNegativeInfinity);
        break;
      case NEON_FCVTMU_scalar:
        fcvtu(fpf, rd, rn, FPNegativeInfinity);
        break;
      case NEON_FCVTZS_scalar:
        fcvts(fpf, rd, rn, FPZero);
        break;
      case NEON_FCVTZU_scalar:
        fcvtu(fpf, rd, rn, FPZero);
        break;
      case NEON_FCVTAS_scalar:
        fcvts(fpf, rd, rn, FPTieAway);
        break;
      case NEON_FCVTAU_scalar:
        fcvtu(fpf, rd, rn, FPTieAway);
        break;
      case NEON_FCVTXN_scalar:
        // Unlike all of the other FP instructions above, fcvtxn encodes dest
        // size S as size<0>=1. There's only one case, so we ignore the form.
        VIXL_ASSERT(instr->ExtractBit(22) == 1);
        fcvtxn(kFormatS, rd, rn);
        break;
      default:
        switch (instr->Mask(NEONScalar2RegMiscMask)) {
          case NEON_SQXTN_scalar:
            sqxtn(vf, rd, rn);
            break;
          case NEON_UQXTN_scalar:
            uqxtn(vf, rd, rn);
            break;
          case NEON_SQXTUN_scalar:
            sqxtun(vf, rd, rn);
            break;
          default:
            VIXL_UNIMPLEMENTED();
        }
    }
  }
}


void Simulator::VisitNEONScalar2RegMiscFP16(const Instruction* instr) {
  VectorFormat fpf = kFormatH;
  FPRounding fpcr_rounding = static_cast<FPRounding>(ReadFpcr().GetRMode());

  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());

  switch (instr->Mask(NEONScalar2RegMiscFP16Mask)) {
    case NEON_FRECPE_H_scalar:
      frecpe(fpf, rd, rn, fpcr_rounding);
      break;
    case NEON_FRECPX_H_scalar:
      frecpx(fpf, rd, rn);
      break;
    case NEON_FRSQRTE_H_scalar:
      frsqrte(fpf, rd, rn);
      break;
    case NEON_FCMGT_H_zero_scalar:
      fcmp_zero(fpf, rd, rn, gt);
      break;
    case NEON_FCMGE_H_zero_scalar:
      fcmp_zero(fpf, rd, rn, ge);
      break;
    case NEON_FCMEQ_H_zero_scalar:
      fcmp_zero(fpf, rd, rn, eq);
      break;
    case NEON_FCMLE_H_zero_scalar:
      fcmp_zero(fpf, rd, rn, le);
      break;
    case NEON_FCMLT_H_zero_scalar:
      fcmp_zero(fpf, rd, rn, lt);
      break;
    case NEON_SCVTF_H_scalar:
      scvtf(fpf, rd, rn, 0, fpcr_rounding);
      break;
    case NEON_UCVTF_H_scalar:
      ucvtf(fpf, rd, rn, 0, fpcr_rounding);
      break;
    case NEON_FCVTNS_H_scalar:
      fcvts(fpf, rd, rn, FPTieEven);
      break;
    case NEON_FCVTNU_H_scalar:
      fcvtu(fpf, rd, rn, FPTieEven);
      break;
    case NEON_FCVTPS_H_scalar:
      fcvts(fpf, rd, rn, FPPositiveInfinity);
      break;
    case NEON_FCVTPU_H_scalar:
      fcvtu(fpf, rd, rn, FPPositiveInfinity);
      break;
    case NEON_FCVTMS_H_scalar:
      fcvts(fpf, rd, rn, FPNegativeInfinity);
      break;
    case NEON_FCVTMU_H_scalar:
      fcvtu(fpf, rd, rn, FPNegativeInfinity);
      break;
    case NEON_FCVTZS_H_scalar:
      fcvts(fpf, rd, rn, FPZero);
      break;
    case NEON_FCVTZU_H_scalar:
      fcvtu(fpf, rd, rn, FPZero);
      break;
    case NEON_FCVTAS_H_scalar:
      fcvts(fpf, rd, rn, FPTieAway);
      break;
    case NEON_FCVTAU_H_scalar:
      fcvtu(fpf, rd, rn, FPTieAway);
      break;
  }
}


void Simulator::VisitNEONScalar3Diff(const Instruction* instr) {
  NEONFormatDecoder nfd(instr, NEONFormatDecoder::LongScalarFormatMap());
  VectorFormat vf = nfd.GetVectorFormat();

  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());
  SimVRegister& rm = ReadVRegister(instr->GetRm());
  switch (instr->Mask(NEONScalar3DiffMask)) {
    case NEON_SQDMLAL_scalar:
      sqdmlal(vf, rd, rn, rm);
      break;
    case NEON_SQDMLSL_scalar:
      sqdmlsl(vf, rd, rn, rm);
      break;
    case NEON_SQDMULL_scalar:
      sqdmull(vf, rd, rn, rm);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
}


void Simulator::VisitNEONScalar3Same(const Instruction* instr) {
  NEONFormatDecoder nfd(instr, NEONFormatDecoder::ScalarFormatMap());
  VectorFormat vf = nfd.GetVectorFormat();

  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());
  SimVRegister& rm = ReadVRegister(instr->GetRm());

  if (instr->Mask(NEONScalar3SameFPFMask) == NEONScalar3SameFPFixed) {
    vf = nfd.GetVectorFormat(nfd.FPScalarFormatMap());
    switch (instr->Mask(NEONScalar3SameFPMask)) {
      case NEON_FMULX_scalar:
        fmulx(vf, rd, rn, rm);
        break;
      case NEON_FACGE_scalar:
        fabscmp(vf, rd, rn, rm, ge);
        break;
      case NEON_FACGT_scalar:
        fabscmp(vf, rd, rn, rm, gt);
        break;
      case NEON_FCMEQ_scalar:
        fcmp(vf, rd, rn, rm, eq);
        break;
      case NEON_FCMGE_scalar:
        fcmp(vf, rd, rn, rm, ge);
        break;
      case NEON_FCMGT_scalar:
        fcmp(vf, rd, rn, rm, gt);
        break;
      case NEON_FRECPS_scalar:
        frecps(vf, rd, rn, rm);
        break;
      case NEON_FRSQRTS_scalar:
        frsqrts(vf, rd, rn, rm);
        break;
      case NEON_FABD_scalar:
        fabd(vf, rd, rn, rm);
        break;
      default:
        VIXL_UNIMPLEMENTED();
    }
  } else {
    switch (instr->Mask(NEONScalar3SameMask)) {
      case NEON_ADD_scalar:
        add(vf, rd, rn, rm);
        break;
      case NEON_SUB_scalar:
        sub(vf, rd, rn, rm);
        break;
      case NEON_CMEQ_scalar:
        cmp(vf, rd, rn, rm, eq);
        break;
      case NEON_CMGE_scalar:
        cmp(vf, rd, rn, rm, ge);
        break;
      case NEON_CMGT_scalar:
        cmp(vf, rd, rn, rm, gt);
        break;
      case NEON_CMHI_scalar:
        cmp(vf, rd, rn, rm, hi);
        break;
      case NEON_CMHS_scalar:
        cmp(vf, rd, rn, rm, hs);
        break;
      case NEON_CMTST_scalar:
        cmptst(vf, rd, rn, rm);
        break;
      case NEON_USHL_scalar:
        ushl(vf, rd, rn, rm);
        break;
      case NEON_SSHL_scalar:
        sshl(vf, rd, rn, rm);
        break;
      case NEON_SQDMULH_scalar:
        sqdmulh(vf, rd, rn, rm);
        break;
      case NEON_SQRDMULH_scalar:
        sqrdmulh(vf, rd, rn, rm);
        break;
      case NEON_UQADD_scalar:
        add(vf, rd, rn, rm).UnsignedSaturate(vf);
        break;
      case NEON_SQADD_scalar:
        add(vf, rd, rn, rm).SignedSaturate(vf);
        break;
      case NEON_UQSUB_scalar:
        sub(vf, rd, rn, rm).UnsignedSaturate(vf);
        break;
      case NEON_SQSUB_scalar:
        sub(vf, rd, rn, rm).SignedSaturate(vf);
        break;
      case NEON_UQSHL_scalar:
        ushl(vf, rd, rn, rm).UnsignedSaturate(vf);
        break;
      case NEON_SQSHL_scalar:
        sshl(vf, rd, rn, rm).SignedSaturate(vf);
        break;
      case NEON_URSHL_scalar:
        ushl(vf, rd, rn, rm).Round(vf);
        break;
      case NEON_SRSHL_scalar:
        sshl(vf, rd, rn, rm).Round(vf);
        break;
      case NEON_UQRSHL_scalar:
        ushl(vf, rd, rn, rm).Round(vf).UnsignedSaturate(vf);
        break;
      case NEON_SQRSHL_scalar:
        sshl(vf, rd, rn, rm).Round(vf).SignedSaturate(vf);
        break;
      default:
        VIXL_UNIMPLEMENTED();
    }
  }
}

void Simulator::VisitNEONScalar3SameFP16(const Instruction* instr) {
  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());
  SimVRegister& rm = ReadVRegister(instr->GetRm());

  switch (instr->Mask(NEONScalar3SameFP16Mask)) {
    case NEON_FABD_H_scalar:
      fabd(kFormatH, rd, rn, rm);
      break;
    case NEON_FMULX_H_scalar:
      fmulx(kFormatH, rd, rn, rm);
      break;
    case NEON_FCMEQ_H_scalar:
      fcmp(kFormatH, rd, rn, rm, eq);
      break;
    case NEON_FCMGE_H_scalar:
      fcmp(kFormatH, rd, rn, rm, ge);
      break;
    case NEON_FCMGT_H_scalar:
      fcmp(kFormatH, rd, rn, rm, gt);
      break;
    case NEON_FACGE_H_scalar:
      fabscmp(kFormatH, rd, rn, rm, ge);
      break;
    case NEON_FACGT_H_scalar:
      fabscmp(kFormatH, rd, rn, rm, gt);
      break;
    case NEON_FRECPS_H_scalar:
      frecps(kFormatH, rd, rn, rm);
      break;
    case NEON_FRSQRTS_H_scalar:
      frsqrts(kFormatH, rd, rn, rm);
      break;
    default:
      VIXL_UNREACHABLE();
  }
}


void Simulator::VisitNEONScalar3SameExtra(const Instruction* instr) {
  NEONFormatDecoder nfd(instr, NEONFormatDecoder::ScalarFormatMap());
  VectorFormat vf = nfd.GetVectorFormat();

  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());
  SimVRegister& rm = ReadVRegister(instr->GetRm());

  switch (instr->Mask(NEONScalar3SameExtraMask)) {
    case NEON_SQRDMLAH_scalar:
      sqrdmlah(vf, rd, rn, rm);
      break;
    case NEON_SQRDMLSH_scalar:
      sqrdmlsh(vf, rd, rn, rm);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
}

void Simulator::VisitNEONScalarByIndexedElement(const Instruction* instr) {
  NEONFormatDecoder nfd(instr, NEONFormatDecoder::LongScalarFormatMap());
  VectorFormat vf = nfd.GetVectorFormat();
  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());
  ByElementOp Op = NULL;

  std::pair<int, int> rm_and_index = instr->GetNEONMulRmAndIndex();
  std::unordered_map<uint32_t, ByElementOp> handler = {
      {"sqdmull_asisdelem_l"_h, &Simulator::sqdmull},
      {"sqdmlal_asisdelem_l"_h, &Simulator::sqdmlal},
      {"sqdmlsl_asisdelem_l"_h, &Simulator::sqdmlsl},
      {"sqdmulh_asisdelem_r"_h, &Simulator::sqdmulh},
      {"sqrdmulh_asisdelem_r"_h, &Simulator::sqrdmulh},
      {"sqrdmlah_asisdelem_r"_h, &Simulator::sqrdmlah},
      {"sqrdmlsh_asisdelem_r"_h, &Simulator::sqrdmlsh},
      {"fmul_asisdelem_rh_h"_h, &Simulator::fmul},
      {"fmul_asisdelem_r_sd"_h, &Simulator::fmul},
      {"fmla_asisdelem_rh_h"_h, &Simulator::fmla},
      {"fmla_asisdelem_r_sd"_h, &Simulator::fmla},
      {"fmls_asisdelem_rh_h"_h, &Simulator::fmls},
      {"fmls_asisdelem_r_sd"_h, &Simulator::fmls},
      {"fmulx_asisdelem_rh_h"_h, &Simulator::fmulx},
      {"fmulx_asisdelem_r_sd"_h, &Simulator::fmulx},
  };

  std::unordered_map<uint32_t, ByElementOp>::const_iterator it =
      handler.find(form_hash_);

  if (it == handler.end()) {
    VIXL_UNIMPLEMENTED();
  } else {
    Op = it->second;
  }

  switch (form_hash_) {
    case "sqdmulh_asisdelem_r"_h:
    case "sqrdmulh_asisdelem_r"_h:
    case "sqrdmlah_asisdelem_r"_h:
    case "sqrdmlsh_asisdelem_r"_h:
      vf = nfd.GetVectorFormat(nfd.ScalarFormatMap());
      break;
    case "fmul_asisdelem_r_sd"_h:
    case "fmla_asisdelem_r_sd"_h:
    case "fmls_asisdelem_r_sd"_h:
    case "fmulx_asisdelem_r_sd"_h:
      vf = nfd.GetVectorFormat(nfd.FPScalarFormatMap());
      break;
    case "fmul_asisdelem_rh_h"_h:
    case "fmla_asisdelem_rh_h"_h:
    case "fmls_asisdelem_rh_h"_h:
    case "fmulx_asisdelem_rh_h"_h:
      vf = kFormatH;
      break;
  }

  (this->*Op)(vf,
              rd,
              rn,
              ReadVRegister(rm_and_index.first),
              rm_and_index.second);
}


void Simulator::VisitNEONScalarCopy(const Instruction* instr) {
  NEONFormatDecoder nfd(instr, NEONFormatDecoder::TriangularScalarFormatMap());
  VectorFormat vf = nfd.GetVectorFormat();

  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());

  if (instr->Mask(NEONScalarCopyMask) == NEON_DUP_ELEMENT_scalar) {
    int imm5 = instr->GetImmNEON5();
    int tz = CountTrailingZeros(imm5, 32);
    int rn_index = ExtractSignedBitfield32(31, tz + 1, imm5);
    dup_element(vf, rd, rn, rn_index);
  } else {
    VIXL_UNIMPLEMENTED();
  }
}


void Simulator::VisitNEONScalarPairwise(const Instruction* instr) {
  NEONFormatDecoder nfd(instr, NEONFormatDecoder::FPScalarPairwiseFormatMap());
  VectorFormat vf = nfd.GetVectorFormat();

  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());
  switch (instr->Mask(NEONScalarPairwiseMask)) {
    case NEON_ADDP_scalar: {
      // All pairwise operations except ADDP use bit U to differentiate FP16
      // from FP32/FP64 variations.
      NEONFormatDecoder nfd_addp(instr, NEONFormatDecoder::FPScalarFormatMap());
      addp(nfd_addp.GetVectorFormat(), rd, rn);
      break;
    }
    case NEON_FADDP_h_scalar:
    case NEON_FADDP_scalar:
      faddp(vf, rd, rn);
      break;
    case NEON_FMAXP_h_scalar:
    case NEON_FMAXP_scalar:
      fmaxp(vf, rd, rn);
      break;
    case NEON_FMAXNMP_h_scalar:
    case NEON_FMAXNMP_scalar:
      fmaxnmp(vf, rd, rn);
      break;
    case NEON_FMINP_h_scalar:
    case NEON_FMINP_scalar:
      fminp(vf, rd, rn);
      break;
    case NEON_FMINNMP_h_scalar:
    case NEON_FMINNMP_scalar:
      fminnmp(vf, rd, rn);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
}


void Simulator::VisitNEONScalarShiftImmediate(const Instruction* instr) {
  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());
  FPRounding fpcr_rounding = static_cast<FPRounding>(ReadFpcr().GetRMode());

  static const NEONFormatMap map = {{22, 21, 20, 19},
                                    {NF_UNDEF,
                                     NF_B,
                                     NF_H,
                                     NF_H,
                                     NF_S,
                                     NF_S,
                                     NF_S,
                                     NF_S,
                                     NF_D,
                                     NF_D,
                                     NF_D,
                                     NF_D,
                                     NF_D,
                                     NF_D,
                                     NF_D,
                                     NF_D}};
  NEONFormatDecoder nfd(instr, &map);
  VectorFormat vf = nfd.GetVectorFormat();

  int highest_set_bit = HighestSetBitPosition(instr->GetImmNEONImmh());
  int immh_immb = instr->GetImmNEONImmhImmb();
  int right_shift = (16 << highest_set_bit) - immh_immb;
  int left_shift = immh_immb - (8 << highest_set_bit);
  switch (instr->Mask(NEONScalarShiftImmediateMask)) {
    case NEON_SHL_scalar:
      shl(vf, rd, rn, left_shift);
      break;
    case NEON_SLI_scalar:
      sli(vf, rd, rn, left_shift);
      break;
    case NEON_SQSHL_imm_scalar:
      sqshl(vf, rd, rn, left_shift);
      break;
    case NEON_UQSHL_imm_scalar:
      uqshl(vf, rd, rn, left_shift);
      break;
    case NEON_SQSHLU_scalar:
      sqshlu(vf, rd, rn, left_shift);
      break;
    case NEON_SRI_scalar:
      sri(vf, rd, rn, right_shift);
      break;
    case NEON_SSHR_scalar:
      sshr(vf, rd, rn, right_shift);
      break;
    case NEON_USHR_scalar:
      ushr(vf, rd, rn, right_shift);
      break;
    case NEON_SRSHR_scalar:
      sshr(vf, rd, rn, right_shift).Round(vf);
      break;
    case NEON_URSHR_scalar:
      ushr(vf, rd, rn, right_shift).Round(vf);
      break;
    case NEON_SSRA_scalar:
      ssra(vf, rd, rn, right_shift);
      break;
    case NEON_USRA_scalar:
      usra(vf, rd, rn, right_shift);
      break;
    case NEON_SRSRA_scalar:
      srsra(vf, rd, rn, right_shift);
      break;
    case NEON_URSRA_scalar:
      ursra(vf, rd, rn, right_shift);
      break;
    case NEON_UQSHRN_scalar:
      uqshrn(vf, rd, rn, right_shift);
      break;
    case NEON_UQRSHRN_scalar:
      uqrshrn(vf, rd, rn, right_shift);
      break;
    case NEON_SQSHRN_scalar:
      sqshrn(vf, rd, rn, right_shift);
      break;
    case NEON_SQRSHRN_scalar:
      sqrshrn(vf, rd, rn, right_shift);
      break;
    case NEON_SQSHRUN_scalar:
      sqshrun(vf, rd, rn, right_shift);
      break;
    case NEON_SQRSHRUN_scalar:
      sqrshrun(vf, rd, rn, right_shift);
      break;
    case NEON_FCVTZS_imm_scalar:
      fcvts(vf, rd, rn, FPZero, right_shift);
      break;
    case NEON_FCVTZU_imm_scalar:
      fcvtu(vf, rd, rn, FPZero, right_shift);
      break;
    case NEON_SCVTF_imm_scalar:
      scvtf(vf, rd, rn, right_shift, fpcr_rounding);
      break;
    case NEON_UCVTF_imm_scalar:
      ucvtf(vf, rd, rn, right_shift, fpcr_rounding);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
}


void Simulator::VisitNEONShiftImmediate(const Instruction* instr) {
  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());
  FPRounding fpcr_rounding = static_cast<FPRounding>(ReadFpcr().GetRMode());

  // 00010->8B, 00011->16B, 001x0->4H, 001x1->8H,
  // 01xx0->2S, 01xx1->4S, 1xxx1->2D, all others undefined.
  static const NEONFormatMap map = {{22, 21, 20, 19, 30},
                                    {NF_UNDEF, NF_UNDEF, NF_8B,    NF_16B,
                                     NF_4H,    NF_8H,    NF_4H,    NF_8H,
                                     NF_2S,    NF_4S,    NF_2S,    NF_4S,
                                     NF_2S,    NF_4S,    NF_2S,    NF_4S,
                                     NF_UNDEF, NF_2D,    NF_UNDEF, NF_2D,
                                     NF_UNDEF, NF_2D,    NF_UNDEF, NF_2D,
                                     NF_UNDEF, NF_2D,    NF_UNDEF, NF_2D,
                                     NF_UNDEF, NF_2D,    NF_UNDEF, NF_2D}};
  NEONFormatDecoder nfd(instr, &map);
  VectorFormat vf = nfd.GetVectorFormat();

  // 0001->8H, 001x->4S, 01xx->2D, all others undefined.
  static const NEONFormatMap map_l =
      {{22, 21, 20, 19},
       {NF_UNDEF, NF_8H, NF_4S, NF_4S, NF_2D, NF_2D, NF_2D, NF_2D}};
  VectorFormat vf_l = nfd.GetVectorFormat(&map_l);

  int highest_set_bit = HighestSetBitPosition(instr->GetImmNEONImmh());
  int immh_immb = instr->GetImmNEONImmhImmb();
  int right_shift = (16 << highest_set_bit) - immh_immb;
  int left_shift = immh_immb - (8 << highest_set_bit);

  switch (instr->Mask(NEONShiftImmediateMask)) {
    case NEON_SHL:
      shl(vf, rd, rn, left_shift);
      break;
    case NEON_SLI:
      sli(vf, rd, rn, left_shift);
      break;
    case NEON_SQSHLU:
      sqshlu(vf, rd, rn, left_shift);
      break;
    case NEON_SRI:
      sri(vf, rd, rn, right_shift);
      break;
    case NEON_SSHR:
      sshr(vf, rd, rn, right_shift);
      break;
    case NEON_USHR:
      ushr(vf, rd, rn, right_shift);
      break;
    case NEON_SRSHR:
      sshr(vf, rd, rn, right_shift).Round(vf);
      break;
    case NEON_URSHR:
      ushr(vf, rd, rn, right_shift).Round(vf);
      break;
    case NEON_SSRA:
      ssra(vf, rd, rn, right_shift);
      break;
    case NEON_USRA:
      usra(vf, rd, rn, right_shift);
      break;
    case NEON_SRSRA:
      srsra(vf, rd, rn, right_shift);
      break;
    case NEON_URSRA:
      ursra(vf, rd, rn, right_shift);
      break;
    case NEON_SQSHL_imm:
      sqshl(vf, rd, rn, left_shift);
      break;
    case NEON_UQSHL_imm:
      uqshl(vf, rd, rn, left_shift);
      break;
    case NEON_SCVTF_imm:
      scvtf(vf, rd, rn, right_shift, fpcr_rounding);
      break;
    case NEON_UCVTF_imm:
      ucvtf(vf, rd, rn, right_shift, fpcr_rounding);
      break;
    case NEON_FCVTZS_imm:
      fcvts(vf, rd, rn, FPZero, right_shift);
      break;
    case NEON_FCVTZU_imm:
      fcvtu(vf, rd, rn, FPZero, right_shift);
      break;
    case NEON_SSHLL:
      vf = vf_l;
      if (instr->Mask(NEON_Q)) {
        sshll2(vf, rd, rn, left_shift);
      } else {
        sshll(vf, rd, rn, left_shift);
      }
      break;
    case NEON_USHLL:
      vf = vf_l;
      if (instr->Mask(NEON_Q)) {
        ushll2(vf, rd, rn, left_shift);
      } else {
        ushll(vf, rd, rn, left_shift);
      }
      break;
    case NEON_SHRN:
      if (instr->Mask(NEON_Q)) {
        shrn2(vf, rd, rn, right_shift);
      } else {
        shrn(vf, rd, rn, right_shift);
      }
      break;
    case NEON_RSHRN:
      if (instr->Mask(NEON_Q)) {
        rshrn2(vf, rd, rn, right_shift);
      } else {
        rshrn(vf, rd, rn, right_shift);
      }
      break;
    case NEON_UQSHRN:
      if (instr->Mask(NEON_Q)) {
        uqshrn2(vf, rd, rn, right_shift);
      } else {
        uqshrn(vf, rd, rn, right_shift);
      }
      break;
    case NEON_UQRSHRN:
      if (instr->Mask(NEON_Q)) {
        uqrshrn2(vf, rd, rn, right_shift);
      } else {
        uqrshrn(vf, rd, rn, right_shift);
      }
      break;
    case NEON_SQSHRN:
      if (instr->Mask(NEON_Q)) {
        sqshrn2(vf, rd, rn, right_shift);
      } else {
        sqshrn(vf, rd, rn, right_shift);
      }
      break;
    case NEON_SQRSHRN:
      if (instr->Mask(NEON_Q)) {
        sqrshrn2(vf, rd, rn, right_shift);
      } else {
        sqrshrn(vf, rd, rn, right_shift);
      }
      break;
    case NEON_SQSHRUN:
      if (instr->Mask(NEON_Q)) {
        sqshrun2(vf, rd, rn, right_shift);
      } else {
        sqshrun(vf, rd, rn, right_shift);
      }
      break;
    case NEON_SQRSHRUN:
      if (instr->Mask(NEON_Q)) {
        sqrshrun2(vf, rd, rn, right_shift);
      } else {
        sqrshrun(vf, rd, rn, right_shift);
      }
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
}


void Simulator::VisitNEONTable(const Instruction* instr) {
  NEONFormatDecoder nfd(instr, NEONFormatDecoder::LogicalFormatMap());
  VectorFormat vf = nfd.GetVectorFormat();

  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());
  SimVRegister& rn2 = ReadVRegister((instr->GetRn() + 1) % kNumberOfVRegisters);
  SimVRegister& rn3 = ReadVRegister((instr->GetRn() + 2) % kNumberOfVRegisters);
  SimVRegister& rn4 = ReadVRegister((instr->GetRn() + 3) % kNumberOfVRegisters);
  SimVRegister& rm = ReadVRegister(instr->GetRm());

  switch (instr->Mask(NEONTableMask)) {
    case NEON_TBL_1v:
      tbl(vf, rd, rn, rm);
      break;
    case NEON_TBL_2v:
      tbl(vf, rd, rn, rn2, rm);
      break;
    case NEON_TBL_3v:
      tbl(vf, rd, rn, rn2, rn3, rm);
      break;
    case NEON_TBL_4v:
      tbl(vf, rd, rn, rn2, rn3, rn4, rm);
      break;
    case NEON_TBX_1v:
      tbx(vf, rd, rn, rm);
      break;
    case NEON_TBX_2v:
      tbx(vf, rd, rn, rn2, rm);
      break;
    case NEON_TBX_3v:
      tbx(vf, rd, rn, rn2, rn3, rm);
      break;
    case NEON_TBX_4v:
      tbx(vf, rd, rn, rn2, rn3, rn4, rm);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
}


void Simulator::VisitNEONPerm(const Instruction* instr) {
  NEONFormatDecoder nfd(instr);
  VectorFormat vf = nfd.GetVectorFormat();

  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());
  SimVRegister& rm = ReadVRegister(instr->GetRm());

  switch (instr->Mask(NEONPermMask)) {
    case NEON_TRN1:
      trn1(vf, rd, rn, rm);
      break;
    case NEON_TRN2:
      trn2(vf, rd, rn, rm);
      break;
    case NEON_UZP1:
      uzp1(vf, rd, rn, rm);
      break;
    case NEON_UZP2:
      uzp2(vf, rd, rn, rm);
      break;
    case NEON_ZIP1:
      zip1(vf, rd, rn, rm);
      break;
    case NEON_ZIP2:
      zip2(vf, rd, rn, rm);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
}

void Simulator::SimulateNEONSHA3(const Instruction* instr) {
  SimVRegister& rd = ReadVRegister(instr->GetRd());
  SimVRegister& rn = ReadVRegister(instr->GetRn());
  SimVRegister& rm = ReadVRegister(instr->GetRm());
  SimVRegister& ra = ReadVRegister(instr->GetRa());
  SimVRegister temp;

  switch (form_hash_) {
    case "bcax_vvv16_crypto4"_h:
      bic(kFormat16B, temp, rm, ra);
      eor(kFormat16B, rd, rn, temp);
      break;
    case "eor3_vvv16_crypto4"_h:
      eor(kFormat16B, temp, rm, ra);
      eor(kFormat16B, rd, rn, temp);
      break;
    case "rax1_vvv2_cryptosha512_3"_h:
      ror(kFormat2D, temp, rm, 63);  // rol(1) => ror(63)
      eor(kFormat2D, rd, rn, temp);
      break;
    case "xar_vvv2_crypto3_imm6"_h:
      int rot = instr->ExtractBits(15, 10);
      eor(kFormat2D, temp, rn, rm);
      ror(kFormat2D, rd, temp, rot);
      break;
  }
}

void Simulator::VisitSVEAddressGeneration(const Instruction* instr) {
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimVRegister& zm = ReadVRegister(instr->GetRm());
  SimVRegister temp;

  VectorFormat vform = kFormatVnD;
  mov(vform, temp, zm);

  switch (instr->Mask(SVEAddressGenerationMask)) {
    case ADR_z_az_d_s32_scaled:
      sxt(vform, temp, temp, kSRegSize);
      break;
    case ADR_z_az_d_u32_scaled:
      uxt(vform, temp, temp, kSRegSize);
      break;
    case ADR_z_az_s_same_scaled:
      vform = kFormatVnS;
      break;
    case ADR_z_az_d_same_scaled:
      // Nothing to do.
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  int shift_amount = instr->ExtractBits(11, 10);
  shl(vform, temp, temp, shift_amount);
  add(vform, zd, zn, temp);
}

void Simulator::VisitSVEBitwiseLogicalWithImm_Unpredicated(
    const Instruction* instr) {
  Instr op = instr->Mask(SVEBitwiseLogicalWithImm_UnpredicatedMask);
  switch (op) {
    case AND_z_zi:
    case EOR_z_zi:
    case ORR_z_zi: {
      int lane_size = instr->GetSVEBitwiseImmLaneSizeInBytesLog2();
      uint64_t imm = instr->GetSVEImmLogical();
      // Valid immediate is a non-zero bits
      VIXL_ASSERT(imm != 0);
      SVEBitwiseImmHelper(static_cast<SVEBitwiseLogicalWithImm_UnpredicatedOp>(
                              op),
                          SVEFormatFromLaneSizeInBytesLog2(lane_size),
                          ReadVRegister(instr->GetRd()),
                          imm);
      break;
    }
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEBroadcastBitmaskImm(const Instruction* instr) {
  switch (instr->Mask(SVEBroadcastBitmaskImmMask)) {
    case DUPM_z_i: {
      /* DUPM uses the same lane size and immediate encoding as bitwise logical
       * immediate instructions. */
      int lane_size = instr->GetSVEBitwiseImmLaneSizeInBytesLog2();
      uint64_t imm = instr->GetSVEImmLogical();
      VectorFormat vform = SVEFormatFromLaneSizeInBytesLog2(lane_size);
      dup_immediate(vform, ReadVRegister(instr->GetRd()), imm);
      break;
    }
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEBitwiseLogicalUnpredicated(const Instruction* instr) {
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimVRegister& zm = ReadVRegister(instr->GetRm());
  Instr op = instr->Mask(SVEBitwiseLogicalUnpredicatedMask);

  LogicalOp logical_op = LogicalOpMask;
  switch (op) {
    case AND_z_zz:
      logical_op = AND;
      break;
    case BIC_z_zz:
      logical_op = BIC;
      break;
    case EOR_z_zz:
      logical_op = EOR;
      break;
    case ORR_z_zz:
      logical_op = ORR;
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
  // Lane size of registers is irrelevant to the bitwise operations, so perform
  // the operation on D-sized lanes.
  SVEBitwiseLogicalUnpredicatedHelper(logical_op, kFormatVnD, zd, zn, zm);
}

void Simulator::VisitSVEBitwiseShiftByImm_Predicated(const Instruction* instr) {
  SimVRegister& zdn = ReadVRegister(instr->GetRd());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());

  SimVRegister scratch;
  SimVRegister result;

  bool for_division = false;
  Shift shift_op = NO_SHIFT;
  switch (instr->Mask(SVEBitwiseShiftByImm_PredicatedMask)) {
    case ASRD_z_p_zi:
      shift_op = ASR;
      for_division = true;
      break;
    case ASR_z_p_zi:
      shift_op = ASR;
      break;
    case LSL_z_p_zi:
      shift_op = LSL;
      break;
    case LSR_z_p_zi:
      shift_op = LSR;
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  std::pair<int, int> shift_and_lane_size =
      instr->GetSVEImmShiftAndLaneSizeLog2(/* is_predicated = */ true);
  unsigned lane_size = shift_and_lane_size.second;
  VectorFormat vform = SVEFormatFromLaneSizeInBytesLog2(lane_size);
  int shift_dist = shift_and_lane_size.first;

  if ((shift_op == ASR) && for_division) {
    asrd(vform, result, zdn, shift_dist);
  } else {
    if (shift_op == LSL) {
      // Shift distance is computed differently for LSL. Convert the result.
      shift_dist = (8 << lane_size) - shift_dist;
    }
    dup_immediate(vform, scratch, shift_dist);
    SVEBitwiseShiftHelper(shift_op, vform, result, zdn, scratch, false);
  }
  mov_merging(vform, zdn, pg, result);
}

void Simulator::VisitSVEBitwiseShiftByVector_Predicated(
    const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zdn = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRn());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  SimVRegister result;

  // SVE uses the whole (saturated) lane for the shift amount.
  bool shift_in_ls_byte = false;

  switch (form_hash_) {
    case "asrr_z_p_zz"_h:
      sshr(vform, result, zm, zdn);
      break;
    case "asr_z_p_zz"_h:
      sshr(vform, result, zdn, zm);
      break;
    case "lslr_z_p_zz"_h:
      sshl(vform, result, zm, zdn, shift_in_ls_byte);
      break;
    case "lsl_z_p_zz"_h:
      sshl(vform, result, zdn, zm, shift_in_ls_byte);
      break;
    case "lsrr_z_p_zz"_h:
      ushr(vform, result, zm, zdn);
      break;
    case "lsr_z_p_zz"_h:
      ushr(vform, result, zdn, zm);
      break;
    case "sqrshl_z_p_zz"_h:
      sshl(vform, result, zdn, zm, shift_in_ls_byte)
          .Round(vform)
          .SignedSaturate(vform);
      break;
    case "sqrshlr_z_p_zz"_h:
      sshl(vform, result, zm, zdn, shift_in_ls_byte)
          .Round(vform)
          .SignedSaturate(vform);
      break;
    case "sqshl_z_p_zz"_h:
      sshl(vform, result, zdn, zm, shift_in_ls_byte).SignedSaturate(vform);
      break;
    case "sqshlr_z_p_zz"_h:
      sshl(vform, result, zm, zdn, shift_in_ls_byte).SignedSaturate(vform);
      break;
    case "srshl_z_p_zz"_h:
      sshl(vform, result, zdn, zm, shift_in_ls_byte).Round(vform);
      break;
    case "srshlr_z_p_zz"_h:
      sshl(vform, result, zm, zdn, shift_in_ls_byte).Round(vform);
      break;
    case "uqrshl_z_p_zz"_h:
      ushl(vform, result, zdn, zm, shift_in_ls_byte)
          .Round(vform)
          .UnsignedSaturate(vform);
      break;
    case "uqrshlr_z_p_zz"_h:
      ushl(vform, result, zm, zdn, shift_in_ls_byte)
          .Round(vform)
          .UnsignedSaturate(vform);
      break;
    case "uqshl_z_p_zz"_h:
      ushl(vform, result, zdn, zm, shift_in_ls_byte).UnsignedSaturate(vform);
      break;
    case "uqshlr_z_p_zz"_h:
      ushl(vform, result, zm, zdn, shift_in_ls_byte).UnsignedSaturate(vform);
      break;
    case "urshl_z_p_zz"_h:
      ushl(vform, result, zdn, zm, shift_in_ls_byte).Round(vform);
      break;
    case "urshlr_z_p_zz"_h:
      ushl(vform, result, zm, zdn, shift_in_ls_byte).Round(vform);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
  mov_merging(vform, zdn, pg, result);
}

void Simulator::VisitSVEBitwiseShiftByWideElements_Predicated(
    const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zdn = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRn());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());

  SimVRegister result;
  Shift shift_op = ASR;

  switch (instr->Mask(SVEBitwiseShiftByWideElements_PredicatedMask)) {
    case ASR_z_p_zw:
      break;
    case LSL_z_p_zw:
      shift_op = LSL;
      break;
    case LSR_z_p_zw:
      shift_op = LSR;
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
  SVEBitwiseShiftHelper(shift_op,
                        vform,
                        result,
                        zdn,
                        zm,
                        /* is_wide_elements = */ true);
  mov_merging(vform, zdn, pg, result);
}

void Simulator::VisitSVEBitwiseShiftUnpredicated(const Instruction* instr) {
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());

  Shift shift_op = NO_SHIFT;
  switch (instr->Mask(SVEBitwiseShiftUnpredicatedMask)) {
    case ASR_z_zi:
    case ASR_z_zw:
      shift_op = ASR;
      break;
    case LSL_z_zi:
    case LSL_z_zw:
      shift_op = LSL;
      break;
    case LSR_z_zi:
    case LSR_z_zw:
      shift_op = LSR;
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  switch (instr->Mask(SVEBitwiseShiftUnpredicatedMask)) {
    case ASR_z_zi:
    case LSL_z_zi:
    case LSR_z_zi: {
      SimVRegister scratch;
      std::pair<int, int> shift_and_lane_size =
          instr->GetSVEImmShiftAndLaneSizeLog2(/* is_predicated = */ false);
      unsigned lane_size = shift_and_lane_size.second;
      VIXL_ASSERT(lane_size <= kDRegSizeInBytesLog2);
      VectorFormat vform = SVEFormatFromLaneSizeInBytesLog2(lane_size);
      int shift_dist = shift_and_lane_size.first;
      if (shift_op == LSL) {
        // Shift distance is computed differently for LSL. Convert the result.
        shift_dist = (8 << lane_size) - shift_dist;
      }
      dup_immediate(vform, scratch, shift_dist);
      SVEBitwiseShiftHelper(shift_op, vform, zd, zn, scratch, false);
      break;
    }
    case ASR_z_zw:
    case LSL_z_zw:
    case LSR_z_zw:
      SVEBitwiseShiftHelper(shift_op,
                            instr->GetSVEVectorFormat(),
                            zd,
                            zn,
                            ReadVRegister(instr->GetRm()),
                            true);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEIncDecRegisterByElementCount(const Instruction* instr) {
  // Although the instructions have a separate encoding class, the lane size is
  // encoded in the same way as most other SVE instructions.
  VectorFormat vform = instr->GetSVEVectorFormat();

  int pattern = instr->GetImmSVEPredicateConstraint();
  int count = GetPredicateConstraintLaneCount(vform, pattern);
  int multiplier = instr->ExtractBits(19, 16) + 1;

  switch (instr->Mask(SVEIncDecRegisterByElementCountMask)) {
    case DECB_r_rs:
    case DECD_r_rs:
    case DECH_r_rs:
    case DECW_r_rs:
      count = -count;
      break;
    case INCB_r_rs:
    case INCD_r_rs:
    case INCH_r_rs:
    case INCW_r_rs:
      // Nothing to do.
      break;
    default:
      VIXL_UNIMPLEMENTED();
      return;
  }

  WriteXRegister(instr->GetRd(),
                 IncDecN(ReadXRegister(instr->GetRd()),
                         count * multiplier,
                         kXRegSize));
}

void Simulator::VisitSVEIncDecVectorByElementCount(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  if (LaneSizeInBitsFromFormat(vform) == kBRegSize) {
    VIXL_UNIMPLEMENTED();
  }

  int pattern = instr->GetImmSVEPredicateConstraint();
  int count = GetPredicateConstraintLaneCount(vform, pattern);
  int multiplier = instr->ExtractBits(19, 16) + 1;

  switch (instr->Mask(SVEIncDecVectorByElementCountMask)) {
    case DECD_z_zs:
    case DECH_z_zs:
    case DECW_z_zs:
      count = -count;
      break;
    case INCD_z_zs:
    case INCH_z_zs:
    case INCW_z_zs:
      // Nothing to do.
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister scratch;
  dup_immediate(vform,
                scratch,
                IncDecN(0,
                        count * multiplier,
                        LaneSizeInBitsFromFormat(vform)));
  add(vform, zd, zd, scratch);
}

void Simulator::VisitSVESaturatingIncDecRegisterByElementCount(
    const Instruction* instr) {
  // Although the instructions have a separate encoding class, the lane size is
  // encoded in the same way as most other SVE instructions.
  VectorFormat vform = instr->GetSVEVectorFormat();

  int pattern = instr->GetImmSVEPredicateConstraint();
  int count = GetPredicateConstraintLaneCount(vform, pattern);
  int multiplier = instr->ExtractBits(19, 16) + 1;

  unsigned width = kXRegSize;
  bool is_signed = false;

  switch (instr->Mask(SVESaturatingIncDecRegisterByElementCountMask)) {
    case SQDECB_r_rs_sx:
    case SQDECD_r_rs_sx:
    case SQDECH_r_rs_sx:
    case SQDECW_r_rs_sx:
      width = kWRegSize;
      VIXL_FALLTHROUGH();
    case SQDECB_r_rs_x:
    case SQDECD_r_rs_x:
    case SQDECH_r_rs_x:
    case SQDECW_r_rs_x:
      is_signed = true;
      count = -count;
      break;
    case SQINCB_r_rs_sx:
    case SQINCD_r_rs_sx:
    case SQINCH_r_rs_sx:
    case SQINCW_r_rs_sx:
      width = kWRegSize;
      VIXL_FALLTHROUGH();
    case SQINCB_r_rs_x:
    case SQINCD_r_rs_x:
    case SQINCH_r_rs_x:
    case SQINCW_r_rs_x:
      is_signed = true;
      break;
    case UQDECB_r_rs_uw:
    case UQDECD_r_rs_uw:
    case UQDECH_r_rs_uw:
    case UQDECW_r_rs_uw:
      width = kWRegSize;
      VIXL_FALLTHROUGH();
    case UQDECB_r_rs_x:
    case UQDECD_r_rs_x:
    case UQDECH_r_rs_x:
    case UQDECW_r_rs_x:
      count = -count;
      break;
    case UQINCB_r_rs_uw:
    case UQINCD_r_rs_uw:
    case UQINCH_r_rs_uw:
    case UQINCW_r_rs_uw:
      width = kWRegSize;
      VIXL_FALLTHROUGH();
    case UQINCB_r_rs_x:
    case UQINCD_r_rs_x:
    case UQINCH_r_rs_x:
    case UQINCW_r_rs_x:
      // Nothing to do.
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  WriteXRegister(instr->GetRd(),
                 IncDecN(ReadXRegister(instr->GetRd()),
                         count * multiplier,
                         width,
                         true,
                         is_signed));
}

void Simulator::VisitSVESaturatingIncDecVectorByElementCount(
    const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  if (LaneSizeInBitsFromFormat(vform) == kBRegSize) {
    VIXL_UNIMPLEMENTED();
  }

  int pattern = instr->GetImmSVEPredicateConstraint();
  int count = GetPredicateConstraintLaneCount(vform, pattern);
  int multiplier = instr->ExtractBits(19, 16) + 1;

  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister scratch;
  dup_immediate(vform,
                scratch,
                IncDecN(0,
                        count * multiplier,
                        LaneSizeInBitsFromFormat(vform)));

  switch (instr->Mask(SVESaturatingIncDecVectorByElementCountMask)) {
    case SQDECD_z_zs:
    case SQDECH_z_zs:
    case SQDECW_z_zs:
      sub(vform, zd, zd, scratch).SignedSaturate(vform);
      break;
    case SQINCD_z_zs:
    case SQINCH_z_zs:
    case SQINCW_z_zs:
      add(vform, zd, zd, scratch).SignedSaturate(vform);
      break;
    case UQDECD_z_zs:
    case UQDECH_z_zs:
    case UQDECW_z_zs:
      sub(vform, zd, zd, scratch).UnsignedSaturate(vform);
      break;
    case UQINCD_z_zs:
    case UQINCH_z_zs:
    case UQINCW_z_zs:
      add(vform, zd, zd, scratch).UnsignedSaturate(vform);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEElementCount(const Instruction* instr) {
  switch (instr->Mask(SVEElementCountMask)) {
    case CNTB_r_s:
    case CNTD_r_s:
    case CNTH_r_s:
    case CNTW_r_s:
      // All handled below.
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  // Although the instructions are separated, the lane size is encoded in the
  // same way as most other SVE instructions.
  VectorFormat vform = instr->GetSVEVectorFormat();

  int pattern = instr->GetImmSVEPredicateConstraint();
  int count = GetPredicateConstraintLaneCount(vform, pattern);
  int multiplier = instr->ExtractBits(19, 16) + 1;
  WriteXRegister(instr->GetRd(), count * multiplier);
}

void Simulator::VisitSVEFPAccumulatingReduction(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& vdn = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRn());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());

  if (vform == kFormatVnB) VIXL_UNIMPLEMENTED();

  switch (instr->Mask(SVEFPAccumulatingReductionMask)) {
    case FADDA_v_p_z:
      fadda(vform, vdn, pg, zm);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEFPArithmetic_Predicated(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zdn = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRn());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());

  if (vform == kFormatVnB) VIXL_UNIMPLEMENTED();

  SimVRegister result;
  switch (instr->Mask(SVEFPArithmetic_PredicatedMask)) {
    case FABD_z_p_zz:
      fabd(vform, result, zdn, zm);
      break;
    case FADD_z_p_zz:
      fadd(vform, result, zdn, zm);
      break;
    case FDIVR_z_p_zz:
      fdiv(vform, result, zm, zdn);
      break;
    case FDIV_z_p_zz:
      fdiv(vform, result, zdn, zm);
      break;
    case FMAXNM_z_p_zz:
      fmaxnm(vform, result, zdn, zm);
      break;
    case FMAX_z_p_zz:
      fmax(vform, result, zdn, zm);
      break;
    case FMINNM_z_p_zz:
      fminnm(vform, result, zdn, zm);
      break;
    case FMIN_z_p_zz:
      fmin(vform, result, zdn, zm);
      break;
    case FMULX_z_p_zz:
      fmulx(vform, result, zdn, zm);
      break;
    case FMUL_z_p_zz:
      fmul(vform, result, zdn, zm);
      break;
    case FSCALE_z_p_zz:
      fscale(vform, result, zdn, zm);
      break;
    case FSUBR_z_p_zz:
      fsub(vform, result, zm, zdn);
      break;
    case FSUB_z_p_zz:
      fsub(vform, result, zdn, zm);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
  mov_merging(vform, zdn, pg, result);
}

void Simulator::VisitSVEFPArithmeticWithImm_Predicated(
    const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  if (LaneSizeInBitsFromFormat(vform) == kBRegSize) {
    VIXL_UNIMPLEMENTED();
  }

  SimVRegister& zdn = ReadVRegister(instr->GetRd());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  SimVRegister result;

  int i1 = instr->ExtractBit(5);
  SimVRegister add_sub_imm, min_max_imm, mul_imm;
  uint64_t half = FPToRawbitsWithSize(LaneSizeInBitsFromFormat(vform), 0.5);
  uint64_t one = FPToRawbitsWithSize(LaneSizeInBitsFromFormat(vform), 1.0);
  uint64_t two = FPToRawbitsWithSize(LaneSizeInBitsFromFormat(vform), 2.0);
  dup_immediate(vform, add_sub_imm, i1 ? one : half);
  dup_immediate(vform, min_max_imm, i1 ? one : 0);
  dup_immediate(vform, mul_imm, i1 ? two : half);

  switch (instr->Mask(SVEFPArithmeticWithImm_PredicatedMask)) {
    case FADD_z_p_zs:
      fadd(vform, result, zdn, add_sub_imm);
      break;
    case FMAXNM_z_p_zs:
      fmaxnm(vform, result, zdn, min_max_imm);
      break;
    case FMAX_z_p_zs:
      fmax(vform, result, zdn, min_max_imm);
      break;
    case FMINNM_z_p_zs:
      fminnm(vform, result, zdn, min_max_imm);
      break;
    case FMIN_z_p_zs:
      fmin(vform, result, zdn, min_max_imm);
      break;
    case FMUL_z_p_zs:
      fmul(vform, result, zdn, mul_imm);
      break;
    case FSUBR_z_p_zs:
      fsub(vform, result, add_sub_imm, zdn);
      break;
    case FSUB_z_p_zs:
      fsub(vform, result, zdn, add_sub_imm);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
  mov_merging(vform, zdn, pg, result);
}

void Simulator::VisitSVEFPTrigMulAddCoefficient(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRn());

  if (vform == kFormatVnB) VIXL_UNIMPLEMENTED();

  switch (instr->Mask(SVEFPTrigMulAddCoefficientMask)) {
    case FTMAD_z_zzi:
      ftmad(vform, zd, zd, zm, instr->ExtractBits(18, 16));
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEFPArithmeticUnpredicated(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimVRegister& zm = ReadVRegister(instr->GetRm());

  if (vform == kFormatVnB) VIXL_UNIMPLEMENTED();

  switch (instr->Mask(SVEFPArithmeticUnpredicatedMask)) {
    case FADD_z_zz:
      fadd(vform, zd, zn, zm);
      break;
    case FMUL_z_zz:
      fmul(vform, zd, zn, zm);
      break;
    case FRECPS_z_zz:
      frecps(vform, zd, zn, zm);
      break;
    case FRSQRTS_z_zz:
      frsqrts(vform, zd, zn, zm);
      break;
    case FSUB_z_zz:
      fsub(vform, zd, zn, zm);
      break;
    case FTSMUL_z_zz:
      ftsmul(vform, zd, zn, zm);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEFPCompareVectors(const Instruction* instr) {
  SimPRegister& pd = ReadPRegister(instr->GetPd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimVRegister& zm = ReadVRegister(instr->GetRm());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister result;

  if (vform == kFormatVnB) VIXL_UNIMPLEMENTED();

  switch (instr->Mask(SVEFPCompareVectorsMask)) {
    case FACGE_p_p_zz:
      fabscmp(vform, result, zn, zm, ge);
      break;
    case FACGT_p_p_zz:
      fabscmp(vform, result, zn, zm, gt);
      break;
    case FCMEQ_p_p_zz:
      fcmp(vform, result, zn, zm, eq);
      break;
    case FCMGE_p_p_zz:
      fcmp(vform, result, zn, zm, ge);
      break;
    case FCMGT_p_p_zz:
      fcmp(vform, result, zn, zm, gt);
      break;
    case FCMNE_p_p_zz:
      fcmp(vform, result, zn, zm, ne);
      break;
    case FCMUO_p_p_zz:
      fcmp(vform, result, zn, zm, uo);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  ExtractFromSimVRegister(vform, pd, result);
  mov_zeroing(pd, pg, pd);
}

void Simulator::VisitSVEFPCompareWithZero(const Instruction* instr) {
  SimPRegister& pd = ReadPRegister(instr->GetPd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  VectorFormat vform = instr->GetSVEVectorFormat();

  if (vform == kFormatVnB) VIXL_UNIMPLEMENTED();

  SimVRegister result;
  SimVRegister zeros;
  dup_immediate(kFormatVnD, zeros, 0);

  switch (instr->Mask(SVEFPCompareWithZeroMask)) {
    case FCMEQ_p_p_z0:
      fcmp(vform, result, zn, zeros, eq);
      break;
    case FCMGE_p_p_z0:
      fcmp(vform, result, zn, zeros, ge);
      break;
    case FCMGT_p_p_z0:
      fcmp(vform, result, zn, zeros, gt);
      break;
    case FCMLE_p_p_z0:
      fcmp(vform, result, zn, zeros, le);
      break;
    case FCMLT_p_p_z0:
      fcmp(vform, result, zn, zeros, lt);
      break;
    case FCMNE_p_p_z0:
      fcmp(vform, result, zn, zeros, ne);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  ExtractFromSimVRegister(vform, pd, result);
  mov_zeroing(pd, pg, pd);
}

void Simulator::VisitSVEFPComplexAddition(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();

  if (LaneSizeInBitsFromFormat(vform) == kBRegSize) {
    VIXL_UNIMPLEMENTED();
  }

  SimVRegister& zdn = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRn());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  int rot = instr->ExtractBit(16);

  SimVRegister result;

  switch (instr->Mask(SVEFPComplexAdditionMask)) {
    case FCADD_z_p_zz:
      fcadd(vform, result, zdn, zm, rot);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
  mov_merging(vform, zdn, pg, result);
}

void Simulator::VisitSVEFPComplexMulAdd(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();

  if (LaneSizeInBitsFromFormat(vform) == kBRegSize) {
    VIXL_UNIMPLEMENTED();
  }

  SimVRegister& zda = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimVRegister& zm = ReadVRegister(instr->GetRm());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  int rot = instr->ExtractBits(14, 13);

  SimVRegister result;

  switch (instr->Mask(SVEFPComplexMulAddMask)) {
    case FCMLA_z_p_zzz:
      fcmla(vform, result, zn, zm, zda, rot);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
  mov_merging(vform, zda, pg, result);
}

void Simulator::VisitSVEFPComplexMulAddIndex(const Instruction* instr) {
  SimVRegister& zda = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  int rot = instr->ExtractBits(11, 10);
  unsigned zm_code = instr->GetRm();
  int index = -1;
  VectorFormat vform, vform_dup;

  switch (instr->Mask(SVEFPComplexMulAddIndexMask)) {
    case FCMLA_z_zzzi_h:
      vform = kFormatVnH;
      vform_dup = kFormatVnS;
      index = zm_code >> 3;
      zm_code &= 0x7;
      break;
    case FCMLA_z_zzzi_s:
      vform = kFormatVnS;
      vform_dup = kFormatVnD;
      index = zm_code >> 4;
      zm_code &= 0xf;
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  if (index >= 0) {
    SimVRegister temp;
    dup_elements_to_segments(vform_dup, temp, ReadVRegister(zm_code), index);
    fcmla(vform, zda, zn, temp, zda, rot);
  }
}

typedef LogicVRegister (Simulator::*FastReduceFn)(VectorFormat vform,
                                                  LogicVRegister dst,
                                                  const LogicVRegister& src);

void Simulator::VisitSVEFPFastReduction(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& vd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  int lane_size = LaneSizeInBitsFromFormat(vform);

  uint64_t inactive_value = 0;
  FastReduceFn fn = nullptr;

  if (vform == kFormatVnB) VIXL_UNIMPLEMENTED();

  switch (instr->Mask(SVEFPFastReductionMask)) {
    case FADDV_v_p_z:
      fn = &Simulator::faddv;
      break;
    case FMAXNMV_v_p_z:
      inactive_value = FPToRawbitsWithSize(lane_size, kFP64DefaultNaN);
      fn = &Simulator::fmaxnmv;
      break;
    case FMAXV_v_p_z:
      inactive_value = FPToRawbitsWithSize(lane_size, kFP64NegativeInfinity);
      fn = &Simulator::fmaxv;
      break;
    case FMINNMV_v_p_z:
      inactive_value = FPToRawbitsWithSize(lane_size, kFP64DefaultNaN);
      fn = &Simulator::fminnmv;
      break;
    case FMINV_v_p_z:
      inactive_value = FPToRawbitsWithSize(lane_size, kFP64PositiveInfinity);
      fn = &Simulator::fminv;
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  SimVRegister scratch;
  dup_immediate(vform, scratch, inactive_value);
  mov_merging(vform, scratch, pg, zn);
  if (fn != nullptr) (this->*fn)(vform, vd, scratch);
}

void Simulator::VisitSVEFPMulIndex(const Instruction* instr) {
  VectorFormat vform = kFormatUndefined;

  switch (instr->Mask(SVEFPMulIndexMask)) {
    case FMUL_z_zzi_d:
      vform = kFormatVnD;
      break;
    case FMUL_z_zzi_h_i3h:
    case FMUL_z_zzi_h:
      vform = kFormatVnH;
      break;
    case FMUL_z_zzi_s:
      vform = kFormatVnS;
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimVRegister temp;

  dup_elements_to_segments(vform, temp, instr->GetSVEMulZmAndIndex());
  fmul(vform, zd, zn, temp);
}

void Simulator::VisitSVEFPMulAdd(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  SimVRegister result;

  if (vform == kFormatVnB) VIXL_UNIMPLEMENTED();

  if (instr->ExtractBit(15) == 0) {
    // Floating-point multiply-accumulate writing addend.
    SimVRegister& zm = ReadVRegister(instr->GetRm());
    SimVRegister& zn = ReadVRegister(instr->GetRn());

    switch (instr->Mask(SVEFPMulAddMask)) {
      // zda = zda + zn * zm
      case FMLA_z_p_zzz:
        fmla(vform, result, zd, zn, zm);
        break;
      // zda = -zda + -zn * zm
      case FNMLA_z_p_zzz:
        fneg(vform, result, zd);
        fmls(vform, result, result, zn, zm);
        break;
      // zda = zda + -zn * zm
      case FMLS_z_p_zzz:
        fmls(vform, result, zd, zn, zm);
        break;
      // zda = -zda + zn * zm
      case FNMLS_z_p_zzz:
        fneg(vform, result, zd);
        fmla(vform, result, result, zn, zm);
        break;
      default:
        VIXL_UNIMPLEMENTED();
        break;
    }
  } else {
    // Floating-point multiply-accumulate writing multiplicand.
    SimVRegister& za = ReadVRegister(instr->GetRm());
    SimVRegister& zm = ReadVRegister(instr->GetRn());

    switch (instr->Mask(SVEFPMulAddMask)) {
      // zdn = za + zdn * zm
      case FMAD_z_p_zzz:
        fmla(vform, result, za, zd, zm);
        break;
      // zdn = -za + -zdn * zm
      case FNMAD_z_p_zzz:
        fneg(vform, result, za);
        fmls(vform, result, result, zd, zm);
        break;
      // zdn = za + -zdn * zm
      case FMSB_z_p_zzz:
        fmls(vform, result, za, zd, zm);
        break;
      // zdn = -za + zdn * zm
      case FNMSB_z_p_zzz:
        fneg(vform, result, za);
        fmla(vform, result, result, zd, zm);
        break;
      default:
        VIXL_UNIMPLEMENTED();
        break;
    }
  }

  mov_merging(vform, zd, pg, result);
}

void Simulator::VisitSVEFPMulAddIndex(const Instruction* instr) {
  VectorFormat vform = kFormatUndefined;

  switch (instr->Mask(SVEFPMulAddIndexMask)) {
    case FMLA_z_zzzi_d:
    case FMLS_z_zzzi_d:
      vform = kFormatVnD;
      break;
    case FMLA_z_zzzi_s:
    case FMLS_z_zzzi_s:
      vform = kFormatVnS;
      break;
    case FMLA_z_zzzi_h:
    case FMLS_z_zzzi_h:
    case FMLA_z_zzzi_h_i3h:
    case FMLS_z_zzzi_h_i3h:
      vform = kFormatVnH;
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimVRegister temp;

  dup_elements_to_segments(vform, temp, instr->GetSVEMulZmAndIndex());
  if (instr->ExtractBit(10) == 1) {
    fmls(vform, zd, zd, zn, temp);
  } else {
    fmla(vform, zd, zd, zn, temp);
  }
}

void Simulator::VisitSVEFPConvertToInt(const Instruction* instr) {
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  int dst_data_size;
  int src_data_size;

  switch (instr->Mask(SVEFPConvertToIntMask)) {
    case FCVTZS_z_p_z_d2w:
    case FCVTZU_z_p_z_d2w:
      dst_data_size = kSRegSize;
      src_data_size = kDRegSize;
      break;
    case FCVTZS_z_p_z_d2x:
    case FCVTZU_z_p_z_d2x:
      dst_data_size = kDRegSize;
      src_data_size = kDRegSize;
      break;
    case FCVTZS_z_p_z_fp162h:
    case FCVTZU_z_p_z_fp162h:
      dst_data_size = kHRegSize;
      src_data_size = kHRegSize;
      break;
    case FCVTZS_z_p_z_fp162w:
    case FCVTZU_z_p_z_fp162w:
      dst_data_size = kSRegSize;
      src_data_size = kHRegSize;
      break;
    case FCVTZS_z_p_z_fp162x:
    case FCVTZU_z_p_z_fp162x:
      dst_data_size = kDRegSize;
      src_data_size = kHRegSize;
      break;
    case FCVTZS_z_p_z_s2w:
    case FCVTZU_z_p_z_s2w:
      dst_data_size = kSRegSize;
      src_data_size = kSRegSize;
      break;
    case FCVTZS_z_p_z_s2x:
    case FCVTZU_z_p_z_s2x:
      dst_data_size = kDRegSize;
      src_data_size = kSRegSize;
      break;
    default:
      VIXL_UNIMPLEMENTED();
      dst_data_size = 0;
      src_data_size = 0;
      break;
  }

  VectorFormat vform =
      SVEFormatFromLaneSizeInBits(std::max(dst_data_size, src_data_size));

  if (instr->ExtractBit(16) == 0) {
    fcvts(vform, dst_data_size, src_data_size, zd, pg, zn, FPZero);
  } else {
    fcvtu(vform, dst_data_size, src_data_size, zd, pg, zn, FPZero);
  }
}

void Simulator::VisitSVEFPConvertPrecision(const Instruction* instr) {
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  VectorFormat dst_data_size = kFormatUndefined;
  VectorFormat src_data_size = kFormatUndefined;

  switch (instr->Mask(SVEFPConvertPrecisionMask)) {
    case FCVT_z_p_z_d2h:
      dst_data_size = kFormatVnH;
      src_data_size = kFormatVnD;
      break;
    case FCVT_z_p_z_d2s:
      dst_data_size = kFormatVnS;
      src_data_size = kFormatVnD;
      break;
    case FCVT_z_p_z_h2d:
      dst_data_size = kFormatVnD;
      src_data_size = kFormatVnH;
      break;
    case FCVT_z_p_z_h2s:
      dst_data_size = kFormatVnS;
      src_data_size = kFormatVnH;
      break;
    case FCVT_z_p_z_s2d:
      dst_data_size = kFormatVnD;
      src_data_size = kFormatVnS;
      break;
    case FCVT_z_p_z_s2h:
      dst_data_size = kFormatVnH;
      src_data_size = kFormatVnS;
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  fcvt(dst_data_size, src_data_size, zd, pg, zn);
}

void Simulator::VisitSVEFPUnaryOp(const Instruction* instr) {
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister result;

  switch (instr->Mask(SVEFPUnaryOpMask)) {
    case FRECPX_z_p_z:
      frecpx(vform, result, zn);
      break;
    case FSQRT_z_p_z:
      fsqrt(vform, result, zn);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
  mov_merging(vform, zd, pg, result);
}

void Simulator::VisitSVEFPRoundToIntegralValue(const Instruction* instr) {
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  VectorFormat vform = instr->GetSVEVectorFormat();
  FPRounding fpcr_rounding = static_cast<FPRounding>(ReadFpcr().GetRMode());
  bool exact_exception = false;

  if (vform == kFormatVnB) VIXL_UNIMPLEMENTED();

  switch (instr->Mask(SVEFPRoundToIntegralValueMask)) {
    case FRINTA_z_p_z:
      fpcr_rounding = FPTieAway;
      break;
    case FRINTI_z_p_z:
      break;  // Use FPCR rounding mode.
    case FRINTM_z_p_z:
      fpcr_rounding = FPNegativeInfinity;
      break;
    case FRINTN_z_p_z:
      fpcr_rounding = FPTieEven;
      break;
    case FRINTP_z_p_z:
      fpcr_rounding = FPPositiveInfinity;
      break;
    case FRINTX_z_p_z:
      exact_exception = true;
      break;
    case FRINTZ_z_p_z:
      fpcr_rounding = FPZero;
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  SimVRegister result;
  frint(vform, result, zn, fpcr_rounding, exact_exception, kFrintToInteger);
  mov_merging(vform, zd, pg, result);
}

void Simulator::VisitSVEIntConvertToFP(const Instruction* instr) {
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  FPRounding fpcr_rounding = static_cast<FPRounding>(ReadFpcr().GetRMode());
  int dst_data_size;
  int src_data_size;

  switch (instr->Mask(SVEIntConvertToFPMask)) {
    case SCVTF_z_p_z_h2fp16:
    case UCVTF_z_p_z_h2fp16:
      dst_data_size = kHRegSize;
      src_data_size = kHRegSize;
      break;
    case SCVTF_z_p_z_w2d:
    case UCVTF_z_p_z_w2d:
      dst_data_size = kDRegSize;
      src_data_size = kSRegSize;
      break;
    case SCVTF_z_p_z_w2fp16:
    case UCVTF_z_p_z_w2fp16:
      dst_data_size = kHRegSize;
      src_data_size = kSRegSize;
      break;
    case SCVTF_z_p_z_w2s:
    case UCVTF_z_p_z_w2s:
      dst_data_size = kSRegSize;
      src_data_size = kSRegSize;
      break;
    case SCVTF_z_p_z_x2d:
    case UCVTF_z_p_z_x2d:
      dst_data_size = kDRegSize;
      src_data_size = kDRegSize;
      break;
    case SCVTF_z_p_z_x2fp16:
    case UCVTF_z_p_z_x2fp16:
      dst_data_size = kHRegSize;
      src_data_size = kDRegSize;
      break;
    case SCVTF_z_p_z_x2s:
    case UCVTF_z_p_z_x2s:
      dst_data_size = kSRegSize;
      src_data_size = kDRegSize;
      break;
    default:
      VIXL_UNIMPLEMENTED();
      dst_data_size = 0;
      src_data_size = 0;
      break;
  }

  VectorFormat vform =
      SVEFormatFromLaneSizeInBits(std::max(dst_data_size, src_data_size));

  if (instr->ExtractBit(16) == 0) {
    scvtf(vform, dst_data_size, src_data_size, zd, pg, zn, fpcr_rounding);
  } else {
    ucvtf(vform, dst_data_size, src_data_size, zd, pg, zn, fpcr_rounding);
  }
}

void Simulator::VisitSVEFPUnaryOpUnpredicated(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  FPRounding fpcr_rounding = static_cast<FPRounding>(ReadFpcr().GetRMode());

  if (vform == kFormatVnB) VIXL_UNIMPLEMENTED();

  switch (instr->Mask(SVEFPUnaryOpUnpredicatedMask)) {
    case FRECPE_z_z:
      frecpe(vform, zd, zn, fpcr_rounding);
      break;
    case FRSQRTE_z_z:
      frsqrte(vform, zd, zn);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEIncDecByPredicateCount(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimPRegister& pg = ReadPRegister(instr->ExtractBits(8, 5));

  int count = CountActiveLanes(vform, pg);

  if (instr->ExtractBit(11) == 0) {
    SimVRegister& zdn = ReadVRegister(instr->GetRd());
    switch (instr->Mask(SVEIncDecByPredicateCountMask)) {
      case DECP_z_p_z:
        sub_uint(vform, zdn, zdn, count);
        break;
      case INCP_z_p_z:
        add_uint(vform, zdn, zdn, count);
        break;
      case SQDECP_z_p_z:
        sub_uint(vform, zdn, zdn, count).SignedSaturate(vform);
        break;
      case SQINCP_z_p_z:
        add_uint(vform, zdn, zdn, count).SignedSaturate(vform);
        break;
      case UQDECP_z_p_z:
        sub_uint(vform, zdn, zdn, count).UnsignedSaturate(vform);
        break;
      case UQINCP_z_p_z:
        add_uint(vform, zdn, zdn, count).UnsignedSaturate(vform);
        break;
      default:
        VIXL_UNIMPLEMENTED();
        break;
    }
  } else {
    bool is_saturating = (instr->ExtractBit(18) == 0);
    bool decrement =
        is_saturating ? instr->ExtractBit(17) : instr->ExtractBit(16);
    bool is_signed = (instr->ExtractBit(16) == 0);
    bool sf = is_saturating ? (instr->ExtractBit(10) != 0) : true;
    unsigned width = sf ? kXRegSize : kWRegSize;

    switch (instr->Mask(SVEIncDecByPredicateCountMask)) {
      case DECP_r_p_r:
      case INCP_r_p_r:
      case SQDECP_r_p_r_sx:
      case SQDECP_r_p_r_x:
      case SQINCP_r_p_r_sx:
      case SQINCP_r_p_r_x:
      case UQDECP_r_p_r_uw:
      case UQDECP_r_p_r_x:
      case UQINCP_r_p_r_uw:
      case UQINCP_r_p_r_x:
        WriteXRegister(instr->GetRd(),
                       IncDecN(ReadXRegister(instr->GetRd()),
                               decrement ? -count : count,
                               width,
                               is_saturating,
                               is_signed));
        break;
      default:
        VIXL_UNIMPLEMENTED();
        break;
    }
  }
}

uint64_t Simulator::IncDecN(uint64_t acc,
                            int64_t delta,
                            unsigned n,
                            bool is_saturating,
                            bool is_signed) {
  VIXL_ASSERT(n <= 64);
  VIXL_ASSERT(IsIntN(n, delta));

  uint64_t sign_mask = UINT64_C(1) << (n - 1);
  uint64_t mask = GetUintMask(n);

  acc &= mask;  // Ignore initial accumulator high bits.
  uint64_t result = (acc + delta) & mask;

  bool result_negative = ((result & sign_mask) != 0);

  if (is_saturating) {
    if (is_signed) {
      bool acc_negative = ((acc & sign_mask) != 0);
      bool delta_negative = delta < 0;

      // If the signs of the operands are the same, but different from the
      // result, there was an overflow.
      if ((acc_negative == delta_negative) &&
          (acc_negative != result_negative)) {
        if (result_negative) {
          // Saturate to [..., INT<n>_MAX].
          result_negative = false;
          result = mask & ~sign_mask;  // E.g. 0x000000007fffffff
        } else {
          // Saturate to [INT<n>_MIN, ...].
          result_negative = true;
          result = ~mask | sign_mask;  // E.g. 0xffffffff80000000
        }
      }
    } else {
      if ((delta < 0) && (result > acc)) {
        // Saturate to [0, ...].
        result = 0;
      } else if ((delta > 0) && (result < acc)) {
        // Saturate to [..., UINT<n>_MAX].
        result = mask;
      }
    }
  }

  // Sign-extend if necessary.
  if (result_negative && is_signed) result |= ~mask;

  return result;
}

void Simulator::VisitSVEIndexGeneration(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  switch (instr->Mask(SVEIndexGenerationMask)) {
    case INDEX_z_ii:
    case INDEX_z_ir:
    case INDEX_z_ri:
    case INDEX_z_rr: {
      uint64_t start = instr->ExtractBit(10) ? ReadXRegister(instr->GetRn())
                                             : instr->ExtractSignedBits(9, 5);
      uint64_t step = instr->ExtractBit(11) ? ReadXRegister(instr->GetRm())
                                            : instr->ExtractSignedBits(20, 16);
      index(vform, zd, start, step);
      break;
    }
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEIntArithmeticUnpredicated(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimVRegister& zm = ReadVRegister(instr->GetRm());
  switch (instr->Mask(SVEIntArithmeticUnpredicatedMask)) {
    case ADD_z_zz:
      add(vform, zd, zn, zm);
      break;
    case SQADD_z_zz:
      add(vform, zd, zn, zm).SignedSaturate(vform);
      break;
    case SQSUB_z_zz:
      sub(vform, zd, zn, zm).SignedSaturate(vform);
      break;
    case SUB_z_zz:
      sub(vform, zd, zn, zm);
      break;
    case UQADD_z_zz:
      add(vform, zd, zn, zm).UnsignedSaturate(vform);
      break;
    case UQSUB_z_zz:
      sub(vform, zd, zn, zm).UnsignedSaturate(vform);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEIntAddSubtractVectors_Predicated(
    const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zdn = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRn());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  SimVRegister result;

  switch (instr->Mask(SVEIntAddSubtractVectors_PredicatedMask)) {
    case ADD_z_p_zz:
      add(vform, result, zdn, zm);
      break;
    case SUBR_z_p_zz:
      sub(vform, result, zm, zdn);
      break;
    case SUB_z_p_zz:
      sub(vform, result, zdn, zm);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
  mov_merging(vform, zdn, pg, result);
}

void Simulator::VisitSVEBitwiseLogical_Predicated(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zdn = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRn());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  SimVRegister result;

  switch (instr->Mask(SVEBitwiseLogical_PredicatedMask)) {
    case AND_z_p_zz:
      SVEBitwiseLogicalUnpredicatedHelper(AND, vform, result, zdn, zm);
      break;
    case BIC_z_p_zz:
      SVEBitwiseLogicalUnpredicatedHelper(BIC, vform, result, zdn, zm);
      break;
    case EOR_z_p_zz:
      SVEBitwiseLogicalUnpredicatedHelper(EOR, vform, result, zdn, zm);
      break;
    case ORR_z_p_zz:
      SVEBitwiseLogicalUnpredicatedHelper(ORR, vform, result, zdn, zm);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
  mov_merging(vform, zdn, pg, result);
}

void Simulator::VisitSVEIntMulVectors_Predicated(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zdn = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRn());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  SimVRegister result;

  switch (instr->Mask(SVEIntMulVectors_PredicatedMask)) {
    case MUL_z_p_zz:
      mul(vform, result, zdn, zm);
      break;
    case SMULH_z_p_zz:
      smulh(vform, result, zdn, zm);
      break;
    case UMULH_z_p_zz:
      umulh(vform, result, zdn, zm);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
  mov_merging(vform, zdn, pg, result);
}

void Simulator::VisitSVEIntMinMaxDifference_Predicated(
    const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zdn = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRn());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  SimVRegister result;

  switch (instr->Mask(SVEIntMinMaxDifference_PredicatedMask)) {
    case SABD_z_p_zz:
      absdiff(vform, result, zdn, zm, true);
      break;
    case SMAX_z_p_zz:
      smax(vform, result, zdn, zm);
      break;
    case SMIN_z_p_zz:
      smin(vform, result, zdn, zm);
      break;
    case UABD_z_p_zz:
      absdiff(vform, result, zdn, zm, false);
      break;
    case UMAX_z_p_zz:
      umax(vform, result, zdn, zm);
      break;
    case UMIN_z_p_zz:
      umin(vform, result, zdn, zm);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
  mov_merging(vform, zdn, pg, result);
}

void Simulator::VisitSVEIntMulImm_Unpredicated(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister scratch;

  switch (instr->Mask(SVEIntMulImm_UnpredicatedMask)) {
    case MUL_z_zi:
      dup_immediate(vform, scratch, instr->GetImmSVEIntWideSigned());
      mul(vform, zd, zd, scratch);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEIntDivideVectors_Predicated(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zdn = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRn());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  SimVRegister result;

  VIXL_ASSERT((vform == kFormatVnS) || (vform == kFormatVnD));

  switch (instr->Mask(SVEIntDivideVectors_PredicatedMask)) {
    case SDIVR_z_p_zz:
      sdiv(vform, result, zm, zdn);
      break;
    case SDIV_z_p_zz:
      sdiv(vform, result, zdn, zm);
      break;
    case UDIVR_z_p_zz:
      udiv(vform, result, zm, zdn);
      break;
    case UDIV_z_p_zz:
      udiv(vform, result, zdn, zm);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
  mov_merging(vform, zdn, pg, result);
}

void Simulator::VisitSVEIntMinMaxImm_Unpredicated(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister scratch;

  uint64_t unsigned_imm = instr->GetImmSVEIntWideUnsigned();
  int64_t signed_imm = instr->GetImmSVEIntWideSigned();

  switch (instr->Mask(SVEIntMinMaxImm_UnpredicatedMask)) {
    case SMAX_z_zi:
      dup_immediate(vform, scratch, signed_imm);
      smax(vform, zd, zd, scratch);
      break;
    case SMIN_z_zi:
      dup_immediate(vform, scratch, signed_imm);
      smin(vform, zd, zd, scratch);
      break;
    case UMAX_z_zi:
      dup_immediate(vform, scratch, unsigned_imm);
      umax(vform, zd, zd, scratch);
      break;
    case UMIN_z_zi:
      dup_immediate(vform, scratch, unsigned_imm);
      umin(vform, zd, zd, scratch);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEIntCompareScalarCountAndLimit(
    const Instruction* instr) {
  unsigned rn_code = instr->GetRn();
  unsigned rm_code = instr->GetRm();
  SimPRegister& pd = ReadPRegister(instr->GetPd());
  VectorFormat vform = instr->GetSVEVectorFormat();

  bool is_64_bit = instr->ExtractBit(12) == 1;
  int rsize = is_64_bit ? kXRegSize : kWRegSize;
  uint64_t mask = is_64_bit ? kXRegMask : kWRegMask;

  uint64_t usrc1 = ReadXRegister(rn_code);
  int64_t ssrc2 = is_64_bit ? ReadXRegister(rm_code) : ReadWRegister(rm_code);
  uint64_t usrc2 = ssrc2 & mask;

  bool reverse = (form_hash_ == "whilege_p_p_rr"_h) ||
                 (form_hash_ == "whilegt_p_p_rr"_h) ||
                 (form_hash_ == "whilehi_p_p_rr"_h) ||
                 (form_hash_ == "whilehs_p_p_rr"_h);

  int lane_count = LaneCountFromFormat(vform);
  bool last = true;
  for (int i = 0; i < lane_count; i++) {
    usrc1 &= mask;
    int64_t ssrc1 = ExtractSignedBitfield64(rsize - 1, 0, usrc1);

    bool cond = false;
    switch (form_hash_) {
      case "whilele_p_p_rr"_h:
        cond = ssrc1 <= ssrc2;
        break;
      case "whilelo_p_p_rr"_h:
        cond = usrc1 < usrc2;
        break;
      case "whilels_p_p_rr"_h:
        cond = usrc1 <= usrc2;
        break;
      case "whilelt_p_p_rr"_h:
        cond = ssrc1 < ssrc2;
        break;
      case "whilege_p_p_rr"_h:
        cond = ssrc1 >= ssrc2;
        break;
      case "whilegt_p_p_rr"_h:
        cond = ssrc1 > ssrc2;
        break;
      case "whilehi_p_p_rr"_h:
        cond = usrc1 > usrc2;
        break;
      case "whilehs_p_p_rr"_h:
        cond = usrc1 >= usrc2;
        break;
      default:
        VIXL_UNIMPLEMENTED();
        break;
    }
    last = last && cond;
    LogicPRegister dst(pd);
    int lane = reverse ? ((lane_count - 1) - i) : i;
    dst.SetActive(vform, lane, last);
    usrc1 += reverse ? -1 : 1;
  }

  PredTest(vform, GetPTrue(), pd);
  LogSystemRegister(NZCV);
}

void Simulator::VisitSVEConditionallyTerminateScalars(
    const Instruction* instr) {
  unsigned rn_code = instr->GetRn();
  unsigned rm_code = instr->GetRm();
  bool is_64_bit = instr->ExtractBit(22) == 1;
  uint64_t src1 = is_64_bit ? ReadXRegister(rn_code) : ReadWRegister(rn_code);
  uint64_t src2 = is_64_bit ? ReadXRegister(rm_code) : ReadWRegister(rm_code);
  bool term = false;
  switch (instr->Mask(SVEConditionallyTerminateScalarsMask)) {
    case CTERMEQ_rr:
      term = src1 == src2;
      break;
    case CTERMNE_rr:
      term = src1 != src2;
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
  ReadNzcv().SetN(term ? 1 : 0);
  ReadNzcv().SetV(term ? 0 : !ReadC());
  LogSystemRegister(NZCV);
}

void Simulator::VisitSVEIntCompareSignedImm(const Instruction* instr) {
  bool commute_inputs = false;
  Condition cond = al;
  switch (instr->Mask(SVEIntCompareSignedImmMask)) {
    case CMPEQ_p_p_zi:
      cond = eq;
      break;
    case CMPGE_p_p_zi:
      cond = ge;
      break;
    case CMPGT_p_p_zi:
      cond = gt;
      break;
    case CMPLE_p_p_zi:
      cond = ge;
      commute_inputs = true;
      break;
    case CMPLT_p_p_zi:
      cond = gt;
      commute_inputs = true;
      break;
    case CMPNE_p_p_zi:
      cond = ne;
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister src2;
  dup_immediate(vform,
                src2,
                ExtractSignedBitfield64(4, 0, instr->ExtractBits(20, 16)));
  SVEIntCompareVectorsHelper(cond,
                             vform,
                             ReadPRegister(instr->GetPd()),
                             ReadPRegister(instr->GetPgLow8()),
                             commute_inputs ? src2
                                            : ReadVRegister(instr->GetRn()),
                             commute_inputs ? ReadVRegister(instr->GetRn())
                                            : src2);
}

void Simulator::VisitSVEIntCompareUnsignedImm(const Instruction* instr) {
  bool commute_inputs = false;
  Condition cond = al;
  switch (instr->Mask(SVEIntCompareUnsignedImmMask)) {
    case CMPHI_p_p_zi:
      cond = hi;
      break;
    case CMPHS_p_p_zi:
      cond = hs;
      break;
    case CMPLO_p_p_zi:
      cond = hi;
      commute_inputs = true;
      break;
    case CMPLS_p_p_zi:
      cond = hs;
      commute_inputs = true;
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister src2;
  dup_immediate(vform, src2, instr->ExtractBits(20, 14));
  SVEIntCompareVectorsHelper(cond,
                             vform,
                             ReadPRegister(instr->GetPd()),
                             ReadPRegister(instr->GetPgLow8()),
                             commute_inputs ? src2
                                            : ReadVRegister(instr->GetRn()),
                             commute_inputs ? ReadVRegister(instr->GetRn())
                                            : src2);
}

void Simulator::VisitSVEIntCompareVectors(const Instruction* instr) {
  Instr op = instr->Mask(SVEIntCompareVectorsMask);
  bool is_wide_elements = false;
  switch (op) {
    case CMPEQ_p_p_zw:
    case CMPGE_p_p_zw:
    case CMPGT_p_p_zw:
    case CMPHI_p_p_zw:
    case CMPHS_p_p_zw:
    case CMPLE_p_p_zw:
    case CMPLO_p_p_zw:
    case CMPLS_p_p_zw:
    case CMPLT_p_p_zw:
    case CMPNE_p_p_zw:
      is_wide_elements = true;
      break;
  }

  Condition cond;
  switch (op) {
    case CMPEQ_p_p_zw:
    case CMPEQ_p_p_zz:
      cond = eq;
      break;
    case CMPGE_p_p_zw:
    case CMPGE_p_p_zz:
      cond = ge;
      break;
    case CMPGT_p_p_zw:
    case CMPGT_p_p_zz:
      cond = gt;
      break;
    case CMPHI_p_p_zw:
    case CMPHI_p_p_zz:
      cond = hi;
      break;
    case CMPHS_p_p_zw:
    case CMPHS_p_p_zz:
      cond = hs;
      break;
    case CMPNE_p_p_zw:
    case CMPNE_p_p_zz:
      cond = ne;
      break;
    case CMPLE_p_p_zw:
      cond = le;
      break;
    case CMPLO_p_p_zw:
      cond = lo;
      break;
    case CMPLS_p_p_zw:
      cond = ls;
      break;
    case CMPLT_p_p_zw:
      cond = lt;
      break;
    default:
      VIXL_UNIMPLEMENTED();
      cond = al;
      break;
  }

  SVEIntCompareVectorsHelper(cond,
                             instr->GetSVEVectorFormat(),
                             ReadPRegister(instr->GetPd()),
                             ReadPRegister(instr->GetPgLow8()),
                             ReadVRegister(instr->GetRn()),
                             ReadVRegister(instr->GetRm()),
                             is_wide_elements);
}

void Simulator::VisitSVEFPExponentialAccelerator(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());

  VIXL_ASSERT((vform == kFormatVnH) || (vform == kFormatVnS) ||
              (vform == kFormatVnD));

  switch (instr->Mask(SVEFPExponentialAcceleratorMask)) {
    case FEXPA_z_z:
      fexpa(vform, zd, zn);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEFPTrigSelectCoefficient(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimVRegister& zm = ReadVRegister(instr->GetRm());

  VIXL_ASSERT((vform == kFormatVnH) || (vform == kFormatVnS) ||
              (vform == kFormatVnD));

  switch (instr->Mask(SVEFPTrigSelectCoefficientMask)) {
    case FTSSEL_z_zz:
      ftssel(vform, zd, zn, zm);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEConstructivePrefix_Unpredicated(
    const Instruction* instr) {
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());

  switch (instr->Mask(SVEConstructivePrefix_UnpredicatedMask)) {
    case MOVPRFX_z_z:
      mov(kFormatVnD, zd, zn);  // The lane size is arbitrary.
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEIntMulAddPredicated(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();

  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRm());

  SimVRegister result;
  switch (instr->Mask(SVEIntMulAddPredicatedMask)) {
    case MLA_z_p_zzz:
      mla(vform, result, zd, ReadVRegister(instr->GetRn()), zm);
      break;
    case MLS_z_p_zzz:
      mls(vform, result, zd, ReadVRegister(instr->GetRn()), zm);
      break;
    case MAD_z_p_zzz:
      // 'za' is encoded in 'Rn'.
      mla(vform, result, ReadVRegister(instr->GetRn()), zd, zm);
      break;
    case MSB_z_p_zzz: {
      // 'za' is encoded in 'Rn'.
      mls(vform, result, ReadVRegister(instr->GetRn()), zd, zm);
      break;
    }
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
  mov_merging(vform, zd, ReadPRegister(instr->GetPgLow8()), result);
}

void Simulator::VisitSVEIntMulAddUnpredicated(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zda = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimVRegister& zm = ReadVRegister(instr->GetRm());

  switch (form_hash_) {
    case "sdot_z_zzz"_h:
      sdot(vform, zda, zn, zm);
      break;
    case "udot_z_zzz"_h:
      udot(vform, zda, zn, zm);
      break;
    case "usdot_z_zzz_s"_h:
      usdot(vform, zda, zn, zm);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEMovprfx(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  SimVRegister& zd = ReadVRegister(instr->GetRd());

  switch (instr->Mask(SVEMovprfxMask)) {
    case MOVPRFX_z_p_z:
      if (instr->ExtractBit(16)) {
        mov_merging(vform, zd, pg, zn);
      } else {
        mov_zeroing(vform, zd, pg, zn);
      }
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEIntReduction(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& vd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());

  if (instr->Mask(SVEIntReductionLogicalFMask) == SVEIntReductionLogicalFixed) {
    switch (instr->Mask(SVEIntReductionLogicalMask)) {
      case ANDV_r_p_z:
        andv(vform, vd, pg, zn);
        break;
      case EORV_r_p_z:
        eorv(vform, vd, pg, zn);
        break;
      case ORV_r_p_z:
        orv(vform, vd, pg, zn);
        break;
      default:
        VIXL_UNIMPLEMENTED();
        break;
    }
  } else {
    switch (instr->Mask(SVEIntReductionMask)) {
      case SADDV_r_p_z:
        saddv(vform, vd, pg, zn);
        break;
      case SMAXV_r_p_z:
        smaxv(vform, vd, pg, zn);
        break;
      case SMINV_r_p_z:
        sminv(vform, vd, pg, zn);
        break;
      case UADDV_r_p_z:
        uaddv(vform, vd, pg, zn);
        break;
      case UMAXV_r_p_z:
        umaxv(vform, vd, pg, zn);
        break;
      case UMINV_r_p_z:
        uminv(vform, vd, pg, zn);
        break;
      default:
        VIXL_UNIMPLEMENTED();
        break;
    }
  }
}

void Simulator::VisitSVEIntUnaryArithmeticPredicated(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zn = ReadVRegister(instr->GetRn());

  SimVRegister result;
  switch (instr->Mask(SVEIntUnaryArithmeticPredicatedMask)) {
    case ABS_z_p_z:
      abs(vform, result, zn);
      break;
    case CLS_z_p_z:
      cls(vform, result, zn);
      break;
    case CLZ_z_p_z:
      clz(vform, result, zn);
      break;
    case CNOT_z_p_z:
      cnot(vform, result, zn);
      break;
    case CNT_z_p_z:
      cnt(vform, result, zn);
      break;
    case FABS_z_p_z:
      fabs_(vform, result, zn);
      break;
    case FNEG_z_p_z:
      fneg(vform, result, zn);
      break;
    case NEG_z_p_z:
      neg(vform, result, zn);
      break;
    case NOT_z_p_z:
      not_(vform, result, zn);
      break;
    case SXTB_z_p_z:
    case SXTH_z_p_z:
    case SXTW_z_p_z:
      sxt(vform, result, zn, (kBitsPerByte << instr->ExtractBits(18, 17)));
      break;
    case UXTB_z_p_z:
    case UXTH_z_p_z:
    case UXTW_z_p_z:
      uxt(vform, result, zn, (kBitsPerByte << instr->ExtractBits(18, 17)));
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  mov_merging(vform, zd, pg, result);
}

void Simulator::VisitSVECopyFPImm_Predicated(const Instruction* instr) {
  // There is only one instruction in this group.
  VIXL_ASSERT(instr->Mask(SVECopyFPImm_PredicatedMask) == FCPY_z_p_i);

  VectorFormat vform = instr->GetSVEVectorFormat();
  SimPRegister& pg = ReadPRegister(instr->ExtractBits(19, 16));
  SimVRegister& zd = ReadVRegister(instr->GetRd());

  if (vform == kFormatVnB) VIXL_UNIMPLEMENTED();

  SimVRegister result;
  switch (instr->Mask(SVECopyFPImm_PredicatedMask)) {
    case FCPY_z_p_i: {
      int imm8 = instr->ExtractBits(12, 5);
      uint64_t value = FPToRawbitsWithSize(LaneSizeInBitsFromFormat(vform),
                                           Instruction::Imm8ToFP64(imm8));
      dup_immediate(vform, result, value);
      break;
    }
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
  mov_merging(vform, zd, pg, result);
}

void Simulator::VisitSVEIntAddSubtractImm_Unpredicated(
    const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister scratch;

  uint64_t imm = instr->GetImmSVEIntWideUnsigned();
  imm <<= instr->ExtractBit(13) * 8;

  switch (instr->Mask(SVEIntAddSubtractImm_UnpredicatedMask)) {
    case ADD_z_zi:
      add_uint(vform, zd, zd, imm);
      break;
    case SQADD_z_zi:
      add_uint(vform, zd, zd, imm).SignedSaturate(vform);
      break;
    case SQSUB_z_zi:
      sub_uint(vform, zd, zd, imm).SignedSaturate(vform);
      break;
    case SUBR_z_zi:
      dup_immediate(vform, scratch, imm);
      sub(vform, zd, scratch, zd);
      break;
    case SUB_z_zi:
      sub_uint(vform, zd, zd, imm);
      break;
    case UQADD_z_zi:
      add_uint(vform, zd, zd, imm).UnsignedSaturate(vform);
      break;
    case UQSUB_z_zi:
      sub_uint(vform, zd, zd, imm).UnsignedSaturate(vform);
      break;
    default:
      break;
  }
}

void Simulator::VisitSVEBroadcastIntImm_Unpredicated(const Instruction* instr) {
  SimVRegister& zd = ReadVRegister(instr->GetRd());

  VectorFormat format = instr->GetSVEVectorFormat();
  int64_t imm = instr->GetImmSVEIntWideSigned();
  int shift = instr->ExtractBit(13) * 8;
  imm *= uint64_t{1} << shift;

  switch (instr->Mask(SVEBroadcastIntImm_UnpredicatedMask)) {
    case DUP_z_i:
      // The encoding of byte-sized lanes with lsl #8 is undefined.
      if ((format == kFormatVnB) && (shift == 8)) {
        VIXL_UNIMPLEMENTED();
      } else {
        dup_immediate(format, zd, imm);
      }
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEBroadcastFPImm_Unpredicated(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zd = ReadVRegister(instr->GetRd());

  switch (instr->Mask(SVEBroadcastFPImm_UnpredicatedMask)) {
    case FDUP_z_i:
      switch (vform) {
        case kFormatVnH:
          dup_immediate(vform, zd, Float16ToRawbits(instr->GetSVEImmFP16()));
          break;
        case kFormatVnS:
          dup_immediate(vform, zd, FloatToRawbits(instr->GetSVEImmFP32()));
          break;
        case kFormatVnD:
          dup_immediate(vform, zd, DoubleToRawbits(instr->GetSVEImmFP64()));
          break;
        default:
          VIXL_UNIMPLEMENTED();
      }
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVE32BitGatherLoadHalfwords_ScalarPlus32BitScaledOffsets(
    const Instruction* instr) {
  switch (instr->Mask(
      SVE32BitGatherLoadHalfwords_ScalarPlus32BitScaledOffsetsMask)) {
    case LD1H_z_p_bz_s_x32_scaled:
    case LD1SH_z_p_bz_s_x32_scaled:
    case LDFF1H_z_p_bz_s_x32_scaled:
    case LDFF1SH_z_p_bz_s_x32_scaled:
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  SVEOffsetModifier mod = (instr->ExtractBit(22) == 1) ? SVE_SXTW : SVE_UXTW;
  SVEGatherLoadScalarPlusVectorHelper(instr, kFormatVnS, mod);
}

void Simulator::VisitSVE32BitGatherLoad_ScalarPlus32BitUnscaledOffsets(
    const Instruction* instr) {
  switch (instr->Mask(SVE32BitGatherLoad_ScalarPlus32BitUnscaledOffsetsMask)) {
    case LD1B_z_p_bz_s_x32_unscaled:
    case LD1H_z_p_bz_s_x32_unscaled:
    case LD1SB_z_p_bz_s_x32_unscaled:
    case LD1SH_z_p_bz_s_x32_unscaled:
    case LD1W_z_p_bz_s_x32_unscaled:
    case LDFF1B_z_p_bz_s_x32_unscaled:
    case LDFF1H_z_p_bz_s_x32_unscaled:
    case LDFF1SB_z_p_bz_s_x32_unscaled:
    case LDFF1SH_z_p_bz_s_x32_unscaled:
    case LDFF1W_z_p_bz_s_x32_unscaled:
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  SVEOffsetModifier mod = (instr->ExtractBit(22) == 1) ? SVE_SXTW : SVE_UXTW;
  SVEGatherLoadScalarPlusVectorHelper(instr, kFormatVnS, mod);
}

void Simulator::VisitSVE32BitGatherLoad_VectorPlusImm(
    const Instruction* instr) {
  switch (instr->Mask(SVE32BitGatherLoad_VectorPlusImmMask)) {
    case LD1B_z_p_ai_s:
      VIXL_UNIMPLEMENTED();
      break;
    case LD1H_z_p_ai_s:
      VIXL_UNIMPLEMENTED();
      break;
    case LD1SB_z_p_ai_s:
      VIXL_UNIMPLEMENTED();
      break;
    case LD1SH_z_p_ai_s:
      VIXL_UNIMPLEMENTED();
      break;
    case LD1W_z_p_ai_s:
      VIXL_UNIMPLEMENTED();
      break;
    case LDFF1B_z_p_ai_s:
      VIXL_UNIMPLEMENTED();
      break;
    case LDFF1H_z_p_ai_s:
      VIXL_UNIMPLEMENTED();
      break;
    case LDFF1SB_z_p_ai_s:
      VIXL_UNIMPLEMENTED();
      break;
    case LDFF1SH_z_p_ai_s:
      VIXL_UNIMPLEMENTED();
      break;
    case LDFF1W_z_p_ai_s:
      VIXL_UNIMPLEMENTED();
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVE32BitGatherLoadWords_ScalarPlus32BitScaledOffsets(
    const Instruction* instr) {
  switch (
      instr->Mask(SVE32BitGatherLoadWords_ScalarPlus32BitScaledOffsetsMask)) {
    case LD1W_z_p_bz_s_x32_scaled:
    case LDFF1W_z_p_bz_s_x32_scaled:
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  SVEOffsetModifier mod = (instr->ExtractBit(22) == 1) ? SVE_SXTW : SVE_UXTW;
  SVEGatherLoadScalarPlusVectorHelper(instr, kFormatVnS, mod);
}

void Simulator::VisitSVE32BitGatherPrefetch_ScalarPlus32BitScaledOffsets(
    const Instruction* instr) {
  switch (
      instr->Mask(SVE32BitGatherPrefetch_ScalarPlus32BitScaledOffsetsMask)) {
    // Ignore prefetch hint instructions.
    case PRFB_i_p_bz_s_x32_scaled:
    case PRFD_i_p_bz_s_x32_scaled:
    case PRFH_i_p_bz_s_x32_scaled:
    case PRFW_i_p_bz_s_x32_scaled:
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVE32BitGatherPrefetch_VectorPlusImm(
    const Instruction* instr) {
  switch (instr->Mask(SVE32BitGatherPrefetch_VectorPlusImmMask)) {
    // Ignore prefetch hint instructions.
    case PRFB_i_p_ai_s:
    case PRFD_i_p_ai_s:
    case PRFH_i_p_ai_s:
    case PRFW_i_p_ai_s:
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEContiguousPrefetch_ScalarPlusImm(
    const Instruction* instr) {
  switch (instr->Mask(SVEContiguousPrefetch_ScalarPlusImmMask)) {
    // Ignore prefetch hint instructions.
    case PRFB_i_p_bi_s:
    case PRFD_i_p_bi_s:
    case PRFH_i_p_bi_s:
    case PRFW_i_p_bi_s:
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEContiguousPrefetch_ScalarPlusScalar(
    const Instruction* instr) {
  switch (instr->Mask(SVEContiguousPrefetch_ScalarPlusScalarMask)) {
    // Ignore prefetch hint instructions.
    case PRFB_i_p_br_s:
    case PRFD_i_p_br_s:
    case PRFH_i_p_br_s:
    case PRFW_i_p_br_s:
      if (instr->GetRm() == kZeroRegCode) {
        VIXL_UNIMPLEMENTED();
      }
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVELoadAndBroadcastElement(const Instruction* instr) {
  bool is_signed;
  switch (instr->Mask(SVELoadAndBroadcastElementMask)) {
    case LD1RB_z_p_bi_u8:
    case LD1RB_z_p_bi_u16:
    case LD1RB_z_p_bi_u32:
    case LD1RB_z_p_bi_u64:
    case LD1RH_z_p_bi_u16:
    case LD1RH_z_p_bi_u32:
    case LD1RH_z_p_bi_u64:
    case LD1RW_z_p_bi_u32:
    case LD1RW_z_p_bi_u64:
    case LD1RD_z_p_bi_u64:
      is_signed = false;
      break;
    case LD1RSB_z_p_bi_s16:
    case LD1RSB_z_p_bi_s32:
    case LD1RSB_z_p_bi_s64:
    case LD1RSH_z_p_bi_s32:
    case LD1RSH_z_p_bi_s64:
    case LD1RSW_z_p_bi_s64:
      is_signed = true;
      break;
    default:
      // This encoding group is complete, so no other values should be possible.
      VIXL_UNREACHABLE();
      is_signed = false;
      break;
  }

  int msize_in_bytes_log2 = instr->GetSVEMsizeFromDtype(is_signed);
  int esize_in_bytes_log2 = instr->GetSVEEsizeFromDtype(is_signed, 13);
  VIXL_ASSERT(msize_in_bytes_log2 <= esize_in_bytes_log2);
  VectorFormat vform = SVEFormatFromLaneSizeInBytesLog2(esize_in_bytes_log2);
  uint64_t offset = instr->ExtractBits(21, 16) << msize_in_bytes_log2;
  uint64_t base = ReadXRegister(instr->GetRn(), Reg31IsStackPointer) + offset;
  VectorFormat unpack_vform =
      SVEFormatFromLaneSizeInBytesLog2(msize_in_bytes_log2);
  SimVRegister temp;
  if (!ld1r(vform, unpack_vform, temp, base, is_signed)) return;
  mov_zeroing(vform,
              ReadVRegister(instr->GetRt()),
              ReadPRegister(instr->GetPgLow8()),
              temp);
}

void Simulator::VisitSVELoadPredicateRegister(const Instruction* instr) {
  switch (instr->Mask(SVELoadPredicateRegisterMask)) {
    case LDR_p_bi: {
      SimPRegister& pt = ReadPRegister(instr->GetPt());
      int pl = GetPredicateLengthInBytes();
      int imm9 = (instr->ExtractBits(21, 16) << 3) | instr->ExtractBits(12, 10);
      uint64_t multiplier = ExtractSignedBitfield64(8, 0, imm9);
      uint64_t base = ReadXRegister(instr->GetRn(), Reg31IsStackPointer);
      uint64_t address = base + multiplier * pl;
      for (int i = 0; i < pl; i++) {
        VIXL_DEFINE_OR_RETURN(value, MemRead<uint8_t>(address + i));
        pt.Insert(i, value);
      }
      LogPRead(instr->GetPt(), address);
      break;
    }
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVELoadVectorRegister(const Instruction* instr) {
  switch (instr->Mask(SVELoadVectorRegisterMask)) {
    case LDR_z_bi: {
      SimVRegister& zt = ReadVRegister(instr->GetRt());
      int vl = GetVectorLengthInBytes();
      int imm9 = (instr->ExtractBits(21, 16) << 3) | instr->ExtractBits(12, 10);
      uint64_t multiplier = ExtractSignedBitfield64(8, 0, imm9);
      uint64_t base = ReadXRegister(instr->GetRn(), Reg31IsStackPointer);
      uint64_t address = base + multiplier * vl;
      for (int i = 0; i < vl; i++) {
        VIXL_DEFINE_OR_RETURN(value, MemRead<uint8_t>(address + i));
        zt.Insert(i, value);
      }
      LogZRead(instr->GetRt(), address);
      break;
    }
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVE64BitGatherLoad_ScalarPlus32BitUnpackedScaledOffsets(
    const Instruction* instr) {
  switch (instr->Mask(
      SVE64BitGatherLoad_ScalarPlus32BitUnpackedScaledOffsetsMask)) {
    case LD1D_z_p_bz_d_x32_scaled:
    case LD1H_z_p_bz_d_x32_scaled:
    case LD1SH_z_p_bz_d_x32_scaled:
    case LD1SW_z_p_bz_d_x32_scaled:
    case LD1W_z_p_bz_d_x32_scaled:
    case LDFF1H_z_p_bz_d_x32_scaled:
    case LDFF1W_z_p_bz_d_x32_scaled:
    case LDFF1D_z_p_bz_d_x32_scaled:
    case LDFF1SH_z_p_bz_d_x32_scaled:
    case LDFF1SW_z_p_bz_d_x32_scaled:
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  SVEOffsetModifier mod = (instr->ExtractBit(22) == 1) ? SVE_SXTW : SVE_UXTW;
  SVEGatherLoadScalarPlusVectorHelper(instr, kFormatVnD, mod);
}

void Simulator::VisitSVE64BitGatherLoad_ScalarPlus64BitScaledOffsets(
    const Instruction* instr) {
  switch (instr->Mask(SVE64BitGatherLoad_ScalarPlus64BitScaledOffsetsMask)) {
    case LD1D_z_p_bz_d_64_scaled:
    case LD1H_z_p_bz_d_64_scaled:
    case LD1SH_z_p_bz_d_64_scaled:
    case LD1SW_z_p_bz_d_64_scaled:
    case LD1W_z_p_bz_d_64_scaled:
    case LDFF1H_z_p_bz_d_64_scaled:
    case LDFF1W_z_p_bz_d_64_scaled:
    case LDFF1D_z_p_bz_d_64_scaled:
    case LDFF1SH_z_p_bz_d_64_scaled:
    case LDFF1SW_z_p_bz_d_64_scaled:
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  SVEGatherLoadScalarPlusVectorHelper(instr, kFormatVnD, SVE_LSL);
}

void Simulator::VisitSVE64BitGatherLoad_ScalarPlus64BitUnscaledOffsets(
    const Instruction* instr) {
  switch (instr->Mask(SVE64BitGatherLoad_ScalarPlus64BitUnscaledOffsetsMask)) {
    case LD1B_z_p_bz_d_64_unscaled:
    case LD1D_z_p_bz_d_64_unscaled:
    case LD1H_z_p_bz_d_64_unscaled:
    case LD1SB_z_p_bz_d_64_unscaled:
    case LD1SH_z_p_bz_d_64_unscaled:
    case LD1SW_z_p_bz_d_64_unscaled:
    case LD1W_z_p_bz_d_64_unscaled:
    case LDFF1B_z_p_bz_d_64_unscaled:
    case LDFF1D_z_p_bz_d_64_unscaled:
    case LDFF1H_z_p_bz_d_64_unscaled:
    case LDFF1SB_z_p_bz_d_64_unscaled:
    case LDFF1SH_z_p_bz_d_64_unscaled:
    case LDFF1SW_z_p_bz_d_64_unscaled:
    case LDFF1W_z_p_bz_d_64_unscaled:
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  SVEGatherLoadScalarPlusVectorHelper(instr,
                                      kFormatVnD,
                                      NO_SVE_OFFSET_MODIFIER);
}

void Simulator::VisitSVE64BitGatherLoad_ScalarPlusUnpacked32BitUnscaledOffsets(
    const Instruction* instr) {
  switch (instr->Mask(
      SVE64BitGatherLoad_ScalarPlusUnpacked32BitUnscaledOffsetsMask)) {
    case LD1B_z_p_bz_d_x32_unscaled:
    case LD1D_z_p_bz_d_x32_unscaled:
    case LD1H_z_p_bz_d_x32_unscaled:
    case LD1SB_z_p_bz_d_x32_unscaled:
    case LD1SH_z_p_bz_d_x32_unscaled:
    case LD1SW_z_p_bz_d_x32_unscaled:
    case LD1W_z_p_bz_d_x32_unscaled:
    case LDFF1B_z_p_bz_d_x32_unscaled:
    case LDFF1H_z_p_bz_d_x32_unscaled:
    case LDFF1W_z_p_bz_d_x32_unscaled:
    case LDFF1D_z_p_bz_d_x32_unscaled:
    case LDFF1SB_z_p_bz_d_x32_unscaled:
    case LDFF1SH_z_p_bz_d_x32_unscaled:
    case LDFF1SW_z_p_bz_d_x32_unscaled:
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  SVEOffsetModifier mod = (instr->ExtractBit(22) == 1) ? SVE_SXTW : SVE_UXTW;
  SVEGatherLoadScalarPlusVectorHelper(instr, kFormatVnD, mod);
}

void Simulator::VisitSVE64BitGatherLoad_VectorPlusImm(
    const Instruction* instr) {
  switch (instr->Mask(SVE64BitGatherLoad_VectorPlusImmMask)) {
    case LD1B_z_p_ai_d:
    case LD1D_z_p_ai_d:
    case LD1H_z_p_ai_d:
    case LD1SB_z_p_ai_d:
    case LD1SH_z_p_ai_d:
    case LD1SW_z_p_ai_d:
    case LD1W_z_p_ai_d:
    case LDFF1B_z_p_ai_d:
    case LDFF1D_z_p_ai_d:
    case LDFF1H_z_p_ai_d:
    case LDFF1SB_z_p_ai_d:
    case LDFF1SH_z_p_ai_d:
    case LDFF1SW_z_p_ai_d:
    case LDFF1W_z_p_ai_d:
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
  bool is_signed = instr->ExtractBit(14) == 0;
  bool is_ff = instr->ExtractBit(13) == 1;
  // Note that these instructions don't use the Dtype encoding.
  int msize_in_bytes_log2 = instr->ExtractBits(24, 23);
  uint64_t imm = instr->ExtractBits(20, 16) << msize_in_bytes_log2;
  LogicSVEAddressVector addr(imm, &ReadVRegister(instr->GetRn()), kFormatVnD);
  addr.SetMsizeInBytesLog2(msize_in_bytes_log2);
  if (is_ff) {
    VIXL_UNIMPLEMENTED();
  } else {
    SVEStructuredLoadHelper(kFormatVnD,
                            ReadPRegister(instr->GetPgLow8()),
                            instr->GetRt(),
                            addr,
                            is_signed);
  }
}

void Simulator::VisitSVE64BitGatherPrefetch_ScalarPlus64BitScaledOffsets(
    const Instruction* instr) {
  switch (
      instr->Mask(SVE64BitGatherPrefetch_ScalarPlus64BitScaledOffsetsMask)) {
    // Ignore prefetch hint instructions.
    case PRFB_i_p_bz_d_64_scaled:
    case PRFD_i_p_bz_d_64_scaled:
    case PRFH_i_p_bz_d_64_scaled:
    case PRFW_i_p_bz_d_64_scaled:
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::
    VisitSVE64BitGatherPrefetch_ScalarPlusUnpacked32BitScaledOffsets(
        const Instruction* instr) {
  switch (instr->Mask(
      SVE64BitGatherPrefetch_ScalarPlusUnpacked32BitScaledOffsetsMask)) {
    // Ignore prefetch hint instructions.
    case PRFB_i_p_bz_d_x32_scaled:
    case PRFD_i_p_bz_d_x32_scaled:
    case PRFH_i_p_bz_d_x32_scaled:
    case PRFW_i_p_bz_d_x32_scaled:
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVE64BitGatherPrefetch_VectorPlusImm(
    const Instruction* instr) {
  switch (instr->Mask(SVE64BitGatherPrefetch_VectorPlusImmMask)) {
    // Ignore prefetch hint instructions.
    case PRFB_i_p_ai_d:
    case PRFD_i_p_ai_d:
    case PRFH_i_p_ai_d:
    case PRFW_i_p_ai_d:
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEContiguousFirstFaultLoad_ScalarPlusScalar(
    const Instruction* instr) {
  bool is_signed;
  switch (instr->Mask(SVEContiguousLoad_ScalarPlusScalarMask)) {
    case LDFF1B_z_p_br_u8:
    case LDFF1B_z_p_br_u16:
    case LDFF1B_z_p_br_u32:
    case LDFF1B_z_p_br_u64:
    case LDFF1H_z_p_br_u16:
    case LDFF1H_z_p_br_u32:
    case LDFF1H_z_p_br_u64:
    case LDFF1W_z_p_br_u32:
    case LDFF1W_z_p_br_u64:
    case LDFF1D_z_p_br_u64:
      is_signed = false;
      break;
    case LDFF1SB_z_p_br_s16:
    case LDFF1SB_z_p_br_s32:
    case LDFF1SB_z_p_br_s64:
    case LDFF1SH_z_p_br_s32:
    case LDFF1SH_z_p_br_s64:
    case LDFF1SW_z_p_br_s64:
      is_signed = true;
      break;
    default:
      // This encoding group is complete, so no other values should be possible.
      VIXL_UNREACHABLE();
      is_signed = false;
      break;
  }

  int msize_in_bytes_log2 = instr->GetSVEMsizeFromDtype(is_signed);
  int esize_in_bytes_log2 = instr->GetSVEEsizeFromDtype(is_signed);
  VIXL_ASSERT(msize_in_bytes_log2 <= esize_in_bytes_log2);
  VectorFormat vform = SVEFormatFromLaneSizeInBytesLog2(esize_in_bytes_log2);
  uint64_t base = ReadXRegister(instr->GetRn(), Reg31IsStackPointer);
  uint64_t offset = ReadXRegister(instr->GetRm());
  offset <<= msize_in_bytes_log2;
  LogicSVEAddressVector addr(base + offset);
  addr.SetMsizeInBytesLog2(msize_in_bytes_log2);
  SVEFaultTolerantLoadHelper(vform,
                             ReadPRegister(instr->GetPgLow8()),
                             instr->GetRt(),
                             addr,
                             kSVEFirstFaultLoad,
                             is_signed);
}

void Simulator::VisitSVEContiguousNonFaultLoad_ScalarPlusImm(
    const Instruction* instr) {
  bool is_signed = false;
  switch (instr->Mask(SVEContiguousNonFaultLoad_ScalarPlusImmMask)) {
    case LDNF1B_z_p_bi_u16:
    case LDNF1B_z_p_bi_u32:
    case LDNF1B_z_p_bi_u64:
    case LDNF1B_z_p_bi_u8:
    case LDNF1D_z_p_bi_u64:
    case LDNF1H_z_p_bi_u16:
    case LDNF1H_z_p_bi_u32:
    case LDNF1H_z_p_bi_u64:
    case LDNF1W_z_p_bi_u32:
    case LDNF1W_z_p_bi_u64:
      break;
    case LDNF1SB_z_p_bi_s16:
    case LDNF1SB_z_p_bi_s32:
    case LDNF1SB_z_p_bi_s64:
    case LDNF1SH_z_p_bi_s32:
    case LDNF1SH_z_p_bi_s64:
    case LDNF1SW_z_p_bi_s64:
      is_signed = true;
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
  int msize_in_bytes_log2 = instr->GetSVEMsizeFromDtype(is_signed);
  int esize_in_bytes_log2 = instr->GetSVEEsizeFromDtype(is_signed);
  VIXL_ASSERT(msize_in_bytes_log2 <= esize_in_bytes_log2);
  VectorFormat vform = SVEFormatFromLaneSizeInBytesLog2(esize_in_bytes_log2);
  int vl = GetVectorLengthInBytes();
  int vl_divisor_log2 = esize_in_bytes_log2 - msize_in_bytes_log2;
  uint64_t base = ReadXRegister(instr->GetRn(), Reg31IsStackPointer);
  uint64_t offset =
      (instr->ExtractSignedBits(19, 16) * vl) / (1 << vl_divisor_log2);
  LogicSVEAddressVector addr(base + offset);
  addr.SetMsizeInBytesLog2(msize_in_bytes_log2);
  SVEFaultTolerantLoadHelper(vform,
                             ReadPRegister(instr->GetPgLow8()),
                             instr->GetRt(),
                             addr,
                             kSVENonFaultLoad,
                             is_signed);
}

void Simulator::VisitSVEContiguousNonTemporalLoad_ScalarPlusImm(
    const Instruction* instr) {
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  VectorFormat vform = kFormatUndefined;

  switch (instr->Mask(SVEContiguousNonTemporalLoad_ScalarPlusImmMask)) {
    case LDNT1B_z_p_bi_contiguous:
      vform = kFormatVnB;
      break;
    case LDNT1D_z_p_bi_contiguous:
      vform = kFormatVnD;
      break;
    case LDNT1H_z_p_bi_contiguous:
      vform = kFormatVnH;
      break;
    case LDNT1W_z_p_bi_contiguous:
      vform = kFormatVnS;
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
  int msize_in_bytes_log2 = LaneSizeInBytesLog2FromFormat(vform);
  int vl = GetVectorLengthInBytes();
  uint64_t base = ReadXRegister(instr->GetRn(), Reg31IsStackPointer);
  uint64_t offset = instr->ExtractSignedBits(19, 16) * vl;
  LogicSVEAddressVector addr(base + offset);
  addr.SetMsizeInBytesLog2(msize_in_bytes_log2);
  SVEStructuredLoadHelper(vform,
                          pg,
                          instr->GetRt(),
                          addr,
                          /* is_signed = */ false);
}

void Simulator::VisitSVEContiguousNonTemporalLoad_ScalarPlusScalar(
    const Instruction* instr) {
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  VectorFormat vform = kFormatUndefined;

  switch (instr->Mask(SVEContiguousNonTemporalLoad_ScalarPlusScalarMask)) {
    case LDNT1B_z_p_br_contiguous:
      vform = kFormatVnB;
      break;
    case LDNT1D_z_p_br_contiguous:
      vform = kFormatVnD;
      break;
    case LDNT1H_z_p_br_contiguous:
      vform = kFormatVnH;
      break;
    case LDNT1W_z_p_br_contiguous:
      vform = kFormatVnS;
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
  int msize_in_bytes_log2 = LaneSizeInBytesLog2FromFormat(vform);
  uint64_t base = ReadXRegister(instr->GetRn(), Reg31IsStackPointer);
  uint64_t offset = ReadXRegister(instr->GetRm()) << msize_in_bytes_log2;
  LogicSVEAddressVector addr(base + offset);
  addr.SetMsizeInBytesLog2(msize_in_bytes_log2);
  SVEStructuredLoadHelper(vform,
                          pg,
                          instr->GetRt(),
                          addr,
                          /* is_signed = */ false);
}

void Simulator::VisitSVELoadAndBroadcastQOWord_ScalarPlusImm(
    const Instruction* instr) {
  SimVRegister& zt = ReadVRegister(instr->GetRt());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());

  uint64_t dwords = 2;
  VectorFormat vform_dst = kFormatVnQ;
  if ((form_hash_ == "ld1rob_z_p_bi_u8"_h) ||
      (form_hash_ == "ld1roh_z_p_bi_u16"_h) ||
      (form_hash_ == "ld1row_z_p_bi_u32"_h) ||
      (form_hash_ == "ld1rod_z_p_bi_u64"_h)) {
    dwords = 4;
    vform_dst = kFormatVnO;
  }

  uint64_t addr = ReadXRegister(instr->GetRn(), Reg31IsStackPointer);
  uint64_t offset =
      instr->ExtractSignedBits(19, 16) * dwords * kDRegSizeInBytes;
  int msz = instr->ExtractBits(24, 23);
  VectorFormat vform = SVEFormatFromLaneSizeInBytesLog2(msz);

  for (unsigned i = 0; i < dwords; i++) {
    if (!ld1(kFormatVnD, zt, i, addr + offset + (i * kDRegSizeInBytes))) return;
  }
  mov_zeroing(vform, zt, pg, zt);
  dup_element(vform_dst, zt, zt, 0);
}

void Simulator::VisitSVELoadAndBroadcastQOWord_ScalarPlusScalar(
    const Instruction* instr) {
  SimVRegister& zt = ReadVRegister(instr->GetRt());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());

  uint64_t bytes = 16;
  VectorFormat vform_dst = kFormatVnQ;
  if ((form_hash_ == "ld1rob_z_p_br_contiguous"_h) ||
      (form_hash_ == "ld1roh_z_p_br_contiguous"_h) ||
      (form_hash_ == "ld1row_z_p_br_contiguous"_h) ||
      (form_hash_ == "ld1rod_z_p_br_contiguous"_h)) {
    bytes = 32;
    vform_dst = kFormatVnO;
  }

  uint64_t addr = ReadXRegister(instr->GetRn(), Reg31IsStackPointer);
  uint64_t offset = ReadXRegister(instr->GetRm());
  int msz = instr->ExtractBits(24, 23);
  VectorFormat vform = SVEFormatFromLaneSizeInBytesLog2(msz);
  offset <<= msz;
  for (unsigned i = 0; i < bytes; i++) {
    if (!ld1(kFormatVnB, zt, i, addr + offset + i)) return;
  }
  mov_zeroing(vform, zt, pg, zt);
  dup_element(vform_dst, zt, zt, 0);
}

void Simulator::VisitSVELoadMultipleStructures_ScalarPlusImm(
    const Instruction* instr) {
  switch (instr->Mask(SVELoadMultipleStructures_ScalarPlusImmMask)) {
    case LD2B_z_p_bi_contiguous:
    case LD2D_z_p_bi_contiguous:
    case LD2H_z_p_bi_contiguous:
    case LD2W_z_p_bi_contiguous:
    case LD3B_z_p_bi_contiguous:
    case LD3D_z_p_bi_contiguous:
    case LD3H_z_p_bi_contiguous:
    case LD3W_z_p_bi_contiguous:
    case LD4B_z_p_bi_contiguous:
    case LD4D_z_p_bi_contiguous:
    case LD4H_z_p_bi_contiguous:
    case LD4W_z_p_bi_contiguous: {
      int vl = GetVectorLengthInBytes();
      int msz = instr->ExtractBits(24, 23);
      int reg_count = instr->ExtractBits(22, 21) + 1;
      uint64_t offset = instr->ExtractSignedBits(19, 16) * vl * reg_count;
      LogicSVEAddressVector addr(
          ReadXRegister(instr->GetRn(), Reg31IsStackPointer) + offset);
      addr.SetMsizeInBytesLog2(msz);
      addr.SetRegCount(reg_count);
      SVEStructuredLoadHelper(SVEFormatFromLaneSizeInBytesLog2(msz),
                              ReadPRegister(instr->GetPgLow8()),
                              instr->GetRt(),
                              addr);
      break;
    }
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVELoadMultipleStructures_ScalarPlusScalar(
    const Instruction* instr) {
  switch (instr->Mask(SVELoadMultipleStructures_ScalarPlusScalarMask)) {
    case LD2B_z_p_br_contiguous:
    case LD2D_z_p_br_contiguous:
    case LD2H_z_p_br_contiguous:
    case LD2W_z_p_br_contiguous:
    case LD3B_z_p_br_contiguous:
    case LD3D_z_p_br_contiguous:
    case LD3H_z_p_br_contiguous:
    case LD3W_z_p_br_contiguous:
    case LD4B_z_p_br_contiguous:
    case LD4D_z_p_br_contiguous:
    case LD4H_z_p_br_contiguous:
    case LD4W_z_p_br_contiguous: {
      int msz = instr->ExtractBits(24, 23);
      uint64_t offset = ReadXRegister(instr->GetRm()) * (uint64_t{1} << msz);
      VectorFormat vform = SVEFormatFromLaneSizeInBytesLog2(msz);
      LogicSVEAddressVector addr(
          ReadXRegister(instr->GetRn(), Reg31IsStackPointer) + offset);
      addr.SetMsizeInBytesLog2(msz);
      addr.SetRegCount(instr->ExtractBits(22, 21) + 1);
      SVEStructuredLoadHelper(vform,
                              ReadPRegister(instr->GetPgLow8()),
                              instr->GetRt(),
                              addr,
                              false);
      break;
    }
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVE32BitScatterStore_ScalarPlus32BitScaledOffsets(
    const Instruction* instr) {
  switch (instr->Mask(SVE32BitScatterStore_ScalarPlus32BitScaledOffsetsMask)) {
    case ST1H_z_p_bz_s_x32_scaled:
    case ST1W_z_p_bz_s_x32_scaled: {
      unsigned msize_in_bytes_log2 = instr->GetSVEMsizeFromDtype(false);
      VIXL_ASSERT(kDRegSizeInBytesLog2 >= msize_in_bytes_log2);
      int scale = instr->ExtractBit(21) * msize_in_bytes_log2;
      uint64_t base = ReadXRegister(instr->GetRn(), Reg31IsStackPointer);
      SVEOffsetModifier mod =
          (instr->ExtractBit(14) == 1) ? SVE_SXTW : SVE_UXTW;
      LogicSVEAddressVector addr(base,
                                 &ReadVRegister(instr->GetRm()),
                                 kFormatVnS,
                                 mod,
                                 scale);
      addr.SetMsizeInBytesLog2(msize_in_bytes_log2);
      SVEStructuredStoreHelper(kFormatVnS,
                               ReadPRegister(instr->GetPgLow8()),
                               instr->GetRt(),
                               addr);
      break;
    }
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVE32BitScatterStore_ScalarPlus32BitUnscaledOffsets(
    const Instruction* instr) {
  switch (
      instr->Mask(SVE32BitScatterStore_ScalarPlus32BitUnscaledOffsetsMask)) {
    case ST1B_z_p_bz_s_x32_unscaled:
    case ST1H_z_p_bz_s_x32_unscaled:
    case ST1W_z_p_bz_s_x32_unscaled: {
      unsigned msize_in_bytes_log2 = instr->GetSVEMsizeFromDtype(false);
      VIXL_ASSERT(kDRegSizeInBytesLog2 >= msize_in_bytes_log2);
      uint64_t base = ReadXRegister(instr->GetRn(), Reg31IsStackPointer);
      SVEOffsetModifier mod =
          (instr->ExtractBit(14) == 1) ? SVE_SXTW : SVE_UXTW;
      LogicSVEAddressVector addr(base,
                                 &ReadVRegister(instr->GetRm()),
                                 kFormatVnS,
                                 mod);
      addr.SetMsizeInBytesLog2(msize_in_bytes_log2);
      SVEStructuredStoreHelper(kFormatVnS,
                               ReadPRegister(instr->GetPgLow8()),
                               instr->GetRt(),
                               addr);
      break;
    }
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVE32BitScatterStore_VectorPlusImm(
    const Instruction* instr) {
  int msz = 0;
  switch (instr->Mask(SVE32BitScatterStore_VectorPlusImmMask)) {
    case ST1B_z_p_ai_s:
      msz = 0;
      break;
    case ST1H_z_p_ai_s:
      msz = 1;
      break;
    case ST1W_z_p_ai_s:
      msz = 2;
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
  uint64_t imm = instr->ExtractBits(20, 16) << msz;
  LogicSVEAddressVector addr(imm, &ReadVRegister(instr->GetRn()), kFormatVnS);
  addr.SetMsizeInBytesLog2(msz);
  SVEStructuredStoreHelper(kFormatVnS,
                           ReadPRegister(instr->GetPgLow8()),
                           instr->GetRt(),
                           addr);
}

void Simulator::VisitSVE64BitScatterStore_ScalarPlus64BitScaledOffsets(
    const Instruction* instr) {
  switch (instr->Mask(SVE64BitScatterStore_ScalarPlus64BitScaledOffsetsMask)) {
    case ST1D_z_p_bz_d_64_scaled:
    case ST1H_z_p_bz_d_64_scaled:
    case ST1W_z_p_bz_d_64_scaled: {
      unsigned msize_in_bytes_log2 = instr->GetSVEMsizeFromDtype(false);
      VIXL_ASSERT(kDRegSizeInBytesLog2 >= msize_in_bytes_log2);
      int scale = instr->ExtractBit(21) * msize_in_bytes_log2;
      uint64_t base = ReadXRegister(instr->GetRn(), Reg31IsStackPointer);
      LogicSVEAddressVector addr(base,
                                 &ReadVRegister(instr->GetRm()),
                                 kFormatVnD,
                                 SVE_LSL,
                                 scale);
      addr.SetMsizeInBytesLog2(msize_in_bytes_log2);
      SVEStructuredStoreHelper(kFormatVnD,
                               ReadPRegister(instr->GetPgLow8()),
                               instr->GetRt(),
                               addr);
      break;
    }
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVE64BitScatterStore_ScalarPlus64BitUnscaledOffsets(
    const Instruction* instr) {
  switch (
      instr->Mask(SVE64BitScatterStore_ScalarPlus64BitUnscaledOffsetsMask)) {
    case ST1B_z_p_bz_d_64_unscaled:
    case ST1D_z_p_bz_d_64_unscaled:
    case ST1H_z_p_bz_d_64_unscaled:
    case ST1W_z_p_bz_d_64_unscaled: {
      unsigned msize_in_bytes_log2 = instr->GetSVEMsizeFromDtype(false);
      VIXL_ASSERT(kDRegSizeInBytesLog2 >= msize_in_bytes_log2);
      uint64_t base = ReadXRegister(instr->GetRn(), Reg31IsStackPointer);
      LogicSVEAddressVector addr(base,
                                 &ReadVRegister(instr->GetRm()),
                                 kFormatVnD,
                                 NO_SVE_OFFSET_MODIFIER);
      addr.SetMsizeInBytesLog2(msize_in_bytes_log2);
      SVEStructuredStoreHelper(kFormatVnD,
                               ReadPRegister(instr->GetPgLow8()),
                               instr->GetRt(),
                               addr);
      break;
    }
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVE64BitScatterStore_ScalarPlusUnpacked32BitScaledOffsets(
    const Instruction* instr) {
  switch (instr->Mask(
      SVE64BitScatterStore_ScalarPlusUnpacked32BitScaledOffsetsMask)) {
    case ST1D_z_p_bz_d_x32_scaled:
    case ST1H_z_p_bz_d_x32_scaled:
    case ST1W_z_p_bz_d_x32_scaled: {
      unsigned msize_in_bytes_log2 = instr->GetSVEMsizeFromDtype(false);
      VIXL_ASSERT(kDRegSizeInBytesLog2 >= msize_in_bytes_log2);
      int scale = instr->ExtractBit(21) * msize_in_bytes_log2;
      uint64_t base = ReadXRegister(instr->GetRn(), Reg31IsStackPointer);
      SVEOffsetModifier mod =
          (instr->ExtractBit(14) == 1) ? SVE_SXTW : SVE_UXTW;
      LogicSVEAddressVector addr(base,
                                 &ReadVRegister(instr->GetRm()),
                                 kFormatVnD,
                                 mod,
                                 scale);
      addr.SetMsizeInBytesLog2(msize_in_bytes_log2);
      SVEStructuredStoreHelper(kFormatVnD,
                               ReadPRegister(instr->GetPgLow8()),
                               instr->GetRt(),
                               addr);
      break;
    }
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::
    VisitSVE64BitScatterStore_ScalarPlusUnpacked32BitUnscaledOffsets(
        const Instruction* instr) {
  switch (instr->Mask(
      SVE64BitScatterStore_ScalarPlusUnpacked32BitUnscaledOffsetsMask)) {
    case ST1B_z_p_bz_d_x32_unscaled:
    case ST1D_z_p_bz_d_x32_unscaled:
    case ST1H_z_p_bz_d_x32_unscaled:
    case ST1W_z_p_bz_d_x32_unscaled: {
      unsigned msize_in_bytes_log2 = instr->GetSVEMsizeFromDtype(false);
      VIXL_ASSERT(kDRegSizeInBytesLog2 >= msize_in_bytes_log2);
      uint64_t base = ReadXRegister(instr->GetRn(), Reg31IsStackPointer);
      SVEOffsetModifier mod =
          (instr->ExtractBit(14) == 1) ? SVE_SXTW : SVE_UXTW;
      LogicSVEAddressVector addr(base,
                                 &ReadVRegister(instr->GetRm()),
                                 kFormatVnD,
                                 mod);
      addr.SetMsizeInBytesLog2(msize_in_bytes_log2);
      SVEStructuredStoreHelper(kFormatVnD,
                               ReadPRegister(instr->GetPgLow8()),
                               instr->GetRt(),
                               addr);
      break;
    }
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVE64BitScatterStore_VectorPlusImm(
    const Instruction* instr) {
  int msz = 0;
  switch (instr->Mask(SVE64BitScatterStore_VectorPlusImmMask)) {
    case ST1B_z_p_ai_d:
      msz = 0;
      break;
    case ST1D_z_p_ai_d:
      msz = 3;
      break;
    case ST1H_z_p_ai_d:
      msz = 1;
      break;
    case ST1W_z_p_ai_d:
      msz = 2;
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
  uint64_t imm = instr->ExtractBits(20, 16) << msz;
  LogicSVEAddressVector addr(imm, &ReadVRegister(instr->GetRn()), kFormatVnD);
  addr.SetMsizeInBytesLog2(msz);
  SVEStructuredStoreHelper(kFormatVnD,
                           ReadPRegister(instr->GetPgLow8()),
                           instr->GetRt(),
                           addr);
}

void Simulator::VisitSVEContiguousNonTemporalStore_ScalarPlusImm(
    const Instruction* instr) {
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  VectorFormat vform = kFormatUndefined;

  switch (instr->Mask(SVEContiguousNonTemporalStore_ScalarPlusImmMask)) {
    case STNT1B_z_p_bi_contiguous:
      vform = kFormatVnB;
      break;
    case STNT1D_z_p_bi_contiguous:
      vform = kFormatVnD;
      break;
    case STNT1H_z_p_bi_contiguous:
      vform = kFormatVnH;
      break;
    case STNT1W_z_p_bi_contiguous:
      vform = kFormatVnS;
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
  int msize_in_bytes_log2 = LaneSizeInBytesLog2FromFormat(vform);
  int vl = GetVectorLengthInBytes();
  uint64_t base = ReadXRegister(instr->GetRn(), Reg31IsStackPointer);
  uint64_t offset = instr->ExtractSignedBits(19, 16) * vl;
  LogicSVEAddressVector addr(base + offset);
  addr.SetMsizeInBytesLog2(msize_in_bytes_log2);
  SVEStructuredStoreHelper(vform, pg, instr->GetRt(), addr);
}

void Simulator::VisitSVEContiguousNonTemporalStore_ScalarPlusScalar(
    const Instruction* instr) {
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  VectorFormat vform = kFormatUndefined;

  switch (instr->Mask(SVEContiguousNonTemporalStore_ScalarPlusScalarMask)) {
    case STNT1B_z_p_br_contiguous:
      vform = kFormatVnB;
      break;
    case STNT1D_z_p_br_contiguous:
      vform = kFormatVnD;
      break;
    case STNT1H_z_p_br_contiguous:
      vform = kFormatVnH;
      break;
    case STNT1W_z_p_br_contiguous:
      vform = kFormatVnS;
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
  int msize_in_bytes_log2 = LaneSizeInBytesLog2FromFormat(vform);
  uint64_t base = ReadXRegister(instr->GetRn(), Reg31IsStackPointer);
  uint64_t offset = ReadXRegister(instr->GetRm()) << msize_in_bytes_log2;
  LogicSVEAddressVector addr(base + offset);
  addr.SetMsizeInBytesLog2(msize_in_bytes_log2);
  SVEStructuredStoreHelper(vform, pg, instr->GetRt(), addr);
}

void Simulator::VisitSVEContiguousStore_ScalarPlusImm(
    const Instruction* instr) {
  switch (instr->Mask(SVEContiguousStore_ScalarPlusImmMask)) {
    case ST1B_z_p_bi:
    case ST1D_z_p_bi:
    case ST1H_z_p_bi:
    case ST1W_z_p_bi: {
      int vl = GetVectorLengthInBytes();
      int msize_in_bytes_log2 = instr->GetSVEMsizeFromDtype(false);
      int esize_in_bytes_log2 = instr->GetSVEEsizeFromDtype(false);
      VIXL_ASSERT(esize_in_bytes_log2 >= msize_in_bytes_log2);
      int vl_divisor_log2 = esize_in_bytes_log2 - msize_in_bytes_log2;
      uint64_t base = ReadXRegister(instr->GetRn(), Reg31IsStackPointer);
      uint64_t offset =
          (instr->ExtractSignedBits(19, 16) * vl) / (1 << vl_divisor_log2);
      VectorFormat vform =
          SVEFormatFromLaneSizeInBytesLog2(esize_in_bytes_log2);
      LogicSVEAddressVector addr(base + offset);
      addr.SetMsizeInBytesLog2(msize_in_bytes_log2);
      SVEStructuredStoreHelper(vform,
                               ReadPRegister(instr->GetPgLow8()),
                               instr->GetRt(),
                               addr);
      break;
    }
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEContiguousStore_ScalarPlusScalar(
    const Instruction* instr) {
  switch (instr->Mask(SVEContiguousStore_ScalarPlusScalarMask)) {
    case ST1B_z_p_br:
    case ST1D_z_p_br:
    case ST1H_z_p_br:
    case ST1W_z_p_br: {
      uint64_t base = ReadXRegister(instr->GetRn(), Reg31IsStackPointer);
      uint64_t offset = ReadXRegister(instr->GetRm());
      offset <<= instr->ExtractBits(24, 23);
      VectorFormat vform =
          SVEFormatFromLaneSizeInBytesLog2(instr->ExtractBits(22, 21));
      LogicSVEAddressVector addr(base + offset);
      addr.SetMsizeInBytesLog2(instr->ExtractBits(24, 23));
      SVEStructuredStoreHelper(vform,
                               ReadPRegister(instr->GetPgLow8()),
                               instr->GetRt(),
                               addr);
      break;
    }
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVECopySIMDFPScalarRegisterToVector_Predicated(
    const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  SimVRegister z_result;

  switch (instr->Mask(SVECopySIMDFPScalarRegisterToVector_PredicatedMask)) {
    case CPY_z_p_v:
      dup_element(vform, z_result, ReadVRegister(instr->GetRn()), 0);
      mov_merging(vform, ReadVRegister(instr->GetRd()), pg, z_result);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEStoreMultipleStructures_ScalarPlusImm(
    const Instruction* instr) {
  switch (instr->Mask(SVEStoreMultipleStructures_ScalarPlusImmMask)) {
    case ST2B_z_p_bi_contiguous:
    case ST2D_z_p_bi_contiguous:
    case ST2H_z_p_bi_contiguous:
    case ST2W_z_p_bi_contiguous:
    case ST3B_z_p_bi_contiguous:
    case ST3D_z_p_bi_contiguous:
    case ST3H_z_p_bi_contiguous:
    case ST3W_z_p_bi_contiguous:
    case ST4B_z_p_bi_contiguous:
    case ST4D_z_p_bi_contiguous:
    case ST4H_z_p_bi_contiguous:
    case ST4W_z_p_bi_contiguous: {
      int vl = GetVectorLengthInBytes();
      int msz = instr->ExtractBits(24, 23);
      int reg_count = instr->ExtractBits(22, 21) + 1;
      uint64_t offset = instr->ExtractSignedBits(19, 16) * vl * reg_count;
      LogicSVEAddressVector addr(
          ReadXRegister(instr->GetRn(), Reg31IsStackPointer) + offset);
      addr.SetMsizeInBytesLog2(msz);
      addr.SetRegCount(reg_count);
      SVEStructuredStoreHelper(SVEFormatFromLaneSizeInBytesLog2(msz),
                               ReadPRegister(instr->GetPgLow8()),
                               instr->GetRt(),
                               addr);
      break;
    }
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEStoreMultipleStructures_ScalarPlusScalar(
    const Instruction* instr) {
  switch (instr->Mask(SVEStoreMultipleStructures_ScalarPlusScalarMask)) {
    case ST2B_z_p_br_contiguous:
    case ST2D_z_p_br_contiguous:
    case ST2H_z_p_br_contiguous:
    case ST2W_z_p_br_contiguous:
    case ST3B_z_p_br_contiguous:
    case ST3D_z_p_br_contiguous:
    case ST3H_z_p_br_contiguous:
    case ST3W_z_p_br_contiguous:
    case ST4B_z_p_br_contiguous:
    case ST4D_z_p_br_contiguous:
    case ST4H_z_p_br_contiguous:
    case ST4W_z_p_br_contiguous: {
      int msz = instr->ExtractBits(24, 23);
      uint64_t offset = ReadXRegister(instr->GetRm()) * (uint64_t{1} << msz);
      VectorFormat vform = SVEFormatFromLaneSizeInBytesLog2(msz);
      LogicSVEAddressVector addr(
          ReadXRegister(instr->GetRn(), Reg31IsStackPointer) + offset);
      addr.SetMsizeInBytesLog2(msz);
      addr.SetRegCount(instr->ExtractBits(22, 21) + 1);
      SVEStructuredStoreHelper(vform,
                               ReadPRegister(instr->GetPgLow8()),
                               instr->GetRt(),
                               addr);
      break;
    }
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEStorePredicateRegister(const Instruction* instr) {
  switch (instr->Mask(SVEStorePredicateRegisterMask)) {
    case STR_p_bi: {
      SimPRegister& pt = ReadPRegister(instr->GetPt());
      int pl = GetPredicateLengthInBytes();
      int imm9 = (instr->ExtractBits(21, 16) << 3) | instr->ExtractBits(12, 10);
      uint64_t multiplier = ExtractSignedBitfield64(8, 0, imm9);
      uint64_t base = ReadXRegister(instr->GetRn(), Reg31IsStackPointer);
      uint64_t address = base + multiplier * pl;
      for (int i = 0; i < pl; i++) {
        if (!MemWrite(address + i, pt.GetLane<uint8_t>(i))) return;
      }
      LogPWrite(instr->GetPt(), address);
      break;
    }
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEStoreVectorRegister(const Instruction* instr) {
  switch (instr->Mask(SVEStoreVectorRegisterMask)) {
    case STR_z_bi: {
      SimVRegister& zt = ReadVRegister(instr->GetRt());
      int vl = GetVectorLengthInBytes();
      int imm9 = (instr->ExtractBits(21, 16) << 3) | instr->ExtractBits(12, 10);
      uint64_t multiplier = ExtractSignedBitfield64(8, 0, imm9);
      uint64_t base = ReadXRegister(instr->GetRn(), Reg31IsStackPointer);
      uint64_t address = base + multiplier * vl;
      for (int i = 0; i < vl; i++) {
        if (!MemWrite(address + i, zt.GetLane<uint8_t>(i))) return;
      }
      LogZWrite(instr->GetRt(), address);
      break;
    }
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEMulIndex(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zda = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  std::pair<int, int> zm_and_index = instr->GetSVEMulZmAndIndex();
  SimVRegister zm = ReadVRegister(zm_and_index.first);
  int index = zm_and_index.second;

  SimVRegister temp;
  dup_elements_to_segments(vform, temp, zm, index);

  switch (form_hash_) {
    case "sdot_z_zzzi_d"_h:
    case "sdot_z_zzzi_s"_h:
      sdot(vform, zda, zn, temp);
      break;
    case "udot_z_zzzi_d"_h:
    case "udot_z_zzzi_s"_h:
      udot(vform, zda, zn, temp);
      break;
    case "sudot_z_zzzi_s"_h:
      usdot(vform, zda, temp, zn);
      break;
    case "usdot_z_zzzi_s"_h:
      usdot(vform, zda, zn, temp);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::SimulateMatrixMul(const Instruction* instr) {
  VectorFormat vform = kFormatVnS;
  SimVRegister& dn = ReadVRegister(instr->GetRd());
  SimVRegister& n = ReadVRegister(instr->GetRn());
  SimVRegister& m = ReadVRegister(instr->GetRm());

  bool n_signed = false;
  bool m_signed = false;
  switch (form_hash_) {
    case "smmla_asimdsame2_g"_h:
      vform = kFormat4S;
      VIXL_FALLTHROUGH();
    case "smmla_z_zzz"_h:
      n_signed = m_signed = true;
      break;
    case "ummla_asimdsame2_g"_h:
      vform = kFormat4S;
      VIXL_FALLTHROUGH();
    case "ummla_z_zzz"_h:
      // Nothing to do.
      break;
    case "usmmla_asimdsame2_g"_h:
      vform = kFormat4S;
      VIXL_FALLTHROUGH();
    case "usmmla_z_zzz"_h:
      m_signed = true;
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
  matmul(vform, dn, n, m, n_signed, m_signed);
}

void Simulator::SimulateSVEFPMatrixMul(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zdn = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimVRegister& zm = ReadVRegister(instr->GetRm());

  switch (form_hash_) {
    case "fmmla_z_zzz_d"_h:
      if (GetVectorLengthInBits() < 256) VisitUnimplemented(instr);
      VIXL_FALLTHROUGH();
    case "fmmla_z_zzz_s"_h:
      fmatmul(vform, zdn, zn, zm);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEPartitionBreakCondition(const Instruction* instr) {
  SimPRegister& pd = ReadPRegister(instr->GetPd());
  SimPRegister& pg = ReadPRegister(instr->ExtractBits(13, 10));
  SimPRegister& pn = ReadPRegister(instr->GetPn());
  SimPRegister result;

  switch (instr->Mask(SVEPartitionBreakConditionMask)) {
    case BRKAS_p_p_p_z:
    case BRKA_p_p_p:
      brka(result, pg, pn);
      break;
    case BRKBS_p_p_p_z:
    case BRKB_p_p_p:
      brkb(result, pg, pn);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  if (instr->ExtractBit(4) == 1) {
    mov_merging(pd, pg, result);
  } else {
    mov_zeroing(pd, pg, result);
  }

  // Set flag if needed.
  if (instr->ExtractBit(22) == 1) {
    PredTest(kFormatVnB, pg, pd);
  }
}

void Simulator::VisitSVEPropagateBreakToNextPartition(
    const Instruction* instr) {
  SimPRegister& pdm = ReadPRegister(instr->GetPd());
  SimPRegister& pg = ReadPRegister(instr->ExtractBits(13, 10));
  SimPRegister& pn = ReadPRegister(instr->GetPn());

  switch (instr->Mask(SVEPropagateBreakToNextPartitionMask)) {
    case BRKNS_p_p_pp:
    case BRKN_p_p_pp:
      brkn(pdm, pg, pn);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  // Set flag if needed.
  if (instr->ExtractBit(22) == 1) {
    // Note that this ignores `pg`.
    PredTest(kFormatVnB, GetPTrue(), pdm);
  }
}

void Simulator::VisitSVEUnpackPredicateElements(const Instruction* instr) {
  SimPRegister& pd = ReadPRegister(instr->GetPd());
  SimPRegister& pn = ReadPRegister(instr->GetPn());

  SimVRegister temp = Simulator::ExpandToSimVRegister(pn);
  SimVRegister zero;
  dup_immediate(kFormatVnB, zero, 0);

  switch (instr->Mask(SVEUnpackPredicateElementsMask)) {
    case PUNPKHI_p_p:
      zip2(kFormatVnB, temp, temp, zero);
      break;
    case PUNPKLO_p_p:
      zip1(kFormatVnB, temp, temp, zero);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
  Simulator::ExtractFromSimVRegister(kFormatVnB, pd, temp);
}

void Simulator::VisitSVEPermutePredicateElements(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimPRegister& pd = ReadPRegister(instr->GetPd());
  SimPRegister& pn = ReadPRegister(instr->GetPn());
  SimPRegister& pm = ReadPRegister(instr->GetPm());

  SimVRegister temp0 = Simulator::ExpandToSimVRegister(pn);
  SimVRegister temp1 = Simulator::ExpandToSimVRegister(pm);

  switch (instr->Mask(SVEPermutePredicateElementsMask)) {
    case TRN1_p_pp:
      trn1(vform, temp0, temp0, temp1);
      break;
    case TRN2_p_pp:
      trn2(vform, temp0, temp0, temp1);
      break;
    case UZP1_p_pp:
      uzp1(vform, temp0, temp0, temp1);
      break;
    case UZP2_p_pp:
      uzp2(vform, temp0, temp0, temp1);
      break;
    case ZIP1_p_pp:
      zip1(vform, temp0, temp0, temp1);
      break;
    case ZIP2_p_pp:
      zip2(vform, temp0, temp0, temp1);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
  Simulator::ExtractFromSimVRegister(kFormatVnB, pd, temp0);
}

void Simulator::VisitSVEReversePredicateElements(const Instruction* instr) {
  switch (instr->Mask(SVEReversePredicateElementsMask)) {
    case REV_p_p: {
      VectorFormat vform = instr->GetSVEVectorFormat();
      SimPRegister& pn = ReadPRegister(instr->GetPn());
      SimPRegister& pd = ReadPRegister(instr->GetPd());
      SimVRegister temp = Simulator::ExpandToSimVRegister(pn);
      rev(vform, temp, temp);
      Simulator::ExtractFromSimVRegister(kFormatVnB, pd, temp);
      break;
    }
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEPermuteVectorExtract(const Instruction* instr) {
  SimVRegister& zdn = ReadVRegister(instr->GetRd());
  // Second source register "Zm" is encoded where "Zn" would usually be.
  SimVRegister& zm = ReadVRegister(instr->GetRn());

  int index = instr->GetSVEExtractImmediate();
  int vl = GetVectorLengthInBytes();
  index = (index >= vl) ? 0 : index;

  switch (instr->Mask(SVEPermuteVectorExtractMask)) {
    case EXT_z_zi_des:
      ext(kFormatVnB, zdn, zdn, zm, index);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEPermuteVectorInterleaving(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimVRegister& zm = ReadVRegister(instr->GetRm());

  switch (instr->Mask(SVEPermuteVectorInterleavingMask)) {
    case TRN1_z_zz:
      trn1(vform, zd, zn, zm);
      break;
    case TRN2_z_zz:
      trn2(vform, zd, zn, zm);
      break;
    case UZP1_z_zz:
      uzp1(vform, zd, zn, zm);
      break;
    case UZP2_z_zz:
      uzp2(vform, zd, zn, zm);
      break;
    case ZIP1_z_zz:
      zip1(vform, zd, zn, zm);
      break;
    case ZIP2_z_zz:
      zip2(vform, zd, zn, zm);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEConditionallyBroadcastElementToVector(
    const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zdn = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRn());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());

  int active_offset = -1;
  switch (instr->Mask(SVEConditionallyBroadcastElementToVectorMask)) {
    case CLASTA_z_p_zz:
      active_offset = 1;
      break;
    case CLASTB_z_p_zz:
      active_offset = 0;
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  if (active_offset >= 0) {
    std::pair<bool, uint64_t> value = clast(vform, pg, zm, active_offset);
    if (value.first) {
      dup_immediate(vform, zdn, value.second);
    } else {
      // Trigger a line of trace for the operation, even though it doesn't
      // change the register value.
      mov(vform, zdn, zdn);
    }
  }
}

void Simulator::VisitSVEConditionallyExtractElementToSIMDFPScalar(
    const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& vdn = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRn());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());

  int active_offset = -1;
  switch (instr->Mask(SVEConditionallyExtractElementToSIMDFPScalarMask)) {
    case CLASTA_v_p_z:
      active_offset = 1;
      break;
    case CLASTB_v_p_z:
      active_offset = 0;
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  if (active_offset >= 0) {
    LogicVRegister dst(vdn);
    uint64_t src1_value = dst.Uint(vform, 0);
    std::pair<bool, uint64_t> src2_value = clast(vform, pg, zm, active_offset);
    dup_immediate(vform, vdn, 0);
    dst.SetUint(vform, 0, src2_value.first ? src2_value.second : src1_value);
  }
}

void Simulator::VisitSVEConditionallyExtractElementToGeneralRegister(
    const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zm = ReadVRegister(instr->GetRn());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());

  int active_offset = -1;
  switch (instr->Mask(SVEConditionallyExtractElementToGeneralRegisterMask)) {
    case CLASTA_r_p_z:
      active_offset = 1;
      break;
    case CLASTB_r_p_z:
      active_offset = 0;
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  if (active_offset >= 0) {
    std::pair<bool, uint64_t> value = clast(vform, pg, zm, active_offset);
    uint64_t masked_src = ReadXRegister(instr->GetRd()) &
                          GetUintMask(LaneSizeInBitsFromFormat(vform));
    WriteXRegister(instr->GetRd(), value.first ? value.second : masked_src);
  }
}

void Simulator::VisitSVEExtractElementToSIMDFPScalarRegister(
    const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& vdn = ReadVRegister(instr->GetRd());
  SimVRegister& zm = ReadVRegister(instr->GetRn());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());

  int active_offset = -1;
  switch (instr->Mask(SVEExtractElementToSIMDFPScalarRegisterMask)) {
    case LASTA_v_p_z:
      active_offset = 1;
      break;
    case LASTB_v_p_z:
      active_offset = 0;
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  if (active_offset >= 0) {
    LogicVRegister dst(vdn);
    std::pair<bool, uint64_t> value = clast(vform, pg, zm, active_offset);
    dup_immediate(vform, vdn, 0);
    dst.SetUint(vform, 0, value.second);
  }
}

void Simulator::VisitSVEExtractElementToGeneralRegister(
    const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zm = ReadVRegister(instr->GetRn());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());

  int active_offset = -1;
  switch (instr->Mask(SVEExtractElementToGeneralRegisterMask)) {
    case LASTA_r_p_z:
      active_offset = 1;
      break;
    case LASTB_r_p_z:
      active_offset = 0;
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  if (active_offset >= 0) {
    std::pair<bool, uint64_t> value = clast(vform, pg, zm, active_offset);
    WriteXRegister(instr->GetRd(), value.second);
  }
}

void Simulator::VisitSVECompressActiveElements(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());

  switch (instr->Mask(SVECompressActiveElementsMask)) {
    case COMPACT_z_p_z:
      compact(vform, zd, pg, zn);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVECopyGeneralRegisterToVector_Predicated(
    const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  SimVRegister z_result;

  switch (instr->Mask(SVECopyGeneralRegisterToVector_PredicatedMask)) {
    case CPY_z_p_r:
      dup_immediate(vform,
                    z_result,
                    ReadXRegister(instr->GetRn(), Reg31IsStackPointer));
      mov_merging(vform, ReadVRegister(instr->GetRd()), pg, z_result);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVECopyIntImm_Predicated(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimPRegister& pg = ReadPRegister(instr->ExtractBits(19, 16));
  SimVRegister& zd = ReadVRegister(instr->GetRd());

  SimVRegister result;
  switch (instr->Mask(SVECopyIntImm_PredicatedMask)) {
    case CPY_z_p_i: {
      // Use unsigned arithmetic to avoid undefined behaviour during the shift.
      uint64_t imm8 = instr->GetImmSVEIntWideSigned();
      dup_immediate(vform, result, imm8 << (instr->ExtractBit(13) * 8));
      break;
    }
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  if (instr->ExtractBit(14) != 0) {
    mov_merging(vform, zd, pg, result);
  } else {
    mov_zeroing(vform, zd, pg, result);
  }
}

void Simulator::VisitSVEReverseWithinElements(const Instruction* instr) {
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());
  SimVRegister result;

  // In NEON, the chunk size in which elements are REVersed is in the
  // instruction mnemonic, and the element size attached to the register.
  // SVE reverses the semantics; the mapping to logic functions below is to
  // account for this.
  VectorFormat chunk_form = instr->GetSVEVectorFormat();
  VectorFormat element_form = kFormatUndefined;

  switch (instr->Mask(SVEReverseWithinElementsMask)) {
    case RBIT_z_p_z:
      rbit(chunk_form, result, zn);
      break;
    case REVB_z_z:
      VIXL_ASSERT((chunk_form == kFormatVnH) || (chunk_form == kFormatVnS) ||
                  (chunk_form == kFormatVnD));
      element_form = kFormatVnB;
      break;
    case REVH_z_z:
      VIXL_ASSERT((chunk_form == kFormatVnS) || (chunk_form == kFormatVnD));
      element_form = kFormatVnH;
      break;
    case REVW_z_z:
      VIXL_ASSERT(chunk_form == kFormatVnD);
      element_form = kFormatVnS;
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  if (instr->Mask(SVEReverseWithinElementsMask) != RBIT_z_p_z) {
    VIXL_ASSERT(element_form != kFormatUndefined);
    switch (chunk_form) {
      case kFormatVnH:
        rev16(element_form, result, zn);
        break;
      case kFormatVnS:
        rev32(element_form, result, zn);
        break;
      case kFormatVnD:
        rev64(element_form, result, zn);
        break;
      default:
        VIXL_UNIMPLEMENTED();
    }
  }

  mov_merging(chunk_form, zd, pg, result);
}

void Simulator::VisitSVEVectorSplice(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimVRegister& zn2 = ReadVRegister((instr->GetRn() + 1) % kNumberOfZRegisters);
  SimPRegister& pg = ReadPRegister(instr->GetPgLow8());

  switch (form_hash_) {
    case "splice_z_p_zz_des"_h:
      splice(vform, zd, pg, zd, zn);
      break;
    case "splice_z_p_zz_con"_h:
      splice(vform, zd, pg, zn, zn2);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEBroadcastGeneralRegister(const Instruction* instr) {
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  switch (instr->Mask(SVEBroadcastGeneralRegisterMask)) {
    case DUP_z_r:
      dup_immediate(instr->GetSVEVectorFormat(),
                    zd,
                    ReadXRegister(instr->GetRn(), Reg31IsStackPointer));
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEInsertSIMDFPScalarRegister(const Instruction* instr) {
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  VectorFormat vform = instr->GetSVEVectorFormat();
  switch (instr->Mask(SVEInsertSIMDFPScalarRegisterMask)) {
    case INSR_z_v:
      insr(vform, zd, ReadDRegisterBits(instr->GetRn()));
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEInsertGeneralRegister(const Instruction* instr) {
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  VectorFormat vform = instr->GetSVEVectorFormat();
  switch (instr->Mask(SVEInsertGeneralRegisterMask)) {
    case INSR_z_r:
      insr(vform, zd, ReadXRegister(instr->GetRn()));
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEBroadcastIndexElement(const Instruction* instr) {
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  switch (instr->Mask(SVEBroadcastIndexElementMask)) {
    case DUP_z_zi: {
      std::pair<int, int> index_and_lane_size =
          instr->GetSVEPermuteIndexAndLaneSizeLog2();
      int index = index_and_lane_size.first;
      int lane_size_in_bytes_log_2 = index_and_lane_size.second;
      VectorFormat vform =
          SVEFormatFromLaneSizeInBytesLog2(lane_size_in_bytes_log_2);
      if ((index < 0) || (index >= LaneCountFromFormat(vform))) {
        // Out of bounds, set the destination register to zero.
        dup_immediate(kFormatVnD, zd, 0);
      } else {
        dup_element(vform, zd, ReadVRegister(instr->GetRn()), index);
      }
      return;
    }
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEReverseVectorElements(const Instruction* instr) {
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  VectorFormat vform = instr->GetSVEVectorFormat();
  switch (instr->Mask(SVEReverseVectorElementsMask)) {
    case REV_z_z:
      rev(vform, zd, ReadVRegister(instr->GetRn()));
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEUnpackVectorElements(const Instruction* instr) {
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  VectorFormat vform = instr->GetSVEVectorFormat();
  switch (instr->Mask(SVEUnpackVectorElementsMask)) {
    case SUNPKHI_z_z:
      unpk(vform, zd, ReadVRegister(instr->GetRn()), kHiHalf, kSignedExtend);
      break;
    case SUNPKLO_z_z:
      unpk(vform, zd, ReadVRegister(instr->GetRn()), kLoHalf, kSignedExtend);
      break;
    case UUNPKHI_z_z:
      unpk(vform, zd, ReadVRegister(instr->GetRn()), kHiHalf, kUnsignedExtend);
      break;
    case UUNPKLO_z_z:
      unpk(vform, zd, ReadVRegister(instr->GetRn()), kLoHalf, kUnsignedExtend);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVETableLookup(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimVRegister& zn2 = ReadVRegister((instr->GetRn() + 1) % kNumberOfZRegisters);
  SimVRegister& zm = ReadVRegister(instr->GetRm());

  switch (form_hash_) {
    case "tbl_z_zz_1"_h:
      tbl(vform, zd, zn, zm);
      break;
    case "tbl_z_zz_2"_h:
      tbl(vform, zd, zn, zn2, zm);
      break;
    case "tbx_z_zz"_h:
      tbx(vform, zd, zn, zm);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEPredicateCount(const Instruction* instr) {
  VectorFormat vform = instr->GetSVEVectorFormat();
  SimPRegister& pg = ReadPRegister(instr->ExtractBits(13, 10));
  SimPRegister& pn = ReadPRegister(instr->GetPn());

  switch (instr->Mask(SVEPredicateCountMask)) {
    case CNTP_r_p_p: {
      WriteXRegister(instr->GetRd(), CountActiveAndTrueLanes(vform, pg, pn));
      break;
    }
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEPredicateLogical(const Instruction* instr) {
  Instr op = instr->Mask(SVEPredicateLogicalMask);
  SimPRegister& pd = ReadPRegister(instr->GetPd());
  SimPRegister& pg = ReadPRegister(instr->ExtractBits(13, 10));
  SimPRegister& pn = ReadPRegister(instr->GetPn());
  SimPRegister& pm = ReadPRegister(instr->GetPm());
  SimPRegister result;
  switch (op) {
    case ANDS_p_p_pp_z:
    case AND_p_p_pp_z:
    case BICS_p_p_pp_z:
    case BIC_p_p_pp_z:
    case EORS_p_p_pp_z:
    case EOR_p_p_pp_z:
    case NANDS_p_p_pp_z:
    case NAND_p_p_pp_z:
    case NORS_p_p_pp_z:
    case NOR_p_p_pp_z:
    case ORNS_p_p_pp_z:
    case ORN_p_p_pp_z:
    case ORRS_p_p_pp_z:
    case ORR_p_p_pp_z:
      SVEPredicateLogicalHelper(static_cast<SVEPredicateLogicalOp>(op),
                                result,
                                pn,
                                pm);
      break;
    case SEL_p_p_pp:
      sel(pd, pg, pn, pm);
      return;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  mov_zeroing(pd, pg, result);
  if (instr->Mask(SVEPredicateLogicalSetFlagsBit) != 0) {
    PredTest(kFormatVnB, pg, pd);
  }
}

void Simulator::VisitSVEPredicateFirstActive(const Instruction* instr) {
  LogicPRegister pg = ReadPRegister(instr->ExtractBits(8, 5));
  LogicPRegister pdn = ReadPRegister(instr->GetPd());
  switch (instr->Mask(SVEPredicateFirstActiveMask)) {
    case PFIRST_p_p_p:
      pfirst(pdn, pg, pdn);
      // TODO: Is this broken when pg == pdn?
      PredTest(kFormatVnB, pg, pdn);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEPredicateInitialize(const Instruction* instr) {
  // This group only contains PTRUE{S}, and there are no unallocated encodings.
  VIXL_STATIC_ASSERT(
      SVEPredicateInitializeMask ==
      (SVEPredicateInitializeFMask | SVEPredicateInitializeSetFlagsBit));
  VIXL_ASSERT((instr->Mask(SVEPredicateInitializeMask) == PTRUE_p_s) ||
              (instr->Mask(SVEPredicateInitializeMask) == PTRUES_p_s));

  LogicPRegister pdn = ReadPRegister(instr->GetPd());
  VectorFormat vform = instr->GetSVEVectorFormat();

  ptrue(vform, pdn, instr->GetImmSVEPredicateConstraint());
  if (instr->ExtractBit(16)) PredTest(vform, pdn, pdn);
}

void Simulator::VisitSVEPredicateNextActive(const Instruction* instr) {
  // This group only contains PNEXT, and there are no unallocated encodings.
  VIXL_STATIC_ASSERT(SVEPredicateNextActiveFMask == SVEPredicateNextActiveMask);
  VIXL_ASSERT(instr->Mask(SVEPredicateNextActiveMask) == PNEXT_p_p_p);

  LogicPRegister pg = ReadPRegister(instr->ExtractBits(8, 5));
  LogicPRegister pdn = ReadPRegister(instr->GetPd());
  VectorFormat vform = instr->GetSVEVectorFormat();

  pnext(vform, pdn, pg, pdn);
  // TODO: Is this broken when pg == pdn?
  PredTest(vform, pg, pdn);
}

void Simulator::VisitSVEPredicateReadFromFFR_Predicated(
    const Instruction* instr) {
  LogicPRegister pd(ReadPRegister(instr->GetPd()));
  LogicPRegister pg(ReadPRegister(instr->GetPn()));
  FlagsUpdate flags = LeaveFlags;
  switch (instr->Mask(SVEPredicateReadFromFFR_PredicatedMask)) {
    case RDFFR_p_p_f:
      // Do nothing.
      break;
    case RDFFRS_p_p_f:
      flags = SetFlags;
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  LogicPRegister ffr(ReadFFR());
  mov_zeroing(pd, pg, ffr);

  if (flags == SetFlags) {
    PredTest(kFormatVnB, pg, pd);
  }
}

void Simulator::VisitSVEPredicateReadFromFFR_Unpredicated(
    const Instruction* instr) {
  LogicPRegister pd(ReadPRegister(instr->GetPd()));
  LogicPRegister ffr(ReadFFR());
  switch (instr->Mask(SVEPredicateReadFromFFR_UnpredicatedMask)) {
    case RDFFR_p_f:
      mov(pd, ffr);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEPredicateTest(const Instruction* instr) {
  switch (instr->Mask(SVEPredicateTestMask)) {
    case PTEST_p_p:
      PredTest(kFormatVnB,
               ReadPRegister(instr->ExtractBits(13, 10)),
               ReadPRegister(instr->GetPn()));
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEPredicateZero(const Instruction* instr) {
  switch (instr->Mask(SVEPredicateZeroMask)) {
    case PFALSE_p:
      pfalse(ReadPRegister(instr->GetPd()));
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEPropagateBreak(const Instruction* instr) {
  SimPRegister& pd = ReadPRegister(instr->GetPd());
  SimPRegister& pg = ReadPRegister(instr->ExtractBits(13, 10));
  SimPRegister& pn = ReadPRegister(instr->GetPn());
  SimPRegister& pm = ReadPRegister(instr->GetPm());

  bool set_flags = false;
  switch (instr->Mask(SVEPropagateBreakMask)) {
    case BRKPAS_p_p_pp:
      set_flags = true;
      VIXL_FALLTHROUGH();
    case BRKPA_p_p_pp:
      brkpa(pd, pg, pn, pm);
      break;
    case BRKPBS_p_p_pp:
      set_flags = true;
      VIXL_FALLTHROUGH();
    case BRKPB_p_p_pp:
      brkpb(pd, pg, pn, pm);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  if (set_flags) {
    PredTest(kFormatVnB, pg, pd);
  }
}

void Simulator::VisitSVEStackFrameAdjustment(const Instruction* instr) {
  uint64_t length = 0;
  switch (instr->Mask(SVEStackFrameAdjustmentMask)) {
    case ADDPL_r_ri:
      length = GetPredicateLengthInBytes();
      break;
    case ADDVL_r_ri:
      length = GetVectorLengthInBytes();
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
  uint64_t base = ReadXRegister(instr->GetRm(), Reg31IsStackPointer);
  WriteXRegister(instr->GetRd(),
                 base + (length * instr->GetImmSVEVLScale()),
                 LogRegWrites,
                 Reg31IsStackPointer);
}

void Simulator::VisitSVEStackFrameSize(const Instruction* instr) {
  int64_t scale = instr->GetImmSVEVLScale();

  switch (instr->Mask(SVEStackFrameSizeMask)) {
    case RDVL_r_i:
      WriteXRegister(instr->GetRd(), GetVectorLengthInBytes() * scale);
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }
}

void Simulator::VisitSVEVectorSelect(const Instruction* instr) {
  // The only instruction in this group is `sel`, and there are no unused
  // encodings.
  VIXL_ASSERT(instr->Mask(SVEVectorSelectMask) == SEL_z_p_zz);

  VectorFormat vform = instr->GetSVEVectorFormat();
  SimVRegister& zd = ReadVRegister(instr->GetRd());
  SimPRegister& pg = ReadPRegister(instr->ExtractBits(13, 10));
  SimVRegister& zn = ReadVRegister(instr->GetRn());
  SimVRegister& zm = ReadVRegister(instr->GetRm());

  sel(vform, zd, pg, zn, zm);
}

void Simulator::VisitSVEFFRInitialise(const Instruction* instr) {
  switch (instr->Mask(SVEFFRInitialiseMask)) {
    case SETFFR_f: {
      LogicPRegister ffr(ReadFFR());
      ffr.SetAllBits();
      break;
    }
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEFFRWriteFromPredicate(const Instruction* instr) {
  switch (instr->Mask(SVEFFRWriteFromPredicateMask)) {
    case WRFFR_f_p: {
      SimPRegister pn(ReadPRegister(instr->GetPn()));
      bool last_active = true;
      for (unsigned i = 0; i < pn.GetSizeInBits(); i++) {
        bool active = pn.GetBit(i);
        if (active && !last_active) {
          // `pn` is non-monotonic. This is UNPREDICTABLE.
          VIXL_ABORT();
        }
        last_active = active;
      }
      mov(ReadFFR(), pn);
      break;
    }
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }
}

void Simulator::VisitSVEContiguousLoad_ScalarPlusImm(const Instruction* instr) {
  bool is_signed;
  switch (instr->Mask(SVEContiguousLoad_ScalarPlusImmMask)) {
    case LD1B_z_p_bi_u8:
    case LD1B_z_p_bi_u16:
    case LD1B_z_p_bi_u32:
    case LD1B_z_p_bi_u64:
    case LD1H_z_p_bi_u16:
    case LD1H_z_p_bi_u32:
    case LD1H_z_p_bi_u64:
    case LD1W_z_p_bi_u32:
    case LD1W_z_p_bi_u64:
    case LD1D_z_p_bi_u64:
      is_signed = false;
      break;
    case LD1SB_z_p_bi_s16:
    case LD1SB_z_p_bi_s32:
    case LD1SB_z_p_bi_s64:
    case LD1SH_z_p_bi_s32:
    case LD1SH_z_p_bi_s64:
    case LD1SW_z_p_bi_s64:
      is_signed = true;
      break;
    default:
      // This encoding group is complete, so no other values should be possible.
      VIXL_UNREACHABLE();
      is_signed = false;
      break;
  }

  int vl = GetVectorLengthInBytes();
  int msize_in_bytes_log2 = instr->GetSVEMsizeFromDtype(is_signed);
  int esize_in_bytes_log2 = instr->GetSVEEsizeFromDtype(is_signed);
  VIXL_ASSERT(esize_in_bytes_log2 >= msize_in_bytes_log2);
  int vl_divisor_log2 = esize_in_bytes_log2 - msize_in_bytes_log2;
  uint64_t base = ReadXRegister(instr->GetRn(), Reg31IsStackPointer);
  uint64_t offset =
      (instr->ExtractSignedBits(19, 16) * vl) / (1 << vl_divisor_log2);
  VectorFormat vform = SVEFormatFromLaneSizeInBytesLog2(esize_in_bytes_log2);
  LogicSVEAddressVector addr(base + offset);
  addr.SetMsizeInBytesLog2(msize_in_bytes_log2);
  SVEStructuredLoadHelper(vform,
                          ReadPRegister(instr->GetPgLow8()),
                          instr->GetRt(),
                          addr,
                          is_signed);
}

void Simulator::VisitSVEContiguousLoad_ScalarPlusScalar(
    const Instruction* instr) {
  bool is_signed;
  switch (instr->Mask(SVEContiguousLoad_ScalarPlusScalarMask)) {
    case LD1B_z_p_br_u8:
    case LD1B_z_p_br_u16:
    case LD1B_z_p_br_u32:
    case LD1B_z_p_br_u64:
    case LD1H_z_p_br_u16:
    case LD1H_z_p_br_u32:
    case LD1H_z_p_br_u64:
    case LD1W_z_p_br_u32:
    case LD1W_z_p_br_u64:
    case LD1D_z_p_br_u64:
      is_signed = false;
      break;
    case LD1SB_z_p_br_s16:
    case LD1SB_z_p_br_s32:
    case LD1SB_z_p_br_s64:
    case LD1SH_z_p_br_s32:
    case LD1SH_z_p_br_s64:
    case LD1SW_z_p_br_s64:
      is_signed = true;
      break;
    default:
      // This encoding group is complete, so no other values should be possible.
      VIXL_UNREACHABLE();
      is_signed = false;
      break;
  }

  int msize_in_bytes_log2 = instr->GetSVEMsizeFromDtype(is_signed);
  int esize_in_bytes_log2 = instr->GetSVEEsizeFromDtype(is_signed);
  VIXL_ASSERT(msize_in_bytes_log2 <= esize_in_bytes_log2);
  VectorFormat vform = SVEFormatFromLaneSizeInBytesLog2(esize_in_bytes_log2);
  uint64_t base = ReadXRegister(instr->GetRn(), Reg31IsStackPointer);
  uint64_t offset = ReadXRegister(instr->GetRm());
  offset <<= msize_in_bytes_log2;
  LogicSVEAddressVector addr(base + offset);
  addr.SetMsizeInBytesLog2(msize_in_bytes_log2);
  SVEStructuredLoadHelper(vform,
                          ReadPRegister(instr->GetPgLow8()),
                          instr->GetRt(),
                          addr,
                          is_signed);
}

void Simulator::DoUnreachable(const Instruction* instr) {
  VIXL_ASSERT((instr->Mask(ExceptionMask) == HLT) &&
              (instr->GetImmException() == kUnreachableOpcode));

  fprintf(stream_,
          "Hit UNREACHABLE marker at pc=%p.\n",
          reinterpret_cast<const void*>(instr));
  abort();
}

void Simulator::Simulate_XdSP_XnSP_Xm(const Instruction* instr) {
  VIXL_ASSERT(form_hash_ == Hash("irg_64i_dp_2src"));
  uint64_t rn = ReadXRegister(instr->GetRn(), Reg31IsStackPointer);
  uint64_t rm = ReadXRegister(instr->GetRm());
  uint64_t tag = GenerateRandomTag(rm & 0xffff);
  uint64_t new_val = GetAddressWithAllocationTag(rn, tag);
  WriteXRegister(instr->GetRd(), new_val, LogRegWrites, Reg31IsStackPointer);
}

void Simulator::SimulateMTEAddSubTag(const Instruction* instr) {
  uint64_t rn = ReadXRegister(instr->GetRn(), Reg31IsStackPointer);
  uint64_t rn_tag = GetAllocationTagFromAddress(rn);
  uint64_t tag_offset = instr->ExtractBits(13, 10);
  // TODO: implement GCR_EL1.Exclude to provide a tag exclusion list.
  uint64_t new_tag = ChooseNonExcludedTag(rn_tag, tag_offset);

  uint64_t offset = instr->ExtractBits(21, 16) * kMTETagGranuleInBytes;
  int carry = 0;
  if (form_hash_ == Hash("subg_64_addsub_immtags")) {
    offset = ~offset;
    carry = 1;
  } else {
    VIXL_ASSERT(form_hash_ == Hash("addg_64_addsub_immtags"));
  }
  uint64_t new_val =
      AddWithCarry(kXRegSize, /* set_flags = */ false, rn, offset, carry);
  new_val = GetAddressWithAllocationTag(new_val, new_tag);
  WriteXRegister(instr->GetRd(), new_val, LogRegWrites, Reg31IsStackPointer);
}

void Simulator::SimulateMTETagMaskInsert(const Instruction* instr) {
  VIXL_ASSERT(form_hash_ == Hash("gmi_64g_dp_2src"));
  uint64_t mask = ReadXRegister(instr->GetRm());
  uint64_t tag = GetAllocationTagFromAddress(
      ReadXRegister(instr->GetRn(), Reg31IsStackPointer));
  uint64_t mask_bit = uint64_t{1} << tag;
  WriteXRegister(instr->GetRd(), mask | mask_bit);
}

void Simulator::SimulateMTESubPointer(const Instruction* instr) {
  uint64_t rn = ReadXRegister(instr->GetRn(), Reg31IsStackPointer);
  uint64_t rm = ReadXRegister(instr->GetRm(), Reg31IsStackPointer);

  VIXL_ASSERT((form_hash_ == Hash("subps_64s_dp_2src")) ||
              (form_hash_ == Hash("subp_64s_dp_2src")));
  bool set_flags = (form_hash_ == Hash("subps_64s_dp_2src"));

  rn = ExtractSignedBitfield64(55, 0, rn);
  rm = ExtractSignedBitfield64(55, 0, rm);
  uint64_t new_val = AddWithCarry(kXRegSize, set_flags, rn, ~rm, 1);
  WriteXRegister(instr->GetRd(), new_val);
}

void Simulator::SimulateMTEStoreTagPair(const Instruction* instr) {
  uint64_t rn = ReadXRegister(instr->GetRn(), Reg31IsStackPointer);
  uint64_t rt = ReadXRegister(instr->GetRt());
  uint64_t rt2 = ReadXRegister(instr->GetRt2());
  int offset = instr->GetImmLSPair() * static_cast<int>(kMTETagGranuleInBytes);

  AddrMode addr_mode = Offset;
  switch (form_hash_) {
    case Hash("stgp_64_ldstpair_off"):
      // Default is the offset mode.
      break;
    case Hash("stgp_64_ldstpair_post"):
      addr_mode = PostIndex;
      break;
    case Hash("stgp_64_ldstpair_pre"):
      addr_mode = PreIndex;
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }

  uintptr_t address = AddressModeHelper(instr->GetRn(), offset, addr_mode);
  if (!IsAligned(address, kMTETagGranuleInBytes)) {
    VIXL_ALIGNMENT_EXCEPTION();
  }

  int tag = GetAllocationTagFromAddress(rn);
  meta_data_.SetMTETag(address, tag);

  if (!MemWrite<uint64_t>(address, rt)) return;
  if (!MemWrite<uint64_t>(address + kXRegSizeInBytes, rt2)) return;
}

void Simulator::SimulateMTEStoreTag(const Instruction* instr) {
  uint64_t rt = ReadXRegister(instr->GetRt(), Reg31IsStackPointer);
  int offset = instr->GetImmLS() * static_cast<int>(kMTETagGranuleInBytes);

  AddrMode addr_mode = Offset;
  switch (form_hash_) {
    case Hash("st2g_64soffset_ldsttags"):
    case Hash("stg_64soffset_ldsttags"):
    case Hash("stz2g_64soffset_ldsttags"):
    case Hash("stzg_64soffset_ldsttags"):
      // Default is the offset mode.
      break;
    case Hash("st2g_64spost_ldsttags"):
    case Hash("stg_64spost_ldsttags"):
    case Hash("stz2g_64spost_ldsttags"):
    case Hash("stzg_64spost_ldsttags"):
      addr_mode = PostIndex;
      break;
    case Hash("st2g_64spre_ldsttags"):
    case Hash("stg_64spre_ldsttags"):
    case Hash("stz2g_64spre_ldsttags"):
    case Hash("stzg_64spre_ldsttags"):
      addr_mode = PreIndex;
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }

  bool is_pair = false;
  switch (form_hash_) {
    case Hash("st2g_64soffset_ldsttags"):
    case Hash("st2g_64spost_ldsttags"):
    case Hash("st2g_64spre_ldsttags"):
    case Hash("stz2g_64soffset_ldsttags"):
    case Hash("stz2g_64spost_ldsttags"):
    case Hash("stz2g_64spre_ldsttags"):
      is_pair = true;
      break;
    default:
      break;
  }

  bool is_zeroing = false;
  switch (form_hash_) {
    case Hash("stz2g_64soffset_ldsttags"):
    case Hash("stz2g_64spost_ldsttags"):
    case Hash("stz2g_64spre_ldsttags"):
    case Hash("stzg_64soffset_ldsttags"):
    case Hash("stzg_64spost_ldsttags"):
    case Hash("stzg_64spre_ldsttags"):
      is_zeroing = true;
      break;
    default:
      break;
  }

  uintptr_t address = AddressModeHelper(instr->GetRn(), offset, addr_mode);

  if (is_zeroing) {
    if (!IsAligned(address, kMTETagGranuleInBytes)) {
      VIXL_ALIGNMENT_EXCEPTION();
    }
    VIXL_STATIC_ASSERT(kMTETagGranuleInBytes >= sizeof(uint64_t));
    VIXL_STATIC_ASSERT(kMTETagGranuleInBytes % sizeof(uint64_t) == 0);

    size_t fill_size = kMTETagGranuleInBytes;
    if (is_pair) {
      fill_size += kMTETagGranuleInBytes;
    }

    size_t fill_offset = 0;
    while (fill_offset < fill_size) {
      if (!MemWrite<uint64_t>(address + fill_offset, 0)) return;
      fill_offset += sizeof(uint64_t);
    }
  }

  int tag = GetAllocationTagFromAddress(rt);
  meta_data_.SetMTETag(address, tag, instr);
  if (is_pair) {
    meta_data_.SetMTETag(address + kMTETagGranuleInBytes, tag, instr);
  }
}

void Simulator::SimulateMTELoadTag(const Instruction* instr) {
  uint64_t rt = ReadXRegister(instr->GetRt());
  int offset = instr->GetImmLS() * static_cast<int>(kMTETagGranuleInBytes);

  switch (form_hash_) {
    case Hash("ldg_64loffset_ldsttags"):
      break;
    default:
      VIXL_UNIMPLEMENTED();
  }

  uintptr_t address = AddressModeHelper(instr->GetRn(), offset, Offset);
  address = AlignDown(address, kMTETagGranuleInBytes);
  uint64_t tag = meta_data_.GetMTETag(address, instr);
  WriteXRegister(instr->GetRt(), GetAddressWithAllocationTag(rt, tag));
}

void Simulator::SimulateCpyFP(const Instruction* instr) {
  MOPSPHelper<"cpy"_h>(instr);
  LogSystemRegister(NZCV);
}

void Simulator::SimulateCpyP(const Instruction* instr) {
  MOPSPHelper<"cpy"_h>(instr);

  int d = instr->GetRd();
  int n = instr->GetRn();
  int s = instr->GetRs();

  // Determine copy direction. For cases in which direction is implementation
  // defined, use forward.
  bool is_backwards = false;
  uint64_t xs = ReadXRegister(s);
  uint64_t xd = ReadXRegister(d);
  uint64_t xn = ReadXRegister(n);

  // Ignore the top byte of addresses for comparisons. We can use xn as is,
  // as it should have zero in bits 63:55.
  uint64_t xs_tbi = ExtractUnsignedBitfield64(55, 0, xs);
  uint64_t xd_tbi = ExtractUnsignedBitfield64(55, 0, xd);
  VIXL_ASSERT(ExtractUnsignedBitfield64(63, 55, xn) == 0);
  if ((xs_tbi < xd_tbi) && ((xs_tbi + xn) > xd_tbi)) {
    is_backwards = true;
    WriteXRegister(s, xs + xn);
    WriteXRegister(d, xd + xn);
  }

  ReadNzcv().SetN(is_backwards ? 1 : 0);
  LogSystemRegister(NZCV);
}

void Simulator::SimulateCpyM(const Instruction* instr) {
  VIXL_ASSERT(instr->IsConsistentMOPSTriplet<"cpy"_h>());
  VIXL_ASSERT(instr->IsMOPSMainOf(GetLastExecutedInstruction(), "cpy"_h));

  int d = instr->GetRd();
  int n = instr->GetRn();
  int s = instr->GetRs();

  uint64_t xd = ReadXRegister(d);
  uint64_t xn = ReadXRegister(n);
  uint64_t xs = ReadXRegister(s);
  bool is_backwards = ReadN();

  int step = 1;
  if (is_backwards) {
    step = -1;
    xs--;
    xd--;
  }

  while (xn--) {
    VIXL_DEFINE_OR_RETURN(temp, MemRead<uint8_t>(xs));
    if (!MemWrite<uint8_t>(xd, temp)) return;
    LogMemTransfer(xd, xs, temp);
    xs += step;
    xd += step;
  }

  if (is_backwards) {
    xs++;
    xd++;
  }

  WriteXRegister(d, xd);
  WriteXRegister(n, 0);
  WriteXRegister(s, xs);
}

void Simulator::SimulateCpyE(const Instruction* instr) {
  USE(instr);
  VIXL_ASSERT(instr->IsConsistentMOPSTriplet<"cpy"_h>());
  VIXL_ASSERT(instr->IsMOPSEpilogueOf(GetLastExecutedInstruction(), "cpy"_h));
  // This implementation does nothing in the epilogue; all copying is completed
  // in the "main" part.
}

void Simulator::SimulateSetP(const Instruction* instr) {
  MOPSPHelper<"set"_h>(instr);
  LogSystemRegister(NZCV);
}

void Simulator::SimulateSetM(const Instruction* instr) {
  VIXL_ASSERT(instr->IsConsistentMOPSTriplet<"set"_h>());
  VIXL_ASSERT(instr->IsMOPSMainOf(GetLastExecutedInstruction(), "set"_h));

  uint64_t xd = ReadXRegister(instr->GetRd());
  uint64_t xn = ReadXRegister(instr->GetRn());
  uint64_t xs = ReadXRegister(instr->GetRs());

  while (xn--) {
    LogWrite(instr->GetRs(), GetPrintRegPartial(kPrintRegLaneSizeB), xd);
    if (!MemWrite<uint8_t>(xd++, static_cast<uint8_t>(xs))) return;
  }
  WriteXRegister(instr->GetRd(), xd);
  WriteXRegister(instr->GetRn(), 0);
}

void Simulator::SimulateSetE(const Instruction* instr) {
  USE(instr);
  VIXL_ASSERT(instr->IsConsistentMOPSTriplet<"set"_h>());
  VIXL_ASSERT(instr->IsMOPSEpilogueOf(GetLastExecutedInstruction(), "set"_h));
  // This implementation does nothing in the epilogue; all setting is completed
  // in the "main" part.
}

void Simulator::SimulateSetGP(const Instruction* instr) {
  MOPSPHelper<"setg"_h>(instr);

  uint64_t xd = ReadXRegister(instr->GetRd());
  uint64_t xn = ReadXRegister(instr->GetRn());

  if ((xn > 0) && !IsAligned(xd, kMTETagGranuleInBytes)) {
    VIXL_ALIGNMENT_EXCEPTION();
  }

  if (!IsAligned(xn, kMTETagGranuleInBytes)) {
    VIXL_ALIGNMENT_EXCEPTION();
  }

  LogSystemRegister(NZCV);
}

void Simulator::SimulateSetGM(const Instruction* instr) {
  uint64_t xd = ReadXRegister(instr->GetRd());
  uint64_t xn = ReadXRegister(instr->GetRn());

  int tag = GetAllocationTagFromAddress(xd);
  while (xn) {
    meta_data_.SetMTETag(xd, tag);
    xd += 16;
    xn -= 16;
  }
  SimulateSetM(instr);
}

void Simulator::DoTrace(const Instruction* instr) {
  VIXL_ASSERT((instr->Mask(ExceptionMask) == HLT) &&
              (instr->GetImmException() == kTraceOpcode));

  // Read the arguments encoded inline in the instruction stream.
  uint32_t parameters;
  uint32_t command;

  VIXL_STATIC_ASSERT(sizeof(*instr) == 1);
  memcpy(&parameters, instr + kTraceParamsOffset, sizeof(parameters));
  memcpy(&command, instr + kTraceCommandOffset, sizeof(command));

  switch (command) {
    case TRACE_ENABLE:
      SetTraceParameters(GetTraceParameters() | parameters);
      break;
    case TRACE_DISABLE:
      SetTraceParameters(GetTraceParameters() & ~parameters);
      break;
    default:
      VIXL_UNREACHABLE();
  }

  WritePc(instr->GetInstructionAtOffset(kTraceLength));
}


void Simulator::DoLog(const Instruction* instr) {
  VIXL_ASSERT((instr->Mask(ExceptionMask) == HLT) &&
              (instr->GetImmException() == kLogOpcode));

  // Read the arguments encoded inline in the instruction stream.
  uint32_t parameters;

  VIXL_STATIC_ASSERT(sizeof(*instr) == 1);
  memcpy(&parameters, instr + kTraceParamsOffset, sizeof(parameters));

  // We don't support a one-shot LOG_DISASM.
  VIXL_ASSERT((parameters & LOG_DISASM) == 0);
  // Print the requested information.
  if (parameters & LOG_SYSREGS) PrintSystemRegisters();
  if (parameters & LOG_REGS) PrintRegisters();
  if (parameters & LOG_VREGS) PrintVRegisters();

  WritePc(instr->GetInstructionAtOffset(kLogLength));
}


void Simulator::DoPrintf(const Instruction* instr) {
  VIXL_ASSERT((instr->Mask(ExceptionMask) == HLT) &&
              (instr->GetImmException() == kPrintfOpcode));

  // Read the arguments encoded inline in the instruction stream.
  uint32_t arg_count;
  uint32_t arg_pattern_list;
  VIXL_STATIC_ASSERT(sizeof(*instr) == 1);
  memcpy(&arg_count, instr + kPrintfArgCountOffset, sizeof(arg_count));
  memcpy(&arg_pattern_list,
         instr + kPrintfArgPatternListOffset,
         sizeof(arg_pattern_list));

  VIXL_ASSERT(arg_count <= kPrintfMaxArgCount);
  VIXL_ASSERT((arg_pattern_list >> (kPrintfArgPatternBits * arg_count)) == 0);

  // We need to call the host printf function with a set of arguments defined by
  // arg_pattern_list. Because we don't know the types and sizes of the
  // arguments, this is very difficult to do in a robust and portable way. To
  // work around the problem, we pick apart the format string, and print one
  // format placeholder at a time.

  // Allocate space for the format string. We take a copy, so we can modify it.
  // Leave enough space for one extra character per expected argument (plus the
  // '\0' termination).
  const char* format_base = ReadRegister<const char*>(0);
  VIXL_ASSERT(format_base != NULL);
  size_t length = strlen(format_base) + 1;
  char* const format = new char[length + arg_count];

  // A list of chunks, each with exactly one format placeholder.
  const char* chunks[kPrintfMaxArgCount];

  // Copy the format string and search for format placeholders.
  uint32_t placeholder_count = 0;
  char* format_scratch = format;
  for (size_t i = 0; i < length; i++) {
    if (format_base[i] != '%') {
      *format_scratch++ = format_base[i];
    } else {
      if (format_base[i + 1] == '%') {
        // Ignore explicit "%%" sequences.
        *format_scratch++ = format_base[i];
        i++;
        // Chunks after the first are passed as format strings to printf, so we
        // need to escape '%' characters in those chunks.
        if (placeholder_count > 0) *format_scratch++ = format_base[i];
      } else {
        VIXL_CHECK(placeholder_count < arg_count);
        // Insert '\0' before placeholders, and store their locations.
        *format_scratch++ = '\0';
        chunks[placeholder_count++] = format_scratch;
        *format_scratch++ = format_base[i];
      }
    }
  }
  VIXL_CHECK(placeholder_count == arg_count);

  // Finally, call printf with each chunk, passing the appropriate register
  // argument. Normally, printf returns the number of bytes transmitted, so we
  // can emulate a single printf call by adding the result from each chunk. If
  // any call returns a negative (error) value, though, just return that value.

  printf("%s", clr_printf);

  // Because '\0' is inserted before each placeholder, the first string in
  // 'format' contains no format placeholders and should be printed literally.
  int result = printf("%s", format);
  int pcs_r = 1;  // Start at x1. x0 holds the format string.
  int pcs_f = 0;  // Start at d0.
  if (result >= 0) {
    for (uint32_t i = 0; i < placeholder_count; i++) {
      int part_result = -1;

      uint32_t arg_pattern = arg_pattern_list >> (i * kPrintfArgPatternBits);
      arg_pattern &= (1 << kPrintfArgPatternBits) - 1;
      switch (arg_pattern) {
        case kPrintfArgW:
          part_result = printf(chunks[i], ReadWRegister(pcs_r++));
          break;
        case kPrintfArgX:
          part_result = printf(chunks[i], ReadXRegister(pcs_r++));
          break;
        case kPrintfArgD:
          part_result = printf(chunks[i], ReadDRegister(pcs_f++));
          break;
        default:
          VIXL_UNREACHABLE();
      }

      if (part_result < 0) {
        // Handle error values.
        result = part_result;
        break;
      }

      result += part_result;
    }
  }

  printf("%s", clr_normal);

  // Printf returns its result in x0 (just like the C library's printf).
  WriteXRegister(0, result);

  // The printf parameters are inlined in the code, so skip them.
  WritePc(instr->GetInstructionAtOffset(kPrintfLength));

  // Set LR as if we'd just called a native printf function.
  WriteLr(ReadPc());

  delete[] format;
}


#ifdef VIXL_HAS_SIMULATED_RUNTIME_CALL_SUPPORT
void Simulator::DoRuntimeCall(const Instruction* instr) {
  VIXL_STATIC_ASSERT(kRuntimeCallAddressSize == sizeof(uintptr_t));
  // The appropriate `Simulator::SimulateRuntimeCall()` wrapper and the function
  // to call are passed inlined in the assembly.
  VIXL_DEFINE_OR_RETURN(call_wrapper_address,
                        MemRead<uintptr_t>(instr + kRuntimeCallWrapperOffset));
  VIXL_DEFINE_OR_RETURN(function_address,
                        MemRead<uintptr_t>(instr + kRuntimeCallFunctionOffset));
  VIXL_DEFINE_OR_RETURN(call_type,
                        MemRead<uint32_t>(instr + kRuntimeCallTypeOffset));
  auto runtime_call_wrapper =
      reinterpret_cast<void (*)(Simulator*, uintptr_t)>(call_wrapper_address);

  if (static_cast<RuntimeCallType>(call_type) == kCallRuntime) {
    const Instruction* addr = instr->GetInstructionAtOffset(kRuntimeCallLength);
    WriteLr(addr);
    GCSPush(reinterpret_cast<uint64_t>(addr));
  }
  runtime_call_wrapper(this, function_address);
  // Read the return address from `lr` and write it into `pc`.
  uint64_t addr = ReadRegister<uint64_t>(kLinkRegCode);
  if (IsGCSCheckEnabled()) {
    uint64_t expected_lr = GCSPeek();
    char msg[128];
    if (expected_lr != 0) {
      if ((expected_lr & 0x3) != 0) {
        snprintf(msg,
                 sizeof(msg),
                 "GCS contains misaligned return address: 0x%016" PRIx64 "\n",
                 expected_lr);
        ReportGCSFailure(msg);
      } else if ((addr != 0) && (addr != expected_lr)) {
        snprintf(msg,
                 sizeof(msg),
                 "GCS mismatch: lr = 0x%016" PRIx64 ", gcs = 0x%016" PRIx64
                 "\n",
                 addr,
                 expected_lr);
        ReportGCSFailure(msg);
      }
      GCSPop();
    }
  }
  WritePc(reinterpret_cast<Instruction*>(addr));
}
#else
void Simulator::DoRuntimeCall(const Instruction* instr) {
  USE(instr);
  VIXL_UNREACHABLE();
}
#endif


void Simulator::DoConfigureCPUFeatures(const Instruction* instr) {
  VIXL_ASSERT(instr->Mask(ExceptionMask) == HLT);

  typedef ConfigureCPUFeaturesElementType ElementType;
  VIXL_ASSERT(CPUFeatures::kNumberOfFeatures <
              std::numeric_limits<ElementType>::max());

  // k{Set,Enable,Disable}CPUFeatures have the same parameter encoding.

  size_t element_size = sizeof(ElementType);
  size_t offset = kConfigureCPUFeaturesListOffset;

  // Read the kNone-terminated list of features.
  CPUFeatures parameters;
  while (true) {
    VIXL_DEFINE_OR_RETURN(feature, MemRead<ElementType>(instr + offset));
    offset += element_size;
    if (feature == static_cast<ElementType>(CPUFeatures::kNone)) break;
    parameters.Combine(static_cast<CPUFeatures::Feature>(feature));
  }

  switch (instr->GetImmException()) {
    case kSetCPUFeaturesOpcode:
      SetCPUFeatures(parameters);
      break;
    case kEnableCPUFeaturesOpcode:
      GetCPUFeatures()->Combine(parameters);
      break;
    case kDisableCPUFeaturesOpcode:
      GetCPUFeatures()->Remove(parameters);
      break;
    default:
      VIXL_UNREACHABLE();
      break;
  }

  WritePc(instr->GetInstructionAtOffset(AlignUp(offset, kInstructionSize)));
}


void Simulator::DoSaveCPUFeatures(const Instruction* instr) {
  VIXL_ASSERT((instr->Mask(ExceptionMask) == HLT) &&
              (instr->GetImmException() == kSaveCPUFeaturesOpcode));
  USE(instr);

  saved_cpu_features_.push_back(*GetCPUFeatures());
}


void Simulator::DoRestoreCPUFeatures(const Instruction* instr) {
  VIXL_ASSERT((instr->Mask(ExceptionMask) == HLT) &&
              (instr->GetImmException() == kRestoreCPUFeaturesOpcode));
  USE(instr);

  SetCPUFeatures(saved_cpu_features_.back());
  saved_cpu_features_.pop_back();
}

#ifdef VIXL_HAS_SIMULATED_MMAP
void* Simulator::Mmap(
    void* address, size_t length, int prot, int flags, int fd, off_t offset) {
  // The underlying system `mmap` in the simulated environment doesn't recognize
  // PROT_BTI and PROT_MTE. Although the kernel probably just ignores the bits
  // it doesn't know, mask those protections out before calling is safer.
  int intenal_prot = prot;
  prot &= ~(PROT_BTI | PROT_MTE);

  uint64_t address2 = reinterpret_cast<uint64_t>(
      mmap(address, length, prot, flags, fd, offset));

  if (intenal_prot & PROT_MTE) {
    // The returning address of `mmap` isn't tagged.
    int tag = static_cast<int>(GenerateRandomTag());
    SetGranuleTag(address2, tag, length);
    address2 = GetAddressWithAllocationTag(address2, tag);
  }

  return reinterpret_cast<void*>(address2);
}


int Simulator::Munmap(void* address, size_t length, int prot) {
  if (prot & PROT_MTE) {
    // Untag the address since `munmap` doesn't recognize the memory tagging
    // managed by the Simulator.
    address = AddressUntag(address);
    CleanGranuleTag(reinterpret_cast<char*>(address), length);
  }

  return munmap(address, length);
}
#endif  // VIXL_HAS_SIMULATED_MMAP

}  // namespace aarch64
}  // namespace vixl

#endif  // VIXL_INCLUDE_SIMULATOR_AARCH64
