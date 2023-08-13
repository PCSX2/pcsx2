// Copyright (c) Microsoft Corporation. All rights reserved.

/*==========================================================================;
 *
 *  Copyright (C) Microsoft Corporation.  All Rights Reserved.
 *
 *  File:       PIXEvents.h
 *  Content:    PIX include file
 *              Don't include this file directly - use pix3.h
 *
 ****************************************************************************/
#pragma once

#ifndef _PixEvents_H_
#define _PixEvents_H_

#ifndef _PIX3_H_
# error Do not include this file directly - use pix3.h
#endif

#include "PIXEventsCommon.h"

#if _MSC_VER < 1800
# error This version of pix3.h is only supported on Visual Studio 2013 or higher
#elif _MSC_VER < 1900
# ifndef constexpr // Visual Studio 2013 doesn't support constexpr
#  define constexpr
#  define PIX3__DEFINED_CONSTEXPR
# endif
#endif

// Xbox does not support CPU events for retail scenarios
#if defined(USE_PIX) || !defined(PIX_XBOX)
#define PIX_CONTEXT_EMIT_CPU_EVENTS
#endif

namespace PIXEventsDetail
{
    template<typename... ARGS>
    struct PIXEventTypeInferer
    {
        static constexpr PIXEventType Begin() { return PIXEvent_BeginEvent_VarArgs; }
        static constexpr PIXEventType SetMarker() { return PIXEvent_SetMarker_VarArgs; }
        static constexpr PIXEventType BeginOnContext() { return PIXEvent_BeginEvent_OnContext_VarArgs; }
        static constexpr PIXEventType SetMarkerOnContext() { return PIXEvent_SetMarker_OnContext_VarArgs; }

        // Xbox and Windows store different types of events for context events.
        // On Xbox these include a context argument, while on Windows they do
        // not. It is important not to change the event types used on the
        // Windows version as there are OS components (eg debug layer & DRED)
        // that decode event structs.
#ifdef PIX_XBOX
        static constexpr PIXEventType GpuBeginOnContext() { return PIXEvent_BeginEvent_OnContext_VarArgs; }
        static constexpr PIXEventType GpuSetMarkerOnContext() { return PIXEvent_SetMarker_OnContext_VarArgs; }
#else
        static constexpr PIXEventType GpuBeginOnContext() { return PIXEvent_BeginEvent_VarArgs; }
        static constexpr PIXEventType GpuSetMarkerOnContext() { return PIXEvent_SetMarker_VarArgs; }
#endif
    };

    template<>
    struct PIXEventTypeInferer<>
    {
        static constexpr PIXEventType Begin() { return PIXEvent_BeginEvent_NoArgs; }
        static constexpr PIXEventType SetMarker() { return PIXEvent_SetMarker_NoArgs; }
        static constexpr PIXEventType BeginOnContext() { return PIXEvent_BeginEvent_OnContext_NoArgs; }
        static constexpr PIXEventType SetMarkerOnContext() { return PIXEvent_SetMarker_OnContext_NoArgs; }

#ifdef PIX_XBOX
        static constexpr PIXEventType GpuBeginOnContext() { return PIXEvent_BeginEvent_OnContext_NoArgs; }
        static constexpr PIXEventType GpuSetMarkerOnContext() { return PIXEvent_SetMarker_OnContext_NoArgs; }
#else
        static constexpr PIXEventType GpuBeginOnContext() { return PIXEvent_BeginEvent_NoArgs; }
        static constexpr PIXEventType GpuSetMarkerOnContext() { return PIXEvent_SetMarker_NoArgs; }
#endif
    };

    inline void PIXCopyEventArguments(_Out_writes_to_ptr_(limit) UINT64*& destination, _In_ const UINT64* limit)
    {
        // nothing
        UNREFERENCED_PARAMETER(destination);
        UNREFERENCED_PARAMETER(limit);
    }

    template<typename ARG, typename... ARGS>
    void PIXCopyEventArguments(_Out_writes_to_ptr_(limit) UINT64*& destination, _In_ const UINT64* limit, ARG const& arg, ARGS const&... args)
    {
        PIXCopyEventArgument(destination, limit, arg);
        PIXCopyEventArguments(destination, limit, args...);
    }

