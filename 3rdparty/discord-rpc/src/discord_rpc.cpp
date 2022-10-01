#include "discord_rpc.h"

#include "backoff.h"
#include "discord_register.h"
#include "msg_queue.h"
#include "rpc_connection.h"
#include "serialization.h"

#include <atomic>
#include <chrono>
#include <mutex>

#ifndef DISCORD_DISABLE_IO_THREAD
#include <condition_variable>
#include <thread>
#endif

constexpr size_t MaxMessageSize{16 * 1024};
constexpr size_t MessageQueueSize{8};
constexpr size_t JoinQueueSize{8};

struct QueuedMessage {
    size_t length;
    char buffer[MaxMessageSize];

    void Copy(const QueuedMessage& other)
    {
        length = other.length;
        if (length) {
            memcpy(buffer, other.buffer, length);
        }
    }
};

struct User {
    // snowflake (64bit int), turned into a ascii decimal string, at most 20 chars +1 null
    // terminator = 21
    char userId[32];
    // 32 unicode glyphs is max name size => 4 bytes per glyph in the worst case, +1 for null
    // terminator = 129
    char username[344];
    // 4 decimal digits + 1 null terminator = 5
    char discriminator[8];
    // optional 'a_' + md5 hex digest (32 bytes) + null terminator = 35
    char avatar[128];
    // Rounded way up because I'm paranoid about games breaking from future changes in these sizes
};

static RpcConnection* Connection{nullptr};
static DiscordEventHandlers QueuedHandlers{};
static DiscordEventHandlers Handlers{};
static std::atomic_bool WasJustConnected{false};
static std::atomic_bool WasJustDisconnected{false};
static std::atomic_bool GotErrorMessage{false};
static std::atomic_bool WasJoinGame{false};
static std::atomic_bool WasSpectateGame{false};
static std::atomic_bool UpdatePresence{false};
static char JoinGameSecret[256];
static char SpectateGameSecret[256];
static int LastErrorCode{0};
static char LastErrorMessage[256];
static int LastDisconnectErrorCode{0};
static char LastDisconnectErrorMessage[256];
static std::mutex PresenceMutex;
static std::mutex HandlerMutex;
static QueuedMessage QueuedPresence{};
static MsgQueue<QueuedMessage, MessageQueueSize> SendQueue;
static MsgQueue<User, JoinQueueSize> JoinAskQueue;
static User connectedUser;

// We want to auto connect, and retry on failure, but not as fast as possible. This does expoential
// backoff from 0.5 seconds to 1 minute
static Backoff ReconnectTimeMs(500, 60 * 1000);
static auto NextConnect = std::chrono::system_clock::now();
static int Pid{0};
static int Nonce{1};

#ifndef DISCORD_DISABLE_IO_THREAD
static void Discord_UpdateConnection(void);
class IoThreadHolder {
private:
    std::atomic_bool keepRunning{true};
    std::mutex waitForIOMutex;
    std::condition_variable waitForIOActivity;
    std::thread ioThread;

public:
    void Start()
    {
        keepRunning.store(true);
        ioThread = std::thread([&]() {
            const std::chrono::duration<int64_t, std::milli> maxWait{500LL};
            Discord_UpdateConnection();
            while (keepRunning.load()) {
                std::unique_lock<std::mutex> lock(waitForIOMutex);
                waitForIOActivity.wait_for(lock, maxWait);
                Discord_UpdateConnection();
            }
        });
    }

    void Notify() { waitForIOActivity.notify_all(); }

    void Stop()
    {
        keepRunning.exchange(false);
        Notify();
        if (ioThread.joinable()) {
            ioThread.join();
        }
    }

    ~IoThreadHolder() { Stop(); }
};
#else
class IoThreadHolder {
public:
    void Start() {}
    void Stop() {}
    void Notify() {}
};
#endif // DISCORD_DISABLE_IO_THREAD
static IoThreadHolder* IoThread{nullptr};

static void UpdateReconnectTime()
{
    NextConnect = std::chrono::system_clock::now() +
      std::chrono::duration<int64_t, std::milli>{ReconnectTimeMs.nextDelay()};
}

