#include "RetroTouchInput.h"

#ifdef __ANDROID__

#include <jni.h>
#include <sdl2/SDL.h>

namespace
{
    enum NavigationAction
    {
        NAV_UP = 0,
        NAV_DOWN,
        NAV_LEFT,
        NAV_RIGHT,
        NAV_OK,
        NAV_BACK
    };

    SDL_Keycode NavigationKey(int action)
    {
        switch (action)
        {
            case NAV_UP:    return SDLK_UP;
            case NAV_DOWN:  return SDLK_DOWN;
            case NAV_LEFT:  return SDLK_LEFT;
            case NAV_RIGHT: return SDLK_RIGHT;
            case NAV_OK:    return SDLK_RETURN;
            case NAV_BACK:  return SDLK_ESCAPE;
            default:        return SDLK_UNKNOWN;
        }
    }

    void PushKey(SDL_Keycode key, bool pressed)
    {
        if (key == SDLK_UNKNOWN)
            return;

        SDL_Event event;
        SDL_zero(event);
        event.type = pressed ? SDL_KEYDOWN : SDL_KEYUP;
        event.key.type = event.type;
        event.key.state = pressed ? SDL_PRESSED : SDL_RELEASED;
        event.key.repeat = 0;
        event.key.keysym.sym = key;
        event.key.keysym.scancode = SDL_GetScancodeFromKey(key);
        event.key.keysym.mod = KMOD_NONE;
        SDL_PushEvent(&event);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_ast_zgloom_ZGloomActivity_nativeRetroTouchSetAction(
    JNIEnv*, jclass, jint action, jboolean pressed)
{
    RetroTouchInput::SetAction(
        static_cast<int>(action),
        pressed == JNI_TRUE);
}

extern "C" JNIEXPORT void JNICALL
Java_com_ast_zgloom_ZGloomActivity_nativeRetroTouchSetMove(
    JNIEnv*, jclass, jfloat x, jfloat y)
{
    RetroTouchInput::SetMove(x, y);
}

extern "C" JNIEXPORT void JNICALL
Java_com_ast_zgloom_ZGloomActivity_nativeRetroTouchAddLook(
    JNIEnv*, jclass, jfloat deltaX, jfloat deltaY)
{
    RetroTouchInput::AddLook(deltaX, deltaY);
}

extern "C" JNIEXPORT void JNICALL
Java_com_ast_zgloom_ZGloomActivity_nativeRetroTouchNavigation(
    JNIEnv*, jclass, jint action, jboolean pressed)
{
    PushKey(
        NavigationKey(static_cast<int>(action)),
        pressed == JNI_TRUE);
}

extern "C" JNIEXPORT void JNICALL
Java_com_ast_zgloom_ZGloomActivity_nativeRetroTouchReset(
    JNIEnv*, jclass)
{
    RetroTouchInput::Reset();
}

#endif