    template<typename STR, typename... ARGS>
    __declspec(noinline) void PIXBeginEventAllocate(PIXEventsThreadInfo* threadInfo, UINT64 color, STR formatString, ARGS... args)
    {
#ifdef PIX_XBOX
        UINT64 time = PIXEventsReplaceBlock(false);
#else
        UINT64 time = PIXEventsReplaceBlock(threadInfo, false);
#endif
        if (!time)
            return;

        UINT64* destination = threadInfo->destination;
        UINT64* limit = threadInfo->biasedLimit;
        if (destination >= limit)
            return;

        limit += PIXEventsSafeFastCopySpaceQwords;
        *destination++ = PIXEncodeEventInfo(time, PIXEventTypeInferer<ARGS...>::Begin());
        *destination++ = color;

        PIXCopyEventArguments(destination, limit, formatString, args...);

        *destination = PIXEventsBlockEndMarker;
        threadInfo->destination = destination;
    }

    template<typename STR, typename... ARGS>
    void PIXBeginEvent(UINT64 color, STR formatString, ARGS... args)
    {
        PIXEventsThreadInfo* threadInfo = PIXGetThreadInfo();
        UINT64* limit = threadInfo->biasedLimit;
        if (limit != nullptr)
        {
            UINT64* destination = threadInfo->destination;
            if (destination < limit)
            {
                limit += PIXEventsSafeFastCopySpaceQwords;
                UINT64 time = PIXGetTimestampCounter();
                *destination++ = PIXEncodeEventInfo(time, PIXEventTypeInferer<ARGS...>::Begin());
                *destination++ = color;

                PIXCopyEventArguments(destination, limit, formatString, args...);

                *destination = PIXEventsBlockEndMarker;
                threadInfo->destination = destination;
            }
            else
            {
                PIXBeginEventAllocate(threadInfo, color, formatString, args...);
            }
        }
    }

    template<typename STR, typename... ARGS>
    __declspec(noinline) void PIXSetMarkerAllocate(PIXEventsThreadInfo* threadInfo, UINT64 color, STR formatString, ARGS... args)
    {
#ifdef PIX_XBOX
        UINT64 time = PIXEventsReplaceBlock(false);
#else
        UINT64 time = PIXEventsReplaceBlock(threadInfo, false);
#endif
        if (!time)
            return;

        UINT64* destination = threadInfo->destination;
        UINT64* limit = threadInfo->biasedLimit;

        if (destination >= limit)
            return;

        limit += PIXEventsSafeFastCopySpaceQwords;
        *destination++ = PIXEncodeEventInfo(time, PIXEventTypeInferer<ARGS...>::SetMarker());
        *destination++ = color;

        PIXCopyEventArguments(destination, limit, formatString, args...);

        *destination = PIXEventsBlockEndMarker;
        threadInfo->destination = destination;
    }

    template<typename STR, typename... ARGS>
    void PIXSetMarker(UINT64 color, STR formatString, ARGS... args)
    {
        PIXEventsThreadInfo* threadInfo = PIXGetThreadInfo();
        UINT64* limit = threadInfo->biasedLimit;
        if (limit != nullptr)
        {
            UINT64* destination = threadInfo->destination;
            if (destination < limit)
            {
                limit += PIXEventsSafeFastCopySpaceQwords;
                UINT64 time = PIXGetTimestampCounter();
                *destination++ = PIXEncodeEventInfo(time, PIXEventTypeInferer<ARGS...>::SetMarker());
                *destination++ = color;

                PIXCopyEventArguments(destination, limit, formatString, args...);

                *destination = PIXEventsBlockEndMarker;
                threadInfo->destination = destination;
            }
            else
            {
                PIXSetMarkerAllocate(threadInfo, color, formatString, args...);
            }
        }
    }

