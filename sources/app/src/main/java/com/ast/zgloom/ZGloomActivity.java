package com.ast.zgloom;

import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.RelativeLayout;
import android.widget.TextView;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Locale;

/**
 * ZGloom main activity based on SDLActivity.
 *
 * This tells SDL which native libraries to load, in which order.
 * The final entry must be "main", which is the shared library that
 * contains the game's SDL_main(...) entry point.
 *
 * Additionally, this activity installs the bundled ZGloom game data
 * from the APK assets into the app-specific external storage directory:
 *
 *   /Android/data/com.ast.zgloom/files/ZGloom/
 *
 * on first launch (or when the data is missing).
 */
public class ZGloomActivity extends SDLActivity {

    private static final String TAG = "ZGloomActivity";
    private static final String ASSET_ROOT = "ZGloom";
    private static final String DATA_DIR_NAME = "ZGloom";
    private static final String DATA_INSTALL_MARKER = ".zgloom_data_v1";
    private static final String DATA_UPDATE6A_MARKER = ".zgloom_update6a_g3dc";
    private static final int COPY_BUFFER_SIZE = 64 * 1024;
    private static final int TOTAL_GAME_DATA_FILES = 717;

    private boolean mInstallStarted = false;
    private boolean mInstallFinished = false;
    private boolean mNativeThreadStarted = false;
    private boolean mBuildInfoVisibleRequested = false;

    private LinearLayout mInstallContainerView = null;
    private TextView mInstallHintView = null;
    private ProgressBar mInstallProgressBar = null;
    private TextView mInstallProgressView = null;
    private TextView mBuildInfoView = null;
    private GloomRetroTouchBridge mRetroTouchBridge = null;

    private long mLastProgressUiUpdateMs = 0L;
    private int mInstallCopiedFiles = 0;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // Let SDLActivity set up the window and layout first.
        super.onCreate(savedInstanceState);

        // Prepare the version/ABI overlay, but keep it hidden while the Android
        // installer may be shown. It is enabled right before the native boot
        // selector starts and disabled again by native hideBuildInfoOverlay()
        // or by the Java fallback when the selector-confirm button is pressed.
        ensureBuildInfoOverlay();
        hideBuildInfoOverlayOnUiThread();

        // RetroTouch is deliberately kept as an external AAR. The small
        // Gloom-specific bridge describes actions/layouts and feeds the native
        // input layer, so future compatible updates only replace the AAR file.
        //
        // ChromeOS is a keyboard/mouse platform for ZGloom, even on Chromebook
        // models that physically have a touchscreen. Never create RetroTouch
        // there: controller detection alone is insufficient because a
        // Chromebook can legitimately have no GAMEPAD/JOYSTICK device at all.
        // Keeping the view out of the hierarchy also guarantees that RetroTouch
        // cannot intercept mouse/touchpad events or show its settings control.
        if (mLayout != null) {
            if (SDLActivity.isChromebook()) {
                Log.i(TAG, "ChromeOS detected: RetroTouch disabled");
            } else {
                mRetroTouchBridge = new GloomRetroTouchBridge(this, mLayout);
            }
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (mRetroTouchBridge != null) {
            mRetroTouchBridge.onResume();
        }
    }

    @Override
    protected void onPause() {
        if (mRetroTouchBridge != null) {
            mRetroTouchBridge.onPause();
        }
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        if (mRetroTouchBridge != null) {
            mRetroTouchBridge.onDestroy();
        }
        super.onDestroy();
    }