#ifdef DISCORD_DISABLE_IO_THREAD
extern "C" DISCORD_EXPORT void Discord_UpdateConnection(void)
#else
static void Discord_UpdateConnection(void)
#endif
{
    if (!Connection) {
        return;
    }

    if (!Connection->IsOpen()) {
        if (std::chrono::system_clock::now() >= NextConnect) {
            UpdateReconnectTime();
            Connection->Open();
        }
    }
    else {
        // reads

        for (;;) {
            JsonDocument message;

            if (!Connection->Read(message)) {
                break;
            }

            const char* evtName = GetStrMember(&message, "evt");
            const char* nonce = GetStrMember(&message, "nonce");

            if (nonce) {
                // in responses only -- should use to match up response when needed.

                if (evtName && strcmp(evtName, "ERROR") == 0) {
                    auto data = GetObjMember(&message, "data");
                    LastErrorCode = GetIntMember(data, "code");
                    StringCopy(LastErrorMessage, GetStrMember(data, "message", ""));
                    GotErrorMessage.store(true);
                }
            }
            else {
                // should have evt == name of event, optional data
                if (evtName == nullptr) {
                    continue;
                }

                auto data = GetObjMember(&message, "data");

                if (strcmp(evtName, "ACTIVITY_JOIN") == 0) {
                    auto secret = GetStrMember(data, "secret");
                    if (secret) {
                        StringCopy(JoinGameSecret, secret);
                        WasJoinGame.store(true);
                    }
                }
                else if (strcmp(evtName, "ACTIVITY_SPECTATE") == 0) {
                    auto secret = GetStrMember(data, "secret");
                    if (secret) {
                        StringCopy(SpectateGameSecret, secret);
                        WasSpectateGame.store(true);
                    }
                }
                else if (strcmp(evtName, "ACTIVITY_JOIN_REQUEST") == 0) {
                    auto user = GetObjMember(data, "user");
                    auto userId = GetStrMember(user, "id");
                    auto username = GetStrMember(user, "username");
                    auto avatar = GetStrMember(user, "avatar");
                    auto joinReq = JoinAskQueue.GetNextAddMessage();
                    if (userId && username && joinReq) {
                        StringCopy(joinReq->userId, userId);
                        StringCopy(joinReq->username, username);
                        auto discriminator = GetStrMember(user, "discriminator");
                        if (discriminator) {
                            StringCopy(joinReq->discriminator, discriminator);
                        }
                        if (avatar) {
                            StringCopy(joinReq->avatar, avatar);
                        }
                        else {
                            joinReq->avatar[0] = 0;
                        }
                        JoinAskQueue.CommitAdd();
                    }
                }
            }
        }

        // writes
        if (UpdatePresence.exchange(false) && QueuedPresence.length) {
            QueuedMessage local;
            {
                std::lock_guard<std::mutex> guard(PresenceMutex);
                local.Copy(QueuedPresence);
            }
            if (!Connection->Write(local.buffer, local.length)) {
                // if we fail to send, requeue
                std::lock_guard<std::mutex> guard(PresenceMutex);
                QueuedPresence.Copy(local);
                UpdatePresence.exchange(true);
            }
        }

        while (SendQueue.HavePendingSends()) {
            auto qmessage = SendQueue.GetNextSendMessage();
            Connection->Write(qmessage->buffer, qmessage->length);
            SendQueue.CommitSend();
        }
    }
}

static void SignalIOActivity()
{
    if (IoThread != nullptr) {
        IoThread->Notify();
    }
}

static bool RegisterForEvent(const char* evtName)
{
    auto qmessage = SendQueue.GetNextAddMessage();
    if (qmessage) {
        qmessage->length =
          JsonWriteSubscribeCommand(qmessage->buffer, sizeof(qmessage->buffer), Nonce++, evtName);
        SendQueue.CommitAdd();
        SignalIOActivity();
        return true;
    }
    return false;
}

static bool DeregisterForEvent(const char* evtName)
{
    auto qmessage = SendQueue.GetNextAddMessage();
    if (qmessage) {
        qmessage->length =
          JsonWriteUnsubscribeCommand(qmessage->buffer, sizeof(qmessage->buffer), Nonce++, evtName);
        SendQueue.CommitAdd();
        SignalIOActivity();
        return true;
    }
    return false;
}

