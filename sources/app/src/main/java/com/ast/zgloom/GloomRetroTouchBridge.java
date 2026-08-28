package com.ast.zgloom;

import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.widget.RelativeLayout;

import com.ast.retrotouch.RetroTouchAdapter;
import com.ast.retrotouch.RetroTouchControl;
import com.ast.retrotouch.RetroTouchControllers;
import com.ast.retrotouch.RetroTouchLayout;
import com.ast.retrotouch.RetroTouchMode;
import com.ast.retrotouch.RetroTouchNavigation;
import com.ast.retrotouch.RetroTouchView;

import java.util.ArrayList;
import java.util.List;

/**
 * Small game-specific adapter between the reusable RetroTouch AAR and ZGloom.
 *
 * RetroTouch owns drawing, multitouch, editing and saved layouts. This class
 * only describes which Gloom actions exist, defines sensible default layouts
 * and forwards callbacks into the native Gloom input bridge.
 */
final class GloomRetroTouchBridge {

    static final int MODE_OFF = 0;
    static final int MODE_GAMEPLAY = 1;
    static final int MODE_NAVIGATION = 2;

    private static final int ACTION_FORWARD = 0;
    private static final int ACTION_BACKWARD = 1;
    private static final int ACTION_TURN_LEFT = 2;
    private static final int ACTION_TURN_RIGHT = 3;
    private static final int ACTION_STRAFE_LEFT = 4;
    private static final int ACTION_STRAFE_RIGHT = 5;
    private static final int ACTION_STRAFE_MODIFIER = 6;
    private static final int ACTION_RUN = 7;
    private static final int ACTION_SHOOT = 8;

    private static final int NAV_UP = 0;
    private static final int NAV_DOWN = 1;
    private static final int NAV_LEFT = 2;
    private static final int NAV_RIGHT = 3;
    private static final int NAV_OK = 4;
    private static final int NAV_BACK = 5;

    private static final long CONTROLLER_POLL_MS = 200L;
    private static final long RUN_DOUBLE_TAP_MS = ViewConfiguration.getDoubleTapTimeout();

    private static final String GAMEPLAY_LAYOUT_ID = "zgloom_android_gameplay_v1";
    private static final String NAVIGATION_LAYOUT_ID = "zgloom_android_navigation_v1";

    private static final String ID_FORWARD = "forward";
    private static final String ID_BACKWARD = "backward";
    private static final String ID_TURN_LEFT = "turn_left";
    private static final String ID_TURN_RIGHT = "turn_right";
    private static final String ID_STRAFE_LEFT = "strafe_left";
    private static final String ID_STRAFE_RIGHT = "strafe_right";
    private static final String ID_STRAFE_MODIFIER = "strafe_modifier";
    private static final String ID_RUN = "run";
    private static final String ID_SHOOT = "shoot";
    private static final String ID_MENU = "menu";
    private static final String ID_MENU_OK = "menu_ok";
    private static final String ID_MENU_BACK = "menu_back";

    private final ZGloomActivity activity;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final RetroTouchView retroTouch;

    private int desiredMode = MODE_OFF;
    private boolean nativeStarted = false;
    private boolean resumed = false;
    private boolean controllerConnected = false;
    private boolean pollScheduled = false;
    private RetroTouchMode appliedMode = RetroTouchMode.OFF;

    // Gloom-specific RUN rule: a normal press behaves like the keyboard RUN
    // key. Two presses within Android's double-tap interval toggle an
    // always-run latch. The latch is deliberately kept here rather than in
    // RetroTouch so the reusable AAR remains game-agnostic.
    private boolean runLatched = false;
    private long lastRunPressMs = 0L;

    private final Runnable controllerPoll = new Runnable() {
        @Override
        public void run() {
            pollScheduled = false;
            if (!resumed) {
                return;
            }

            boolean connected = RetroTouchControllers.isControllerConnected();
            if (connected != controllerConnected) {
                controllerConnected = connected;
                applyMode();
            }

            scheduleControllerPoll();
        }
    };

