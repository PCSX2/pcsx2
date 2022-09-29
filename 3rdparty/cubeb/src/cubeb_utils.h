/*
 * Copyright © 2016 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

#if !defined(CUBEB_UTILS)
#define CUBEB_UTILS

#include "cubeb/cubeb.h"

#ifdef __cplusplus

#include <assert.h>
#include <mutex>
#include <stdint.h>
#include <string.h>
#include <type_traits>
#if defined(_WIN32)
#include "cubeb_utils_win.h"
#else
#include "cubeb_utils_unix.h"
#endif

/** Similar to memcpy, but accounts for the size of an element. */
template <typename T>
void
PodCopy(T * destination, const T * source, size_t count)
{
  static_assert(std::is_trivial<T>::value, "Requires trivial type");
  assert(destination && source);
  memcpy(destination, source, count * sizeof(T));
}

/** Similar to memmove, but accounts for the size of an element. */
template <typename T>
void
PodMove(T * destination, const T * source, size_t count)
{
  static_assert(std::is_trivial<T>::value, "Requires trivial type");
  assert(destination && source);
  memmove(destination, source, count * sizeof(T));
}

/** Similar to a memset to zero, but accounts for the size of an element. */
template <typename T>
void
PodZero(T * destination, size_t count)
{
  static_assert(std::is_trivial<T>::value, "Requires trivial type");
  assert(destination);
  memset(destination, 0, count * sizeof(T));
}

namespace {
template <typename T, typename Trait>
void
Copy(T * destination, const T * source, size_t count, Trait)
{
  for (size_t i = 0; i < count; i++) {
    destination[i] = source[i];
  }
}

template <typename T>
void
Copy(T * destination, const T * source, size_t count, std::true_type)
{
  PodCopy(destination, source, count);
}
} // namespace

/**
 * This allows copying a number of elements from a `source` pointer to a
 * `destination` pointer, using `memcpy` if it is safe to do so, or a loop that
 * calls the constructors and destructors otherwise.
 */
template <typename T>
void
Copy(T * destination, const T * source, size_t count)
{
  assert(destination && source);
  Copy(destination, source, count, typename std::is_trivial<T>::type());
}

namespace {
template <typename T, typename Trait>
void
ConstructDefault(T * destination, size_t count, Trait)
{
  for (size_t i = 0; i < count; i++) {
    destination[i] = T();
  }
}

template <typename T>
void
ConstructDefault(T * destination, size_t count, std::true_type)
{
  PodZero(destination, count);
}
} // namespace

/**
 * This allows zeroing (using memset) or default-constructing a number of
 * elements calling the constructors and destructors if necessary.
 */
template <typename T>
void
ConstructDefault(T * destination, size_t count)
{
  assert(destination);
  ConstructDefault(destination, count, typename std::is_arithmetic<T>::type());
}

