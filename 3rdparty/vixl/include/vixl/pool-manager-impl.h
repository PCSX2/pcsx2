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

#ifndef VIXL_POOL_MANAGER_IMPL_H_
#define VIXL_POOL_MANAGER_IMPL_H_

#include "pool-manager.h"

#include <algorithm>
#include "assembler-base-vixl.h"

namespace vixl {


template <typename T>
T PoolManager<T>::Emit(MacroAssemblerInterface* masm,
                       T pc,
                       int num_bytes,
                       ForwardReference<T>* new_reference,
                       LocationBase<T>* new_object,
                       EmitOption option) {
  // Make sure that the buffer still has the alignment we think it does.
  VIXL_ASSERT(IsAligned(masm->AsAssemblerBase()
                            ->GetBuffer()
                            ->GetStartAddress<uintptr_t>(),
                        buffer_alignment_));

  // We should not call this method when the pools are blocked.
  VIXL_ASSERT(!IsBlocked());
  if (objects_.empty()) return pc;

  // Emit header.
  if (option == kBranchRequired) {
    masm->EmitPoolHeader();
    // TODO: The pc at this point might not actually be aligned according to
    // alignment_. This is to support the current AARCH32 MacroAssembler which
    // does not have a fixed size instruction set. In practice, the pc will be
    // aligned to the alignment instructions need for the current instruction
    // set, so we do not need to align it here. All other calculations do take
    // the alignment into account, which only makes the checkpoint calculations
    // more conservative when we use T32. Uncomment the following assertion if
    // the AARCH32 MacroAssembler is modified to only support one ISA at the
    // time.
    // VIXL_ASSERT(pc == AlignUp(pc, alignment_));
    pc += header_size_;
  } else {
    // If the header is optional, we might need to add some extra padding to
    // meet the minimum location of the first object.
    if (pc < objects_[0].min_location_) {
      int32_t padding = objects_[0].min_location_ - pc;
      masm->EmitNopBytes(padding);
      pc += padding;
    }
  }

  PoolObject<T>* existing_object = GetObjectIfTracked(new_object);

  // Go through all objects and emit one by one.
  for (objects_iter iter = objects_.begin(); iter != objects_.end();) {
    PoolObject<T>& current = *iter;
    if (ShouldSkipObject(&current,
                         pc,
                         num_bytes,
                         new_reference,
                         new_object,
                         existing_object)) {
      ++iter;
      continue;
    }
    LocationBase<T>* label_base = current.label_base_;
    T aligned_pc = AlignUp(pc, current.alignment_);
    masm->EmitPaddingBytes(aligned_pc - pc);
    pc = aligned_pc;
    VIXL_ASSERT(pc >= current.min_location_);
    VIXL_ASSERT(pc <= current.max_location_);
    // First call SetLocation, which will also resolve the references, and then
    // call EmitPoolObject, which might add a new reference.
    label_base->SetLocation(masm->AsAssemblerBase(), pc);
    label_base->EmitPoolObject(masm);
    int object_size = label_base->GetPoolObjectSizeInBytes();
    if (label_base->ShouldDeletePoolObjectOnPlacement()) {
      label_base->MarkBound();
      iter = RemoveAndDelete(iter);
    } else {
      VIXL_ASSERT(!current.label_base_->ShouldDeletePoolObjectOnPlacement());
      current.label_base_->UpdatePoolObject(&current);
      VIXL_ASSERT(current.alignment_ >= label_base->GetPoolObjectAlignment());
      ++iter;
    }
    pc += object_size;
  }

  // Recalculate the checkpoint before emitting the footer. The footer might
  // call Bind() which will check if we need to emit.
  RecalculateCheckpoint();

  // Always emit footer - this might add some padding.
  masm->EmitPoolFooter();
  pc = AlignUp(pc, alignment_);

  return pc;
}

template <typename T>
bool PoolManager<T>::ShouldSkipObject(PoolObject<T>* pool_object,
                                      T pc,
                                      int num_bytes,
                                      ForwardReference<T>* new_reference,
                                      LocationBase<T>* new_object,
                                      PoolObject<T>* existing_object) const {
  // We assume that all objects before this have been skipped and all objects
  // after this will be emitted, therefore we will emit the whole pool. Add
  // the header size and alignment, as well as the number of bytes we are
  // planning to emit.
  T max_actual_location = pc + num_bytes + max_pool_size_;

  if (new_reference != NULL) {
    // If we're adding a new object, also assume that it will have to be emitted
    // before the object we are considering to skip.
    VIXL_ASSERT(new_object != NULL);
    T new_object_alignment = std::max(new_reference->object_alignment_,
                                      new_object->GetPoolObjectAlignment());
    if ((existing_object != NULL) &&
        (existing_object->alignment_ > new_object_alignment)) {
      new_object_alignment = existing_object->alignment_;
    }
    max_actual_location +=
        (new_object->GetPoolObjectSizeInBytes() + new_object_alignment - 1);
  }

  // Hard limit.
  if (max_actual_location >= pool_object->max_location_) return false;

  // Use heuristic.
  return (pc < pool_object->skip_until_location_hint_);
}

template <typename T>
T PoolManager<T>::UpdateCheckpointForObject(T checkpoint,
                                            const PoolObject<T>* object) {
  checkpoint -= object->label_base_->GetPoolObjectSizeInBytes();
  if (checkpoint > object->max_location_) checkpoint = object->max_location_;
  checkpoint = AlignDown(checkpoint, object->alignment_);
  return checkpoint;
}

template <typename T>
static T MaxCheckpoint() {
  return std::numeric_limits<T>::max();
}

template <typename T>
static inline bool CheckCurrentPC(T pc, T checkpoint) {
  VIXL_ASSERT(pc <= checkpoint);
  // We must emit the pools if we are at the checkpoint now.
  return pc == checkpoint;
}

template <typename T>
static inline bool CheckFuturePC(T pc, T checkpoint) {
  // We do not need to emit the pools now if the projected future PC will be
  // equal to the checkpoint (we will need to emit the pools then).
  return pc > checkpoint;
}

template <typename T>
bool PoolManager<T>::MustEmit(T pc,
                              int num_bytes,
                              ForwardReference<T>* reference,
                              LocationBase<T>* label_base) const {
  // Check if we are at or past the checkpoint.
  if (CheckCurrentPC(pc, checkpoint_)) return true;

  // Check if the future PC will be past the checkpoint.
  pc += num_bytes;
  if (CheckFuturePC(pc, checkpoint_)) return true;

  // No new reference - nothing to do.
  if (reference == NULL) {
    VIXL_ASSERT(label_base == NULL);
    return false;
  }

  if (objects_.empty()) {
    // Basic assertions that restrictions on the new (and only) reference are
    // possible to satisfy.
    VIXL_ASSERT(AlignUp(pc + header_size_, alignment_) >=
                reference->min_object_location_);
    VIXL_ASSERT(pc <= reference->max_object_location_);
    return false;
  }

  // Check if the object is already being tracked.
  const PoolObject<T>* existing_object = GetObjectIfTracked(label_base);
  if (existing_object != NULL) {
    // If the existing_object is already in existing_objects_ and its new
    // alignment and new location restrictions are not stricter, skip the more
    // expensive check.
    if ((reference->min_object_location_ <= existing_object->min_location_) &&
        (reference->max_object_location_ >= existing_object->max_location_) &&
        (reference->object_alignment_ <= existing_object->alignment_)) {
      return false;
    }
  }

  // Create a temporary object.
  PoolObject<T> temp(label_base);
  temp.RestrictRange(reference->min_object_location_,
                     reference->max_object_location_);
  temp.RestrictAlignment(reference->object_alignment_);
  if (existing_object != NULL) {
    temp.RestrictRange(existing_object->min_location_,
                       existing_object->max_location_);
    temp.RestrictAlignment(existing_object->alignment_);
  }

  // Check if the new reference can be added after the end of the current pool.
  // If yes, we don't need to emit.
  T last_reachable = AlignDown(temp.max_location_, temp.alignment_);
  const PoolObject<T>& last = objects_.back();
  T after_pool = AlignDown(last.max_location_, last.alignment_) +
                 last.label_base_->GetPoolObjectSizeInBytes();
  // The current object can be placed at the end of the pool, even if the last
  // object is placed at the last possible location.
  if (last_reachable >= after_pool) return false;
  // The current object can be placed after the code we are about to emit and
  // after the existing pool (with a pessimistic size estimate).
  if (last_reachable >= pc + num_bytes + max_pool_size_) return false;

  // We're not in a trivial case, so we need to recalculate the checkpoint.

  // Check (conservatively) if we can fit it into the objects_ array, without
  // breaking our assumptions. Here we want to recalculate the checkpoint as
  // if the new reference was added to the PoolManager but without actually
  // adding it (as removing it is non-trivial).

  T checkpoint = MaxCheckpoint<T>();
  // Will temp be the last object in objects_?
  if (PoolObjectLessThan(last, temp)) {
    checkpoint = UpdateCheckpointForObject(checkpoint, &temp);
    if (checkpoint < temp.min_location_) return true;
  }

  bool temp_not_placed_yet = true;
  for (int i = static_cast<int>(objects_.size()) - 1; i >= 0; --i) {
    const PoolObject<T>& current = objects_[i];
    if (temp_not_placed_yet && PoolObjectLessThan(current, temp)) {
      checkpoint = UpdateCheckpointForObject(checkpoint, &temp);
      if (checkpoint < temp.min_location_) return true;
      if (CheckFuturePC(pc, checkpoint)) return true;
      temp_not_placed_yet = false;
    }
    if (current.label_base_ == label_base) continue;
    checkpoint = UpdateCheckpointForObject(checkpoint, &current);
    if (checkpoint < current.min_location_) return true;
    if (CheckFuturePC(pc, checkpoint)) return true;
  }
  // temp is the object with the smallest max_location_.
  if (temp_not_placed_yet) {
    checkpoint = UpdateCheckpointForObject(checkpoint, &temp);
    if (checkpoint < temp.min_location_) return true;
  }

  // Take the header into account.
  checkpoint -= header_size_;
  checkpoint = AlignDown(checkpoint, alignment_);

  return CheckFuturePC(pc, checkpoint);
}

template <typename T>
void PoolManager<T>::RecalculateCheckpoint(SortOption sort_option) {
  // TODO: Improve the max_pool_size_ estimate by starting from the
  // min_location_ of the first object, calculating the end of the pool as if
  // all objects were placed starting from there, and in the end adding the
  // maximum object alignment found minus one (which is the maximum extra
  // padding we would need if we were to relocate the pool to a different
  // address).
  max_pool_size_ = 0;

  if (objects_.empty()) {
    checkpoint_ = MaxCheckpoint<T>();
    return;
  }

  // Sort objects by their max_location_.
  if (sort_option == kSortRequired) {
    std::sort(objects_.begin(), objects_.end(), PoolObjectLessThan);
  }

  // Add the header size and header and footer max alignment to the maximum
  // pool size.
  max_pool_size_ += header_size_ + 2 * (alignment_ - 1);

  T checkpoint = MaxCheckpoint<T>();
  int last_object_index = static_cast<int>(objects_.size()) - 1;
  for (int i = last_object_index; i >= 0; --i) {
    // Bring back the checkpoint by the size of the current object, unless
    // we need to bring it back more, then align.
    PoolObject<T>& current = objects_[i];
    checkpoint = UpdateCheckpointForObject(checkpoint, &current);
    VIXL_ASSERT(checkpoint >= current.min_location_);
    max_pool_size_ += (current.alignment_ - 1 +
                       current.label_base_->GetPoolObjectSizeInBytes());
  }
  // Take the header into account.
  checkpoint -= header_size_;
  checkpoint = AlignDown(checkpoint, alignment_);

  // Update the checkpoint of the pool manager.
  checkpoint_ = checkpoint;

  // NOTE: To handle min_location_ in the generic case, we could make a second
  // pass of the objects_ vector, increasing the checkpoint as needed, while
  // maintaining the alignment requirements.
  // It should not be possible to have any issues with min_location_ with actual
  // code, since there should always be some kind of branch over the pool,
  // whether introduced by the pool emission or by the user, which will make
  // sure the min_location_ requirement is satisfied. It's possible that the
  // user could emit code in the literal pool and intentionally load the first
  // value and then fall-through into the pool, but that is not a supported use
  // of VIXL and we will assert in that case.
}

template <typename T>
bool PoolManager<T>::PoolObjectLessThan(const PoolObject<T>& a,
                                        const PoolObject<T>& b) {
  if (a.max_location_ != b.max_location_)
    return (a.max_location_ < b.max_location_);
  int a_size = a.label_base_->GetPoolObjectSizeInBytes();
  int b_size = b.label_base_->GetPoolObjectSizeInBytes();
  if (a_size != b_size) return (a_size < b_size);
  if (a.alignment_ != b.alignment_) return (a.alignment_ < b.alignment_);
  if (a.min_location_ != b.min_location_)
    return (a.min_location_ < b.min_location_);
  return false;
}

template <typename T>
void PoolManager<T>::AddObjectReference(const ForwardReference<T>* reference,
                                        LocationBase<T>* label_base) {
  VIXL_ASSERT(reference->object_alignment_ <= buffer_alignment_);
  VIXL_ASSERT(label_base->GetPoolObjectAlignment() <= buffer_alignment_);

  PoolObject<T>* object = GetObjectIfTracked(label_base);

  if (object == NULL) {
    PoolObject<T> new_object(label_base);
    new_object.RestrictRange(reference->min_object_location_,
                             reference->max_object_location_);
    new_object.RestrictAlignment(reference->object_alignment_);
    Insert(new_object);
  } else {
    object->RestrictRange(reference->min_object_location_,
                          reference->max_object_location_);
    object->RestrictAlignment(reference->object_alignment_);

    // Move the object, if needed.
    if (objects_.size() != 1) {
      PoolObject<T> new_object(*object);
      ptrdiff_t distance = std::distance(objects_.data(), object);
      objects_.erase(objects_.begin() + distance);
      Insert(new_object);
    }
  }
  // No need to sort, we inserted the object in an already sorted array.
  RecalculateCheckpoint(kNoSortRequired);
}

template <typename T>
void PoolManager<T>::Insert(const PoolObject<T>& new_object) {
  bool inserted = false;
  // Place the object in the right position.
  for (objects_iter iter = objects_.begin(); iter != objects_.end(); ++iter) {
    PoolObject<T>& current = *iter;
    if (!PoolObjectLessThan(current, new_object)) {
      objects_.insert(iter, new_object);
      inserted = true;
      break;
    }
  }
  if (!inserted) {
    objects_.push_back(new_object);
  }
}

template <typename T>
void PoolManager<T>::RemoveAndDelete(PoolObject<T>* object) {
  for (objects_iter iter = objects_.begin(); iter != objects_.end(); ++iter) {
    PoolObject<T>& current = *iter;
    if (current.label_base_ == object->label_base_) {
      (void)RemoveAndDelete(iter);
      return;
    }
  }
  VIXL_UNREACHABLE();
}

template <typename T>
typename PoolManager<T>::objects_iter PoolManager<T>::RemoveAndDelete(
    objects_iter iter) {
  PoolObject<T>& object = *iter;
  LocationBase<T>* label_base = object.label_base_;

  // Check if we also need to delete the LocationBase object.
  if (label_base->ShouldBeDeletedOnPoolManagerDestruction()) {
    delete_on_destruction_.push_back(label_base);
  }
  if (label_base->ShouldBeDeletedOnPlacementByPoolManager()) {
    VIXL_ASSERT(!label_base->ShouldBeDeletedOnPoolManagerDestruction());
    delete label_base;
  }

  return objects_.erase(iter);
}

template <typename T>
T PoolManager<T>::Bind(MacroAssemblerInterface* masm,
                       LocationBase<T>* object,
                       T location) {
  PoolObject<T>* existing_object = GetObjectIfTracked(object);
  int alignment;
  T min_location;
  if (existing_object == NULL) {
    alignment = object->GetMaxAlignment();
    min_location = object->GetMinLocation();
  } else {
    alignment = existing_object->alignment_;
    min_location = existing_object->min_location_;
  }

  // Align if needed, and add necessary padding to reach the min_location_.
  T aligned_location = AlignUp(location, alignment);
  masm->EmitNopBytes(aligned_location - location);
  location = aligned_location;
  while (location < min_location) {
    masm->EmitNopBytes(alignment);
    location += alignment;
  }

  object->SetLocation(masm->AsAssemblerBase(), location);
  object->MarkBound();

  if (existing_object != NULL) {
    RemoveAndDelete(existing_object);
    // No need to sort, we removed the object from a sorted array.
    RecalculateCheckpoint(kNoSortRequired);
  }

  // We assume that the maximum padding we can possibly add here is less
  // than the header alignment - hence that we're not going to go past our
  // checkpoint.
  VIXL_ASSERT(!CheckFuturePC(location, checkpoint_));
  return location;
}

template <typename T>
void PoolManager<T>::Release(T pc) {
  USE(pc);
  if (--monitor_ == 0) {
    // Ensure the pool has not been blocked for too long.
    VIXL_ASSERT(pc <= checkpoint_);
  }
}

template <typename T>
PoolManager<T>::~PoolManager<T>() VIXL_NEGATIVE_TESTING_ALLOW_EXCEPTION {
#ifdef VIXL_DEBUG
  // Check for unbound objects.
  for (objects_iter iter = objects_.begin(); iter != objects_.end(); ++iter) {
    // There should not be any bound objects left in the pool. For unbound
    // objects, we will check in the destructor of the object itself.
    VIXL_ASSERT(!(*iter).label_base_->IsBound());
  }
#endif
  // Delete objects the pool manager owns.
  for (typename std::vector<LocationBase<T>*>::iterator
           iter = delete_on_destruction_.begin(),
           end = delete_on_destruction_.end();
       iter != end;
       ++iter) {
    delete *iter;
  }
}

template <typename T>
int PoolManager<T>::GetPoolSizeForTest() const {
  // Iterate over objects and return their cumulative size. This does not take
  // any padding into account, just the size of the objects themselves.
  int size = 0;
  for (const_objects_iter iter = objects_.begin(); iter != objects_.end();
       ++iter) {
    size += (*iter).label_base_->GetPoolObjectSizeInBytes();
  }
  return size;
}
}

#endif  // VIXL_POOL_MANAGER_IMPL_H_
