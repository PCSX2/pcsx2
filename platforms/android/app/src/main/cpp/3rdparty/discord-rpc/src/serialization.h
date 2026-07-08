#pragma once

#include <stdint.h>

#ifndef __MINGW32__
#pragma warning(push)

#pragma warning(disable : 4061) // enum is not explicitly handled by a case label
#pragma warning(disable : 4365) // signed/unsigned mismatch
#pragma warning(disable : 4464) // relative include path contains
#pragma warning(disable : 4668) // is not defined as a preprocessor macro
#pragma warning(disable : 6313) // Incorrect operator
#endif                          // __MINGW32__

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#ifndef __MINGW32__
#pragma warning(pop)
#endif // __MINGW32__

// if only there was a standard library function for this
template <size_t Len>
inline size_t StringCopy(char (&dest)[Len], const char* src)
{
    if (!src || !Len) {
        return 0;
    }
    size_t copied;
    char* out = dest;
    for (copied = 1; *src && copied < Len; ++copied) {
        *out++ = *src++;
    }
    *out = 0;
    return copied - 1;
}

size_t JsonWriteHandshakeObj(char* dest, size_t maxLen, int version, const char* applicationId);

// Commands
struct DiscordRichPresence;
size_t JsonWriteRichPresenceObj(char* dest,
                                size_t maxLen,
                                int nonce,
                                int pid,
                                const DiscordRichPresence* presence);
size_t JsonWriteSubscribeCommand(char* dest, size_t maxLen, int nonce, const char* evtName);

size_t JsonWriteUnsubscribeCommand(char* dest, size_t maxLen, int nonce, const char* evtName);

size_t JsonWriteJoinReply(char* dest, size_t maxLen, const char* userId, int reply, int nonce);

// I want to use as few allocations as I can get away with, and to do that with RapidJson, you need
// to supply some of your own allocators for stuff rather than use the defaults

class LinearAllocator {
public:
    char* buffer_;
    char* end_;
    LinearAllocator()
    {
        assert(0); // needed for some default case in rapidjson, should not use
    }
    LinearAllocator(char* buffer, size_t size)
      : buffer_(buffer)
      , end_(buffer + size)
    {
    }
    static const bool kNeedFree = false;
    void* Malloc(size_t size)
    {
        char* res = buffer_;
        buffer_ += size;
        if (buffer_ > end_) {
            buffer_ = res;
            return nullptr;
        }
        return res;
    }
    void* Realloc(void* originalPtr, size_t originalSize, size_t newSize)
    {
        if (newSize == 0) {
            return nullptr;
        }
        // allocate how much you need in the first place
        assert(!originalPtr && !originalSize);
        // unused parameter warning
        (void)(originalPtr);
        (void)(originalSize);
        return Malloc(newSize);
    }
    static void Free(void* ptr)
    {
        /* shrug */
        (void)ptr;
    }
};

template <size_t Size>
class FixedLinearAllocator : public LinearAllocator {
public:
    char fixedBuffer_[Size];
    FixedLinearAllocator()
      : LinearAllocator(fixedBuffer_, Size)
    {
    }
    static const bool kNeedFree = false;
};

// wonder why this isn't a thing already, maybe I missed it
class DirectStringBuffer {
public:
    using Ch = char;
    char* buffer_;
    char* end_;
    char* current_;

    DirectStringBuffer(char* buffer, size_t maxLen)
      : buffer_(buffer)
      , end_(buffer + maxLen)
      , current_(buffer)
    {
    }

    void Put(char c)
    {
        if (current_ < end_) {
            *current_++ = c;
        }
    }
    void Flush() {}
    size_t GetSize() const { return (size_t)(current_ - buffer_); }
};

using MallocAllocator = rapidjson::CrtAllocator;
using PoolAllocator = rapidjson::MemoryPoolAllocator<MallocAllocator>;
using UTF8 = rapidjson::UTF8<char>;
// Writer appears to need about 16 bytes per nested object level (with 64bit size_t)
using StackAllocator = FixedLinearAllocator<2048>;
constexpr size_t WriterNestingLevels = 2048 / (2 * sizeof(size_t));
using JsonWriterBase =
  rapidjson::Writer<DirectStringBuffer, UTF8, UTF8, StackAllocator, rapidjson::kWriteNoFlags>;
class JsonWriter : public JsonWriterBase {
public:
    DirectStringBuffer stringBuffer_;
    StackAllocator stackAlloc_;

    JsonWriter(char* dest, size_t maxLen)
      : JsonWriterBase(stringBuffer_, &stackAlloc_, WriterNestingLevels)
      , stringBuffer_(dest, maxLen)
      , stackAlloc_()
    {
    }

    size_t Size() const { return stringBuffer_.GetSize(); }
};

using JsonDocumentBase = rapidjson::GenericDocument<UTF8, PoolAllocator, StackAllocator>;
class JsonDocument : public JsonDocumentBase {
public:
    static const int kDefaultChunkCapacity = 32 * 1024;
    // json parser will use this buffer first, then allocate more if needed; I seriously doubt we
    // send any messages that would use all of this, though.
    char parseBuffer_[32 * 1024];
    MallocAllocator mallocAllocator_;
    PoolAllocator poolAllocator_;
    StackAllocator stackAllocator_;
    JsonDocument()
      : JsonDocumentBase(rapidjson::kObjectType,
                         &poolAllocator_,
                         sizeof(stackAllocator_.fixedBuffer_),
                         &stackAllocator_)
      , poolAllocator_(parseBuffer_, sizeof(parseBuffer_), kDefaultChunkCapacity, &mallocAllocator_)
      , stackAllocator_()
    {
    }
};

using JsonValue = rapidjson::GenericValue<UTF8, PoolAllocator>;

inline JsonValue* GetObjMember(JsonValue* obj, const char* name)
{
    if (obj) {
        auto member = obj->FindMember(name);
        if (member != obj->MemberEnd() && member->value.IsObject()) {
            return &member->value;
        }
    }
    return nullptr;
}

inline int GetIntMember(JsonValue* obj, const char* name, int notFoundDefault = 0)
{
    if (obj) {
        auto member = obj->FindMember(name);
        if (member != obj->MemberEnd() && member->value.IsInt()) {
            return member->value.GetInt();
        }
    }
    return notFoundDefault;
}

inline const char* GetStrMember(JsonValue* obj,
                                const char* name,
                                const char* notFoundDefault = nullptr)
{
    if (obj) {
        auto member = obj->FindMember(name);
        if (member != obj->MemberEnd() && member->value.IsString()) {
            return member->value.GetString();
        }
    }
    return notFoundDefault;
}
