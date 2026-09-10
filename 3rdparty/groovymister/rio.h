#include <limits>
#include <iostream>
using std::cout;
using std::endl;

// Registered-IO surface for toolchains whose headers lack it (mingw-w64).
// Keyed on RIO_INVALID_CQ: it ships in the SAME SDK block (mswsockdef.h,
// NTDDI >= WIN8) as the RIO types themselves, so it is present exactly when
// this block must be skipped (MSVC: defining it here collides with the SDK's
// non-identical types, C2371/C2011) and absent exactly when it must be active
// (mingw-w64 defines NO RIO types anywhere). Do NOT key on
// WSA_FLAG_REGISTERED_IO: both toolchains define that flag (mingw
// winsock2.h:526) and it says nothing about the types. Keying on it skips the
// block on mingw and breaks the build.
#if defined(_WIN32) && !defined(RIO_INVALID_CQ)
#define WSA_FLAG_REGISTERED_IO 0x100
#define SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER _WSAIORW(IOC_WS2, 36)
#define WSAID_MULTIPLE_RIO {0x8509e081, 0x96dd, 0x4005, { 0xb1, 0x65, 0x9e, 0x2e, 0xe8, 0xc7, 0x9e, 0x3f } }

/* Windows SDK actually have these definitions :
   * typedef struct RIO_BUFFERID_t *RIO_BUFFERID, **PRIO_BUFFERID;
   * typedef struct RIO_CQ_t *RIO_CQ, **PRIO_CQ;
   * typedef struct RIO_RQ_t *RIO_RQ, **PRIO_RQ;
   * But since RIO_BUFFERID_t, RIO_CQ_t and RIO_RQ_t are not defined I replaced these with void.
   */
typedef void *RIO_BUFFERID, **PRIO_BUFFERID;
typedef void *RIO_CQ, **PRIO_CQ;
typedef void *RIO_RQ, **PRIO_RQ;

#define RIO_MSG_DONT_NOTIFY           0x00000001
#define RIO_MSG_DEFER                 0x00000002
#define RIO_MSG_WAITALL               0x00000004
#define RIO_MSG_COMMIT_ONLY           0x00000008

#define RIO_INVALID_BUFFERID          ((RIO_BUFFERID)(ULONG_PTR)0xFFFFFFFF)
#define RIO_INVALID_CQ                ((RIO_CQ)0)
#define RIO_INVALID_RQ                ((RIO_RQ)0)

#define RIO_MAX_CQ_SIZE               0x8000000
#define RIO_CORRUPT_CQ                0xFFFFFFFF

typedef struct _RIORESULT {
  LONG Status;
  ULONG BytesTransferred;
  ULONGLONG SocketContext;
  ULONGLONG RequestContext;
} RIORESULT, *PRIORESULT;

typedef struct _RIO_BUF {
  RIO_BUFFERID BufferId;
  ULONG Offset;
  ULONG Length;
} RIO_BUF, *PRIO_BUF;

typedef BOOL(PASCAL FAR *LPFN_RIORECEIVE)(
    _In_ RIO_RQ SocketQueue, _In_reads_(DataBufferCount) PRIO_BUF pData,
    _In_ ULONG DataBufferCount, _In_ DWORD Flags, _In_ PVOID RequestContext);

typedef int(PASCAL FAR *LPFN_RIORECEIVEEX)(
    _In_ RIO_RQ SocketQueue, _In_reads_(DataBufferCount) PRIO_BUF pData,
    _In_ ULONG DataBufferCount, _In_opt_ PRIO_BUF pLocalAddress,
    _In_opt_ PRIO_BUF pRemoteAddress, _In_opt_ PRIO_BUF pControlContext,
    _In_opt_ PRIO_BUF pFlags, _In_ DWORD Flags, _In_ PVOID RequestContext);

typedef BOOL(PASCAL FAR *LPFN_RIOSEND)(
    _In_ RIO_RQ SocketQueue, _In_reads_(DataBufferCount) PRIO_BUF pData,
    _In_ ULONG DataBufferCount, _In_ DWORD Flags, _In_ PVOID RequestContext);

typedef BOOL(PASCAL FAR *LPFN_RIOSENDEX)(
    _In_ RIO_RQ SocketQueue, _In_reads_(DataBufferCount) PRIO_BUF pData,
    _In_ ULONG DataBufferCount, _In_opt_ PRIO_BUF pLocalAddress,
    _In_opt_ PRIO_BUF pRemoteAddress, _In_opt_ PRIO_BUF pControlContext,
    _In_opt_ PRIO_BUF pFlags, _In_ DWORD Flags, _In_ PVOID RequestContext);