template <typename T> class auto_array {
public:
  explicit auto_array(uint32_t capacity = 0)
      : data_(capacity ? new T[capacity] : nullptr), capacity_(capacity),
        length_(0)
  {
  }

  ~auto_array() { delete[] data_; }

  /** Get a constant pointer to the underlying data. */
  T * data() const { return data_; }

  T * end() const { return data_ + length_; }

  const T & at(size_t index) const
  {
    assert(index < length_ && "out of range");
    return data_[index];
  }

  T & at(size_t index)
  {
    assert(index < length_ && "out of range");
    return data_[index];
  }

  /** Get how much underlying storage this auto_array has. */
  size_t capacity() const { return capacity_; }

  /** Get how much elements this auto_array contains. */
  size_t length() const { return length_; }

  /** Keeps the storage, but removes all the elements from the array. */
  void clear() { length_ = 0; }

  /** Change the storage of this auto array, copying the elements to the new
   * storage.
   * @returns true in case of success
   * @returns false if the new capacity is not big enough to accomodate for the
   *                elements in the array.
   */
  bool reserve(size_t new_capacity)
  {
    if (new_capacity < length_) {
      return false;
    }
    T * new_data = new T[new_capacity];
    if (data_ && length_) {
      PodCopy(new_data, data_, length_);
    }
    capacity_ = new_capacity;
    delete[] data_;
    data_ = new_data;

    return true;
  }

  /** Append `length` elements to the end of the array, resizing the array if
   * needed.
   * @parameter elements the elements to append to the array.
   * @parameter length the number of elements to append to the array.
   */
  void push(const T * elements, size_t length)
  {
    if (length_ + length > capacity_) {
      reserve(length_ + length);
    }
    PodCopy(data_ + length_, elements, length);
    length_ += length;
  }

  /** Append `length` zero-ed elements to the end of the array, resizing the
   * array if needed.
   * @parameter length the number of elements to append to the array.
   */
  void push_silence(size_t length)
  {
    if (length_ + length > capacity_) {
      reserve(length + length_);
    }
    PodZero(data_ + length_, length);
    length_ += length;
  }

  /** Prepend `length` zero-ed elements to the end of the array, resizing the
   * array if needed.
   * @parameter length the number of elements to prepend to the array.
   */
  void push_front_silence(size_t length)
  {
    if (length_ + length > capacity_) {
      reserve(length + length_);
    }
    PodMove(data_ + length, data_, length_);
    PodZero(data_, length);
    length_ += length;
  }

  /** Return the number of free elements in the array. */
  size_t available() const { return capacity_ - length_; }

  /** Copies `length` elements to `elements` if it is not null, and shift
   * the remaining elements of the `auto_array` to the beginning.
   * @parameter elements a buffer to copy the elements to, or nullptr.
   * @parameter length the number of elements to copy.
   * @returns true in case of success.
   * @returns false if the auto_array contains less than `length` elements. */
  bool pop(T * elements, size_t length)
  {
    if (length > length_) {
      return false;
    }
    if (elements) {
      PodCopy(elements, data_, length);
    }
    PodMove(data_, data_ + length, length_ - length);

    length_ -= length;

    return true;
  }

  void set_length(size_t length)
  {
    assert(length <= capacity_);
    length_ = length;
  }

private:
  /** The underlying storage */
  T * data_;
  /** The size, in number of elements, of the storage. */
  size_t capacity_;
  /** The number of elements the array contains. */
  size_t length_;
};

struct auto_array_wrapper {
  virtual void push(void * elements, size_t length) = 0;
  virtual size_t length() = 0;
  virtual void push_silence(size_t length) = 0;
  virtual bool pop(size_t length) = 0;
  virtual void * data() = 0;
  virtual void * end() = 0;
  virtual void clear() = 0;
  virtual bool reserve(size_t capacity) = 0;
  virtual void set_length(size_t length) = 0;
  virtual ~auto_array_wrapper() {}
};

template <typename T>
struct auto_array_wrapper_impl : public auto_array_wrapper {
  auto_array_wrapper_impl() {}

  explicit auto_array_wrapper_impl(uint32_t size) : ar(size) {}

  void push(void * elements, size_t length) override
  {
    ar.push(static_cast<T *>(elements), length);
  }

  size_t length() override { return ar.length(); }

  void push_silence(size_t length) override { ar.push_silence(length); }

  bool pop(size_t length) override { return ar.pop(nullptr, length); }

  void * data() override { return ar.data(); }

  void * end() override { return ar.end(); }

  void clear() override { ar.clear(); }

  bool reserve(size_t capacity) override { return ar.reserve(capacity); }

  void set_length(size_t length) override { ar.set_length(length); }

  ~auto_array_wrapper_impl() { ar.clear(); }

private:
  auto_array<T> ar;
};

extern "C" {
size_t
cubeb_sample_size(cubeb_sample_format format);
}

using auto_lock = std::lock_guard<owned_critical_section>;
#endif // __cplusplus

#endif /* CUBEB_UTILS */
