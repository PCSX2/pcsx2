#pragma once

#include "types.h"

namespace discord {

class OverlayManager final {
public:
    ~OverlayManager() = default;

    void IsEnabled(bool* enabled);
    void IsLocked(bool* locked);
    void SetLocked(bool locked, std::function<void(Result)> callback);
    void OpenActivityInvite(ActivityActionType type, std::function<void(Result)> callback);
    void OpenGuildInvite(char const* code, std::function<void(Result)> callback);
    void OpenVoiceSettings(std::function<void(Result)> callback);

    Event<bool> OnToggle;

private:
    friend class Core;

    OverlayManager() = default;
    OverlayManager(OverlayManager const& rhs) = delete;
    OverlayManager& operator=(OverlayManager const& rhs) = delete;
    OverlayManager(OverlayManager&& rhs) = delete;
    OverlayManager& operator=(OverlayManager&& rhs) = delete;

    IDiscordOverlayManager* internal_;
    static IDiscordOverlayEvents events_;
};

} // namespace discord
