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

#include <cmath>

#include "simulator-aarch64.h"

namespace vixl {
namespace aarch64 {

using vixl::internal::SimFloat16;

template <typename T>
bool IsFloat64() {
  return false;
}
template <>
bool IsFloat64<double>() {
  return true;
}

template <typename T>
bool IsFloat32() {
  return false;
}
template <>
bool IsFloat32<float>() {
  return true;
}

template <typename T>
bool IsFloat16() {
  return false;
}
template <>
bool IsFloat16<Float16>() {
  return true;
}
template <>
bool IsFloat16<SimFloat16>() {
  return true;
}

template <>
double Simulator::FPDefaultNaN<double>() {
  return kFP64DefaultNaN;
}


template <>
float Simulator::FPDefaultNaN<float>() {
  return kFP32DefaultNaN;
}


template <>
SimFloat16 Simulator::FPDefaultNaN<SimFloat16>() {
  return SimFloat16(kFP16DefaultNaN);
}


double Simulator::FixedToDouble(int64_t src, int fbits, FPRounding round) {
  if (src >= 0) {
    return UFixedToDouble(src, fbits, round);
  } else if (src == INT64_MIN) {
    return -UFixedToDouble(src, fbits, round);
  } else {
    return -UFixedToDouble(-src, fbits, round);
  }
}


double Simulator::UFixedToDouble(uint64_t src, int fbits, FPRounding round) {
  // An input of 0 is a special case because the result is effectively
  // subnormal: The exponent is encoded as 0 and there is no implicit 1 bit.
  if (src == 0) {
    return 0.0;
  }

  // Calculate the exponent. The highest significant bit will have the value
  // 2^exponent.
  const int highest_significant_bit = 63 - CountLeadingZeros(src);
  const int64_t exponent = highest_significant_bit - fbits;

  return FPRoundToDouble(0, exponent, src, round);
}


float Simulator::FixedToFloat(int64_t src, int fbits, FPRounding round) {
  if (src >= 0) {
    return UFixedToFloat(src, fbits, round);
  } else if (src == INT64_MIN) {
    return -UFixedToFloat(src, fbits, round);
  } else {
    return -UFixedToFloat(-src, fbits, round);
  }
}


float Simulator::UFixedToFloat(uint64_t src, int fbits, FPRounding round) {
  // An input of 0 is a special case because the result is effectively
  // subnormal: The exponent is encoded as 0 and there is no implicit 1 bit.
  if (src == 0) {
    return 0.0f;
  }

  // Calculate the exponent. The highest significant bit will have the value
  // 2^exponent.
  const int highest_significant_bit = 63 - CountLeadingZeros(src);
  const int32_t exponent = highest_significant_bit - fbits;

  return FPRoundToFloat(0, exponent, src, round);
}


SimFloat16 Simulator::FixedToFloat16(int64_t src, int fbits, FPRounding round) {
  if (src >= 0) {
    return UFixedToFloat16(src, fbits, round);
  } else if (src == INT64_MIN) {
    return -UFixedToFloat16(src, fbits, round);
  } else {
    return -UFixedToFloat16(-src, fbits, round);
  }
}


SimFloat16 Simulator::UFixedToFloat16(uint64_t src,
                                      int fbits,
                                      FPRounding round) {
  // An input of 0 is a special case because the result is effectively
  // subnormal: The exponent is encoded as 0 and there is no implicit 1 bit.
  if (src == 0) {
    return 0.0f;
  }

  // Calculate the exponent. The highest significant bit will have the value
  // 2^exponent.
  const int highest_significant_bit = 63 - CountLeadingZeros(src);
  const int16_t exponent = highest_significant_bit - fbits;

  return FPRoundToFloat16(0, exponent, src, round);
}


uint64_t Simulator::GenerateRandomTag(uint16_t exclude) {
  uint64_t rtag = nrand48(rand_state_) >> 28;
  VIXL_ASSERT(IsUint4(rtag));

  if (exclude == 0) {
    exclude = nrand48(rand_state_) >> 27;
  }

  // TODO: implement this to better match the specification, which calls for a
  // true random mode, and a pseudo-random mode with state (EL1.TAG) modified by
  // PRNG.
  return ChooseNonExcludedTag(rtag, 0, exclude);
}


void Simulator::ld1(VectorFormat vform, LogicVRegister dst, uint64_t addr) {
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    LoadLane(dst, vform, i, addr);
    addr += LaneSizeInBytesFromFormat(vform);
  }
}


void Simulator::ld1(VectorFormat vform,
                    LogicVRegister dst,
                    int index,
                    uint64_t addr) {
  LoadLane(dst, vform, index, addr);
}


void Simulator::ld1r(VectorFormat vform,
                     VectorFormat unpack_vform,
                     LogicVRegister dst,
                     uint64_t addr,
                     bool is_signed) {
  unsigned unpack_size = LaneSizeInBytesFromFormat(unpack_vform);
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    if (is_signed) {
      LoadIntToLane(dst, vform, unpack_size, i, addr);
    } else {
      LoadUintToLane(dst, vform, unpack_size, i, addr);
    }
  }
}


void Simulator::ld1r(VectorFormat vform, LogicVRegister dst, uint64_t addr) {
  ld1r(vform, vform, dst, addr);
}


void Simulator::ld2(VectorFormat vform,
                    LogicVRegister dst1,
                    LogicVRegister dst2,
                    uint64_t addr1) {
  dst1.ClearForWrite(vform);
  dst2.ClearForWrite(vform);
  int esize = LaneSizeInBytesFromFormat(vform);
  uint64_t addr2 = addr1 + esize;
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    LoadLane(dst1, vform, i, addr1);
    LoadLane(dst2, vform, i, addr2);
    addr1 += 2 * esize;
    addr2 += 2 * esize;
  }
}


void Simulator::ld2(VectorFormat vform,
                    LogicVRegister dst1,
                    LogicVRegister dst2,
                    int index,
                    uint64_t addr1) {
  dst1.ClearForWrite(vform);
  dst2.ClearForWrite(vform);
  uint64_t addr2 = addr1 + LaneSizeInBytesFromFormat(vform);
  LoadLane(dst1, vform, index, addr1);
  LoadLane(dst2, vform, index, addr2);
}


void Simulator::ld2r(VectorFormat vform,
                     LogicVRegister dst1,
                     LogicVRegister dst2,
                     uint64_t addr) {
  dst1.ClearForWrite(vform);
  dst2.ClearForWrite(vform);
  uint64_t addr2 = addr + LaneSizeInBytesFromFormat(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    LoadLane(dst1, vform, i, addr);
    LoadLane(dst2, vform, i, addr2);
  }
}


void Simulator::ld3(VectorFormat vform,
                    LogicVRegister dst1,
                    LogicVRegister dst2,
                    LogicVRegister dst3,
                    uint64_t addr1) {
  dst1.ClearForWrite(vform);
  dst2.ClearForWrite(vform);
  dst3.ClearForWrite(vform);
  int esize = LaneSizeInBytesFromFormat(vform);
  uint64_t addr2 = addr1 + esize;
  uint64_t addr3 = addr2 + esize;
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    LoadLane(dst1, vform, i, addr1);
    LoadLane(dst2, vform, i, addr2);
    LoadLane(dst3, vform, i, addr3);
    addr1 += 3 * esize;
    addr2 += 3 * esize;
    addr3 += 3 * esize;
  }
}


void Simulator::ld3(VectorFormat vform,
                    LogicVRegister dst1,
                    LogicVRegister dst2,
                    LogicVRegister dst3,
                    int index,
                    uint64_t addr1) {
  dst1.ClearForWrite(vform);
  dst2.ClearForWrite(vform);
  dst3.ClearForWrite(vform);
  uint64_t addr2 = addr1 + LaneSizeInBytesFromFormat(vform);
  uint64_t addr3 = addr2 + LaneSizeInBytesFromFormat(vform);
  LoadLane(dst1, vform, index, addr1);
  LoadLane(dst2, vform, index, addr2);
  LoadLane(dst3, vform, index, addr3);
}


void Simulator::ld3r(VectorFormat vform,
                     LogicVRegister dst1,
                     LogicVRegister dst2,
                     LogicVRegister dst3,
                     uint64_t addr) {
  dst1.ClearForWrite(vform);
  dst2.ClearForWrite(vform);
  dst3.ClearForWrite(vform);
  uint64_t addr2 = addr + LaneSizeInBytesFromFormat(vform);
  uint64_t addr3 = addr2 + LaneSizeInBytesFromFormat(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    LoadLane(dst1, vform, i, addr);
    LoadLane(dst2, vform, i, addr2);
    LoadLane(dst3, vform, i, addr3);
  }
}


void Simulator::ld4(VectorFormat vform,
                    LogicVRegister dst1,
                    LogicVRegister dst2,
                    LogicVRegister dst3,
                    LogicVRegister dst4,
                    uint64_t addr1) {
  dst1.ClearForWrite(vform);
  dst2.ClearForWrite(vform);
  dst3.ClearForWrite(vform);
  dst4.ClearForWrite(vform);
  int esize = LaneSizeInBytesFromFormat(vform);
  uint64_t addr2 = addr1 + esize;
  uint64_t addr3 = addr2 + esize;
  uint64_t addr4 = addr3 + esize;
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    LoadLane(dst1, vform, i, addr1);
    LoadLane(dst2, vform, i, addr2);
    LoadLane(dst3, vform, i, addr3);
    LoadLane(dst4, vform, i, addr4);
    addr1 += 4 * esize;
    addr2 += 4 * esize;
    addr3 += 4 * esize;
    addr4 += 4 * esize;
  }
}


void Simulator::ld4(VectorFormat vform,
                    LogicVRegister dst1,
                    LogicVRegister dst2,
                    LogicVRegister dst3,
                    LogicVRegister dst4,
                    int index,
                    uint64_t addr1) {
  dst1.ClearForWrite(vform);
  dst2.ClearForWrite(vform);
  dst3.ClearForWrite(vform);
  dst4.ClearForWrite(vform);
  uint64_t addr2 = addr1 + LaneSizeInBytesFromFormat(vform);
  uint64_t addr3 = addr2 + LaneSizeInBytesFromFormat(vform);
  uint64_t addr4 = addr3 + LaneSizeInBytesFromFormat(vform);
  LoadLane(dst1, vform, index, addr1);
  LoadLane(dst2, vform, index, addr2);
  LoadLane(dst3, vform, index, addr3);
  LoadLane(dst4, vform, index, addr4);
}


void Simulator::ld4r(VectorFormat vform,
                     LogicVRegister dst1,
                     LogicVRegister dst2,
                     LogicVRegister dst3,
                     LogicVRegister dst4,
                     uint64_t addr) {
  dst1.ClearForWrite(vform);
  dst2.ClearForWrite(vform);
  dst3.ClearForWrite(vform);
  dst4.ClearForWrite(vform);
  uint64_t addr2 = addr + LaneSizeInBytesFromFormat(vform);
  uint64_t addr3 = addr2 + LaneSizeInBytesFromFormat(vform);
  uint64_t addr4 = addr3 + LaneSizeInBytesFromFormat(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    LoadLane(dst1, vform, i, addr);
    LoadLane(dst2, vform, i, addr2);
    LoadLane(dst3, vform, i, addr3);
    LoadLane(dst4, vform, i, addr4);
  }
}


void Simulator::st1(VectorFormat vform, LogicVRegister src, uint64_t addr) {
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    StoreLane(src, vform, i, addr);
    addr += LaneSizeInBytesFromFormat(vform);
  }
}


void Simulator::st1(VectorFormat vform,
                    LogicVRegister src,
                    int index,
                    uint64_t addr) {
  StoreLane(src, vform, index, addr);
}


void Simulator::st2(VectorFormat vform,
                    LogicVRegister src,
                    LogicVRegister src2,
                    uint64_t addr) {
  int esize = LaneSizeInBytesFromFormat(vform);
  uint64_t addr2 = addr + esize;
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    StoreLane(src, vform, i, addr);
    StoreLane(src2, vform, i, addr2);
    addr += 2 * esize;
    addr2 += 2 * esize;
  }
}


void Simulator::st2(VectorFormat vform,
                    LogicVRegister src,
                    LogicVRegister src2,
                    int index,
                    uint64_t addr) {
  int esize = LaneSizeInBytesFromFormat(vform);
  StoreLane(src, vform, index, addr);
  StoreLane(src2, vform, index, addr + 1 * esize);
}


void Simulator::st3(VectorFormat vform,
                    LogicVRegister src,
                    LogicVRegister src2,
                    LogicVRegister src3,
                    uint64_t addr) {
  int esize = LaneSizeInBytesFromFormat(vform);
  uint64_t addr2 = addr + esize;
  uint64_t addr3 = addr2 + esize;
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    StoreLane(src, vform, i, addr);
    StoreLane(src2, vform, i, addr2);
    StoreLane(src3, vform, i, addr3);
    addr += 3 * esize;
    addr2 += 3 * esize;
    addr3 += 3 * esize;
  }
}


void Simulator::st3(VectorFormat vform,
                    LogicVRegister src,
                    LogicVRegister src2,
                    LogicVRegister src3,
                    int index,
                    uint64_t addr) {
  int esize = LaneSizeInBytesFromFormat(vform);
  StoreLane(src, vform, index, addr);
  StoreLane(src2, vform, index, addr + 1 * esize);
  StoreLane(src3, vform, index, addr + 2 * esize);
}


void Simulator::st4(VectorFormat vform,
                    LogicVRegister src,
                    LogicVRegister src2,
                    LogicVRegister src3,
                    LogicVRegister src4,
                    uint64_t addr) {
  int esize = LaneSizeInBytesFromFormat(vform);
  uint64_t addr2 = addr + esize;
  uint64_t addr3 = addr2 + esize;
  uint64_t addr4 = addr3 + esize;
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    StoreLane(src, vform, i, addr);
    StoreLane(src2, vform, i, addr2);
    StoreLane(src3, vform, i, addr3);
    StoreLane(src4, vform, i, addr4);
    addr += 4 * esize;
    addr2 += 4 * esize;
    addr3 += 4 * esize;
    addr4 += 4 * esize;
  }
}


void Simulator::st4(VectorFormat vform,
                    LogicVRegister src,
                    LogicVRegister src2,
                    LogicVRegister src3,
                    LogicVRegister src4,
                    int index,
                    uint64_t addr) {
  int esize = LaneSizeInBytesFromFormat(vform);
  StoreLane(src, vform, index, addr);
  StoreLane(src2, vform, index, addr + 1 * esize);
  StoreLane(src3, vform, index, addr + 2 * esize);
  StoreLane(src4, vform, index, addr + 3 * esize);
}


LogicVRegister Simulator::cmp(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& src1,
                              const LogicVRegister& src2,
                              Condition cond) {
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    int64_t sa = src1.Int(vform, i);
    int64_t sb = src2.Int(vform, i);
    uint64_t ua = src1.Uint(vform, i);
    uint64_t ub = src2.Uint(vform, i);
    bool result = false;
    switch (cond) {
      case eq:
        result = (ua == ub);
        break;
      case ge:
        result = (sa >= sb);
        break;
      case gt:
        result = (sa > sb);
        break;
      case hi:
        result = (ua > ub);
        break;
      case hs:
        result = (ua >= ub);
        break;
      case lt:
        result = (sa < sb);
        break;
      case le:
        result = (sa <= sb);
        break;
      default:
        VIXL_UNREACHABLE();
        break;
    }
    dst.SetUint(vform, i, result ? MaxUintFromFormat(vform) : 0);
  }
  return dst;
}


LogicVRegister Simulator::cmp(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& src1,
                              int imm,
                              Condition cond) {
  SimVRegister temp;
  LogicVRegister imm_reg = dup_immediate(vform, temp, imm);
  return cmp(vform, dst, src1, imm_reg, cond);
}


LogicVRegister Simulator::cmptst(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    uint64_t ua = src1.Uint(vform, i);
    uint64_t ub = src2.Uint(vform, i);
    dst.SetUint(vform, i, ((ua & ub) != 0) ? MaxUintFromFormat(vform) : 0);
  }
  return dst;
}


LogicVRegister Simulator::add(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& src1,
                              const LogicVRegister& src2) {
  int lane_size = LaneSizeInBitsFromFormat(vform);
  dst.ClearForWrite(vform);

  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    // Test for unsigned saturation.
    uint64_t ua = src1.UintLeftJustified(vform, i);
    uint64_t ub = src2.UintLeftJustified(vform, i);
    uint64_t ur = ua + ub;
    if (ur < ua) {
      dst.SetUnsignedSat(i, true);
    }

    // Test for signed saturation.
    bool pos_a = (ua >> 63) == 0;
    bool pos_b = (ub >> 63) == 0;
    bool pos_r = (ur >> 63) == 0;
    // If the signs of the operands are the same, but different from the result,
    // there was an overflow.
    if ((pos_a == pos_b) && (pos_a != pos_r)) {
      dst.SetSignedSat(i, pos_a);
    }
    dst.SetInt(vform, i, ur >> (64 - lane_size));
  }
  return dst;
}

LogicVRegister Simulator::add_uint(VectorFormat vform,
                                   LogicVRegister dst,
                                   const LogicVRegister& src1,
                                   uint64_t value) {
  int lane_size = LaneSizeInBitsFromFormat(vform);
  VIXL_ASSERT(IsUintN(lane_size, value));
  dst.ClearForWrite(vform);
  // Left-justify `value`.
  uint64_t ub = value << (64 - lane_size);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    // Test for unsigned saturation.
    uint64_t ua = src1.UintLeftJustified(vform, i);
    uint64_t ur = ua + ub;
    if (ur < ua) {
      dst.SetUnsignedSat(i, true);
    }

    // Test for signed saturation.
    // `value` is always positive, so we have an overflow if the (signed) result
    // is smaller than the first operand.
    if (RawbitsToInt64(ur) < RawbitsToInt64(ua)) {
      dst.SetSignedSat(i, true);
    }

    dst.SetInt(vform, i, ur >> (64 - lane_size));
  }
  return dst;
}

LogicVRegister Simulator::addp(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2) {
  SimVRegister temp1, temp2;
  uzp1(vform, temp1, src1, src2);
  uzp2(vform, temp2, src1, src2);
  add(vform, dst, temp1, temp2);
  if (IsSVEFormat(vform)) {
    interleave_top_bottom(vform, dst, dst);
  }
  return dst;
}

LogicVRegister Simulator::sdiv(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2) {
  VIXL_ASSERT((vform == kFormatVnS) || (vform == kFormatVnD));

  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    int64_t val1 = src1.Int(vform, i);
    int64_t val2 = src2.Int(vform, i);
    int64_t min_int = (vform == kFormatVnD) ? kXMinInt : kWMinInt;
    int64_t quotient = 0;
    if ((val1 == min_int) && (val2 == -1)) {
      quotient = min_int;
    } else if (val2 != 0) {
      quotient = val1 / val2;
    }
    dst.SetInt(vform, i, quotient);
  }

  return dst;
}

LogicVRegister Simulator::udiv(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2) {
  VIXL_ASSERT((vform == kFormatVnS) || (vform == kFormatVnD));

  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    uint64_t val1 = src1.Uint(vform, i);
    uint64_t val2 = src2.Uint(vform, i);
    uint64_t quotient = 0;
    if (val2 != 0) {
      quotient = val1 / val2;
    }
    dst.SetUint(vform, i, quotient);
  }

  return dst;
}


LogicVRegister Simulator::mla(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& srca,
                              const LogicVRegister& src1,
                              const LogicVRegister& src2) {
  SimVRegister temp;
  mul(vform, temp, src1, src2);
  add(vform, dst, srca, temp);
  return dst;
}


LogicVRegister Simulator::mls(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& srca,
                              const LogicVRegister& src1,
                              const LogicVRegister& src2) {
  SimVRegister temp;
  mul(vform, temp, src1, src2);
  sub(vform, dst, srca, temp);
  return dst;
}


LogicVRegister Simulator::mul(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& src1,
                              const LogicVRegister& src2) {
  dst.ClearForWrite(vform);

  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    dst.SetUint(vform, i, src1.Uint(vform, i) * src2.Uint(vform, i));
  }
  return dst;
}


LogicVRegister Simulator::mul(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& src1,
                              const LogicVRegister& src2,
                              int index) {
  SimVRegister temp;
  VectorFormat indexform = VectorFormatFillQ(vform);
  return mul(vform, dst, src1, dup_element(indexform, temp, src2, index));
}


LogicVRegister Simulator::smulh(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2) {
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    int64_t dst_val = 0xbadbeef;
    int64_t val1 = src1.Int(vform, i);
    int64_t val2 = src2.Int(vform, i);
    switch (LaneSizeInBitsFromFormat(vform)) {
      case 8:
        dst_val = internal::MultiplyHigh<8>(val1, val2);
        break;
      case 16:
        dst_val = internal::MultiplyHigh<16>(val1, val2);
        break;
      case 32:
        dst_val = internal::MultiplyHigh<32>(val1, val2);
        break;
      case 64:
        dst_val = internal::MultiplyHigh<64>(val1, val2);
        break;
      default:
        VIXL_UNREACHABLE();
        break;
    }
    dst.SetInt(vform, i, dst_val);
  }
  return dst;
}


LogicVRegister Simulator::umulh(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2) {
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    uint64_t dst_val = 0xbadbeef;
    uint64_t val1 = src1.Uint(vform, i);
    uint64_t val2 = src2.Uint(vform, i);
    switch (LaneSizeInBitsFromFormat(vform)) {
      case 8:
        dst_val = internal::MultiplyHigh<8>(val1, val2);
        break;
      case 16:
        dst_val = internal::MultiplyHigh<16>(val1, val2);
        break;
      case 32:
        dst_val = internal::MultiplyHigh<32>(val1, val2);
        break;
      case 64:
        dst_val = internal::MultiplyHigh<64>(val1, val2);
        break;
      default:
        VIXL_UNREACHABLE();
        break;
    }
    dst.SetUint(vform, i, dst_val);
  }
  return dst;
}


LogicVRegister Simulator::mla(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& src1,
                              const LogicVRegister& src2,
                              int index) {
  SimVRegister temp;
  VectorFormat indexform = VectorFormatFillQ(vform);
  return mla(vform, dst, dst, src1, dup_element(indexform, temp, src2, index));
}


LogicVRegister Simulator::mls(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& src1,
                              const LogicVRegister& src2,
                              int index) {
  SimVRegister temp;
  VectorFormat indexform = VectorFormatFillQ(vform);
  return mls(vform, dst, dst, src1, dup_element(indexform, temp, src2, index));
}

LogicVRegister Simulator::sqdmull(VectorFormat vform,
                                  LogicVRegister dst,
                                  const LogicVRegister& src1,
                                  const LogicVRegister& src2,
                                  int index) {
  SimVRegister temp;
  VectorFormat indexform =
      VectorFormatHalfWidthDoubleLanes(VectorFormatFillQ(vform));
  return sqdmull(vform, dst, src1, dup_element(indexform, temp, src2, index));
}

LogicVRegister Simulator::sqdmlal(VectorFormat vform,
                                  LogicVRegister dst,
                                  const LogicVRegister& src1,
                                  const LogicVRegister& src2,
                                  int index) {
  SimVRegister temp;
  VectorFormat indexform =
      VectorFormatHalfWidthDoubleLanes(VectorFormatFillQ(vform));
  return sqdmlal(vform, dst, src1, dup_element(indexform, temp, src2, index));
}

LogicVRegister Simulator::sqdmlsl(VectorFormat vform,
                                  LogicVRegister dst,
                                  const LogicVRegister& src1,
                                  const LogicVRegister& src2,
                                  int index) {
  SimVRegister temp;
  VectorFormat indexform =
      VectorFormatHalfWidthDoubleLanes(VectorFormatFillQ(vform));
  return sqdmlsl(vform, dst, src1, dup_element(indexform, temp, src2, index));
}

LogicVRegister Simulator::sqdmulh(VectorFormat vform,
                                  LogicVRegister dst,
                                  const LogicVRegister& src1,
                                  const LogicVRegister& src2,
                                  int index) {
  SimVRegister temp;
  VectorFormat indexform = VectorFormatFillQ(vform);
  return sqdmulh(vform, dst, src1, dup_element(indexform, temp, src2, index));
}


LogicVRegister Simulator::sqrdmulh(VectorFormat vform,
                                   LogicVRegister dst,
                                   const LogicVRegister& src1,
                                   const LogicVRegister& src2,
                                   int index) {
  SimVRegister temp;
  VectorFormat indexform = VectorFormatFillQ(vform);
  return sqrdmulh(vform, dst, src1, dup_element(indexform, temp, src2, index));
}


LogicVRegister Simulator::sqrdmlah(VectorFormat vform,
                                   LogicVRegister dst,
                                   const LogicVRegister& src1,
                                   const LogicVRegister& src2,
                                   int index) {
  SimVRegister temp;
  VectorFormat indexform = VectorFormatFillQ(vform);
  return sqrdmlah(vform, dst, src1, dup_element(indexform, temp, src2, index));
}


LogicVRegister Simulator::sqrdmlsh(VectorFormat vform,
                                   LogicVRegister dst,
                                   const LogicVRegister& src1,
                                   const LogicVRegister& src2,
                                   int index) {
  SimVRegister temp;
  VectorFormat indexform = VectorFormatFillQ(vform);
  return sqrdmlsh(vform, dst, src1, dup_element(indexform, temp, src2, index));
}


uint64_t Simulator::PolynomialMult(uint64_t op1,
                                   uint64_t op2,
                                   int lane_size_in_bits) const {
  VIXL_ASSERT(static_cast<unsigned>(lane_size_in_bits) <= kSRegSize);
  VIXL_ASSERT(IsUintN(lane_size_in_bits, op1));
  VIXL_ASSERT(IsUintN(lane_size_in_bits, op2));
  uint64_t result = 0;
  for (int i = 0; i < lane_size_in_bits; ++i) {
    if ((op1 >> i) & 1) {
      result = result ^ (op2 << i);
    }
  }
  return result;
}


LogicVRegister Simulator::pmul(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2) {
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    dst.SetUint(vform,
                i,
                PolynomialMult(src1.Uint(vform, i),
                               src2.Uint(vform, i),
                               LaneSizeInBitsFromFormat(vform)));
  }
  return dst;
}


LogicVRegister Simulator::pmull(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2) {
  dst.ClearForWrite(vform);

  VectorFormat vform_src = VectorFormatHalfWidth(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    dst.SetUint(vform,
                i,
                PolynomialMult(src1.Uint(vform_src, i),
                               src2.Uint(vform_src, i),
                               LaneSizeInBitsFromFormat(vform_src)));
  }

  return dst;
}


LogicVRegister Simulator::pmull2(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  VectorFormat vform_src = VectorFormatHalfWidthDoubleLanes(vform);
  dst.ClearForWrite(vform);
  int lane_count = LaneCountFromFormat(vform);
  for (int i = 0; i < lane_count; i++) {
    dst.SetUint(vform,
                i,
                PolynomialMult(src1.Uint(vform_src, lane_count + i),
                               src2.Uint(vform_src, lane_count + i),
                               LaneSizeInBitsFromFormat(vform_src)));
  }
  return dst;
}


LogicVRegister Simulator::sub(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& src1,
                              const LogicVRegister& src2) {
  int lane_size = LaneSizeInBitsFromFormat(vform);
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    // Test for unsigned saturation.
    uint64_t ua = src1.UintLeftJustified(vform, i);
    uint64_t ub = src2.UintLeftJustified(vform, i);
    uint64_t ur = ua - ub;
    if (ub > ua) {
      dst.SetUnsignedSat(i, false);
    }

    // Test for signed saturation.
    bool pos_a = (ua >> 63) == 0;
    bool pos_b = (ub >> 63) == 0;
    bool pos_r = (ur >> 63) == 0;
    // If the signs of the operands are different, and the sign of the first
    // operand doesn't match the result, there was an overflow.
    if ((pos_a != pos_b) && (pos_a != pos_r)) {
      dst.SetSignedSat(i, pos_a);
    }

    dst.SetInt(vform, i, ur >> (64 - lane_size));
  }
  return dst;
}

LogicVRegister Simulator::sub_uint(VectorFormat vform,
                                   LogicVRegister dst,
                                   const LogicVRegister& src1,
                                   uint64_t value) {
  int lane_size = LaneSizeInBitsFromFormat(vform);
  VIXL_ASSERT(IsUintN(lane_size, value));
  dst.ClearForWrite(vform);
  // Left-justify `value`.
  uint64_t ub = value << (64 - lane_size);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    // Test for unsigned saturation.
    uint64_t ua = src1.UintLeftJustified(vform, i);
    uint64_t ur = ua - ub;
    if (ub > ua) {
      dst.SetUnsignedSat(i, false);
    }

    // Test for signed saturation.
    // `value` is always positive, so we have an overflow if the (signed) result
    // is greater than the first operand.
    if (RawbitsToInt64(ur) > RawbitsToInt64(ua)) {
      dst.SetSignedSat(i, false);
    }

    dst.SetInt(vform, i, ur >> (64 - lane_size));
  }
  return dst;
}

LogicVRegister Simulator::and_(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2) {
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    dst.SetUint(vform, i, src1.Uint(vform, i) & src2.Uint(vform, i));
  }
  return dst;
}


LogicVRegister Simulator::orr(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& src1,
                              const LogicVRegister& src2) {
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    dst.SetUint(vform, i, src1.Uint(vform, i) | src2.Uint(vform, i));
  }
  return dst;
}


LogicVRegister Simulator::orn(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& src1,
                              const LogicVRegister& src2) {
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    dst.SetUint(vform, i, src1.Uint(vform, i) | ~src2.Uint(vform, i));
  }
  return dst;
}


LogicVRegister Simulator::eor(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& src1,
                              const LogicVRegister& src2) {
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    dst.SetUint(vform, i, src1.Uint(vform, i) ^ src2.Uint(vform, i));
  }
  return dst;
}


LogicVRegister Simulator::bic(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& src1,
                              const LogicVRegister& src2) {
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    dst.SetUint(vform, i, src1.Uint(vform, i) & ~src2.Uint(vform, i));
  }
  return dst;
}


