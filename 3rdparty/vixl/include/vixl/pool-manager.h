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

#ifndef VIXL_POOL_MANAGER_H_
#define VIXL_POOL_MANAGER_H_

#include <cstddef>
#include <limits>
#include <map>
#include <stdint.h>
#include <vector>

#include "globals-vixl.h"
#include "macro-assembler-interface.h"
#include "utils-vixl.h"

namespace vixl {

class TestPoolManager;

// There are four classes declared in this header file:
// PoolManager, PoolObject, ForwardReference and LocationBase.

// The PoolManager manages both literal and veneer pools, and is designed to be
// shared between AArch32 and AArch64. A pool is represented as an abstract
// collection of references to objects. The manager does not need to know
// architecture-specific details about literals and veneers; the actual
// emission of the pool objects is delegated.
//
// Literal and Label will derive from LocationBase. The MacroAssembler will
// create these objects as instructions that reference pool objects are
// encountered, and ask the PoolManager to track them. The PoolManager will
// create an internal PoolObject object for each object derived from
// LocationBase.  Some of these PoolObject objects will be deleted when placed
// (e.g. the ones corresponding to Literals), whereas others will be updated
// with a new range when placed (e.g.  Veneers) and deleted when Bind() is
// called on the PoolManager with their corresponding object as a parameter.
//
// A ForwardReference represents a reference to a PoolObject that will be
// placed later in the instruction stream. Each ForwardReference may only refer
// to one PoolObject, but many ForwardReferences may refer to the same
// object.
//
// A PoolObject represents an object that has not yet been placed.  The final
// location of a PoolObject (and hence the LocationBase object to which it
// corresponds) is constrained mostly by the instructions that refer to it, but
// PoolObjects can also have inherent constraints, such as alignment.
//
// LocationBase objects, unlike PoolObject objects, can be used outside of the
// pool manager (e.g. as manually placed literals, which may still have
// forward references that need to be resolved).
//
// At the moment, each LocationBase will have at most one PoolObject that keeps
// the relevant information for placing this object in the pool. When that
// object is placed, all forward references of the object are resolved. For
// that reason, we do not need to keep track of the ForwardReference objects in
// the PoolObject.

// T is an integral type used for representing locations. For a 32-bit
// architecture it will typically be int32_t, whereas for a 64-bit
// architecture it will be int64_t.
template <typename T>
class ForwardReference;
template <typename T>
class PoolObject;
template <typename T>
class PoolManager;

// Represents an object that has a size and alignment, and either has a known
// location or has not been placed yet. An object of a subclass of LocationBase
// will typically keep track of a number of ForwardReferences when it has not
// yet been placed, but LocationBase does not assume or implement that
// functionality.  LocationBase provides virtual methods for emitting the
// object, updating all the forward references, and giving the PoolManager
// information on the lifetime of this object and the corresponding PoolObject.
template <typename T>
class LocationBase {
 public:
  // The size of a LocationBase object is restricted to 4KB, in order to avoid
  // situations where the size of the pool becomes larger than the range of
  // an unconditional branch. This cannot happen without having large objects,
  // as typically the range of an unconditional branch is the larger range
  // an instruction supports.
  // TODO: This would ideally be an architecture-specific value, perhaps
  // another template parameter.
  static const int kMaxObjectSize = 4 * KBytes;

  // By default, LocationBase objects are aligned naturally to their size.
  LocationBase(uint32_t type, int size)
      : pool_object_size_(size),
        pool_object_alignment_(size),
        pool_object_type_(type),
        is_bound_(false),
        location_(0) {
    VIXL_ASSERT(size > 0);
    VIXL_ASSERT(size <= kMaxObjectSize);
    VIXL_ASSERT(IsPowerOf2(size));
  }

  // Allow alignment to be specified, as long as it is smaller than the size.
  LocationBase(uint32_t type, int size, int alignment)
      : pool_object_size_(size),
        pool_object_alignment_(alignment),
        pool_object_type_(type),
        is_bound_(false),
        location_(0) {
    VIXL_ASSERT(size > 0);
    VIXL_ASSERT(size <= kMaxObjectSize);
    VIXL_ASSERT(IsPowerOf2(alignment));
    VIXL_ASSERT(alignment <= size);
  }

