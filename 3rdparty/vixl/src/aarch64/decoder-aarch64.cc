// Copyright 2019, VIXL authors
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

#include "decoder-aarch64.h"

#include <string>

#include "../globals-vixl.h"
#include "../utils-vixl.h"

#include "decoder-constants-aarch64.h"

namespace vixl {
namespace aarch64 {

void Decoder::Decode(const Instruction* instr) {
  std::list<DecoderVisitor*>::iterator it;
  for (it = visitors_.begin(); it != visitors_.end(); it++) {
    VIXL_ASSERT((*it)->IsConstVisitor());
  }
  VIXL_ASSERT(compiled_decoder_root_ != NULL);
  compiled_decoder_root_->Decode(instr);
}

void Decoder::Decode(Instruction* instr) {
  compiled_decoder_root_->Decode(const_cast<const Instruction*>(instr));
}

void Decoder::AddDecodeNode(const DecodeNode& node) {
  if (decode_nodes_.count(node.GetName()) == 0) {
    decode_nodes_.insert(std::make_pair(node.GetName(), node));
  }
}

DecodeNode* Decoder::GetDecodeNode(std::string name) {
  if (decode_nodes_.count(name) != 1) {
    std::string msg = "Can't find decode node " + name + ".\n";
    VIXL_ABORT_WITH_MSG(msg.c_str());
  }
  return &decode_nodes_[name];
}

void Decoder::ConstructDecodeGraph() {
  // Add all of the decoding nodes to the Decoder.
  for (unsigned i = 0; i < ArrayLength(kDecodeMapping); i++) {
    AddDecodeNode(DecodeNode(kDecodeMapping[i], this));

    // Add a node for each instruction form named, identified by having no '_'
    // prefix on the node name.
    const DecodeMapping& map = kDecodeMapping[i];
    for (unsigned j = 0; j < map.mapping.size(); j++) {
      if ((map.mapping[j].handler != NULL) &&
          (map.mapping[j].handler[0] != '_')) {
        AddDecodeNode(DecodeNode(map.mapping[j].handler, this));
      }
    }
  }

  // Add an "unallocated" node, used when an instruction encoding is not
  // recognised by the decoding graph.
  AddDecodeNode(DecodeNode("unallocated", this));

  // Compile the graph from the root.
  compiled_decoder_root_ = GetDecodeNode("Root")->Compile(this);
}

void Decoder::AppendVisitor(DecoderVisitor* new_visitor) {
  visitors_.push_back(new_visitor);
}


void Decoder::PrependVisitor(DecoderVisitor* new_visitor) {
  visitors_.push_front(new_visitor);
}


void Decoder::InsertVisitorBefore(DecoderVisitor* new_visitor,
                                  DecoderVisitor* registered_visitor) {
  std::list<DecoderVisitor*>::iterator it;
  for (it = visitors_.begin(); it != visitors_.end(); it++) {
    if (*it == registered_visitor) {
      visitors_.insert(it, new_visitor);
      return;
    }
  }
  // We reached the end of the list. The last element must be
  // registered_visitor.
  VIXL_ASSERT(*it == registered_visitor);
  visitors_.insert(it, new_visitor);
}


void Decoder::InsertVisitorAfter(DecoderVisitor* new_visitor,
                                 DecoderVisitor* registered_visitor) {
  std::list<DecoderVisitor*>::iterator it;
  for (it = visitors_.begin(); it != visitors_.end(); it++) {
    if (*it == registered_visitor) {
      it++;
      visitors_.insert(it, new_visitor);
      return;
    }
  }
  // We reached the end of the list. The last element must be
  // registered_visitor.
  VIXL_ASSERT(*it == registered_visitor);
  visitors_.push_back(new_visitor);
}


void Decoder::RemoveVisitor(DecoderVisitor* visitor) {
  visitors_.remove(visitor);
}

void Decoder::VisitNamedInstruction(const Instruction* instr,
                                    const std::string& name) {
  std::list<DecoderVisitor*>::iterator it;
  Metadata m = {{"form", name}};
  uint32_t form_hash = Hash(name.c_str());

  // If an encoding is unallocated for this form, add the information to the
  // metadata.
  auto range = form_to_unalloc_.equal_range(form_hash);
  for (auto itu = range.first; itu != range.second; ++itu) {
    uint32_t mask = itu->second >> 32;
    uint32_t value = itu->second & 0xffffffff;
    if (instr->Mask(mask) == value) {
      m.insert({"unallocated", ""});
      break;
    }
  }

  for (it = visitors_.begin(); it != visitors_.end(); it++) {
    (*it)->Visit(&m, instr);
  }
}

void Decoder::PopulatePerInstructionUnallocatedMap(FormToUnallocMap* ftm) {
  using UnallocToFormMap =
      std::unordered_map<uint64_t, std::unordered_set<uint32_t>>;

  // Map from mask/value (as uint64) to instruction form. Given an encoding,
  // if, after applying the bitmask (top 32 bits), the resulting encoding equals
  // bottom 32 bits, then the encoding is unallocated for the instructions
  // indexed by the mask/value. On object construction, this is used to build a
  // map from instruction to mask/value, allowing fast lookup during
  // disassembly.
  static const UnallocToFormMap forms =
      {{0x00000001'00000001,
        {"casp_cp32_ldstexcl"_h,
         "caspa_cp32_ldstexcl"_h,
         "caspl_cp32_ldstexcl"_h,
         "caspal_cp32_ldstexcl"_h,
         "casp_cp64_ldstexcl"_h,
         "caspa_cp64_ldstexcl"_h,
         "caspl_cp64_ldstexcl"_h,
         "caspal_cp64_ldstexcl"_h}},
       {0x0000001f'0000001f,
        {"cpyen_cpy_memcms"_h,   "cpyern_cpy_memcms"_h,  "cpyewn_cpy_memcms"_h,
         "cpye_cpy_memcms"_h,    "cpyfen_cpy_memcms"_h,  "cpyfern_cpy_memcms"_h,
         "cpyfewn_cpy_memcms"_h, "cpyfe_cpy_memcms"_h,   "cpyfmn_cpy_memcms"_h,
         "cpyfmrn_cpy_memcms"_h, "cpyfmwn_cpy_memcms"_h, "cpyfm_cpy_memcms"_h,
         "cpyfpn_cpy_memcms"_h,  "cpyfprn_cpy_memcms"_h, "cpyfpwn_cpy_memcms"_h,
         "cpyfp_cpy_memcms"_h,   "cpymn_cpy_memcms"_h,   "cpymrn_cpy_memcms"_h,
         "cpymwn_cpy_memcms"_h,  "cpym_cpy_memcms"_h,    "cpypn_cpy_memcms"_h,
         "cpyprn_cpy_memcms"_h,  "cpypwn_cpy_memcms"_h,  "cpyp_cpy_memcms"_h,
         "seten_set_memcms"_h,   "sete_set_memcms"_h,    "setgen_set_memcms"_h,
         "setge_set_memcms"_h,   "setgmn_set_memcms"_h,  "setgm_set_memcms"_h,
         "setgpn_set_memcms"_h,  "setgp_set_memcms"_h,   "setmn_set_memcms"_h,
         "setm_set_memcms"_h,    "setpn_set_memcms"_h,   "setp_set_memcms"_h}},
       {0x000003e0'000003e0,
        {"cpyen_cpy_memcms"_h,   "cpyern_cpy_memcms"_h,  "cpyewn_cpy_memcms"_h,
         "cpye_cpy_memcms"_h,    "cpyfen_cpy_memcms"_h,  "cpyfern_cpy_memcms"_h,
         "cpyfewn_cpy_memcms"_h, "cpyfe_cpy_memcms"_h,   "cpyfmn_cpy_memcms"_h,
         "cpyfmrn_cpy_memcms"_h, "cpyfmwn_cpy_memcms"_h, "cpyfm_cpy_memcms"_h,
         "cpyfpn_cpy_memcms"_h,  "cpyfprn_cpy_memcms"_h, "cpyfpwn_cpy_memcms"_h,
         "cpyfp_cpy_memcms"_h,   "cpymn_cpy_memcms"_h,   "cpymrn_cpy_memcms"_h,
         "cpymwn_cpy_memcms"_h,  "cpym_cpy_memcms"_h,    "cpypn_cpy_memcms"_h,
         "cpyprn_cpy_memcms"_h,  "cpypwn_cpy_memcms"_h,  "cpyp_cpy_memcms"_h,
         "seten_set_memcms"_h,   "sete_set_memcms"_h,    "setgen_set_memcms"_h,
         "setge_set_memcms"_h,   "setgmn_set_memcms"_h,  "setgm_set_memcms"_h,
         "setgpn_set_memcms"_h,  "setgp_set_memcms"_h,   "setmn_set_memcms"_h,
         "setm_set_memcms"_h,    "setpn_set_memcms"_h,   "setp_set_memcms"_h}},
       {0x00001c00'00001400,
        {"add_32_addsub_ext"_h,
         "add_64_addsub_ext"_h,
         "subs_32s_addsub_ext"_h,
         "subs_64s_addsub_ext"_h,
         "sub_32_addsub_ext"_h,
         "sub_64_addsub_ext"_h}},
       {0x00001800'00001800,
        {"add_32_addsub_ext"_h,
         "add_64_addsub_ext"_h,
         "subs_32s_addsub_ext"_h,
         "subs_64s_addsub_ext"_h,
         "sub_32_addsub_ext"_h,
         "sub_64_addsub_ext"_h}},
       {0x00010000'00010000,
        {"casp_cp32_ldstexcl"_h,
         "caspa_cp32_ldstexcl"_h,
         "caspl_cp32_ldstexcl"_h,
         "caspal_cp32_ldstexcl"_h,
         "casp_cp64_ldstexcl"_h,
         "caspa_cp64_ldstexcl"_h,
         "caspl_cp64_ldstexcl"_h,
         "caspal_cp64_ldstexcl"_h}},
       {0x000207e0'000007c0, {"and_z_zi"_h, "eor_z_zi"_h, "orr_z_zi"_h}},
       {0x000207e0'000007e0, {"and_z_zi"_h, "eor_z_zi"_h, "orr_z_zi"_h}},
       {0x00030000'00000000, {"smov_asimdins_w_w"_h}},
       {0x00070000'00000000, {"smov_asimdins_x_x"_h, "umov_asimdins_w_w"_h}},
       {0x000f0000'00000000,
        {"umov_asimdins_w_w"_h,
         "umov_asimdins_x_x"_h,
         "dup_asimdins_dv_v"_h,
         "dup_asimdins_dr_r"_h,
         "ins_asimdins_iv_v"_h,
         "ins_asimdins_ir_r"_h}},
       {0x001f0000'00000000, {"dup_z_zi"_h}},
       {0x001f0000'001f0000,
        {"prfb_i_p_br_s"_h,      "prfd_i_p_br_s"_h,      "prfh_i_p_br_s"_h,
         "prfw_i_p_br_s"_h,      "cpyen_cpy_memcms"_h,   "cpyern_cpy_memcms"_h,
         "cpyewn_cpy_memcms"_h,  "cpye_cpy_memcms"_h,    "cpyfen_cpy_memcms"_h,
         "cpyfern_cpy_memcms"_h, "cpyfewn_cpy_memcms"_h, "cpyfe_cpy_memcms"_h,
         "cpyfmn_cpy_memcms"_h,  "cpyfmrn_cpy_memcms"_h, "cpyfmwn_cpy_memcms"_h,
         "cpyfm_cpy_memcms"_h,   "cpyfpn_cpy_memcms"_h,  "cpyfprn_cpy_memcms"_h,
         "cpyfpwn_cpy_memcms"_h, "cpyfp_cpy_memcms"_h,   "cpymn_cpy_memcms"_h,
         "cpymrn_cpy_memcms"_h,  "cpymwn_cpy_memcms"_h,  "cpym_cpy_memcms"_h,
         "cpypn_cpy_memcms"_h,   "cpyprn_cpy_memcms"_h,  "cpypwn_cpy_memcms"_h,
         "cpyp_cpy_memcms"_h}},
       {0x0040f800'0000f800,
        {"ands_32s_log_imm"_h,
         "ands_64s_log_imm"_h,
         "and_32_log_imm"_h,
         "and_64_log_imm"_h,
         "eor_32_log_imm"_h,
         "eor_64_log_imm"_h,
         "orr_32_log_imm"_h,
         "orr_64_log_imm"_h}},
       {0x0040fc00'00007c00,
        {"ands_32s_log_imm"_h,
         "ands_64s_log_imm"_h,
         "and_32_log_imm"_h,
         "and_64_log_imm"_h,
         "eor_32_log_imm"_h,
         "eor_64_log_imm"_h,
         "orr_32_log_imm"_h,
         "orr_64_log_imm"_h}},
       {0x0040fc00'0000bc00,
        {"ands_32s_log_imm"_h,
         "ands_64s_log_imm"_h,
         "and_32_log_imm"_h,
         "and_64_log_imm"_h,
         "eor_32_log_imm"_h,
         "eor_64_log_imm"_h,
         "orr_32_log_imm"_h,
         "orr_64_log_imm"_h}},
       {0x0040fc00'0000dc00,
        {"ands_32s_log_imm"_h,
         "ands_64s_log_imm"_h,
         "and_32_log_imm"_h,
         "and_64_log_imm"_h,
         "eor_32_log_imm"_h,
         "eor_64_log_imm"_h,
         "orr_32_log_imm"_h,
         "orr_64_log_imm"_h}},
       {0x0040fc00'0000ec00,
        {"ands_32s_log_imm"_h,
         "ands_64s_log_imm"_h,
         "and_32_log_imm"_h,
         "and_64_log_imm"_h,
         "eor_32_log_imm"_h,
         "eor_64_log_imm"_h,
         "orr_32_log_imm"_h,
         "orr_64_log_imm"_h}},
       {0x0040fc00'0000f400,
        {"ands_32s_log_imm"_h,
         "ands_64s_log_imm"_h,
         "and_32_log_imm"_h,
         "and_64_log_imm"_h,
         "eor_32_log_imm"_h,
         "eor_64_log_imm"_h,
         "orr_32_log_imm"_h,
         "orr_64_log_imm"_h}},
       {0x00200000'00200000, {"fcmla_asimdelem_c_s"_h}},
       {0x00400000'00000000,
        {"shl_asisdshf_r"_h,
         "sli_asisdshf_r"_h,
         "sri_asisdshf_r"_h,
         "srshr_asisdshf_r"_h,
         "srsra_asisdshf_r"_h,
         "sshr_asisdshf_r"_h,
         "ssra_asisdshf_r"_h,
         "urshr_asisdshf_r"_h,
         "ursra_asisdshf_r"_h,
         "ushr_asisdshf_r"_h,
         "usra_asisdshf_r"_h,
         "pmullb_z_zz"_h,
         "pmullt_z_zz"_h,
         "fcvtxn_asisdmisc_n"_h,
         "fcvtxn_asimdmisc_n"_h}},
       {0x00400000'00400000, {"urecpe_asimdmisc_r"_h, "ursqrte_asimdmisc_r"_h,
                              "cnt_asimdmisc_r"_h,    "rev16_asimdmisc_r"_h,
                              "shrn_asimdshf_n"_h,    "rshrn_asimdshf_n"_h,
                              "sqshrn_asimdshf_n"_h,  "sqrshrn_asimdshf_n"_h,
                              "sqshrun_asimdshf_n"_h, "sqrshrun_asimdshf_n"_h,
                              "uqshrn_asimdshf_n"_h,  "uqrshrn_asimdshf_n"_h,
                              "sshll_asimdshf_l"_h,   "ushll_asimdshf_l"_h,
                              "sqrshrn_asisdshf_n"_h, "sqrshrun_asisdshf_n"_h,
                              "sqshrn_asisdshf_n"_h,  "sqshrun_asisdshf_n"_h,
                              "uqrshrn_asisdshf_n"_h, "uqshrn_asisdshf_n"_h}},
       {0x00060000'00000000, {"flogb_z_p_z"_h}},
       {0x00580000'00000000,
        {"rshrnb_z_zi"_h,    "rshrnt_z_zi"_h,    "shrnb_z_zi"_h,
         "shrnt_z_zi"_h,     "sqrshrnb_z_zi"_h,  "sqrshrnt_z_zi"_h,
         "sqrshrunb_z_zi"_h, "sqrshrunt_z_zi"_h, "sqshrnb_z_zi"_h,
         "sqshrnt_z_zi"_h,   "sqshrunb_z_zi"_h,  "sqshrunt_z_zi"_h,
         "uqrshrnb_z_zi"_h,  "uqrshrnt_z_zi"_h,  "uqshrnb_z_zi"_h,
         "uqshrnt_z_zi"_h,   "sshllb_z_zi"_h,    "sshllt_z_zi"_h,
         "ushllb_z_zi"_h,    "ushllt_z_zi"_h,    "sqxtnb_z_zz"_h,
         "sqxtnt_z_zz"_h,    "sqxtunb_z_zz"_h,   "sqxtunt_z_zz"_h,
         "uqxtnb_z_zz"_h,    "uqxtnt_z_zz"_h}},
       {0x00580000'00180000,
        {"sqxtnb_z_zz"_h,
         "sqxtnt_z_zz"_h,
         "sqxtunb_z_zz"_h,
         "sqxtunt_z_zz"_h,
         "uqxtnb_z_zz"_h,
         "uqxtnt_z_zz"_h}},
       {0x00580000'00480000,
        {"sqxtnb_z_zz"_h,
         "sqxtnt_z_zz"_h,
         "sqxtunb_z_zz"_h,
         "sqxtunt_z_zz"_h,
         "uqxtnb_z_zz"_h,
         "uqxtnt_z_zz"_h}},
       {0x00580000'00500000,
        {"sqxtnb_z_zz"_h,
         "sqxtnt_z_zz"_h,
         "sqxtunb_z_zz"_h,
         "sqxtunt_z_zz"_h,
         "uqxtnb_z_zz"_h,
         "uqxtnt_z_zz"_h}},
       {0x00580000'00580000,
        {"sqxtnb_z_zz"_h,
         "sqxtnt_z_zz"_h,
         "sqxtunb_z_zz"_h,
         "sqxtunt_z_zz"_h,
         "uqxtnb_z_zz"_h,
         "uqxtnt_z_zz"_h}},
       {0x00600000'00600000,
        {"fmla_asimdelem_r_sd"_h,
         "fmls_asimdelem_r_sd"_h,
         "fmulx_asimdelem_r_sd"_h,
         "fmul_asimdelem_r_sd"_h}},
       {0x00700000'00000000,
        {"fcvtzs_asisdshf_c"_h,
         "fcvtzu_asisdshf_c"_h,
         "scvtf_asisdshf_c"_h,
         "ucvtf_asisdshf_c"_h}},
       {0x00780000'00000000,
        {"sqrshrn_asisdshf_n"_h,
         "sqrshrun_asisdshf_n"_h,
         "sqshrn_asisdshf_n"_h,
         "sqshrun_asisdshf_n"_h,
         "uqrshrn_asisdshf_n"_h,
         "uqshrn_asisdshf_n"_h}},
       {0x00780000'00080000,
        {"scvtf_asimdshf_c"_h,
         "ucvtf_asimdshf_c"_h,
         "fcvtzs_asimdshf_c"_h,
         "fcvtzu_asimdshf_c"_h}},
       {0x00800000'00000000,
        {"compact_z_p_z"_h, "sdot_z_zzz"_h, "udot_z_zzz"_h}},
       {0x00800000'00800000,
        {"cnt_asimdmisc_r"_h, "rev16_asimdmisc_r"_h, "rev32_asimdmisc_r"_h}},
       {0x00c00000'00000000,
        {"smlalb_z_zzz"_h,
         "smlalt_z_zzz"_h,
         "smlslb_z_zzz"_h,
         "smlslt_z_zzz"_h,
         "sqdmlalb_z_zzz"_h,
         "sqdmlalbt_z_zzz"_h,
         "sqdmlalt_z_zzz"_h,
         "sqdmlslb_z_zzz"_h,
         "sqdmlslbt_z_zzz"_h,
         "sqdmlslt_z_zzz"_h,
         "umlalb_z_zzz"_h,
         "umlalt_z_zzz"_h,
         "umlslb_z_zzz"_h,
         "umlslt_z_zzz"_h,
         "faddp_z_p_zz"_h,
         "fmaxnmp_z_p_zz"_h,
         "fmaxp_z_p_zz"_h,
         "fminnmp_z_p_zz"_h,
         "fminp_z_p_zz"_h,
         "urecpe_z_p_z"_h,
         "ursqrte_z_p_z"_h,
         "saddwb_z_zz"_h,
         "saddwt_z_zz"_h,
         "ssubwb_z_zz"_h,
         "ssubwt_z_zz"_h,
         "uaddwb_z_zz"_h,
         "uaddwt_z_zz"_h,
         "usubwb_z_zz"_h,
         "usubwt_z_zz"_h,
         "sadalp_z_p_z"_h,
         "uadalp_z_p_z"_h,
         "sabalb_z_zzz"_h,
         "sabalt_z_zzz"_h,
         "sabdlb_z_zz"_h,
         "sabdlt_z_zz"_h,
         "saddlb_z_zz"_h,
         "saddlbt_z_zz"_h,
         "saddlt_z_zz"_h,
         "smullb_z_zz"_h,
         "smullt_z_zz"_h,
         "sqdmullb_z_zz"_h,
         "sqdmullt_z_zz"_h,
         "ssublb_z_zz"_h,
         "ssublbt_z_zz"_h,
         "ssublt_z_zz"_h,
         "ssubltb_z_zz"_h,
         "uabalb_z_zzz"_h,
         "uabalt_z_zzz"_h,
         "uabdlb_z_zz"_h,
         "uabdlt_z_zz"_h,
         "uaddlb_z_zz"_h,
         "uaddlt_z_zz"_h,
         "umullb_z_zz"_h,
         "umullt_z_zz"_h,
         "usublb_z_zz"_h,
         "usublt_z_zz"_h,
         "addhnb_z_zz"_h,
         "addhnt_z_zz"_h,
         "raddhnb_z_zz"_h,
         "raddhnt_z_zz"_h,
         "rsubhnb_z_zz"_h,
         "rsubhnt_z_zz"_h,
         "subhnb_z_zz"_h,
         "subhnt_z_zz"_h,
         "cmeq_asisdsame_only"_h,
         "cmge_asisdsame_only"_h,
         "cmgt_asisdsame_only"_h,
         "cmhi_asisdsame_only"_h,
         "cmhs_asisdsame_only"_h,
         "cmtst_asisdsame_only"_h,
         "add_asisdsame_only"_h,
         "sub_asisdsame_only"_h,
         "addp_asisdpair_only"_h,
         "frinta_z_p_z"_h,
         "frinti_z_p_z"_h,
         "frintm_z_p_z"_h,
         "frintn_z_p_z"_h,
         "frintp_z_p_z"_h,
         "frintx_z_p_z"_h,
         "frintz_z_p_z"_h,
         "frecpx_z_p_z"_h,
         "fsqrt_z_p_z"_h,
         "frecpe_z_z"_h,
         "frsqrte_z_z"_h,
         "fmad_z_p_zzz"_h,
         "fmla_z_p_zzz"_h,
         "fmls_z_p_zzz"_h,
         "fmsb_z_p_zzz"_h,
         "fnmad_z_p_zzz"_h,
         "fnmla_z_p_zzz"_h,
         "fnmls_z_p_zzz"_h,
         "fnmsb_z_p_zzz"_h,
         "faddv_v_p_z"_h,
         "fmaxnmv_v_p_z"_h,
         "fmaxv_v_p_z"_h,
         "fminnmv_v_p_z"_h,
         "fminv_v_p_z"_h,
         "fcmla_z_p_zzz"_h,
         "fcadd_z_p_zz"_h,
         "fcmeq_p_p_z0"_h,
         "fcmge_p_p_z0"_h,
         "fcmgt_p_p_z0"_h,
         "fcmle_p_p_z0"_h,
         "fcmlt_p_p_z0"_h,
         "fcmne_p_p_z0"_h,
         "facge_p_p_zz"_h,
         "facgt_p_p_zz"_h,
         "fcmeq_p_p_zz"_h,
         "fcmge_p_p_zz"_h,
         "fcmgt_p_p_zz"_h,
         "fcmne_p_p_zz"_h,
         "fcmuo_p_p_zz"_h,
         "fadd_z_zz"_h,
         "fmul_z_zz"_h,
         "frecps_z_zz"_h,
         "frsqrts_z_zz"_h,
         "fsub_z_zz"_h,
         "ftsmul_z_zz"_h,
         "fadda_v_p_z"_h,
         "sxtw_z_p_z"_h,
         "uxtw_z_p_z"_h,
         "sxth_z_p_z"_h,
         "uxth_z_p_z"_h,
         "sxtb_z_p_z"_h,
         "uxtb_z_p_z"_h,
         "fabs_z_p_z"_h,
         "fneg_z_p_z"_h,
         "sunpkhi_z_z"_h,
         "sunpklo_z_z"_h,
         "uunpkhi_z_z"_h,
         "uunpklo_z_z"_h,
         "revb_z_z"_h,
         "revh_z_z"_h,
         "revw_z_z"_h,
         "ftssel_z_zz"_h,
         "ftmad_z_zzi"_h,
         "fexpa_z_z"_h,
         "fabd_z_p_zz"_h,
         "fadd_z_p_zz"_h,
         "fdivr_z_p_zz"_h,
         "fdiv_z_p_zz"_h,
         "fmaxnm_z_p_zz"_h,
         "fmax_z_p_zz"_h,
         "fminnm_z_p_zz"_h,
         "fmin_z_p_zz"_h,
         "fmulx_z_p_zz"_h,
         "fmul_z_p_zz"_h,
         "fscale_z_p_zz"_h,
         "fsubr_z_p_zz"_h,
         "fsub_z_p_zz"_h,
         "fadd_z_p_zs"_h,
         "fmaxnm_z_p_zs"_h,
         "fmax_z_p_zs"_h,
         "fminnm_z_p_zs"_h,
         "fmin_z_p_zs"_h,
         "fmul_z_p_zs"_h,
         "fsubr_z_p_zs"_h,
         "fsub_z_p_zs"_h,
         "abs_asisdmisc_r"_h,
         "neg_asisdmisc_r"_h,
         "cmeq_asisdmisc_z"_h,
         "cmge_asisdmisc_z"_h,
         "cmgt_asisdmisc_z"_h,
         "cmle_asisdmisc_z"_h,
         "cmlt_asisdmisc_z"_h,
         "cdot_z_zzz"_h,
         "histcnt_z_p_zz"_h,
         "sdiv_z_p_zz"_h,
         "sdivr_z_p_zz"_h,
         "udiv_z_p_zz"_h,
         "udivr_z_p_zz"_h,
         "fdup_z_i"_h,
         "fcpy_z_p_i"_h,
         "sqdmulh_asimdsame_only"_h,
         "sqrdmulh_asimdsame_only"_h,
         "fcmla_asimdsame2_c"_h,
         "fcadd_asimdsame2_c"_h,
         "sqrdmlah_asimdsame2_only"_h,
         "sqrdmlsh_asimdsame2_only"_h,
         "sdot_asimdsame2_d"_h,
         "udot_asimdsame2_d"_h,
         "mla_asimdelem_r"_h,
         "mls_asimdelem_r"_h,
         "mul_asimdelem_r"_h,
         "sqdmulh_asimdelem_r"_h,
         "sqrdmlah_asimdelem_r"_h,
         "sqrdmlsh_asimdelem_r"_h,
         "sqrdmulh_asimdelem_r"_h,
         "sqdmlal_asisddiff_only"_h,
         "sqdmlsl_asisddiff_only"_h,
         "sqdmull_asisddiff_only"_h,
         "sqdmulh_asisdsame_only"_h,
         "sqrdmulh_asisdsame_only"_h,
         "sqrdmlah_asisdsame2_only"_h,
         "sqrdmlsh_asisdsame2_only"_h,
         "srshl_asisdsame_only"_h,
         "urshl_asisdsame_only"_h,
         "sshl_asisdsame_only"_h,
         "ushl_asisdsame_only"_h,
         "sqdmulh_asisdelem_r"_h,
         "sqrdmlah_asisdelem_r"_h,
         "sqrdmlsh_asisdelem_r"_h,
         "sqrdmulh_asisdelem_r"_h,
         "sqdmlal_asisdelem_l"_h,
         "sqdmlsl_asisdelem_l"_h,
         "sqdmull_asisdelem_l"_h,
         "smlal_asimdelem_l"_h,
         "smlsl_asimdelem_l"_h,
         "smull_asimdelem_l"_h,
         "umlal_asimdelem_l"_h,
         "umlsl_asimdelem_l"_h,
         "umull_asimdelem_l"_h,
         "sqdmull_asimdelem_l"_h,
         "sqdmlal_asimdelem_l"_h,
         "sqdmlsl_asimdelem_l"_h,
         "sqdmlal_asimddiff_l"_h,
         "sqdmlsl_asimddiff_l"_h,
         "sqdmull_asimddiff_l"_h}},
       {0x00c00300'00000000,
        {"asr_z_p_zi"_h,
         "asrd_z_p_zi"_h,
         "lsl_z_p_zi"_h,
         "lsr_z_p_zi"_h,
         "sqshl_z_p_zi"_h,
         "sqshlu_z_p_zi"_h,
         "srshr_z_p_zi"_h,
         "uqshl_z_p_zi"_h,
         "urshr_z_p_zi"_h}},
       {0x00c00000'00400000,
        {"urecpe_z_p_z"_h,
         "ursqrte_z_p_z"_h,
         "histseg_z_zz"_h,
         "pmul_z_zz"_h,
         "cmeq_asisdsame_only"_h,
         "cmge_asisdsame_only"_h,
         "cmgt_asisdsame_only"_h,
         "cmhi_asisdsame_only"_h,
         "cmhs_asisdsame_only"_h,
         "cmtst_asisdsame_only"_h,
         "add_asisdsame_only"_h,
         "sub_asisdsame_only"_h,
         "addp_asisdpair_only"_h,
         "sxtw_z_p_z"_h,
         "uxtw_z_p_z"_h,
         "sxth_z_p_z"_h,
         "uxth_z_p_z"_h,
         "revh_z_z"_h,
         "revw_z_z"_h,
         "pmul_asimdsame_only"_h,
         "abs_asisdmisc_r"_h,
         "neg_asisdmisc_r"_h,
         "cmeq_asisdmisc_z"_h,
         "cmge_asisdmisc_z"_h,
         "cmgt_asisdmisc_z"_h,
         "cmle_asisdmisc_z"_h,
         "cmlt_asisdmisc_z"_h,
         "cdot_z_zzz"_h,
         "histcnt_z_p_zz"_h,
         "pmull_asimddiff_l"_h,
         "sdot_asimdsame2_d"_h,
         "udot_asimdsame2_d"_h,
         "srshl_asisdsame_only"_h,
         "urshl_asisdsame_only"_h,
         "sshl_asisdsame_only"_h,
         "ushl_asisdsame_only"_h}},
       {0x00c00000'00800000,
        {"histseg_z_zz"_h,         "pmul_z_zz"_h,
         "cmeq_asisdsame_only"_h,  "cmge_asisdsame_only"_h,
         "cmgt_asisdsame_only"_h,  "cmhi_asisdsame_only"_h,
         "cmhs_asisdsame_only"_h,  "cmtst_asisdsame_only"_h,
         "add_asisdsame_only"_h,   "sub_asisdsame_only"_h,
         "addp_asisdpair_only"_h,  "sxtw_z_p_z"_h,
         "uxtw_z_p_z"_h,           "revw_z_z"_h,
         "pmul_asimdsame_only"_h,  "abs_asisdmisc_r"_h,
         "neg_asisdmisc_r"_h,      "cmeq_asisdmisc_z"_h,
         "cmge_asisdmisc_z"_h,     "cmgt_asisdmisc_z"_h,
         "cmle_asisdmisc_z"_h,     "cmlt_asisdmisc_z"_h,
         "match_p_p_zz"_h,         "nmatch_p_p_zz"_h,
         "pmull_asimddiff_l"_h,    "srshl_asisdsame_only"_h,
         "urshl_asisdsame_only"_h, "sshl_asisdsame_only"_h,
         "ushl_asisdsame_only"_h}},
       {0x00c00000'00c00000,
        {"asr_z_p_zw"_h,
         "lsl_z_p_zw"_h,
         "lsr_z_p_zw"_h,
         "urecpe_z_p_z"_h,
         "ursqrte_z_p_z"_h,
         "histseg_z_zz"_h,
         "pmul_z_zz"_h,
         "asr_z_zw"_h,
         "lsl_z_zw"_h,
         "lsr_z_zw"_h,
         "pmul_asimdsame_only"_h,
         "match_p_p_zz"_h,
         "nmatch_p_p_zz"_h,
         "adds_32_addsub_shift"_h,
         "adds_64_addsub_shift"_h,
         "add_32_addsub_shift"_h,
         "add_64_addsub_shift"_h,
         "subs_32_addsub_shift"_h,
         "subs_64_addsub_shift"_h,
         "sub_32_addsub_shift"_h,
         "sub_64_addsub_shift"_h,
         "mla_asimdsame_only"_h,
         "mls_asimdsame_only"_h,
         "mul_asimdsame_only"_h,
         "saba_asimdsame_only"_h,
         "sabd_asimdsame_only"_h,
         "shadd_asimdsame_only"_h,
         "shsub_asimdsame_only"_h,
         "smaxp_asimdsame_only"_h,
         "smax_asimdsame_only"_h,
         "sminp_asimdsame_only"_h,
         "smin_asimdsame_only"_h,
         "srhadd_asimdsame_only"_h,
         "uaba_asimdsame_only"_h,
         "uabd_asimdsame_only"_h,
         "uhadd_asimdsame_only"_h,
         "uhsub_asimdsame_only"_h,
         "umaxp_asimdsame_only"_h,
         "umax_asimdsame_only"_h,
         "uminp_asimdsame_only"_h,
         "umin_asimdsame_only"_h,
         "urhadd_asimdsame_only"_h,
         "sqdmulh_asimdsame_only"_h,
         "sqrdmulh_asimdsame_only"_h,
         "sqrdmlah_asimdsame2_only"_h,
         "sqrdmlsh_asimdsame2_only"_h,
         "sdot_asimdsame2_d"_h,
         "udot_asimdsame2_d"_h,
         "clz_asimdmisc_r"_h,
         "cls_asimdmisc_r"_h,
         "rev64_asimdmisc_r"_h,
         "mla_asimdelem_r"_h,
         "mls_asimdelem_r"_h,
         "mul_asimdelem_r"_h,
         "sqdmulh_asimdelem_r"_h,
         "sqrdmlah_asimdelem_r"_h,
         "sqrdmlsh_asimdelem_r"_h,
         "sqrdmulh_asimdelem_r"_h,
         "sqxtn_asisdmisc_n"_h,
         "sqxtun_asisdmisc_n"_h,
         "uqxtn_asisdmisc_n"_h,
         "sqdmlal_asisddiff_only"_h,
         "sqdmlsl_asisddiff_only"_h,
         "sqdmull_asisddiff_only"_h,
         "sqdmulh_asisdsame_only"_h,
         "sqrdmulh_asisdsame_only"_h,
         "sqrdmlah_asisdsame2_only"_h,
         "sqrdmlsh_asisdsame2_only"_h,
         "sqdmulh_asisdelem_r"_h,
         "sqrdmlah_asisdelem_r"_h,
         "sqrdmlsh_asisdelem_r"_h,
         "sqrdmulh_asisdelem_r"_h,
         "sqdmlal_asisdelem_l"_h,
         "sqdmlsl_asisdelem_l"_h,
         "sqdmull_asisdelem_l"_h,
         "shll_asimdmisc_s"_h,
         "xtn_asimdmisc_n"_h,
         "sqxtn_asimdmisc_n"_h,
         "uqxtn_asimdmisc_n"_h,
         "sqxtun_asimdmisc_n"_h,
         "smlal_asimdelem_l"_h,
         "smlsl_asimdelem_l"_h,
         "smull_asimdelem_l"_h,
         "umlal_asimdelem_l"_h,
         "umlsl_asimdelem_l"_h,
         "umull_asimdelem_l"_h,
         "sqdmull_asimdelem_l"_h,
         "sqdmlal_asimdelem_l"_h,
         "sqdmlsl_asimdelem_l"_h,
         "saddlv_asimdall_only"_h,
         "uaddlv_asimdall_only"_h,
         "addv_asimdall_only"_h,
         "smaxv_asimdall_only"_h,
         "sminv_asimdall_only"_h,
         "umaxv_asimdall_only"_h,
         "uminv_asimdall_only"_h,
         "sabal_asimddiff_l"_h,
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
         "saddw_asimddiff_w"_h,
         "ssubw_asimddiff_w"_h,
         "uaddw_asimddiff_w"_h,
         "usubw_asimddiff_w"_h
         "addhn_asimddiff_n"_h,
         "raddhn_asimddiff_n"_h,
         "rsubhn_asimddiff_n"_h,
         "subhn_asimddiff_n"_h,
         "sqdmlal_asimddiff_l"_h,
         "sqdmlsl_asimddiff_l"_h,
         "sqdmull_asimddiff_l"_h}},
       {0x00c02000'00002000, {"dup_z_i"_h}},
       {0x00d80000'00000000,
        {"xar_z_zzi"_h,
         "asr_z_zi"_h,
         "lsr_z_zi"_h,
         "sri_z_zzi"_h,
         "srsra_z_zi"_h,
         "ssra_z_zi"_h,
         "ursra_z_zi"_h,
         "usra_z_zi"_h,
         "lsl_z_zi"_h,
         "sli_z_zzi"_h}},
       {0x40000000'00000000, {"fcmla_asimdelem_c_s"_h}},
       {0x40000800'00000800, {"fcmla_asimdelem_c_h"_h}},
       {0x40000c00'00000c00,
        {"ld2_asisdlse_r2"_h,
         "ld2_asisdlsep_i2_i"_h,
         "ld2_asisdlsep_r2_r"_h,
         "st2_asisdlse_r2"_h,
         "st2_asisdlsep_i2_i"_h,
         "st2_asisdlsep_r2_r"_h,
         "ld3_asisdlse_r3"_h,
         "ld3_asisdlsep_i3_i"_h,
         "ld3_asisdlsep_r3_r"_h,
         "st3_asisdlse_r3"_h,
         "st3_asisdlsep_i3_i"_h,
         "st3_asisdlsep_r3_r"_h,
         "ld4_asisdlse_r4"_h,
         "ld4_asisdlsep_i4_i"_h,
         "ld4_asisdlsep_r4_r"_h,
         "st4_asisdlse_r4"_h,
         "st4_asisdlsep_i4_i"_h,
         "st4_asisdlsep_r4_r"_h}},
       {0x40004000'00004000, {"ext_asimdext_only"_h}},
       {0x400f0000'00080000, {"dup_asimdins_dv_v"_h, "dup_asimdins_dr_d"_h}},
       {0x40400000'00000000,
        {"fmaxnmv_asimdall_only_sd"_h,
         "fminnmv_asimdall_only_sd"_h,
         "fmaxv_asimdall_only_sd"_h,
         "fminv_asimdall_only_sd"_h}},
       {0x40400000'00400000,
        {"fabs_asimdmisc_r"_h,         "fcvtas_asimdmisc_r"_h,
         "fcvtau_asimdmisc_r"_h,       "fcvtms_asimdmisc_r"_h,
         "fcvtmu_asimdmisc_r"_h,       "fcvtns_asimdmisc_r"_h,
         "fcvtnu_asimdmisc_r"_h,       "fcvtps_asimdmisc_r"_h,
         "fcvtpu_asimdmisc_r"_h,       "fcvtzs_asimdmisc_r"_h,
         "fcvtzu_asimdmisc_r"_h,       "fneg_asimdmisc_r"_h,
         "frecpe_asimdmisc_r"_h,       "frint32x_asimdmisc_r"_h,
         "frint32z_asimdmisc_r"_h,     "frint64x_asimdmisc_r"_h,
         "frint64z_asimdmisc_r"_h,     "frinta_asimdmisc_r"_h,
         "frinti_asimdmisc_r"_h,       "frintm_asimdmisc_r"_h,
         "frintn_asimdmisc_r"_h,       "frintp_asimdmisc_r"_h,
         "frintx_asimdmisc_r"_h,       "frintz_asimdmisc_r"_h,
         "frsqrte_asimdmisc_r"_h,      "fsqrt_asimdmisc_r"_h,
         "scvtf_asimdmisc_r"_h,        "ucvtf_asimdmisc_r"_h,
         "fmaxnmv_asimdall_only_sd"_h, "fminnmv_asimdall_only_sd"_h,
         "fmaxv_asimdall_only_sd"_h,   "fminv_asimdall_only_sd"_h,
         "fcmeq_asimdmisc_fz"_h,       "fcmge_asimdmisc_fz"_h,
         "fcmgt_asimdmisc_fz"_h,       "fcmle_asimdmisc_fz"_h,
         "fcmlt_asimdmisc_fz"_h,       "fabd_asimdsame_only"_h,
         "facge_asimdsame_only"_h,     "facgt_asimdsame_only"_h,
         "faddp_asimdsame_only"_h,     "fadd_asimdsame_only"_h,
         "fcmeq_asimdsame_only"_h,     "fcmge_asimdsame_only"_h,
         "fcmgt_asimdsame_only"_h,     "fdiv_asimdsame_only"_h,
         "fmaxnmp_asimdsame_only"_h,   "fmaxnm_asimdsame_only"_h,
         "fmaxp_asimdsame_only"_h,     "fmax_asimdsame_only"_h,
         "fminnmp_asimdsame_only"_h,   "fminnm_asimdsame_only"_h,
         "fminp_asimdsame_only"_h,     "fmin_asimdsame_only"_h,
         "fmla_asimdsame_only"_h,      "fmls_asimdsame_only"_h,
         "fmulx_asimdsame_only"_h,     "fmul_asimdsame_only"_h,
         "frecps_asimdsame_only"_h,    "frsqrts_asimdsame_only"_h,
         "fsub_asimdsame_only"_h,      "fmla_asimdelem_r_sd"_h,
         "fmls_asimdelem_r_sd"_h,      "fmulx_asimdelem_r_sd"_h,
         "fmul_asimdelem_r_sd"_h,      "sri_asimdshf_r"_h,
         "srshr_asimdshf_r"_h,         "srsra_asimdshf_r"_h,
         "sshr_asimdshf_r"_h,          "ssra_asimdshf_r"_h,
         "urshr_asimdshf_r"_h,         "ursra_asimdshf_r"_h,
         "ushr_asimdshf_r"_h,          "usra_asimdshf_r"_h,
         "scvtf_asimdshf_c"_h,         "ucvtf_asimdshf_c"_h,
         "fcvtzs_asimdshf_c"_h,        "fcvtzu_asimdshf_c"_h}},
       {0x40400000'40400000,
        {"fmaxnmv_asimdall_only_sd"_h,
         "fminnmv_asimdall_only_sd"_h,
         "fmaxv_asimdall_only_sd"_h,
         "fminv_asimdall_only_sd"_h}},
       {0x40c00000'00800000,
        {"saddlv_asimdall_only"_h,
         "uaddlv_asimdall_only"_h,
         "addv_asimdall_only"_h,
         "smaxv_asimdall_only"_h,
         "sminv_asimdall_only"_h,
         "umaxv_asimdall_only"_h,
         "uminv_asimdall_only"_h}},
       {0x40c00000'00c00000,
        {"cmeq_asimdmisc_z"_h,       "cmge_asimdmisc_z"_h,
         "cmgt_asimdmisc_z"_h,       "cmle_asimdmisc_z"_h,
         "cmlt_asimdmisc_z"_h,       "addp_asimdsame_only"_h,
         "add_asimdsame_only"_h,     "cmeq_asimdsame_only"_h,
         "cmge_asimdsame_only"_h,    "cmgt_asimdsame_only"_h,
         "cmhi_asimdsame_only"_h,    "cmhs_asimdsame_only"_h,
         "cmtst_asimdsame_only"_h,   "sqadd_asimdsame_only"_h,
         "sqdmulh_asimdsame_only"_h, "sqrdmulh_asimdsame_only"_h,
         "sqrshl_asimdsame_only"_h,  "sqshl_asimdsame_only"_h,
         "sqsub_asimdsame_only"_h,   "srshl_asimdsame_only"_h,
         "sshl_asimdsame_only"_h,    "sub_asimdsame_only"_h,
         "uqadd_asimdsame_only"_h,   "uqrshl_asimdsame_only"_h,
         "uqshl_asimdsame_only"_h,   "uqsub_asimdsame_only"_h,
         "urshl_asimdsame_only"_h,   "ushl_asimdsame_only"_h,
         "trn1_asimdperm_only"_h,    "trn2_asimdperm_only"_h,
         "uzp1_asimdperm_only"_h,    "uzp2_asimdperm_only"_h,
         "zip1_asimdperm_only"_h,    "zip2_asimdperm_only"_h,
         "fcmla_asimdsame2_c"_h,     "fcadd_asimdsame2_c"_h}},
       {0x80200000'00200000,
        {"sbfm_64m_bitfield"_h,
         "sbfm_32m_bitfield"_h,
         "ubfm_32m_bitfield"_h,
         "ubfm_64m_bitfield"_h,
         "bfm_32m_bitfield"_h,
         "bfm_64m_bitfield"_h}},
       {0x80008000'00000000,
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
       {0x80008000'00008000,
        {"sbfm_64m_bitfield"_h,
         "sbfm_32m_bitfield"_h,
         "ubfm_32m_bitfield"_h,
         "ubfm_64m_bitfield"_h,
         "bfm_32m_bitfield"_h,
         "bfm_64m_bitfield"_h}},
       {0xc0000000'40000000,
        {"cpyen_cpy_memcms"_h,   "cpyern_cpy_memcms"_h,  "cpyewn_cpy_memcms"_h,
         "cpye_cpy_memcms"_h,    "cpyfen_cpy_memcms"_h,  "cpyfern_cpy_memcms"_h,
         "cpyfewn_cpy_memcms"_h, "cpyfe_cpy_memcms"_h,   "cpyfmn_cpy_memcms"_h,
         "cpyfmrn_cpy_memcms"_h, "cpyfmwn_cpy_memcms"_h, "cpyfm_cpy_memcms"_h,
         "cpyfpn_cpy_memcms"_h,  "cpyfprn_cpy_memcms"_h, "cpyfpwn_cpy_memcms"_h,
         "cpyfp_cpy_memcms"_h,   "cpymn_cpy_memcms"_h,   "cpymrn_cpy_memcms"_h,
         "cpymwn_cpy_memcms"_h,  "cpym_cpy_memcms"_h,    "cpypn_cpy_memcms"_h,
         "cpyprn_cpy_memcms"_h,  "cpypwn_cpy_memcms"_h,  "cpyp_cpy_memcms"_h,
         "seten_set_memcms"_h,   "sete_set_memcms"_h,    "setgen_set_memcms"_h,
         "setge_set_memcms"_h,   "setgmn_set_memcms"_h,  "setgm_set_memcms"_h,
         "setgpn_set_memcms"_h,  "setgp_set_memcms"_h,   "setmn_set_memcms"_h,
         "setm_set_memcms"_h,    "setpn_set_memcms"_h,   "setp_set_memcms"_h}},
       {0xc0000000'80000000,
        {"cpyen_cpy_memcms"_h,   "cpyern_cpy_memcms"_h,  "cpyewn_cpy_memcms"_h,
         "cpye_cpy_memcms"_h,    "cpyfen_cpy_memcms"_h,  "cpyfern_cpy_memcms"_h,
         "cpyfewn_cpy_memcms"_h, "cpyfe_cpy_memcms"_h,   "cpyfmn_cpy_memcms"_h,
         "cpyfmrn_cpy_memcms"_h, "cpyfmwn_cpy_memcms"_h, "cpyfm_cpy_memcms"_h,
         "cpyfpn_cpy_memcms"_h,  "cpyfprn_cpy_memcms"_h, "cpyfpwn_cpy_memcms"_h,
         "cpyfp_cpy_memcms"_h,   "cpymn_cpy_memcms"_h,   "cpymrn_cpy_memcms"_h,
         "cpymwn_cpy_memcms"_h,  "cpym_cpy_memcms"_h,    "cpypn_cpy_memcms"_h,
         "cpyprn_cpy_memcms"_h,  "cpypwn_cpy_memcms"_h,  "cpyp_cpy_memcms"_h,
         "seten_set_memcms"_h,   "sete_set_memcms"_h,    "setgen_set_memcms"_h,
         "setge_set_memcms"_h,   "setgmn_set_memcms"_h,  "setgm_set_memcms"_h,
         "setgpn_set_memcms"_h,  "setgp_set_memcms"_h,   "setmn_set_memcms"_h,
         "setm_set_memcms"_h,    "setpn_set_memcms"_h,   "setp_set_memcms"_h}},
       {0xc0000000'c0000000,
        {"cpyen_cpy_memcms"_h,   "cpyern_cpy_memcms"_h,  "cpyewn_cpy_memcms"_h,
         "cpye_cpy_memcms"_h,    "cpyfen_cpy_memcms"_h,  "cpyfern_cpy_memcms"_h,
         "cpyfewn_cpy_memcms"_h, "cpyfe_cpy_memcms"_h,   "cpyfmn_cpy_memcms"_h,
         "cpyfmrn_cpy_memcms"_h, "cpyfmwn_cpy_memcms"_h, "cpyfm_cpy_memcms"_h,
         "cpyfpn_cpy_memcms"_h,  "cpyfprn_cpy_memcms"_h, "cpyfpwn_cpy_memcms"_h,
         "cpyfp_cpy_memcms"_h,   "cpymn_cpy_memcms"_h,   "cpymrn_cpy_memcms"_h,
         "cpymwn_cpy_memcms"_h,  "cpym_cpy_memcms"_h,    "cpypn_cpy_memcms"_h,
         "cpyprn_cpy_memcms"_h,  "cpypwn_cpy_memcms"_h,  "cpyp_cpy_memcms"_h,
         "seten_set_memcms"_h,   "sete_set_memcms"_h,    "setgen_set_memcms"_h,
         "setge_set_memcms"_h,   "setgmn_set_memcms"_h,  "setgm_set_memcms"_h,
         "setgpn_set_memcms"_h,  "setgp_set_memcms"_h,   "setmn_set_memcms"_h,
         "setm_set_memcms"_h,    "setpn_set_memcms"_h,   "setp_set_memcms"_h}}};

  for (auto& itm : forms) {
    const std::unordered_set<uint32_t>& s = forms.at(itm.first);
    for (const uint32_t& its : s) {
      ftm->insert(std::make_pair(its, itm.first));
    }
  }
}

// Initialise empty vectors for sampled bits and pattern table.
const std::vector<uint8_t> DecodeNode::kEmptySampledBits;
const std::vector<DecodePattern> DecodeNode::kEmptyPatternTable;

void DecodeNode::CompileNodeForBits(Decoder* decoder,
                                    std::string name,
                                    uint32_t bits) {
  DecodeNode* n = decoder->GetDecodeNode(name);
  VIXL_ASSERT(n != NULL);
  if (!n->IsCompiled()) {
    n->Compile(decoder);
  }
  VIXL_ASSERT(n->IsCompiled());
  compiled_node_->SetNodeForBits(bits, n->GetCompiledNode());
}


#define INSTANTIATE_TEMPLATE_M(M)                      \
  case 0x##M:                                          \
    bit_extract_fn = &Instruction::ExtractBits<0x##M>; \
    break;
#define INSTANTIATE_TEMPLATE_MV(M, V)                           \
  case 0x##M##V:                                                \
    bit_extract_fn = &Instruction::IsMaskedValue<0x##M, 0x##V>; \
    break;

BitExtractFn DecodeNode::GetBitExtractFunctionHelper(uint32_t x, uint32_t y) {
  // Instantiate a templated bit extraction function for every pattern we
  // might encounter. If the assertion in the default clause is reached, add a
  // new instantiation below using the information in the failure message.
  BitExtractFn bit_extract_fn = NULL;

  // The arguments x and y represent the mask and value. If y is 0, x is the
  // mask. Otherwise, y is the mask, and x is the value to compare against a
  // masked result.
  uint64_t signature = (static_cast<uint64_t>(y) << 32) | x;
  switch (signature) {
    INSTANTIATE_TEMPLATE_M(00000002);
    INSTANTIATE_TEMPLATE_M(00000010);
    INSTANTIATE_TEMPLATE_M(00000060);
    INSTANTIATE_TEMPLATE_M(000000df);
    INSTANTIATE_TEMPLATE_M(00000100);
    INSTANTIATE_TEMPLATE_M(00000200);
    INSTANTIATE_TEMPLATE_M(00000400);
    INSTANTIATE_TEMPLATE_M(00000800);
    INSTANTIATE_TEMPLATE_M(00000c00);
    INSTANTIATE_TEMPLATE_M(00000c10);
    INSTANTIATE_TEMPLATE_M(00000f00);
    INSTANTIATE_TEMPLATE_M(00000fc0);
    INSTANTIATE_TEMPLATE_M(00001000);
    INSTANTIATE_TEMPLATE_M(00001400);
    INSTANTIATE_TEMPLATE_M(00001800);
    INSTANTIATE_TEMPLATE_M(00001c00);
    INSTANTIATE_TEMPLATE_M(00002000);
    INSTANTIATE_TEMPLATE_M(00002010);
    INSTANTIATE_TEMPLATE_M(00002400);
    INSTANTIATE_TEMPLATE_M(00003000);
    INSTANTIATE_TEMPLATE_M(00003020);
    INSTANTIATE_TEMPLATE_M(00003400);
    INSTANTIATE_TEMPLATE_M(00003800);
    INSTANTIATE_TEMPLATE_M(00003c00);
    INSTANTIATE_TEMPLATE_M(00013000);
    INSTANTIATE_TEMPLATE_M(000203e0);
    INSTANTIATE_TEMPLATE_M(000303e0);
    INSTANTIATE_TEMPLATE_M(00040000);
    INSTANTIATE_TEMPLATE_M(00040010);
    INSTANTIATE_TEMPLATE_M(00060000);
    INSTANTIATE_TEMPLATE_M(00061000);
    INSTANTIATE_TEMPLATE_M(00070000);
    INSTANTIATE_TEMPLATE_M(000703c0);
    INSTANTIATE_TEMPLATE_M(00080000);
    INSTANTIATE_TEMPLATE_M(00090000);
    INSTANTIATE_TEMPLATE_M(000f0000);
    INSTANTIATE_TEMPLATE_M(000f0010);
    INSTANTIATE_TEMPLATE_M(00100000);
    INSTANTIATE_TEMPLATE_M(00180000);
    INSTANTIATE_TEMPLATE_M(001b1c00);
    INSTANTIATE_TEMPLATE_M(001f0000);
    INSTANTIATE_TEMPLATE_M(001f0018);
    INSTANTIATE_TEMPLATE_M(001f2000);
    INSTANTIATE_TEMPLATE_M(001f3000);
    INSTANTIATE_TEMPLATE_M(00400000);
    INSTANTIATE_TEMPLATE_M(00400018);
    INSTANTIATE_TEMPLATE_M(00400800);
    INSTANTIATE_TEMPLATE_M(00403000);
    INSTANTIATE_TEMPLATE_M(00500000);
    INSTANTIATE_TEMPLATE_M(00500800);
    INSTANTIATE_TEMPLATE_M(00583000);
    INSTANTIATE_TEMPLATE_M(005f0000);
    INSTANTIATE_TEMPLATE_M(00800000);
    INSTANTIATE_TEMPLATE_M(00800400);
    INSTANTIATE_TEMPLATE_M(00800c1d);
    INSTANTIATE_TEMPLATE_M(0080101f);
    INSTANTIATE_TEMPLATE_M(00801c00);
    INSTANTIATE_TEMPLATE_M(00803000);
    INSTANTIATE_TEMPLATE_M(00803c00);
    INSTANTIATE_TEMPLATE_M(009f0000);
    INSTANTIATE_TEMPLATE_M(009f2000);
    INSTANTIATE_TEMPLATE_M(00c00000);
    INSTANTIATE_TEMPLATE_M(00c00010);
    INSTANTIATE_TEMPLATE_M(00c0001f);
    INSTANTIATE_TEMPLATE_M(00c00200);
    INSTANTIATE_TEMPLATE_M(00c00400);
    INSTANTIATE_TEMPLATE_M(00c00c00);
    INSTANTIATE_TEMPLATE_M(00c00c19);
    INSTANTIATE_TEMPLATE_M(00c01000);
    INSTANTIATE_TEMPLATE_M(00c01400);
    INSTANTIATE_TEMPLATE_M(00c01c00);
    INSTANTIATE_TEMPLATE_M(00c02000);
    INSTANTIATE_TEMPLATE_M(00c03000);
    INSTANTIATE_TEMPLATE_M(00c03c00);
    INSTANTIATE_TEMPLATE_M(00c70000);
    INSTANTIATE_TEMPLATE_M(00c83000);
    INSTANTIATE_TEMPLATE_M(00d00200);
    INSTANTIATE_TEMPLATE_M(00d80800);
    INSTANTIATE_TEMPLATE_M(00d81800);
    INSTANTIATE_TEMPLATE_M(00d81c00);
    INSTANTIATE_TEMPLATE_M(00d82800);
    INSTANTIATE_TEMPLATE_M(00d82c00);
    INSTANTIATE_TEMPLATE_M(00d92400);
    INSTANTIATE_TEMPLATE_M(00d93000);
    INSTANTIATE_TEMPLATE_M(00db0000);
    INSTANTIATE_TEMPLATE_M(00db2000);
    INSTANTIATE_TEMPLATE_M(00dc0000);
    INSTANTIATE_TEMPLATE_M(00dc2000);
    INSTANTIATE_TEMPLATE_M(00df0000);
    INSTANTIATE_TEMPLATE_M(40000000);
    INSTANTIATE_TEMPLATE_M(40000010);
    INSTANTIATE_TEMPLATE_M(40000c00);
    INSTANTIATE_TEMPLATE_M(40002000);
    INSTANTIATE_TEMPLATE_M(40002010);
    INSTANTIATE_TEMPLATE_M(40003000);
    INSTANTIATE_TEMPLATE_M(40003c00);
    INSTANTIATE_TEMPLATE_M(401f2000);
    INSTANTIATE_TEMPLATE_M(40400800);
    INSTANTIATE_TEMPLATE_M(40400c00);
    INSTANTIATE_TEMPLATE_M(40403c00);
    INSTANTIATE_TEMPLATE_M(405f0000);
    INSTANTIATE_TEMPLATE_M(40800000);
    INSTANTIATE_TEMPLATE_M(40800c00);
    INSTANTIATE_TEMPLATE_M(40802000);
    INSTANTIATE_TEMPLATE_M(40802010);
    INSTANTIATE_TEMPLATE_M(40803400);
    INSTANTIATE_TEMPLATE_M(40803c00);
    INSTANTIATE_TEMPLATE_M(40c00000);
    INSTANTIATE_TEMPLATE_M(40c00400);
    INSTANTIATE_TEMPLATE_M(40c00800);
    INSTANTIATE_TEMPLATE_M(40c00c00);
    INSTANTIATE_TEMPLATE_M(40c00c10);
    INSTANTIATE_TEMPLATE_M(40c02000);
    INSTANTIATE_TEMPLATE_M(40c02010);
    INSTANTIATE_TEMPLATE_M(40c02c00);
    INSTANTIATE_TEMPLATE_M(40c03c00);
    INSTANTIATE_TEMPLATE_M(40c80000);
    INSTANTIATE_TEMPLATE_M(40c90000);
    INSTANTIATE_TEMPLATE_M(40cf0000);
    INSTANTIATE_TEMPLATE_M(40d02000);
    INSTANTIATE_TEMPLATE_M(40d02010);
    INSTANTIATE_TEMPLATE_M(40d80000);
    INSTANTIATE_TEMPLATE_M(40d81800);
    INSTANTIATE_TEMPLATE_M(40dc0000);
    INSTANTIATE_TEMPLATE_M(bf20c000);
    INSTANTIATE_TEMPLATE_MV(00000006, 00000000);
    INSTANTIATE_TEMPLATE_MV(00000006, 00000006);
    INSTANTIATE_TEMPLATE_MV(00000007, 00000000);
    INSTANTIATE_TEMPLATE_MV(0000001f, 0000001f);
    INSTANTIATE_TEMPLATE_MV(00000210, 00000000);
    INSTANTIATE_TEMPLATE_MV(000003e0, 00000000);
    INSTANTIATE_TEMPLATE_MV(000003e0, 000003e0);
    INSTANTIATE_TEMPLATE_MV(000003e2, 000003e0);
    INSTANTIATE_TEMPLATE_MV(000003e6, 000003e0);
    INSTANTIATE_TEMPLATE_MV(000003e6, 000003e6);
    INSTANTIATE_TEMPLATE_MV(00000c00, 00000000);
    INSTANTIATE_TEMPLATE_MV(00000fc0, 00000000);
    INSTANTIATE_TEMPLATE_MV(000013e0, 00001000);
    INSTANTIATE_TEMPLATE_MV(00001c00, 00000000);
    INSTANTIATE_TEMPLATE_MV(00002400, 00000000);
    INSTANTIATE_TEMPLATE_MV(00003000, 00000000);
    INSTANTIATE_TEMPLATE_MV(00003000, 00001000);
    INSTANTIATE_TEMPLATE_MV(00003000, 00002000);
    INSTANTIATE_TEMPLATE_MV(00003000, 00003000);
    INSTANTIATE_TEMPLATE_MV(00003010, 00000000);
    INSTANTIATE_TEMPLATE_MV(0000301f, 0000001f);
    INSTANTIATE_TEMPLATE_MV(00003c00, 00003c00);
    INSTANTIATE_TEMPLATE_MV(00040010, 00000000);
    INSTANTIATE_TEMPLATE_MV(00060000, 00000000);
    INSTANTIATE_TEMPLATE_MV(00061000, 00000000);
    INSTANTIATE_TEMPLATE_MV(00070000, 00030000);
    INSTANTIATE_TEMPLATE_MV(0007309f, 0000001f);
    INSTANTIATE_TEMPLATE_MV(00073ee0, 00033060);
    INSTANTIATE_TEMPLATE_MV(00073f9f, 0000001f);
    INSTANTIATE_TEMPLATE_MV(000f0000, 00000000);
    INSTANTIATE_TEMPLATE_MV(000f0010, 00000000);
    INSTANTIATE_TEMPLATE_MV(00100200, 00000000);
    INSTANTIATE_TEMPLATE_MV(00100210, 00000000);
    INSTANTIATE_TEMPLATE_MV(00160000, 00000000);
    INSTANTIATE_TEMPLATE_MV(00170000, 00000000);
    INSTANTIATE_TEMPLATE_MV(001c0000, 00000000);
    INSTANTIATE_TEMPLATE_MV(001d0000, 00000000);
    INSTANTIATE_TEMPLATE_MV(001e0000, 00000000);
    INSTANTIATE_TEMPLATE_MV(001f0000, 00000000);
    INSTANTIATE_TEMPLATE_MV(001f0000, 00010000);
    INSTANTIATE_TEMPLATE_MV(001f0000, 00100000);
    INSTANTIATE_TEMPLATE_MV(001f0000, 001f0000);
    INSTANTIATE_TEMPLATE_MV(001f3000, 00000000);
    INSTANTIATE_TEMPLATE_MV(001f3000, 00001000);
    INSTANTIATE_TEMPLATE_MV(001f3000, 001f0000);
    INSTANTIATE_TEMPLATE_MV(001f300f, 0000000d);
    INSTANTIATE_TEMPLATE_MV(001f301f, 0000000d);
    INSTANTIATE_TEMPLATE_MV(001f33e0, 000103e0);
    INSTANTIATE_TEMPLATE_MV(001f3800, 00000000);
    INSTANTIATE_TEMPLATE_MV(00401000, 00400000);
    INSTANTIATE_TEMPLATE_MV(005f3000, 001f0000);
    INSTANTIATE_TEMPLATE_MV(005f3000, 001f1000);
    INSTANTIATE_TEMPLATE_MV(00800010, 00000000);
    INSTANTIATE_TEMPLATE_MV(00800400, 00000000);
    INSTANTIATE_TEMPLATE_MV(00800410, 00000000);
    INSTANTIATE_TEMPLATE_MV(00803000, 00002000);
    INSTANTIATE_TEMPLATE_MV(00870000, 00000000);
    INSTANTIATE_TEMPLATE_MV(009f0000, 00010000);
    INSTANTIATE_TEMPLATE_MV(00c00000, 00000000);
    INSTANTIATE_TEMPLATE_MV(00c00000, 00400000);
    INSTANTIATE_TEMPLATE_MV(00c0001f, 00000000);
    INSTANTIATE_TEMPLATE_MV(00c001ff, 00000000);
    INSTANTIATE_TEMPLATE_MV(00c00200, 00400000);
    INSTANTIATE_TEMPLATE_MV(00c0020f, 00400000);
    INSTANTIATE_TEMPLATE_MV(00c003e0, 00000000);
    INSTANTIATE_TEMPLATE_MV(00c00800, 00000000);
    INSTANTIATE_TEMPLATE_MV(00d80800, 00000000);
    INSTANTIATE_TEMPLATE_MV(00df0000, 00000000);
    INSTANTIATE_TEMPLATE_MV(00df3800, 001f0800);
    INSTANTIATE_TEMPLATE_MV(40002000, 40000000);
    INSTANTIATE_TEMPLATE_MV(40003c00, 00000000);
    INSTANTIATE_TEMPLATE_MV(40040000, 00000000);
    INSTANTIATE_TEMPLATE_MV(401f2000, 401f0000);
    INSTANTIATE_TEMPLATE_MV(40800c00, 40000400);
    INSTANTIATE_TEMPLATE_MV(40c00000, 00000000);
    INSTANTIATE_TEMPLATE_MV(40c00000, 00400000);
    INSTANTIATE_TEMPLATE_MV(40c00000, 40000000);
    INSTANTIATE_TEMPLATE_MV(40c00000, 40800000);
    INSTANTIATE_TEMPLATE_MV(40df0000, 00000000);
    default: {
      static bool printed_preamble = false;
      if (!printed_preamble) {
        printf("One or more missing template instantiations.\n");
        printf(
            "Add the following to either GetBitExtractFunction() "
            "implementations\n");
        printf("in %s near line %d:\n", __FILE__, __LINE__);
        printed_preamble = true;
      }

      if (y == 0) {
        printf("  INSTANTIATE_TEMPLATE_M(%08x);\n", x);
        bit_extract_fn = &Instruction::ExtractBitsAbsent;
      } else {
        printf("  INSTANTIATE_TEMPLATE_MV(%08x, %08x);\n", y, x);
        bit_extract_fn = &Instruction::IsMaskedValueAbsent;
      }
    }
  }
  return bit_extract_fn;
}

#undef INSTANTIATE_TEMPLATE_M
#undef INSTANTIATE_TEMPLATE_MV

bool DecodeNode::TryCompileOptimisedDecodeTable(Decoder* decoder) {
  // EitherOr optimisation: if there are only one or two patterns in the table,
  // try to optimise the node to exploit that.
  size_t table_size = pattern_table_.size();
  if ((table_size <= 2) && (GetSampledBitsCount() > 1)) {
    // TODO: support 'x' in this optimisation by dropping the sampled bit
    // positions before making the mask/value.
    if (!PatternContainsSymbol(pattern_table_[0].pattern,
                               PatternSymbol::kSymbolX) &&
        (table_size == 1)) {
      // A pattern table consisting of a fixed pattern with no x's, and an
      // "otherwise" or absent case. Optimise this into an instruction mask and
      // value test.
      uint32_t single_decode_mask = 0;
      uint32_t single_decode_value = 0;
      const std::vector<uint8_t>& bits = GetSampledBits();

      // Construct the instruction mask and value from the pattern.
      VIXL_ASSERT(bits.size() == GetPatternLength(pattern_table_[0].pattern));
      for (size_t i = 0; i < bits.size(); i++) {
        single_decode_mask |= 1U << bits[i];
        if (GetSymbolAt(pattern_table_[0].pattern, i) ==
            PatternSymbol::kSymbol1) {
          single_decode_value |= 1U << bits[i];
        }
      }
      BitExtractFn bit_extract_fn =
          GetBitExtractFunction(single_decode_mask, single_decode_value);

      // Create a compiled node that contains a two entry table for the
      // either/or cases.
      CreateCompiledNode(bit_extract_fn, 2);

      // Set DecodeNode for when the instruction after masking doesn't match the
      // value.
      CompileNodeForBits(decoder, "unallocated", 0);

      // Set DecodeNode for when it does match.
      CompileNodeForBits(decoder, pattern_table_[0].handler, 1);

      return true;
    }
  }
  return false;
}

CompiledDecodeNode* DecodeNode::Compile(Decoder* decoder) {
  if (IsLeafNode()) {
    // A leaf node is a simple wrapper around a visitor function, with no
    // instruction decoding to do.
    CreateVisitorNode();
  } else if (!TryCompileOptimisedDecodeTable(decoder)) {
    // The "otherwise" node is the default next node if no pattern matches.
    std::string otherwise = "unallocated";

    // For each pattern in pattern_table_, create an entry in matches that
    // has a corresponding mask and value for the pattern.
    std::vector<MaskValuePair> matches;
    for (size_t i = 0; i < pattern_table_.size(); i++) {
      matches.push_back(GenerateMaskValuePair(
          GenerateOrderedPattern(pattern_table_[i].pattern)));
    }

    BitExtractFn bit_extract_fn =
        GetBitExtractFunction(GenerateSampledBitsMask());

    // Create a compiled node that contains a table with an entry for every bit
    // pattern.
    CreateCompiledNode(bit_extract_fn,
                       static_cast<size_t>(1) << GetSampledBitsCount());
    VIXL_ASSERT(compiled_node_ != NULL);

    // When we find a pattern matches the representation, set the node's decode
    // function for that representation to the corresponding function.
    for (uint32_t bits = 0; bits < (1U << GetSampledBitsCount()); bits++) {
      for (size_t i = 0; i < matches.size(); i++) {
        if ((bits & matches[i].first) == matches[i].second) {
          // Only one instruction class should match for each value of bits, so
          // if we get here, the node pointed to should still be unallocated.
          VIXL_ASSERT(compiled_node_->GetNodeForBits(bits) == NULL);
          CompileNodeForBits(decoder, pattern_table_[i].handler, bits);
          break;
        }
      }

      // If the decode_table_ entry for these bits is still NULL, the
      // instruction must be handled by the "otherwise" case, which by default
      // is the Unallocated visitor.
      if (compiled_node_->GetNodeForBits(bits) == NULL) {
        CompileNodeForBits(decoder, otherwise, bits);
      }
    }
  }

  VIXL_ASSERT(compiled_node_ != NULL);
  return compiled_node_;
}

void CompiledDecodeNode::Decode(const Instruction* instr) const {
  if (IsLeafNode()) {
    // If this node is a leaf, call the registered visitor function.
    VIXL_ASSERT(decoder_ != NULL);
    decoder_->VisitNamedInstruction(instr, instruction_name_);
  } else {
    // Otherwise, using the sampled bit extractor for this node, look up the
    // next node in the decode tree, and call its Decode method.
    VIXL_ASSERT(bit_extract_fn_ != NULL);
    VIXL_ASSERT((instr->*bit_extract_fn_)() < decode_table_size_);
    VIXL_ASSERT(decode_table_[(instr->*bit_extract_fn_)()] != NULL);
    decode_table_[(instr->*bit_extract_fn_)()]->Decode(instr);
  }
}

DecodeNode::MaskValuePair DecodeNode::GenerateMaskValuePair(
    uint32_t pattern) const {
  uint32_t mask = 0, value = 0;
  for (size_t i = 0; i < GetPatternLength(pattern); i++) {
    PatternSymbol sym = GetSymbolAt(pattern, i);
    mask = (mask << 1) | ((sym == PatternSymbol::kSymbolX) ? 0 : 1);
    value = (value << 1) | (static_cast<uint32_t>(sym) & 1);
  }
  return std::make_pair(mask, value);
}

uint32_t DecodeNode::GenerateOrderedPattern(uint32_t pattern) const {
  const std::vector<uint8_t>& sampled_bits = GetSampledBits();
  uint64_t temp = 0xffffffffffffffff;

  // Place symbols into the field of set bits. Symbols are two bits wide and
  // take values 0, 1 or 2, so 3 will represent "no symbol".
  for (size_t i = 0; i < sampled_bits.size(); i++) {
    int shift = sampled_bits[i] * 2;
    temp ^= static_cast<uint64_t>(kEndOfPattern) << shift;
    temp |= static_cast<uint64_t>(GetSymbolAt(pattern, i)) << shift;
  }

  // Iterate over temp and extract new pattern ordered by sample position.
  uint32_t result = kEndOfPattern;  // End of pattern marker.

  // Iterate over the pattern one symbol (two bits) at a time.
  for (int i = 62; i >= 0; i -= 2) {
    uint32_t sym = (temp >> i) & kPatternSymbolMask;

    // If this is a valid symbol, shift into the result.
    if (sym != kEndOfPattern) {
      result = (result << 2) | sym;
    }
  }

  // The length of the ordered pattern must be the same as the input pattern,
  // and the number of sampled bits.
  VIXL_ASSERT(GetPatternLength(result) == GetPatternLength(pattern));
  VIXL_ASSERT(GetPatternLength(result) == sampled_bits.size());

  return result;
}

uint32_t DecodeNode::GenerateSampledBitsMask() const {
  uint32_t mask = 0;
  for (int bit : GetSampledBits()) {
    mask |= 1 << bit;
  }
  return mask;
}

}  // namespace aarch64
}  // namespace vixl
