#include "RetroTouchInput.h"

#include <algorithm>
#include <cstring>
#include <mutex>

#ifdef __ANDROID__
#include <jni.h>
#include <sdl2/SDL_system.h>
#endif

namespace
{
    std::mutex gInputMutex;
    RetroTouchInput::State gState = {};
    float gLookX = 0.0f;
    float gLookY = 0.0f;

    float ClampAxis(float value)
    {
        return std::max(-1.0f, std::min(1.0f, value));
    }
}

namespace RetroTouchInput
{
    void SetAction(int action, bool pressed)
    {
        if (action < 0 || action >= ACTION_COUNT)
            return;

        std::lock_guard<std::mutex> lock(gInputMutex);
        gState.actions[action] = pressed;
    }

    void SetMove(float x, float y)
    {
        std::lock_guard<std::mutex> lock(gInputMutex);
        gState.moveX = ClampAxis(x);
        gState.moveY = ClampAxis(y);
    }

    void AddLook(float deltaX, float deltaY)
    {
        std::lock_guard<std::mutex> lock(gInputMutex);
        gLookX += deltaX;
        gLookY += deltaY;
    }

    State GetState()
    {
        std::lock_guard<std::mutex> lock(gInputMutex);
        return gState;
    }

    void ConsumeLook(float& deltaX, float& deltaY)
    {
        std::lock_guard<std::mutex> lock(gInputMutex);
        deltaX = gLookX;
        deltaY = gLookY;
        gLookX = 0.0f;
        gLookY = 0.0f;
    }

    void Reset()
    {
        std::lock_guard<std::mutex> lock(gInputMutex);
        std::memset(&gState, 0, sizeof(gState));
        gLookX = 0.0f;
        gLookY = 0.0f;
    }

#ifdef __ANDROID__
    void NotifyAndroidMode(int mode)
    {
        JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
        jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
        if (!env || !activity)
            return;

        jclass activityClass = env->GetObjectClass(activity);
        if (!activityClass)
        {
            env->DeleteLocalRef(activity);
            return;
        }

        jmethodID method = env->GetMethodID(
            activityClass,
            "setRetroTouchDesiredMode",
            "(I)V");

        if (method)
            env->CallVoidMethod(activity, method, static_cast<jint>(mode));

        if (env->ExceptionCheck())
            env->ExceptionClear();

        env->DeleteLocalRef(activityClass);
        env->DeleteLocalRef(activity);
    }
#endif
}
