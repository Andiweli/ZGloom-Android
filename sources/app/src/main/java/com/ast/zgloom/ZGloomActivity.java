package com.ast.zgloom;

import android.content.res.AssetManager;
import android.os.Bundle;
import android.util.Log;
import android.view.Gravity;
import android.widget.RelativeLayout;
import android.widget.TextView;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

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

    private boolean mInstallStarted = false;
    private boolean mInstallFinished = false;
    private TextView mInstallHintView = null;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // Let SDLActivity set up the window and layout first.
        super.onCreate(savedInstanceState);
    }

        @Override
    protected void resumeNativeThread() {
        // Delay starting the native thread until game data is installed.
        if (mInstallFinished) {
            super.resumeNativeThread();
            return;
        }

        if (!mInstallStarted) {
            mInstallStarted = true;

            // Show a simple centered message while installing assets.
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    if (mLayout == null) {
                        return;
                    }
                    mInstallHintView = new TextView(ZGloomActivity.this);
                    mInstallHintView.setText(getString(R.string.install_game_data_hint));
                    mInstallHintView.setTextColor(0xFFFFFFFF);
                    mInstallHintView.setTextSize(18);
                    mInstallHintView.setGravity(Gravity.CENTER);

                    RelativeLayout.LayoutParams lp =
                            new RelativeLayout.LayoutParams(
                                    RelativeLayout.LayoutParams.WRAP_CONTENT,
                                    RelativeLayout.LayoutParams.WRAP_CONTENT);
                    lp.addRule(RelativeLayout.CENTER_IN_PARENT, RelativeLayout.TRUE);
                    mLayout.addView(mInstallHintView, lp);
                }
            });

            // Run the asset installation on a background thread to avoid blocking the UI.
            new Thread(new Runnable() {
                @Override
                public void run() {
                    installGameDataIfNeeded();
                    mInstallFinished = true;

                    // Once done, remove the hint and start the native thread.
                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            if (mLayout != null && mInstallHintView != null) {
                                mLayout.removeView(mInstallHintView);
                                mInstallHintView = null;
                            }
                            ZGloomActivity.super.resumeNativeThread();
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
            return;
        }

        AssetManager am = getAssets();
        Log.i(TAG, "Installing ZGloom data from assets/" + ASSET_ROOT +
                " to " + dataRoot.getAbsolutePath());

        try {
            copyAssetTree(am, ASSET_ROOT, dataRoot);
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
     * Recursively copy an asset directory tree into the given destination directory.
     *
     * @param am         AssetManager
     * @param assetPath  Path inside the APK assets (e.g. "ZGloom" or "ZGloom/subdir")
     * @param dest       Destination file or directory under external storage
     */
    private void copyAssetTree(AssetManager am, String assetPath, File dest) throws IOException {
        String[] names = am.list(assetPath);
        if (names == null || names.length == 0) {
            // This is a file, copy it
            copyAssetFile(am, assetPath, dest);
            return;
        }

        // It's a directory
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

            byte[] buffer = new byte[8192];
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
