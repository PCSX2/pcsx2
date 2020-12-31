#if !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "overlay_manager.h"

#include "core.h"

#include <cstring>
#include <memory>

namespace discord {

class OverlayEvents final {
public:
    static void OnToggle(void* callbackData, bool locked)
    {
        auto* core = reinterpret_cast<Core*>(callbackData);
        if (!core) {
            return;
        }

        auto& module = core->OverlayManager();
        module.OnToggle((locked != 0));
    }
};

IDiscordOverlayEvents OverlayManager::events_{
  &OverlayEvents::OnToggle,
};

void OverlayManager::IsEnabled(bool* enabled)
{
    if (!enabled) {
        return;
    }

    internal_->is_enabled(internal_, reinterpret_cast<bool*>(enabled));
}

void OverlayManager::IsLocked(bool* locked)
{
    if (!locked) {
        return;
    }

    internal_->is_locked(internal_, reinterpret_cast<bool*>(locked));
}

void OverlayManager::SetLocked(bool locked, std::function<void(Result)> callback)
{
    static auto wrapper = [](void* callbackData, EDiscordResult result) -> void {
        std::unique_ptr<std::function<void(Result)>> cb(
          reinterpret_cast<std::function<void(Result)>*>(callbackData));
        if (!cb || !(*cb)) {
            return;
        }
        (*cb)(static_cast<Result>(result));
    };
    std::unique_ptr<std::function<void(Result)>> cb{};
    cb.reset(new std::function<void(Result)>(std::move(callback)));
    internal_->set_locked(internal_, (locked ? 1 : 0), cb.release(), wrapper);
}

void OverlayManager::OpenActivityInvite(ActivityActionType type,
                                        std::function<void(Result)> callback)
{
    static auto wrapper = [](void* callbackData, EDiscordResult result) -> void {
        std::unique_ptr<std::function<void(Result)>> cb(
          reinterpret_cast<std::function<void(Result)>*>(callbackData));
        if (!cb || !(*cb)) {
            return;
        }
        (*cb)(static_cast<Result>(result));
    };
    std::unique_ptr<std::function<void(Result)>> cb{};
    cb.reset(new std::function<void(Result)>(std::move(callback)));
    internal_->open_activity_invite(
      internal_, static_cast<EDiscordActivityActionType>(type), cb.release(), wrapper);
}

void OverlayManager::OpenGuildInvite(char const* code, std::function<void(Result)> callback)
{
    static auto wrapper = [](void* callbackData, EDiscordResult result) -> void {
        std::unique_ptr<std::function<void(Result)>> cb(
          reinterpret_cast<std::function<void(Result)>*>(callbackData));
        if (!cb || !(*cb)) {
            return;
        }
        (*cb)(static_cast<Result>(result));
    };
    std::unique_ptr<std::function<void(Result)>> cb{};
    cb.reset(new std::function<void(Result)>(std::move(callback)));
    internal_->open_guild_invite(internal_, const_cast<char*>(code), cb.release(), wrapper);
}

void OverlayManager::OpenVoiceSettings(std::function<void(Result)> callback)
{
    static auto wrapper = [](void* callbackData, EDiscordResult result) -> void {
        std::unique_ptr<std::function<void(Result)>> cb(
          reinterpret_cast<std::function<void(Result)>*>(callbackData));
        if (!cb || !(*cb)) {
            return;
        }
        (*cb)(static_cast<Result>(result));
    };
    std::unique_ptr<std::function<void(Result)>> cb{};
    cb.reset(new std::function<void(Result)>(std::move(callback)));
    internal_->open_voice_settings(internal_, cb.release(), wrapper);
}

} // namespace discord