    template<typename STR, typename... ARGS>
    __declspec(noinline) void PIXBeginEventOnContextCpuAllocate(PIXEventsThreadInfo* threadInfo, void* context, UINT64 color, STR formatString, ARGS... args)
    {
#ifdef PIX_XBOX
        UINT64 time = PIXEventsReplaceBlock(false);
#else
        UINT64 time = PIXEventsReplaceBlock(threadInfo, false);
#endif
        if (!time)
            return;

        UINT64* destination = threadInfo->destination;
        UINT64* limit = threadInfo->biasedLimit;

        if (destination >= limit)
            return;

        limit += PIXEventsSafeFastCopySpaceQwords;
        *destination++ = PIXEncodeEventInfo(time, PIXEventTypeInferer<ARGS...>::BeginOnContext());
        *destination++ = color;

#ifdef PIX_XBOX
        UNREFERENCED_PARAMETER(context);
        PIXCopyEventArguments(destination, limit, formatString, args...);
#else
        PIXCopyEventArguments(destination, limit, context, formatString, args...);
#endif

        *destination = PIXEventsBlockEndMarker;
        threadInfo->destination = destination;
    }

    template<typename STR, typename... ARGS>
    void PIXBeginEventOnContextCpu(void* context, UINT64 color, STR formatString, ARGS... args)
    {
        PIXEventsThreadInfo* threadInfo = PIXGetThreadInfo();
        UINT64* limit = threadInfo->biasedLimit;
        if (limit != nullptr)
        {
            UINT64* destination = threadInfo->destination;
            if (destination < limit)
            {
                limit += PIXEventsSafeFastCopySpaceQwords;
                UINT64 time = PIXGetTimestampCounter();
                *destination++ = PIXEncodeEventInfo(time, PIXEventTypeInferer<ARGS...>::BeginOnContext());
                *destination++ = color;

#ifdef PIX_XBOX
                PIXCopyEventArguments(destination, limit, formatString, args...);
#else
                PIXCopyEventArguments(destination, limit, context, formatString, args...);
#endif

                *destination = PIXEventsBlockEndMarker;
                threadInfo->destination = destination;
            }
            else
            {
                PIXBeginEventOnContextCpuAllocate(threadInfo, context, color, formatString, args...);
            }
        }
    }

    template<typename CONTEXT, typename STR, typename... ARGS>
    void PIXBeginEvent(CONTEXT* context, UINT64 color, STR formatString, ARGS... args)
    {
#ifdef PIX_CONTEXT_EMIT_CPU_EVENTS
        PIXBeginEventOnContextCpu(context, color, formatString, args...);
#endif

        // TODO: we've already encoded this once for the CPU event - figure out way to avoid doing it again
        UINT64 buffer[PIXEventsGraphicsRecordSpaceQwords];
        UINT64* destination = buffer;
        UINT64* limit = buffer + PIXEventsGraphicsRecordSpaceQwords - PIXEventsReservedTailSpaceQwords;

        *destination++ = PIXEncodeEventInfo(0, PIXEventTypeInferer<ARGS...>::GpuBeginOnContext());
        *destination++ = color;

        PIXCopyEventArguments(destination, limit, formatString, args...);
        *destination = 0ull;

        PIXBeginGPUEventOnContext(context, static_cast<void*>(buffer), static_cast<UINT>(reinterpret_cast<BYTE*>(destination) - reinterpret_cast<BYTE*>(buffer)));
    }

    template<typename STR, typename... ARGS>
    __declspec(noinline) void PIXSetMarkerOnContextCpuAllocate(PIXEventsThreadInfo* threadInfo, void* context, UINT64 color, STR formatString, ARGS... args)
    {
#ifdef PIX_XBOX
        UINT64 time = PIXEventsReplaceBlock(false);
#else
        UINT64 time = PIXEventsReplaceBlock(threadInfo, false);
#endif
        if (!time)
            return;

        UINT64* destination = threadInfo->destination;
        UINT64* limit = threadInfo->biasedLimit;

        if (destination >= limit)
            return;

        limit += PIXEventsSafeFastCopySpaceQwords;
        *destination++ = PIXEncodeEventInfo(time, PIXEventTypeInferer<ARGS...>::SetMarkerOnContext());
        *destination++ = color;

#ifdef PIX_XBOX
        UNREFERENCED_PARAMETER(context);
        PIXCopyEventArguments(destination, limit, formatString, args...);
#else
        PIXCopyEventArguments(destination, limit, context, formatString, args...);
#endif

        *destination = PIXEventsBlockEndMarker;
        threadInfo->destination = destination;
    }

