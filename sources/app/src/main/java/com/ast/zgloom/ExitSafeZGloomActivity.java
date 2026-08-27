package com.ast.zgloom;

import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

/**
 * ZGloom activity wrapper that keeps the Android/ChromeOS window opaque while
 * SDL removes its SurfaceView during shutdown.
 *
 * The wrapper also requests Android's standard immersive fullscreen mode.
 * Android Automotive OS / the vehicle System UI remains free to keep mandatory
 * vehicle controls visible if the OEM does not allow them to be hidden.
 *
 * The original ZGloomActivity remains unchanged. All data-installation,
 * build-info overlay, controller and SDL startup behaviour is inherited.
 */
public class ExitSafeZGloomActivity extends ZGloomActivity {

    private static final int EXIT_BACKGROUND_COLOR = Color.BLACK;
    private static final long EXIT_REMOVE_DELAY_MS = 50L;

    private boolean mFinishScheduled = false;
    private boolean mTaskRemovalCommitted = false;
    private View mExitCover = null;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        /*
         * SurfaceView is rendered in a separate surface. Keep the normal view
         * hierarchy opaque as a fallback so Android/ChromeOS never exposes the
         * theme/default window colour while the SDL surface is disappearing.
         */
        if (mLayout != null) {
            mLayout.setBackgroundColor(EXIT_BACKGROUND_COLOR);
        }

        getWindow().setBackgroundDrawable(
                new ColorDrawable(EXIT_BACKGROUND_COLOR));
        getWindow().getDecorView().setBackgroundColor(EXIT_BACKGROUND_COLOR);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            getWindow().setStatusBarColor(EXIT_BACKGROUND_COLOR);
            getWindow().setNavigationBarColor(EXIT_BACKGROUND_COLOR);
        }

        // Ask Android/AAOS for the maximum normal fullscreen area.
        applyImmersiveMode();

        // Suppress the normal Activity transition on supported Android versions.
        overridePendingTransition(0, 0);
    }

    @Override
    protected void onResume() {
        super.onResume();

        /*
         * Android may restore system bars while another Activity, permission
         * dialog or vehicle UI was in front. Re-apply immersive mode whenever
         * ZGloom becomes active again.
         */
        applyImmersiveMode();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);

        if (hasFocus && !mFinishScheduled) {
            /*
             * Re-apply after focus changes as System UI can temporarily reveal
             * its bars while switching between ZGloom and the vehicle UI.
             */
            applyImmersiveMode();
        }
    }

    @Override
    public void finish() {
        if (Looper.myLooper() != Looper.getMainLooper()) {
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    ExitSafeZGloomActivity.this.finish();
                }
            });
            return;
        }

        if (mFinishScheduled) {
            return;
        }
        mFinishScheduled = true;

        /*
         * SDL calls finish() after the native renderer has started shutting
         * down. Cover the separate SDL SurfaceView before Android gets a chance
         * to composite an empty Activity frame.
         */
        showExitCover();

        View decorView = getWindow().getDecorView();
        decorView.postOnAnimation(new Runnable() {
            @Override
            public void run() {
                moveTaskBehindAndScheduleRemoval();
            }
        });
    }

    @Override
    protected void onDestroy() {
        /*
         * Keep every Android-owned layer black during SDL's final cleanup.
         * This is the point where ChromeOS can otherwise reveal a white frame
         * after the SurfaceView has already disappeared.
         */
        getWindow().setBackgroundDrawable(
                new ColorDrawable(EXIT_BACKGROUND_COLOR));
        getWindow().getDecorView().setBackgroundColor(EXIT_BACKGROUND_COLOR);

        super.onDestroy();
    }

    /**
     * Request Android's immersive fullscreen mode using the legacy System UI
     * flags. This path is available with the project's older compile SDK and
     * is still honoured by Android 12.
     *
     * On Android Automotive OS this is only a request. Mandatory OEM vehicle
     * controls can remain visible and are not bypassed by this code.
     */
    @SuppressWarnings("deprecation")
    private void applyImmersiveMode() {
        final View decorView = getWindow().getDecorView();

        decorView.setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                        | View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
    }

    private void showExitCover() {
        if (mExitCover != null) {
            return;
        }

        FrameLayout cover = new FrameLayout(this);
        cover.setBackgroundColor(EXIT_BACKGROUND_COLOR);
        cover.setClickable(true);
        cover.setFocusable(true);

        addContentView(
                cover,
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.MATCH_PARENT));

        cover.bringToFront();
        cover.requestFocus();
        mExitCover = cover;
    }

    private void moveTaskBehindAndScheduleRemoval() {
        if (mTaskRemovalCommitted) {
            return;
        }

        overridePendingTransition(0, 0);

        /*
         * Let ChromeOS switch away from ZGloom while the Activity and its black
         * cover are still alive. Remove the task only after the compositor has
         * had a frame to present the previous window/desktop.
         */
        moveTaskToBack(true);

        new Handler(Looper.getMainLooper()).postDelayed(
                new Runnable() {
                    @Override
                    public void run() {
                        removeTaskAndFinish();
                    }
                },
                EXIT_REMOVE_DELAY_MS);
    }

    private void removeTaskAndFinish() {
        if (mTaskRemovalCommitted) {
            return;
        }
        mTaskRemovalCommitted = true;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            super.finishAndRemoveTask();
        } else {
            /*
             * OUYA / Android 4.x: finishAndRemoveTask() does not exist.
             * Keep the old-device path conventional and animation-free.
             */
            super.finish();
        }

        overridePendingTransition(0, 0);
    }
}