extern "C" DISCORD_EXPORT void Discord_Initialize(const char* applicationId,
                                                  DiscordEventHandlers* handlers,
                                                  int autoRegister,
                                                  const char* optionalSteamId)
{
    IoThread = new (std::nothrow) IoThreadHolder();
    if (IoThread == nullptr) {
        return;
    }

    if (autoRegister) {
        if (optionalSteamId && optionalSteamId[0]) {
            Discord_RegisterSteamGame(applicationId, optionalSteamId);
        }
        else {
            Discord_Register(applicationId, nullptr);
        }
    }

    Pid = GetProcessId();

    {
        std::lock_guard<std::mutex> guard(HandlerMutex);

        if (handlers) {
            QueuedHandlers = *handlers;
        }
        else {
            QueuedHandlers = {};
        }

        Handlers = {};
    }

    if (Connection) {
        return;
    }

    Connection = RpcConnection::Create(applicationId);
    Connection->onConnect = [](JsonDocument& readyMessage) {
        Discord_UpdateHandlers(&QueuedHandlers);
        if (QueuedPresence.length > 0) {
            UpdatePresence.exchange(true);
            SignalIOActivity();
        }
        auto data = GetObjMember(&readyMessage, "data");
        auto user = GetObjMember(data, "user");
        auto userId = GetStrMember(user, "id");
        auto username = GetStrMember(user, "username");
        auto avatar = GetStrMember(user, "avatar");
        if (userId && username) {
            StringCopy(connectedUser.userId, userId);
            StringCopy(connectedUser.username, username);
            auto discriminator = GetStrMember(user, "discriminator");
            if (discriminator) {
                StringCopy(connectedUser.discriminator, discriminator);
            }
            if (avatar) {
                StringCopy(connectedUser.avatar, avatar);
            }
            else {
                connectedUser.avatar[0] = 0;
            }
        }
        WasJustConnected.exchange(true);
        ReconnectTimeMs.reset();
    };
    Connection->onDisconnect = [](int err, const char* message) {
        LastDisconnectErrorCode = err;
        StringCopy(LastDisconnectErrorMessage, message);
        WasJustDisconnected.exchange(true);
        UpdateReconnectTime();
    };

    IoThread->Start();
}

extern "C" DISCORD_EXPORT void Discord_Shutdown(void)
{
    if (!Connection) {
        return;
    }
    Connection->onConnect = nullptr;
    Connection->onDisconnect = nullptr;
    Handlers = {};
    if (IoThread != nullptr) {
      IoThread->Stop();
      delete IoThread;
      IoThread = nullptr;
    }

    // HACK: We need to send the updated (cleared) presence, but we're shutting down.
    // Force an update, wait 100ms for it to clear (hopefully this will be long enough), and get any responses.
    Discord_UpdateConnection();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Discord_UpdateConnection();

    QueuedPresence.length = 0;
    UpdatePresence.exchange(false);

    RpcConnection::Destroy(Connection);
}

extern "C" DISCORD_EXPORT void Discord_UpdatePresence(const DiscordRichPresence* presence)
{
    {
        std::lock_guard<std::mutex> guard(PresenceMutex);
        QueuedPresence.length = JsonWriteRichPresenceObj(
          QueuedPresence.buffer, sizeof(QueuedPresence.buffer), Nonce++, Pid, presence);
        UpdatePresence.exchange(true);
    }
    SignalIOActivity();
}

extern "C" DISCORD_EXPORT void Discord_ClearPresence(void)
{
    Discord_UpdatePresence(nullptr);
}

extern "C" DISCORD_EXPORT void Discord_Respond(const char* userId, /* DISCORD_REPLY_ */ int reply)
{
    // if we are not connected, let's not batch up stale messages for later
    if (!Connection || !Connection->IsOpen()) {
        return;
    }
    auto qmessage = SendQueue.GetNextAddMessage();
    if (qmessage) {
        qmessage->length =
          JsonWriteJoinReply(qmessage->buffer, sizeof(qmessage->buffer), userId, reply, Nonce++);
        SendQueue.CommitAdd();
        SignalIOActivity();
    }
}