    template<typename STR, typename... ARGS>
    void PIXSetMarkerOnContextCpu(void* context, UINT64 color, STR formatString, ARGS... args)
    {
        PIXEventsThreadInfo* threadInfo = PIXGetThreadInfo();
        UINT64* limit = threadInfo->biasedLimit;
        if (limit != nullptr)
        {
            UINT64* destination = threadInfo->destination;
            if (destination < limit)
            {
                limit += PIXEventsSafeFastCopySpaceQwords;
                UINT64 time = PIXGetTimestampCounter();
                *destination++ = PIXEncodeEventInfo(time, PIXEventTypeInferer<ARGS...>::SetMarkerOnContext());
                *destination++ = color;

#ifdef PIX_XBOX
                PIXCopyEventArguments(destination, limit, formatString, args...);
#else
                PIXCopyEventArguments(destination, limit, context, formatString, args...);
#endif

                *destination = PIXEventsBlockEndMarker;
                threadInfo->destination = destination;
            }
            else
            {
                PIXSetMarkerOnContextCpuAllocate(threadInfo, context, color, formatString, args...);
            }
        }
    }

    template<typename CONTEXT, typename STR, typename... ARGS>
    void PIXSetMarker(CONTEXT* context, UINT64 color, STR formatString, ARGS... args)
    {
#ifdef PIX_CONTEXT_EMIT_CPU_EVENTS
        PIXSetMarkerOnContextCpu(context, color, formatString, args...);
#endif

        UINT64 buffer[PIXEventsGraphicsRecordSpaceQwords];
        UINT64* destination = buffer;
        UINT64* limit = buffer + PIXEventsGraphicsRecordSpaceQwords - PIXEventsReservedTailSpaceQwords;

        *destination++ = PIXEncodeEventInfo(0, PIXEventTypeInferer<ARGS...>::GpuSetMarkerOnContext());
        *destination++ = color;

        PIXCopyEventArguments(destination, limit, formatString, args...);
        *destination = 0ull;

        PIXSetGPUMarkerOnContext(context, static_cast<void*>(buffer), static_cast<UINT>(reinterpret_cast<BYTE*>(destination) - reinterpret_cast<BYTE*>(buffer)));
    }

    __declspec(noinline) inline void PIXEndEventAllocate(PIXEventsThreadInfo* threadInfo)
    {
#ifdef PIX_XBOX
        UINT64 time = PIXEventsReplaceBlock(true);
#else
        UINT64 time = PIXEventsReplaceBlock(threadInfo, true);
#endif
        if (!time)
            return;

        UINT64* destination = threadInfo->destination;
        UINT64* limit = threadInfo->biasedLimit;

        if (destination >= limit)
            return;

        limit += PIXEventsSafeFastCopySpaceQwords;
        *destination++ = PIXEncodeEventInfo(time, PIXEvent_EndEvent);
        *destination = PIXEventsBlockEndMarker;
        threadInfo->destination = destination;
    }

    inline void PIXEndEvent()
    {
        PIXEventsThreadInfo* threadInfo = PIXGetThreadInfo();
        UINT64* limit = threadInfo->biasedLimit;
        if (limit != nullptr)
        {
            UINT64* destination = threadInfo->destination;
            if (destination < limit)
            {
                limit += PIXEventsSafeFastCopySpaceQwords;
                UINT64 time = PIXGetTimestampCounter();
                *destination++ = PIXEncodeEventInfo(time, PIXEvent_EndEvent);
                *destination = PIXEventsBlockEndMarker;
                threadInfo->destination = destination;
            }
            else
            {
                PIXEndEventAllocate(threadInfo);
            }
        }
    }