LogicVRegister Simulator::bic(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& src,
                              uint64_t imm) {
  uint64_t result[16];
  int lane_count = LaneCountFromFormat(vform);
  for (int i = 0; i < lane_count; ++i) {
    result[i] = src.Uint(vform, i) & ~imm;
  }
  dst.ClearForWrite(vform);
  for (int i = 0; i < lane_count; ++i) {
    dst.SetUint(vform, i, result[i]);
  }
  return dst;
}


LogicVRegister Simulator::bif(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& src1,
                              const LogicVRegister& src2) {
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    uint64_t operand1 = dst.Uint(vform, i);
    uint64_t operand2 = ~src2.Uint(vform, i);
    uint64_t operand3 = src1.Uint(vform, i);
    uint64_t result = operand1 ^ ((operand1 ^ operand3) & operand2);
    dst.SetUint(vform, i, result);
  }
  return dst;
}


LogicVRegister Simulator::bit(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& src1,
                              const LogicVRegister& src2) {
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    uint64_t operand1 = dst.Uint(vform, i);
    uint64_t operand2 = src2.Uint(vform, i);
    uint64_t operand3 = src1.Uint(vform, i);
    uint64_t result = operand1 ^ ((operand1 ^ operand3) & operand2);
    dst.SetUint(vform, i, result);
  }
  return dst;
}


LogicVRegister Simulator::bsl(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& src_mask,
                              const LogicVRegister& src1,
                              const LogicVRegister& src2) {
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    uint64_t operand1 = src2.Uint(vform, i);
    uint64_t operand2 = src_mask.Uint(vform, i);
    uint64_t operand3 = src1.Uint(vform, i);
    uint64_t result = operand1 ^ ((operand1 ^ operand3) & operand2);
    dst.SetUint(vform, i, result);
  }
  return dst;
}


LogicVRegister Simulator::sminmax(VectorFormat vform,
                                  LogicVRegister dst,
                                  const LogicVRegister& src1,
                                  const LogicVRegister& src2,
                                  bool max) {
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    int64_t src1_val = src1.Int(vform, i);
    int64_t src2_val = src2.Int(vform, i);
    int64_t dst_val;
    if (max) {
      dst_val = (src1_val > src2_val) ? src1_val : src2_val;
    } else {
      dst_val = (src1_val < src2_val) ? src1_val : src2_val;
    }
    dst.SetInt(vform, i, dst_val);
  }
  return dst;
}


LogicVRegister Simulator::smax(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2) {
  return sminmax(vform, dst, src1, src2, true);
}


LogicVRegister Simulator::smin(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2) {
  return sminmax(vform, dst, src1, src2, false);
}


LogicVRegister Simulator::sminmaxp(VectorFormat vform,
                                   LogicVRegister dst,
                                   const LogicVRegister& src1,
                                   const LogicVRegister& src2,
                                   bool max) {
  unsigned lanes = LaneCountFromFormat(vform);
  int64_t result[kZRegMaxSizeInBytes];
  const LogicVRegister* src = &src1;
  for (unsigned j = 0; j < 2; j++) {
    for (unsigned i = 0; i < lanes; i += 2) {
      int64_t first_val = src->Int(vform, i);
      int64_t second_val = src->Int(vform, i + 1);
      int64_t dst_val;
      if (max) {
        dst_val = (first_val > second_val) ? first_val : second_val;
      } else {
        dst_val = (first_val < second_val) ? first_val : second_val;
      }
      VIXL_ASSERT(((i >> 1) + (j * lanes / 2)) < ArrayLength(result));
      result[(i >> 1) + (j * lanes / 2)] = dst_val;
    }
    src = &src2;
  }
  dst.SetIntArray(vform, result);
  if (IsSVEFormat(vform)) {
    interleave_top_bottom(vform, dst, dst);
  }
  return dst;
}


LogicVRegister Simulator::smaxp(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2) {
  return sminmaxp(vform, dst, src1, src2, true);
}


LogicVRegister Simulator::sminp(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2) {
  return sminmaxp(vform, dst, src1, src2, false);
}


LogicVRegister Simulator::addp(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src) {
  VIXL_ASSERT(vform == kFormatD);

  uint64_t dst_val = src.Uint(kFormat2D, 0) + src.Uint(kFormat2D, 1);
  dst.ClearForWrite(vform);
  dst.SetUint(vform, 0, dst_val);
  return dst;
}


LogicVRegister Simulator::addv(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src) {
  VectorFormat vform_dst =
      ScalarFormatFromLaneSize(LaneSizeInBitsFromFormat(vform));


  int64_t dst_val = 0;
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    dst_val += src.Int(vform, i);
  }

  dst.ClearForWrite(vform_dst);
  dst.SetInt(vform_dst, 0, dst_val);
  return dst;
}


LogicVRegister Simulator::saddlv(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src) {
  VectorFormat vform_dst =
      ScalarFormatFromLaneSize(LaneSizeInBitsFromFormat(vform) * 2);

  int64_t dst_val = 0;
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    dst_val += src.Int(vform, i);
  }

  dst.ClearForWrite(vform_dst);
  dst.SetInt(vform_dst, 0, dst_val);
  return dst;
}


LogicVRegister Simulator::uaddlv(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src) {
  VectorFormat vform_dst =
      ScalarFormatFromLaneSize(LaneSizeInBitsFromFormat(vform) * 2);

  uint64_t dst_val = 0;
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    dst_val += src.Uint(vform, i);
  }

  dst.ClearForWrite(vform_dst);
  dst.SetUint(vform_dst, 0, dst_val);
  return dst;
}


LogicVRegister Simulator::sminmaxv(VectorFormat vform,
                                   LogicVRegister dst,
                                   const LogicPRegister& pg,
                                   const LogicVRegister& src,
                                   bool max) {
  int64_t dst_val = max ? INT64_MIN : INT64_MAX;
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    if (!pg.IsActive(vform, i)) continue;

    int64_t src_val = src.Int(vform, i);
    if (max) {
      dst_val = (src_val > dst_val) ? src_val : dst_val;
    } else {
      dst_val = (src_val < dst_val) ? src_val : dst_val;
    }
  }
  dst.ClearForWrite(ScalarFormatFromFormat(vform));
  dst.SetInt(vform, 0, dst_val);
  return dst;
}


LogicVRegister Simulator::smaxv(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src) {
  sminmaxv(vform, dst, GetPTrue(), src, true);
  return dst;
}


LogicVRegister Simulator::sminv(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src) {
  sminmaxv(vform, dst, GetPTrue(), src, false);
  return dst;
}


LogicVRegister Simulator::smaxv(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicPRegister& pg,
                                const LogicVRegister& src) {
  VIXL_ASSERT(IsSVEFormat(vform));
  sminmaxv(vform, dst, pg, src, true);
  return dst;
}


LogicVRegister Simulator::sminv(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicPRegister& pg,
                                const LogicVRegister& src) {
  VIXL_ASSERT(IsSVEFormat(vform));
  sminmaxv(vform, dst, pg, src, false);
  return dst;
}


LogicVRegister Simulator::uminmax(VectorFormat vform,
                                  LogicVRegister dst,
                                  const LogicVRegister& src1,
                                  const LogicVRegister& src2,
                                  bool max) {
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    uint64_t src1_val = src1.Uint(vform, i);
    uint64_t src2_val = src2.Uint(vform, i);
    uint64_t dst_val;
    if (max) {
      dst_val = (src1_val > src2_val) ? src1_val : src2_val;
    } else {
      dst_val = (src1_val < src2_val) ? src1_val : src2_val;
    }
    dst.SetUint(vform, i, dst_val);
  }
  return dst;
}


LogicVRegister Simulator::umax(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2) {
  return uminmax(vform, dst, src1, src2, true);
}


LogicVRegister Simulator::umin(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2) {
  return uminmax(vform, dst, src1, src2, false);
}


LogicVRegister Simulator::uminmaxp(VectorFormat vform,
                                   LogicVRegister dst,
                                   const LogicVRegister& src1,
                                   const LogicVRegister& src2,
                                   bool max) {
  unsigned lanes = LaneCountFromFormat(vform);
  uint64_t result[kZRegMaxSizeInBytes];
  const LogicVRegister* src = &src1;
  for (unsigned j = 0; j < 2; j++) {
    for (unsigned i = 0; i < lanes; i += 2) {
      uint64_t first_val = src->Uint(vform, i);
      uint64_t second_val = src->Uint(vform, i + 1);
      uint64_t dst_val;
      if (max) {
        dst_val = (first_val > second_val) ? first_val : second_val;
      } else {
        dst_val = (first_val < second_val) ? first_val : second_val;
      }
      VIXL_ASSERT(((i >> 1) + (j * lanes / 2)) < ArrayLength(result));
      result[(i >> 1) + (j * lanes / 2)] = dst_val;
    }
    src = &src2;
  }
  dst.SetUintArray(vform, result);
  if (IsSVEFormat(vform)) {
    interleave_top_bottom(vform, dst, dst);
  }
  return dst;
}


LogicVRegister Simulator::umaxp(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2) {
  return uminmaxp(vform, dst, src1, src2, true);
}


LogicVRegister Simulator::uminp(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2) {
  return uminmaxp(vform, dst, src1, src2, false);
}


LogicVRegister Simulator::uminmaxv(VectorFormat vform,
                                   LogicVRegister dst,
                                   const LogicPRegister& pg,
                                   const LogicVRegister& src,
                                   bool max) {
  uint64_t dst_val = max ? 0 : UINT64_MAX;
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    if (!pg.IsActive(vform, i)) continue;

    uint64_t src_val = src.Uint(vform, i);
    if (max) {
      dst_val = (src_val > dst_val) ? src_val : dst_val;
    } else {
      dst_val = (src_val < dst_val) ? src_val : dst_val;
    }
  }
  dst.ClearForWrite(ScalarFormatFromFormat(vform));
  dst.SetUint(vform, 0, dst_val);
  return dst;
}


LogicVRegister Simulator::umaxv(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src) {
  uminmaxv(vform, dst, GetPTrue(), src, true);
  return dst;
}


LogicVRegister Simulator::uminv(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src) {
  uminmaxv(vform, dst, GetPTrue(), src, false);
  return dst;
}


LogicVRegister Simulator::umaxv(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicPRegister& pg,
                                const LogicVRegister& src) {
  VIXL_ASSERT(IsSVEFormat(vform));
  uminmaxv(vform, dst, pg, src, true);
  return dst;
}


LogicVRegister Simulator::uminv(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicPRegister& pg,
                                const LogicVRegister& src) {
  VIXL_ASSERT(IsSVEFormat(vform));
  uminmaxv(vform, dst, pg, src, false);
  return dst;
}


LogicVRegister Simulator::shl(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& src,
                              int shift) {
  VIXL_ASSERT(shift >= 0);
  SimVRegister temp;
  LogicVRegister shiftreg = dup_immediate(vform, temp, shift);
  return ushl(vform, dst, src, shiftreg);
}


LogicVRegister Simulator::sshll(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src,
                                int shift) {
  VIXL_ASSERT(shift >= 0);
  SimVRegister temp1, temp2;
  LogicVRegister shiftreg = dup_immediate(vform, temp1, shift);
  LogicVRegister extendedreg = sxtl(vform, temp2, src);
  return sshl(vform, dst, extendedreg, shiftreg);
}


LogicVRegister Simulator::sshll2(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src,
                                 int shift) {
  VIXL_ASSERT(shift >= 0);
  SimVRegister temp1, temp2;
  LogicVRegister shiftreg = dup_immediate(vform, temp1, shift);
  LogicVRegister extendedreg = sxtl2(vform, temp2, src);
  return sshl(vform, dst, extendedreg, shiftreg);
}


LogicVRegister Simulator::shll(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src) {
  int shift = LaneSizeInBitsFromFormat(vform) / 2;
  return sshll(vform, dst, src, shift);
}


LogicVRegister Simulator::shll2(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src) {
  int shift = LaneSizeInBitsFromFormat(vform) / 2;
  return sshll2(vform, dst, src, shift);
}


LogicVRegister Simulator::ushll(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src,
                                int shift) {
  VIXL_ASSERT(shift >= 0);
  SimVRegister temp1, temp2;
  LogicVRegister shiftreg = dup_immediate(vform, temp1, shift);
  LogicVRegister extendedreg = uxtl(vform, temp2, src);
  return ushl(vform, dst, extendedreg, shiftreg);
}


LogicVRegister Simulator::ushll2(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src,
                                 int shift) {
  VIXL_ASSERT(shift >= 0);
  SimVRegister temp1, temp2;
  LogicVRegister shiftreg = dup_immediate(vform, temp1, shift);
  LogicVRegister extendedreg = uxtl2(vform, temp2, src);
  return ushl(vform, dst, extendedreg, shiftreg);
}

std::pair<bool, uint64_t> Simulator::clast(VectorFormat vform,
                                           const LogicPRegister& pg,
                                           const LogicVRegister& src,
                                           int offset_from_last_active) {
  // Untested for any other values.
  VIXL_ASSERT((offset_from_last_active == 0) || (offset_from_last_active == 1));

  int last_active = GetLastActive(vform, pg);
  int lane_count = LaneCountFromFormat(vform);
  int index =
      ((last_active + offset_from_last_active) + lane_count) % lane_count;
  return std::make_pair(last_active >= 0, src.Uint(vform, index));
}

LogicVRegister Simulator::compact(VectorFormat vform,
                                  LogicVRegister dst,
                                  const LogicPRegister& pg,
                                  const LogicVRegister& src) {
  int j = 0;
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    if (pg.IsActive(vform, i)) {
      dst.SetUint(vform, j++, src.Uint(vform, i));
    }
  }
  for (; j < LaneCountFromFormat(vform); j++) {
    dst.SetUint(vform, j, 0);
  }
  return dst;
}

LogicVRegister Simulator::splice(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicPRegister& pg,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  int lane_count = LaneCountFromFormat(vform);
  int first_active = GetFirstActive(vform, pg);
  int last_active = GetLastActive(vform, pg);
  int dst_idx = 0;
  uint64_t result[kZRegMaxSizeInBytes];

  if (first_active >= 0) {
    VIXL_ASSERT(last_active >= first_active);
    VIXL_ASSERT(last_active < lane_count);
    for (int i = first_active; i <= last_active; i++) {
      result[dst_idx++] = src1.Uint(vform, i);
    }
  }

  VIXL_ASSERT(dst_idx <= lane_count);
  for (int i = dst_idx; i < lane_count; i++) {
    result[i] = src2.Uint(vform, i - dst_idx);
  }

  dst.SetUintArray(vform, result);

  return dst;
}

LogicVRegister Simulator::sel(VectorFormat vform,
                              LogicVRegister dst,
                              const SimPRegister& pg,
                              const LogicVRegister& src1,
                              const LogicVRegister& src2) {
  int p_reg_bits_per_lane =
      LaneSizeInBitsFromFormat(vform) / kZRegBitsPerPRegBit;
  for (int lane = 0; lane < LaneCountFromFormat(vform); lane++) {
    uint64_t lane_value = pg.GetBit(lane * p_reg_bits_per_lane)
                              ? src1.Uint(vform, lane)
                              : src2.Uint(vform, lane);
    dst.SetUint(vform, lane, lane_value);
  }
  return dst;
}


LogicPRegister Simulator::sel(LogicPRegister dst,
                              const LogicPRegister& pg,
                              const LogicPRegister& src1,
                              const LogicPRegister& src2) {
  for (int i = 0; i < dst.GetChunkCount(); i++) {
    LogicPRegister::ChunkType mask = pg.GetChunk(i);
    LogicPRegister::ChunkType result =
        (mask & src1.GetChunk(i)) | (~mask & src2.GetChunk(i));
    dst.SetChunk(i, result);
  }
  return dst;
}


LogicVRegister Simulator::sli(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& src,
                              int shift) {
  dst.ClearForWrite(vform);
  int lane_count = LaneCountFromFormat(vform);
  for (int i = 0; i < lane_count; i++) {
    uint64_t src_lane = src.Uint(vform, i);
    uint64_t dst_lane = dst.Uint(vform, i);
    uint64_t shifted = src_lane << shift;
    uint64_t mask = MaxUintFromFormat(vform) << shift;
    dst.SetUint(vform, i, (dst_lane & ~mask) | shifted);
  }
  return dst;
}


LogicVRegister Simulator::sqshl(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src,
                                int shift) {
  VIXL_ASSERT(shift >= 0);
  SimVRegister temp;
  LogicVRegister shiftreg = dup_immediate(vform, temp, shift);
  return sshl(vform, dst, src, shiftreg).SignedSaturate(vform);
}


LogicVRegister Simulator::uqshl(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src,
                                int shift) {
  VIXL_ASSERT(shift >= 0);
  SimVRegister temp;
  LogicVRegister shiftreg = dup_immediate(vform, temp, shift);
  return ushl(vform, dst, src, shiftreg).UnsignedSaturate(vform);
}


LogicVRegister Simulator::sqshlu(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src,
                                 int shift) {
  VIXL_ASSERT(shift >= 0);
  SimVRegister temp;
  LogicVRegister shiftreg = dup_immediate(vform, temp, shift);
  return sshl(vform, dst, src, shiftreg).UnsignedSaturate(vform);
}


LogicVRegister Simulator::sri(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& src,
                              int shift) {
  dst.ClearForWrite(vform);
  int lane_count = LaneCountFromFormat(vform);
  VIXL_ASSERT((shift > 0) &&
              (shift <= static_cast<int>(LaneSizeInBitsFromFormat(vform))));
  for (int i = 0; i < lane_count; i++) {
    uint64_t src_lane = src.Uint(vform, i);
    uint64_t dst_lane = dst.Uint(vform, i);
    uint64_t shifted;
    uint64_t mask;
    if (shift == 64) {
      shifted = 0;
      mask = 0;
    } else {
      shifted = src_lane >> shift;
      mask = MaxUintFromFormat(vform) >> shift;
    }
    dst.SetUint(vform, i, (dst_lane & ~mask) | shifted);
  }
  return dst;
}


LogicVRegister Simulator::ushr(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src,
                               int shift) {
  VIXL_ASSERT(shift >= 0);
  SimVRegister temp;
  LogicVRegister shiftreg = dup_immediate(vform, temp, -shift);
  return ushl(vform, dst, src, shiftreg);
}


LogicVRegister Simulator::sshr(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src,
                               int shift) {
  VIXL_ASSERT(shift >= 0);
  SimVRegister temp;
  LogicVRegister shiftreg = dup_immediate(vform, temp, -shift);
  return sshl(vform, dst, src, shiftreg);
}


LogicVRegister Simulator::ssra(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src,
                               int shift) {
  SimVRegister temp;
  LogicVRegister shifted_reg = sshr(vform, temp, src, shift);
  return add(vform, dst, dst, shifted_reg);
}


LogicVRegister Simulator::usra(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src,
                               int shift) {
  SimVRegister temp;
  LogicVRegister shifted_reg = ushr(vform, temp, src, shift);
  return add(vform, dst, dst, shifted_reg);
}


LogicVRegister Simulator::srsra(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src,
                                int shift) {
  SimVRegister temp;
  LogicVRegister shifted_reg = sshr(vform, temp, src, shift).Round(vform);
  return add(vform, dst, dst, shifted_reg);
}


LogicVRegister Simulator::ursra(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src,
                                int shift) {
  SimVRegister temp;
  LogicVRegister shifted_reg = ushr(vform, temp, src, shift).Round(vform);
  return add(vform, dst, dst, shifted_reg);
}


LogicVRegister Simulator::cls(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& src) {
  int lane_size_in_bits = LaneSizeInBitsFromFormat(vform);
  int lane_count = LaneCountFromFormat(vform);

  // Ensure that we can store one result per lane.
  int result[kZRegMaxSizeInBytes];

  for (int i = 0; i < lane_count; i++) {
    result[i] = CountLeadingSignBits(src.Int(vform, i), lane_size_in_bits);
  }

  dst.ClearForWrite(vform);
  for (int i = 0; i < lane_count; ++i) {
    dst.SetUint(vform, i, result[i]);
  }
  return dst;
}


LogicVRegister Simulator::clz(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& src) {
  int lane_size_in_bits = LaneSizeInBitsFromFormat(vform);
  int lane_count = LaneCountFromFormat(vform);

  // Ensure that we can store one result per lane.
  int result[kZRegMaxSizeInBytes];

  for (int i = 0; i < lane_count; i++) {
    result[i] = CountLeadingZeros(src.Uint(vform, i), lane_size_in_bits);
  }

  dst.ClearForWrite(vform);
  for (int i = 0; i < lane_count; ++i) {
    dst.SetUint(vform, i, result[i]);
  }
  return dst;
}


LogicVRegister Simulator::cnot(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src) {
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    uint64_t value = (src.Uint(vform, i) == 0) ? 1 : 0;
    dst.SetUint(vform, i, value);
  }
  return dst;
}


LogicVRegister Simulator::cnt(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& src) {
  int lane_size_in_bits = LaneSizeInBitsFromFormat(vform);
  int lane_count = LaneCountFromFormat(vform);

  // Ensure that we can store one result per lane.
  int result[kZRegMaxSizeInBytes];

  for (int i = 0; i < lane_count; i++) {
    result[i] = CountSetBits(src.Uint(vform, i), lane_size_in_bits);
  }

  dst.ClearForWrite(vform);
  for (int i = 0; i < lane_count; ++i) {
    dst.SetUint(vform, i, result[i]);
  }
  return dst;
}

static int64_t CalculateSignedShiftDistance(int64_t shift_val,
                                            int esize,
                                            bool shift_in_ls_byte) {
  if (shift_in_ls_byte) {
    // Neon uses the least-significant byte of the lane as the shift distance.
    shift_val = ExtractSignedBitfield64(7, 0, shift_val);
  } else {
    // SVE uses a saturated shift distance in the range
    //  -(esize + 1) ... (esize + 1).
    if (shift_val > (esize + 1)) shift_val = esize + 1;
    if (shift_val < -(esize + 1)) shift_val = -(esize + 1);
  }
  return shift_val;
}

LogicVRegister Simulator::sshl(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2,
                               bool shift_in_ls_byte) {
  dst.ClearForWrite(vform);
  int esize = LaneSizeInBitsFromFormat(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    int64_t shift_val = CalculateSignedShiftDistance(src2.Int(vform, i),
                                                     esize,
                                                     shift_in_ls_byte);

    int64_t lj_src_val = src1.IntLeftJustified(vform, i);

    // Set signed saturation state.
    if ((shift_val > CountLeadingSignBits(lj_src_val)) && (lj_src_val != 0)) {
      dst.SetSignedSat(i, lj_src_val >= 0);
    }

    // Set unsigned saturation state.
    if (lj_src_val < 0) {
      dst.SetUnsignedSat(i, false);
    } else if ((shift_val > CountLeadingZeros(lj_src_val)) &&
               (lj_src_val != 0)) {
      dst.SetUnsignedSat(i, true);
    }

    int64_t src_val = src1.Int(vform, i);
    bool src_is_negative = src_val < 0;
    if (shift_val > 63) {
      dst.SetInt(vform, i, 0);
    } else if (shift_val < -63) {
      dst.SetRounding(i, src_is_negative);
      dst.SetInt(vform, i, src_is_negative ? -1 : 0);
    } else {
      // Use unsigned types for shifts, as behaviour is undefined for signed
      // lhs.
      uint64_t usrc_val = static_cast<uint64_t>(src_val);

      if (shift_val < 0) {
        // Convert to right shift.
        shift_val = -shift_val;

        // Set rounding state by testing most-significant bit shifted out.
        // Rounding only needed on right shifts.
        if (((usrc_val >> (shift_val - 1)) & 1) == 1) {
          dst.SetRounding(i, true);
        }

        usrc_val >>= shift_val;

        if (src_is_negative) {
          // Simulate sign-extension.
          usrc_val |= (~UINT64_C(0) << (64 - shift_val));
        }
      } else {
        usrc_val <<= shift_val;
      }
      dst.SetUint(vform, i, usrc_val);
    }
  }
  return dst;
}


LogicVRegister Simulator::ushl(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2,
                               bool shift_in_ls_byte) {
  dst.ClearForWrite(vform);
  int esize = LaneSizeInBitsFromFormat(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    int64_t shift_val = CalculateSignedShiftDistance(src2.Int(vform, i),
                                                     esize,
                                                     shift_in_ls_byte);

    uint64_t lj_src_val = src1.UintLeftJustified(vform, i);

    // Set saturation state.
    if ((shift_val > CountLeadingZeros(lj_src_val)) && (lj_src_val != 0)) {
      dst.SetUnsignedSat(i, true);
    }

    uint64_t src_val = src1.Uint(vform, i);
    if ((shift_val > 63) || (shift_val < -64)) {
      dst.SetUint(vform, i, 0);
    } else {
      if (shift_val < 0) {
        // Set rounding state. Rounding only needed on right shifts.
        if (((src_val >> (-shift_val - 1)) & 1) == 1) {
          dst.SetRounding(i, true);
        }

        if (shift_val == -64) {
          src_val = 0;
        } else {
          src_val >>= -shift_val;
        }
      } else {
        src_val <<= shift_val;
      }
      dst.SetUint(vform, i, src_val);
    }
  }
  return dst;
}

LogicVRegister Simulator::sshr(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2) {
  SimVRegister temp;
  // Saturate to sidestep the min-int problem.
  neg(vform, temp, src2).SignedSaturate(vform);
  sshl(vform, dst, src1, temp, false);
  return dst;
}

LogicVRegister Simulator::ushr(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2) {
  SimVRegister temp;
  // Saturate to sidestep the min-int problem.
  neg(vform, temp, src2).SignedSaturate(vform);
  ushl(vform, dst, src1, temp, false);
  return dst;
}

LogicVRegister Simulator::neg(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& src) {
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    // Test for signed saturation.
    int64_t sa = src.Int(vform, i);
    if (sa == MinIntFromFormat(vform)) {
      dst.SetSignedSat(i, true);
    }
    dst.SetInt(vform, i, (sa == INT64_MIN) ? sa : -sa);
  }
  return dst;
}


LogicVRegister Simulator::suqadd(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    int64_t sa = src1.IntLeftJustified(vform, i);
    uint64_t ub = src2.UintLeftJustified(vform, i);
    uint64_t ur = sa + ub;

    int64_t sr;
    memcpy(&sr, &ur, sizeof(sr));
    if (sr < sa) {  // Test for signed positive saturation.
      dst.SetInt(vform, i, MaxIntFromFormat(vform));
    } else {
      dst.SetUint(vform, i, src1.Int(vform, i) + src2.Uint(vform, i));
    }
  }
  return dst;
}


LogicVRegister Simulator::usqadd(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    uint64_t ua = src1.UintLeftJustified(vform, i);
    int64_t sb = src2.IntLeftJustified(vform, i);
    uint64_t ur = ua + sb;

    if ((sb > 0) && (ur <= ua)) {
      dst.SetUint(vform, i, MaxUintFromFormat(vform));  // Positive saturation.
    } else if ((sb < 0) && (ur >= ua)) {
      dst.SetUint(vform, i, 0);  // Negative saturation.
    } else {
      dst.SetUint(vform, i, src1.Uint(vform, i) + src2.Int(vform, i));
    }
  }
  return dst;
}


LogicVRegister Simulator::abs(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& src) {
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    // Test for signed saturation.
    int64_t sa = src.Int(vform, i);
    if (sa == MinIntFromFormat(vform)) {
      dst.SetSignedSat(i, true);
    }
    if (sa < 0) {
      dst.SetInt(vform, i, (sa == INT64_MIN) ? sa : -sa);
    } else {
      dst.SetInt(vform, i, sa);
    }
  }
  return dst;
}


LogicVRegister Simulator::andv(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicPRegister& pg,
                               const LogicVRegister& src) {
  VIXL_ASSERT(IsSVEFormat(vform));
  uint64_t result = GetUintMask(LaneSizeInBitsFromFormat(vform));
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    if (!pg.IsActive(vform, i)) continue;

    result &= src.Uint(vform, i);
  }
  VectorFormat vform_dst =
      ScalarFormatFromLaneSize(LaneSizeInBitsFromFormat(vform));
  dst.ClearForWrite(vform_dst);
  dst.SetUint(vform_dst, 0, result);
  return dst;
}


LogicVRegister Simulator::eorv(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicPRegister& pg,
                               const LogicVRegister& src) {
  VIXL_ASSERT(IsSVEFormat(vform));
  uint64_t result = 0;
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    if (!pg.IsActive(vform, i)) continue;

    result ^= src.Uint(vform, i);
  }
  VectorFormat vform_dst =
      ScalarFormatFromLaneSize(LaneSizeInBitsFromFormat(vform));
  dst.ClearForWrite(vform_dst);
  dst.SetUint(vform_dst, 0, result);
  return dst;
}


LogicVRegister Simulator::orv(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicPRegister& pg,
                              const LogicVRegister& src) {
  VIXL_ASSERT(IsSVEFormat(vform));
  uint64_t result = 0;
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    if (!pg.IsActive(vform, i)) continue;

    result |= src.Uint(vform, i);
  }
  VectorFormat vform_dst =
      ScalarFormatFromLaneSize(LaneSizeInBitsFromFormat(vform));
  dst.ClearForWrite(vform_dst);
  dst.SetUint(vform_dst, 0, result);
  return dst;
}


LogicVRegister Simulator::saddv(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicPRegister& pg,
                                const LogicVRegister& src) {
  VIXL_ASSERT(IsSVEFormat(vform));
  VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) <= kSRegSize);
  int64_t result = 0;
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    if (!pg.IsActive(vform, i)) continue;

    // The destination register always has D-lane sizes and the source register
    // always has S-lanes or smaller, so signed integer overflow -- undefined
    // behaviour -- can't occur.
    result += src.Int(vform, i);
  }

  dst.ClearForWrite(kFormatD);
  dst.SetInt(kFormatD, 0, result);
  return dst;
}