  // Constructor for locations that are already bound.
  explicit LocationBase(T location)
      : pool_object_size_(-1),
        pool_object_alignment_(-1),
        pool_object_type_(0),
        is_bound_(true),
        location_(location) {}

  virtual ~LocationBase() VIXL_NEGATIVE_TESTING_ALLOW_EXCEPTION {}

  // The PoolManager should assume ownership of some objects, and delete them
  // after they have been placed. This can happen for example for literals that
  // are created internally to the MacroAssembler and the user doesn't get a
  // handle to. By default, the PoolManager will not do this.
  virtual bool ShouldBeDeletedOnPlacementByPoolManager() const { return false; }
  // The PoolManager should assume ownership of some objects, and delete them
  // when it is destroyed. By default, the PoolManager will not do this.
  virtual bool ShouldBeDeletedOnPoolManagerDestruction() const { return false; }

  // Emit the PoolObject. Derived classes will implement this method to emit
  // the necessary data and/or code (for example, to emit a literal or a
  // veneer). This should not add padding, as it is added explicitly by the pool
  // manager.
  virtual void EmitPoolObject(MacroAssemblerInterface* masm) = 0;

  // Resolve the references to this object. Will encode the necessary offset
  // in the instruction corresponding to each reference and then delete it.
  // TODO: An alternative here would be to provide a ResolveReference()
  // method that only asks the LocationBase to resolve a specific reference
  // (thus allowing the pool manager to resolve some of the references only).
  // This would mean we need to have some kind of API to get all the references
  // to a LabelObject.
  virtual void ResolveReferences(internal::AssemblerBase* assembler) = 0;

  // Returns true when the PoolObject corresponding to this LocationBase object
  // needs to be removed from the pool once placed, and false if it needs to
  // be updated instead (in which case UpdatePoolObject will be called).
  virtual bool ShouldDeletePoolObjectOnPlacement() const { return true; }

  // Update the PoolObject after placing it, if necessary. This will happen for
  // example in the case of a placed veneer, where we need to use a new updated
  // range and a new reference (from the newly added branch instruction).
  // By default, this does nothing, to avoid forcing objects that will not need
  // this to have an empty implementation.
  virtual void UpdatePoolObject(PoolObject<T>*) {}

  // Implement heuristics for emitting this object. If a margin is to be used
  // as a hint during pool emission, we will try not to emit the object if we
  // are further away from the maximum reachable location by more than the
  // margin.
  virtual bool UsePoolObjectEmissionMargin() const { return false; }
  virtual T GetPoolObjectEmissionMargin() const {
    VIXL_ASSERT(UsePoolObjectEmissionMargin() == false);
    return 0;
  }

  int GetPoolObjectSizeInBytes() const { return pool_object_size_; }
  int GetPoolObjectAlignment() const { return pool_object_alignment_; }
  uint32_t GetPoolObjectType() const { return pool_object_type_; }

  bool IsBound() const { return is_bound_; }
  T GetLocation() const { return location_; }

  // This function can be called multiple times before the object is marked as
  // bound with MarkBound() below. This is because some objects (e.g. the ones
  // used to represent labels) can have veneers; every time we place a veneer
  // we need to keep track of the location in order to resolve the references
  // to the object. Reusing the location_ field for this is convenient.
  void SetLocation(internal::AssemblerBase* assembler, T location) {
    VIXL_ASSERT(!is_bound_);
    location_ = location;
    ResolveReferences(assembler);
  }

  void MarkBound() {
    VIXL_ASSERT(!is_bound_);
    is_bound_ = true;
  }

  // The following two functions are used when an object is bound by a call to
  // PoolManager<T>::Bind().
  virtual int GetMaxAlignment() const {
    VIXL_ASSERT(!ShouldDeletePoolObjectOnPlacement());
    return 1;
  }
  virtual T GetMinLocation() const {
    VIXL_ASSERT(!ShouldDeletePoolObjectOnPlacement());
    return 0;
  }