    __declspec(noinline) inline void PIXEndEventOnContextCpuAllocate(PIXEventsThreadInfo* threadInfo, void* context)
    {
#ifdef PIX_XBOX
        UINT64 time = PIXEventsReplaceBlock(true);
#else
        UINT64 time = PIXEventsReplaceBlock(threadInfo, true);
#endif
        if (!time)
            return;

        UINT64* destination = threadInfo->destination;
        UINT64* limit = threadInfo->biasedLimit;

        if (destination >= limit)
            return;

        limit += PIXEventsSafeFastCopySpaceQwords;
        *destination++ = PIXEncodeEventInfo(time, PIXEvent_EndEvent_OnContext);
#ifdef PIX_XBOX
        UNREFERENCED_PARAMETER(context);
#else
        PIXCopyEventArgument(destination, limit, context);
#endif
        *destination = PIXEventsBlockEndMarker;
        threadInfo->destination = destination;
    }

    inline void PIXEndEventOnContextCpu(void* context)
    {
        PIXEventsThreadInfo* threadInfo = PIXGetThreadInfo();
        UINT64* limit = threadInfo->biasedLimit;
        if (limit != nullptr)
        {
            UINT64* destination = threadInfo->destination;
            if (destination < limit)
            {
                limit += PIXEventsSafeFastCopySpaceQwords;
                UINT64 time = PIXGetTimestampCounter();
                *destination++ = PIXEncodeEventInfo(time, PIXEvent_EndEvent_OnContext);
#ifndef PIX_XBOX
                PIXCopyEventArgument(destination, limit, context);
#endif
                *destination = PIXEventsBlockEndMarker;
                threadInfo->destination = destination;
            }
            else
            {
                PIXEndEventOnContextCpuAllocate(threadInfo, context);
            }
        }
    }

    template<typename CONTEXT>
    void PIXEndEvent(CONTEXT* context)
    {
#ifdef PIX_CONTEXT_EMIT_CPU_EVENTS
        PIXEndEventOnContextCpu(context);
#endif
        PIXEndGPUEventOnContext(context);
    }
}

#if defined(USE_PIX)

template<typename... ARGS>
void PIXBeginEvent(UINT64 color, PCWSTR formatString, ARGS... args)
{
    PIXEventsDetail::PIXBeginEvent(color, formatString, args...);
}

template<typename... ARGS>
void PIXBeginEvent(UINT64 color, PCSTR formatString, ARGS... args)
{
    PIXEventsDetail::PIXBeginEvent(color, formatString, args...);
}

template<typename... ARGS>
void PIXSetMarker(UINT64 color, PCWSTR formatString, ARGS... args)
{
    PIXEventsDetail::PIXSetMarker(color, formatString, args...);
}

template<typename... ARGS>
void PIXSetMarker(UINT64 color, PCSTR formatString, ARGS... args)
{
    PIXEventsDetail::PIXSetMarker(color, formatString, args...);
}

template<typename CONTEXT, typename... ARGS>
void PIXBeginEvent(CONTEXT* context, UINT64 color, PCWSTR formatString, ARGS... args)
{
    PIXEventsDetail::PIXBeginEvent(context, color, formatString, args...);
}

template<typename CONTEXT, typename... ARGS>
void PIXBeginEvent(CONTEXT* context, UINT64 color, PCSTR formatString, ARGS... args)
{
    PIXEventsDetail::PIXBeginEvent(context, color, formatString, args...);
}

template<typename CONTEXT, typename... ARGS>
void PIXSetMarker(CONTEXT* context, UINT64 color, PCWSTR formatString, ARGS... args)
{
    PIXEventsDetail::PIXSetMarker(context, color, formatString, args...);
}

template<typename CONTEXT, typename... ARGS>
void PIXSetMarker(CONTEXT* context, UINT64 color, PCSTR formatString, ARGS... args)
{
    PIXEventsDetail::PIXSetMarker(context, color, formatString, args...);
}

inline void PIXEndEvent()
{
    PIXEventsDetail::PIXEndEvent();
}

template<typename CONTEXT>
void PIXEndEvent(CONTEXT* context)
{
    PIXEventsDetail::PIXEndEvent(context);
}

#else // USE_PIX_RETAIL

