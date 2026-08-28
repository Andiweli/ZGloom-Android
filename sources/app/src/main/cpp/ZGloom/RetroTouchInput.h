#pragma once

namespace RetroTouchInput
{
    enum Action
    {
        ACTION_FORWARD = 0,
        ACTION_BACKWARD,
        ACTION_TURN_LEFT,
        ACTION_TURN_RIGHT,
        ACTION_STRAFE_LEFT,
        ACTION_STRAFE_RIGHT,
        ACTION_STRAFE_MODIFIER,
        ACTION_RUN,
        ACTION_SHOOT,
        ACTION_COUNT
    };

    struct State
    {
        bool actions[ACTION_COUNT];
        float moveX;
        float moveY;
    };

    void SetAction(int action, bool pressed);
    void SetMove(float x, float y);
    void AddLook(float deltaX, float deltaY);
    State GetState();
    void ConsumeLook(float& deltaX, float& deltaY);
    void Reset();

#ifdef __ANDROID__
    // 0 = OFF, 1 = GAMEPLAY, 2 = NAVIGATION. The Java bridge applies
    // controller suppression and performs the actual RetroTouch mode change.
    void NotifyAndroidMode(int mode);
#endif
}