LogicVRegister Simulator::uaddv(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicPRegister& pg,
                                const LogicVRegister& src) {
  VIXL_ASSERT(IsSVEFormat(vform));
  uint64_t result = 0;
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    if (!pg.IsActive(vform, i)) continue;

    result += src.Uint(vform, i);
  }

  dst.ClearForWrite(kFormatD);
  dst.SetUint(kFormatD, 0, result);
  return dst;
}


LogicVRegister Simulator::extractnarrow(VectorFormat dstform,
                                        LogicVRegister dst,
                                        bool dst_is_signed,
                                        const LogicVRegister& src,
                                        bool src_is_signed) {
  bool upperhalf = false;
  VectorFormat srcform = dstform;
  if ((dstform == kFormat16B) || (dstform == kFormat8H) ||
      (dstform == kFormat4S)) {
    upperhalf = true;
    srcform = VectorFormatHalfLanes(srcform);
  }
  srcform = VectorFormatDoubleWidth(srcform);

  LogicVRegister src_copy = src;

  int offset;
  if (upperhalf) {
    offset = LaneCountFromFormat(dstform) / 2;
  } else {
    offset = 0;
  }

  for (int i = 0; i < LaneCountFromFormat(srcform); i++) {
    int64_t ssrc = src_copy.Int(srcform, i);
    uint64_t usrc = src_copy.Uint(srcform, i);

    // Test for signed saturation
    if (ssrc > MaxIntFromFormat(dstform)) {
      dst.SetSignedSat(offset + i, true);
    } else if (ssrc < MinIntFromFormat(dstform)) {
      dst.SetSignedSat(offset + i, false);
    }

    // Test for unsigned saturation
    if (src_is_signed) {
      if (ssrc > static_cast<int64_t>(MaxUintFromFormat(dstform))) {
        dst.SetUnsignedSat(offset + i, true);
      } else if (ssrc < 0) {
        dst.SetUnsignedSat(offset + i, false);
      }
    } else {
      if (usrc > MaxUintFromFormat(dstform)) {
        dst.SetUnsignedSat(offset + i, true);
      }
    }

    int64_t result;
    if (src_is_signed) {
      result = ssrc & MaxUintFromFormat(dstform);
    } else {
      result = usrc & MaxUintFromFormat(dstform);
    }

    if (dst_is_signed) {
      dst.SetInt(dstform, offset + i, result);
    } else {
      dst.SetUint(dstform, offset + i, result);
    }
  }

  if (!upperhalf) {
    dst.ClearForWrite(dstform);
  }
  return dst;
}


LogicVRegister Simulator::xtn(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& src) {
  return extractnarrow(vform, dst, true, src, true);
}


LogicVRegister Simulator::sqxtn(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src) {
  return extractnarrow(vform, dst, true, src, true).SignedSaturate(vform);
}


LogicVRegister Simulator::sqxtun(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src) {
  return extractnarrow(vform, dst, false, src, true).UnsignedSaturate(vform);
}


LogicVRegister Simulator::uqxtn(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src) {
  return extractnarrow(vform, dst, false, src, false).UnsignedSaturate(vform);
}


LogicVRegister Simulator::absdiff(VectorFormat vform,
                                  LogicVRegister dst,
                                  const LogicVRegister& src1,
                                  const LogicVRegister& src2,
                                  bool is_signed) {
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    bool src1_gt_src2 = is_signed ? (src1.Int(vform, i) > src2.Int(vform, i))
                                  : (src1.Uint(vform, i) > src2.Uint(vform, i));
    // Always calculate the answer using unsigned arithmetic, to avoid
    // implementation-defined signed overflow.
    if (src1_gt_src2) {
      dst.SetUint(vform, i, src1.Uint(vform, i) - src2.Uint(vform, i));
    } else {
      dst.SetUint(vform, i, src2.Uint(vform, i) - src1.Uint(vform, i));
    }
  }
  return dst;
}


LogicVRegister Simulator::saba(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2) {
  SimVRegister temp;
  dst.ClearForWrite(vform);
  absdiff(vform, temp, src1, src2, true);
  add(vform, dst, dst, temp);
  return dst;
}


LogicVRegister Simulator::uaba(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2) {
  SimVRegister temp;
  dst.ClearForWrite(vform);
  absdiff(vform, temp, src1, src2, false);
  add(vform, dst, dst, temp);
  return dst;
}


LogicVRegister Simulator::not_(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src) {
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    dst.SetUint(vform, i, ~src.Uint(vform, i));
  }
  return dst;
}


LogicVRegister Simulator::rbit(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src) {
  uint64_t result[kZRegMaxSizeInBytes];
  int lane_count = LaneCountFromFormat(vform);
  int lane_size_in_bits = LaneSizeInBitsFromFormat(vform);
  uint64_t reversed_value;
  uint64_t value;
  for (int i = 0; i < lane_count; i++) {
    value = src.Uint(vform, i);
    reversed_value = 0;
    for (int j = 0; j < lane_size_in_bits; j++) {
      reversed_value = (reversed_value << 1) | (value & 1);
      value >>= 1;
    }
    result[i] = reversed_value;
  }

  dst.ClearForWrite(vform);
  for (int i = 0; i < lane_count; ++i) {
    dst.SetUint(vform, i, result[i]);
  }
  return dst;
}


LogicVRegister Simulator::rev(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& src) {
  VIXL_ASSERT(IsSVEFormat(vform));
  int lane_count = LaneCountFromFormat(vform);
  for (int i = 0; i < lane_count / 2; i++) {
    uint64_t t = src.Uint(vform, i);
    dst.SetUint(vform, i, src.Uint(vform, lane_count - i - 1));
    dst.SetUint(vform, lane_count - i - 1, t);
  }
  return dst;
}


LogicVRegister Simulator::rev_byte(VectorFormat vform,
                                   LogicVRegister dst,
                                   const LogicVRegister& src,
                                   int rev_size) {
  uint64_t result[kZRegMaxSizeInBytes] = {};
  int lane_count = LaneCountFromFormat(vform);
  int lane_size = LaneSizeInBytesFromFormat(vform);
  int lanes_per_loop = rev_size / lane_size;
  for (int i = 0; i < lane_count; i += lanes_per_loop) {
    for (int j = 0; j < lanes_per_loop; j++) {
      result[i + lanes_per_loop - 1 - j] = src.Uint(vform, i + j);
    }
  }
  dst.ClearForWrite(vform);
  for (int i = 0; i < lane_count; ++i) {
    dst.SetUint(vform, i, result[i]);
  }
  return dst;
}


LogicVRegister Simulator::rev16(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src) {
  return rev_byte(vform, dst, src, 2);
}


LogicVRegister Simulator::rev32(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src) {
  return rev_byte(vform, dst, src, 4);
}


LogicVRegister Simulator::rev64(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src) {
  return rev_byte(vform, dst, src, 8);
}

LogicVRegister Simulator::addlp(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src,
                                bool is_signed,
                                bool do_accumulate) {
  VectorFormat vformsrc = VectorFormatHalfWidthDoubleLanes(vform);
  VIXL_ASSERT(LaneSizeInBitsFromFormat(vformsrc) <= kSRegSize);

  uint64_t result[kZRegMaxSizeInBytes];
  int lane_count = LaneCountFromFormat(vform);
  for (int i = 0; i < lane_count; i++) {
    if (is_signed) {
      result[i] = static_cast<uint64_t>(src.Int(vformsrc, 2 * i) +
                                        src.Int(vformsrc, 2 * i + 1));
    } else {
      result[i] = src.Uint(vformsrc, 2 * i) + src.Uint(vformsrc, 2 * i + 1);
    }
  }

  dst.ClearForWrite(vform);
  for (int i = 0; i < lane_count; ++i) {
    if (do_accumulate) {
      result[i] += dst.Uint(vform, i);
    }
    dst.SetUint(vform, i, result[i]);
  }

  return dst;
}


LogicVRegister Simulator::saddlp(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src) {
  return addlp(vform, dst, src, true, false);
}


LogicVRegister Simulator::uaddlp(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src) {
  return addlp(vform, dst, src, false, false);
}


LogicVRegister Simulator::sadalp(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src) {
  return addlp(vform, dst, src, true, true);
}


LogicVRegister Simulator::uadalp(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src) {
  return addlp(vform, dst, src, false, true);
}

LogicVRegister Simulator::ror(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& src,
                              int rotation) {
  int width = LaneSizeInBitsFromFormat(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    uint64_t value = src.Uint(vform, i);
    dst.SetUint(vform, i, RotateRight(value, rotation, width));
  }
  return dst;
}

LogicVRegister Simulator::ext(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& src1,
                              const LogicVRegister& src2,
                              int index) {
  uint8_t result[kZRegMaxSizeInBytes] = {};
  int lane_count = LaneCountFromFormat(vform);
  for (int i = 0; i < lane_count - index; ++i) {
    result[i] = src1.Uint(vform, i + index);
  }
  for (int i = 0; i < index; ++i) {
    result[lane_count - index + i] = src2.Uint(vform, i);
  }
  dst.ClearForWrite(vform);
  for (int i = 0; i < lane_count; ++i) {
    dst.SetUint(vform, i, result[i]);
  }
  return dst;
}

LogicVRegister Simulator::rotate_elements_right(VectorFormat vform,
                                                LogicVRegister dst,
                                                const LogicVRegister& src,
                                                int index) {
  if (index < 0) index += LaneCountFromFormat(vform);
  VIXL_ASSERT((index >= 0) && (index < LaneCountFromFormat(vform)));
  index *= LaneSizeInBytesFromFormat(vform);
  return ext(kFormatVnB, dst, src, src, index);
}


template <typename T>
LogicVRegister Simulator::fadda(VectorFormat vform,
                                LogicVRegister acc,
                                const LogicPRegister& pg,
                                const LogicVRegister& src) {
  T result = acc.Float<T>(0);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    if (!pg.IsActive(vform, i)) continue;

    result = FPAdd(result, src.Float<T>(i));
  }
  VectorFormat vform_dst =
      ScalarFormatFromLaneSize(LaneSizeInBitsFromFormat(vform));
  acc.ClearForWrite(vform_dst);
  acc.SetFloat(0, result);
  return acc;
}

LogicVRegister Simulator::fadda(VectorFormat vform,
                                LogicVRegister acc,
                                const LogicPRegister& pg,
                                const LogicVRegister& src) {
  switch (LaneSizeInBitsFromFormat(vform)) {
    case kHRegSize:
      fadda<SimFloat16>(vform, acc, pg, src);
      break;
    case kSRegSize:
      fadda<float>(vform, acc, pg, src);
      break;
    case kDRegSize:
      fadda<double>(vform, acc, pg, src);
      break;
    default:
      VIXL_UNREACHABLE();
  }
  return acc;
}

template <typename T>
LogicVRegister Simulator::fcadd(VectorFormat vform,
                                LogicVRegister dst,          // d
                                const LogicVRegister& src1,  // n
                                const LogicVRegister& src2,  // m
                                int rot) {
  int elements = LaneCountFromFormat(vform);

  T element1, element3;
  rot = (rot == 1) ? 270 : 90;

  // Loop example:
  // 2S --> (2/2 = 1 - 1 = 0) --> 1 x Complex Number (2x components: r+i)
  // 4S --> (4/2 = 2) - 1 = 1) --> 2 x Complex Number (2x2 components: r+i)

  for (int e = 0; e <= (elements / 2) - 1; e++) {
    switch (rot) {
      case 90:
        element1 = FPNeg(src2.Float<T>(e * 2 + 1));
        element3 = src2.Float<T>(e * 2);
        break;
      case 270:
        element1 = src2.Float<T>(e * 2 + 1);
        element3 = FPNeg(src2.Float<T>(e * 2));
        break;
      default:
        VIXL_UNREACHABLE();
        return dst;  // prevents "element(n) may be unintialized" errors
    }
    dst.ClearForWrite(vform);
    dst.SetFloat<T>(e * 2, FPAdd(src1.Float<T>(e * 2), element1));
    dst.SetFloat<T>(e * 2 + 1, FPAdd(src1.Float<T>(e * 2 + 1), element3));
  }
  return dst;
}


LogicVRegister Simulator::fcadd(VectorFormat vform,
                                LogicVRegister dst,          // d
                                const LogicVRegister& src1,  // n
                                const LogicVRegister& src2,  // m
                                int rot) {
  if (LaneSizeInBitsFromFormat(vform) == kHRegSize) {
    fcadd<SimFloat16>(vform, dst, src1, src2, rot);
  } else if (LaneSizeInBitsFromFormat(vform) == kSRegSize) {
    fcadd<float>(vform, dst, src1, src2, rot);
  } else {
    VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kDRegSize);
    fcadd<double>(vform, dst, src1, src2, rot);
  }
  return dst;
}

template <typename T>
LogicVRegister Simulator::fcmla(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2,
                                const LogicVRegister& acc,
                                int index,
                                int rot) {
  int elements = LaneCountFromFormat(vform);

  T element1, element2, element3, element4;
  rot *= 90;

  // Loop example:
  // 2S --> (2/2 = 1 - 1 = 0) --> 1 x Complex Number (2x components: r+i)
  // 4S --> (4/2 = 2) - 1 = 1) --> 2 x Complex Number (2x2 components: r+i)

  for (int e = 0; e <= (elements / 2) - 1; e++) {
    // Index == -1 indicates a vector/vector rather than vector/indexed-element
    // operation.
    int f = (index < 0) ? e : index;

    switch (rot) {
      case 0:
        element1 = src2.Float<T>(f * 2);
        element2 = src1.Float<T>(e * 2);
        element3 = src2.Float<T>(f * 2 + 1);
        element4 = src1.Float<T>(e * 2);
        break;
      case 90:
        element1 = FPNeg(src2.Float<T>(f * 2 + 1));
        element2 = src1.Float<T>(e * 2 + 1);
        element3 = src2.Float<T>(f * 2);
        element4 = src1.Float<T>(e * 2 + 1);
        break;
      case 180:
        element1 = FPNeg(src2.Float<T>(f * 2));
        element2 = src1.Float<T>(e * 2);
        element3 = FPNeg(src2.Float<T>(f * 2 + 1));
        element4 = src1.Float<T>(e * 2);
        break;
      case 270:
        element1 = src2.Float<T>(f * 2 + 1);
        element2 = src1.Float<T>(e * 2 + 1);
        element3 = FPNeg(src2.Float<T>(f * 2));
        element4 = src1.Float<T>(e * 2 + 1);
        break;
      default:
        VIXL_UNREACHABLE();
        return dst;  // prevents "element(n) may be unintialized" errors
    }
    dst.ClearForWrite(vform);
    dst.SetFloat<T>(vform,
                    e * 2,
                    FPMulAdd(acc.Float<T>(e * 2), element2, element1));
    dst.SetFloat<T>(vform,
                    e * 2 + 1,
                    FPMulAdd(acc.Float<T>(e * 2 + 1), element4, element3));
  }
  return dst;
}

LogicVRegister Simulator::fcmla(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2,
                                const LogicVRegister& acc,
                                int rot) {
  if (LaneSizeInBitsFromFormat(vform) == kHRegSize) {
    fcmla<SimFloat16>(vform, dst, src1, src2, acc, -1, rot);
  } else if (LaneSizeInBitsFromFormat(vform) == kSRegSize) {
    fcmla<float>(vform, dst, src1, src2, acc, -1, rot);
  } else {
    fcmla<double>(vform, dst, src1, src2, acc, -1, rot);
  }
  return dst;
}


LogicVRegister Simulator::fcmla(VectorFormat vform,
                                LogicVRegister dst,          // d
                                const LogicVRegister& src1,  // n
                                const LogicVRegister& src2,  // m
                                int index,
                                int rot) {
  if (LaneSizeInBitsFromFormat(vform) == kHRegSize) {
    VIXL_UNIMPLEMENTED();
  } else if (LaneSizeInBitsFromFormat(vform) == kSRegSize) {
    fcmla<float>(vform, dst, src1, src2, dst, index, rot);
  } else {
    fcmla<double>(vform, dst, src1, src2, dst, index, rot);
  }
  return dst;
}

LogicVRegister Simulator::cadd(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2,
                               int rot,
                               bool saturate) {
  SimVRegister src1_r, src1_i;
  SimVRegister src2_r, src2_i;
  SimVRegister zero;
  zero.Clear();
  uzp1(vform, src1_r, src1, zero);
  uzp2(vform, src1_i, src1, zero);
  uzp1(vform, src2_r, src2, zero);
  uzp2(vform, src2_i, src2, zero);

  if (rot == 90) {
    if (saturate) {
      sub(vform, src1_r, src1_r, src2_i).SignedSaturate(vform);
      add(vform, src1_i, src1_i, src2_r).SignedSaturate(vform);
    } else {
      sub(vform, src1_r, src1_r, src2_i);
      add(vform, src1_i, src1_i, src2_r);
    }
  } else {
    VIXL_ASSERT(rot == 270);
    if (saturate) {
      add(vform, src1_r, src1_r, src2_i).SignedSaturate(vform);
      sub(vform, src1_i, src1_i, src2_r).SignedSaturate(vform);
    } else {
      add(vform, src1_r, src1_r, src2_i);
      sub(vform, src1_i, src1_i, src2_r);
    }
  }

  zip1(vform, dst, src1_r, src1_i);
  return dst;
}

LogicVRegister Simulator::cmla(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& srca,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2,
                               int rot) {
  SimVRegister src1_a;
  SimVRegister src2_a, src2_b;
  SimVRegister srca_i, srca_r;
  SimVRegister zero, temp;
  zero.Clear();

  if ((rot == 0) || (rot == 180)) {
    uzp1(vform, src1_a, src1, zero);
    uzp1(vform, src2_a, src2, zero);
    uzp2(vform, src2_b, src2, zero);
  } else {
    uzp2(vform, src1_a, src1, zero);
    uzp2(vform, src2_a, src2, zero);
    uzp1(vform, src2_b, src2, zero);
  }

  uzp1(vform, srca_r, srca, zero);
  uzp2(vform, srca_i, srca, zero);

  bool sub_r = (rot == 90) || (rot == 180);
  bool sub_i = (rot == 180) || (rot == 270);

  mul(vform, temp, src1_a, src2_a);
  if (sub_r) {
    sub(vform, srca_r, srca_r, temp);
  } else {
    add(vform, srca_r, srca_r, temp);
  }

  mul(vform, temp, src1_a, src2_b);
  if (sub_i) {
    sub(vform, srca_i, srca_i, temp);
  } else {
    add(vform, srca_i, srca_i, temp);
  }

  zip1(vform, dst, srca_r, srca_i);
  return dst;
}

LogicVRegister Simulator::cmla(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& srca,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2,
                               int index,
                               int rot) {
  SimVRegister temp;
  dup_elements_to_segments(VectorFormatDoubleWidth(vform), temp, src2, index);
  return cmla(vform, dst, srca, src1, temp, rot);
}

LogicVRegister Simulator::bgrp(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2,
                               bool do_bext) {
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    uint64_t value = src1.Uint(vform, i);
    uint64_t mask = src2.Uint(vform, i);
    int high_pos = 0;
    int low_pos = 0;
    uint64_t result_high = 0;
    uint64_t result_low = 0;
    for (unsigned j = 0; j < LaneSizeInBitsFromFormat(vform); j++) {
      if ((mask & 1) == 0) {
        result_high |= (value & 1) << high_pos;
        high_pos++;
      } else {
        result_low |= (value & 1) << low_pos;
        low_pos++;
      }
      mask >>= 1;
      value >>= 1;
    }

    if (!do_bext) {
      result_low |= result_high << low_pos;
    }

    dst.SetUint(vform, i, result_low);
  }
  return dst;
}

LogicVRegister Simulator::bdep(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2) {
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    uint64_t value = src1.Uint(vform, i);
    uint64_t mask = src2.Uint(vform, i);
    uint64_t result = 0;
    for (unsigned j = 0; j < LaneSizeInBitsFromFormat(vform); j++) {
      if ((mask & 1) == 1) {
        result |= (value & 1) << j;
        value >>= 1;
      }
      mask >>= 1;
    }
    dst.SetUint(vform, i, result);
  }
  return dst;
}

LogicVRegister Simulator::histogram(VectorFormat vform,
                                    LogicVRegister dst,
                                    const LogicPRegister& pg,
                                    const LogicVRegister& src1,
                                    const LogicVRegister& src2,
                                    bool do_segmented) {
  int elements_per_segment = kQRegSize / LaneSizeInBitsFromFormat(vform);
  uint64_t result[kZRegMaxSizeInBytes];

  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    uint64_t count = 0;
    uint64_t value = src1.Uint(vform, i);

    int segment = do_segmented ? (i / elements_per_segment) : 0;
    int segment_offset = segment * elements_per_segment;
    int hist_limit = do_segmented ? elements_per_segment : (i + 1);
    for (int j = 0; j < hist_limit; j++) {
      if (pg.IsActive(vform, j) &&
          (value == src2.Uint(vform, j + segment_offset))) {
        count++;
      }
    }
    result[i] = count;
  }
  dst.SetUintArray(vform, result);
  return dst;
}

LogicVRegister Simulator::dup_element(VectorFormat vform,
                                      LogicVRegister dst,
                                      const LogicVRegister& src,
                                      int src_index) {
  if ((vform == kFormatVnQ) || (vform == kFormatVnO)) {
    // When duplicating an element larger than 64 bits, split the element into
    // 64-bit parts, and duplicate the parts across the destination.
    uint64_t d[4];
    int count = (vform == kFormatVnQ) ? 2 : 4;
    for (int i = 0; i < count; i++) {
      d[i] = src.Uint(kFormatVnD, (src_index * count) + i);
    }
    dst.Clear();
    for (int i = 0; i < LaneCountFromFormat(vform) * count; i++) {
      dst.SetUint(kFormatVnD, i, d[i % count]);
    }
  } else {
    int lane_count = LaneCountFromFormat(vform);
    uint64_t value = src.Uint(vform, src_index);
    dst.ClearForWrite(vform);
    for (int i = 0; i < lane_count; ++i) {
      dst.SetUint(vform, i, value);
    }
  }
  return dst;
}

LogicVRegister Simulator::dup_elements_to_segments(VectorFormat vform,
                                                   LogicVRegister dst,
                                                   const LogicVRegister& src,
                                                   int src_index) {
  // In SVE, a segment is a 128-bit portion of a vector, like a Q register,
  // whereas in NEON, the size of segment is equal to the size of register
  // itself.
  int segment_size = std::min(kQRegSize, RegisterSizeInBitsFromFormat(vform));
  VIXL_ASSERT(IsMultiple(segment_size, LaneSizeInBitsFromFormat(vform)));
  int lanes_per_segment = segment_size / LaneSizeInBitsFromFormat(vform);

  VIXL_ASSERT(src_index >= 0);
  VIXL_ASSERT(src_index < lanes_per_segment);

  dst.ClearForWrite(vform);
  for (int j = 0; j < LaneCountFromFormat(vform); j += lanes_per_segment) {
    uint64_t value = src.Uint(vform, j + src_index);
    for (int i = 0; i < lanes_per_segment; i++) {
      dst.SetUint(vform, j + i, value);
    }
  }
  return dst;
}

LogicVRegister Simulator::dup_elements_to_segments(
    VectorFormat vform,
    LogicVRegister dst,
    const std::pair<int, int>& src_and_index) {
  return dup_elements_to_segments(vform,
                                  dst,
                                  ReadVRegister(src_and_index.first),
                                  src_and_index.second);
}

LogicVRegister Simulator::dup_immediate(VectorFormat vform,
                                        LogicVRegister dst,
                                        uint64_t imm) {
  int lane_count = LaneCountFromFormat(vform);
  uint64_t value = imm & MaxUintFromFormat(vform);
  dst.ClearForWrite(vform);
  for (int i = 0; i < lane_count; ++i) {
    dst.SetUint(vform, i, value);
  }
  return dst;
}


LogicVRegister Simulator::ins_element(VectorFormat vform,
                                      LogicVRegister dst,
                                      int dst_index,
                                      const LogicVRegister& src,
                                      int src_index) {
  dst.SetUint(vform, dst_index, src.Uint(vform, src_index));
  return dst;
}


LogicVRegister Simulator::ins_immediate(VectorFormat vform,
                                        LogicVRegister dst,
                                        int dst_index,
                                        uint64_t imm) {
  uint64_t value = imm & MaxUintFromFormat(vform);
  dst.SetUint(vform, dst_index, value);
  return dst;
}


LogicVRegister Simulator::index(VectorFormat vform,
                                LogicVRegister dst,
                                uint64_t start,
                                uint64_t step) {
  VIXL_ASSERT(IsSVEFormat(vform));
  uint64_t value = start;
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    dst.SetUint(vform, i, value);
    value += step;
  }
  return dst;
}


LogicVRegister Simulator::insr(VectorFormat vform,
                               LogicVRegister dst,
                               uint64_t imm) {
  VIXL_ASSERT(IsSVEFormat(vform));
  for (int i = LaneCountFromFormat(vform) - 1; i > 0; i--) {
    dst.SetUint(vform, i, dst.Uint(vform, i - 1));
  }
  dst.SetUint(vform, 0, imm);
  return dst;
}


LogicVRegister Simulator::mov(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& src) {
  dst.ClearForWrite(vform);
  for (int lane = 0; lane < LaneCountFromFormat(vform); lane++) {
    dst.SetUint(vform, lane, src.Uint(vform, lane));
  }
  return dst;
}


LogicPRegister Simulator::mov(LogicPRegister dst, const LogicPRegister& src) {
  // Avoid a copy if the registers already alias.
  if (dst.Aliases(src)) return dst;

  for (int i = 0; i < dst.GetChunkCount(); i++) {
    dst.SetChunk(i, src.GetChunk(i));
  }
  return dst;
}


LogicVRegister Simulator::mov_merging(VectorFormat vform,
                                      LogicVRegister dst,
                                      const SimPRegister& pg,
                                      const LogicVRegister& src) {
  return sel(vform, dst, pg, src, dst);
}

LogicVRegister Simulator::mov_zeroing(VectorFormat vform,
                                      LogicVRegister dst,
                                      const SimPRegister& pg,
                                      const LogicVRegister& src) {
  SimVRegister zero;
  dup_immediate(vform, zero, 0);
  return sel(vform, dst, pg, src, zero);
}

LogicVRegister Simulator::mov_alternating(VectorFormat vform,
                                          LogicVRegister dst,
                                          const LogicVRegister& src,
                                          int start_at) {
  VIXL_ASSERT((start_at == 0) || (start_at == 1));
  for (int i = start_at; i < LaneCountFromFormat(vform); i += 2) {
    dst.SetUint(vform, i, src.Uint(vform, i));
  }
  return dst;
}

LogicPRegister Simulator::mov_merging(LogicPRegister dst,
                                      const LogicPRegister& pg,
                                      const LogicPRegister& src) {
  return sel(dst, pg, src, dst);
}

LogicPRegister Simulator::mov_zeroing(LogicPRegister dst,
                                      const LogicPRegister& pg,
                                      const LogicPRegister& src) {
  SimPRegister all_false;
  return sel(dst, pg, src, pfalse(all_false));
}

LogicVRegister Simulator::movi(VectorFormat vform,
                               LogicVRegister dst,
                               uint64_t imm) {
  int lane_count = LaneCountFromFormat(vform);
  dst.ClearForWrite(vform);
  for (int i = 0; i < lane_count; ++i) {
    dst.SetUint(vform, i, imm);
  }
  return dst;
}


LogicVRegister Simulator::mvni(VectorFormat vform,
                               LogicVRegister dst,
                               uint64_t imm) {
  int lane_count = LaneCountFromFormat(vform);
  dst.ClearForWrite(vform);
  for (int i = 0; i < lane_count; ++i) {
    dst.SetUint(vform, i, ~imm);
  }
  return dst;
}


LogicVRegister Simulator::orr(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& src,
                              uint64_t imm) {
  uint64_t result[16];
  int lane_count = LaneCountFromFormat(vform);
  for (int i = 0; i < lane_count; ++i) {
    result[i] = src.Uint(vform, i) | imm;
  }
  dst.ClearForWrite(vform);
  for (int i = 0; i < lane_count; ++i) {
    dst.SetUint(vform, i, result[i]);
  }
  return dst;
}


LogicVRegister Simulator::uxtl(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src,
                               bool is_2) {
  VectorFormat vform_half = VectorFormatHalfWidth(vform);
  int lane_count = LaneCountFromFormat(vform);
  int src_offset = is_2 ? lane_count : 0;

  dst.ClearForWrite(vform);
  for (int i = 0; i < lane_count; i++) {
    dst.SetUint(vform, i, src.Uint(vform_half, src_offset + i));
  }
  return dst;
}


LogicVRegister Simulator::sxtl(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src,
                               bool is_2) {
  VectorFormat vform_half = VectorFormatHalfWidth(vform);
  int lane_count = LaneCountFromFormat(vform);
  int src_offset = is_2 ? lane_count : 0;

  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    dst.SetInt(vform, i, src.Int(vform_half, src_offset + i));
  }
  return dst;
}


LogicVRegister Simulator::uxtl2(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src) {
  return uxtl(vform, dst, src, /* is_2 = */ true);
}


LogicVRegister Simulator::sxtl2(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src) {
  return sxtl(vform, dst, src, /* is_2 = */ true);
}


LogicVRegister Simulator::uxt(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& src,
                              unsigned from_size_in_bits) {
  int lane_count = LaneCountFromFormat(vform);
  uint64_t mask = GetUintMask(from_size_in_bits);

  dst.ClearForWrite(vform);
  for (int i = 0; i < lane_count; i++) {
    dst.SetInt(vform, i, src.Uint(vform, i) & mask);
  }
  return dst;
}


LogicVRegister Simulator::sxt(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& src,
                              unsigned from_size_in_bits) {
  int lane_count = LaneCountFromFormat(vform);

  dst.ClearForWrite(vform);
  for (int i = 0; i < lane_count; i++) {
    uint64_t value =
        ExtractSignedBitfield64(from_size_in_bits - 1, 0, src.Uint(vform, i));
    dst.SetInt(vform, i, value);
  }
  return dst;
}