 private:
  // The size of the corresponding PoolObject, in bytes.
  int pool_object_size_;
  // The alignment of the corresponding PoolObject; this must be a power of two.
  int pool_object_alignment_;

  // Different derived classes should have different type values. This can be
  // used internally by the PoolManager for grouping of objects.
  uint32_t pool_object_type_;
  // Has the object been bound to a location yet?
  bool is_bound_;

 protected:
  // See comment on SetLocation() for the use of this field.
  T location_;
};

template <typename T>
class PoolObject {
 public:
  // By default, PoolObjects have no inherent position constraints.
  explicit PoolObject(LocationBase<T>* parent)
      : label_base_(parent),
        min_location_(0),
        max_location_(std::numeric_limits<T>::max()),
        alignment_(parent->GetPoolObjectAlignment()),
        skip_until_location_hint_(0),
        type_(parent->GetPoolObjectType()) {
    VIXL_ASSERT(IsPowerOf2(alignment_));
    UpdateLocationHint();
  }

  // Reset the minimum and maximum location and the alignment of the object.
  // This function is public in order to allow the LocationBase corresponding to
  // this PoolObject to update the PoolObject when placed, e.g. in the case of
  // veneers. The size and type of the object cannot be modified.
  void Update(T min, T max, int alignment) {
    // We don't use RestrictRange here as the new range is independent of the
    // old range (and the maximum location is typically larger).
    min_location_ = min;
    max_location_ = max;
    RestrictAlignment(alignment);
    UpdateLocationHint();
  }

 private:
  void RestrictRange(T min, T max) {
    VIXL_ASSERT(min <= max_location_);
    VIXL_ASSERT(max >= min_location_);
    min_location_ = std::max(min_location_, min);
    max_location_ = std::min(max_location_, max);
    UpdateLocationHint();
  }

  void RestrictAlignment(int alignment) {
    VIXL_ASSERT(IsPowerOf2(alignment));
    VIXL_ASSERT(IsPowerOf2(alignment_));
    alignment_ = std::max(alignment_, alignment);
  }

  void UpdateLocationHint() {
    if (label_base_->UsePoolObjectEmissionMargin()) {
      skip_until_location_hint_ =
          max_location_ - label_base_->GetPoolObjectEmissionMargin();
    }
  }

  // The LocationBase that this pool object represents.
  LocationBase<T>* label_base_;

  // Hard, precise location constraints for the start location of the object.
  // They are both inclusive, that is the start location of the object can be
  // at any location between min_location_ and max_location_, themselves
  // included.
  T min_location_;
  T max_location_;

  // The alignment must be a power of two.
  int alignment_;

  // Avoid generating this object until skip_until_location_hint_. This
  // supports cases where placing the object in the pool has an inherent cost
  // that could be avoided in some other way. Veneers are a typical example; we
  // would prefer to branch directly (over a pool) rather than use veneers, so
  // this value can be set using some heuristic to leave them in the pool.
  // This value is only a hint, which will be ignored if it has to in order to
  // meet the hard constraints we have.
  T skip_until_location_hint_;

  // Used only to group objects of similar type together. The PoolManager does
  // not know what the types represent.
  uint32_t type_;

  friend class PoolManager<T>;
};

// Class that represents a forward reference. It is the responsibility of
// LocationBase objects to keep track of forward references and patch them when
// an object is placed - this class is only used by the PoolManager in order to
// restrict the requirements on PoolObjects it is tracking.
template <typename T>
class ForwardReference {
 public:
  ForwardReference(T location,
                   int size,
                   T min_object_location,
                   T max_object_location,
                   int object_alignment = 1)
      : location_(location),
        size_(size),
        object_alignment_(object_alignment),
        min_object_location_(min_object_location),
        max_object_location_(max_object_location) {
    VIXL_ASSERT(AlignDown(max_object_location, object_alignment) >=
                min_object_location);
  }

  bool LocationIsEncodable(T location) const {
    return location >= min_object_location_ &&
           location <= max_object_location_ &&
           IsAligned(location, object_alignment_);
  }