inline void PIXBeginEvent(UINT64, _In_ PCSTR, ...) {}
inline void PIXBeginEvent(UINT64, _In_ PCWSTR, ...) {}
inline void PIXBeginEvent(void*, UINT64, _In_ PCSTR, ...) {}
inline void PIXBeginEvent(void*, UINT64, _In_ PCWSTR, ...) {}
inline void PIXEndEvent() {}
inline void PIXEndEvent(void*) {}
inline void PIXSetMarker(UINT64, _In_ PCSTR, ...) {}
inline void PIXSetMarker(UINT64, _In_ PCWSTR, ...) {}
inline void PIXSetMarker(void*, UINT64, _In_ PCSTR, ...) {}
inline void PIXSetMarker(void*, UINT64, _In_ PCWSTR, ...) {}

#endif // USE_PIX

template<typename CONTEXT, typename... ARGS>
void PIXBeginRetailEvent(CONTEXT* context, UINT64 color, PCWSTR formatString, ARGS... args)
{
    PIXEventsDetail::PIXBeginEvent(context, color, formatString, args...);
}

template<typename CONTEXT, typename... ARGS>
void PIXBeginRetailEvent(CONTEXT* context, UINT64 color, PCSTR formatString, ARGS... args)
{
    PIXEventsDetail::PIXBeginEvent(context, color, formatString, args...);
}

template<typename CONTEXT, typename... ARGS>
void PIXSetRetailMarker(CONTEXT* context, UINT64 color, PCWSTR formatString, ARGS... args)
{
    PIXEventsDetail::PIXSetMarker(context, color, formatString, args...);
}

template<typename CONTEXT, typename... ARGS>
void PIXSetRetailMarker(CONTEXT* context, UINT64 color, PCSTR formatString, ARGS... args)
{
    PIXEventsDetail::PIXSetMarker(context, color, formatString, args...);
}

template<typename CONTEXT>
void PIXEndRetailEvent(CONTEXT* context)
{
    PIXEventsDetail::PIXEndEvent(context);
}

template<typename CONTEXT>
class PIXScopedEventObject
{
    CONTEXT* m_context;

public:
    template<typename... ARGS>
    PIXScopedEventObject(CONTEXT* context, UINT64 color, PCWSTR formatString, ARGS... args)
        : m_context(context)
    {
        PIXBeginEvent(m_context, color, formatString, args...);
    }

    template<typename... ARGS>
    PIXScopedEventObject(CONTEXT* context, UINT64 color, PCSTR formatString, ARGS... args)
        : m_context(context)
    {
        PIXBeginEvent(m_context, color, formatString, args...);
    }

    ~PIXScopedEventObject()
    {
        PIXEndEvent(m_context);
    }
};

template<typename CONTEXT>
class PIXScopedRetailEventObject
{
    CONTEXT* m_context;

public:
    template<typename... ARGS>
    PIXScopedRetailEventObject(CONTEXT* context, UINT64 color, PCWSTR formatString, ARGS... args)
        : m_context(context)
    {
        PIXBeginRetailEvent(m_context, color, formatString, args...);
    }

    template<typename... ARGS>
    PIXScopedRetailEventObject(CONTEXT* context, UINT64 color, PCSTR formatString, ARGS... args)
        : m_context(context)
    {
        PIXBeginRetailEvent(m_context, color, formatString, args...);
    }

    ~PIXScopedRetailEventObject()
    {
        PIXEndRetailEvent(m_context);
    }
};

template<>
class PIXScopedEventObject<void>
{
public:
    template<typename... ARGS>
    PIXScopedEventObject(UINT64 color, PCWSTR formatString, ARGS... args)
    {
        PIXBeginEvent(color, formatString, args...);
    }

    template<typename... ARGS>
    PIXScopedEventObject(UINT64 color, PCSTR formatString, ARGS... args)
    {
        PIXBeginEvent(color, formatString, args...);
    }

    ~PIXScopedEventObject()
    {
        PIXEndEvent();
    }
};

#define PIXConcatenate(a, b) a ## b
#define PIXGetScopedEventVariableName(a, b) PIXConcatenate(a, b)
#define PIXScopedEvent(context, ...) PIXScopedEventObject<PIXInferScopedEventType<decltype(context)>::Type> PIXGetScopedEventVariableName(pixEvent, __LINE__)(context, __VA_ARGS__)

#ifdef PIX3__DEFINED_CONSTEXPR
#undef constexpr
#undef PIX3__DEFINED_CONSTEXPR
#endif

#endif // _PIXEvents_H__