LogicVRegister Simulator::shrn(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src,
                               int shift) {
  SimVRegister temp;
  VectorFormat vform_src = VectorFormatDoubleWidth(vform);
  VectorFormat vform_dst = vform;
  LogicVRegister shifted_src = ushr(vform_src, temp, src, shift);
  return extractnarrow(vform_dst, dst, false, shifted_src, false);
}


LogicVRegister Simulator::shrn2(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src,
                                int shift) {
  SimVRegister temp;
  VectorFormat vformsrc = VectorFormatDoubleWidth(VectorFormatHalfLanes(vform));
  VectorFormat vformdst = vform;
  LogicVRegister shifted_src = ushr(vformsrc, temp, src, shift);
  return extractnarrow(vformdst, dst, false, shifted_src, false);
}


LogicVRegister Simulator::rshrn(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src,
                                int shift) {
  SimVRegister temp;
  VectorFormat vformsrc = VectorFormatDoubleWidth(vform);
  VectorFormat vformdst = vform;
  LogicVRegister shifted_src = ushr(vformsrc, temp, src, shift).Round(vformsrc);
  return extractnarrow(vformdst, dst, false, shifted_src, false);
}


LogicVRegister Simulator::rshrn2(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src,
                                 int shift) {
  SimVRegister temp;
  VectorFormat vformsrc = VectorFormatDoubleWidth(VectorFormatHalfLanes(vform));
  VectorFormat vformdst = vform;
  LogicVRegister shifted_src = ushr(vformsrc, temp, src, shift).Round(vformsrc);
  return extractnarrow(vformdst, dst, false, shifted_src, false);
}

LogicVRegister Simulator::Table(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& ind,
                                bool zero_out_of_bounds,
                                const LogicVRegister* tab1,
                                const LogicVRegister* tab2,
                                const LogicVRegister* tab3,
                                const LogicVRegister* tab4) {
  VIXL_ASSERT(tab1 != NULL);
  int lane_count = LaneCountFromFormat(vform);
  VIXL_ASSERT((tab3 == NULL) || (lane_count <= 16));
  uint64_t table[kZRegMaxSizeInBytes * 2];
  uint64_t result[kZRegMaxSizeInBytes];

  // For Neon, the table source registers are always 16B, and Neon allows only
  // 8B or 16B vform for the destination, so infer the table format from the
  // destination.
  VectorFormat vform_tab = (vform == kFormat8B) ? kFormat16B : vform;

  uint64_t tab_size = tab1->UintArray(vform_tab, &table[0]);
  if (tab2 != NULL) tab_size += tab2->UintArray(vform_tab, &table[tab_size]);
  if (tab3 != NULL) tab_size += tab3->UintArray(vform_tab, &table[tab_size]);
  if (tab4 != NULL) tab_size += tab4->UintArray(vform_tab, &table[tab_size]);

  for (int i = 0; i < lane_count; i++) {
    uint64_t index = ind.Uint(vform, i);
    result[i] = zero_out_of_bounds ? 0 : dst.Uint(vform, i);
    if (index < tab_size) result[i] = table[index];
  }
  dst.SetUintArray(vform, result);
  return dst;
}

LogicVRegister Simulator::tbl(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& tab,
                              const LogicVRegister& ind) {
  return Table(vform, dst, ind, true, &tab);
}


LogicVRegister Simulator::tbl(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& tab,
                              const LogicVRegister& tab2,
                              const LogicVRegister& ind) {
  return Table(vform, dst, ind, true, &tab, &tab2);
}


LogicVRegister Simulator::tbl(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& tab,
                              const LogicVRegister& tab2,
                              const LogicVRegister& tab3,
                              const LogicVRegister& ind) {
  return Table(vform, dst, ind, true, &tab, &tab2, &tab3);
}


LogicVRegister Simulator::tbl(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& tab,
                              const LogicVRegister& tab2,
                              const LogicVRegister& tab3,
                              const LogicVRegister& tab4,
                              const LogicVRegister& ind) {
  return Table(vform, dst, ind, true, &tab, &tab2, &tab3, &tab4);
}


LogicVRegister Simulator::tbx(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& tab,
                              const LogicVRegister& ind) {
  return Table(vform, dst, ind, false, &tab);
}


LogicVRegister Simulator::tbx(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& tab,
                              const LogicVRegister& tab2,
                              const LogicVRegister& ind) {
  return Table(vform, dst, ind, false, &tab, &tab2);
}


LogicVRegister Simulator::tbx(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& tab,
                              const LogicVRegister& tab2,
                              const LogicVRegister& tab3,
                              const LogicVRegister& ind) {
  return Table(vform, dst, ind, false, &tab, &tab2, &tab3);
}


LogicVRegister Simulator::tbx(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& tab,
                              const LogicVRegister& tab2,
                              const LogicVRegister& tab3,
                              const LogicVRegister& tab4,
                              const LogicVRegister& ind) {
  return Table(vform, dst, ind, false, &tab, &tab2, &tab3, &tab4);
}


LogicVRegister Simulator::uqshrn(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src,
                                 int shift) {
  return shrn(vform, dst, src, shift).UnsignedSaturate(vform);
}


LogicVRegister Simulator::uqshrn2(VectorFormat vform,
                                  LogicVRegister dst,
                                  const LogicVRegister& src,
                                  int shift) {
  return shrn2(vform, dst, src, shift).UnsignedSaturate(vform);
}


LogicVRegister Simulator::uqrshrn(VectorFormat vform,
                                  LogicVRegister dst,
                                  const LogicVRegister& src,
                                  int shift) {
  return rshrn(vform, dst, src, shift).UnsignedSaturate(vform);
}


LogicVRegister Simulator::uqrshrn2(VectorFormat vform,
                                   LogicVRegister dst,
                                   const LogicVRegister& src,
                                   int shift) {
  return rshrn2(vform, dst, src, shift).UnsignedSaturate(vform);
}


LogicVRegister Simulator::sqshrn(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src,
                                 int shift) {
  SimVRegister temp;
  VectorFormat vformsrc = VectorFormatDoubleWidth(vform);
  VectorFormat vformdst = vform;
  LogicVRegister shifted_src = sshr(vformsrc, temp, src, shift);
  return sqxtn(vformdst, dst, shifted_src);
}


LogicVRegister Simulator::sqshrn2(VectorFormat vform,
                                  LogicVRegister dst,
                                  const LogicVRegister& src,
                                  int shift) {
  SimVRegister temp;
  VectorFormat vformsrc = VectorFormatDoubleWidth(VectorFormatHalfLanes(vform));
  VectorFormat vformdst = vform;
  LogicVRegister shifted_src = sshr(vformsrc, temp, src, shift);
  return sqxtn(vformdst, dst, shifted_src);
}


LogicVRegister Simulator::sqrshrn(VectorFormat vform,
                                  LogicVRegister dst,
                                  const LogicVRegister& src,
                                  int shift) {
  SimVRegister temp;
  VectorFormat vformsrc = VectorFormatDoubleWidth(vform);
  VectorFormat vformdst = vform;
  LogicVRegister shifted_src = sshr(vformsrc, temp, src, shift).Round(vformsrc);
  return sqxtn(vformdst, dst, shifted_src);
}


LogicVRegister Simulator::sqrshrn2(VectorFormat vform,
                                   LogicVRegister dst,
                                   const LogicVRegister& src,
                                   int shift) {
  SimVRegister temp;
  VectorFormat vformsrc = VectorFormatDoubleWidth(VectorFormatHalfLanes(vform));
  VectorFormat vformdst = vform;
  LogicVRegister shifted_src = sshr(vformsrc, temp, src, shift).Round(vformsrc);
  return sqxtn(vformdst, dst, shifted_src);
}


LogicVRegister Simulator::sqshrun(VectorFormat vform,
                                  LogicVRegister dst,
                                  const LogicVRegister& src,
                                  int shift) {
  SimVRegister temp;
  VectorFormat vformsrc = VectorFormatDoubleWidth(vform);
  VectorFormat vformdst = vform;
  LogicVRegister shifted_src = sshr(vformsrc, temp, src, shift);
  return sqxtun(vformdst, dst, shifted_src);
}


LogicVRegister Simulator::sqshrun2(VectorFormat vform,
                                   LogicVRegister dst,
                                   const LogicVRegister& src,
                                   int shift) {
  SimVRegister temp;
  VectorFormat vformsrc = VectorFormatDoubleWidth(VectorFormatHalfLanes(vform));
  VectorFormat vformdst = vform;
  LogicVRegister shifted_src = sshr(vformsrc, temp, src, shift);
  return sqxtun(vformdst, dst, shifted_src);
}


LogicVRegister Simulator::sqrshrun(VectorFormat vform,
                                   LogicVRegister dst,
                                   const LogicVRegister& src,
                                   int shift) {
  SimVRegister temp;
  VectorFormat vformsrc = VectorFormatDoubleWidth(vform);
  VectorFormat vformdst = vform;
  LogicVRegister shifted_src = sshr(vformsrc, temp, src, shift).Round(vformsrc);
  return sqxtun(vformdst, dst, shifted_src);
}


LogicVRegister Simulator::sqrshrun2(VectorFormat vform,
                                    LogicVRegister dst,
                                    const LogicVRegister& src,
                                    int shift) {
  SimVRegister temp;
  VectorFormat vformsrc = VectorFormatDoubleWidth(VectorFormatHalfLanes(vform));
  VectorFormat vformdst = vform;
  LogicVRegister shifted_src = sshr(vformsrc, temp, src, shift).Round(vformsrc);
  return sqxtun(vformdst, dst, shifted_src);
}


LogicVRegister Simulator::uaddl(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2) {
  SimVRegister temp1, temp2;
  uxtl(vform, temp1, src1);
  uxtl(vform, temp2, src2);
  add(vform, dst, temp1, temp2);
  return dst;
}


LogicVRegister Simulator::uaddl2(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  SimVRegister temp1, temp2;
  uxtl2(vform, temp1, src1);
  uxtl2(vform, temp2, src2);
  add(vform, dst, temp1, temp2);
  return dst;
}


LogicVRegister Simulator::uaddw(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2) {
  SimVRegister temp;
  uxtl(vform, temp, src2);
  add(vform, dst, src1, temp);
  return dst;
}


LogicVRegister Simulator::uaddw2(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  SimVRegister temp;
  uxtl2(vform, temp, src2);
  add(vform, dst, src1, temp);
  return dst;
}


LogicVRegister Simulator::saddl(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2) {
  SimVRegister temp1, temp2;
  sxtl(vform, temp1, src1);
  sxtl(vform, temp2, src2);
  add(vform, dst, temp1, temp2);
  return dst;
}


LogicVRegister Simulator::saddl2(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  SimVRegister temp1, temp2;
  sxtl2(vform, temp1, src1);
  sxtl2(vform, temp2, src2);
  add(vform, dst, temp1, temp2);
  return dst;
}


LogicVRegister Simulator::saddw(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2) {
  SimVRegister temp;
  sxtl(vform, temp, src2);
  add(vform, dst, src1, temp);
  return dst;
}


LogicVRegister Simulator::saddw2(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  SimVRegister temp;
  sxtl2(vform, temp, src2);
  add(vform, dst, src1, temp);
  return dst;
}


LogicVRegister Simulator::usubl(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2) {
  SimVRegister temp1, temp2;
  uxtl(vform, temp1, src1);
  uxtl(vform, temp2, src2);
  sub(vform, dst, temp1, temp2);
  return dst;
}


LogicVRegister Simulator::usubl2(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  SimVRegister temp1, temp2;
  uxtl2(vform, temp1, src1);
  uxtl2(vform, temp2, src2);
  sub(vform, dst, temp1, temp2);
  return dst;
}


LogicVRegister Simulator::usubw(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2) {
  SimVRegister temp;
  uxtl(vform, temp, src2);
  sub(vform, dst, src1, temp);
  return dst;
}


LogicVRegister Simulator::usubw2(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  SimVRegister temp;
  uxtl2(vform, temp, src2);
  sub(vform, dst, src1, temp);
  return dst;
}


LogicVRegister Simulator::ssubl(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2) {
  SimVRegister temp1, temp2;
  sxtl(vform, temp1, src1);
  sxtl(vform, temp2, src2);
  sub(vform, dst, temp1, temp2);
  return dst;
}


LogicVRegister Simulator::ssubl2(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  SimVRegister temp1, temp2;
  sxtl2(vform, temp1, src1);
  sxtl2(vform, temp2, src2);
  sub(vform, dst, temp1, temp2);
  return dst;
}


LogicVRegister Simulator::ssubw(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2) {
  SimVRegister temp;
  sxtl(vform, temp, src2);
  sub(vform, dst, src1, temp);
  return dst;
}


LogicVRegister Simulator::ssubw2(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  SimVRegister temp;
  sxtl2(vform, temp, src2);
  sub(vform, dst, src1, temp);
  return dst;
}


LogicVRegister Simulator::uabal(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2) {
  SimVRegister temp1, temp2;
  uxtl(vform, temp1, src1);
  uxtl(vform, temp2, src2);
  uaba(vform, dst, temp1, temp2);
  return dst;
}


LogicVRegister Simulator::uabal2(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  SimVRegister temp1, temp2;
  uxtl2(vform, temp1, src1);
  uxtl2(vform, temp2, src2);
  uaba(vform, dst, temp1, temp2);
  return dst;
}


LogicVRegister Simulator::sabal(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2) {
  SimVRegister temp1, temp2;
  sxtl(vform, temp1, src1);
  sxtl(vform, temp2, src2);
  saba(vform, dst, temp1, temp2);
  return dst;
}


LogicVRegister Simulator::sabal2(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  SimVRegister temp1, temp2;
  sxtl2(vform, temp1, src1);
  sxtl2(vform, temp2, src2);
  saba(vform, dst, temp1, temp2);
  return dst;
}


LogicVRegister Simulator::uabdl(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2) {
  SimVRegister temp1, temp2;
  uxtl(vform, temp1, src1);
  uxtl(vform, temp2, src2);
  absdiff(vform, dst, temp1, temp2, false);
  return dst;
}


LogicVRegister Simulator::uabdl2(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  SimVRegister temp1, temp2;
  uxtl2(vform, temp1, src1);
  uxtl2(vform, temp2, src2);
  absdiff(vform, dst, temp1, temp2, false);
  return dst;
}


LogicVRegister Simulator::sabdl(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2) {
  SimVRegister temp1, temp2;
  sxtl(vform, temp1, src1);
  sxtl(vform, temp2, src2);
  absdiff(vform, dst, temp1, temp2, true);
  return dst;
}


LogicVRegister Simulator::sabdl2(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  SimVRegister temp1, temp2;
  sxtl2(vform, temp1, src1);
  sxtl2(vform, temp2, src2);
  absdiff(vform, dst, temp1, temp2, true);
  return dst;
}


LogicVRegister Simulator::umull(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2,
                                bool is_2) {
  SimVRegister temp1, temp2;
  uxtl(vform, temp1, src1, is_2);
  uxtl(vform, temp2, src2, is_2);
  mul(vform, dst, temp1, temp2);
  return dst;
}


LogicVRegister Simulator::umull2(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  return umull(vform, dst, src1, src2, /* is_2 = */ true);
}


LogicVRegister Simulator::smull(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2,
                                bool is_2) {
  SimVRegister temp1, temp2;
  sxtl(vform, temp1, src1, is_2);
  sxtl(vform, temp2, src2, is_2);
  mul(vform, dst, temp1, temp2);
  return dst;
}


LogicVRegister Simulator::smull2(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  return smull(vform, dst, src1, src2, /* is_2 = */ true);
}


LogicVRegister Simulator::umlsl(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2,
                                bool is_2) {
  SimVRegister temp1, temp2;
  uxtl(vform, temp1, src1, is_2);
  uxtl(vform, temp2, src2, is_2);
  mls(vform, dst, dst, temp1, temp2);
  return dst;
}


LogicVRegister Simulator::umlsl2(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  return umlsl(vform, dst, src1, src2, /* is_2 = */ true);
}


LogicVRegister Simulator::smlsl(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2,
                                bool is_2) {
  SimVRegister temp1, temp2;
  sxtl(vform, temp1, src1, is_2);
  sxtl(vform, temp2, src2, is_2);
  mls(vform, dst, dst, temp1, temp2);
  return dst;
}


LogicVRegister Simulator::smlsl2(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  return smlsl(vform, dst, src1, src2, /* is_2 = */ true);
}


LogicVRegister Simulator::umlal(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2,
                                bool is_2) {
  SimVRegister temp1, temp2;
  uxtl(vform, temp1, src1, is_2);
  uxtl(vform, temp2, src2, is_2);
  mla(vform, dst, dst, temp1, temp2);
  return dst;
}


LogicVRegister Simulator::umlal2(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  return umlal(vform, dst, src1, src2, /* is_2 = */ true);
}


LogicVRegister Simulator::smlal(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2,
                                bool is_2) {
  SimVRegister temp1, temp2;
  sxtl(vform, temp1, src1, is_2);
  sxtl(vform, temp2, src2, is_2);
  mla(vform, dst, dst, temp1, temp2);
  return dst;
}


LogicVRegister Simulator::smlal2(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  return smlal(vform, dst, src1, src2, /* is_2 = */ true);
}


LogicVRegister Simulator::sqdmlal(VectorFormat vform,
                                  LogicVRegister dst,
                                  const LogicVRegister& src1,
                                  const LogicVRegister& src2,
                                  bool is_2) {
  SimVRegister temp;
  LogicVRegister product = sqdmull(vform, temp, src1, src2, is_2);
  return add(vform, dst, dst, product).SignedSaturate(vform);
}


LogicVRegister Simulator::sqdmlal2(VectorFormat vform,
                                   LogicVRegister dst,
                                   const LogicVRegister& src1,
                                   const LogicVRegister& src2) {
  return sqdmlal(vform, dst, src1, src2, /* is_2 = */ true);
}


LogicVRegister Simulator::sqdmlsl(VectorFormat vform,
                                  LogicVRegister dst,
                                  const LogicVRegister& src1,
                                  const LogicVRegister& src2,
                                  bool is_2) {
  SimVRegister temp;
  LogicVRegister product = sqdmull(vform, temp, src1, src2, is_2);
  return sub(vform, dst, dst, product).SignedSaturate(vform);
}


LogicVRegister Simulator::sqdmlsl2(VectorFormat vform,
                                   LogicVRegister dst,
                                   const LogicVRegister& src1,
                                   const LogicVRegister& src2) {
  return sqdmlsl(vform, dst, src1, src2, /* is_2 = */ true);
}


LogicVRegister Simulator::sqdmull(VectorFormat vform,
                                  LogicVRegister dst,
                                  const LogicVRegister& src1,
                                  const LogicVRegister& src2,
                                  bool is_2) {
  SimVRegister temp;
  LogicVRegister product = smull(vform, temp, src1, src2, is_2);
  return add(vform, dst, product, product).SignedSaturate(vform);
}


LogicVRegister Simulator::sqdmull2(VectorFormat vform,
                                   LogicVRegister dst,
                                   const LogicVRegister& src1,
                                   const LogicVRegister& src2) {
  return sqdmull(vform, dst, src1, src2, /* is_2 = */ true);
}

LogicVRegister Simulator::sqrdmulh(VectorFormat vform,
                                   LogicVRegister dst,
                                   const LogicVRegister& src1,
                                   const LogicVRegister& src2,
                                   bool round) {
  int esize = LaneSizeInBitsFromFormat(vform);

  SimVRegister temp_lo, temp_hi;

  // Compute low and high multiplication results.
  mul(vform, temp_lo, src1, src2);
  smulh(vform, temp_hi, src1, src2);

  // Double by shifting high half, and adding in most-significant bit of low
  // half.
  shl(vform, temp_hi, temp_hi, 1);
  usra(vform, temp_hi, temp_lo, esize - 1);

  if (round) {
    // Add the second (due to doubling) most-significant bit of the low half
    // into the result.
    shl(vform, temp_lo, temp_lo, 1);
    usra(vform, temp_hi, temp_lo, esize - 1);
  }

  SimPRegister not_sat;
  LogicPRegister ptemp(not_sat);
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    // Saturation only occurs when src1 = src2 = minimum representable value.
    // Check this as a special case.
    ptemp.SetActive(vform, i, true);
    if ((src1.Int(vform, i) == MinIntFromFormat(vform)) &&
        (src2.Int(vform, i) == MinIntFromFormat(vform))) {
      ptemp.SetActive(vform, i, false);
    }
    dst.SetInt(vform, i, MaxIntFromFormat(vform));
  }

  mov_merging(vform, dst, not_sat, temp_hi);
  return dst;
}


LogicVRegister Simulator::dot(VectorFormat vform,
                              LogicVRegister dst,
                              const LogicVRegister& src1,
                              const LogicVRegister& src2,
                              bool is_src1_signed,
                              bool is_src2_signed) {
  VectorFormat quarter_vform =
      VectorFormatHalfWidthDoubleLanes(VectorFormatHalfWidthDoubleLanes(vform));

  dst.ClearForWrite(vform);
  for (int e = 0; e < LaneCountFromFormat(vform); e++) {
    uint64_t result = 0;
    int64_t element1, element2;
    for (int i = 0; i < 4; i++) {
      int index = 4 * e + i;
      if (is_src1_signed) {
        element1 = src1.Int(quarter_vform, index);
      } else {
        element1 = src1.Uint(quarter_vform, index);
      }
      if (is_src2_signed) {
        element2 = src2.Int(quarter_vform, index);
      } else {
        element2 = src2.Uint(quarter_vform, index);
      }
      result += element1 * element2;
    }
    dst.SetUint(vform, e, result + dst.Uint(vform, e));
  }
  return dst;
}


LogicVRegister Simulator::sdot(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2) {
  return dot(vform, dst, src1, src2, true, true);
}


LogicVRegister Simulator::udot(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2) {
  return dot(vform, dst, src1, src2, false, false);
}

LogicVRegister Simulator::usdot(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2) {
  return dot(vform, dst, src1, src2, false, true);
}

LogicVRegister Simulator::cdot(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& acc,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2,
                               int rot) {
  VIXL_ASSERT((rot == 0) || (rot == 90) || (rot == 180) || (rot == 270));
  VectorFormat quarter_vform =
      VectorFormatHalfWidthDoubleLanes(VectorFormatHalfWidthDoubleLanes(vform));

  int sel_a = ((rot == 0) || (rot == 180)) ? 0 : 1;
  int sel_b = 1 - sel_a;
  int sub_i = ((rot == 90) || (rot == 180)) ? 1 : -1;

  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    int64_t result = acc.Int(vform, i);
    for (int j = 0; j < 2; j++) {
      int64_t r1 = src1.Int(quarter_vform, (4 * i) + (2 * j) + 0);
      int64_t i1 = src1.Int(quarter_vform, (4 * i) + (2 * j) + 1);
      int64_t r2 = src2.Int(quarter_vform, (4 * i) + (2 * j) + sel_a);
      int64_t i2 = src2.Int(quarter_vform, (4 * i) + (2 * j) + sel_b);
      result += (r1 * r2) + (sub_i * i1 * i2);
    }
    dst.SetInt(vform, i, result);
  }
  return dst;
}

LogicVRegister Simulator::sqrdcmlah(VectorFormat vform,
                                    LogicVRegister dst,
                                    const LogicVRegister& srca,
                                    const LogicVRegister& src1,
                                    const LogicVRegister& src2,
                                    int rot) {
  SimVRegister src1_a, src1_b;
  SimVRegister src2_a, src2_b;
  SimVRegister srca_i, srca_r;
  SimVRegister zero, temp;
  zero.Clear();

  if ((rot == 0) || (rot == 180)) {
    uzp1(vform, src1_a, src1, zero);
    uzp1(vform, src2_a, src2, zero);
    uzp2(vform, src2_b, src2, zero);
  } else {
    uzp2(vform, src1_a, src1, zero);
    uzp2(vform, src2_a, src2, zero);
    uzp1(vform, src2_b, src2, zero);
  }

  uzp1(vform, srca_r, srca, zero);
  uzp2(vform, srca_i, srca, zero);

  bool sub_r = (rot == 90) || (rot == 180);
  bool sub_i = (rot == 180) || (rot == 270);

  const bool round = true;
  sqrdmlash(vform, srca_r, src1_a, src2_a, round, sub_r);
  sqrdmlash(vform, srca_i, src1_a, src2_b, round, sub_i);
  zip1(vform, dst, srca_r, srca_i);
  return dst;
}

LogicVRegister Simulator::sqrdcmlah(VectorFormat vform,
                                    LogicVRegister dst,
                                    const LogicVRegister& srca,
                                    const LogicVRegister& src1,
                                    const LogicVRegister& src2,
                                    int index,
                                    int rot) {
  SimVRegister temp;
  dup_elements_to_segments(VectorFormatDoubleWidth(vform), temp, src2, index);
  return sqrdcmlah(vform, dst, srca, src1, temp, rot);
}

LogicVRegister Simulator::sqrdmlash_d(VectorFormat vform,
                                      LogicVRegister dst,
                                      const LogicVRegister& src1,
                                      const LogicVRegister& src2,
                                      bool round,
                                      bool sub_op) {
  // 2 * INT_64_MIN * INT_64_MIN causes INT_128 to overflow.
  // To avoid this, we use:
  //     (dst << (esize - 1) + src1 * src2 + 1 << (esize - 2)) >> (esize - 1)
  // which is same as:
  //     (dst << esize + 2 * src1 * src2 + 1 << (esize - 1)) >> esize.

  VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kDRegSize);
  int esize = kDRegSize;
  vixl_uint128_t round_const, accum;
  round_const.first = 0;
  if (round) {
    round_const.second = UINT64_C(1) << (esize - 2);
  } else {
    round_const.second = 0;
  }

  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    // Shift the whole value left by `esize - 1` bits.
    accum.first = dst.Int(vform, i) >> 1;
    accum.second = dst.Int(vform, i) << (esize - 1);

    vixl_uint128_t product = Mul64(src1.Int(vform, i), src2.Int(vform, i));

    if (sub_op) {
      product = Neg128(product);
    }
    accum = Add128(accum, product);

    // Perform rounding.
    accum = Add128(accum, round_const);

    // Arithmetic shift the whole value right by `esize - 1` bits.
    accum.second = (accum.first << 1) | (accum.second >> (esize - 1));
    accum.first = -(accum.first >> (esize - 1));

    // Perform saturation.
    bool is_pos = (accum.first == 0) ? true : false;
    if (is_pos &&
        (accum.second > static_cast<uint64_t>(MaxIntFromFormat(vform)))) {
      accum.second = MaxIntFromFormat(vform);
    } else if (!is_pos && (accum.second <
                           static_cast<uint64_t>(MinIntFromFormat(vform)))) {
      accum.second = MinIntFromFormat(vform);
    }

    dst.SetInt(vform, i, accum.second);
  }

  return dst;
}

LogicVRegister Simulator::sqrdmlash(VectorFormat vform,
                                    LogicVRegister dst,
                                    const LogicVRegister& src1,
                                    const LogicVRegister& src2,
                                    bool round,
                                    bool sub_op) {
  // 2 * INT_32_MIN * INT_32_MIN causes int64_t to overflow.
  // To avoid this, we use:
  //     (dst << (esize - 1) + src1 * src2 + 1 << (esize - 2)) >> (esize - 1)
  // which is same as:
  //     (dst << esize + 2 * src1 * src2 + 1 << (esize - 1)) >> esize.

  if (vform == kFormatVnD) {
    return sqrdmlash_d(vform, dst, src1, src2, round, sub_op);
  }

  int esize = LaneSizeInBitsFromFormat(vform);
  int round_const = round ? (1 << (esize - 2)) : 0;
  int64_t accum;

  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    accum = dst.Int(vform, i) << (esize - 1);
    if (sub_op) {
      accum -= src1.Int(vform, i) * src2.Int(vform, i);
    } else {
      accum += src1.Int(vform, i) * src2.Int(vform, i);
    }
    accum += round_const;
    accum = accum >> (esize - 1);

    if (accum > MaxIntFromFormat(vform)) {
      accum = MaxIntFromFormat(vform);
    } else if (accum < MinIntFromFormat(vform)) {
      accum = MinIntFromFormat(vform);
    }
    dst.SetInt(vform, i, accum);
  }
  return dst;
}


LogicVRegister Simulator::sqrdmlah(VectorFormat vform,
                                   LogicVRegister dst,
                                   const LogicVRegister& src1,
                                   const LogicVRegister& src2,
                                   bool round) {
  return sqrdmlash(vform, dst, src1, src2, round, false);
}


LogicVRegister Simulator::sqrdmlsh(VectorFormat vform,
                                   LogicVRegister dst,
                                   const LogicVRegister& src1,
                                   const LogicVRegister& src2,
                                   bool round) {
  return sqrdmlash(vform, dst, src1, src2, round, true);
}


LogicVRegister Simulator::sqdmulh(VectorFormat vform,
                                  LogicVRegister dst,
                                  const LogicVRegister& src1,
                                  const LogicVRegister& src2) {
  return sqrdmulh(vform, dst, src1, src2, false);
}


LogicVRegister Simulator::addhn(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2) {
  SimVRegister temp;
  add(VectorFormatDoubleWidth(vform), temp, src1, src2);
  shrn(vform, dst, temp, LaneSizeInBitsFromFormat(vform));
  return dst;
}


LogicVRegister Simulator::addhn2(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  SimVRegister temp;
  add(VectorFormatDoubleWidth(VectorFormatHalfLanes(vform)), temp, src1, src2);
  shrn2(vform, dst, temp, LaneSizeInBitsFromFormat(vform));
  return dst;
}


LogicVRegister Simulator::raddhn(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  SimVRegister temp;
  add(VectorFormatDoubleWidth(vform), temp, src1, src2);
  rshrn(vform, dst, temp, LaneSizeInBitsFromFormat(vform));
  return dst;
}