  T GetLocation() const { return location_; }
  T GetMinLocation() const { return min_object_location_; }
  T GetMaxLocation() const { return max_object_location_; }
  int GetAlignment() const { return object_alignment_; }

  // Needed for InvalSet.
  void SetLocationToInvalidateOnly(T location) { location_ = location; }

 private:
  // The location of the thing that contains the reference. For example, this
  // can be the location of the branch or load instruction.
  T location_;

  // The size of the instruction that makes the reference, in bytes.
  int size_;

  // The alignment that the object must satisfy for this reference - must be a
  // power of two.
  int object_alignment_;

  // Specify the possible locations where the object could be stored. AArch32's
  // PC offset, and T32's PC alignment calculations should be applied by the
  // Assembler, not here. The PoolManager deals only with simple locations.
  // Including min_object_address_ is necessary to handle AArch32 some
  // instructions which have a minimum offset of 0, but also have the implicit
  // PC offset.
  // Note that this structure cannot handle sparse ranges, such as A32's ADR,
  // but doing so is costly and probably not useful in practice. The min and
  // and max object location both refer to the beginning of the object, are
  // inclusive and are not affected by the object size. E.g. if
  // max_object_location_ is equal to X, we can place the object at location X
  // regardless of its size.
  T min_object_location_;
  T max_object_location_;

  friend class PoolManager<T>;
};


template <typename T>
class PoolManager {
 public:
  PoolManager(int header_size, int alignment, int buffer_alignment)
      : header_size_(header_size),
        alignment_(alignment),
        buffer_alignment_(buffer_alignment),
        checkpoint_(std::numeric_limits<T>::max()),
        max_pool_size_(0),
        monitor_(0) {}

  ~PoolManager() VIXL_NEGATIVE_TESTING_ALLOW_EXCEPTION;

  // Check if we will need to emit the pool at location 'pc', when planning to
  // generate a certain number of bytes. This optionally takes a
  // ForwardReference we are about to generate, in which case the size of the
  // reference must be included in 'num_bytes'.
  bool MustEmit(T pc,
                int num_bytes = 0,
                ForwardReference<T>* reference = NULL,
                LocationBase<T>* object = NULL) const;

  enum EmitOption { kBranchRequired, kNoBranchRequired };

  // Emit the pool at location 'pc', using 'masm' as the macroassembler.
  // The branch over the header can be optionally omitted using 'option'.
  // Returns the new PC after pool emission.
  // This expects a number of bytes that are about to be emitted, to be taken
  // into account in heuristics for pool object emission.
  // This also optionally takes a forward reference and an object as
  // parameters, to be used in the case where emission of the pool is triggered
  // by adding a new reference to the pool that does not fit. The pool manager
  // will need this information in order to apply its heuristics correctly.
  T Emit(MacroAssemblerInterface* masm,
         T pc,
         int num_bytes = 0,
         ForwardReference<T>* new_reference = NULL,
         LocationBase<T>* new_object = NULL,
         EmitOption option = kBranchRequired);

  // Add 'reference' to 'object'. Should not be preceded by a call to MustEmit()
  // that returned true, unless Emit() has been successfully afterwards.
  void AddObjectReference(const ForwardReference<T>* reference,
                          LocationBase<T>* object);

  // This is to notify the pool that a LocationBase has been bound to a location
  // and does not need to be tracked anymore.
  // This will happen, for example, for Labels, which are manually bound by the
  // user.
  // This can potentially add some padding bytes in order to meet the object
  // requirements, and will return the new location.
  T Bind(MacroAssemblerInterface* masm, LocationBase<T>* object, T location);

  // Functions for blocking and releasing the pools.
  void Block() { monitor_++; }
  void Release(T pc);
  bool IsBlocked() const { return monitor_ != 0; }

 private:
  typedef typename std::vector<PoolObject<T> >::iterator objects_iter;
  typedef
      typename std::vector<PoolObject<T> >::const_iterator const_objects_iter;

