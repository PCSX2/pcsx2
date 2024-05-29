// Copyright 2017, VIXL authors
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

#include "code-buffer-vixl.h"
#include "utils-vixl.h"

namespace vixl {


CodeBuffer::CodeBuffer(byte* buffer, size_t capacity)
    : buffer_(reinterpret_cast<byte*>(buffer)),
      cursor_(reinterpret_cast<byte*>(buffer)),
      dirty_(false),
      capacity_(capacity) {
  VIXL_ASSERT(buffer_ != NULL);
}


CodeBuffer::~CodeBuffer() VIXL_NEGATIVE_TESTING_ALLOW_EXCEPTION {
  VIXL_ASSERT(!IsDirty());
}


void CodeBuffer::EmitString(const char* string) {
  const auto len = strlen(string) + 1;
  VIXL_ASSERT(HasSpaceFor(len));
  char* dst = reinterpret_cast<char*>(cursor_);
  dirty_ = true;
  memcpy(dst, string, len);
  cursor_ = reinterpret_cast<byte*>(dst + len);
}


void CodeBuffer::EmitData(const void* data, size_t size) {
  VIXL_ASSERT(HasSpaceFor(size));
  dirty_ = true;
  memcpy(cursor_, data, size);
  cursor_ = cursor_ + size;
}


void CodeBuffer::UpdateData(size_t offset, const void* data, size_t size) {
  dirty_ = true;
  byte* dst = buffer_ + offset;
  VIXL_ASSERT(dst + size <= cursor_);
  memcpy(dst, data, size);
}


void CodeBuffer::Align() {
  byte* end = AlignUp(cursor_, 4);
  const size_t padding_size = end - cursor_;
  VIXL_ASSERT(padding_size <= 4);
  EmitZeroedBytes(static_cast<int>(padding_size));
}

void CodeBuffer::EmitZeroedBytes(int n) {
  VIXL_ASSERT(HasSpaceFor(n));
  dirty_ = true;
  memset(cursor_, 0, n);
  cursor_ += n;
}

void CodeBuffer::Reset() {
  cursor_ = buffer_;
  SetClean();
}


}  // namespace vixl