LogicVRegister Simulator::raddhn2(VectorFormat vform,
                                  LogicVRegister dst,
                                  const LogicVRegister& src1,
                                  const LogicVRegister& src2) {
  SimVRegister temp;
  add(VectorFormatDoubleWidth(VectorFormatHalfLanes(vform)), temp, src1, src2);
  rshrn2(vform, dst, temp, LaneSizeInBitsFromFormat(vform));
  return dst;
}


LogicVRegister Simulator::subhn(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2) {
  SimVRegister temp;
  sub(VectorFormatDoubleWidth(vform), temp, src1, src2);
  shrn(vform, dst, temp, LaneSizeInBitsFromFormat(vform));
  return dst;
}


LogicVRegister Simulator::subhn2(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  SimVRegister temp;
  sub(VectorFormatDoubleWidth(VectorFormatHalfLanes(vform)), temp, src1, src2);
  shrn2(vform, dst, temp, LaneSizeInBitsFromFormat(vform));
  return dst;
}


LogicVRegister Simulator::rsubhn(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  SimVRegister temp;
  sub(VectorFormatDoubleWidth(vform), temp, src1, src2);
  rshrn(vform, dst, temp, LaneSizeInBitsFromFormat(vform));
  return dst;
}


LogicVRegister Simulator::rsubhn2(VectorFormat vform,
                                  LogicVRegister dst,
                                  const LogicVRegister& src1,
                                  const LogicVRegister& src2) {
  SimVRegister temp;
  sub(VectorFormatDoubleWidth(VectorFormatHalfLanes(vform)), temp, src1, src2);
  rshrn2(vform, dst, temp, LaneSizeInBitsFromFormat(vform));
  return dst;
}


LogicVRegister Simulator::trn1(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2) {
  uint64_t result[kZRegMaxSizeInBytes] = {};
  int lane_count = LaneCountFromFormat(vform);
  int pairs = lane_count / 2;
  for (int i = 0; i < pairs; ++i) {
    result[2 * i] = src1.Uint(vform, 2 * i);
    result[(2 * i) + 1] = src2.Uint(vform, 2 * i);
  }

  dst.ClearForWrite(vform);
  for (int i = 0; i < lane_count; ++i) {
    dst.SetUint(vform, i, result[i]);
  }
  return dst;
}


LogicVRegister Simulator::trn2(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2) {
  uint64_t result[kZRegMaxSizeInBytes] = {};
  int lane_count = LaneCountFromFormat(vform);
  int pairs = lane_count / 2;
  for (int i = 0; i < pairs; ++i) {
    result[2 * i] = src1.Uint(vform, (2 * i) + 1);
    result[(2 * i) + 1] = src2.Uint(vform, (2 * i) + 1);
  }

  dst.ClearForWrite(vform);
  for (int i = 0; i < lane_count; ++i) {
    dst.SetUint(vform, i, result[i]);
  }
  return dst;
}


LogicVRegister Simulator::zip1(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2) {
  uint64_t result[kZRegMaxSizeInBytes] = {};
  int lane_count = LaneCountFromFormat(vform);
  int pairs = lane_count / 2;
  for (int i = 0; i < pairs; ++i) {
    result[2 * i] = src1.Uint(vform, i);
    result[(2 * i) + 1] = src2.Uint(vform, i);
  }

  dst.ClearForWrite(vform);
  for (int i = 0; i < lane_count; ++i) {
    dst.SetUint(vform, i, result[i]);
  }
  return dst;
}


LogicVRegister Simulator::zip2(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2) {
  uint64_t result[kZRegMaxSizeInBytes] = {};
  int lane_count = LaneCountFromFormat(vform);
  int pairs = lane_count / 2;
  for (int i = 0; i < pairs; ++i) {
    result[2 * i] = src1.Uint(vform, pairs + i);
    result[(2 * i) + 1] = src2.Uint(vform, pairs + i);
  }

  dst.ClearForWrite(vform);
  for (int i = 0; i < lane_count; ++i) {
    dst.SetUint(vform, i, result[i]);
  }
  return dst;
}


LogicVRegister Simulator::uzp1(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2) {
  uint64_t result[kZRegMaxSizeInBytes * 2];
  int lane_count = LaneCountFromFormat(vform);
  for (int i = 0; i < lane_count; ++i) {
    result[i] = src1.Uint(vform, i);
    result[lane_count + i] = src2.Uint(vform, i);
  }

  dst.ClearForWrite(vform);
  for (int i = 0; i < lane_count; ++i) {
    dst.SetUint(vform, i, result[2 * i]);
  }
  return dst;
}


LogicVRegister Simulator::uzp2(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2) {
  uint64_t result[kZRegMaxSizeInBytes * 2];
  int lane_count = LaneCountFromFormat(vform);
  for (int i = 0; i < lane_count; ++i) {
    result[i] = src1.Uint(vform, i);
    result[lane_count + i] = src2.Uint(vform, i);
  }

  dst.ClearForWrite(vform);
  for (int i = 0; i < lane_count; ++i) {
    dst.SetUint(vform, i, result[(2 * i) + 1]);
  }
  return dst;
}

LogicVRegister Simulator::interleave_top_bottom(VectorFormat vform,
                                                LogicVRegister dst,
                                                const LogicVRegister& src) {
  // Interleave the top and bottom half of a vector, ie. for a vector:
  //
  //   [ ... | F | D | B | ... | E | C | A ]
  //
  // where B is the first element in the top half of the vector, produce a
  // result vector:
  //
  //   [ ... | ... | F | E | D | C | B | A ]

  uint64_t result[kZRegMaxSizeInBytes] = {};
  int lane_count = LaneCountFromFormat(vform);
  for (int i = 0; i < lane_count; i += 2) {
    result[i] = src.Uint(vform, i / 2);
    result[i + 1] = src.Uint(vform, (lane_count / 2) + (i / 2));
  }
  dst.SetUintArray(vform, result);
  return dst;
}

template <typename T>
T Simulator::FPNeg(T op) {
  return -op;
}

template <typename T>
T Simulator::FPAdd(T op1, T op2) {
  T result = FPProcessNaNs(op1, op2);
  if (IsNaN(result)) {
    return result;
  }

  if (IsInf(op1) && IsInf(op2) && (op1 != op2)) {
    // inf + -inf returns the default NaN.
    FPProcessException();
    return FPDefaultNaN<T>();
  } else {
    // Other cases should be handled by standard arithmetic.
    return op1 + op2;
  }
}


template <typename T>
T Simulator::FPSub(T op1, T op2) {
  // NaNs should be handled elsewhere.
  VIXL_ASSERT(!IsNaN(op1) && !IsNaN(op2));

  if (IsInf(op1) && IsInf(op2) && (op1 == op2)) {
    // inf - inf returns the default NaN.
    FPProcessException();
    return FPDefaultNaN<T>();
  } else {
    // Other cases should be handled by standard arithmetic.
    return op1 - op2;
  }
}

template <typename T>
T Simulator::FPMulNaNs(T op1, T op2) {
  T result = FPProcessNaNs(op1, op2);
  return IsNaN(result) ? result : FPMul(op1, op2);
}

template <typename T>
T Simulator::FPMul(T op1, T op2) {
  // NaNs should be handled elsewhere.
  VIXL_ASSERT(!IsNaN(op1) && !IsNaN(op2));

  if ((IsInf(op1) && (op2 == 0.0)) || (IsInf(op2) && (op1 == 0.0))) {
    // inf * 0.0 returns the default NaN.
    FPProcessException();
    return FPDefaultNaN<T>();
  } else {
    // Other cases should be handled by standard arithmetic.
    return op1 * op2;
  }
}


template <typename T>
T Simulator::FPMulx(T op1, T op2) {
  if ((IsInf(op1) && (op2 == 0.0)) || (IsInf(op2) && (op1 == 0.0))) {
    // inf * 0.0 returns +/-2.0.
    T two = 2.0;
    return copysign(1.0, op1) * copysign(1.0, op2) * two;
  }
  return FPMul(op1, op2);
}


template <typename T>
T Simulator::FPMulAdd(T a, T op1, T op2) {
  T result = FPProcessNaNs3(a, op1, op2);

  T sign_a = copysign(1.0, a);
  T sign_prod = copysign(1.0, op1) * copysign(1.0, op2);
  bool isinf_prod = IsInf(op1) || IsInf(op2);
  bool operation_generates_nan =
      (IsInf(op1) && (op2 == 0.0)) ||                     // inf * 0.0
      (IsInf(op2) && (op1 == 0.0)) ||                     // 0.0 * inf
      (IsInf(a) && isinf_prod && (sign_a != sign_prod));  // inf - inf

  if (IsNaN(result)) {
    // Generated NaNs override quiet NaNs propagated from a.
    if (operation_generates_nan && IsQuietNaN(a)) {
      FPProcessException();
      return FPDefaultNaN<T>();
    } else {
      return result;
    }
  }

  // If the operation would produce a NaN, return the default NaN.
  if (operation_generates_nan) {
    FPProcessException();
    return FPDefaultNaN<T>();
  }

  // Work around broken fma implementations for exact zero results: The sign of
  // exact 0.0 results is positive unless both a and op1 * op2 are negative.
  if (((op1 == 0.0) || (op2 == 0.0)) && (a == 0.0)) {
    return ((sign_a < T(0.0)) && (sign_prod < T(0.0))) ? -0.0 : 0.0;
  }

  result = FusedMultiplyAdd(op1, op2, a);
  VIXL_ASSERT(!IsNaN(result));

  // Work around broken fma implementations for rounded zero results: If a is
  // 0.0, the sign of the result is the sign of op1 * op2 before rounding.
  if ((a == 0.0) && (result == 0.0)) {
    return copysign(0.0, sign_prod);
  }

  return result;
}


template <typename T>
T Simulator::FPDiv(T op1, T op2) {
  // NaNs should be handled elsewhere.
  VIXL_ASSERT(!IsNaN(op1) && !IsNaN(op2));

  if ((IsInf(op1) && IsInf(op2)) || ((op1 == 0.0) && (op2 == 0.0))) {
    // inf / inf and 0.0 / 0.0 return the default NaN.
    FPProcessException();
    return FPDefaultNaN<T>();
  } else {
    if (op2 == 0.0) {
      FPProcessException();
      if (!IsNaN(op1)) {
        double op1_sign = copysign(1.0, op1);
        double op2_sign = copysign(1.0, op2);
        return static_cast<T>(op1_sign * op2_sign * kFP64PositiveInfinity);
      }
    }

    // Other cases should be handled by standard arithmetic.
    return op1 / op2;
  }
}


template <typename T>
T Simulator::FPSqrt(T op) {
  if (IsNaN(op)) {
    return FPProcessNaN(op);
  } else if (op < T(0.0)) {
    FPProcessException();
    return FPDefaultNaN<T>();
  } else {
    return sqrt(op);
  }
}


template <typename T>
T Simulator::FPMax(T a, T b) {
  T result = FPProcessNaNs(a, b);
  if (IsNaN(result)) return result;

  if ((a == 0.0) && (b == 0.0) && (copysign(1.0, a) != copysign(1.0, b))) {
    // a and b are zero, and the sign differs: return +0.0.
    return 0.0;
  } else {
    return (a > b) ? a : b;
  }
}


template <typename T>
T Simulator::FPMaxNM(T a, T b) {
  if (IsQuietNaN(a) && !IsQuietNaN(b)) {
    a = kFP64NegativeInfinity;
  } else if (!IsQuietNaN(a) && IsQuietNaN(b)) {
    b = kFP64NegativeInfinity;
  }

  T result = FPProcessNaNs(a, b);
  return IsNaN(result) ? result : FPMax(a, b);
}


template <typename T>
T Simulator::FPMin(T a, T b) {
  T result = FPProcessNaNs(a, b);
  if (IsNaN(result)) return result;

  if ((a == 0.0) && (b == 0.0) && (copysign(1.0, a) != copysign(1.0, b))) {
    // a and b are zero, and the sign differs: return -0.0.
    return -0.0;
  } else {
    return (a < b) ? a : b;
  }
}


template <typename T>
T Simulator::FPMinNM(T a, T b) {
  if (IsQuietNaN(a) && !IsQuietNaN(b)) {
    a = kFP64PositiveInfinity;
  } else if (!IsQuietNaN(a) && IsQuietNaN(b)) {
    b = kFP64PositiveInfinity;
  }

  T result = FPProcessNaNs(a, b);
  return IsNaN(result) ? result : FPMin(a, b);
}


template <typename T>
T Simulator::FPRecipStepFused(T op1, T op2) {
  const T two = 2.0;
  if ((IsInf(op1) && (op2 == 0.0)) || ((op1 == 0.0) && (IsInf(op2)))) {
    return two;
  } else if (IsInf(op1) || IsInf(op2)) {
    // Return +inf if signs match, otherwise -inf.
    return ((op1 >= 0.0) == (op2 >= 0.0)) ? kFP64PositiveInfinity
                                          : kFP64NegativeInfinity;
  } else {
    return FusedMultiplyAdd(op1, op2, two);
  }
}

template <typename T>
bool IsNormal(T value) {
  return std::isnormal(value);
}

template <>
bool IsNormal(SimFloat16 value) {
  uint16_t rawbits = Float16ToRawbits(value);
  uint16_t exp_mask = 0x7c00;
  // Check that the exponent is neither all zeroes or all ones.
  return ((rawbits & exp_mask) != 0) && ((~rawbits & exp_mask) != 0);
}


template <typename T>
T Simulator::FPRSqrtStepFused(T op1, T op2) {
  const T one_point_five = 1.5;
  const T two = 2.0;

  if ((IsInf(op1) && (op2 == 0.0)) || ((op1 == 0.0) && (IsInf(op2)))) {
    return one_point_five;
  } else if (IsInf(op1) || IsInf(op2)) {
    // Return +inf if signs match, otherwise -inf.
    return ((op1 >= 0.0) == (op2 >= 0.0)) ? kFP64PositiveInfinity
                                          : kFP64NegativeInfinity;
  } else {
    // The multiply-add-halve operation must be fully fused, so avoid interim
    // rounding by checking which operand can be losslessly divided by two
    // before doing the multiply-add.
    if (IsNormal(op1 / two)) {
      return FusedMultiplyAdd(op1 / two, op2, one_point_five);
    } else if (IsNormal(op2 / two)) {
      return FusedMultiplyAdd(op1, op2 / two, one_point_five);
    } else {
      // Neither operand is normal after halving: the result is dominated by
      // the addition term, so just return that.
      return one_point_five;
    }
  }
}

int32_t Simulator::FPToFixedJS(double value) {
  // The Z-flag is set when the conversion from double precision floating-point
  // to 32-bit integer is exact. If the source value is +/-Infinity, -0.0, NaN,
  // outside the bounds of a 32-bit integer, or isn't an exact integer then the
  // Z-flag is unset.
  int Z = 1;
  int32_t result;

  if ((value == 0.0) || (value == kFP64PositiveInfinity) ||
      (value == kFP64NegativeInfinity)) {
    // +/- zero and infinity all return zero, however -0 and +/- Infinity also
    // unset the Z-flag.
    result = 0.0;
    if ((value != 0.0) || std::signbit(value)) {
      Z = 0;
    }
  } else if (std::isnan(value)) {
    // NaN values unset the Z-flag and set the result to 0.
    FPProcessNaN(value);
    result = 0;
    Z = 0;
  } else {
    // All other values are converted to an integer representation, rounded
    // toward zero.
    double int_result = std::floor(value);
    double error = value - int_result;

    if ((error != 0.0) && (int_result < 0.0)) {
      int_result++;
    }

    // Constrain the value into the range [INT32_MIN, INT32_MAX]. We can almost
    // write a one-liner with std::round, but the behaviour on ties is incorrect
    // for our purposes.
    double mod_const = static_cast<double>(UINT64_C(1) << 32);
    double mod_error =
        (int_result / mod_const) - std::floor(int_result / mod_const);
    double constrained;
    if (mod_error == 0.5) {
      constrained = INT32_MIN;
    } else {
      constrained = int_result - mod_const * round(int_result / mod_const);
    }

    VIXL_ASSERT(std::floor(constrained) == constrained);
    VIXL_ASSERT(constrained >= INT32_MIN);
    VIXL_ASSERT(constrained <= INT32_MAX);

    // Take the bottom 32 bits of the result as a 32-bit integer.
    result = static_cast<int32_t>(constrained);

    if ((int_result < INT32_MIN) || (int_result > INT32_MAX) ||
        (error != 0.0)) {
      // If the integer result is out of range or the conversion isn't exact,
      // take exception and unset the Z-flag.
      FPProcessException();
      Z = 0;
    }
  }

  ReadNzcv().SetN(0);
  ReadNzcv().SetZ(Z);
  ReadNzcv().SetC(0);
  ReadNzcv().SetV(0);

  return result;
}

double Simulator::FPRoundIntCommon(double value, FPRounding round_mode) {
  VIXL_ASSERT((value != kFP64PositiveInfinity) &&
              (value != kFP64NegativeInfinity));
  VIXL_ASSERT(!IsNaN(value));

  double int_result = std::floor(value);
  double error = value - int_result;
  switch (round_mode) {
    case FPTieAway: {
      // Take care of correctly handling the range ]-0.5, -0.0], which must
      // yield -0.0.
      if ((-0.5 < value) && (value < 0.0)) {
        int_result = -0.0;

      } else if ((error > 0.5) || ((error == 0.5) && (int_result >= 0.0))) {
        // If the error is greater than 0.5, or is equal to 0.5 and the integer
        // result is positive, round up.
        int_result++;
      }
      break;
    }
    case FPTieEven: {
      // Take care of correctly handling the range [-0.5, -0.0], which must
      // yield -0.0.
      if ((-0.5 <= value) && (value < 0.0)) {
        int_result = -0.0;

        // If the error is greater than 0.5, or is equal to 0.5 and the integer
        // result is odd, round up.
      } else if ((error > 0.5) ||
                 ((error == 0.5) && (std::fmod(int_result, 2) != 0))) {
        int_result++;
      }
      break;
    }
    case FPZero: {
      // If value>0 then we take floor(value)
      // otherwise, ceil(value).
      if (value < 0) {
        int_result = ceil(value);
      }
      break;
    }
    case FPNegativeInfinity: {
      // We always use floor(value).
      break;
    }
    case FPPositiveInfinity: {
      // Take care of correctly handling the range ]-1.0, -0.0], which must
      // yield -0.0.
      if ((-1.0 < value) && (value < 0.0)) {
        int_result = -0.0;

        // If the error is non-zero, round up.
      } else if (error > 0.0) {
        int_result++;
      }
      break;
    }
    default:
      VIXL_UNIMPLEMENTED();
  }
  return int_result;
}

double Simulator::FPRoundInt(double value, FPRounding round_mode) {
  if ((value == 0.0) || (value == kFP64PositiveInfinity) ||
      (value == kFP64NegativeInfinity)) {
    return value;
  } else if (IsNaN(value)) {
    return FPProcessNaN(value);
  }
  return FPRoundIntCommon(value, round_mode);
}

double Simulator::FPRoundInt(double value,
                             FPRounding round_mode,
                             FrintMode frint_mode) {
  if (frint_mode == kFrintToInteger) {
    return FPRoundInt(value, round_mode);
  }

  VIXL_ASSERT((frint_mode == kFrintToInt32) || (frint_mode == kFrintToInt64));

  if (value == 0.0) {
    return value;
  }

  if ((value == kFP64PositiveInfinity) || (value == kFP64NegativeInfinity) ||
      IsNaN(value)) {
    if (frint_mode == kFrintToInt32) {
      return INT32_MIN;
    } else {
      return INT64_MIN;
    }
  }

  double result = FPRoundIntCommon(value, round_mode);

  // We want to compare `result > INT64_MAX` below, but INT64_MAX isn't exactly
  // representable as a double, and is rounded to (INT64_MAX + 1) when
  // converted. To avoid this, we compare `result >= int64_max_plus_one`
  // instead; this is safe because `result` is known to be integral, and
  // `int64_max_plus_one` is exactly representable as a double.
  constexpr uint64_t int64_max_plus_one = static_cast<uint64_t>(INT64_MAX) + 1;
  VIXL_STATIC_ASSERT(static_cast<uint64_t>(static_cast<double>(
                         int64_max_plus_one)) == int64_max_plus_one);

  if (frint_mode == kFrintToInt32) {
    if ((result > INT32_MAX) || (result < INT32_MIN)) {
      return INT32_MIN;
    }
  } else if ((result >= int64_max_plus_one) || (result < INT64_MIN)) {
    return INT64_MIN;
  }

  return result;
}

int16_t Simulator::FPToInt16(double value, FPRounding rmode) {
  value = FPRoundInt(value, rmode);
  if (value >= kHMaxInt) {
    return kHMaxInt;
  } else if (value < kHMinInt) {
    return kHMinInt;
  }
  return IsNaN(value) ? 0 : static_cast<int16_t>(value);
}


int32_t Simulator::FPToInt32(double value, FPRounding rmode) {
  value = FPRoundInt(value, rmode);
  if (value >= kWMaxInt) {
    return kWMaxInt;
  } else if (value < kWMinInt) {
    return kWMinInt;
  }
  return IsNaN(value) ? 0 : static_cast<int32_t>(value);
}


int64_t Simulator::FPToInt64(double value, FPRounding rmode) {
  value = FPRoundInt(value, rmode);
  // This is equivalent to "if (value >= kXMaxInt)" but avoids rounding issues
  // as a result of kMaxInt not being representable as a double.
  if (value >= 9223372036854775808.) {
    return kXMaxInt;
  } else if (value < kXMinInt) {
    return kXMinInt;
  }
  return IsNaN(value) ? 0 : static_cast<int64_t>(value);
}


uint16_t Simulator::FPToUInt16(double value, FPRounding rmode) {
  value = FPRoundInt(value, rmode);
  if (value >= kHMaxUInt) {
    return kHMaxUInt;
  } else if (value < 0.0) {
    return 0;
  }
  return IsNaN(value) ? 0 : static_cast<uint16_t>(value);
}


uint32_t Simulator::FPToUInt32(double value, FPRounding rmode) {
  value = FPRoundInt(value, rmode);
  if (value >= kWMaxUInt) {
    return kWMaxUInt;
  } else if (value < 0.0) {
    return 0;
  }
  return IsNaN(value) ? 0 : static_cast<uint32_t>(value);
}


uint64_t Simulator::FPToUInt64(double value, FPRounding rmode) {
  value = FPRoundInt(value, rmode);
  // This is equivalent to "if (value >= kXMaxUInt)" but avoids rounding issues
  // as a result of kMaxUInt not being representable as a double.
  if (value >= 18446744073709551616.) {
    return kXMaxUInt;
  } else if (value < 0.0) {
    return 0;
  }
  return IsNaN(value) ? 0 : static_cast<uint64_t>(value);
}


#define DEFINE_NEON_FP_VECTOR_OP(FN, OP, PROCNAN)                \
  template <typename T>                                          \
  LogicVRegister Simulator::FN(VectorFormat vform,               \
                               LogicVRegister dst,               \
                               const LogicVRegister& src1,       \
                               const LogicVRegister& src2) {     \
    dst.ClearForWrite(vform);                                    \
    for (int i = 0; i < LaneCountFromFormat(vform); i++) {       \
      T op1 = src1.Float<T>(i);                                  \
      T op2 = src2.Float<T>(i);                                  \
      T result;                                                  \
      if (PROCNAN) {                                             \
        result = FPProcessNaNs(op1, op2);                        \
        if (!IsNaN(result)) {                                    \
          result = OP(op1, op2);                                 \
        }                                                        \
      } else {                                                   \
        result = OP(op1, op2);                                   \
      }                                                          \
      dst.SetFloat(vform, i, result);                            \
    }                                                            \
    return dst;                                                  \
  }                                                              \
                                                                 \
  LogicVRegister Simulator::FN(VectorFormat vform,               \
                               LogicVRegister dst,               \
                               const LogicVRegister& src1,       \
                               const LogicVRegister& src2) {     \
    if (LaneSizeInBitsFromFormat(vform) == kHRegSize) {          \
      FN<SimFloat16>(vform, dst, src1, src2);                    \
    } else if (LaneSizeInBitsFromFormat(vform) == kSRegSize) {   \
      FN<float>(vform, dst, src1, src2);                         \
    } else {                                                     \
      VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kDRegSize); \
      FN<double>(vform, dst, src1, src2);                        \
    }                                                            \
    return dst;                                                  \
  }
NEON_FP3SAME_LIST(DEFINE_NEON_FP_VECTOR_OP)
#undef DEFINE_NEON_FP_VECTOR_OP


LogicVRegister Simulator::fnmul(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2) {
  SimVRegister temp;
  LogicVRegister product = fmul(vform, temp, src1, src2);
  return fneg(vform, dst, product);
}


template <typename T>
LogicVRegister Simulator::frecps(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    T op1 = -src1.Float<T>(i);
    T op2 = src2.Float<T>(i);
    T result = FPProcessNaNs(op1, op2);
    dst.SetFloat(vform, i, IsNaN(result) ? result : FPRecipStepFused(op1, op2));
  }
  return dst;
}


LogicVRegister Simulator::frecps(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  if (LaneSizeInBitsFromFormat(vform) == kHRegSize) {
    frecps<SimFloat16>(vform, dst, src1, src2);
  } else if (LaneSizeInBitsFromFormat(vform) == kSRegSize) {
    frecps<float>(vform, dst, src1, src2);
  } else {
    VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kDRegSize);
    frecps<double>(vform, dst, src1, src2);
  }
  return dst;
}


template <typename T>
LogicVRegister Simulator::frsqrts(VectorFormat vform,
                                  LogicVRegister dst,
                                  const LogicVRegister& src1,
                                  const LogicVRegister& src2) {
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    T op1 = -src1.Float<T>(i);
    T op2 = src2.Float<T>(i);
    T result = FPProcessNaNs(op1, op2);
    dst.SetFloat(vform, i, IsNaN(result) ? result : FPRSqrtStepFused(op1, op2));
  }
  return dst;
}


LogicVRegister Simulator::frsqrts(VectorFormat vform,
                                  LogicVRegister dst,
                                  const LogicVRegister& src1,
                                  const LogicVRegister& src2) {
  if (LaneSizeInBitsFromFormat(vform) == kHRegSize) {
    frsqrts<SimFloat16>(vform, dst, src1, src2);
  } else if (LaneSizeInBitsFromFormat(vform) == kSRegSize) {
    frsqrts<float>(vform, dst, src1, src2);
  } else {
    VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kDRegSize);
    frsqrts<double>(vform, dst, src1, src2);
  }
  return dst;
}


template <typename T>
LogicVRegister Simulator::fcmp(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2,
                               Condition cond) {
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    bool result = false;
    T op1 = src1.Float<T>(i);
    T op2 = src2.Float<T>(i);
    bool unordered = IsNaN(FPProcessNaNs(op1, op2));

    switch (cond) {
      case eq:
        result = (op1 == op2);
        break;
      case ge:
        result = (op1 >= op2);
        break;
      case gt:
        result = (op1 > op2);
        break;
      case le:
        result = (op1 <= op2);
        break;
      case lt:
        result = (op1 < op2);
        break;
      case ne:
        result = (op1 != op2);
        break;
      case uo:
        result = unordered;
        break;
      default:
        // Other conditions are defined in terms of those above.
        VIXL_UNREACHABLE();
        break;
    }

    if (result && unordered) {
      // Only `uo` and `ne` can be true for unordered comparisons.
      VIXL_ASSERT((cond == uo) || (cond == ne));
    }

    dst.SetUint(vform, i, result ? MaxUintFromFormat(vform) : 0);
  }
  return dst;
}


LogicVRegister Simulator::fcmp(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2,
                               Condition cond) {
  if (LaneSizeInBitsFromFormat(vform) == kHRegSize) {
    fcmp<SimFloat16>(vform, dst, src1, src2, cond);
  } else if (LaneSizeInBitsFromFormat(vform) == kSRegSize) {
    fcmp<float>(vform, dst, src1, src2, cond);
  } else {
    VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kDRegSize);
    fcmp<double>(vform, dst, src1, src2, cond);
  }
  return dst;
}


LogicVRegister Simulator::fcmp_zero(VectorFormat vform,
                                    LogicVRegister dst,
                                    const LogicVRegister& src,
                                    Condition cond) {
  SimVRegister temp;
  if (LaneSizeInBitsFromFormat(vform) == kHRegSize) {
    LogicVRegister zero_reg =
        dup_immediate(vform, temp, Float16ToRawbits(SimFloat16(0.0)));
    fcmp<SimFloat16>(vform, dst, src, zero_reg, cond);
  } else if (LaneSizeInBitsFromFormat(vform) == kSRegSize) {
    LogicVRegister zero_reg = dup_immediate(vform, temp, FloatToRawbits(0.0));
    fcmp<float>(vform, dst, src, zero_reg, cond);
  } else {
    VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kDRegSize);
    LogicVRegister zero_reg = dup_immediate(vform, temp, DoubleToRawbits(0.0));
    fcmp<double>(vform, dst, src, zero_reg, cond);
  }
  return dst;
}


LogicVRegister Simulator::fabscmp(VectorFormat vform,
                                  LogicVRegister dst,
                                  const LogicVRegister& src1,
                                  const LogicVRegister& src2,
                                  Condition cond) {
  SimVRegister temp1, temp2;
  if (LaneSizeInBitsFromFormat(vform) == kHRegSize) {
    LogicVRegister abs_src1 = fabs_<SimFloat16>(vform, temp1, src1);
    LogicVRegister abs_src2 = fabs_<SimFloat16>(vform, temp2, src2);
    fcmp<SimFloat16>(vform, dst, abs_src1, abs_src2, cond);
  } else if (LaneSizeInBitsFromFormat(vform) == kSRegSize) {
    LogicVRegister abs_src1 = fabs_<float>(vform, temp1, src1);
    LogicVRegister abs_src2 = fabs_<float>(vform, temp2, src2);
    fcmp<float>(vform, dst, abs_src1, abs_src2, cond);
  } else {
    VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kDRegSize);
    LogicVRegister abs_src1 = fabs_<double>(vform, temp1, src1);
    LogicVRegister abs_src2 = fabs_<double>(vform, temp2, src2);
    fcmp<double>(vform, dst, abs_src1, abs_src2, cond);
  }
  return dst;
}