  PoolObject<T>* GetObjectIfTracked(LocationBase<T>* label) {
    return const_cast<PoolObject<T>*>(
        static_cast<const PoolManager<T>*>(this)->GetObjectIfTracked(label));
  }

  const PoolObject<T>* GetObjectIfTracked(LocationBase<T>* label) const {
    for (const_objects_iter iter = objects_.begin(); iter != objects_.end();
         ++iter) {
      const PoolObject<T>& current = *iter;
      if (current.label_base_ == label) return &current;
    }
    return NULL;
  }

  // Helper function for calculating the checkpoint.
  enum SortOption { kSortRequired, kNoSortRequired };
  void RecalculateCheckpoint(SortOption sort_option = kSortRequired);

  // Comparison function for using std::sort() on objects_. PoolObject A is
  // ordered before PoolObject B when A should be emitted before B. The
  // comparison depends on the max_location_, size_, alignment_ and
  // min_location_.
  static bool PoolObjectLessThan(const PoolObject<T>& a,
                                 const PoolObject<T>& b);

  // Helper function used in the checkpoint calculation. 'checkpoint' is the
  // current checkpoint, which is modified to take 'object' into account. The
  // new checkpoint is returned.
  static T UpdateCheckpointForObject(T checkpoint, const PoolObject<T>* object);

  // Helper function to add a new object into a sorted objects_ array.
  void Insert(const PoolObject<T>& new_object);

  // Helper functions to remove an object from objects_ and delete the
  // corresponding LocationBase object, if necessary. This will be called
  // either after placing the object, or when Bind() is called.
  void RemoveAndDelete(PoolObject<T>* object);
  objects_iter RemoveAndDelete(objects_iter iter);

  // Helper function to check if we should skip emitting an object.
  bool ShouldSkipObject(PoolObject<T>* pool_object,
                        T pc,
                        int num_bytes,
                        ForwardReference<T>* new_reference,
                        LocationBase<T>* new_object,
                        PoolObject<T>* existing_object) const;

  // Used only for debugging.
  void DumpCurrentState(T pc) const;

  // Methods used for testing only, via the test friend classes.
  bool PoolIsEmptyForTest() const { return objects_.empty(); }
  T GetCheckpointForTest() const { return checkpoint_; }
  int GetPoolSizeForTest() const;

  // The objects we are tracking references to. The objects_ vector is sorted
  // at all times between calls to the public members of the PoolManager. It
  // is sorted every time we add, delete or update a PoolObject.
  // TODO: Consider a more efficient data structure here, to allow us to delete
  // elements as we emit them.
  std::vector<PoolObject<T> > objects_;

  // Objects to be deleted on pool destruction.
  std::vector<LocationBase<T>*> delete_on_destruction_;

  // The header_size_ and alignment_ values are hardcoded for each instance of
  // PoolManager. The PoolManager does not know how to emit the header, and
  // relies on the EmitPoolHeader and EndPool methods of the
  // MacroAssemblerInterface for that.  It will also emit padding if necessary,
  // both for the header and at the end of the pool, according to alignment_,
  // and using the EmitNopBytes and EmitPaddingBytes method of the
  // MacroAssemblerInterface.

  // The size of the header, in bytes.
  int header_size_;
  // The alignment of the header - must be a power of two.
  int alignment_;
  // The alignment of the buffer - we cannot guarantee any object alignment
  // larger than this alignment. When a buffer is grown, this alignment has
  // to be guaranteed.
  // TODO: Consider extending this to describe the guaranteed alignment as the
  // modulo of a known number.
  int buffer_alignment_;

  // The current checkpoint. This is the latest location at which the pool
  // *must* be emitted. This should not be visible outside the pool manager
  // and should only be updated in RecalculateCheckpoint.
  T checkpoint_;

  // Maximum size of the pool, assuming we need the maximum possible padding
  // for each object and for the header. It is only updated in
  // RecalculateCheckpoint.
  T max_pool_size_;

  // Indicates whether the emission of this pool is blocked.
  int monitor_;

  friend class vixl::TestPoolManager;
};


}  // namespace vixl

#endif  // VIXL_POOL_MANAGER_H_