    @Override
    protected void resumeNativeThread() {
        // Delay starting the native thread until game data is installed.
        if (mInstallFinished) {
            if (!mNativeThreadStarted) {
                startNativeThreadWithSelectorOverlay();
            } else {
                super.resumeNativeThread();
            }
            return;
        }

        if (isGameDataAlreadyInstalled()) {
            // Update 6A replaces Zombie Massacre's g3-dc title overlay even on
            // existing installations.  The normal first-install marker would
            // otherwise leave the older asset in external storage forever.
            installUpdate6AAssetsIfNeeded();
            mInstallFinished = true;
            startNativeThreadWithSelectorOverlay();
            return;
        }

        if (!mInstallStarted) {
            mInstallStarted = true;

            // The installer screen must not show the version/ABI overlay.
            hideBuildInfoOverlay();

            // Show a centered installer screen with progress while installing assets.
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    createInstallProgressView();
                }
            });

            // Run the asset installation on a background thread to avoid blocking the UI.
            new Thread(new Runnable() {
                @Override
                public void run() {
                    installGameDataIfNeeded();
                    installUpdate6AAssetsIfNeeded();
                    mInstallFinished = true;

                    // Once done, remove the hint and start the native thread.
                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            if (mLayout != null && mInstallContainerView != null) {
                                mLayout.removeView(mInstallContainerView);
                            }
                            mInstallContainerView = null;
                            mInstallHintView = null;
                            mInstallProgressBar = null;
                            mInstallProgressView = null;
                            startNativeThreadWithSelectorOverlay();
                        }
                    });
                }
            }, "ZGloomDataInstall").start();

            // Do not call super.resumeNativeThread() here; we will call it
            // from the background thread when installation has finished.
            return;
        }

        // Installation is still running; the background thread will resume
        // the native thread when it completes.
    }

    @Override
    protected String[] getLibraries() {
        // Order is important: SDL core, then optional add-ons, then the game.
        return new String[] {
                "SDL2",
                "SDL2_mixer",
                "xmp",
                "main"
        };
    }

    /**
     * Java-side fallback: if native hideBuildInfoOverlay() is missed for any
     * reason, hide the overlay when the user confirms/leaves the game selector.
     * DPAD navigation is intentionally ignored so the text stays visible while
     * the selector is being navigated.
     */
    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (event != null && event.getAction() == KeyEvent.ACTION_DOWN && isSelectorConfirmKey(event.getKeyCode())) {
            hideBuildInfoOverlay();
        }
        return super.dispatchKeyEvent(event);
    }

    private boolean isSelectorConfirmKey(int keyCode) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_ENTER:
            case KeyEvent.KEYCODE_NUMPAD_ENTER:
            case KeyEvent.KEYCODE_DPAD_CENTER:
            case KeyEvent.KEYCODE_BUTTON_A:
            case KeyEvent.KEYCODE_BUTTON_B:
            case KeyEvent.KEYCODE_BUTTON_START:
                return true;
            default:
                return false;
        }
    }

    private void startNativeThreadWithSelectorOverlay() {
        if (!mNativeThreadStarted) {
            mNativeThreadStarted = true;
            requestBuildInfoOverlayVisible();
        }
        ZGloomActivity.super.resumeNativeThread();

        if (mRetroTouchBridge != null) {
            mRetroTouchBridge.onNativeThreadStarted();
        }
    }

    /**
     * Called by native code when the Gloom state changes. The bridge performs
     * the UI-thread mode switch and suppresses touch while a controller exists.
     */
    public void setRetroTouchDesiredMode(int mode) {
        if (mRetroTouchBridge != null) {
            mRetroTouchBridge.setDesiredMode(mode);
        }
    }

    // JNI entry points used by GloomRetroTouchBridge. Action indices mirror
    // RetroTouchInput::Action; navigation is intentionally separate because
    // Gloom menus consume SDL key events while gameplay polls continuous state.
    static native void nativeRetroTouchSetAction(int action, boolean pressed);
    static native void nativeRetroTouchSetMove(float x, float y);
    static native void nativeRetroTouchAddLook(float deltaX, float deltaY);
    static native void nativeRetroTouchNavigation(int action, boolean pressed);
    static native void nativeRetroTouchReset();

    /**
     * Called by native code through JNI before the boot selector is displayed.
     */
    public void showBuildInfoOverlay() {
        requestBuildInfoOverlayVisible();
    }

    /**
     * Called by native code through JNI when the boot selector has been left.
     */
    public void hideBuildInfoOverlay() {
        mBuildInfoVisibleRequested = false;
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                hideBuildInfoOverlayOnUiThread();
            }
        });
    }

    private void requestBuildInfoOverlayVisible() {
        mBuildInfoVisibleRequested = true;
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (!mBuildInfoVisibleRequested) {
                    hideBuildInfoOverlayOnUiThread();
                    return;
                }
                showBuildInfoOverlayOnUiThread();
            }
        });
    }

    private void showBuildInfoOverlayOnUiThread() {
        ensureBuildInfoOverlay();
        if (mBuildInfoView != null) {
            mBuildInfoView.setText(makeBuildInfoText());
            mBuildInfoView.setVisibility(View.VISIBLE);
            mBuildInfoView.bringToFront();
            Log.i(TAG, "Build info overlay shown: " + mBuildInfoView.getText());
        }
    }

    private void hideBuildInfoOverlayOnUiThread() {
        if (mBuildInfoView != null) {
            mBuildInfoView.setVisibility(View.GONE);
            Log.i(TAG, "Build info overlay hidden");
        }
    }

    private void ensureBuildInfoOverlay() {
        if (mLayout == null) {
            return;
        }

        if (mBuildInfoView == null) {
            mBuildInfoView = new TextView(this);
            mBuildInfoView.setTextColor(0xFFCFCFCF);
            mBuildInfoView.setTextSize(11);
            mBuildInfoView.setGravity(Gravity.CENTER);
            mBuildInfoView.setSingleLine(true);
            mBuildInfoView.setIncludeFontPadding(false);
            mBuildInfoView.setShadowLayer(3.0f, 1.0f, 1.0f, 0xFF000000);
            mBuildInfoView.setText(makeBuildInfoText());

            final int sidePadding = dpToPx(8);
            final int bottomMargin = dpToPx(18);
            mBuildInfoView.setPadding(sidePadding, 0, sidePadding, 0);

            RelativeLayout.LayoutParams lp =
                    new RelativeLayout.LayoutParams(
                            RelativeLayout.LayoutParams.WRAP_CONTENT,
                            RelativeLayout.LayoutParams.WRAP_CONTENT);
            lp.addRule(RelativeLayout.ALIGN_PARENT_BOTTOM, RelativeLayout.TRUE);
            lp.addRule(RelativeLayout.CENTER_HORIZONTAL, RelativeLayout.TRUE);
            lp.setMargins(0, 0, 0, bottomMargin);
            mLayout.addView(mBuildInfoView, lp);
        } else {
            mBuildInfoView.setText(makeBuildInfoText());
        }
    }

    private String makeBuildInfoText() {
        return "v" + getVersionNameSafe() + " (" + getRuntimeAbiSafe() + ")";
    }

    private String getVersionNameSafe() {
        try {
            PackageInfo pi = getPackageManager().getPackageInfo(getPackageName(), 0);
            if (pi != null && pi.versionName != null && pi.versionName.length() > 0) {
                return pi.versionName;
            }
        } catch (PackageManager.NameNotFoundException ignored) {
        }
        return "unknown";
    }

    @SuppressWarnings("deprecation")
    private String getRuntimeAbiSafe() {
        // Prefer the directory of the loaded native libraries. This reflects the
        // ABI actually used by this APK, even when a 32-bit APK runs on a
        // 64-bit-capable Android device.
        try {
            String libDir = getApplicationInfo().nativeLibraryDir;
            if (libDir != null) {
                String lower = libDir.toLowerCase(Locale.US);
                if (lower.contains("arm64")) {
                    return "arm64-v8a";
                }
                if (lower.contains("armeabi-v7a") || lower.endsWith("/arm") || lower.contains("/arm/")) {
                    return "armeabi-v7a";
                }
            }
        } catch (Exception ignored) {
        }

        if (Build.VERSION.SDK_INT >= 21) {
            String[] abis = Build.SUPPORTED_ABIS;
            if (abis != null && abis.length > 0 && abis[0] != null && abis[0].length() > 0) {
                return abis[0];
            }
        }

        if (Build.CPU_ABI != null && Build.CPU_ABI.length() > 0) {
            return Build.CPU_ABI;
        }

        return "unknown-abi";
    }

    private int dpToPx(int dp) {
        float density = getResources().getDisplayMetrics().density;
        return (int)(dp * density + 0.5f);
    }

    private void createInstallProgressView() {
        if (mLayout == null || mInstallContainerView != null) {
            return;
        }

        mInstallContainerView = new LinearLayout(this);
        mInstallContainerView.setOrientation(LinearLayout.VERTICAL);
        mInstallContainerView.setGravity(Gravity.CENTER);

        final int sidePadding = dpToPx(24);
        final int progressWidth = dpToPx(320);
        mInstallContainerView.setPadding(sidePadding, 0, sidePadding, 0);

        mInstallHintView = new TextView(this);
        mInstallHintView.setText(getString(R.string.install_game_data_hint));
        mInstallHintView.setTextColor(0xFFFFFFFF);
        mInstallHintView.setTextSize(18);
        mInstallHintView.setGravity(Gravity.CENTER);
        mInstallHintView.setShadowLayer(3.0f, 1.0f, 1.0f, 0xFF000000);
        mInstallContainerView.addView(
                mInstallHintView,
                new LinearLayout.LayoutParams(
                        LinearLayout.LayoutParams.WRAP_CONTENT,
                        LinearLayout.LayoutParams.WRAP_CONTENT));

        mInstallProgressBar = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        mInstallProgressBar.setIndeterminate(false);
        mInstallProgressBar.setMax(1);
        mInstallProgressBar.setProgress(0);

        LinearLayout.LayoutParams progressLp =
                new LinearLayout.LayoutParams(
                        progressWidth,
                        LinearLayout.LayoutParams.WRAP_CONTENT);
        progressLp.setMargins(0, dpToPx(12), 0, 0);
        mInstallContainerView.addView(mInstallProgressBar, progressLp);

        mInstallProgressView = new TextView(this);
        mInstallProgressView.setText("0% - 0 of " + TOTAL_GAME_DATA_FILES + " files copied");
        mInstallProgressView.setTextColor(0xFFCFCFCF);
        mInstallProgressView.setTextSize(12);
        mInstallProgressView.setGravity(Gravity.CENTER);
        mInstallProgressView.setSingleLine(true);
        mInstallProgressView.setIncludeFontPadding(false);
        mInstallProgressView.setShadowLayer(3.0f, 1.0f, 1.0f, 0xFF000000);

        LinearLayout.LayoutParams textLp =
                new LinearLayout.LayoutParams(
                        LinearLayout.LayoutParams.WRAP_CONTENT,
                        LinearLayout.LayoutParams.WRAP_CONTENT);
        textLp.setMargins(0, dpToPx(4), 0, 0);
        mInstallContainerView.addView(mInstallProgressView, textLp);

        RelativeLayout.LayoutParams lp =
                new RelativeLayout.LayoutParams(
                        RelativeLayout.LayoutParams.WRAP_CONTENT,
                        RelativeLayout.LayoutParams.WRAP_CONTENT);
        lp.addRule(RelativeLayout.CENTER_IN_PARENT, RelativeLayout.TRUE);
        mLayout.addView(mInstallContainerView, lp);
    }

    private void updateInstallProgress(final int copiedFiles, final int totalFiles, boolean force) {
        long now = System.currentTimeMillis();
        if (!force && now - mLastProgressUiUpdateMs < 100 && copiedFiles < totalFiles) {
            return;
        }
        mLastProgressUiUpdateMs = now;

        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (mInstallProgressBar == null || mInstallProgressView == null) {
                    return;
                }

                int displayTotal = totalFiles;
                if (displayTotal <= 0) {
                    displayTotal = 1;
                }

                int displayCopied = copiedFiles;
                if (displayCopied < 0) {
                    displayCopied = 0;
                }

                int progressTotal = displayTotal;
                if (displayCopied > progressTotal) {
                    progressTotal = displayCopied;
                }

                mInstallProgressBar.setMax(progressTotal);
                mInstallProgressBar.setProgress(displayCopied);

                int percent = (int)((displayCopied * 100L) / displayTotal);
                if (percent > 100) {
                    percent = 100;
                }

                mInstallProgressView.setText(
                        String.format(Locale.US,
                                "%d%% - %d of %d files copied",
                                percent,
                                displayCopied,
                                displayTotal));
            }
        });
    }

    /**
     * Ensure that the ZGloom game data exists in:
     *   getExternalFilesDir(null)/ZGloom
     *
     * If not present, copy it recursively from APK assets/ZGloom.
     */
    private void installGameDataIfNeeded() {
        File ext = getExternalFilesDir(null);
        if (ext == null) {
            Log.e(TAG, "getExternalFilesDir(null) returned null; cannot install game data");
            return;
        }

        File dataRoot = new File(ext, DATA_DIR_NAME);
        File marker = new File(dataRoot, DATA_INSTALL_MARKER);

        if (marker.exists()) {
            Log.i(TAG, "Game data already installed at: " + dataRoot.getAbsolutePath());
            updateInstallProgress(TOTAL_GAME_DATA_FILES, TOTAL_GAME_DATA_FILES, true);
            return;
        }

        AssetManager am = getAssets();
        Log.i(TAG, "Installing ZGloom data from assets/" + ASSET_ROOT +
                " to " + dataRoot.getAbsolutePath());

        try {
            mInstallCopiedFiles = 0;
            updateInstallProgress(0, TOTAL_GAME_DATA_FILES, true);

            copyAssetTree(am, ASSET_ROOT, dataRoot);
            updateInstallProgress(mInstallCopiedFiles, TOTAL_GAME_DATA_FILES, true);

            // Create/refresh marker
            if (!dataRoot.exists() && !dataRoot.mkdirs()) {
                Log.w(TAG, "Failed to create dataRoot directory for marker: " + dataRoot);
            }
            try {
                if (marker.createNewFile()) {
                    Log.i(TAG, "Created data install marker: " + marker.getAbsolutePath());
                }
            } catch (IOException e) {
                Log.w(TAG, "Failed to create data install marker: " + marker.getAbsolutePath(), e);
            }
        } catch (IOException e) {
            Log.e(TAG, "Error while installing ZGloom data from assets", e);
        }
    }


    /**
     * Install the small Update 6A data replacement on already initialized data
     * folders.  This deliberately overwrites only Zombie Massacre's g3-dc and
     * its palette; all user game data and configuration files remain untouched.
     */
    private void installUpdate6AAssetsIfNeeded() {
        File ext = getExternalFilesDir(null);
        if (ext == null) {
            Log.w(TAG, "Update 6A: external files directory unavailable");
            return;
        }

        File dataRoot = new File(ext, DATA_DIR_NAME);
        File patchMarker = new File(dataRoot, DATA_UPDATE6A_MARKER);
        if (patchMarker.exists()) {
            return;
        }

        File massacrePixs = new File(new File(dataRoot, "massacre"), "pixs");
        AssetManager am = getAssets();
        try {
            copyAssetFile(am, ASSET_ROOT + "/massacre/pixs/g3-dc",
                    new File(massacrePixs, "g3-dc"));
            copyAssetFile(am, ASSET_ROOT + "/massacre/pixs/g3-dc.pal",
                    new File(massacrePixs, "g3-dc.pal"));

            if (!dataRoot.exists() && !dataRoot.mkdirs()) {
                Log.w(TAG, "Update 6A: failed to create data root for marker");
            }
            if (!patchMarker.exists() && !patchMarker.createNewFile()) {
                Log.w(TAG, "Update 6A: patch marker was not created");
            }
            Log.i(TAG, "Update 6A: Zombie Massacre g3-dc assets installed");
        } catch (IOException e) {
            Log.e(TAG, "Update 6A: failed to install Zombie Massacre g3-dc assets", e);
        }
    }

    private boolean isGameDataAlreadyInstalled() {
        File ext = getExternalFilesDir(null);
        if (ext == null) {
            return false;
        }
        File marker = new File(new File(ext, DATA_DIR_NAME), DATA_INSTALL_MARKER);
        return marker.exists();
    }

    /**
     * Recursively copy an asset directory tree into the given destination directory.
     *
     * Progress is based on TOTAL_GAME_DATA_FILES and updated only while real files
     * are copied. No separate pre-scan is performed, so the installer avoids an
     * extra AssetManager traversal on slow Android 4.x devices like the OUYA.
     *
     * @param am         AssetManager
     * @param assetPath  Path inside the APK assets (e.g. "ZGloom" or "ZGloom/subdir")
     * @param dest       Destination file or directory under external storage
     */
    private void copyAssetTree(AssetManager am, String assetPath, File dest) throws IOException {
        String[] names = am.list(assetPath);
        if (names == null || names.length == 0) {
            copyAssetFile(am, assetPath, dest);
            mInstallCopiedFiles++;
            updateInstallProgress(
                    mInstallCopiedFiles,
                    TOTAL_GAME_DATA_FILES,
                    mInstallCopiedFiles >= TOTAL_GAME_DATA_FILES);
            return;
        }

        if (!dest.exists() && !dest.mkdirs()) {
            Log.w(TAG, "Failed to create directory: " + dest.getAbsolutePath());
        }

        for (String name : names) {
            String childAssetPath = assetPath + "/" + name;
            File childDest = new File(dest, name);
            copyAssetTree(am, childAssetPath, childDest);
        }
    }

    /**
     * Copy a single asset file to the given destination file.
     */
    private void copyAssetFile(AssetManager am, String assetPath, File destFile) throws IOException {
        InputStream in = null;
        OutputStream out = null;
        try {
            in = am.open(assetPath);
            File parent = destFile.getParentFile();
            if (parent != null && !parent.exists() && !parent.mkdirs()) {
                Log.w(TAG, "Failed to create parent directory: " + parent.getAbsolutePath());
            }
            out = new FileOutputStream(destFile);

            byte[] buffer = new byte[COPY_BUFFER_SIZE];
            int read;
            while ((read = in.read(buffer)) != -1) {
                out.write(buffer, 0, read);
            }
            out.flush();
        } finally {
            if (in != null) {
                try { in.close(); } catch (IOException ignored) {}
            }
            if (out != null) {
                try { out.close(); } catch (IOException ignored) {}
            }
        }
    }
}