template <typename T>
LogicVRegister Simulator::fmla(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& srca,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2) {
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    T op1 = src1.Float<T>(i);
    T op2 = src2.Float<T>(i);
    T acc = srca.Float<T>(i);
    T result = FPMulAdd(acc, op1, op2);
    dst.SetFloat(vform, i, result);
  }
  return dst;
}


LogicVRegister Simulator::fmla(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& srca,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2) {
  if (LaneSizeInBitsFromFormat(vform) == kHRegSize) {
    fmla<SimFloat16>(vform, dst, srca, src1, src2);
  } else if (LaneSizeInBitsFromFormat(vform) == kSRegSize) {
    fmla<float>(vform, dst, srca, src1, src2);
  } else {
    VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kDRegSize);
    fmla<double>(vform, dst, srca, src1, src2);
  }
  return dst;
}


template <typename T>
LogicVRegister Simulator::fmls(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& srca,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2) {
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    T op1 = -src1.Float<T>(i);
    T op2 = src2.Float<T>(i);
    T acc = srca.Float<T>(i);
    T result = FPMulAdd(acc, op1, op2);
    dst.SetFloat(i, result);
  }
  return dst;
}


LogicVRegister Simulator::fmls(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& srca,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2) {
  if (LaneSizeInBitsFromFormat(vform) == kHRegSize) {
    fmls<SimFloat16>(vform, dst, srca, src1, src2);
  } else if (LaneSizeInBitsFromFormat(vform) == kSRegSize) {
    fmls<float>(vform, dst, srca, src1, src2);
  } else {
    VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kDRegSize);
    fmls<double>(vform, dst, srca, src1, src2);
  }
  return dst;
}


LogicVRegister Simulator::fmlal(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2) {
  VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kSRegSize);
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    float op1 = FPToFloat(src1.Float<SimFloat16>(i), kIgnoreDefaultNaN);
    float op2 = FPToFloat(src2.Float<SimFloat16>(i), kIgnoreDefaultNaN);
    float acc = dst.Float<float>(i);
    float result = FPMulAdd(acc, op1, op2);
    dst.SetFloat(i, result);
  }
  return dst;
}


LogicVRegister Simulator::fmlal2(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kSRegSize);
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    int src = i + LaneCountFromFormat(vform);
    float op1 = FPToFloat(src1.Float<SimFloat16>(src), kIgnoreDefaultNaN);
    float op2 = FPToFloat(src2.Float<SimFloat16>(src), kIgnoreDefaultNaN);
    float acc = dst.Float<float>(i);
    float result = FPMulAdd(acc, op1, op2);
    dst.SetFloat(i, result);
  }
  return dst;
}


LogicVRegister Simulator::fmlsl(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2) {
  VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kSRegSize);
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    float op1 = -FPToFloat(src1.Float<SimFloat16>(i), kIgnoreDefaultNaN);
    float op2 = FPToFloat(src2.Float<SimFloat16>(i), kIgnoreDefaultNaN);
    float acc = dst.Float<float>(i);
    float result = FPMulAdd(acc, op1, op2);
    dst.SetFloat(i, result);
  }
  return dst;
}


LogicVRegister Simulator::fmlsl2(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kSRegSize);
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    int src = i + LaneCountFromFormat(vform);
    float op1 = -FPToFloat(src1.Float<SimFloat16>(src), kIgnoreDefaultNaN);
    float op2 = FPToFloat(src2.Float<SimFloat16>(src), kIgnoreDefaultNaN);
    float acc = dst.Float<float>(i);
    float result = FPMulAdd(acc, op1, op2);
    dst.SetFloat(i, result);
  }
  return dst;
}


LogicVRegister Simulator::fmlal(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2,
                                int index) {
  VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kSRegSize);
  dst.ClearForWrite(vform);
  float op2 = FPToFloat(src2.Float<SimFloat16>(index), kIgnoreDefaultNaN);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    float op1 = FPToFloat(src1.Float<SimFloat16>(i), kIgnoreDefaultNaN);
    float acc = dst.Float<float>(i);
    float result = FPMulAdd(acc, op1, op2);
    dst.SetFloat(i, result);
  }
  return dst;
}


LogicVRegister Simulator::fmlal2(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2,
                                 int index) {
  VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kSRegSize);
  dst.ClearForWrite(vform);
  float op2 = FPToFloat(src2.Float<SimFloat16>(index), kIgnoreDefaultNaN);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    int src = i + LaneCountFromFormat(vform);
    float op1 = FPToFloat(src1.Float<SimFloat16>(src), kIgnoreDefaultNaN);
    float acc = dst.Float<float>(i);
    float result = FPMulAdd(acc, op1, op2);
    dst.SetFloat(i, result);
  }
  return dst;
}


LogicVRegister Simulator::fmlsl(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2,
                                int index) {
  VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kSRegSize);
  dst.ClearForWrite(vform);
  float op2 = FPToFloat(src2.Float<SimFloat16>(index), kIgnoreDefaultNaN);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    float op1 = -FPToFloat(src1.Float<SimFloat16>(i), kIgnoreDefaultNaN);
    float acc = dst.Float<float>(i);
    float result = FPMulAdd(acc, op1, op2);
    dst.SetFloat(i, result);
  }
  return dst;
}


LogicVRegister Simulator::fmlsl2(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2,
                                 int index) {
  VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kSRegSize);
  dst.ClearForWrite(vform);
  float op2 = FPToFloat(src2.Float<SimFloat16>(index), kIgnoreDefaultNaN);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    int src = i + LaneCountFromFormat(vform);
    float op1 = -FPToFloat(src1.Float<SimFloat16>(src), kIgnoreDefaultNaN);
    float acc = dst.Float<float>(i);
    float result = FPMulAdd(acc, op1, op2);
    dst.SetFloat(i, result);
  }
  return dst;
}


template <typename T>
LogicVRegister Simulator::fneg(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src) {
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    T op = src.Float<T>(i);
    op = -op;
    dst.SetFloat(i, op);
  }
  return dst;
}


LogicVRegister Simulator::fneg(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src) {
  if (LaneSizeInBitsFromFormat(vform) == kHRegSize) {
    fneg<SimFloat16>(vform, dst, src);
  } else if (LaneSizeInBitsFromFormat(vform) == kSRegSize) {
    fneg<float>(vform, dst, src);
  } else {
    VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kDRegSize);
    fneg<double>(vform, dst, src);
  }
  return dst;
}


template <typename T>
LogicVRegister Simulator::fabs_(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src) {
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    T op = src.Float<T>(i);
    if (copysign(1.0, op) < 0.0) {
      op = -op;
    }
    dst.SetFloat(i, op);
  }
  return dst;
}


LogicVRegister Simulator::fabs_(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src) {
  if (LaneSizeInBitsFromFormat(vform) == kHRegSize) {
    fabs_<SimFloat16>(vform, dst, src);
  } else if (LaneSizeInBitsFromFormat(vform) == kSRegSize) {
    fabs_<float>(vform, dst, src);
  } else {
    VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kDRegSize);
    fabs_<double>(vform, dst, src);
  }
  return dst;
}


LogicVRegister Simulator::fabd(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2) {
  SimVRegister temp;
  fsub(vform, temp, src1, src2);
  fabs_(vform, dst, temp);
  return dst;
}


LogicVRegister Simulator::fsqrt(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src) {
  dst.ClearForWrite(vform);
  if (LaneSizeInBitsFromFormat(vform) == kHRegSize) {
    for (int i = 0; i < LaneCountFromFormat(vform); i++) {
      SimFloat16 result = FPSqrt(src.Float<SimFloat16>(i));
      dst.SetFloat(i, result);
    }
  } else if (LaneSizeInBitsFromFormat(vform) == kSRegSize) {
    for (int i = 0; i < LaneCountFromFormat(vform); i++) {
      float result = FPSqrt(src.Float<float>(i));
      dst.SetFloat(i, result);
    }
  } else {
    VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kDRegSize);
    for (int i = 0; i < LaneCountFromFormat(vform); i++) {
      double result = FPSqrt(src.Float<double>(i));
      dst.SetFloat(i, result);
    }
  }
  return dst;
}


#define DEFINE_NEON_FP_PAIR_OP(FNP, FN, OP)                                    \
  LogicVRegister Simulator::FNP(VectorFormat vform,                            \
                                LogicVRegister dst,                            \
                                const LogicVRegister& src1,                    \
                                const LogicVRegister& src2) {                  \
    SimVRegister temp1, temp2;                                                 \
    uzp1(vform, temp1, src1, src2);                                            \
    uzp2(vform, temp2, src1, src2);                                            \
    FN(vform, dst, temp1, temp2);                                              \
    if (IsSVEFormat(vform)) {                                                  \
      interleave_top_bottom(vform, dst, dst);                                  \
    }                                                                          \
    return dst;                                                                \
  }                                                                            \
                                                                               \
  LogicVRegister Simulator::FNP(VectorFormat vform,                            \
                                LogicVRegister dst,                            \
                                const LogicVRegister& src) {                   \
    if (vform == kFormatH) {                                                   \
      SimFloat16 result(OP(SimFloat16(RawbitsToFloat16(src.Uint(vform, 0))),   \
                           SimFloat16(RawbitsToFloat16(src.Uint(vform, 1))))); \
      dst.SetUint(vform, 0, Float16ToRawbits(result));                         \
    } else if (vform == kFormatS) {                                            \
      float result = OP(src.Float<float>(0), src.Float<float>(1));             \
      dst.SetFloat(0, result);                                                 \
    } else {                                                                   \
      VIXL_ASSERT(vform == kFormatD);                                          \
      double result = OP(src.Float<double>(0), src.Float<double>(1));          \
      dst.SetFloat(0, result);                                                 \
    }                                                                          \
    dst.ClearForWrite(vform);                                                  \
    return dst;                                                                \
  }
NEON_FPPAIRWISE_LIST(DEFINE_NEON_FP_PAIR_OP)
#undef DEFINE_NEON_FP_PAIR_OP

template <typename T>
LogicVRegister Simulator::FPPairedAcrossHelper(VectorFormat vform,
                                               LogicVRegister dst,
                                               const LogicVRegister& src,
                                               typename TFPPairOp<T>::type fn,
                                               uint64_t inactive_value) {
  int lane_count = LaneCountFromFormat(vform);
  T result[kZRegMaxSizeInBytes / sizeof(T)];
  // Copy the source vector into a working array. Initialise the unused elements
  // at the end of the array to the same value that a false predicate would set.
  for (int i = 0; i < static_cast<int>(ArrayLength(result)); i++) {
    result[i] = (i < lane_count)
                    ? src.Float<T>(i)
                    : RawbitsWithSizeToFP<T>(sizeof(T) * 8, inactive_value);
  }

  // Pairwise reduce the elements to a single value, using the pair op function
  // argument.
  for (int step = 1; step < lane_count; step *= 2) {
    for (int i = 0; i < lane_count; i += step * 2) {
      result[i] = (this->*fn)(result[i], result[i + step]);
    }
  }
  dst.ClearForWrite(ScalarFormatFromFormat(vform));
  dst.SetFloat<T>(0, result[0]);
  return dst;
}

LogicVRegister Simulator::FPPairedAcrossHelper(
    VectorFormat vform,
    LogicVRegister dst,
    const LogicVRegister& src,
    typename TFPPairOp<SimFloat16>::type fn16,
    typename TFPPairOp<float>::type fn32,
    typename TFPPairOp<double>::type fn64,
    uint64_t inactive_value) {
  switch (LaneSizeInBitsFromFormat(vform)) {
    case kHRegSize:
      return FPPairedAcrossHelper<SimFloat16>(vform,
                                              dst,
                                              src,
                                              fn16,
                                              inactive_value);
    case kSRegSize:
      return FPPairedAcrossHelper<float>(vform, dst, src, fn32, inactive_value);
    default:
      VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kDRegSize);
      return FPPairedAcrossHelper<double>(vform,
                                          dst,
                                          src,
                                          fn64,
                                          inactive_value);
  }
}

LogicVRegister Simulator::faddv(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src) {
  return FPPairedAcrossHelper(vform,
                              dst,
                              src,
                              &Simulator::FPAdd<SimFloat16>,
                              &Simulator::FPAdd<float>,
                              &Simulator::FPAdd<double>,
                              0);
}

LogicVRegister Simulator::fmaxv(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src) {
  int lane_size = LaneSizeInBitsFromFormat(vform);
  uint64_t inactive_value =
      FPToRawbitsWithSize(lane_size, kFP64NegativeInfinity);
  return FPPairedAcrossHelper(vform,
                              dst,
                              src,
                              &Simulator::FPMax<SimFloat16>,
                              &Simulator::FPMax<float>,
                              &Simulator::FPMax<double>,
                              inactive_value);
}


LogicVRegister Simulator::fminv(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src) {
  int lane_size = LaneSizeInBitsFromFormat(vform);
  uint64_t inactive_value =
      FPToRawbitsWithSize(lane_size, kFP64PositiveInfinity);
  return FPPairedAcrossHelper(vform,
                              dst,
                              src,
                              &Simulator::FPMin<SimFloat16>,
                              &Simulator::FPMin<float>,
                              &Simulator::FPMin<double>,
                              inactive_value);
}


LogicVRegister Simulator::fmaxnmv(VectorFormat vform,
                                  LogicVRegister dst,
                                  const LogicVRegister& src) {
  int lane_size = LaneSizeInBitsFromFormat(vform);
  uint64_t inactive_value = FPToRawbitsWithSize(lane_size, kFP64DefaultNaN);
  return FPPairedAcrossHelper(vform,
                              dst,
                              src,
                              &Simulator::FPMaxNM<SimFloat16>,
                              &Simulator::FPMaxNM<float>,
                              &Simulator::FPMaxNM<double>,
                              inactive_value);
}


LogicVRegister Simulator::fminnmv(VectorFormat vform,
                                  LogicVRegister dst,
                                  const LogicVRegister& src) {
  int lane_size = LaneSizeInBitsFromFormat(vform);
  uint64_t inactive_value = FPToRawbitsWithSize(lane_size, kFP64DefaultNaN);
  return FPPairedAcrossHelper(vform,
                              dst,
                              src,
                              &Simulator::FPMinNM<SimFloat16>,
                              &Simulator::FPMinNM<float>,
                              &Simulator::FPMinNM<double>,
                              inactive_value);
}


LogicVRegister Simulator::fmul(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2,
                               int index) {
  dst.ClearForWrite(vform);
  SimVRegister temp;
  if (LaneSizeInBitsFromFormat(vform) == kHRegSize) {
    LogicVRegister index_reg = dup_element(kFormat8H, temp, src2, index);
    fmul<SimFloat16>(vform, dst, src1, index_reg);
  } else if (LaneSizeInBitsFromFormat(vform) == kSRegSize) {
    LogicVRegister index_reg = dup_element(kFormat4S, temp, src2, index);
    fmul<float>(vform, dst, src1, index_reg);
  } else {
    VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kDRegSize);
    LogicVRegister index_reg = dup_element(kFormat2D, temp, src2, index);
    fmul<double>(vform, dst, src1, index_reg);
  }
  return dst;
}


LogicVRegister Simulator::fmla(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2,
                               int index) {
  dst.ClearForWrite(vform);
  SimVRegister temp;
  if (LaneSizeInBitsFromFormat(vform) == kHRegSize) {
    LogicVRegister index_reg = dup_element(kFormat8H, temp, src2, index);
    fmla<SimFloat16>(vform, dst, dst, src1, index_reg);
  } else if (LaneSizeInBitsFromFormat(vform) == kSRegSize) {
    LogicVRegister index_reg = dup_element(kFormat4S, temp, src2, index);
    fmla<float>(vform, dst, dst, src1, index_reg);
  } else {
    VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kDRegSize);
    LogicVRegister index_reg = dup_element(kFormat2D, temp, src2, index);
    fmla<double>(vform, dst, dst, src1, index_reg);
  }
  return dst;
}


LogicVRegister Simulator::fmls(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2,
                               int index) {
  dst.ClearForWrite(vform);
  SimVRegister temp;
  if (LaneSizeInBitsFromFormat(vform) == kHRegSize) {
    LogicVRegister index_reg = dup_element(kFormat8H, temp, src2, index);
    fmls<SimFloat16>(vform, dst, dst, src1, index_reg);
  } else if (LaneSizeInBitsFromFormat(vform) == kSRegSize) {
    LogicVRegister index_reg = dup_element(kFormat4S, temp, src2, index);
    fmls<float>(vform, dst, dst, src1, index_reg);
  } else {
    VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kDRegSize);
    LogicVRegister index_reg = dup_element(kFormat2D, temp, src2, index);
    fmls<double>(vform, dst, dst, src1, index_reg);
  }
  return dst;
}


LogicVRegister Simulator::fmulx(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2,
                                int index) {
  dst.ClearForWrite(vform);
  SimVRegister temp;
  if (LaneSizeInBitsFromFormat(vform) == kHRegSize) {
    LogicVRegister index_reg = dup_element(kFormat8H, temp, src2, index);
    fmulx<SimFloat16>(vform, dst, src1, index_reg);
  } else if (LaneSizeInBitsFromFormat(vform) == kSRegSize) {
    LogicVRegister index_reg = dup_element(kFormat4S, temp, src2, index);
    fmulx<float>(vform, dst, src1, index_reg);
  } else {
    VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kDRegSize);
    LogicVRegister index_reg = dup_element(kFormat2D, temp, src2, index);
    fmulx<double>(vform, dst, src1, index_reg);
  }
  return dst;
}


LogicVRegister Simulator::frint(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src,
                                FPRounding rounding_mode,
                                bool inexact_exception,
                                FrintMode frint_mode) {
  dst.ClearForWrite(vform);
  if (LaneSizeInBitsFromFormat(vform) == kHRegSize) {
    VIXL_ASSERT(frint_mode == kFrintToInteger);
    for (int i = 0; i < LaneCountFromFormat(vform); i++) {
      SimFloat16 input = src.Float<SimFloat16>(i);
      SimFloat16 rounded = FPRoundInt(input, rounding_mode);
      if (inexact_exception && !IsNaN(input) && (input != rounded)) {
        FPProcessException();
      }
      dst.SetFloat<SimFloat16>(i, rounded);
    }
  } else if (LaneSizeInBitsFromFormat(vform) == kSRegSize) {
    for (int i = 0; i < LaneCountFromFormat(vform); i++) {
      float input = src.Float<float>(i);
      float rounded = FPRoundInt(input, rounding_mode, frint_mode);

      if (inexact_exception && !IsNaN(input) && (input != rounded)) {
        FPProcessException();
      }
      dst.SetFloat<float>(i, rounded);
    }
  } else {
    VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kDRegSize);
    for (int i = 0; i < LaneCountFromFormat(vform); i++) {
      double input = src.Float<double>(i);
      double rounded = FPRoundInt(input, rounding_mode, frint_mode);
      if (inexact_exception && !IsNaN(input) && (input != rounded)) {
        FPProcessException();
      }
      dst.SetFloat<double>(i, rounded);
    }
  }
  return dst;
}

LogicVRegister Simulator::fcvt(VectorFormat dst_vform,
                               VectorFormat src_vform,
                               LogicVRegister dst,
                               const LogicPRegister& pg,
                               const LogicVRegister& src) {
  unsigned dst_data_size_in_bits = LaneSizeInBitsFromFormat(dst_vform);
  unsigned src_data_size_in_bits = LaneSizeInBitsFromFormat(src_vform);
  VectorFormat vform = SVEFormatFromLaneSizeInBits(
      std::max(dst_data_size_in_bits, src_data_size_in_bits));

  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    if (!pg.IsActive(vform, i)) continue;

    uint64_t src_raw_bits = ExtractUnsignedBitfield64(src_data_size_in_bits - 1,
                                                      0,
                                                      src.Uint(vform, i));
    double dst_value =
        RawbitsWithSizeToFP<double>(src_data_size_in_bits, src_raw_bits);

    uint64_t dst_raw_bits =
        FPToRawbitsWithSize(dst_data_size_in_bits, dst_value);

    dst.SetUint(vform, i, dst_raw_bits);
  }

  return dst;
}

LogicVRegister Simulator::fcvts(VectorFormat vform,
                                unsigned dst_data_size_in_bits,
                                unsigned src_data_size_in_bits,
                                LogicVRegister dst,
                                const LogicPRegister& pg,
                                const LogicVRegister& src,
                                FPRounding round,
                                int fbits) {
  VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) >= dst_data_size_in_bits);
  VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) >= src_data_size_in_bits);

  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    if (!pg.IsActive(vform, i)) continue;

    uint64_t value = ExtractUnsignedBitfield64(src_data_size_in_bits - 1,
                                               0,
                                               src.Uint(vform, i));
    double result = RawbitsWithSizeToFP<double>(src_data_size_in_bits, value) *
                    std::pow(2.0, fbits);

    switch (dst_data_size_in_bits) {
      case kHRegSize:
        dst.SetInt(vform, i, FPToInt16(result, round));
        break;
      case kSRegSize:
        dst.SetInt(vform, i, FPToInt32(result, round));
        break;
      case kDRegSize:
        dst.SetInt(vform, i, FPToInt64(result, round));
        break;
      default:
        VIXL_UNIMPLEMENTED();
        break;
    }
  }

  return dst;
}

LogicVRegister Simulator::fcvts(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src,
                                FPRounding round,
                                int fbits) {
  dst.ClearForWrite(vform);
  return fcvts(vform,
               LaneSizeInBitsFromFormat(vform),
               LaneSizeInBitsFromFormat(vform),
               dst,
               GetPTrue(),
               src,
               round,
               fbits);
}

LogicVRegister Simulator::fcvtu(VectorFormat vform,
                                unsigned dst_data_size_in_bits,
                                unsigned src_data_size_in_bits,
                                LogicVRegister dst,
                                const LogicPRegister& pg,
                                const LogicVRegister& src,
                                FPRounding round,
                                int fbits) {
  VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) >= dst_data_size_in_bits);
  VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) >= src_data_size_in_bits);

  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    if (!pg.IsActive(vform, i)) continue;

    uint64_t value = ExtractUnsignedBitfield64(src_data_size_in_bits - 1,
                                               0,
                                               src.Uint(vform, i));
    double result = RawbitsWithSizeToFP<double>(src_data_size_in_bits, value) *
                    std::pow(2.0, fbits);

    switch (dst_data_size_in_bits) {
      case kHRegSize:
        dst.SetUint(vform, i, FPToUInt16(result, round));
        break;
      case kSRegSize:
        dst.SetUint(vform, i, FPToUInt32(result, round));
        break;
      case kDRegSize:
        dst.SetUint(vform, i, FPToUInt64(result, round));
        break;
      default:
        VIXL_UNIMPLEMENTED();
        break;
    }
  }

  return dst;
}

LogicVRegister Simulator::fcvtu(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src,
                                FPRounding round,
                                int fbits) {
  dst.ClearForWrite(vform);
  return fcvtu(vform,
               LaneSizeInBitsFromFormat(vform),
               LaneSizeInBitsFromFormat(vform),
               dst,
               GetPTrue(),
               src,
               round,
               fbits);
}

LogicVRegister Simulator::fcvtl(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src) {
  if (LaneSizeInBitsFromFormat(vform) == kSRegSize) {
    for (int i = LaneCountFromFormat(vform) - 1; i >= 0; i--) {
      // TODO: Full support for SimFloat16 in SimRegister(s).
      dst.SetFloat(i,
                   FPToFloat(RawbitsToFloat16(src.Float<uint16_t>(i)),
                             ReadDN()));
    }
  } else {
    VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kDRegSize);
    for (int i = LaneCountFromFormat(vform) - 1; i >= 0; i--) {
      dst.SetFloat(i, FPToDouble(src.Float<float>(i), ReadDN()));
    }
  }
  return dst;
}


LogicVRegister Simulator::fcvtl2(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src) {
  int lane_count = LaneCountFromFormat(vform);
  if (LaneSizeInBitsFromFormat(vform) == kSRegSize) {
    for (int i = 0; i < lane_count; i++) {
      // TODO: Full support for SimFloat16 in SimRegister(s).
      dst.SetFloat(i,
                   FPToFloat(RawbitsToFloat16(
                                 src.Float<uint16_t>(i + lane_count)),
                             ReadDN()));
    }
  } else {
    VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kDRegSize);
    for (int i = 0; i < lane_count; i++) {
      dst.SetFloat(i, FPToDouble(src.Float<float>(i + lane_count), ReadDN()));
    }
  }
  return dst;
}


LogicVRegister Simulator::fcvtn(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src) {
  SimVRegister tmp;
  LogicVRegister srctmp = mov(kFormat2D, tmp, src);
  dst.ClearForWrite(vform);
  if (LaneSizeInBitsFromFormat(vform) == kHRegSize) {
    for (int i = 0; i < LaneCountFromFormat(vform); i++) {
      dst.SetFloat(i,
                   Float16ToRawbits(FPToFloat16(srctmp.Float<float>(i),
                                                FPTieEven,
                                                ReadDN())));
    }
  } else {
    VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kSRegSize);
    for (int i = 0; i < LaneCountFromFormat(vform); i++) {
      dst.SetFloat(i, FPToFloat(srctmp.Float<double>(i), FPTieEven, ReadDN()));
    }
  }
  return dst;
}


LogicVRegister Simulator::fcvtn2(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src) {
  int lane_count = LaneCountFromFormat(vform) / 2;
  if (LaneSizeInBitsFromFormat(vform) == kHRegSize) {
    for (int i = lane_count - 1; i >= 0; i--) {
      dst.SetFloat(i + lane_count,
                   Float16ToRawbits(
                       FPToFloat16(src.Float<float>(i), FPTieEven, ReadDN())));
    }
  } else {
    VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kSRegSize);
    for (int i = lane_count - 1; i >= 0; i--) {
      dst.SetFloat(i + lane_count,
                   FPToFloat(src.Float<double>(i), FPTieEven, ReadDN()));
    }
  }
  return dst;
}


LogicVRegister Simulator::fcvtxn(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src) {
  SimVRegister tmp;
  LogicVRegister srctmp = mov(kFormat2D, tmp, src);
  int input_lane_count = LaneCountFromFormat(vform);
  if (IsSVEFormat(vform)) {
    mov(kFormatVnB, tmp, src);
    input_lane_count /= 2;
  }

  dst.ClearForWrite(vform);
  VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kSRegSize);

  for (int i = 0; i < input_lane_count; i++) {
    dst.SetFloat(i, FPToFloat(srctmp.Float<double>(i), FPRoundOdd, ReadDN()));
  }
  return dst;
}


LogicVRegister Simulator::fcvtxn2(VectorFormat vform,
                                  LogicVRegister dst,
                                  const LogicVRegister& src) {
  VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kSRegSize);
  int lane_count = LaneCountFromFormat(vform) / 2;
  for (int i = lane_count - 1; i >= 0; i--) {
    dst.SetFloat(i + lane_count,
                 FPToFloat(src.Float<double>(i), FPRoundOdd, ReadDN()));
  }
  return dst;
}


// Based on reference C function recip_sqrt_estimate from ARM ARM.
double Simulator::recip_sqrt_estimate(double a) {
  int quot0, quot1, s;
  double r;
  if (a < 0.5) {
    quot0 = static_cast<int>(a * 512.0);
    r = 1.0 / sqrt((static_cast<double>(quot0) + 0.5) / 512.0);
  } else {
    quot1 = static_cast<int>(a * 256.0);
    r = 1.0 / sqrt((static_cast<double>(quot1) + 0.5) / 256.0);
  }
  s = static_cast<int>(256.0 * r + 0.5);
  return static_cast<double>(s) / 256.0;
}


static inline uint64_t Bits(uint64_t val, int start_bit, int end_bit) {
  return ExtractUnsignedBitfield64(start_bit, end_bit, val);
}


template <typename T>
T Simulator::FPRecipSqrtEstimate(T op) {
  if (IsNaN(op)) {
    return FPProcessNaN(op);
  } else if (op == 0.0) {
    if (copysign(1.0, op) < 0.0) {
      return kFP64NegativeInfinity;
    } else {
      return kFP64PositiveInfinity;
    }
  } else if (copysign(1.0, op) < 0.0) {
    FPProcessException();
    return FPDefaultNaN<T>();
  } else if (IsInf(op)) {
    return 0.0;
  } else {
    uint64_t fraction;
    int exp, result_exp;

    if (IsFloat16<T>()) {
      exp = Float16Exp(op);
      fraction = Float16Mantissa(op);
      fraction <<= 42;
    } else if (IsFloat32<T>()) {
      exp = FloatExp(op);
      fraction = FloatMantissa(op);
      fraction <<= 29;
    } else {
      VIXL_ASSERT(IsFloat64<T>());
      exp = DoubleExp(op);
      fraction = DoubleMantissa(op);
    }

    if (exp == 0) {
      while (Bits(fraction, 51, 51) == 0) {
        fraction = Bits(fraction, 50, 0) << 1;
        exp -= 1;
      }
      fraction = Bits(fraction, 50, 0) << 1;
    }

    double scaled;
    if (Bits(exp, 0, 0) == 0) {
      scaled = DoublePack(0, 1022, Bits(fraction, 51, 44) << 44);
    } else {
      scaled = DoublePack(0, 1021, Bits(fraction, 51, 44) << 44);
    }

    if (IsFloat16<T>()) {
      result_exp = (44 - exp) / 2;
    } else if (IsFloat32<T>()) {
      result_exp = (380 - exp) / 2;
    } else {
      VIXL_ASSERT(IsFloat64<T>());
      result_exp = (3068 - exp) / 2;
    }

    uint64_t estimate = DoubleToRawbits(recip_sqrt_estimate(scaled));

    if (IsFloat16<T>()) {
      uint16_t exp_bits = static_cast<uint16_t>(Bits(result_exp, 4, 0));
      uint16_t est_bits = static_cast<uint16_t>(Bits(estimate, 51, 42));
      return Float16Pack(0, exp_bits, est_bits);
    } else if (IsFloat32<T>()) {
      uint32_t exp_bits = static_cast<uint32_t>(Bits(result_exp, 7, 0));
      uint32_t est_bits = static_cast<uint32_t>(Bits(estimate, 51, 29));
      return FloatPack(0, exp_bits, est_bits);
    } else {
      VIXL_ASSERT(IsFloat64<T>());
      return DoublePack(0, Bits(result_exp, 10, 0), Bits(estimate, 51, 0));
    }
  }
}