typedef VOID(PASCAL FAR *LPFN_RIOCLOSECOMPLETIONQUEUE)(_In_ RIO_CQ CQ);

typedef enum _RIO_NOTIFICATION_COMPLETION_TYPE {
  RIO_EVENT_COMPLETION = 1,
  RIO_IOCP_COMPLETION = 2,
} RIO_NOTIFICATION_COMPLETION_TYPE,
    *PRIO_NOTIFICATION_COMPLETION_TYPE;

typedef struct _RIO_NOTIFICATION_COMPLETION {
  RIO_NOTIFICATION_COMPLETION_TYPE Type;
  union {
    struct {
      HANDLE EventHandle;
      BOOL NotifyReset;
    } Event;
    struct {
      HANDLE IocpHandle;
      PVOID CompletionKey;
      PVOID Overlapped;
    } Iocp;
  };
} RIO_NOTIFICATION_COMPLETION, *PRIO_NOTIFICATION_COMPLETION;

typedef RIO_CQ(PASCAL FAR *LPFN_RIOCREATECOMPLETIONQUEUE)(
    _In_ DWORD QueueSize,
    _In_opt_ PRIO_NOTIFICATION_COMPLETION NotificationCompletion);

typedef RIO_RQ(PASCAL FAR *LPFN_RIOCREATEREQUESTQUEUE)(
    _In_ SOCKET Socket, _In_ ULONG MaxOutstandingReceive,
    _In_ ULONG MaxReceiveDataBuffers, _In_ ULONG MaxOutstandingSend,
    _In_ ULONG MaxSendDataBuffers, _In_ RIO_CQ ReceiveCQ, _In_ RIO_CQ SendCQ,
    _In_ PVOID SocketContext);

typedef ULONG(PASCAL FAR *LPFN_RIODEQUEUECOMPLETION)(
    _In_ RIO_CQ CQ, _Out_writes_to_(ArraySize, return ) PRIORESULT Array,
    _In_ ULONG ArraySize);

typedef VOID(PASCAL FAR *LPFN_RIODEREGISTERBUFFER)(_In_ RIO_BUFFERID BufferId);

typedef INT(PASCAL FAR *LPFN_RIONOTIFY)(_In_ RIO_CQ CQ);

typedef RIO_BUFFERID(PASCAL FAR *LPFN_RIOREGISTERBUFFER)(_In_ PCHAR DataBuffer,
                                                         _In_ DWORD DataLength);

typedef BOOL(PASCAL FAR *LPFN_RIORESIZECOMPLETIONQUEUE)(_In_ RIO_CQ CQ,
                                                        _In_ DWORD QueueSize);

typedef BOOL(PASCAL FAR *LPFN_RIORESIZEREQUESTQUEUE)(
    _In_ RIO_RQ RQ, _In_ DWORD MaxOutstandingReceive,
    _In_ DWORD MaxOutstandingSend);

typedef struct _RIO_EXTENSION_FUNCTION_TABLE {
  DWORD cbSize;
  LPFN_RIORECEIVE RIOReceive;
  LPFN_RIORECEIVEEX RIOReceiveEx;
  LPFN_RIOSEND RIOSend;
  LPFN_RIOSENDEX RIOSendEx;
  LPFN_RIOCLOSECOMPLETIONQUEUE RIOCloseCompletionQueue;
  LPFN_RIOCREATECOMPLETIONQUEUE RIOCreateCompletionQueue;
  LPFN_RIOCREATEREQUESTQUEUE RIOCreateRequestQueue;
  LPFN_RIODEQUEUECOMPLETION RIODequeueCompletion;
  LPFN_RIODEREGISTERBUFFER RIODeregisterBuffer;
  LPFN_RIONOTIFY RIONotify;
  LPFN_RIOREGISTERBUFFER RIORegisterBuffer;
  LPFN_RIORESIZECOMPLETIONQUEUE RIOResizeCompletionQueue;
  LPFN_RIORESIZEREQUESTQUEUE RIOResizeRequestQueue;
} RIO_EXTENSION_FUNCTION_TABLE, *PRIO_EXTENSION_FUNCTION_TABLE;

#endif


