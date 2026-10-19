/*
 * Copyright (C) 2026 The LineageOS project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.systemui.biometrics;

import android.content.Context;
import android.provider.Settings;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;

/**
 * Controller for managing the UDFPS HBM (High Brightness Mode) Scrim on MediaTek devices lacking LHBM support.
 *
 * Logic reversed from Smali: Lcom/android/systemui/biometrics/PriUdfpsScrimController; from LAVA
 */
public class MtkUdfpsScrimController {
    private static MtkUdfpsScrimController mInstance;

    // Accessed directly by UdfpsController
    public View mScrimView;
    public WindowManager mWindowManager;

    private final Context mContext;

    public MtkUdfpsScrimController(Context context) {
        mContext = context;
    }

    public static MtkUdfpsScrimController getInstance(Context context) {
        if (mInstance == null) {
            mInstance = new MtkUdfpsScrimController(context);
        }
        return mInstance;
    }

    /**
     * Calculates the alpha (transparency) for the HBM overlay based on screen brightness.
     * The formula ensures the sensor gets a consistent amount of light.
     *
     * @param brightness Current system brightness (0-255)
     * @return Alpha value (0.0f to 1.0f)
     */
    public float calculateAlpha(int brightness) {
        int b = Math.max(0, Math.min(255, brightness));

        // Device-specific FOD alpha calibration.
        // Format: brightness:alpha,brightness:alpha,...
        // Example: 0:0.85,128:0.40,255:0.20
        //
        // The setting can be updated through ADB without rebuilding.
        String curve = null;

        if (mContext != null) {
            curve = Settings.Global.getString(
                mContext.getContentResolver(),
                "nashc_fod_alpha_curve"
            );
        }

        // Default nashc calibration for fresh ROM installs.
        // Settings.Global can still override this curve.
        if (curve == null || curve.isEmpty()) {
            curve = "0:0.94,4:0.82,8:0.64,10:0.61,22:0.525,40:0.43,100:0.22,180:0.18,220:0.15,255:0.11";
        }

        if (curve != null && !curve.isEmpty()
                && curve.length() <= 512) {
            try {
                int previousBrightness = -1;
                float previousAlpha = 0.0f;

                for (String entry : curve.split(",")) {
                    String[] values = entry.trim().split(":");

                    if (values.length != 2) {
                        throw new IllegalArgumentException(
                            "Invalid FOD calibration entry"
                        );
                    }

                    int level = Integer.parseInt(values[0].trim());
                    float alpha = Float.parseFloat(values[1].trim());

                    if (level < 0 || level > 255
                            || level <= previousBrightness
                            || !Float.isFinite(alpha)
                            || alpha < 0.0f || alpha > 1.0f) {
                        throw new IllegalArgumentException(
                            "Invalid FOD calibration values"
                        );
                    }

                    if (b <= level) {
                        float result;

                        if (previousBrightness < 0) {
                            result = alpha;
                        } else {
                            float fraction =
                                (float) (b - previousBrightness) /
                                (level - previousBrightness);

                            result = previousAlpha +
                                fraction * (alpha - previousAlpha);
                        }

                        Log.d("MtkUdfpsScrimController",
                            "Calibrated FOD alpha: brightness="
                            + b + " alpha=" + result);

                        return result;
                    }

                    previousBrightness = level;
                    previousAlpha = alpha;
                }

                Log.d("MtkUdfpsScrimController",
                    "Calibrated FOD alpha: brightness="
                    + b + " alpha=" + previousAlpha);

                return previousAlpha;

            } catch (IllegalArgumentException e) {
                Log.w("MtkUdfpsScrimController",
                    "Invalid calibration; using original alpha", e);
            }
        }

        // Original Axion behavior when calibration is absent.
        float alpha = 1.0f - (b / 255.0f);

        if (b < 25) {
            alpha *= 0.95f;
        }

        return Math.max(0.0f, Math.min(1.0f, alpha));
    }

    public int getSystemBrightness() {
        if (mContext == null) {
            return 127;
        }

        // Try Float first
        float brightFloat = Settings.System.getFloat(
            mContext.getContentResolver(),
            "screen_brightness_float",
            -1.0f
        );

        if (brightFloat >= 0.0f) {
            return (int) (brightFloat * 255.0f);
        }

        // Fallback
        return Settings.System.getInt(
            mContext.getContentResolver(),
            Settings.System.SCREEN_BRIGHTNESS,
            127
        );
    }
}