LogicVRegister Simulator::frsqrte(VectorFormat vform,
                                  LogicVRegister dst,
                                  const LogicVRegister& src) {
  dst.ClearForWrite(vform);
  if (LaneSizeInBitsFromFormat(vform) == kHRegSize) {
    for (int i = 0; i < LaneCountFromFormat(vform); i++) {
      SimFloat16 input = src.Float<SimFloat16>(i);
      dst.SetFloat(vform, i, FPRecipSqrtEstimate<SimFloat16>(input));
    }
  } else if (LaneSizeInBitsFromFormat(vform) == kSRegSize) {
    for (int i = 0; i < LaneCountFromFormat(vform); i++) {
      float input = src.Float<float>(i);
      dst.SetFloat(vform, i, FPRecipSqrtEstimate<float>(input));
    }
  } else {
    VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kDRegSize);
    for (int i = 0; i < LaneCountFromFormat(vform); i++) {
      double input = src.Float<double>(i);
      dst.SetFloat(vform, i, FPRecipSqrtEstimate<double>(input));
    }
  }
  return dst;
}

template <typename T>
T Simulator::FPRecipEstimate(T op, FPRounding rounding) {
  uint32_t sign;

  if (IsFloat16<T>()) {
    sign = Float16Sign(op);
  } else if (IsFloat32<T>()) {
    sign = FloatSign(op);
  } else {
    VIXL_ASSERT(IsFloat64<T>());
    sign = DoubleSign(op);
  }

  if (IsNaN(op)) {
    return FPProcessNaN(op);
  } else if (IsInf(op)) {
    return (sign == 1) ? -0.0 : 0.0;
  } else if (op == 0.0) {
    FPProcessException();  // FPExc_DivideByZero exception.
    return (sign == 1) ? kFP64NegativeInfinity : kFP64PositiveInfinity;
  } else if ((IsFloat16<T>() && (std::fabs(op) < std::pow(2.0, -16.0))) ||
             (IsFloat32<T>() && (std::fabs(op) < std::pow(2.0, -128.0))) ||
             (IsFloat64<T>() && (std::fabs(op) < std::pow(2.0, -1024.0)))) {
    bool overflow_to_inf = false;
    switch (rounding) {
      case FPTieEven:
        overflow_to_inf = true;
        break;
      case FPPositiveInfinity:
        overflow_to_inf = (sign == 0);
        break;
      case FPNegativeInfinity:
        overflow_to_inf = (sign == 1);
        break;
      case FPZero:
        overflow_to_inf = false;
        break;
      default:
        break;
    }
    FPProcessException();  // FPExc_Overflow and FPExc_Inexact.
    if (overflow_to_inf) {
      return (sign == 1) ? kFP64NegativeInfinity : kFP64PositiveInfinity;
    } else {
      // Return FPMaxNormal(sign).
      if (IsFloat16<T>()) {
        return Float16Pack(sign, 0x1f, 0x3ff);
      } else if (IsFloat32<T>()) {
        return FloatPack(sign, 0xfe, 0x07fffff);
      } else {
        VIXL_ASSERT(IsFloat64<T>());
        return DoublePack(sign, 0x7fe, 0x0fffffffffffffl);
      }
    }
  } else {
    uint64_t fraction;
    int exp, result_exp;

    if (IsFloat16<T>()) {
      sign = Float16Sign(op);
      exp = Float16Exp(op);
      fraction = Float16Mantissa(op);
      fraction <<= 42;
    } else if (IsFloat32<T>()) {
      sign = FloatSign(op);
      exp = FloatExp(op);
      fraction = FloatMantissa(op);
      fraction <<= 29;
    } else {
      VIXL_ASSERT(IsFloat64<T>());
      sign = DoubleSign(op);
      exp = DoubleExp(op);
      fraction = DoubleMantissa(op);
    }

    if (exp == 0) {
      if (Bits(fraction, 51, 51) == 0) {
        exp -= 1;
        fraction = Bits(fraction, 49, 0) << 2;
      } else {
        fraction = Bits(fraction, 50, 0) << 1;
      }
    }

    double scaled = DoublePack(0, 1022, Bits(fraction, 51, 44) << 44);

    if (IsFloat16<T>()) {
      result_exp = (29 - exp);  // In range 29-30 = -1 to 29+1 = 30.
    } else if (IsFloat32<T>()) {
      result_exp = (253 - exp);  // In range 253-254 = -1 to 253+1 = 254.
    } else {
      VIXL_ASSERT(IsFloat64<T>());
      result_exp = (2045 - exp);  // In range 2045-2046 = -1 to 2045+1 = 2046.
    }

    double estimate = recip_estimate(scaled);

    fraction = DoubleMantissa(estimate);
    if (result_exp == 0) {
      fraction = (UINT64_C(1) << 51) | Bits(fraction, 51, 1);
    } else if (result_exp == -1) {
      fraction = (UINT64_C(1) << 50) | Bits(fraction, 51, 2);
      result_exp = 0;
    }
    if (IsFloat16<T>()) {
      uint16_t exp_bits = static_cast<uint16_t>(Bits(result_exp, 4, 0));
      uint16_t frac_bits = static_cast<uint16_t>(Bits(fraction, 51, 42));
      return Float16Pack(sign, exp_bits, frac_bits);
    } else if (IsFloat32<T>()) {
      uint32_t exp_bits = static_cast<uint32_t>(Bits(result_exp, 7, 0));
      uint32_t frac_bits = static_cast<uint32_t>(Bits(fraction, 51, 29));
      return FloatPack(sign, exp_bits, frac_bits);
    } else {
      VIXL_ASSERT(IsFloat64<T>());
      return DoublePack(sign, Bits(result_exp, 10, 0), Bits(fraction, 51, 0));
    }
  }
}


LogicVRegister Simulator::frecpe(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src,
                                 FPRounding round) {
  dst.ClearForWrite(vform);
  if (LaneSizeInBitsFromFormat(vform) == kHRegSize) {
    for (int i = 0; i < LaneCountFromFormat(vform); i++) {
      SimFloat16 input = src.Float<SimFloat16>(i);
      dst.SetFloat(vform, i, FPRecipEstimate<SimFloat16>(input, round));
    }
  } else if (LaneSizeInBitsFromFormat(vform) == kSRegSize) {
    for (int i = 0; i < LaneCountFromFormat(vform); i++) {
      float input = src.Float<float>(i);
      dst.SetFloat(vform, i, FPRecipEstimate<float>(input, round));
    }
  } else {
    VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kDRegSize);
    for (int i = 0; i < LaneCountFromFormat(vform); i++) {
      double input = src.Float<double>(i);
      dst.SetFloat(vform, i, FPRecipEstimate<double>(input, round));
    }
  }
  return dst;
}


LogicVRegister Simulator::ursqrte(VectorFormat vform,
                                  LogicVRegister dst,
                                  const LogicVRegister& src) {
  dst.ClearForWrite(vform);
  uint64_t operand;
  uint32_t result;
  double dp_operand, dp_result;
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    operand = src.Uint(vform, i);
    if (operand <= 0x3FFFFFFF) {
      result = 0xFFFFFFFF;
    } else {
      dp_operand = operand * std::pow(2.0, -32);
      dp_result = recip_sqrt_estimate(dp_operand) * std::pow(2.0, 31);
      result = static_cast<uint32_t>(dp_result);
    }
    dst.SetUint(vform, i, result);
  }
  return dst;
}


// Based on reference C function recip_estimate from ARM ARM.
double Simulator::recip_estimate(double a) {
  int q, s;
  double r;
  q = static_cast<int>(a * 512.0);
  r = 1.0 / ((static_cast<double>(q) + 0.5) / 512.0);
  s = static_cast<int>(256.0 * r + 0.5);
  return static_cast<double>(s) / 256.0;
}


LogicVRegister Simulator::urecpe(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src) {
  dst.ClearForWrite(vform);
  uint64_t operand;
  uint32_t result;
  double dp_operand, dp_result;
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    operand = src.Uint(vform, i);
    if (operand <= 0x7FFFFFFF) {
      result = 0xFFFFFFFF;
    } else {
      dp_operand = operand * std::pow(2.0, -32);
      dp_result = recip_estimate(dp_operand) * std::pow(2.0, 31);
      result = static_cast<uint32_t>(dp_result);
    }
    dst.SetUint(vform, i, result);
  }
  return dst;
}

LogicPRegister Simulator::pfalse(LogicPRegister dst) {
  dst.Clear();
  return dst;
}

LogicPRegister Simulator::pfirst(LogicPRegister dst,
                                 const LogicPRegister& pg,
                                 const LogicPRegister& src) {
  int first_pg = GetFirstActive(kFormatVnB, pg);
  VIXL_ASSERT(first_pg < LaneCountFromFormat(kFormatVnB));
  mov(dst, src);
  if (first_pg >= 0) dst.SetActive(kFormatVnB, first_pg, true);
  return dst;
}

LogicPRegister Simulator::ptrue(VectorFormat vform,
                                LogicPRegister dst,
                                int pattern) {
  int count = GetPredicateConstraintLaneCount(vform, pattern);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    dst.SetActive(vform, i, i < count);
  }
  return dst;
}

LogicPRegister Simulator::pnext(VectorFormat vform,
                                LogicPRegister dst,
                                const LogicPRegister& pg,
                                const LogicPRegister& src) {
  int next = GetLastActive(vform, src) + 1;
  while (next < LaneCountFromFormat(vform)) {
    if (pg.IsActive(vform, next)) break;
    next++;
  }

  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    dst.SetActive(vform, i, (i == next));
  }
  return dst;
}

template <typename T>
LogicVRegister Simulator::frecpx(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src) {
  dst.ClearForWrite(vform);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    T op = src.Float<T>(i);
    T result;
    if (IsNaN(op)) {
      result = FPProcessNaN(op);
    } else {
      int exp;
      uint32_t sign;
      if (IsFloat16<T>()) {
        sign = Float16Sign(op);
        exp = Float16Exp(op);
        exp = (exp == 0) ? (0x1F - 1) : static_cast<int>(Bits(~exp, 4, 0));
        result = Float16Pack(sign, exp, 0);
      } else if (IsFloat32<T>()) {
        sign = FloatSign(op);
        exp = FloatExp(op);
        exp = (exp == 0) ? (0xFF - 1) : static_cast<int>(Bits(~exp, 7, 0));
        result = FloatPack(sign, exp, 0);
      } else {
        VIXL_ASSERT(IsFloat64<T>());
        sign = DoubleSign(op);
        exp = DoubleExp(op);
        exp = (exp == 0) ? (0x7FF - 1) : static_cast<int>(Bits(~exp, 10, 0));
        result = DoublePack(sign, exp, 0);
      }
    }
    dst.SetFloat(i, result);
  }
  return dst;
}


LogicVRegister Simulator::frecpx(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src) {
  if (LaneSizeInBitsFromFormat(vform) == kHRegSize) {
    frecpx<SimFloat16>(vform, dst, src);
  } else if (LaneSizeInBitsFromFormat(vform) == kSRegSize) {
    frecpx<float>(vform, dst, src);
  } else {
    VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kDRegSize);
    frecpx<double>(vform, dst, src);
  }
  return dst;
}

LogicVRegister Simulator::flogb(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src) {
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    double op = 0.0;
    switch (vform) {
      case kFormatVnH:
        op = FPToDouble(src.Float<SimFloat16>(i), kIgnoreDefaultNaN);
        break;
      case kFormatVnS:
        op = src.Float<float>(i);
        break;
      case kFormatVnD:
        op = src.Float<double>(i);
        break;
      default:
        VIXL_UNREACHABLE();
    }

    switch (std::fpclassify(op)) {
      case FP_INFINITE:
        dst.SetInt(vform, i, MaxIntFromFormat(vform));
        break;
      case FP_NAN:
      case FP_ZERO:
        dst.SetInt(vform, i, MinIntFromFormat(vform));
        break;
      case FP_SUBNORMAL: {
        // DoubleMantissa returns the mantissa of its input, leaving 12 zero
        // bits where the sign and exponent would be. We subtract 12 to
        // find the number of leading zero bits in the mantissa itself.
        int64_t mant_zero_count = CountLeadingZeros(DoubleMantissa(op)) - 12;
        // Log2 of a subnormal is the lowest exponent a normal number can
        // represent, together with the zeros in the mantissa.
        dst.SetInt(vform, i, -1023 - mant_zero_count);
        break;
      }
      case FP_NORMAL:
        // Log2 of a normal number is the exponent minus the bias.
        dst.SetInt(vform, i, static_cast<int64_t>(DoubleExp(op)) - 1023);
        break;
    }
  }
  return dst;
}

LogicVRegister Simulator::ftsmul(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  SimVRegister maybe_neg_src1;

  // The bottom bit of src2 controls the sign of the result. Use it to
  // conditionally invert the sign of one `fmul` operand.
  shl(vform, maybe_neg_src1, src2, LaneSizeInBitsFromFormat(vform) - 1);
  eor(vform, maybe_neg_src1, maybe_neg_src1, src1);

  // Multiply src1 by the modified neg_src1, which is potentially its negation.
  // In the case of NaNs, NaN * -NaN will return the first NaN intact, so src1,
  // rather than neg_src1, must be the first source argument.
  fmul(vform, dst, src1, maybe_neg_src1);

  return dst;
}

LogicVRegister Simulator::ftssel(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  unsigned lane_bits = LaneSizeInBitsFromFormat(vform);
  uint64_t sign_bit = UINT64_C(1) << (lane_bits - 1);
  uint64_t one;

  if (lane_bits == kHRegSize) {
    one = Float16ToRawbits(Float16(1.0));
  } else if (lane_bits == kSRegSize) {
    one = FloatToRawbits(1.0);
  } else {
    VIXL_ASSERT(lane_bits == kDRegSize);
    one = DoubleToRawbits(1.0);
  }

  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    // Use integer accessors for this operation, as this is a data manipulation
    // task requiring no calculation.
    uint64_t op = src1.Uint(vform, i);

    // Only the bottom two bits of the src2 register are significant, indicating
    // the quadrant. Bit 0 controls whether src1 or 1.0 is written to dst. Bit 1
    // determines the sign of the value written to dst.
    uint64_t q = src2.Uint(vform, i);
    if ((q & 1) == 1) op = one;
    if ((q & 2) == 2) op ^= sign_bit;

    dst.SetUint(vform, i, op);
  }

  return dst;
}

template <typename T>
LogicVRegister Simulator::FTMaddHelper(VectorFormat vform,
                                       LogicVRegister dst,
                                       const LogicVRegister& src1,
                                       const LogicVRegister& src2,
                                       uint64_t coeff_pos,
                                       uint64_t coeff_neg) {
  SimVRegister zero;
  dup_immediate(kFormatVnB, zero, 0);

  SimVRegister cf;
  SimVRegister cfn;
  dup_immediate(vform, cf, coeff_pos);
  dup_immediate(vform, cfn, coeff_neg);

  // The specification requires testing the top bit of the raw value, rather
  // than the sign of the floating point number, so use an integer comparison
  // here.
  SimPRegister is_neg;
  SVEIntCompareVectorsHelper(lt,
                             vform,
                             is_neg,
                             GetPTrue(),
                             src2,
                             zero,
                             false,
                             LeaveFlags);
  mov_merging(vform, cf, is_neg, cfn);

  SimVRegister temp;
  fabs_<T>(vform, temp, src2);
  fmla<T>(vform, cf, cf, src1, temp);
  mov(vform, dst, cf);
  return dst;
}


LogicVRegister Simulator::ftmad(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src1,
                                const LogicVRegister& src2,
                                unsigned index) {
  static const uint64_t ftmad_coeff16[] = {0x3c00,
                                           0xb155,
                                           0x2030,
                                           0x0000,
                                           0x0000,
                                           0x0000,
                                           0x0000,
                                           0x0000,
                                           0x3c00,
                                           0xb800,
                                           0x293a,
                                           0x0000,
                                           0x0000,
                                           0x0000,
                                           0x0000,
                                           0x0000};

  static const uint64_t ftmad_coeff32[] = {0x3f800000,
                                           0xbe2aaaab,
                                           0x3c088886,
                                           0xb95008b9,
                                           0x36369d6d,
                                           0x00000000,
                                           0x00000000,
                                           0x00000000,
                                           0x3f800000,
                                           0xbf000000,
                                           0x3d2aaaa6,
                                           0xbab60705,
                                           0x37cd37cc,
                                           0x00000000,
                                           0x00000000,
                                           0x00000000};

  static const uint64_t ftmad_coeff64[] = {0x3ff0000000000000,
                                           0xbfc5555555555543,
                                           0x3f8111111110f30c,
                                           0xbf2a01a019b92fc6,
                                           0x3ec71de351f3d22b,
                                           0xbe5ae5e2b60f7b91,
                                           0x3de5d8408868552f,
                                           0x0000000000000000,
                                           0x3ff0000000000000,
                                           0xbfe0000000000000,
                                           0x3fa5555555555536,
                                           0xbf56c16c16c13a0b,
                                           0x3efa01a019b1e8d8,
                                           0xbe927e4f7282f468,
                                           0x3e21ee96d2641b13,
                                           0xbda8f76380fbb401};
  VIXL_ASSERT((index + 8) < ArrayLength(ftmad_coeff64));
  VIXL_ASSERT(ArrayLength(ftmad_coeff16) == ArrayLength(ftmad_coeff64));
  VIXL_ASSERT(ArrayLength(ftmad_coeff32) == ArrayLength(ftmad_coeff64));

  if (LaneSizeInBitsFromFormat(vform) == kHRegSize) {
    FTMaddHelper<SimFloat16>(vform,
                             dst,
                             src1,
                             src2,
                             ftmad_coeff16[index],
                             ftmad_coeff16[index + 8]);
  } else if (LaneSizeInBitsFromFormat(vform) == kSRegSize) {
    FTMaddHelper<float>(vform,
                        dst,
                        src1,
                        src2,
                        ftmad_coeff32[index],
                        ftmad_coeff32[index + 8]);
  } else {
    VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kDRegSize);
    FTMaddHelper<double>(vform,
                         dst,
                         src1,
                         src2,
                         ftmad_coeff64[index],
                         ftmad_coeff64[index + 8]);
  }
  return dst;
}

LogicVRegister Simulator::fexpa(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src) {
  static const uint64_t fexpa_coeff16[] = {0x0000, 0x0016, 0x002d, 0x0045,
                                           0x005d, 0x0075, 0x008e, 0x00a8,
                                           0x00c2, 0x00dc, 0x00f8, 0x0114,
                                           0x0130, 0x014d, 0x016b, 0x0189,
                                           0x01a8, 0x01c8, 0x01e8, 0x0209,
                                           0x022b, 0x024e, 0x0271, 0x0295,
                                           0x02ba, 0x02e0, 0x0306, 0x032e,
                                           0x0356, 0x037f, 0x03a9, 0x03d4};

  static const uint64_t fexpa_coeff32[] =
      {0x000000, 0x0164d2, 0x02cd87, 0x043a29, 0x05aac3, 0x071f62, 0x08980f,
       0x0a14d5, 0x0b95c2, 0x0d1adf, 0x0ea43a, 0x1031dc, 0x11c3d3, 0x135a2b,
       0x14f4f0, 0x16942d, 0x1837f0, 0x19e046, 0x1b8d3a, 0x1d3eda, 0x1ef532,
       0x20b051, 0x227043, 0x243516, 0x25fed7, 0x27cd94, 0x29a15b, 0x2b7a3a,
       0x2d583f, 0x2f3b79, 0x3123f6, 0x3311c4, 0x3504f3, 0x36fd92, 0x38fbaf,
       0x3aff5b, 0x3d08a4, 0x3f179a, 0x412c4d, 0x4346cd, 0x45672a, 0x478d75,
       0x49b9be, 0x4bec15, 0x4e248c, 0x506334, 0x52a81e, 0x54f35b, 0x5744fd,
       0x599d16, 0x5bfbb8, 0x5e60f5, 0x60ccdf, 0x633f89, 0x65b907, 0x68396a,
       0x6ac0c7, 0x6d4f30, 0x6fe4ba, 0x728177, 0x75257d, 0x77d0df, 0x7a83b3,
       0x7d3e0c};

  static const uint64_t fexpa_coeff64[] =
      {0X0000000000000, 0X02c9a3e778061, 0X059b0d3158574, 0X0874518759bc8,
       0X0b5586cf9890f, 0X0e3ec32d3d1a2, 0X11301d0125b51, 0X1429aaea92de0,
       0X172b83c7d517b, 0X1a35beb6fcb75, 0X1d4873168b9aa, 0X2063b88628cd6,
       0X2387a6e756238, 0X26b4565e27cdd, 0X29e9df51fdee1, 0X2d285a6e4030b,
       0X306fe0a31b715, 0X33c08b26416ff, 0X371a7373aa9cb, 0X3a7db34e59ff7,
       0X3dea64c123422, 0X4160a21f72e2a, 0X44e086061892d, 0X486a2b5c13cd0,
       0X4bfdad5362a27, 0X4f9b2769d2ca7, 0X5342b569d4f82, 0X56f4736b527da,
       0X5ab07dd485429, 0X5e76f15ad2148, 0X6247eb03a5585, 0X6623882552225,
       0X6a09e667f3bcd, 0X6dfb23c651a2f, 0X71f75e8ec5f74, 0X75feb564267c9,
       0X7a11473eb0187, 0X7e2f336cf4e62, 0X82589994cce13, 0X868d99b4492ed,
       0X8ace5422aa0db, 0X8f1ae99157736, 0X93737b0cdc5e5, 0X97d829fde4e50,
       0X9c49182a3f090, 0Xa0c667b5de565, 0Xa5503b23e255d, 0Xa9e6b5579fdbf,
       0Xae89f995ad3ad, 0Xb33a2b84f15fb, 0Xb7f76f2fb5e47, 0Xbcc1e904bc1d2,
       0Xc199bdd85529c, 0Xc67f12e57d14b, 0Xcb720dcef9069, 0Xd072d4a07897c,
       0Xd5818dcfba487, 0Xda9e603db3285, 0Xdfc97337b9b5f, 0Xe502ee78b3ff6,
       0Xea4afa2a490da, 0Xefa1bee615a27, 0Xf50765b6e4540, 0Xfa7c1819e90d8};

  unsigned lane_size = LaneSizeInBitsFromFormat(vform);
  int index_highbit = 5;
  int op_highbit, op_shift;
  const uint64_t* fexpa_coeff;

  if (lane_size == kHRegSize) {
    index_highbit = 4;
    VIXL_ASSERT(ArrayLength(fexpa_coeff16) == (1U << (index_highbit + 1)));
    fexpa_coeff = fexpa_coeff16;
    op_highbit = 9;
    op_shift = 10;
  } else if (lane_size == kSRegSize) {
    VIXL_ASSERT(ArrayLength(fexpa_coeff32) == (1U << (index_highbit + 1)));
    fexpa_coeff = fexpa_coeff32;
    op_highbit = 13;
    op_shift = 23;
  } else {
    VIXL_ASSERT(lane_size == kDRegSize);
    VIXL_ASSERT(ArrayLength(fexpa_coeff64) == (1U << (index_highbit + 1)));
    fexpa_coeff = fexpa_coeff64;
    op_highbit = 16;
    op_shift = 52;
  }

  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    uint64_t op = src.Uint(vform, i);
    uint64_t result = fexpa_coeff[Bits(op, index_highbit, 0)];
    result |= (Bits(op, op_highbit, index_highbit + 1) << op_shift);
    dst.SetUint(vform, i, result);
  }
  return dst;
}

template <typename T>
LogicVRegister Simulator::fscale(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  T two = T(2.0);
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    T src1_val = src1.Float<T>(i);
    if (!IsNaN(src1_val)) {
      int64_t scale = src2.Int(vform, i);
      // TODO: this is a low-performance implementation, but it's simple and
      // less likely to be buggy. Consider replacing it with something faster.

      // Scales outside of these bounds become infinity or zero, so there's no
      // point iterating further.
      scale = std::min<int64_t>(std::max<int64_t>(scale, -2048), 2048);

      // Compute src1_val * 2 ^ scale. If scale is positive, multiply by two and
      // decrement scale until it's zero.
      while (scale-- > 0) {
        src1_val = FPMul(src1_val, two);
      }

      // If scale is negative, divide by two and increment scale until it's
      // zero. Initially, scale is (src2 - 1), so we pre-increment.
      while (++scale < 0) {
        src1_val = FPDiv(src1_val, two);
      }
    }
    dst.SetFloat<T>(i, src1_val);
  }
  return dst;
}

LogicVRegister Simulator::fscale(VectorFormat vform,
                                 LogicVRegister dst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2) {
  if (LaneSizeInBitsFromFormat(vform) == kHRegSize) {
    fscale<SimFloat16>(vform, dst, src1, src2);
  } else if (LaneSizeInBitsFromFormat(vform) == kSRegSize) {
    fscale<float>(vform, dst, src1, src2);
  } else {
    VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kDRegSize);
    fscale<double>(vform, dst, src1, src2);
  }
  return dst;
}

LogicVRegister Simulator::scvtf(VectorFormat vform,
                                unsigned dst_data_size_in_bits,
                                unsigned src_data_size_in_bits,
                                LogicVRegister dst,
                                const LogicPRegister& pg,
                                const LogicVRegister& src,
                                FPRounding round,
                                int fbits) {
  VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) >= dst_data_size_in_bits);
  VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) >= src_data_size_in_bits);
  dst.ClearForWrite(vform);

  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    if (!pg.IsActive(vform, i)) continue;

    int64_t value = ExtractSignedBitfield64(src_data_size_in_bits - 1,
                                            0,
                                            src.Uint(vform, i));

    switch (dst_data_size_in_bits) {
      case kHRegSize: {
        SimFloat16 result = FixedToFloat16(value, fbits, round);
        dst.SetUint(vform, i, Float16ToRawbits(result));
        break;
      }
      case kSRegSize: {
        float result = FixedToFloat(value, fbits, round);
        dst.SetUint(vform, i, FloatToRawbits(result));
        break;
      }
      case kDRegSize: {
        double result = FixedToDouble(value, fbits, round);
        dst.SetUint(vform, i, DoubleToRawbits(result));
        break;
      }
      default:
        VIXL_UNIMPLEMENTED();
        break;
    }
  }

  return dst;
}

LogicVRegister Simulator::scvtf(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src,
                                int fbits,
                                FPRounding round) {
  return scvtf(vform,
               LaneSizeInBitsFromFormat(vform),
               LaneSizeInBitsFromFormat(vform),
               dst,
               GetPTrue(),
               src,
               round,
               fbits);
}

LogicVRegister Simulator::ucvtf(VectorFormat vform,
                                unsigned dst_data_size_in_bits,
                                unsigned src_data_size_in_bits,
                                LogicVRegister dst,
                                const LogicPRegister& pg,
                                const LogicVRegister& src,
                                FPRounding round,
                                int fbits) {
  VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) >= dst_data_size_in_bits);
  VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) >= src_data_size_in_bits);
  dst.ClearForWrite(vform);

  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    if (!pg.IsActive(vform, i)) continue;

    uint64_t value = ExtractUnsignedBitfield64(src_data_size_in_bits - 1,
                                               0,
                                               src.Uint(vform, i));

    switch (dst_data_size_in_bits) {
      case kHRegSize: {
        SimFloat16 result = UFixedToFloat16(value, fbits, round);
        dst.SetUint(vform, i, Float16ToRawbits(result));
        break;
      }
      case kSRegSize: {
        float result = UFixedToFloat(value, fbits, round);
        dst.SetUint(vform, i, FloatToRawbits(result));
        break;
      }
      case kDRegSize: {
        double result = UFixedToDouble(value, fbits, round);
        dst.SetUint(vform, i, DoubleToRawbits(result));
        break;
      }
      default:
        VIXL_UNIMPLEMENTED();
        break;
    }
  }

  return dst;
}

LogicVRegister Simulator::ucvtf(VectorFormat vform,
                                LogicVRegister dst,
                                const LogicVRegister& src,
                                int fbits,
                                FPRounding round) {
  return ucvtf(vform,
               LaneSizeInBitsFromFormat(vform),
               LaneSizeInBitsFromFormat(vform),
               dst,
               GetPTrue(),
               src,
               round,
               fbits);
}

LogicVRegister Simulator::unpk(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src,
                               UnpackType unpack_type,
                               ExtendType extend_type) {
  VectorFormat vform_half = VectorFormatHalfWidth(vform);
  const int lane_count = LaneCountFromFormat(vform);
  const int src_start_lane = (unpack_type == kLoHalf) ? 0 : lane_count;

  switch (extend_type) {
    case kSignedExtend: {
      int64_t result[kZRegMaxSizeInBytes];
      for (int i = 0; i < lane_count; ++i) {
        result[i] = src.Int(vform_half, i + src_start_lane);
      }
      for (int i = 0; i < lane_count; ++i) {
        dst.SetInt(vform, i, result[i]);
      }
      break;
    }
    case kUnsignedExtend: {
      uint64_t result[kZRegMaxSizeInBytes];
      for (int i = 0; i < lane_count; ++i) {
        result[i] = src.Uint(vform_half, i + src_start_lane);
      }
      for (int i = 0; i < lane_count; ++i) {
        dst.SetUint(vform, i, result[i]);
      }
      break;
    }
    default:
      VIXL_UNREACHABLE();
  }
  return dst;
}