    GloomRetroTouchBridge(ZGloomActivity activity, ViewGroup root) {
        this.activity = activity;
        this.retroTouch = new RetroTouchView(activity);

        configureActions();
        configureLayouts();
        configureListener();

        // Controller availability is the single hide/show rule. Do not combine
        // it with RetroTouch's temporary three-second controller-input hiding.
        retroTouch.setAutoHideOnController(false);
        retroTouch.setLookWhileHoldingAction(ID_SHOOT, true);
        retroTouch.setMode(RetroTouchMode.OFF);

        RelativeLayout.LayoutParams fill = new RelativeLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT);
        root.addView(retroTouch, fill);
    }

    void onNativeThreadStarted() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                nativeStarted = true;

                // The first native screen is the Gloom game selector, which is
                // keyboard/controller driven. Give touch users the same D-pad,
                // OK and Back navigation immediately.
                desiredMode = MODE_NAVIGATION;
                controllerConnected = RetroTouchControllers.isControllerConnected();
                applyMode();
                scheduleControllerPoll();
            }
        });
    }

    void onResume() {
        resumed = true;
        controllerConnected = RetroTouchControllers.isControllerConnected();
        applyMode();
        scheduleControllerPoll();
    }

    void onPause() {
        resumed = false;
        handler.removeCallbacks(controllerPoll);
        pollScheduled = false;

        retroTouch.resetInputState();
        if (nativeStarted) {
            ZGloomActivity.nativeRetroTouchReset();
        }
        applyMode();
    }

    void onDestroy() {
        resumed = false;
        handler.removeCallbacks(controllerPoll);
        pollScheduled = false;
        retroTouch.resetInputState();
        if (nativeStarted) {
            ZGloomActivity.nativeRetroTouchReset();
        }
    }

    void setDesiredMode(final int mode) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                int safeMode = mode;
                if (safeMode < MODE_OFF || safeMode > MODE_NAVIGATION) {
                    safeMode = MODE_OFF;
                }

                if (desiredMode != safeMode) {
                    desiredMode = safeMode;
                    applyMode();
                }
            }
        });
    }

    private void configureActions() {
        // These nine actions mirror CONTROL OPTIONS -> CONFIGURE KEYS in Gloom.
        // They are semantic actions, not hard-coded W/A/S/D keys, so changing a
        // keyboard binding does not alter what a touch button means.
        retroTouch.registerAction(ID_FORWARD, "Forward");
        retroTouch.registerAction(ID_BACKWARD, "Backward");
        retroTouch.registerAction(ID_TURN_LEFT, "Turn\nLeft");
        retroTouch.registerAction(ID_TURN_RIGHT, "Turn\nRight");
        retroTouch.registerAction(ID_STRAFE_LEFT, "Strafe\nLeft");
        retroTouch.registerAction(ID_STRAFE_RIGHT, "Strafe\nRight");
        retroTouch.registerAction(ID_STRAFE_MODIFIER, "Strafe\nModifier");
        retroTouch.registerAction(ID_RUN, "Run");
        retroTouch.registerAction(ID_SHOOT, "Shoot");

        // Gloom's menu toggle is fixed to ESC/START rather than configurable,
        // but it is essential on a touch-only device.
        retroTouch.registerAction(ID_MENU, "Menu");

        // Navigation actions are used by the separate title/menu layout.
        retroTouch.registerAction(ID_MENU_OK, "OK");
        retroTouch.registerAction(ID_MENU_BACK, "Back");
    }

    private void configureLayouts() {
        List<RetroTouchControl> gameplay = new ArrayList<RetroTouchControl>();

        // Left stick: forward/backward plus dedicated strafe-left/right. The
        // remaining configurable actions stay available through ADD/ACTION in
        // the RetroTouch editor without overcrowding the default layout.
        gameplay.add(RetroTouchControl.moveStick(
                "move", 0.17f, 0.76f, 0.26f));

        // Gloom has horizontal mouse-look only. RetroTouch still uses a normal
        // two-axis look zone; native Gloom consumes X and safely ignores Y.
        gameplay.add(RetroTouchControl.lookZone(
                "look", 0.72f, 0.50f, 0.54f, 0.78f));

        // SHOOT sits inside the look zone so one finger can hold fire and keep
        // turning, while the other finger remains on the movement stick.
        gameplay.add(RetroTouchControl.button(
                "shoot_button", ID_SHOOT, "Shoot", 0.89f, 0.72f, 0.16f));
        gameplay.add(RetroTouchControl.button(
                "run_button", ID_RUN, "Run", 0.74f, 0.84f, 0.12f));
        gameplay.add(RetroTouchControl.button(
                "menu_button", ID_MENU, "Menu", 0.56f, 0.10f, 0.09f));

        retroTouch.setGameplayLayout(
                new RetroTouchLayout(GAMEPLAY_LAYOUT_ID, gameplay));

        List<RetroTouchControl> navigation = new ArrayList<RetroTouchControl>();
        navigation.add(RetroTouchControl.dPad(
                "navigation", 0.18f, 0.74f, 0.28f));
        navigation.add(RetroTouchControl.button(
                "ok", ID_MENU_OK, "OK", 0.87f, 0.68f, 0.14f));
        navigation.add(RetroTouchControl.button(
                "back", ID_MENU_BACK, "Back", 0.74f, 0.82f, 0.11f));

        retroTouch.setNavigationLayout(
                new RetroTouchLayout(NAVIGATION_LAYOUT_ID, navigation));
    }

    private void configureListener() {
        retroTouch.setListener(new RetroTouchAdapter() {
            @Override
            public void onAction(String actionId, boolean pressed) {
                if (!nativeStarted) {
                    return;
                }

                if (RetroTouchNavigation.UP.equals(actionId)) {
                    ZGloomActivity.nativeRetroTouchNavigation(NAV_UP, pressed);
                } else if (RetroTouchNavigation.DOWN.equals(actionId)) {
                    ZGloomActivity.nativeRetroTouchNavigation(NAV_DOWN, pressed);
                } else if (RetroTouchNavigation.LEFT.equals(actionId)) {
                    ZGloomActivity.nativeRetroTouchNavigation(NAV_LEFT, pressed);
                } else if (RetroTouchNavigation.RIGHT.equals(actionId)) {
                    ZGloomActivity.nativeRetroTouchNavigation(NAV_RIGHT, pressed);
                } else if (ID_MENU_OK.equals(actionId)) {
                    ZGloomActivity.nativeRetroTouchNavigation(NAV_OK, pressed);
                } else if (ID_MENU_BACK.equals(actionId) || ID_MENU.equals(actionId)) {
                    ZGloomActivity.nativeRetroTouchNavigation(NAV_BACK, pressed);
                } else if (ID_RUN.equals(actionId)) {
                    handleRunAction(pressed);
                } else {
                    int action = nativeActionForId(actionId);
                    if (action >= 0) {
                        ZGloomActivity.nativeRetroTouchSetAction(action, pressed);
                    }
                }
            }

            @Override
            public void onMove(float x, float y) {
                if (nativeStarted) {
                    ZGloomActivity.nativeRetroTouchSetMove(x, y);
                }
            }

            @Override
            public void onLook(float deltaX, float deltaY) {
                if (!nativeStarted) {
                    return;
                }

                // RetroTouch reports normalized deltas. Convert them back to a
                // resolution-independent pixel-like delta before Gloom applies
                // its existing MOUSE SENSITIVITY value.
                float scale = Math.max(1.0f,
                        Math.min(retroTouch.getWidth(), retroTouch.getHeight()));
                ZGloomActivity.nativeRetroTouchAddLook(
                        deltaX * scale,
                        deltaY * scale);
            }

            @Override
            public void onEditorStateChanged(boolean editing) {
                if (!nativeStarted) {
                    return;
                }

                if (editing) {
                    // Opening the editor must never leave a held movement/fire
                    // state active behind the editor UI. Keep the RUN toggle
                    // itself, but stop feeding it to Gloom while editing.
                    ZGloomActivity.nativeRetroTouchReset();
                    lastRunPressMs = 0L;
                } else {
                    restoreRunLatch();
                }
            }
        });
    }


    private void handleRunAction(boolean pressed) {
        if (!nativeStarted) {
            return;
        }

        if (pressed) {
            final long now = SystemClock.uptimeMillis();
            final boolean doubleTap = lastRunPressMs > 0L
                    && now - lastRunPressMs <= RUN_DOUBLE_TAP_MS;

            if (doubleTap) {
                runLatched = !runLatched;
                lastRunPressMs = 0L;
                retroTouch.setActionLatched(ID_RUN, runLatched);
            } else {
                lastRunPressMs = now;
            }

            // A finger currently held on RUN always means RUN, even when this
            // second tap just switched the persistent latch off.
            ZGloomActivity.nativeRetroTouchSetAction(ACTION_RUN, true);
        } else {
            // When latched, releasing the finger keeps RUN active. Otherwise
            // this is the normal momentary RUN-button release.
            ZGloomActivity.nativeRetroTouchSetAction(ACTION_RUN, runLatched);
        }
    }

    private void restoreRunLatch() {
        if (!nativeStarted || appliedMode != RetroTouchMode.GAMEPLAY) {
            return;
        }

        retroTouch.setActionLatched(ID_RUN, runLatched);
        ZGloomActivity.nativeRetroTouchSetAction(ACTION_RUN, runLatched);
    }

    private int nativeActionForId(String actionId) {
        if (ID_FORWARD.equals(actionId)) return ACTION_FORWARD;
        if (ID_BACKWARD.equals(actionId)) return ACTION_BACKWARD;
        if (ID_TURN_LEFT.equals(actionId)) return ACTION_TURN_LEFT;
        if (ID_TURN_RIGHT.equals(actionId)) return ACTION_TURN_RIGHT;
        if (ID_STRAFE_LEFT.equals(actionId)) return ACTION_STRAFE_LEFT;
        if (ID_STRAFE_RIGHT.equals(actionId)) return ACTION_STRAFE_RIGHT;
        if (ID_STRAFE_MODIFIER.equals(actionId)) return ACTION_STRAFE_MODIFIER;
        if (ID_RUN.equals(actionId)) return ACTION_RUN;
        if (ID_SHOOT.equals(actionId)) return ACTION_SHOOT;
        return -1;
    }

    private void applyMode() {
        RetroTouchMode target = RetroTouchMode.OFF;

        if (nativeStarted && resumed && !controllerConnected) {
            if (desiredMode == MODE_GAMEPLAY) {
                target = RetroTouchMode.GAMEPLAY;
            } else if (desiredMode == MODE_NAVIGATION) {
                target = RetroTouchMode.NAVIGATION;
            }
        }

        if (target == appliedMode) {
            return;
        }

        // Release the old mode first. This covers controller hot-plug, menu
        // transitions, pause/resume and native state changes without sticky
        // movement, fire or D-pad directions.
        retroTouch.resetInputState();
        if (nativeStarted) {
            ZGloomActivity.nativeRetroTouchReset();
        }

        retroTouch.setMode(target);
        appliedMode = target;
        lastRunPressMs = 0L;

        // resetInputState() above correctly releases all transient inputs, but
        // always-run is a Gloom gameplay toggle. Re-apply it when returning to
        // gameplay after a menu, controller hot-plug or Activity pause/resume.
        restoreRunLatch();
    }

    private void scheduleControllerPoll() {
        if (!resumed || pollScheduled) {
            return;
        }
        pollScheduled = true;
        handler.postDelayed(controllerPoll, CONTROLLER_POLL_MS);
    }

    private void runOnUiThread(Runnable runnable) {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            runnable.run();
        } else {
            activity.runOnUiThread(runnable);
        }
    }
}
