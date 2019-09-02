package com.sti.usbcameralibrary;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import androidx.annotation.NonNull;

import java.io.File;

public class USBCameraLib {
    private static boolean isInited = false;

    protected static String IMG_SAVE_ROOT_PATH;

    /**
     * 初始化
     *
     * @param photoRootPath 图片储存根目录
     */
    public static void init(@NonNull String photoRootPath) {
        IMG_SAVE_ROOT_PATH = photoRootPath;

        File file = new File(IMG_SAVE_ROOT_PATH);
        if (!file.canRead()) {
            file.mkdirs();
        }

        isInited = true;
    }

    public static void openUSBCamera(Activity activity, int requestCode) {
        if (isInited) {
            activity.startActivityForResult(new Intent(activity, USBCameraActivity.class), requestCode);
        } else {
            throw new IllegalStateException("USBCameraLib need init!");
        }
    }
}