LogicPRegister Simulator::SVEIntCompareVectorsHelper(Condition cond,
                                                     VectorFormat vform,
                                                     LogicPRegister dst,
                                                     const LogicPRegister& mask,
                                                     const LogicVRegister& src1,
                                                     const LogicVRegister& src2,
                                                     bool is_wide_elements,
                                                     FlagsUpdate flags) {
  for (int lane = 0; lane < LaneCountFromFormat(vform); lane++) {
    bool result = false;
    if (mask.IsActive(vform, lane)) {
      int64_t op1 = 0xbadbeef;
      int64_t op2 = 0xbadbeef;
      int d_lane = (lane * LaneSizeInBitsFromFormat(vform)) / kDRegSize;
      switch (cond) {
        case eq:
        case ge:
        case gt:
        case lt:
        case le:
        case ne:
          op1 = src1.Int(vform, lane);
          op2 = is_wide_elements ? src2.Int(kFormatVnD, d_lane)
                                 : src2.Int(vform, lane);
          break;
        case hi:
        case hs:
        case ls:
        case lo:
          op1 = src1.Uint(vform, lane);
          op2 = is_wide_elements ? src2.Uint(kFormatVnD, d_lane)
                                 : src2.Uint(vform, lane);
          break;
        default:
          VIXL_UNREACHABLE();
      }

      switch (cond) {
        case eq:
          result = (op1 == op2);
          break;
        case ne:
          result = (op1 != op2);
          break;
        case ge:
          result = (op1 >= op2);
          break;
        case gt:
          result = (op1 > op2);
          break;
        case le:
          result = (op1 <= op2);
          break;
        case lt:
          result = (op1 < op2);
          break;
        case hs:
          result = (static_cast<uint64_t>(op1) >= static_cast<uint64_t>(op2));
          break;
        case hi:
          result = (static_cast<uint64_t>(op1) > static_cast<uint64_t>(op2));
          break;
        case ls:
          result = (static_cast<uint64_t>(op1) <= static_cast<uint64_t>(op2));
          break;
        case lo:
          result = (static_cast<uint64_t>(op1) < static_cast<uint64_t>(op2));
          break;
        default:
          VIXL_UNREACHABLE();
      }
    }
    dst.SetActive(vform, lane, result);
  }

  if (flags == SetFlags) PredTest(vform, mask, dst);

  return dst;
}

LogicVRegister Simulator::SVEBitwiseShiftHelper(Shift shift_op,
                                                VectorFormat vform,
                                                LogicVRegister dst,
                                                const LogicVRegister& src1,
                                                const LogicVRegister& src2,
                                                bool is_wide_elements) {
  unsigned lane_size = LaneSizeInBitsFromFormat(vform);
  VectorFormat shift_vform = is_wide_elements ? kFormatVnD : vform;

  for (int lane = 0; lane < LaneCountFromFormat(vform); lane++) {
    int shift_src_lane = lane;
    if (is_wide_elements) {
      // If the shift amount comes from wide elements, select the D-sized lane
      // which occupies the corresponding lanes of the value to be shifted.
      shift_src_lane = (lane * lane_size) / kDRegSize;
    }
    uint64_t shift_amount = src2.Uint(shift_vform, shift_src_lane);

    // Saturate shift_amount to the size of the lane that will be shifted.
    if (shift_amount > lane_size) shift_amount = lane_size;

    uint64_t value = src1.Uint(vform, lane);
    int64_t result = ShiftOperand(lane_size,
                                  value,
                                  shift_op,
                                  static_cast<unsigned>(shift_amount));
    dst.SetUint(vform, lane, result);
  }

  return dst;
}

LogicVRegister Simulator::asrd(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               int shift) {
  VIXL_ASSERT((shift > 0) && (static_cast<unsigned>(shift) <=
                              LaneSizeInBitsFromFormat(vform)));

  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    int64_t value = src1.Int(vform, i);
    if (shift <= 63) {
      if (value < 0) {
        // The max possible mask is 0x7fff'ffff'ffff'ffff, which can be safely
        // cast to int64_t, and cannot cause signed overflow in the result.
        value = value + GetUintMask(shift);
      }
      value = ShiftOperand(kDRegSize, value, ASR, shift);
    } else {
      value = 0;
    }
    dst.SetInt(vform, i, value);
  }
  return dst;
}

LogicVRegister Simulator::SVEBitwiseLogicalUnpredicatedHelper(
    LogicalOp logical_op,
    VectorFormat vform,
    LogicVRegister zd,
    const LogicVRegister& zn,
    const LogicVRegister& zm) {
  VIXL_ASSERT(IsSVEFormat(vform));
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    uint64_t op1 = zn.Uint(vform, i);
    uint64_t op2 = zm.Uint(vform, i);
    uint64_t result = 0;
    switch (logical_op) {
      case AND:
        result = op1 & op2;
        break;
      case BIC:
        result = op1 & ~op2;
        break;
      case EOR:
        result = op1 ^ op2;
        break;
      case ORR:
        result = op1 | op2;
        break;
      default:
        VIXL_UNIMPLEMENTED();
    }
    zd.SetUint(vform, i, result);
  }

  return zd;
}

LogicPRegister Simulator::SVEPredicateLogicalHelper(SVEPredicateLogicalOp op,
                                                    LogicPRegister pd,
                                                    const LogicPRegister& pn,
                                                    const LogicPRegister& pm) {
  for (int i = 0; i < pn.GetChunkCount(); i++) {
    LogicPRegister::ChunkType op1 = pn.GetChunk(i);
    LogicPRegister::ChunkType op2 = pm.GetChunk(i);
    LogicPRegister::ChunkType result = 0;
    switch (op) {
      case ANDS_p_p_pp_z:
      case AND_p_p_pp_z:
        result = op1 & op2;
        break;
      case BICS_p_p_pp_z:
      case BIC_p_p_pp_z:
        result = op1 & ~op2;
        break;
      case EORS_p_p_pp_z:
      case EOR_p_p_pp_z:
        result = op1 ^ op2;
        break;
      case NANDS_p_p_pp_z:
      case NAND_p_p_pp_z:
        result = ~(op1 & op2);
        break;
      case NORS_p_p_pp_z:
      case NOR_p_p_pp_z:
        result = ~(op1 | op2);
        break;
      case ORNS_p_p_pp_z:
      case ORN_p_p_pp_z:
        result = op1 | ~op2;
        break;
      case ORRS_p_p_pp_z:
      case ORR_p_p_pp_z:
        result = op1 | op2;
        break;
      default:
        VIXL_UNIMPLEMENTED();
    }
    pd.SetChunk(i, result);
  }
  return pd;
}

LogicVRegister Simulator::SVEBitwiseImmHelper(
    SVEBitwiseLogicalWithImm_UnpredicatedOp op,
    VectorFormat vform,
    LogicVRegister zd,
    uint64_t imm) {
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    uint64_t op1 = zd.Uint(vform, i);
    uint64_t result = 0;
    switch (op) {
      case AND_z_zi:
        result = op1 & imm;
        break;
      case EOR_z_zi:
        result = op1 ^ imm;
        break;
      case ORR_z_zi:
        result = op1 | imm;
        break;
      default:
        VIXL_UNIMPLEMENTED();
    }
    zd.SetUint(vform, i, result);
  }

  return zd;
}

void Simulator::SVEStructuredStoreHelper(VectorFormat vform,
                                         const LogicPRegister& pg,
                                         unsigned zt_code,
                                         const LogicSVEAddressVector& addr) {
  VIXL_ASSERT(zt_code < kNumberOfZRegisters);

  int esize_in_bytes_log2 = LaneSizeInBytesLog2FromFormat(vform);
  int msize_in_bytes_log2 = addr.GetMsizeInBytesLog2();
  int msize_in_bytes = addr.GetMsizeInBytes();
  int reg_count = addr.GetRegCount();

  VIXL_ASSERT(esize_in_bytes_log2 >= msize_in_bytes_log2);
  VIXL_ASSERT((reg_count >= 1) && (reg_count <= 4));

  unsigned zt_codes[4] = {zt_code,
                          (zt_code + 1) % kNumberOfZRegisters,
                          (zt_code + 2) % kNumberOfZRegisters,
                          (zt_code + 3) % kNumberOfZRegisters};

  LogicVRegister zt[4] = {
      ReadVRegister(zt_codes[0]),
      ReadVRegister(zt_codes[1]),
      ReadVRegister(zt_codes[2]),
      ReadVRegister(zt_codes[3]),
  };

  // For unpacked forms (e.g. `st1b { z0.h }, ...`, the upper parts of the lanes
  // are ignored, so read the source register using the VectorFormat that
  // corresponds with the storage format, and multiply the index accordingly.
  VectorFormat unpack_vform =
      SVEFormatFromLaneSizeInBytesLog2(msize_in_bytes_log2);
  int unpack_shift = esize_in_bytes_log2 - msize_in_bytes_log2;

  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    if (!pg.IsActive(vform, i)) continue;

    for (int r = 0; r < reg_count; r++) {
      uint64_t element_address = addr.GetElementAddress(i, r);
      StoreLane(zt[r], unpack_vform, i << unpack_shift, element_address);
    }
  }

  if (ShouldTraceWrites()) {
    PrintRegisterFormat format = GetPrintRegisterFormat(vform);
    if (esize_in_bytes_log2 == msize_in_bytes_log2) {
      // Use an FP format where it's likely that we're accessing FP data.
      format = GetPrintRegisterFormatTryFP(format);
    }
    // Stores don't represent a change to the source register's value, so only
    // print the relevant part of the value.
    format = GetPrintRegPartial(format);

    PrintZStructAccess(zt_code,
                       reg_count,
                       pg,
                       format,
                       msize_in_bytes,
                       "->",
                       addr);
  }
}

void Simulator::SVEStructuredLoadHelper(VectorFormat vform,
                                        const LogicPRegister& pg,
                                        unsigned zt_code,
                                        const LogicSVEAddressVector& addr,
                                        bool is_signed) {
  int esize_in_bytes_log2 = LaneSizeInBytesLog2FromFormat(vform);
  int msize_in_bytes_log2 = addr.GetMsizeInBytesLog2();
  int msize_in_bytes = addr.GetMsizeInBytes();
  int reg_count = addr.GetRegCount();

  VIXL_ASSERT(zt_code < kNumberOfZRegisters);
  VIXL_ASSERT(esize_in_bytes_log2 >= msize_in_bytes_log2);
  VIXL_ASSERT((reg_count >= 1) && (reg_count <= 4));

  unsigned zt_codes[4] = {zt_code,
                          (zt_code + 1) % kNumberOfZRegisters,
                          (zt_code + 2) % kNumberOfZRegisters,
                          (zt_code + 3) % kNumberOfZRegisters};
  LogicVRegister zt[4] = {
      ReadVRegister(zt_codes[0]),
      ReadVRegister(zt_codes[1]),
      ReadVRegister(zt_codes[2]),
      ReadVRegister(zt_codes[3]),
  };

  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    for (int r = 0; r < reg_count; r++) {
      uint64_t element_address = addr.GetElementAddress(i, r);

      if (!pg.IsActive(vform, i)) {
        zt[r].SetUint(vform, i, 0);
        continue;
      }

      if (is_signed) {
        LoadIntToLane(zt[r], vform, msize_in_bytes, i, element_address);
      } else {
        LoadUintToLane(zt[r], vform, msize_in_bytes, i, element_address);
      }
    }
  }

  if (ShouldTraceVRegs()) {
    PrintRegisterFormat format = GetPrintRegisterFormat(vform);
    if ((esize_in_bytes_log2 == msize_in_bytes_log2) && !is_signed) {
      // Use an FP format where it's likely that we're accessing FP data.
      format = GetPrintRegisterFormatTryFP(format);
    }
    PrintZStructAccess(zt_code,
                       reg_count,
                       pg,
                       format,
                       msize_in_bytes,
                       "<-",
                       addr);
  }
}

LogicPRegister Simulator::brka(LogicPRegister pd,
                               const LogicPRegister& pg,
                               const LogicPRegister& pn) {
  bool break_ = false;
  for (int i = 0; i < LaneCountFromFormat(kFormatVnB); i++) {
    if (pg.IsActive(kFormatVnB, i)) {
      pd.SetActive(kFormatVnB, i, !break_);
      break_ |= pn.IsActive(kFormatVnB, i);
    }
  }

  return pd;
}

LogicPRegister Simulator::brkb(LogicPRegister pd,
                               const LogicPRegister& pg,
                               const LogicPRegister& pn) {
  bool break_ = false;
  for (int i = 0; i < LaneCountFromFormat(kFormatVnB); i++) {
    if (pg.IsActive(kFormatVnB, i)) {
      break_ |= pn.IsActive(kFormatVnB, i);
      pd.SetActive(kFormatVnB, i, !break_);
    }
  }

  return pd;
}

LogicPRegister Simulator::brkn(LogicPRegister pdm,
                               const LogicPRegister& pg,
                               const LogicPRegister& pn) {
  if (!IsLastActive(kFormatVnB, pg, pn)) {
    pfalse(pdm);
  }
  return pdm;
}

LogicPRegister Simulator::brkpa(LogicPRegister pd,
                                const LogicPRegister& pg,
                                const LogicPRegister& pn,
                                const LogicPRegister& pm) {
  bool last_active = IsLastActive(kFormatVnB, pg, pn);

  for (int i = 0; i < LaneCountFromFormat(kFormatVnB); i++) {
    bool active = false;
    if (pg.IsActive(kFormatVnB, i)) {
      active = last_active;
      last_active = last_active && !pm.IsActive(kFormatVnB, i);
    }
    pd.SetActive(kFormatVnB, i, active);
  }

  return pd;
}

LogicPRegister Simulator::brkpb(LogicPRegister pd,
                                const LogicPRegister& pg,
                                const LogicPRegister& pn,
                                const LogicPRegister& pm) {
  bool last_active = IsLastActive(kFormatVnB, pg, pn);

  for (int i = 0; i < LaneCountFromFormat(kFormatVnB); i++) {
    bool active = false;
    if (pg.IsActive(kFormatVnB, i)) {
      last_active = last_active && !pm.IsActive(kFormatVnB, i);
      active = last_active;
    }
    pd.SetActive(kFormatVnB, i, active);
  }

  return pd;
}

void Simulator::SVEFaultTolerantLoadHelper(VectorFormat vform,
                                           const LogicPRegister& pg,
                                           unsigned zt_code,
                                           const LogicSVEAddressVector& addr,
                                           SVEFaultTolerantLoadType type,
                                           bool is_signed) {
  int esize_in_bytes = LaneSizeInBytesFromFormat(vform);
  int msize_in_bits = addr.GetMsizeInBits();
  int msize_in_bytes = addr.GetMsizeInBytes();

  VIXL_ASSERT(zt_code < kNumberOfZRegisters);
  VIXL_ASSERT(esize_in_bytes >= msize_in_bytes);
  VIXL_ASSERT(addr.GetRegCount() == 1);

  LogicVRegister zt = ReadVRegister(zt_code);
  LogicPRegister ffr = ReadFFR();

  // Non-faulting loads are allowed to fail arbitrarily. To stress user
  // code, fail a random element in roughly one in eight full-vector loads.
  uint32_t rnd = static_cast<uint32_t>(jrand48(rand_state_));
  int fake_fault_at_lane = rnd % (LaneCountFromFormat(vform) * 8);

  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    uint64_t value = 0;

    if (pg.IsActive(vform, i)) {
      uint64_t element_address = addr.GetElementAddress(i, 0);

      if (type == kSVEFirstFaultLoad) {
        // First-faulting loads always load the first active element, regardless
        // of FFR. The result will be discarded if its FFR lane is inactive, but
        // it could still generate a fault.
        value = MemReadUint(msize_in_bytes, element_address);
        // All subsequent elements have non-fault semantics.
        type = kSVENonFaultLoad;

      } else if (ffr.IsActive(vform, i)) {
        // Simulation of fault-tolerant loads relies on system calls, and is
        // likely to be relatively slow, so we only actually perform the load if
        // its FFR lane is active.

        bool can_read = (i < fake_fault_at_lane) &&
                        CanReadMemory(element_address, msize_in_bytes);
        if (can_read) {
          value = MemReadUint(msize_in_bytes, element_address);
        } else {
          // Propagate the fault to the end of FFR.
          for (int j = i; j < LaneCountFromFormat(vform); j++) {
            ffr.SetActive(vform, j, false);
          }
        }
      }
    }

    // The architecture permits a few possible results for inactive FFR lanes
    // (including those caused by a fault in this instruction). We choose to
    // leave the register value unchanged (like merging predication) because
    // no other input to this instruction can have the same behaviour.
    //
    // Note that this behaviour takes precedence over pg's zeroing predication.

    if (ffr.IsActive(vform, i)) {
      int msb = msize_in_bits - 1;
      if (is_signed) {
        zt.SetInt(vform, i, ExtractSignedBitfield64(msb, 0, value));
      } else {
        zt.SetUint(vform, i, ExtractUnsignedBitfield64(msb, 0, value));
      }
    }
  }

  if (ShouldTraceVRegs()) {
    PrintRegisterFormat format = GetPrintRegisterFormat(vform);
    if ((esize_in_bytes == msize_in_bytes) && !is_signed) {
      // Use an FP format where it's likely that we're accessing FP data.
      format = GetPrintRegisterFormatTryFP(format);
    }
    // Log accessed lanes that are active in both pg and ffr. PrintZStructAccess
    // expects a single mask, so combine the two predicates.
    SimPRegister mask;
    SVEPredicateLogicalHelper(AND_p_p_pp_z, mask, pg, ffr);
    PrintZStructAccess(zt_code, 1, mask, format, msize_in_bytes, "<-", addr);
  }
}

void Simulator::SVEGatherLoadScalarPlusVectorHelper(const Instruction* instr,
                                                    VectorFormat vform,
                                                    SVEOffsetModifier mod) {
  bool is_signed = instr->ExtractBit(14) == 0;
  bool is_ff = instr->ExtractBit(13) == 1;
  // Note that these instructions don't use the Dtype encoding.
  int msize_in_bytes_log2 = instr->ExtractBits(24, 23);
  int scale = instr->ExtractBit(21) * msize_in_bytes_log2;
  uint64_t base = ReadXRegister(instr->GetRn(), Reg31IsStackPointer);
  LogicSVEAddressVector addr(base,
                             &ReadVRegister(instr->GetRm()),
                             vform,
                             mod,
                             scale);
  addr.SetMsizeInBytesLog2(msize_in_bytes_log2);
  if (is_ff) {
    SVEFaultTolerantLoadHelper(vform,
                               ReadPRegister(instr->GetPgLow8()),
                               instr->GetRt(),
                               addr,
                               kSVEFirstFaultLoad,
                               is_signed);
  } else {
    SVEStructuredLoadHelper(vform,
                            ReadPRegister(instr->GetPgLow8()),
                            instr->GetRt(),
                            addr,
                            is_signed);
  }
}

int Simulator::GetFirstActive(VectorFormat vform,
                              const LogicPRegister& pg) const {
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    if (pg.IsActive(vform, i)) return i;
  }
  return -1;
}

int Simulator::GetLastActive(VectorFormat vform,
                             const LogicPRegister& pg) const {
  for (int i = LaneCountFromFormat(vform) - 1; i >= 0; i--) {
    if (pg.IsActive(vform, i)) return i;
  }
  return -1;
}

int Simulator::CountActiveLanes(VectorFormat vform,
                                const LogicPRegister& pg) const {
  int count = 0;
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    count += pg.IsActive(vform, i) ? 1 : 0;
  }
  return count;
}

int Simulator::CountActiveAndTrueLanes(VectorFormat vform,
                                       const LogicPRegister& pg,
                                       const LogicPRegister& pn) const {
  int count = 0;
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    count += (pg.IsActive(vform, i) && pn.IsActive(vform, i)) ? 1 : 0;
  }
  return count;
}

int Simulator::GetPredicateConstraintLaneCount(VectorFormat vform,
                                               int pattern) const {
  VIXL_ASSERT(IsSVEFormat(vform));
  int all = LaneCountFromFormat(vform);
  VIXL_ASSERT(all > 0);

  switch (pattern) {
    case SVE_VL1:
    case SVE_VL2:
    case SVE_VL3:
    case SVE_VL4:
    case SVE_VL5:
    case SVE_VL6:
    case SVE_VL7:
    case SVE_VL8:
      // VL1-VL8 are encoded directly.
      VIXL_STATIC_ASSERT(SVE_VL1 == 1);
      VIXL_STATIC_ASSERT(SVE_VL8 == 8);
      return (pattern <= all) ? pattern : 0;
    case SVE_VL16:
    case SVE_VL32:
    case SVE_VL64:
    case SVE_VL128:
    case SVE_VL256: {
      // VL16-VL256 are encoded as log2(N) + c.
      int min = 16 << (pattern - SVE_VL16);
      return (min <= all) ? min : 0;
    }
    // Special cases.
    case SVE_POW2:
      return 1 << HighestSetBitPosition(all);
    case SVE_MUL4:
      return all - (all % 4);
    case SVE_MUL3:
      return all - (all % 3);
    case SVE_ALL:
      return all;
  }
  // Unnamed cases architecturally return 0.
  return 0;
}

LogicPRegister Simulator::match(VectorFormat vform,
                                LogicPRegister dst,
                                const LogicVRegister& haystack,
                                const LogicVRegister& needles,
                                bool negate_match) {
  SimVRegister ztemp;
  SimPRegister ptemp;

  pfalse(dst);
  int lanes_per_segment = kQRegSize / LaneSizeInBitsFromFormat(vform);
  for (int i = 0; i < lanes_per_segment; i++) {
    dup_elements_to_segments(vform, ztemp, needles, i);
    SVEIntCompareVectorsHelper(eq,
                               vform,
                               ptemp,
                               GetPTrue(),
                               haystack,
                               ztemp,
                               false,
                               LeaveFlags);
    SVEPredicateLogicalHelper(ORR_p_p_pp_z, dst, dst, ptemp);
  }
  if (negate_match) {
    ptrue(vform, ptemp, SVE_ALL);
    SVEPredicateLogicalHelper(EOR_p_p_pp_z, dst, dst, ptemp);
  }
  return dst;
}

uint64_t LogicSVEAddressVector::GetStructAddress(int lane) const {
  if (IsContiguous()) {
    return base_ + (lane * GetRegCount()) * GetMsizeInBytes();
  }

  VIXL_ASSERT(IsScatterGather());
  VIXL_ASSERT(vector_ != NULL);

  // For scatter-gather accesses, we need to extract the offset from vector_,
  // and apply modifiers.

  uint64_t offset = 0;
  switch (vector_form_) {
    case kFormatVnS:
      offset = vector_->GetLane<uint32_t>(lane);
      break;
    case kFormatVnD:
      offset = vector_->GetLane<uint64_t>(lane);
      break;
    default:
      VIXL_UNIMPLEMENTED();
      break;
  }

  switch (vector_mod_) {
    case SVE_MUL_VL:
      VIXL_UNIMPLEMENTED();
      break;
    case SVE_LSL:
      // We apply the shift below. There's nothing to do here.
      break;
    case NO_SVE_OFFSET_MODIFIER:
      VIXL_ASSERT(vector_shift_ == 0);
      break;
    case SVE_UXTW:
      offset = ExtractUnsignedBitfield64(kWRegSize - 1, 0, offset);
      break;
    case SVE_SXTW:
      offset = ExtractSignedBitfield64(kWRegSize - 1, 0, offset);
      break;
  }

  return base_ + (offset << vector_shift_);
}

LogicVRegister Simulator::pack_odd_elements(VectorFormat vform,
                                            LogicVRegister dst,
                                            const LogicVRegister& src) {
  SimVRegister zero;
  zero.Clear();
  return uzp2(vform, dst, src, zero);
}

LogicVRegister Simulator::pack_even_elements(VectorFormat vform,
                                             LogicVRegister dst,
                                             const LogicVRegister& src) {
  SimVRegister zero;
  zero.Clear();
  return uzp1(vform, dst, src, zero);
}

LogicVRegister Simulator::adcl(VectorFormat vform,
                               LogicVRegister dst,
                               const LogicVRegister& src1,
                               const LogicVRegister& src2,
                               bool top) {
  unsigned reg_size = LaneSizeInBitsFromFormat(vform);
  VIXL_ASSERT((reg_size == kSRegSize) || (reg_size == kDRegSize));

  for (int i = 0; i < LaneCountFromFormat(vform); i += 2) {
    uint64_t left = src1.Uint(vform, i + (top ? 1 : 0));
    uint64_t right = dst.Uint(vform, i);
    unsigned carry_in = src2.Uint(vform, i + 1) & 1;
    std::pair<uint64_t, uint8_t> val_and_flags =
        AddWithCarry(reg_size, left, right, carry_in);

    // Set even lanes to the result of the addition.
    dst.SetUint(vform, i, val_and_flags.first);

    // Set odd lanes to the carry flag from the addition.
    uint64_t carry_out = (val_and_flags.second >> 1) & 1;
    dst.SetUint(vform, i + 1, carry_out);
  }
  return dst;
}

// Multiply the 2x8 8-bit matrix in src1 by the 8x2 8-bit matrix in src2, add
// the 2x2 32-bit result to the matrix in srcdst, and write back to srcdst.
//
// Matrices of the form:
//
//  src1 = ( a b c d e f g h )  src2 = ( A B )
//         ( i j k l m n o p )         ( C D )
//                                     ( E F )
//                                     ( G H )
//                                     ( I J )
//                                     ( K L )
//                                     ( M N )
//                                     ( O P )
//
// Are stored in the input vector registers as:
//
//           15  14  13  12  11  10  9   8   7   6   5   4   3   2   1   0
//  src1 = [ p | o | n | m | l | k | j | i | h | g | f | e | d | c | b | a ]
//  src2 = [ P | N | L | J | H | F | D | B | O | M | K | I | G | E | C | A ]
//
LogicVRegister Simulator::matmul(VectorFormat vform_dst,
                                 LogicVRegister srcdst,
                                 const LogicVRegister& src1,
                                 const LogicVRegister& src2,
                                 bool src1_signed,
                                 bool src2_signed) {
  // Two destination forms are supported: Q register containing four S-sized
  // elements (4S) and Z register containing n S-sized elements (VnS).
  VIXL_ASSERT((vform_dst == kFormat4S) || (vform_dst == kFormatVnS));
  VectorFormat vform_src = kFormatVnB;
  int b_per_segment = kQRegSize / kBRegSize;
  int s_per_segment = kQRegSize / kSRegSize;
  int64_t result[kZRegMaxSizeInBytes / kSRegSizeInBytes] = {};
  int segment_count = LaneCountFromFormat(vform_dst) / 4;
  for (int seg = 0; seg < segment_count; seg++) {
    for (int i = 0; i < 2; i++) {
      for (int j = 0; j < 2; j++) {
        int dstidx = (2 * i) + j + (seg * s_per_segment);
        int64_t sum = srcdst.Int(vform_dst, dstidx);
        for (int k = 0; k < 8; k++) {
          int idx1 = (8 * i) + k + (seg * b_per_segment);
          int idx2 = (8 * j) + k + (seg * b_per_segment);
          int64_t e1 = src1_signed ? src1.Int(vform_src, idx1)
                                   : src1.Uint(vform_src, idx1);
          int64_t e2 = src2_signed ? src2.Int(vform_src, idx2)
                                   : src2.Uint(vform_src, idx2);
          sum += e1 * e2;
        }
        result[dstidx] = sum;
      }
    }
  }
  srcdst.SetIntArray(vform_dst, result);
  return srcdst;
}

// Multiply the 2x2 FP matrix in src1 by the 2x2 FP matrix in src2, add the 2x2
// result to the matrix in srcdst, and write back to srcdst.
//
// Matrices of the form:
//
//  src1 = ( a b )  src2 = ( A B )
//         ( c d )         ( C D )
//
// Are stored in the input vector registers as:
//
//           3   2   1   0
//  src1 = [ d | c | b | a ]
//  src2 = [ D | B | C | A ]
//
template <typename T>
LogicVRegister Simulator::fmatmul(VectorFormat vform,
                                  LogicVRegister srcdst,
                                  const LogicVRegister& src1,
                                  const LogicVRegister& src2) {
  T result[kZRegMaxSizeInBytes / sizeof(T)];
  int T_per_segment = 4;
  int segment_count = GetVectorLengthInBytes() / (T_per_segment * sizeof(T));
  for (int seg = 0; seg < segment_count; seg++) {
    int segoff = seg * T_per_segment;
    for (int i = 0; i < 2; i++) {
      for (int j = 0; j < 2; j++) {
        T prod0 = FPMulNaNs(src1.Float<T>(2 * i + 0 + segoff),
                            src2.Float<T>(2 * j + 0 + segoff));
        T prod1 = FPMulNaNs(src1.Float<T>(2 * i + 1 + segoff),
                            src2.Float<T>(2 * j + 1 + segoff));
        T sum = FPAdd(srcdst.Float<T>(2 * i + j + segoff), prod0);
        result[2 * i + j + segoff] = FPAdd(sum, prod1);
      }
    }
  }
  for (int i = 0; i < LaneCountFromFormat(vform); i++) {
    // Elements outside a multiple of 4T are set to zero. This happens only
    // for double precision operations, when the VL is a multiple of 128 bits,
    // but not a multiple of 256 bits.
    T value = (i < (T_per_segment * segment_count)) ? result[i] : 0;
    srcdst.SetFloat<T>(vform, i, value);
  }
  return srcdst;
}

LogicVRegister Simulator::fmatmul(VectorFormat vform,
                                  LogicVRegister dst,
                                  const LogicVRegister& src1,
                                  const LogicVRegister& src2) {
  if (LaneSizeInBitsFromFormat(vform) == kSRegSize) {
    fmatmul<float>(vform, dst, src1, src2);
  } else {
    VIXL_ASSERT(LaneSizeInBitsFromFormat(vform) == kDRegSize);
    fmatmul<double>(vform, dst, src1, src2);
  }
  return dst;
}

}  // namespace aarch64
}  // namespace vixl

#endif  // VIXL_INCLUDE_SIMULATOR_AARCH64