extern "C" DISCORD_EXPORT void Discord_RunCallbacks(void)
{
    // Note on some weirdness: internally we might connect, get other signals, disconnect any number
    // of times inbetween calls here. Externally, we want the sequence to seem sane, so any other
    // signals are book-ended by calls to ready and disconnect.

    if (!Connection) {
        return;
    }

    bool wasDisconnected = WasJustDisconnected.exchange(false);
    bool isConnected = Connection->IsOpen();

    if (isConnected) {
        // if we are connected, disconnect cb first
        std::lock_guard<std::mutex> guard(HandlerMutex);
        if (wasDisconnected && Handlers.disconnected) {
            Handlers.disconnected(LastDisconnectErrorCode, LastDisconnectErrorMessage);
        }
    }

    if (WasJustConnected.exchange(false)) {
        std::lock_guard<std::mutex> guard(HandlerMutex);
        if (Handlers.ready) {
            DiscordUser du{connectedUser.userId,
                           connectedUser.username,
                           connectedUser.discriminator,
                           connectedUser.avatar};
            Handlers.ready(&du);
        }
    }

    if (GotErrorMessage.exchange(false)) {
        std::lock_guard<std::mutex> guard(HandlerMutex);
        if (Handlers.errored) {
            Handlers.errored(LastErrorCode, LastErrorMessage);
        }
    }

    if (WasJoinGame.exchange(false)) {
        std::lock_guard<std::mutex> guard(HandlerMutex);
        if (Handlers.joinGame) {
            Handlers.joinGame(JoinGameSecret);
        }
    }

    if (WasSpectateGame.exchange(false)) {
        std::lock_guard<std::mutex> guard(HandlerMutex);
        if (Handlers.spectateGame) {
            Handlers.spectateGame(SpectateGameSecret);
        }
    }

    // Right now this batches up any requests and sends them all in a burst; I could imagine a world
    // where the implementer would rather sequentially accept/reject each one before the next invite
    // is sent. I left it this way because I could also imagine wanting to process these all and
    // maybe show them in one common dialog and/or start fetching the avatars in parallel, and if
    // not it should be trivial for the implementer to make a queue themselves.
    while (JoinAskQueue.HavePendingSends()) {
        auto req = JoinAskQueue.GetNextSendMessage();
        {
            std::lock_guard<std::mutex> guard(HandlerMutex);
            if (Handlers.joinRequest) {
                DiscordUser du{req->userId, req->username, req->discriminator, req->avatar};
                Handlers.joinRequest(&du);
            }
        }
        JoinAskQueue.CommitSend();
    }

    if (!isConnected) {
        // if we are not connected, disconnect message last
        std::lock_guard<std::mutex> guard(HandlerMutex);
        if (wasDisconnected && Handlers.disconnected) {
            Handlers.disconnected(LastDisconnectErrorCode, LastDisconnectErrorMessage);
        }
    }
}

extern "C" DISCORD_EXPORT void Discord_UpdateHandlers(DiscordEventHandlers* newHandlers)
{
    if (newHandlers) {
#define HANDLE_EVENT_REGISTRATION(handler_name, event)              \
    if (!Handlers.handler_name && newHandlers->handler_name) {      \
        RegisterForEvent(event);                                    \
    }                                                               \
    else if (Handlers.handler_name && !newHandlers->handler_name) { \
        DeregisterForEvent(event);                                  \
    }

        std::lock_guard<std::mutex> guard(HandlerMutex);
        HANDLE_EVENT_REGISTRATION(joinGame, "ACTIVITY_JOIN")
        HANDLE_EVENT_REGISTRATION(spectateGame, "ACTIVITY_SPECTATE")
        HANDLE_EVENT_REGISTRATION(joinRequest, "ACTIVITY_JOIN_REQUEST")

#undef HANDLE_EVENT_REGISTRATION

        Handlers = *newHandlers;
    }
    else {
        std::lock_guard<std::mutex> guard(HandlerMutex);
        Handlers = {};
    }
    return;
}
