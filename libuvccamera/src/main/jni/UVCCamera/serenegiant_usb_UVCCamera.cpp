/*
 * UVCCamera
 * library and sample to access to UVC web camera on non-rooted Android device
 *
 * Copyright (c) 2014-2017 saki t_saki@serenegiant.com
 *
 * File name: serenegiant_usb_UVCCamera.cpp
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 * All files in the folder are under this Apache License, Version 2.0.
 * Files in the jni/libjpeg, jni/libusb, jin/libuvc, jni/rapidjson folder may have a different license, see the respective files.
*/

#if 1    // デバッグ情報を出さない時
#ifndef LOG_NDEBUG
#define	LOG_NDEBUG		// LOGV/LOGD/MARKを出力しない時
#endif
#undef USE_LOGALL            // 指定したLOGxだけを出力
#else
#define USE_LOGALL
#undef LOG_NDEBUG
#undef NDEBUG
#endif

#include <jni.h>
#include <android/native_window_jni.h>
#include <math.h>
#include <iosfwd>
#include <string>

#include "libUVCCamera.h"
#include "UVCCamera.h"

//uvc_c9_control.basic.layout_dst_width = UVC_C9_BASIC_LAYOUT_DST_WIDTH;
//uvc_c9_control.basic.layout_dst_height = UVC_C9_BASIC_LAYOUT_DST_HEIGHT;
//uvc_c9_control.basic.layout_src_width = UVC_C9_BASIC_LAYOUT_SRC_WIDTH;
//uvc_c9_control.basic.layout_src_height = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;

int UVC_C9_BASIC_LAYOUT_DST_WIDTH = 1920;
int UVC_C9_BASIC_LAYOUT_DST_HEIGHT = 1080;
int UVC_C9_BASIC_LAYOUT_SRC_WIDTH = 3840;
int UVC_C9_BASIC_LAYOUT_SRC_HEIGHT = 640;
int UVC_C9_DIVIDER_HEIGHT = 2;

#define GET_LEGAL_LOCATION(a) a % 16==0?a:a*1;
#define GET_LEGAL_LOCATION_(a) a % 16==0?a:a*1;
//得到坐标点对16取模，如果结果小于8，减去后为最接近的16倍数的值，大于8，加上不足16的部分，得到最近值
#define GET_LEGAL_LOCATION_PARAMS(a) a % 16 == 0? a :a % 16 <= 8 ? a - a % 16  : a + (16 - a % 16) ;
#define GET_LEGAL_DST_HEIGHT_PARAMS(a) a >UVC_C9_BASIC_LAYOUT_SRC_HEIGHT?UVC_C9_BASIC_LAYOUT_SRC_HEIGHT+10:a+10 ;

//
int UVC_C9_BASIC_LAYOUT_PER_SRC_WIDTH = UVC_C9_BASIC_LAYOUT_SRC_WIDTH / 4;
int UVC_C9_BASIC_LAYOUT_PER_DST_WIDTH = UVC_C9_BASIC_LAYOUT_DST_WIDTH / 2;
int UVC_C9_BASIC_LAYOUT_PER_DST_HALF_WIDTH = UVC_C9_BASIC_LAYOUT_DST_WIDTH / 2;
int UVC_C9_BASIC_LAYOUT_PER_CAMERA_DST_HALF_WIDTH = UVC_C9_BASIC_LAYOUT_DST_WIDTH / 4;
int UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH = UVC_C9_BASIC_LAYOUT_SRC_WIDTH / 8;
int UVC_C9_BASIC_LAYOUT_DEST_INTERVAL_WIDTH = UVC_C9_BASIC_LAYOUT_DST_WIDTH / 2;
int UVC_C9_HALF_OF_DEST_WIDTH = UVC_C9_BASIC_LAYOUT_DST_WIDTH / 2;
int UVC_C9_BASIC_LAYOUT_DEST_HEIGHT_MARGIN = 320;
int UVC_C9_CAMERA_0_LEFT_X = 0;
int UVC_C9_CAMERA_0_RIGHT_X =
        UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH + UVC_C9_BASIC_LAYOUT_PER_SRC_WIDTH * 3;
int UVC_C9_CAMERA_1_X = UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH;
int UVC_C9_CAMERA_2_X =
        UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH + UVC_C9_BASIC_LAYOUT_PER_SRC_WIDTH;
int UVC_C9_CAMERA_3_X =
        UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH + UVC_C9_BASIC_LAYOUT_PER_SRC_WIDTH * 2;
int UVC_360_DY = GET_LEGAL_DST_HEIGHT_PARAMS(
        (UVC_C9_BASIC_LAYOUT_DST_HEIGHT - UVC_C9_BASIC_LAYOUT_DST_WIDTH / 6));
int UVC_360_HEIGHT = UVC_C9_BASIC_LAYOUT_DST_WIDTH / 6;
float UVC_SCALE = 1;

int SPACE_WIDTH_HEIGHT = 16;
int UVC_HALF_OF_DST_HEIGHT = UVC_C9_BASIC_LAYOUT_DST_HEIGHT / 2;
#define GET_LEGAL_DST_HEIGHT_PARAMS(a) a >UVC_C9_BASIC_LAYOUT_SRC_HEIGHT?UVC_C9_BASIC_LAYOUT_SRC_HEIGHT:a ;


int ret = -1;

/**
 * set the value into the long field
 * @param env: this param should not be null
 * @param bullet_obj: this param should not be null
 * @param field_name
 * @params val
 */
static jlong setField_long(JNIEnv *env, jobject java_obj, const char *field_name, jlong val) {
#if LOCAL_DEBUG
    LOGV("setField_long:");
#endif

    jclass clazz = env->GetObjectClass(java_obj);
    jfieldID field = env->GetFieldID(clazz, field_name, "J");
    if (LIKELY(field))
        env->SetLongField(java_obj, field, val);
    else {
        LOGE("__setField_long:field '%s' not found", field_name);
    }
#ifdef ANDROID_NDK
    env->DeleteLocalRef(clazz);
#endif
    return val;
}

/**
 * @param env: this param should not be null
 * @param bullet_obj: this param should not be null
 */
static jlong
__setField_long(JNIEnv *env, jobject java_obj, jclass clazz, const char *field_name, jlong val) {
#if LOCAL_DEBUG
    LOGV("__setField_long:");
#endif

    jfieldID field = env->GetFieldID(clazz, field_name, "J");
    if (LIKELY(field))
        env->SetLongField(java_obj, field, val);
    else {
        LOGE("__setField_long:field '%s' not found", field_name);
    }
    return val;
}

/**
 * @param env: this param should not be null
 * @param bullet_obj: this param should not be null
 */
jint __setField_int(JNIEnv *env, jobject java_obj, jclass clazz, const char *field_name, jint val) {
    LOGV("__setField_int:");

    jfieldID id = env->GetFieldID(clazz, field_name, "I");
    if (LIKELY(id))
        env->SetIntField(java_obj, id, val);
    else {
        LOGE("__setField_int:field '%s' not found", field_name);
        env->ExceptionClear();    // clear java.lang.NoSuchFieldError exception
    }
    return val;
}

/**
 * set the value into int field
 * @param env: this param should not be null
 * @param java_obj: this param should not be null
 * @param field_name
 * @params val
 */
jint setField_int(JNIEnv *env, jobject java_obj, const char *field_name, jint val) {
    LOGV("setField_int:");

    jclass clazz = env->GetObjectClass(java_obj);
    __setField_int(env, java_obj, clazz, field_name, val);
#ifdef ANDROID_NDK
    env->DeleteLocalRef(clazz);
#endif
    return val;
}

static ID_TYPE nativeCreate(JNIEnv *env, jobject thiz) {

    ENTER();
    UVCCamera *camera = new UVCCamera();
    setField_long(env, thiz, "mNativePtr", reinterpret_cast<ID_TYPE>(camera));
    RETURN(reinterpret_cast<ID_TYPE>(camera), ID_TYPE);
}

// native側のカメラオブジェクトを破棄
static void nativeDestroy(JNIEnv *env, jobject thiz,
                          ID_TYPE id_camera) {

    ENTER();
    setField_long(env, thiz, "mNativePtr", 0);
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        SAFE_DELETE(camera);
    }
    EXIT();
}

//======================================================================
// カメラへ接続
static jint nativeConnect(JNIEnv *env, jobject thiz,
                          ID_TYPE id_camera,
                          jint vid, jint pid, jint fd,
                          jint busNum, jint devAddr, jstring usbfs_str) {

    ENTER();
    int result = JNI_ERR;
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    const char *c_usbfs = env->GetStringUTFChars(usbfs_str, JNI_FALSE);
    if (LIKELY(camera && (fd > 0))) {
//		libusb_set_debug(NULL, LIBUSB_LOG_LEVEL_DEBUG);
        result = camera->connect(vid, pid, fd, busNum, devAddr, c_usbfs);
    }
    env->ReleaseStringUTFChars(usbfs_str, c_usbfs);
    RETURN(result, jint);
}

// カメラとの接続を解除
static jint nativeRelease(JNIEnv *env, jobject thiz,
                          ID_TYPE id_camera) {

    ENTER();
    int result = JNI_ERR;
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->release();
    }
    RETURN(result, jint);
}

//======================================================================
static jint nativeSetStatusCallback(JNIEnv *env, jobject thiz,
                                    ID_TYPE id_camera, jobject jIStatusCallback) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        jobject status_callback_obj = env->NewGlobalRef(jIStatusCallback);
        result = camera->setStatusCallback(env, status_callback_obj);
    }
    RETURN(result, jint);
}

static jint nativeSetButtonCallback(JNIEnv *env, jobject thiz,
                                    ID_TYPE id_camera, jobject jIButtonCallback) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        jobject button_callback_obj = env->NewGlobalRef(jIButtonCallback);
        result = camera->setButtonCallback(env, button_callback_obj);
    }
    RETURN(result, jint);
}

static jobject nativeGetSupportedSize(JNIEnv *env, jobject thiz,
                                      ID_TYPE id_camera) {

    ENTER();
    jstring result = NULL;
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        char *c_str = camera->getSupportedSize();
        if (LIKELY(c_str)) {
            result = env->NewStringUTF(c_str);
            free(c_str);
        }
    }
    RETURN(result, jobject);
}

//======================================================================
// プレビュー画面の大きさをセット
static jint nativeSetPreviewSize(JNIEnv *env, jobject thiz,
                                 ID_TYPE id_camera, jint width, jint height, jint min_fps,
                                 jint max_fps, jint mode, jfloat bandwidth) {

    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        return camera->setPreviewSize(width, height, min_fps, max_fps, mode, bandwidth);
    }
    RETURN(JNI_ERR, jint);
}

static jint nativeStartPreview(JNIEnv *env, jobject thiz,
                               ID_TYPE id_camera) {

    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        return camera->startPreview();
    }
    RETURN(JNI_ERR, jint);
}

// プレビューを停止
static jint nativeStopPreview(JNIEnv *env, jobject thiz,
                              ID_TYPE id_camera) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->stopPreview();
    }
    RETURN(result, jint);
}

static jint nativeSetPreviewDisplay(JNIEnv *env, jobject thiz,
                                    ID_TYPE id_camera, jobject jSurface) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        ANativeWindow *preview_window = jSurface ? ANativeWindow_fromSurface(env, jSurface) : NULL;
        result = camera->setPreviewDisplay(preview_window);
    }
    RETURN(result, jint);
}

static jint nativeSetFrameCallback(JNIEnv *env, jobject thiz,
                                   ID_TYPE id_camera, jobject jIFrameCallback, jint pixel_format) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        jobject frame_callback_obj = env->NewGlobalRef(jIFrameCallback);
        result = camera->setFrameCallback(env, frame_callback_obj, pixel_format);
    }
    RETURN(result, jint);
}

static jint nativeSetCaptureDisplay(JNIEnv *env, jobject thiz,
                                    ID_TYPE id_camera, jobject jSurface) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        ANativeWindow *capture_window = jSurface ? ANativeWindow_fromSurface(env, jSurface) : NULL;
        result = camera->setCaptureDisplay(capture_window);
    }
    RETURN(result, jint);
}

//======================================================================
// カメラコントロールでサポートしている機能を取得する
static jlong nativeGetCtrlSupports(JNIEnv *env, jobject thiz,
                                   ID_TYPE id_camera) {

    jlong result = 0;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        uint64_t supports;
        int r = camera->getCtrlSupports(&supports);
        if (!r)
            result = supports;
    }
    RETURN(result, jlong);
}

// プロセッシングユニットでサポートしている機能を取得する
static jlong nativeGetProcSupports(JNIEnv *env, jobject thiz,
                                   ID_TYPE id_camera) {

    jlong result = 0;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        uint64_t supports;
        int r = camera->getProcSupports(&supports);
        if (!r)
            result = supports;
    }
    RETURN(result, jlong);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdateScanningModeLimit(JNIEnv *env, jobject thiz,
                                          ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateScanningModeLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mScanningModeMin", min);
            setField_int(env, thiz, "mScanningModeMax", max);
            setField_int(env, thiz, "mScanningModeDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetScanningMode(JNIEnv *env, jobject thiz,
                                  ID_TYPE id_camera, jint scanningMode) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setScanningMode(scanningMode);
    }
    RETURN(result, jint);
}

static jint nativeGetScanningMode(JNIEnv *env, jobject thiz,
                                  ID_TYPE id_camera) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getScanningMode();
    }
    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdateExposureModeLimit(JNIEnv *env, jobject thiz,
                                          ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateExposureModeLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mExposureModeMin", min);
            setField_int(env, thiz, "mExposureModeMax", max);
            setField_int(env, thiz, "mExposureModeDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetExposureMode(JNIEnv *env, jobject thiz,
                                  ID_TYPE id_camera, int exposureMode) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setExposureMode(exposureMode);
    }
    RETURN(result, jint);
}

static jint nativeGetExposureMode(JNIEnv *env, jobject thiz,
                                  ID_TYPE id_camera) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getExposureMode();
    }
    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdateExposurePriorityLimit(JNIEnv *env, jobject thiz,
                                              ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateExposurePriorityLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mExposurePriorityMin", min);
            setField_int(env, thiz, "mExposurePriorityMax", max);
            setField_int(env, thiz, "mExposurePriorityDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetExposurePriority(JNIEnv *env, jobject thiz,
                                      ID_TYPE id_camera, int priority) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setExposurePriority(priority);
    }
    RETURN(result, jint);
}

static jint nativeGetExposurePriority(JNIEnv *env, jobject thiz,
                                      ID_TYPE id_camera) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getExposurePriority();
    }
    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdateExposureLimit(JNIEnv *env, jobject thiz,
                                      ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateExposureLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mExposureMin", min);
            setField_int(env, thiz, "mExposureMax", max);
            setField_int(env, thiz, "mExposureDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetExposure(JNIEnv *env, jobject thiz,
                              ID_TYPE id_camera, int exposure) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setExposure(exposure);
    }
    RETURN(result, jint);
}

static jint nativeGetExposure(JNIEnv *env, jobject thiz,
                              ID_TYPE id_camera) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getExposure();
    }
    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdateExposureRelLimit(JNIEnv *env, jobject thiz,
                                         ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateExposureRelLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mExposureRelMin", min);
            setField_int(env, thiz, "mExposureRelMax", max);
            setField_int(env, thiz, "mExposureRelDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetExposureRel(JNIEnv *env, jobject thiz,
                                 ID_TYPE id_camera, jint exposure_rel) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setExposureRel(exposure_rel);
    }
    RETURN(result, jint);
}

static jint nativeGetExposureRel(JNIEnv *env, jobject thiz,
                                 ID_TYPE id_camera) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getExposureRel();
    }
    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdateAutoFocusLimit(JNIEnv *env, jobject thiz,
                                       ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateAutoFocusLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mAutoFocusMin", min);
            setField_int(env, thiz, "mAutoFocusMax", max);
            setField_int(env, thiz, "mAutoFocusDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetAutoFocus(JNIEnv *env, jobject thiz,
                               ID_TYPE id_camera, jboolean autofocus) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setAutoFocus(autofocus);
    }
    RETURN(result, jint);
}

static jint nativeGetAutoFocus(JNIEnv *env, jobject thiz,
                               ID_TYPE id_camera) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getAutoFocus();
    }
    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdateAutoWhiteBlanceLimit(JNIEnv *env, jobject thiz,
                                             ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateAutoWhiteBlanceLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mAutoWhiteBlanceMin", min);
            setField_int(env, thiz, "mAutoWhiteBlanceMax", max);
            setField_int(env, thiz, "mAutoWhiteBlanceDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetAutoWhiteBlance(JNIEnv *env, jobject thiz,
                                     ID_TYPE id_camera, jboolean autofocus) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setAutoWhiteBlance(autofocus);
    }
    RETURN(result, jint);
}

static jint nativeGetAutoWhiteBlance(JNIEnv *env, jobject thiz,
                                     ID_TYPE id_camera) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getAutoWhiteBlance();
    }
    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdateAutoWhiteBlanceCompoLimit(JNIEnv *env, jobject thiz,
                                                  ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateAutoWhiteBlanceCompoLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mAutoWhiteBlanceCompoMin", min);
            setField_int(env, thiz, "mAutoWhiteBlanceCompoMax", max);
            setField_int(env, thiz, "mAutoWhiteBlanceCompoDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetAutoWhiteBlanceCompo(JNIEnv *env, jobject thiz,
                                          ID_TYPE id_camera, jboolean autofocus_compo) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setAutoWhiteBlanceCompo(autofocus_compo);
    }
    RETURN(result, jint);
}

static jint nativeGetAutoWhiteBlanceCompo(JNIEnv *env, jobject thiz,
                                          ID_TYPE id_camera) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getAutoWhiteBlanceCompo();
    }
    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdateBrightnessLimit(JNIEnv *env, jobject thiz,
                                        ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateBrightnessLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mBrightnessMin", min);
            setField_int(env, thiz, "mBrightnessMax", max);
            setField_int(env, thiz, "mBrightnessDef", def);
        }
    }
    RETURN(result, jint);
}


//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdateCameraSizeCurLimit(JNIEnv *env, jobject thiz,
                                           ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateCameraSizeCurLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mCameraSizeCurMin", min);
            setField_int(env, thiz, "mCameraSizeCurMax", max);
            setField_int(env, thiz, "mCameraSizeCurDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetBrightness(JNIEnv *env, jobject thiz,
                                ID_TYPE id_camera, jint brightness) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setBrightness(brightness);
    }
    RETURN(result, jint);
}

static jint nativeGetBrightness(JNIEnv *env, jobject thiz,
                                ID_TYPE id_camera) {

    jint result = 0;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getBrightness();
    }
    RETURN(result, jint);
}

/**
 * 设置当前分辨率
 * @param env
 * @param thiz
 * @param id_camera
 * @param width 宽
 * @param height 高
 * @return
 */
static jint nativeSetResolution(JNIEnv *env, jobject thiz,
                                ID_TYPE id_camera, jint width, jint height) {
    uvc_c4_control uvcC4Control;
    uvcC4Control.width = width;
    uvcC4Control.height = height;
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setResolution(&uvcC4Control);
    }
    RETURN(result, jint);
}

/**
 * 获取当前分辨率
 * @param env
 * @param thiz
 * @param id_camera
 * @return arr[0] width;
 *         arr[1] height;
 *         arr[2] hfov;
 *         arr[3] vfov;
 *         arr[4] max_hfov;
 *         arr[5] max_vfov
 *
 */
static jintArray nativeGetResolution(JNIEnv *env, jobject thiz,
                                     ID_TYPE id_camera) {
    uint16_t value[8];
    jintArray result;
    int jsize1 = 8;
    result = (*env).NewIntArray(jsize1);
    jint resultCode = 0;
    jint fill[8];

    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        resultCode = camera->getResolution(value);
    }
    for (int i = 0; i < 8; i++) {
        fill[i] = value[i];
    }
    (*env).SetIntArrayRegion(result, 0, jsize1, fill);

    return result;
}

/**
 * 获取原始尺寸和目标尺寸
 * @param env
 * @param thiz
 * @param id_camera
 * @return
 */
static jshortArray nativeGetClipLocation(JNIEnv *env, jobject thiz,
                                         ID_TYPE id_camera) {
    uint8_t value[60];
    jshortArray result;
    int jsize1 = 60;
    result = (*env).NewShortArray(jsize1);
    jint resultCode = 0;
    jshort fill[60];

    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        resultCode = camera->getClipLocation(value);
    }
    for (int i = 0; i < 8; i++) {
        fill[i] = value[i]; // put whatever logic you want to populate the values here.
    }
    (*env).SetShortArrayRegion(result, 0, jsize1, fill);

//    for (int i = 0; i < 8; ++i) {
//        LOGI("ctrl---cpp %d", value[i]);
//    }
    return result;
//    RETURN(resultCode, jint);
}

static jshortArray nativeGetC1Version(JNIEnv *env, jobject thiz,
                                      ID_TYPE id_camera) {
    uint8_t value[60];
    jshortArray result;
    int jsize1 = 60;
    result = (*env).NewShortArray(jsize1);
    jint resultCode = 0;
    jshort fill[60];

    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    uvc_c1_control uvcC1ControlInfo;
    if (LIKELY(camera)) {
        resultCode = camera->getCtrlC1(&uvcC1ControlInfo);
    }
    LOGI("ret=%d,\nxu_api_level = %d,\ncfv_firmware = %s,\nall_firmware_version = %s", resultCode,
         uvcC1ControlInfo.xu_api_level, uvcC1ControlInfo.cfv_firmware,
         uvcC1ControlInfo.all_firmware_version);
    for (int i = 0; i < 8; i++) {
        fill[i] = value[i]; // put whatever logic you want to populate the values here.
    }
    (*env).SetShortArrayRegion(result, 0, jsize1, fill);

//    for (int i = 0; i < 8; ++i) {
//        LOGI("ctrl---cpp %d", value[i]);
//    }
    return result;
//    RETURN(resultCode, jint);
}

/**
 * 设置裁剪
 * @param env
 * @param thiz
 * @param id_camera
 * @return
 */
static jint nativeSetClipLocation(JNIEnv *env, jobject thiz,
                                  ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    uvc_c9_control_info uvcC9ControlInfo;
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setClipLocation(uvcC9ControlInfo);
    }
    RETURN(result, jint);
}


enum C9_HEADER {
    C9_BASIC = 0xE0000001,
    C9_WINDOW,
    C9_TRIGGER
};
enum C9_MODE {
    C9_GRID,
    C9_PER_CAMERA,
    C9_360,
    C9_MIC,
    C9_SPEAKER_CUSTOM_360_PIP,
    C9_STABLE_360,
    C9_STABLE,
    C9_SCROLL_360
};
C9_MODE c9Mode = C9_360;

static jint nativeGetC9Mode(JNIEnv *env, jobject thiz,
                            ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    if (c9Mode == C9_360) {
        result = 0;
    } else if (c9Mode == C9_PER_CAMERA) {
        result = 99;
    } else if (c9Mode == C9_SCROLL_360) {
        result = 6;
    } else if (c9Mode == C9_STABLE) {
        result = 5;
    } else if (c9Mode == C9_GRID) {
        result = 4;
    } else if (c9Mode == C9_SPEAKER_CUSTOM_360_PIP) {
        result = 3;
    } else if (c9Mode == C9_MIC) {
        result = 2;
    } else if (c9Mode == C9_STABLE_360) {
        result = 1;
    } else {
        result = 0;
    }
    RETURN(result, jint);
}

static jint nativeSetClipMode(JNIEnv *env, jobject thiz,
                              ID_TYPE id_camera, jint mode) {
    jint result = JNI_ERR;
    ENTER();
    uvc_c9_control_info uvcC9ControlInfo1;

    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        if (mode == 0) {
            c9Mode = C9_360;
        } else if (mode == 99) {
            c9Mode = C9_PER_CAMERA;
        } else if (mode == 4) {
            c9Mode = C9_GRID;
        } else if (mode == 2) {
            c9Mode = C9_MIC;
        } else if (mode == 3) {
            c9Mode = C9_SPEAKER_CUSTOM_360_PIP;
        } else if (mode == 5) {
            c9Mode = C9_STABLE;
        } else if (mode == 1) {
            c9Mode = C9_STABLE_360;
        } else if (mode == 6) {
            c9Mode = C9_SCROLL_360;
        }
    }
    RETURN(result, jint);
}

static jint nativeSetDensity(JNIEnv *env, jobject thiz,
                             ID_TYPE id_camera, jint src_width, jint src_height,
                             jint dst_width, jint dst_height) {
    jint result = JNI_ERR;
    ENTER();
    UVC_C9_BASIC_LAYOUT_SRC_WIDTH = src_width;
    UVC_C9_BASIC_LAYOUT_SRC_HEIGHT = src_height;
    UVC_C9_BASIC_LAYOUT_DST_WIDTH = dst_width;
    UVC_C9_BASIC_LAYOUT_DST_HEIGHT = dst_height;


    UVC_C9_BASIC_LAYOUT_PER_SRC_WIDTH = UVC_C9_BASIC_LAYOUT_SRC_WIDTH / 4;
    UVC_C9_BASIC_LAYOUT_PER_DST_WIDTH = UVC_C9_BASIC_LAYOUT_DST_WIDTH / 2;
    UVC_C9_BASIC_LAYOUT_PER_DST_HALF_WIDTH = UVC_C9_BASIC_LAYOUT_DST_WIDTH / 2;
    UVC_C9_BASIC_LAYOUT_PER_CAMERA_DST_HALF_WIDTH = UVC_C9_BASIC_LAYOUT_DST_WIDTH / 4;
    UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH = UVC_C9_BASIC_LAYOUT_SRC_WIDTH / 8;
    UVC_C9_BASIC_LAYOUT_DEST_INTERVAL_WIDTH = UVC_C9_BASIC_LAYOUT_DST_WIDTH / 2;
    UVC_C9_HALF_OF_DEST_WIDTH = UVC_C9_BASIC_LAYOUT_DST_WIDTH / 2;

    UVC_C9_CAMERA_0_LEFT_X = 0;
    UVC_C9_CAMERA_0_RIGHT_X =
            UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH + UVC_C9_BASIC_LAYOUT_PER_SRC_WIDTH * 3;
    UVC_C9_CAMERA_1_X = UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH;
    UVC_C9_CAMERA_2_X =
            UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH + UVC_C9_BASIC_LAYOUT_PER_SRC_WIDTH;
    UVC_C9_CAMERA_3_X =
            UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH + UVC_C9_BASIC_LAYOUT_PER_SRC_WIDTH * 2;
    UVC_360_HEIGHT = GET_LEGAL_LOCATION_PARAMS((UVC_C9_BASIC_LAYOUT_DST_WIDTH + 8) / 6);
    LOGI("UVC_360_HEIGHT init =%d", UVC_360_HEIGHT);
    UVC_360_DY = GET_LEGAL_DST_HEIGHT_PARAMS(UVC_C9_BASIC_LAYOUT_DST_HEIGHT - UVC_360_HEIGHT);
    SPACE_WIDTH_HEIGHT = 16;
    UVC_HALF_OF_DST_HEIGHT = UVC_C9_BASIC_LAYOUT_DST_HEIGHT / 2;
    UVC_C9_BASIC_LAYOUT_DEST_HEIGHT_MARGIN = (UVC_C9_BASIC_LAYOUT_DST_HEIGHT - UVC_360_HEIGHT) / 2;
    UVC_SCALE = UVC_C9_BASIC_LAYOUT_PER_SRC_WIDTH * 1.0 / UVC_C9_HALF_OF_DEST_WIDTH;
    RETURN(result, jint);
}

/**
 * 上部分展示4个摄像头中的1个的画面，下部分展示360°
 * @param env
 * @param thiz
 * @param id_camera
 * @param cameraId
 * @return
 */
static jint nativeSetClipLocationCameraIndex_360(JNIEnv *env, jobject thiz,
                                                 ID_TYPE id_camera, jint stableX) {
    jint result = JNI_ERR;
    ENTER();
    uvc_c9_control_info uvc_c9_control;
    stableX = GET_LEGAL_LOCATION_PARAMS(stableX);
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        if (c9Mode != C9_PER_CAMERA) {
            RETURN(result, jint);
        }
        camera->getClipLocation1(uvc_c9_control);
        int window_id = 0;
        int marginTop = 0;
        //拼接完剩余高度
        int heightLeaved =
                UVC_C9_BASIC_LAYOUT_DST_HEIGHT - UVC_360_HEIGHT - UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
        if (heightLeaved < 0) {
            marginTop = 0;
        } else {
            marginTop = heightLeaved / 2;
        }

        uvc_c9_control.window.src_x = 0;
        uvc_c9_control.window.src_y = 0;
        uvc_c9_control.window.src_w = UVC_C9_BASIC_LAYOUT_SRC_WIDTH;
        uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
        uvc_c9_control.window.dst_x = 0;
        uvc_c9_control.window.dst_y = UVC_360_DY + marginTop + UVC_C9_DIVIDER_HEIGHT;
        uvc_c9_control.window.dst_w = UVC_C9_BASIC_LAYOUT_DST_WIDTH;
        uvc_c9_control.window.dst_h = UVC_360_HEIGHT;
        uvc_c9_control.window.window_id = window_id;
        uvc_c9_control.header = C9_WINDOW;
        ret = camera->setClipLocation(uvc_c9_control);
        window_id++;

        //自定义角度
        if (stableX + UVC_C9_HALF_OF_DEST_WIDTH <= UVC_C9_BASIC_LAYOUT_SRC_WIDTH
            && stableX - UVC_C9_HALF_OF_DEST_WIDTH >= 0) {

            uvc_c9_control.window.src_x = GET_LEGAL_LOCATION(stableX - UVC_C9_HALF_OF_DEST_WIDTH);
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = UVC_C9_BASIC_LAYOUT_DST_WIDTH;
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = 0;
            uvc_c9_control.window.dst_y = marginTop;
            uvc_c9_control.window.dst_w = UVC_C9_BASIC_LAYOUT_DST_WIDTH;
            uvc_c9_control.window.dst_h = UVC_360_DY;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

        } else if (stableX + UVC_C9_HALF_OF_DEST_WIDTH > UVC_C9_BASIC_LAYOUT_SRC_WIDTH) {

            uvc_c9_control.window.src_x = GET_LEGAL_LOCATION(stableX - UVC_C9_HALF_OF_DEST_WIDTH);
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = GET_LEGAL_LOCATION(UVC_C9_HALF_OF_DEST_WIDTH +
                                                             UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                             stableX);
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = 0;
            uvc_c9_control.window.dst_y = marginTop;
            uvc_c9_control.window.dst_w = GET_LEGAL_LOCATION(UVC_C9_HALF_OF_DEST_WIDTH +
                                                             UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                             stableX);
            uvc_c9_control.window.dst_h = UVC_360_DY;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

            uvc_c9_control.window.src_x = 0;
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = GET_LEGAL_LOCATION_(UVC_C9_HALF_OF_DEST_WIDTH -
                                                              (UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                               stableX));
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = GET_LEGAL_LOCATION_(
                                                  (UVC_C9_HALF_OF_DEST_WIDTH +
                                                   UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                   stableX));
            uvc_c9_control.window.dst_y = marginTop;
            uvc_c9_control.window.dst_w = GET_LEGAL_LOCATION_(UVC_C9_HALF_OF_DEST_WIDTH -
                                                              (UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                               stableX));
            uvc_c9_control.window.dst_h = UVC_360_DY;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

        } else if (stableX - UVC_C9_HALF_OF_DEST_WIDTH < 0) {
            uvc_c9_control.window.src_x = 0;
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = GET_LEGAL_LOCATION(UVC_C9_HALF_OF_DEST_WIDTH + stableX);
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = GET_LEGAL_LOCATION(UVC_C9_HALF_OF_DEST_WIDTH - stableX);
            uvc_c9_control.window.dst_y = marginTop;
            uvc_c9_control.window.dst_w = GET_LEGAL_LOCATION(UVC_C9_HALF_OF_DEST_WIDTH + stableX);
            uvc_c9_control.window.dst_h = UVC_360_DY;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

            uvc_c9_control.window.src_x = GET_LEGAL_LOCATION_(UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                              (UVC_C9_HALF_OF_DEST_WIDTH -
                                                               stableX));
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = GET_LEGAL_LOCATION_(
                                                  (UVC_C9_HALF_OF_DEST_WIDTH - stableX));
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = 0;
            uvc_c9_control.window.dst_y = marginTop;
            uvc_c9_control.window.dst_w = GET_LEGAL_LOCATION_(UVC_C9_HALF_OF_DEST_WIDTH - stableX);
            uvc_c9_control.window.dst_h = UVC_360_DY;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;
        }

        uvc_c9_control.basic.layout_dst_width = UVC_C9_BASIC_LAYOUT_DST_WIDTH;
        uvc_c9_control.basic.layout_dst_height = UVC_C9_BASIC_LAYOUT_DST_HEIGHT;
        uvc_c9_control.basic.layout_src_width = UVC_C9_BASIC_LAYOUT_SRC_WIDTH;
        uvc_c9_control.basic.layout_src_height = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
        uvc_c9_control.basic.window_num = window_id;

        uvc_c9_control.header = C9_BASIC;
        ret = camera->setClipLocation(uvc_c9_control);
        uvc_c9_control.trigger.enable = 1;
        uvc_c9_control.header = C9_TRIGGER;
        ret = camera->setClipLocation(uvc_c9_control);
    }
    RETURN(result, jint);
}

/**
 * 话随声动，上部分展示自定义角度，右下角展示说话人，底部展示360°
 * @param env
 * @param thiz
 * @param id_camera
 * @param speakerX
 * @param stableX
 * @return
 */
static jint nativeSetClipLocationStable_360(JNIEnv *env, jobject thiz,
                                            ID_TYPE id_camera,
                                            jint stableX) {
    jint result = JNI_ERR;
    ENTER();
    uvc_c9_control_info uvc_c9_control;
    stableX = GET_LEGAL_LOCATION_PARAMS(stableX);
    int marginTop = 0;
    //拼接完剩余高度
    int heightLeaved =
            UVC_C9_BASIC_LAYOUT_DST_HEIGHT - UVC_360_HEIGHT - UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
    if (heightLeaved < 0) {
        marginTop = 0;
    } else {
        marginTop = heightLeaved / 2;
    }
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        if (c9Mode != C9_STABLE_360) {
            RETURN(result, jint);
        }
        camera->getClipLocation1(uvc_c9_control);
        int window_id = 0;


        uvc_c9_control.window.src_x = 0;
        uvc_c9_control.window.src_y = 0;
        uvc_c9_control.window.src_w = UVC_C9_BASIC_LAYOUT_SRC_WIDTH;
        uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
        uvc_c9_control.window.dst_x = 0;
        uvc_c9_control.window.dst_y = UVC_360_DY + marginTop + UVC_C9_DIVIDER_HEIGHT;
        uvc_c9_control.window.dst_w = UVC_C9_BASIC_LAYOUT_DST_WIDTH;
        uvc_c9_control.window.dst_h = UVC_360_HEIGHT;
        uvc_c9_control.window.window_id = window_id;
        uvc_c9_control.header = C9_WINDOW;
        ret = camera->setClipLocation(uvc_c9_control);
        window_id++;

        //自定义角度
        if (stableX + UVC_C9_HALF_OF_DEST_WIDTH <= UVC_C9_BASIC_LAYOUT_SRC_WIDTH
            && stableX - UVC_C9_HALF_OF_DEST_WIDTH >= 0) {
            uvc_c9_control.window.src_x = GET_LEGAL_LOCATION(stableX - UVC_C9_HALF_OF_DEST_WIDTH);
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = UVC_C9_BASIC_LAYOUT_DST_WIDTH;
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = 0;
            uvc_c9_control.window.dst_y = marginTop;
            uvc_c9_control.window.dst_w = UVC_C9_BASIC_LAYOUT_DST_WIDTH;
            uvc_c9_control.window.dst_h = UVC_360_DY;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

        } else if (stableX + UVC_C9_HALF_OF_DEST_WIDTH > UVC_C9_BASIC_LAYOUT_SRC_WIDTH) {

            uvc_c9_control.window.src_x = GET_LEGAL_LOCATION(stableX - UVC_C9_HALF_OF_DEST_WIDTH);
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = GET_LEGAL_LOCATION(UVC_C9_HALF_OF_DEST_WIDTH +
                                                             UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                             stableX);
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = 0;
            uvc_c9_control.window.dst_y = marginTop;
            uvc_c9_control.window.dst_w = GET_LEGAL_LOCATION(UVC_C9_HALF_OF_DEST_WIDTH +
                                                             UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                             stableX);
            uvc_c9_control.window.dst_h = UVC_360_DY;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

            uvc_c9_control.window.src_x = 0;
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = GET_LEGAL_LOCATION_(UVC_C9_HALF_OF_DEST_WIDTH -
                                                              (UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                               stableX));
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = GET_LEGAL_LOCATION_(
                                                  (UVC_C9_HALF_OF_DEST_WIDTH +
                                                   UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                   stableX));
            uvc_c9_control.window.dst_y = marginTop;
            uvc_c9_control.window.dst_w = GET_LEGAL_LOCATION_(UVC_C9_HALF_OF_DEST_WIDTH -
                                                              (UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                               stableX));
            uvc_c9_control.window.dst_h = UVC_360_DY;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

        } else if (stableX - UVC_C9_HALF_OF_DEST_WIDTH < 0) {
            uvc_c9_control.window.src_x = 0;
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = GET_LEGAL_LOCATION(UVC_C9_HALF_OF_DEST_WIDTH + stableX);
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = GET_LEGAL_LOCATION(UVC_C9_HALF_OF_DEST_WIDTH - stableX);
            uvc_c9_control.window.dst_y = 0;
            uvc_c9_control.window.dst_w = GET_LEGAL_LOCATION(UVC_C9_HALF_OF_DEST_WIDTH + stableX);
            uvc_c9_control.window.dst_h = UVC_360_DY;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;
            uvc_c9_control.window.src_x = GET_LEGAL_LOCATION_(UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                              (UVC_C9_HALF_OF_DEST_WIDTH -
                                                               stableX));
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = GET_LEGAL_LOCATION_(
                                                  (UVC_C9_HALF_OF_DEST_WIDTH - stableX));
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = 0;
            uvc_c9_control.window.dst_y = 0;
            uvc_c9_control.window.dst_w = GET_LEGAL_LOCATION_(UVC_C9_HALF_OF_DEST_WIDTH - stableX);
            uvc_c9_control.window.dst_h = UVC_360_DY;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;
        }

        uvc_c9_control.basic.layout_dst_width = UVC_C9_BASIC_LAYOUT_DST_WIDTH;
        uvc_c9_control.basic.layout_dst_height = UVC_C9_BASIC_LAYOUT_DST_HEIGHT;
        uvc_c9_control.basic.layout_src_width = UVC_C9_BASIC_LAYOUT_SRC_WIDTH;
        uvc_c9_control.basic.layout_src_height = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
        uvc_c9_control.basic.window_num = window_id;

        uvc_c9_control.header = C9_BASIC;
        ret = camera->setClipLocation(uvc_c9_control);
        uvc_c9_control.trigger.enable = 1;
        uvc_c9_control.header = C9_TRIGGER;
        ret = camera->setClipLocation(uvc_c9_control);

    }
    RETURN(result, jint);
}

static jint nativeSetClipLocationStable(JNIEnv *env, jobject thiz,
                                        ID_TYPE id_camera,
                                        jint stableX) {
    jint result = JNI_ERR;
    ENTER();
    uvc_c9_control_info uvc_c9_control;
    stableX = GET_LEGAL_LOCATION_PARAMS(stableX);
    int marginTop = 0;
    //拼接完剩余高度
    int heightLeaved = UVC_C9_BASIC_LAYOUT_DST_HEIGHT - UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
    if (heightLeaved < 0) {
        marginTop = 0;
    } else {
        marginTop = heightLeaved / 2;
    }
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        if (c9Mode != C9_STABLE) {
            RETURN(result, jint);
        }
        camera->getClipLocation1(uvc_c9_control);
        int window_id = 0;

        //自定义角度
        if (stableX + UVC_C9_HALF_OF_DEST_WIDTH <= UVC_C9_BASIC_LAYOUT_SRC_WIDTH
            && stableX - UVC_C9_HALF_OF_DEST_WIDTH >= 0) {
            uvc_c9_control.window.src_x = GET_LEGAL_LOCATION(stableX - UVC_C9_HALF_OF_DEST_WIDTH);
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = UVC_C9_BASIC_LAYOUT_DST_WIDTH;
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = 0;
            uvc_c9_control.window.dst_y = marginTop;
            uvc_c9_control.window.dst_w = UVC_C9_BASIC_LAYOUT_DST_WIDTH;
            uvc_c9_control.window.dst_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

        } else if (stableX + UVC_C9_HALF_OF_DEST_WIDTH > UVC_C9_BASIC_LAYOUT_SRC_WIDTH) {

            uvc_c9_control.window.src_x = GET_LEGAL_LOCATION(stableX - UVC_C9_HALF_OF_DEST_WIDTH);
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = GET_LEGAL_LOCATION(UVC_C9_HALF_OF_DEST_WIDTH +
                                                             UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                             stableX);
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = 0;
            uvc_c9_control.window.dst_y = marginTop;
            uvc_c9_control.window.dst_w = GET_LEGAL_LOCATION(UVC_C9_HALF_OF_DEST_WIDTH +
                                                             UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                             stableX);
            uvc_c9_control.window.dst_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

            uvc_c9_control.window.src_x = 0;
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = GET_LEGAL_LOCATION_(UVC_C9_HALF_OF_DEST_WIDTH -
                                                              (UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                               stableX));
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = GET_LEGAL_LOCATION_(
                                                  (UVC_C9_HALF_OF_DEST_WIDTH +
                                                   UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                   stableX));
            uvc_c9_control.window.dst_y = marginTop;
            uvc_c9_control.window.dst_w = GET_LEGAL_LOCATION_(UVC_C9_HALF_OF_DEST_WIDTH -
                                                              (UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                               stableX));
            uvc_c9_control.window.dst_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

        } else if (stableX - UVC_C9_HALF_OF_DEST_WIDTH < 0) {


            uvc_c9_control.window.src_x = 0;
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = GET_LEGAL_LOCATION(UVC_C9_HALF_OF_DEST_WIDTH + stableX);
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = GET_LEGAL_LOCATION(UVC_C9_HALF_OF_DEST_WIDTH - stableX);
            uvc_c9_control.window.dst_y = marginTop;
            uvc_c9_control.window.dst_w = GET_LEGAL_LOCATION(UVC_C9_HALF_OF_DEST_WIDTH + stableX);
            uvc_c9_control.window.dst_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;


            uvc_c9_control.window.src_x = GET_LEGAL_LOCATION_(UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                              (UVC_C9_HALF_OF_DEST_WIDTH -
                                                               stableX));
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = GET_LEGAL_LOCATION_(
                                                  (UVC_C9_HALF_OF_DEST_WIDTH - stableX));
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = 0;
            uvc_c9_control.window.dst_y = marginTop;
            uvc_c9_control.window.dst_w = GET_LEGAL_LOCATION_(UVC_C9_HALF_OF_DEST_WIDTH - stableX);
            uvc_c9_control.window.dst_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;
        }

        uvc_c9_control.basic.layout_dst_width = UVC_C9_BASIC_LAYOUT_DST_WIDTH;
        uvc_c9_control.basic.layout_dst_height = UVC_C9_BASIC_LAYOUT_DST_HEIGHT;
        uvc_c9_control.basic.layout_src_width = UVC_C9_BASIC_LAYOUT_SRC_WIDTH;
        uvc_c9_control.basic.layout_src_height = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
        uvc_c9_control.basic.window_num = window_id;

        uvc_c9_control.header = C9_BASIC;
        ret = camera->setClipLocation(uvc_c9_control);

        uvc_c9_control.trigger.enable = 1;
        uvc_c9_control.header = C9_TRIGGER;
        ret = camera->setClipLocation(uvc_c9_control);

    }
    RETURN(result, jint);
}

/**
 * 话随声动，上部分展示自定义角度，右下角展示说话人，底部展示360°
 * @param env
 * @param thiz
 * @param id_camera
 * @param speakerX
 * @param stableX
 * @return
 */
static jint nativeSetClipLocationScroll_360(JNIEnv *env, jobject thiz,
                                            ID_TYPE id_camera,
                                            jint stableX) {
    jint result = JNI_ERR;
    ENTER();
    stableX = GET_LEGAL_LOCATION_PARAMS(stableX);
    uvc_c9_control_info uvc_c9_control;
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        if (c9Mode != C9_SCROLL_360) {
            RETURN(result, jint);
        }
        camera->getClipLocation1(uvc_c9_control);
        int window_id = 0;
        uvc_c9_control.window.src_x = 0;
        uvc_c9_control.window.src_y = 0;
        uvc_c9_control.window.src_w = UVC_C9_BASIC_LAYOUT_SRC_WIDTH;
        uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
        uvc_c9_control.window.dst_x = 0;
        uvc_c9_control.window.dst_y = UVC_360_DY + UVC_C9_DIVIDER_HEIGHT;
        uvc_c9_control.window.dst_w = UVC_C9_BASIC_LAYOUT_DST_WIDTH;
        uvc_c9_control.window.dst_h = UVC_360_HEIGHT;
        uvc_c9_control.window.window_id = window_id;
        uvc_c9_control.header = C9_WINDOW;
        ret = camera->setClipLocation(uvc_c9_control);
        window_id++;

        uvc_c9_control.window.src_x = GET_LEGAL_LOCATION(stableX);
        uvc_c9_control.window.src_y = 0;
        uvc_c9_control.window.src_w = UVC_C9_BASIC_LAYOUT_DST_WIDTH;
        uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
        uvc_c9_control.window.dst_x = 0;
        uvc_c9_control.window.dst_y = 0;
        uvc_c9_control.window.dst_w = UVC_C9_BASIC_LAYOUT_DST_WIDTH;
        uvc_c9_control.window.dst_h = UVC_360_DY;
        uvc_c9_control.window.window_id = window_id;
        uvc_c9_control.header = C9_WINDOW;
        ret = camera->setClipLocation(uvc_c9_control);
        window_id++;

        uvc_c9_control.basic.layout_dst_width = UVC_C9_BASIC_LAYOUT_DST_WIDTH;
        uvc_c9_control.basic.layout_dst_height = UVC_C9_BASIC_LAYOUT_DST_HEIGHT;
        uvc_c9_control.basic.layout_src_width = UVC_C9_BASIC_LAYOUT_SRC_WIDTH;
        uvc_c9_control.basic.layout_src_height = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
        uvc_c9_control.basic.window_num = window_id;

        uvc_c9_control.header = C9_BASIC;
        ret = camera->setClipLocation(uvc_c9_control);

        uvc_c9_control.trigger.enable = 1;
        uvc_c9_control.header = C9_TRIGGER;
        ret = camera->setClipLocation(uvc_c9_control);

    }
    RETURN(result, jint);
}

/**
 * 话随声动，上部分展示自定义角度，右下角展示说话人，底部展示360°
 * @param env
 * @param thiz
 * @param id_camera
 * @param speakerX
 * @param stableX
 * @return
 */
static jint nativeSetClipLocation_PIP(JNIEnv *env, jobject thiz,
                                      ID_TYPE id_camera, jint speakerX,
                                      jint stableX, jint width) {
    jint result = JNI_ERR;
    ENTER();
    uvc_c9_control_info uvc_c9_control;
    stableX = GET_LEGAL_LOCATION_PARAMS(stableX);
    speakerX = GET_LEGAL_LOCATION_PARAMS(speakerX);
    int srcWidth = UVC_C9_BASIC_LAYOUT_SRC_WIDTH;
    int srcHeight = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
    int dstWidth = UVC_C9_BASIC_LAYOUT_DST_WIDTH;
    int dstHeight = UVC_C9_BASIC_LAYOUT_DST_HEIGHT;
    width = GET_LEGAL_LOCATION_PARAMS((dstWidth + 8) / 6);
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        if (c9Mode != C9_SPEAKER_CUSTOM_360_PIP) {
            RETURN(result, jint);
        }
        int marginTop = 0;
        //拼接完剩余高度
        int heightLeaved =
                UVC_C9_BASIC_LAYOUT_DST_HEIGHT - UVC_360_HEIGHT - UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
        if (heightLeaved < 0) {
            marginTop = 0;
        } else {
            marginTop = heightLeaved / 2;
        }
        camera->getClipLocation1(uvc_c9_control);
        int window_id = 0;

        uvc_c9_control.window.src_x = 0;
        uvc_c9_control.window.src_y = 0;
        uvc_c9_control.window.src_w = srcWidth;
        uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
        uvc_c9_control.window.dst_x = 0;
        uvc_c9_control.window.dst_y = UVC_360_DY + marginTop + UVC_C9_DIVIDER_HEIGHT;
        uvc_c9_control.window.dst_w = UVC_C9_BASIC_LAYOUT_DST_WIDTH;
        uvc_c9_control.window.dst_h = UVC_360_HEIGHT;
        uvc_c9_control.window.window_id = window_id;
        uvc_c9_control.header = C9_WINDOW;
        ret = camera->setClipLocation(uvc_c9_control);
        window_id++;
        //自定义角度
        if (stableX + UVC_C9_HALF_OF_DEST_WIDTH <= UVC_C9_BASIC_LAYOUT_SRC_WIDTH
            && stableX - UVC_C9_HALF_OF_DEST_WIDTH >= 0) {

            uvc_c9_control.window.src_x = GET_LEGAL_LOCATION(stableX - UVC_C9_HALF_OF_DEST_WIDTH);
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = UVC_C9_BASIC_LAYOUT_DST_WIDTH;
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = 0;
            uvc_c9_control.window.dst_y = marginTop;
            uvc_c9_control.window.dst_w = UVC_C9_BASIC_LAYOUT_DST_WIDTH;
            uvc_c9_control.window.dst_h = UVC_360_DY;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

        } else if (stableX + UVC_C9_HALF_OF_DEST_WIDTH > UVC_C9_BASIC_LAYOUT_SRC_WIDTH) {

            uvc_c9_control.window.src_x = GET_LEGAL_LOCATION(stableX - UVC_C9_HALF_OF_DEST_WIDTH);
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = GET_LEGAL_LOCATION(UVC_C9_HALF_OF_DEST_WIDTH +
                                                             UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                             stableX);
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = 0;
            uvc_c9_control.window.dst_y = marginTop;
            uvc_c9_control.window.dst_w = GET_LEGAL_LOCATION(UVC_C9_HALF_OF_DEST_WIDTH +
                                                             UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                             stableX);
            uvc_c9_control.window.dst_h = UVC_360_DY;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

            uvc_c9_control.window.src_x = 0;
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = GET_LEGAL_LOCATION_(UVC_C9_HALF_OF_DEST_WIDTH -
                                                              (UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                               stableX));
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = GET_LEGAL_LOCATION_(
                                                  (UVC_C9_HALF_OF_DEST_WIDTH +
                                                   UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                   stableX));
            uvc_c9_control.window.dst_y = marginTop;
            uvc_c9_control.window.dst_w = GET_LEGAL_LOCATION_(UVC_C9_HALF_OF_DEST_WIDTH -
                                                              (UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                               stableX));
            uvc_c9_control.window.dst_h = UVC_360_DY;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

        } else if (stableX - UVC_C9_HALF_OF_DEST_WIDTH < 0) {
            uvc_c9_control.window.src_x = 0;
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = GET_LEGAL_LOCATION(UVC_C9_HALF_OF_DEST_WIDTH + stableX);
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = GET_LEGAL_LOCATION(UVC_C9_HALF_OF_DEST_WIDTH -
                                                             stableX);
            uvc_c9_control.window.dst_y = marginTop;
            uvc_c9_control.window.dst_w = GET_LEGAL_LOCATION(UVC_C9_HALF_OF_DEST_WIDTH + stableX);
            uvc_c9_control.window.dst_h = UVC_360_DY;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

            uvc_c9_control.window.src_x = GET_LEGAL_LOCATION_(UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                              (UVC_C9_HALF_OF_DEST_WIDTH -
                                                               stableX));
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = GET_LEGAL_LOCATION_(
                                                  (UVC_C9_HALF_OF_DEST_WIDTH - stableX));
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = 0;
            uvc_c9_control.window.dst_y = marginTop;
            uvc_c9_control.window.dst_w = GET_LEGAL_LOCATION_(UVC_C9_HALF_OF_DEST_WIDTH - stableX);
            uvc_c9_control.window.dst_h = UVC_360_DY;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;
        }

        int halfWidth = GET_LEGAL_LOCATION_PARAMS(width / 2);
        width = halfWidth * 2;
        int dx = UVC_C9_BASIC_LAYOUT_DST_WIDTH - width;
        if (dx % 16 != 0) {
            LOGI("dx 异常");
        }

        //说话人
        if (speakerX + halfWidth <= UVC_C9_BASIC_LAYOUT_SRC_WIDTH && speakerX - halfWidth >= 0) {
            int srcX = GET_LEGAL_LOCATION(speakerX - halfWidth);
            if (srcX % 16 != 0) {
                LOGI("srcX 异常");
            }
            uvc_c9_control.window.src_x = srcX;
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = width;
            uvc_c9_control.window.src_h = width;
            uvc_c9_control.window.dst_x = dx;
            uvc_c9_control.window.dst_y =
                    UVC_C9_BASIC_LAYOUT_DST_HEIGHT - UVC_360_HEIGHT - width - marginTop;
            uvc_c9_control.window.dst_w = width;
            uvc_c9_control.window.dst_h = width;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;
        } else if (speakerX + halfWidth > UVC_C9_BASIC_LAYOUT_SRC_WIDTH) {

            int srcX = GET_LEGAL_LOCATION(speakerX - halfWidth);
            int srcW = GET_LEGAL_LOCATION(halfWidth + srcWidth - speakerX);
            int dstW = GET_LEGAL_LOCATION((halfWidth + srcWidth - speakerX));
            if (srcX % 16 != 0) {
                LOGI("srcX 异常");
            }
            if (srcW % 16 != 0) {
                LOGI("srcW 异常");
            }
            if (dstW % 16 != 0) {
                LOGI("dstW 异常");
            }


            uvc_c9_control.window.src_x = srcX;
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = srcW;
            uvc_c9_control.window.src_h = width;
            uvc_c9_control.window.dst_x = dx;
            uvc_c9_control.window.dst_y =
                    UVC_C9_BASIC_LAYOUT_DST_HEIGHT - UVC_360_HEIGHT - width - marginTop;
            uvc_c9_control.window.dst_w = dstW;
            uvc_c9_control.window.dst_h = width;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

            srcW = GET_LEGAL_LOCATION_(halfWidth - (srcWidth - speakerX));
            int dstX = GET_LEGAL_LOCATION_(
                    (halfWidth + srcWidth - speakerX) + dx);
            dstW = GET_LEGAL_LOCATION_(halfWidth - (srcWidth - speakerX));

            if (dstX % 16 != 0) {
                LOGI("dstX 异常");
            }
            if (dstW % 16 != 0) {
                LOGI("dstW 异常");
            }

            uvc_c9_control.window.src_x = 0;
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = srcW;
            uvc_c9_control.window.src_h = width;
            uvc_c9_control.window.dst_x = dstX;
            uvc_c9_control.window.dst_y =
                    UVC_C9_BASIC_LAYOUT_DST_HEIGHT - UVC_360_HEIGHT - width - marginTop;
            uvc_c9_control.window.dst_w = dstW;
            uvc_c9_control.window.dst_h = width;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

        } else if (speakerX - halfWidth < 0) {

            int srcW = GET_LEGAL_LOCATION(halfWidth + speakerX);
            int dstX = GET_LEGAL_LOCATION((halfWidth - speakerX) + dx);
            int dstW = GET_LEGAL_LOCATION(halfWidth + speakerX);
            if (srcW % 16 != 0) {
                LOGI("srcW 异常");
            }
            if (dstX % 16 != 0) {
                LOGI("dstX 异常");
            }
            if (dstW % 16 != 0) {
                LOGI("dstW 异常");
            }

            uvc_c9_control.window.src_x = 0;
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = srcW;
            uvc_c9_control.window.src_h = width;
            uvc_c9_control.window.dst_x = dstX;
            uvc_c9_control.window.dst_y =
                    UVC_C9_BASIC_LAYOUT_DST_HEIGHT - UVC_360_HEIGHT - width - marginTop;
            uvc_c9_control.window.dst_w = dstW;
            uvc_c9_control.window.dst_h = width;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

            int srcX = GET_LEGAL_LOCATION_(srcWidth - (halfWidth - speakerX));
            srcW = GET_LEGAL_LOCATION_((halfWidth - speakerX));
            dstW = GET_LEGAL_LOCATION_(width - (halfWidth + speakerX));


            if (srcX % 16 != 0) {
                LOGI("srcX 异常");
            }
            if (srcW % 16 != 0) {
                LOGI("srcW 异常");
            }
            if (dstW % 16 != 0) {
                LOGI("dstW 异常");
            }

            uvc_c9_control.window.src_x = srcX;
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = srcW;
            uvc_c9_control.window.src_h = width;
            uvc_c9_control.window.dst_x = dx;
            uvc_c9_control.window.dst_y =
                    UVC_C9_BASIC_LAYOUT_DST_HEIGHT - UVC_360_HEIGHT - width - marginTop;
            uvc_c9_control.window.dst_w = dstW;
            uvc_c9_control.window.dst_h = width;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

        }

        uvc_c9_control.basic.layout_dst_width = UVC_C9_BASIC_LAYOUT_DST_WIDTH;
        uvc_c9_control.basic.layout_dst_height = UVC_C9_BASIC_LAYOUT_DST_HEIGHT;
        uvc_c9_control.basic.layout_src_width = UVC_C9_BASIC_LAYOUT_SRC_WIDTH;
        uvc_c9_control.basic.layout_src_height = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
        uvc_c9_control.basic.window_num = window_id;

        uvc_c9_control.header = C9_BASIC;
        ret = camera->setClipLocation(uvc_c9_control);

        uvc_c9_control.trigger.enable = 1;
        uvc_c9_control.header = C9_TRIGGER;
        ret = camera->setClipLocation(uvc_c9_control);

    }
    RETURN(result, jint);
}

static jint nativeSetClipLocationGridByCenterX(JNIEnv *env, jobject thiz,
                                               ID_TYPE id_camera, jint stableX,
                                               jint index1, jint index2, jint index3) {
    jint result = JNI_ERR;
    ENTER();
    uvc_c9_control_info uvc_c9_control;
    stableX = GET_LEGAL_LOCATION_PARAMS(stableX);
    index1 = GET_LEGAL_LOCATION_PARAMS(index1);
    index2 = GET_LEGAL_LOCATION_PARAMS(index2);
    index3 = GET_LEGAL_LOCATION_PARAMS(index3);
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        if (c9Mode != C9_GRID) {
            RETURN(result, jint);
        }
        camera->getClipLocation1(uvc_c9_control);
        int window_id = 0;

        //自定义角度
        if (stableX + UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH <= UVC_C9_BASIC_LAYOUT_SRC_WIDTH
            && stableX - UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH >= 0) {

            uvc_c9_control.window.src_x = GET_LEGAL_LOCATION(
                                                  stableX - UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH);
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = UVC_C9_BASIC_LAYOUT_PER_SRC_WIDTH;
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = 0;
            uvc_c9_control.window.dst_y = 0;
            uvc_c9_control.window.dst_w = UVC_C9_HALF_OF_DEST_WIDTH;
            uvc_c9_control.window.dst_h = UVC_HALF_OF_DST_HEIGHT;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

        } else if (stableX + UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH >
                   UVC_C9_BASIC_LAYOUT_SRC_WIDTH) {

            uvc_c9_control.window.src_x = GET_LEGAL_LOCATION(
                                                  stableX - UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH);
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = GET_LEGAL_LOCATION(
                                                  UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH +
                                                  UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                  stableX);
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = 0;
            uvc_c9_control.window.dst_y = 0;
            uvc_c9_control.window.dst_w = GET_LEGAL_LOCATION(UVC_C9_HALF_OF_DEST_WIDTH +
                                                             UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                             stableX);
            uvc_c9_control.window.dst_h = UVC_HALF_OF_DST_HEIGHT;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

            uvc_c9_control.window.src_x = 0;
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = GET_LEGAL_LOCATION_(
                                                  UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH -
                                                  (UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                   stableX));
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = GET_LEGAL_LOCATION_((UVC_C9_HALF_OF_DEST_WIDTH +
                                                               UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                               stableX));
            uvc_c9_control.window.dst_y = 0;
            uvc_c9_control.window.dst_w = GET_LEGAL_LOCATION_(UVC_C9_HALF_OF_DEST_WIDTH -
                                                              (UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                               stableX));
            uvc_c9_control.window.dst_h = UVC_HALF_OF_DST_HEIGHT;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

        } else if (stableX - UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH < 0) {
            uvc_c9_control.window.src_x = 0;
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = GET_LEGAL_LOCATION(
                                                  UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH + stableX);
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = GET_LEGAL_LOCATION((int) (
                    (UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH -
                     stableX) / UVC_SCALE));
            uvc_c9_control.window.dst_y = 0;
            uvc_c9_control.window.dst_w = GET_LEGAL_LOCATION(
                                                  (int) ((UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH +
                                                          stableX) / UVC_SCALE));
            uvc_c9_control.window.dst_h = UVC_HALF_OF_DST_HEIGHT;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

            uvc_c9_control.window.src_x = GET_LEGAL_LOCATION_(
                                                  (UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                   (UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH +
                                                    stableX)));
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = GET_LEGAL_LOCATION_(
                                                  (UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH -
                                                   stableX));
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = 0;
            uvc_c9_control.window.dst_y = 0;
            uvc_c9_control.window.dst_w = GET_LEGAL_LOCATION_(
                                                  (int) ((UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH -
                                                          stableX) / UVC_SCALE));
            uvc_c9_control.window.dst_h = UVC_HALF_OF_DST_HEIGHT;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;
        }

        if (index1 + UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH <= UVC_C9_BASIC_LAYOUT_SRC_WIDTH
            && index1 - UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH >= 0) {

            uvc_c9_control.window.src_x = GET_LEGAL_LOCATION(
                                                  index1 - UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH);
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = UVC_C9_BASIC_LAYOUT_PER_SRC_WIDTH;
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = UVC_C9_HALF_OF_DEST_WIDTH;
            uvc_c9_control.window.dst_y = 0;
            uvc_c9_control.window.dst_w = UVC_C9_HALF_OF_DEST_WIDTH;
            uvc_c9_control.window.dst_h = UVC_HALF_OF_DST_HEIGHT;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

        } else if (index1 + UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH >
                   UVC_C9_BASIC_LAYOUT_SRC_WIDTH) {

            uvc_c9_control.window.src_x = GET_LEGAL_LOCATION(
                                                  index1 - UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH);
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = GET_LEGAL_LOCATION(
                                                  UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH +
                                                  UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                  index1);
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = UVC_C9_HALF_OF_DEST_WIDTH;
            uvc_c9_control.window.dst_y = 0;
            uvc_c9_control.window.dst_w = GET_LEGAL_LOCATION(UVC_C9_HALF_OF_DEST_WIDTH +
                                                             UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                             index1);
            uvc_c9_control.window.dst_h = UVC_HALF_OF_DST_HEIGHT;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

            uvc_c9_control.window.src_x = 0;
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = GET_LEGAL_LOCATION_(
                                                  UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH -
                                                  (UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                   index1));
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = GET_LEGAL_LOCATION_(
                                                  UVC_C9_HALF_OF_DEST_WIDTH +
                                                  (UVC_C9_HALF_OF_DEST_WIDTH +
                                                   UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                   index1));
            uvc_c9_control.window.dst_y = 0;
            uvc_c9_control.window.dst_w = GET_LEGAL_LOCATION_(UVC_C9_HALF_OF_DEST_WIDTH -
                                                              (UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                               index1));
            uvc_c9_control.window.dst_h = UVC_360_DY;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

        } else if (index1 - UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH < 0) {
            uvc_c9_control.window.src_x = 0;
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = GET_LEGAL_LOCATION(
                                                  UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH + index1);
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = GET_LEGAL_LOCATION((int) (UVC_C9_HALF_OF_DEST_WIDTH +
                                                                    (UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH -
                                                                     index1) / UVC_SCALE));
            uvc_c9_control.window.dst_y = 0;
            uvc_c9_control.window.dst_w = GET_LEGAL_LOCATION((int) ((
                                                                            UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH +
                                                                            index1) / UVC_SCALE));
            uvc_c9_control.window.dst_h = UVC_HALF_OF_DST_HEIGHT;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

            uvc_c9_control.window.src_x = GET_LEGAL_LOCATION_(
                                                  (UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                   (UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH +
                                                    index1)));
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = GET_LEGAL_LOCATION_(
                                                  (UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH -
                                                   index1));
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = GET_LEGAL_LOCATION(UVC_C9_HALF_OF_DEST_WIDTH);
            uvc_c9_control.window.dst_y = 0;
            uvc_c9_control.window.dst_w = GET_LEGAL_LOCATION_((int) ((
                                                                             UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH -
                                                                             index1) / UVC_SCALE));
            uvc_c9_control.window.dst_h = UVC_HALF_OF_DST_HEIGHT;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;
        }


        if (index2 + UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH <= UVC_C9_BASIC_LAYOUT_SRC_WIDTH
            && index2 - UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH >= 0) {

            uvc_c9_control.window.src_x = GET_LEGAL_LOCATION(
                                                  index2 - UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH);
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = UVC_C9_BASIC_LAYOUT_PER_SRC_WIDTH;
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = 0;
            uvc_c9_control.window.dst_y = UVC_HALF_OF_DST_HEIGHT;
            uvc_c9_control.window.dst_w = UVC_C9_HALF_OF_DEST_WIDTH;
            uvc_c9_control.window.dst_h = UVC_HALF_OF_DST_HEIGHT;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

        } else if (index2 + UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH >
                   UVC_C9_BASIC_LAYOUT_SRC_WIDTH) {

            uvc_c9_control.window.src_x = GET_LEGAL_LOCATION(
                                                  index2 - UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH);
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = GET_LEGAL_LOCATION(
                                                  UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH +
                                                  UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                  index2);
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = 0;
            uvc_c9_control.window.dst_y = UVC_HALF_OF_DST_HEIGHT;
            uvc_c9_control.window.dst_w = GET_LEGAL_LOCATION(UVC_C9_HALF_OF_DEST_WIDTH +
                                                             UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                             index2);
            uvc_c9_control.window.dst_h = UVC_HALF_OF_DST_HEIGHT;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

            uvc_c9_control.window.src_x = 0;
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = GET_LEGAL_LOCATION_(
                                                  UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH -
                                                  (UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                   index2));
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = GET_LEGAL_LOCATION_((UVC_C9_HALF_OF_DEST_WIDTH +
                                                               UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                               index2));
            uvc_c9_control.window.dst_y = UVC_HALF_OF_DST_HEIGHT;
            uvc_c9_control.window.dst_w = GET_LEGAL_LOCATION_(UVC_C9_HALF_OF_DEST_WIDTH -
                                                              (UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                               index2));
            uvc_c9_control.window.dst_h = UVC_360_DY;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

        } else if (index2 - UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH < 0) {
            uvc_c9_control.window.src_x = 0;
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = GET_LEGAL_LOCATION(
                                                  UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH + index2);
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = GET_LEGAL_LOCATION((int) ((
                    (UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH -
                     index2) / UVC_SCALE)));
            uvc_c9_control.window.dst_y = UVC_HALF_OF_DST_HEIGHT;
            uvc_c9_control.window.dst_w = GET_LEGAL_LOCATION(
                                                  UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH + index2);
            uvc_c9_control.window.dst_h = UVC_HALF_OF_DST_HEIGHT;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

            uvc_c9_control.window.src_x = GET_LEGAL_LOCATION_(
                                                  (UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                   (UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH +
                                                    index2)));
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = GET_LEGAL_LOCATION_(
                                                  (UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH -
                                                   index2));
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = 0;
            uvc_c9_control.window.dst_y = UVC_HALF_OF_DST_HEIGHT;
            uvc_c9_control.window.dst_w = GET_LEGAL_LOCATION_((int) ((
                                                                             UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH -
                                                                             index2) / UVC_SCALE));
            uvc_c9_control.window.dst_h = UVC_HALF_OF_DST_HEIGHT;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;
        }
        if (index3 + UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH <= UVC_C9_BASIC_LAYOUT_SRC_WIDTH
            && index3 - UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH >= 0) {

            uvc_c9_control.window.src_x = GET_LEGAL_LOCATION(
                                                  index3 - UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH);
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = UVC_C9_BASIC_LAYOUT_PER_SRC_WIDTH;
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = UVC_C9_HALF_OF_DEST_WIDTH;
            uvc_c9_control.window.dst_y = UVC_HALF_OF_DST_HEIGHT;
            uvc_c9_control.window.dst_w = UVC_C9_HALF_OF_DEST_WIDTH;
            uvc_c9_control.window.dst_h = UVC_HALF_OF_DST_HEIGHT;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

        } else if (index3 + UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH >
                   UVC_C9_BASIC_LAYOUT_SRC_WIDTH) {

            uvc_c9_control.window.src_x = GET_LEGAL_LOCATION(
                                                  index3 - UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH);
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = GET_LEGAL_LOCATION(
                                                  UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH +
                                                  UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                  index3);
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = UVC_C9_HALF_OF_DEST_WIDTH;
            uvc_c9_control.window.dst_y = UVC_HALF_OF_DST_HEIGHT;
            uvc_c9_control.window.dst_w = GET_LEGAL_LOCATION(UVC_C9_HALF_OF_DEST_WIDTH +
                                                             UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                             index3);
            uvc_c9_control.window.dst_h = UVC_HALF_OF_DST_HEIGHT;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

            uvc_c9_control.window.src_x = 0;
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = GET_LEGAL_LOCATION_(
                                                  UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH -
                                                  (UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                   index3));
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = GET_LEGAL_LOCATION_(
                                                  UVC_C9_HALF_OF_DEST_WIDTH +
                                                  (UVC_C9_HALF_OF_DEST_WIDTH +
                                                   UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                   index3));
            uvc_c9_control.window.dst_y = UVC_HALF_OF_DST_HEIGHT;
            uvc_c9_control.window.dst_w = GET_LEGAL_LOCATION_(UVC_C9_HALF_OF_DEST_WIDTH -
                                                              (UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                               index3));
            uvc_c9_control.window.dst_h = UVC_360_DY;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

        } else if (index3 - UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH < 0) {
            uvc_c9_control.window.src_x = 0;
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = GET_LEGAL_LOCATION(
                                                  UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH + index3);
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = GET_LEGAL_LOCATION((int) (UVC_C9_HALF_OF_DEST_WIDTH +
                                                                    (UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH -
                                                                     index3) / UVC_SCALE));
            uvc_c9_control.window.dst_y = UVC_HALF_OF_DST_HEIGHT;
            uvc_c9_control.window.dst_w = GET_LEGAL_LOCATION((int) ((
                                                                            UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH +
                                                                            index3) / UVC_SCALE));
            uvc_c9_control.window.dst_h = UVC_HALF_OF_DST_HEIGHT;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

            uvc_c9_control.window.src_x = GET_LEGAL_LOCATION_(
                                                  (UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                                   (UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH +
                                                    index3)));
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = GET_LEGAL_LOCATION_(
                                                  (UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH -
                                                   index3));
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = UVC_C9_HALF_OF_DEST_WIDTH;
            uvc_c9_control.window.dst_y = UVC_HALF_OF_DST_HEIGHT;
            uvc_c9_control.window.dst_w = GET_LEGAL_LOCATION_((int) ((
                                                                             UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH -
                                                                             index3) / UVC_SCALE));
            uvc_c9_control.window.dst_h = UVC_HALF_OF_DST_HEIGHT;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;
        }


        uvc_c9_control.basic.layout_dst_width = UVC_C9_BASIC_LAYOUT_DST_WIDTH;
        uvc_c9_control.basic.layout_dst_height = UVC_C9_BASIC_LAYOUT_DST_HEIGHT;
        uvc_c9_control.basic.layout_src_width = UVC_C9_BASIC_LAYOUT_SRC_WIDTH;
        uvc_c9_control.basic.layout_src_height = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
        uvc_c9_control.basic.window_num = window_id;

        uvc_c9_control.header = C9_BASIC;
        ret = camera->setClipLocation(uvc_c9_control);

        uvc_c9_control.trigger.enable = 1;
        uvc_c9_control.header = C9_TRIGGER;
        ret = camera->setClipLocation(uvc_c9_control);
    }
    RETURN(result, jint);
}

/**
 * 话随声动 上部分显示说话人，下部分展示360°
 * @param env
 * @param thiz
 * @param id_camera
 * @param x
 * @return
 */
static jint nativeSetClipLocationSpeaker360(JNIEnv *env, jobject thiz,
                                            ID_TYPE id_camera, jint x) {
    jint result = JNI_ERR;
    ENTER();
    uvc_c9_control_info uvc_c9_control;

    x = GET_LEGAL_LOCATION_PARAMS(x);
    if (x % 16 != 0) {
        LOGI("setClipLocation speakerlocation error %d=", x);
    }
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        if (c9Mode != C9_MIC) {
            RETURN(result, jint);
        }
        int marginTop = 0;
        //拼接完剩余高度
        int heightLeaved =
                UVC_C9_BASIC_LAYOUT_DST_HEIGHT - UVC_360_HEIGHT - UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
        if (heightLeaved < 0) {
            marginTop = 0;
        } else {
            marginTop = heightLeaved / 2;
        }
//        uint8_t uint8[2];
//        camera->getResolution(uint8);
        camera->getClipLocation1(uvc_c9_control);
        int window_id = 0;


        uvc_c9_control.window.src_x = 0;
        uvc_c9_control.window.src_y = 0;
        uvc_c9_control.window.src_w = UVC_C9_BASIC_LAYOUT_SRC_WIDTH;
        uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
        uvc_c9_control.window.dst_x = 0;
        uvc_c9_control.window.dst_y = UVC_360_DY + marginTop + UVC_C9_DIVIDER_HEIGHT;
        uvc_c9_control.window.dst_w = UVC_C9_BASIC_LAYOUT_DST_WIDTH;
        uvc_c9_control.window.dst_h = UVC_360_HEIGHT;
        uvc_c9_control.window.window_id = window_id;
        uvc_c9_control.header = C9_WINDOW;
        ret = camera->setClipLocation(uvc_c9_control);
        window_id++;
//

        LOGI("setClipLocation speakerlocation %d=", x);
        if (x + UVC_C9_HALF_OF_DEST_WIDTH <= UVC_C9_BASIC_LAYOUT_SRC_WIDTH &&
            x - UVC_C9_HALF_OF_DEST_WIDTH >= 0) {
            int srcX = GET_LEGAL_LOCATION(x - UVC_C9_HALF_OF_DEST_WIDTH);
            if (srcX % 16 != 0) {
                LOGI("srcX 异常");
            }
            uvc_c9_control.window.src_x = srcX;
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = UVC_C9_BASIC_LAYOUT_DST_WIDTH;
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = 0;
            uvc_c9_control.window.dst_y = marginTop;
            uvc_c9_control.window.dst_w = UVC_C9_BASIC_LAYOUT_DST_WIDTH;
            uvc_c9_control.window.dst_h = UVC_360_DY;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;


        } else if (x + UVC_C9_HALF_OF_DEST_WIDTH > UVC_C9_BASIC_LAYOUT_SRC_WIDTH) {
            int srcXX = GET_LEGAL_LOCATION(x - UVC_C9_HALF_OF_DEST_WIDTH);
            int srcWW = GET_LEGAL_LOCATION(UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                           (x - UVC_C9_HALF_OF_DEST_WIDTH));
            int dstWw = GET_LEGAL_LOCATION(UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                           (x - UVC_C9_HALF_OF_DEST_WIDTH));
            if (srcXX % 16 != 0) {
                LOGI("srcXX 异常");
            }
            if (srcWW % 16 != 0) {
                LOGI("srcWW 异常");
            }
            if (dstWw % 16 != 0) {
                LOGI("dstWw 异常");
            }
            uvc_c9_control.window.src_x = srcXX;
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = srcWW;
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = 0;
            uvc_c9_control.window.dst_y = marginTop;
            uvc_c9_control.window.dst_w = dstWw;
            uvc_c9_control.window.dst_h = UVC_360_DY;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

            int srcW = GET_LEGAL_LOCATION_(UVC_C9_HALF_OF_DEST_WIDTH -
                                           (UVC_C9_BASIC_LAYOUT_SRC_WIDTH - x));
            int dstX = GET_LEGAL_LOCATION_(UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                           (x - UVC_C9_HALF_OF_DEST_WIDTH));
            int dstW = GET_LEGAL_LOCATION_(UVC_C9_HALF_OF_DEST_WIDTH -
                                           (UVC_C9_BASIC_LAYOUT_SRC_WIDTH - x));

            if (srcW % 16 != 0) {
                LOGI("srcW 异常");
            }
            if (dstX % 16 != 0) {
                LOGI("dstX 异常");
            }
            if (dstW % 16 != 0) {
                LOGI("dstW 异常");
            }
            uvc_c9_control.window.src_x = 0;
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = srcW;
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = dstX;
            uvc_c9_control.window.dst_y = marginTop;
            uvc_c9_control.window.dst_w = dstW;
            uvc_c9_control.window.dst_h = UVC_360_DY;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

        } else if (x - UVC_C9_HALF_OF_DEST_WIDTH < 0) {
            int dstX = GET_LEGAL_LOCATION(UVC_C9_HALF_OF_DEST_WIDTH - x);
            int srcW = GET_LEGAL_LOCATION(UVC_C9_HALF_OF_DEST_WIDTH + x);
            int dstW = GET_LEGAL_LOCATION(UVC_C9_HALF_OF_DEST_WIDTH + x);
            if (dstX % 16 != 0) {
                LOGI("dstX  异常");
            }
            if (srcW % 16 != 0) {
                LOGI("srcW 异常");
            }
            if (dstW % 16 != 0) {
                LOGI("dstW 异常");
            }
            uvc_c9_control.window.src_x = 0;
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = srcW;
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = dstX;
            uvc_c9_control.window.dst_y = marginTop;
            uvc_c9_control.window.dst_w = dstW;
            uvc_c9_control.window.dst_h = UVC_360_DY;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;


            int srcX = GET_LEGAL_LOCATION_(UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                           (UVC_C9_HALF_OF_DEST_WIDTH - x));
            srcW = GET_LEGAL_LOCATION_(UVC_C9_HALF_OF_DEST_WIDTH - x);
            dstW = GET_LEGAL_LOCATION_(UVC_C9_HALF_OF_DEST_WIDTH - x);
            if (srcX % 16 != 0) {
                LOGI("srcX 1 异常");
            }
            if (srcW % 16 != 0) {
                LOGI("srcW 1 异常");
            }
            if (dstW % 16 != 0) {
                LOGI("dstW 1 异常");
            }
            uvc_c9_control.window.src_x = srcX;
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = srcW;
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = 0;
            uvc_c9_control.window.dst_y = marginTop;
            uvc_c9_control.window.dst_w = dstW;
            uvc_c9_control.window.dst_h = UVC_360_DY;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;
        }
        uvc_c9_control.basic.layout_dst_width = UVC_C9_BASIC_LAYOUT_DST_WIDTH;
        uvc_c9_control.basic.layout_dst_height = UVC_C9_BASIC_LAYOUT_DST_HEIGHT;
        uvc_c9_control.basic.layout_src_width = UVC_C9_BASIC_LAYOUT_SRC_WIDTH;
        uvc_c9_control.basic.layout_src_height = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
        uvc_c9_control.basic.window_num = window_id;

        uvc_c9_control.header = C9_BASIC;
        ret = camera->setClipLocation(uvc_c9_control);


        uvc_c9_control.trigger.enable = 1;
        uvc_c9_control.header = C9_TRIGGER;
        ret = camera->setClipLocation(uvc_c9_control);

    }
    RETURN(result, jint);
}
static jint nativeSetClipLocationSpeaker_360(JNIEnv *env, jobject thiz,
                                            ID_TYPE id_camera, jint x) {
    jint result = JNI_ERR;
    ENTER();
    uvc_c9_control_info uvc_c9_control;

    x = GET_LEGAL_LOCATION_PARAMS(x);
    if (x % 16 != 0) {
        LOGI("setClipLocation speakerlocation error %d=", x);
    }
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        if (c9Mode != C9_MIC) {
            RETURN(result, jint);
        }
        int marginTop = 0;
        //拼接完剩余高度
        int heightLeaved =
                UVC_C9_BASIC_LAYOUT_DST_HEIGHT - UVC_360_HEIGHT - UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
        if (heightLeaved < 0) {
            marginTop = 0;
        } else {
            marginTop = heightLeaved / 2;
        }
//        uint8_t uint8[2];
//        camera->getResolution(uint8);
        camera->getClipLocation1(uvc_c9_control);
        int window_id = 0;


        uvc_c9_control.window.src_x = 0;
        uvc_c9_control.window.src_y = 0;
        uvc_c9_control.window.src_w = UVC_C9_BASIC_LAYOUT_SRC_WIDTH;
        uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
        uvc_c9_control.window.dst_x = 0;
        uvc_c9_control.window.dst_y = UVC_360_DY + marginTop + UVC_C9_DIVIDER_HEIGHT;
        uvc_c9_control.window.dst_w = UVC_C9_BASIC_LAYOUT_DST_WIDTH;
        uvc_c9_control.window.dst_h = UVC_360_HEIGHT;
        uvc_c9_control.window.window_id = window_id;
        uvc_c9_control.header = C9_WINDOW;
        ret = camera->setClipLocation(uvc_c9_control);
        window_id++;
//

        if (x + UVC_C9_HALF_OF_DEST_WIDTH <= UVC_C9_BASIC_LAYOUT_SRC_WIDTH &&
            x - UVC_C9_HALF_OF_DEST_WIDTH >= 0) {
            int srcX = GET_LEGAL_LOCATION(x - UVC_C9_HALF_OF_DEST_WIDTH);
            if (srcX % 16 != 0) {
                LOGI("srcX 异常");
            }
            uvc_c9_control.window.src_x = srcX;
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = UVC_C9_BASIC_LAYOUT_DST_WIDTH;
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = 0;
            uvc_c9_control.window.dst_y = marginTop;
            uvc_c9_control.window.dst_w = UVC_C9_BASIC_LAYOUT_DST_WIDTH;
            uvc_c9_control.window.dst_h = UVC_360_DY;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;


        } else if (x + UVC_C9_HALF_OF_DEST_WIDTH > UVC_C9_BASIC_LAYOUT_SRC_WIDTH) {
            int srcXX = GET_LEGAL_LOCATION(x - UVC_C9_HALF_OF_DEST_WIDTH);
            int srcWW = GET_LEGAL_LOCATION(UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                           (x - UVC_C9_HALF_OF_DEST_WIDTH));
            int dstWw = GET_LEGAL_LOCATION(UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                           (x - UVC_C9_HALF_OF_DEST_WIDTH));
            if (srcXX % 16 != 0) {
                LOGI("srcXX 异常");
            }
            if (srcWW % 16 != 0) {
                LOGI("srcWW 异常");
            }
            if (dstWw % 16 != 0) {
                LOGI("dstWw 异常");
            }
            uvc_c9_control.window.src_x = srcXX;
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = srcWW;
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = 0;
            uvc_c9_control.window.dst_y = marginTop;
            uvc_c9_control.window.dst_w = dstWw;
            uvc_c9_control.window.dst_h = UVC_360_DY;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

            int srcW = GET_LEGAL_LOCATION_(UVC_C9_HALF_OF_DEST_WIDTH -
                                           (UVC_C9_BASIC_LAYOUT_SRC_WIDTH - x));
            int dstX = GET_LEGAL_LOCATION_(UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                           (x - UVC_C9_HALF_OF_DEST_WIDTH));
            int dstW = GET_LEGAL_LOCATION_(UVC_C9_HALF_OF_DEST_WIDTH -
                                           (UVC_C9_BASIC_LAYOUT_SRC_WIDTH - x));

            if (srcW % 16 != 0) {
                LOGI("srcW 异常");
            }
            if (dstX % 16 != 0) {
                LOGI("dstX 异常");
            }
            if (dstW % 16 != 0) {
                LOGI("dstW 异常");
            }
            uvc_c9_control.window.src_x = 0;
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = srcW;
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = dstX;
            uvc_c9_control.window.dst_y = marginTop;
            uvc_c9_control.window.dst_w = dstW;
            uvc_c9_control.window.dst_h = UVC_360_DY;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;

        } else if (x - UVC_C9_HALF_OF_DEST_WIDTH < 0) {
            int dstX = GET_LEGAL_LOCATION(UVC_C9_HALF_OF_DEST_WIDTH - x);
            int srcW = GET_LEGAL_LOCATION(UVC_C9_HALF_OF_DEST_WIDTH + x);
            int dstW = GET_LEGAL_LOCATION(UVC_C9_HALF_OF_DEST_WIDTH + x);
            if (dstX % 16 != 0) {
                LOGI("dstX  异常");
            }
            if (srcW % 16 != 0) {
                LOGI("srcW 异常");
            }
            if (dstW % 16 != 0) {
                LOGI("dstW 异常");
            }
            uvc_c9_control.window.src_x = 0;
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = srcW;
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = dstX;
            uvc_c9_control.window.dst_y = marginTop;
            uvc_c9_control.window.dst_w = dstW;
            uvc_c9_control.window.dst_h = UVC_360_DY;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;


            int srcX = GET_LEGAL_LOCATION_(UVC_C9_BASIC_LAYOUT_SRC_WIDTH -
                                           (UVC_C9_HALF_OF_DEST_WIDTH - x));
            srcW = GET_LEGAL_LOCATION_(UVC_C9_HALF_OF_DEST_WIDTH - x);
            dstW = GET_LEGAL_LOCATION_(UVC_C9_HALF_OF_DEST_WIDTH - x);
            if (srcX % 16 != 0) {
                LOGI("srcX 1 异常");
            }
            if (srcW % 16 != 0) {
                LOGI("srcW 1 异常");
            }
            if (dstW % 16 != 0) {
                LOGI("dstW 1 异常");
            }
            uvc_c9_control.window.src_x = srcX;
            uvc_c9_control.window.src_y = 0;
            uvc_c9_control.window.src_w = srcW;
            uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
            uvc_c9_control.window.dst_x = 0;
            uvc_c9_control.window.dst_y = marginTop;
            uvc_c9_control.window.dst_w = dstW;
            uvc_c9_control.window.dst_h = UVC_360_DY;
            uvc_c9_control.window.window_id = window_id;
            uvc_c9_control.header = C9_WINDOW;
            ret = camera->setClipLocation(uvc_c9_control);
            window_id++;
        }
        uvc_c9_control.basic.layout_dst_width = UVC_C9_BASIC_LAYOUT_DST_WIDTH;
        uvc_c9_control.basic.layout_dst_height = UVC_C9_BASIC_LAYOUT_DST_HEIGHT;
        uvc_c9_control.basic.layout_src_width = UVC_C9_BASIC_LAYOUT_SRC_WIDTH;
        uvc_c9_control.basic.layout_src_height = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
        uvc_c9_control.basic.window_num = window_id;

        uvc_c9_control.header = C9_BASIC;
        ret = camera->setClipLocation(uvc_c9_control);


        uvc_c9_control.trigger.enable = 1;
        uvc_c9_control.header = C9_TRIGGER;
        ret = camera->setClipLocation(uvc_c9_control);

    }
    RETURN(result, jint);
}

/**
 * 宫格
 * @param env
 * @param thiz
 * @param id_camera
 * @return
 */
static jint nativeSetClipLocationGrid(JNIEnv *env, jobject thiz,
                                      ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    uvc_c9_control_info uvc_c9_control;

    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        if (c9Mode != C9_GRID) {
            RETURN(result, jint);
        }
        camera->getClipLocation1(uvc_c9_control);
        int window_id = 0;


        uvc_c9_control.window.src_x = UVC_C9_CAMERA_1_X;
        uvc_c9_control.window.src_y = 0;
        uvc_c9_control.window.src_w = UVC_C9_BASIC_LAYOUT_PER_SRC_WIDTH;
        uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
        uvc_c9_control.window.dst_x = 0;
        uvc_c9_control.window.dst_y = 0;
        uvc_c9_control.window.dst_w = UVC_C9_BASIC_LAYOUT_PER_DST_HALF_WIDTH;
        uvc_c9_control.window.dst_h = UVC_HALF_OF_DST_HEIGHT;
        uvc_c9_control.window.window_id = window_id;
        uvc_c9_control.header = C9_WINDOW;
        ret = camera->setClipLocation(uvc_c9_control);
        window_id++;
        LOGI("nativeSetClipLocationGrid ret=%d", ret);

        uvc_c9_control.window.src_x = UVC_C9_CAMERA_2_X;
        uvc_c9_control.window.src_y = 0;
        uvc_c9_control.window.src_w = UVC_C9_BASIC_LAYOUT_PER_SRC_WIDTH;
        uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
        uvc_c9_control.window.dst_x = 0;
        uvc_c9_control.window.dst_y = UVC_HALF_OF_DST_HEIGHT;
        uvc_c9_control.window.dst_w = UVC_C9_BASIC_LAYOUT_PER_DST_HALF_WIDTH;
        uvc_c9_control.window.dst_h = UVC_HALF_OF_DST_HEIGHT;
        uvc_c9_control.window.window_id = window_id;
        uvc_c9_control.header = C9_WINDOW;
        ret = camera->setClipLocation(uvc_c9_control);
        window_id++;
        LOGI("nativeSetClipLocationGrid ret=%d", ret);

        uvc_c9_control.window.src_x = UVC_C9_CAMERA_3_X;
        uvc_c9_control.window.src_y = 0;
        uvc_c9_control.window.src_w = UVC_C9_BASIC_LAYOUT_PER_SRC_WIDTH;
        uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
        uvc_c9_control.window.dst_x = UVC_C9_HALF_OF_DEST_WIDTH;
        uvc_c9_control.window.dst_y = 0;
        uvc_c9_control.window.dst_w = UVC_C9_BASIC_LAYOUT_DEST_INTERVAL_WIDTH;
        uvc_c9_control.window.dst_h = UVC_HALF_OF_DST_HEIGHT;
        uvc_c9_control.window.window_id = window_id;
        uvc_c9_control.header = C9_WINDOW;
        ret = camera->setClipLocation(uvc_c9_control);
        window_id++;
        LOGI("nativeSetClipLocationGrid ret=%d", ret);

        uvc_c9_control.window.src_x = UVC_C9_CAMERA_0_RIGHT_X;
        uvc_c9_control.window.src_y = 0;
        uvc_c9_control.window.src_w = UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH;
        uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
        uvc_c9_control.window.dst_x = UVC_C9_HALF_OF_DEST_WIDTH;
        uvc_c9_control.window.dst_y = UVC_HALF_OF_DST_HEIGHT;
        uvc_c9_control.window.dst_w = UVC_C9_BASIC_LAYOUT_PER_CAMERA_DST_HALF_WIDTH;
        uvc_c9_control.window.dst_h = UVC_HALF_OF_DST_HEIGHT;
        uvc_c9_control.window.window_id = window_id;
        uvc_c9_control.header = C9_WINDOW;
        ret = camera->setClipLocation(uvc_c9_control);
        window_id++;
        LOGI("nativeSetClipLocationGrid ret=%d", ret);

        uvc_c9_control.window.src_x = UVC_C9_CAMERA_0_LEFT_X;
        uvc_c9_control.window.src_y = 0;
        uvc_c9_control.window.src_w = UVC_C9_BASIC_LAYOUT_PER_SRC_HALF_WIDTH;
        uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
        uvc_c9_control.window.dst_x =
                UVC_C9_BASIC_LAYOUT_PER_CAMERA_DST_HALF_WIDTH + UVC_C9_HALF_OF_DEST_WIDTH;
        uvc_c9_control.window.dst_y = UVC_HALF_OF_DST_HEIGHT;
        uvc_c9_control.window.dst_w = UVC_C9_BASIC_LAYOUT_PER_CAMERA_DST_HALF_WIDTH;
        uvc_c9_control.window.dst_h = UVC_HALF_OF_DST_HEIGHT;
        uvc_c9_control.window.window_id = window_id;
        uvc_c9_control.header = C9_WINDOW;
        ret = camera->setClipLocation(uvc_c9_control);
        window_id++;
        LOGI("nativeSetClipLocationGrid ret=%d", ret);

        uvc_c9_control.basic.layout_dst_width = UVC_C9_BASIC_LAYOUT_DST_WIDTH;
        uvc_c9_control.basic.layout_dst_height = UVC_C9_BASIC_LAYOUT_DST_HEIGHT;
        uvc_c9_control.basic.layout_src_width = UVC_C9_BASIC_LAYOUT_SRC_WIDTH;
        uvc_c9_control.basic.layout_src_height = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
        uvc_c9_control.basic.window_num = window_id;
        uvc_c9_control.header = C9_BASIC;
        ret = camera->setClipLocation(uvc_c9_control);
        LOGI("nativeSetClipLocationGrid ret=%d", ret);

        uvc_c9_control.trigger.enable = 1;
        uvc_c9_control.header = C9_TRIGGER;
        ret = camera->setClipLocation(uvc_c9_control);
        LOGI("nativeSetClipLocationGrid ret=%d", ret);
    }
    RETURN(result, jint);
}


/**
 * 360° 画面
 * @param env
 * @param thiz
 * @param id_camera
 * @return
 */
static jint nativeSetClipLocation360(JNIEnv *env, jobject thiz,
                                     ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    uvc_c9_control_info uvc_c9_control;

    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        if (c9Mode != C9_360) {
            RETURN(result, jint);
        }

        camera->getClipLocation1(uvc_c9_control);
        int window_id = 0;
        uvc_c9_control.window.src_x = 0;
        uvc_c9_control.window.src_y = 0;
        uvc_c9_control.window.src_w = UVC_C9_BASIC_LAYOUT_SRC_WIDTH;
        uvc_c9_control.window.src_h = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
        uvc_c9_control.window.dst_x = 0;
        uvc_c9_control.window.dst_y = UVC_C9_BASIC_LAYOUT_DEST_HEIGHT_MARGIN;
        uvc_c9_control.window.dst_w = UVC_C9_BASIC_LAYOUT_DST_WIDTH;
        uvc_c9_control.window.dst_h = UVC_360_HEIGHT;
        uvc_c9_control.window.window_id = window_id;
        uvc_c9_control.header = C9_WINDOW;
        ret = camera->setClipLocation(uvc_c9_control);
        window_id++;
        LOGI("nativeSetClipLocation360 window ret=%d", ret);

        uvc_c9_control.basic.layout_dst_width = UVC_C9_BASIC_LAYOUT_DST_WIDTH;
        uvc_c9_control.basic.layout_dst_height = UVC_C9_BASIC_LAYOUT_DST_HEIGHT;
        uvc_c9_control.basic.layout_src_width = UVC_C9_BASIC_LAYOUT_SRC_WIDTH;
        uvc_c9_control.basic.layout_src_height = UVC_C9_BASIC_LAYOUT_SRC_HEIGHT;
        uvc_c9_control.basic.window_num = window_id;
        uvc_c9_control.header = C9_BASIC;
        ret = camera->setClipLocation(uvc_c9_control);
        LOGI("nativeSetClipLocation360 basic ret=%d", ret);

        uvc_c9_control.trigger.enable = 1;
        uvc_c9_control.header = C9_TRIGGER;
        ret = camera->setClipLocation(uvc_c9_control);
        LOGI("nativeSetClipLocation360 update ret=%d", ret);
    }

    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdateFocusLimit(JNIEnv *env, jobject thiz,
                                   ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateFocusLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mFocusMin", min);
            setField_int(env, thiz, "mFocusMax", max);
            setField_int(env, thiz, "mFocusDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetFocus(JNIEnv *env, jobject thiz,
                           ID_TYPE id_camera, jint focus) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setFocus(focus);
    }
    RETURN(result, jint);
}

static jint nativeGetFocus(JNIEnv *env, jobject thiz,
                           ID_TYPE id_camera) {

    jint result = 0;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getFocus();
    }
    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdateFocusRelLimit(JNIEnv *env, jobject thiz,
                                      ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateFocusRelLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mFocusRelMin", min);
            setField_int(env, thiz, "mFocusRelMax", max);
            setField_int(env, thiz, "mFocusRelDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetFocusRel(JNIEnv *env, jobject thiz,
                              ID_TYPE id_camera, jint focus_rel) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setFocusRel(focus_rel);
    }
    RETURN(result, jint);
}

static jint nativeGetFocusRel(JNIEnv *env, jobject thiz,
                              ID_TYPE id_camera) {

    jint result = 0;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getFocusRel();
    }
    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdateIrisLimit(JNIEnv *env, jobject thiz,
                                  ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateIrisLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mIrisMin", min);
            setField_int(env, thiz, "mIrisMax", max);
            setField_int(env, thiz, "mIrisDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetIris(JNIEnv *env, jobject thiz,
                          ID_TYPE id_camera, jint iris) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setIris(iris);
    }
    RETURN(result, jint);
}

static jint nativeGetIris(JNIEnv *env, jobject thiz,
                          ID_TYPE id_camera) {

    jint result = 0;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getIris();
    }
    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdateIrisRelLimit(JNIEnv *env, jobject thiz,
                                     ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateIrisRelLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mIrisRelMin", min);
            setField_int(env, thiz, "mIrisRelMax", max);
            setField_int(env, thiz, "mIrisRelDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetIrisRel(JNIEnv *env, jobject thiz,
                             ID_TYPE id_camera, jint iris_rel) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setIrisRel(iris_rel);
    }
    RETURN(result, jint);
}

static jint nativeGetIrisRel(JNIEnv *env, jobject thiz,
                             ID_TYPE id_camera) {

    jint result = 0;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getIrisRel();
    }
    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdatePanLimit(JNIEnv *env, jobject thiz,
                                 ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updatePanLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mPanMin", min);
            setField_int(env, thiz, "mPanMax", max);
            setField_int(env, thiz, "mPanDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetPan(JNIEnv *env, jobject thiz,
                         ID_TYPE id_camera, jint pan) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setPan(pan);
    }
    RETURN(result, jint);
}

static jint nativeGetPan(JNIEnv *env, jobject thiz,
                         ID_TYPE id_camera) {

    jint result = 0;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getPan();
    }
    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdateTiltLimit(JNIEnv *env, jobject thiz,
                                  ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateTiltLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mTiltMin", min);
            setField_int(env, thiz, "mTiltMax", max);
            setField_int(env, thiz, "mTiltDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetTilt(JNIEnv *env, jobject thiz,
                          ID_TYPE id_camera, jint tilt) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setTilt(tilt);
    }
    RETURN(result, jint);
}

static jint nativeGetTilt(JNIEnv *env, jobject thiz,
                          ID_TYPE id_camera) {

    jint result = 0;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getTilt();
    }
    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdateRollLimit(JNIEnv *env, jobject thiz,
                                  ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateRollLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mRollMin", min);
            setField_int(env, thiz, "mRollMax", max);
            setField_int(env, thiz, "mRollDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetRoll(JNIEnv *env, jobject thiz,
                          ID_TYPE id_camera, jint roll) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setRoll(roll);
    }
    RETURN(result, jint);
}

static jint nativeGetRoll(JNIEnv *env, jobject thiz,
                          ID_TYPE id_camera) {

    jint result = 0;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getRoll();
    }
    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdatePanRelLimit(JNIEnv *env, jobject thiz,
                                    ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updatePanRelLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mPanRelMin", min);
            setField_int(env, thiz, "mPanRelMax", max);
            setField_int(env, thiz, "mPanRelDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetPanRel(JNIEnv *env, jobject thiz,
                            ID_TYPE id_camera, jint pan_rel) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setPanRel(pan_rel);
    }
    RETURN(result, jint);
}

static jint nativeGetPanRel(JNIEnv *env, jobject thiz,
                            ID_TYPE id_camera) {

    jint result = 0;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getPanRel();
    }
    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdateTiltRelLimit(JNIEnv *env, jobject thiz,
                                     ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateTiltRelLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mTiltRelMin", min);
            setField_int(env, thiz, "mTiltRelMax", max);
            setField_int(env, thiz, "mTiltRelDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetTiltRel(JNIEnv *env, jobject thiz,
                             ID_TYPE id_camera, jint tilt_rel) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setTiltRel(tilt_rel);
    }
    RETURN(result, jint);
}

static jint nativeGetTiltRel(JNIEnv *env, jobject thiz,
                             ID_TYPE id_camera) {

    jint result = 0;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getTiltRel();
    }
    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdateRollRelLimit(JNIEnv *env, jobject thiz,
                                     ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateRollRelLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mRollRelMin", min);
            setField_int(env, thiz, "mRollRelMax", max);
            setField_int(env, thiz, "mRollRelDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetRollRel(JNIEnv *env, jobject thiz,
                             ID_TYPE id_camera, jint roll_rel) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setRollRel(roll_rel);
    }
    RETURN(result, jint);
}

static jint nativeGetRollRel(JNIEnv *env, jobject thiz,
                             ID_TYPE id_camera) {

    jint result = 0;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getRollRel();
    }
    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdateContrastLimit(JNIEnv *env, jobject thiz,
                                      ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateContrastLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mContrastMin", min);
            setField_int(env, thiz, "mContrastMax", max);
            setField_int(env, thiz, "mContrastDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetContrast(JNIEnv *env, jobject thiz,
                              ID_TYPE id_camera, jint contrast) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setContrast(contrast);
    }
    RETURN(result, jint);
}

static jint nativeGetContrast(JNIEnv *env, jobject thiz,
                              ID_TYPE id_camera) {

    jint result = 0;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getContrast();
    }
    RETURN(result, jint);
}

//======================================================================
// Java method correspond to this function should not be a static mathod
static jint nativeUpdateAutoContrastLimit(JNIEnv *env, jobject thiz,
                                          ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateAutoContrastLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mAutoContrastMin", min);
            setField_int(env, thiz, "mAutoContrastMax", max);
            setField_int(env, thiz, "mAutoContrastDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetAutoContrast(JNIEnv *env, jobject thiz,
                                  ID_TYPE id_camera, jboolean autocontrast) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setAutoContrast(autocontrast);
    }
    RETURN(result, jint);
}

static jint nativeGetAutoContrast(JNIEnv *env, jobject thiz,
                                  ID_TYPE id_camera) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getAutoContrast();
    }
    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdateSharpnessLimit(JNIEnv *env, jobject thiz,
                                       ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateSharpnessLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mSharpnessMin", min);
            setField_int(env, thiz, "mSharpnessMax", max);
            setField_int(env, thiz, "mSharpnessDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetSharpness(JNIEnv *env, jobject thiz,
                               ID_TYPE id_camera, jint sharpness) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setSharpness(sharpness);
    }
    RETURN(result, jint);
}

static jint nativeGetSharpness(JNIEnv *env, jobject thiz,
                               ID_TYPE id_camera) {

    jint result = 0;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getSharpness();
    }
    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdateGainLimit(JNIEnv *env, jobject thiz,
                                  ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateGainLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mGainMin", min);
            setField_int(env, thiz, "mGainMax", max);
            setField_int(env, thiz, "mGainDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetGain(JNIEnv *env, jobject thiz,
                          ID_TYPE id_camera, jint gain) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setGain(gain);
    }
    RETURN(result, jint);
}

static jint nativeGetGain(JNIEnv *env, jobject thiz,
                          ID_TYPE id_camera) {

    jint result = 0;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getGain();
    }
    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdateGammaLimit(JNIEnv *env, jobject thiz,
                                   ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateGammaLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mGammaMin", min);
            setField_int(env, thiz, "mGammaMax", max);
            setField_int(env, thiz, "mGammaDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetGamma(JNIEnv *env, jobject thiz,
                           ID_TYPE id_camera, jint gamma) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setGamma(gamma);
    }
    RETURN(result, jint);
}

static jint nativeGetGamma(JNIEnv *env, jobject thiz,
                           ID_TYPE id_camera) {

    jint result = 0;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getGamma();
    }
    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdateWhiteBlanceLimit(JNIEnv *env, jobject thiz,
                                         ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateWhiteBlanceLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mWhiteBlanceMin", min);
            setField_int(env, thiz, "mWhiteBlanceMax", max);
            setField_int(env, thiz, "mWhiteBlanceDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetWhiteBlance(JNIEnv *env, jobject thiz,
                                 ID_TYPE id_camera, jint whiteBlance) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setWhiteBlance(whiteBlance);
    }
    RETURN(result, jint);
}

static jint nativeGetWhiteBlance(JNIEnv *env, jobject thiz,
                                 ID_TYPE id_camera) {

    jint result = 0;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getWhiteBlance();
    }
    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdateWhiteBlanceCompoLimit(JNIEnv *env, jobject thiz,
                                              ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateWhiteBlanceCompoLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mWhiteBlanceCompoMin", min);
            setField_int(env, thiz, "mWhiteBlanceCompoMax", max);
            setField_int(env, thiz, "mWhiteBlanceCompoDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetWhiteBlanceCompo(JNIEnv *env, jobject thiz,
                                      ID_TYPE id_camera, jint whiteBlance_compo) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setWhiteBlanceCompo(whiteBlance_compo);
    }
    RETURN(result, jint);
}

static jint nativeGetWhiteBlanceCompo(JNIEnv *env, jobject thiz,
                                      ID_TYPE id_camera) {

    jint result = 0;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getWhiteBlanceCompo();
    }
    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdateBacklightCompLimit(JNIEnv *env, jobject thiz,
                                           ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateBacklightCompLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mBacklightCompMin", min);
            setField_int(env, thiz, "mBacklightCompMax", max);
            setField_int(env, thiz, "mBacklightCompDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetBacklightComp(JNIEnv *env, jobject thiz,
                                   ID_TYPE id_camera, jint backlight_comp) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setBacklightComp(backlight_comp);
    }
    RETURN(result, jint);
}

static jint nativeGetBacklightComp(JNIEnv *env, jobject thiz,
                                   ID_TYPE id_camera) {

    jint result = 0;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getBacklightComp();
    }
    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdateSaturationLimit(JNIEnv *env, jobject thiz,
                                        ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateSaturationLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mSaturationMin", min);
            setField_int(env, thiz, "mSaturationMax", max);
            setField_int(env, thiz, "mSaturationDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetSaturation(JNIEnv *env, jobject thiz,
                                ID_TYPE id_camera, jint saturation) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setSaturation(saturation);
    }
    RETURN(result, jint);
}

static jint nativeGetSaturation(JNIEnv *env, jobject thiz,
                                ID_TYPE id_camera) {

    jint result = 0;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getSaturation();
    }
    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdateHueLimit(JNIEnv *env, jobject thiz,
                                 ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateHueLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mHueMin", min);
            setField_int(env, thiz, "mHueMax", max);
            setField_int(env, thiz, "mHueDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetHue(JNIEnv *env, jobject thiz,
                         ID_TYPE id_camera, jint hue) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setHue(hue);
    }
    RETURN(result, jint);
}

static jint nativeGetHue(JNIEnv *env, jobject thiz,
                         ID_TYPE id_camera) {

    jint result = 0;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getHue();
    }
    RETURN(result, jint);
}

//======================================================================
// Java method correspond to this function should not be a static mathod
static jint nativeUpdateAutoHueLimit(JNIEnv *env, jobject thiz,
                                     ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateAutoHueLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mAutoHueMin", min);
            setField_int(env, thiz, "mAutoHueMax", max);
            setField_int(env, thiz, "mAutoHueDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetAutoHue(JNIEnv *env, jobject thiz,
                             ID_TYPE id_camera, jboolean autohue) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setAutoHue(autohue);
    }
    RETURN(result, jint);
}

static jint nativeGetAutoHue(JNIEnv *env, jobject thiz,
                             ID_TYPE id_camera) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getAutoHue();
    }
    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdatePowerlineFrequencyLimit(JNIEnv *env, jobject thiz,
                                                ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updatePowerlineFrequencyLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mPowerlineFrequencyMin", min);
            setField_int(env, thiz, "mPowerlineFrequencyMax", max);
            setField_int(env, thiz, "mPowerlineFrequencyDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetPowerlineFrequency(JNIEnv *env, jobject thiz,
                                        ID_TYPE id_camera, jint frequency) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setPowerlineFrequency(frequency);
    }
    RETURN(result, jint);
}

static jint nativeGetPowerlineFrequency(JNIEnv *env, jobject thiz,
                                        ID_TYPE id_camera) {

    jint result = 0;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getPowerlineFrequency();
    }
    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdateZoomLimit(JNIEnv *env, jobject thiz,
                                  ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateZoomLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mZoomMin", min);
            setField_int(env, thiz, "mZoomMax", max);
            setField_int(env, thiz, "mZoomDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetZoom(JNIEnv *env, jobject thiz,
                          ID_TYPE id_camera, jint zoom) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setZoom(zoom);
    }
    RETURN(result, jint);
}

static jint nativeGetZoom(JNIEnv *env, jobject thiz,
                          ID_TYPE id_camera) {

    jint result = 0;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getZoom();
    }
    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdateZoomRelLimit(JNIEnv *env, jobject thiz,
                                     ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateZoomRelLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mZoomRelMin", min);
            setField_int(env, thiz, "mZoomRelMax", max);
            setField_int(env, thiz, "mZoomRelDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetZoomRel(JNIEnv *env, jobject thiz,
                             ID_TYPE id_camera, jint zoom_rel) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setZoomRel(zoom_rel);
    }
    RETURN(result, jint);
}

static jint nativeGetZoomRel(JNIEnv *env, jobject thiz,
                             ID_TYPE id_camera) {

    jint result = 0;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getZoomRel();
    }
    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdateDigitalMultiplierLimit(JNIEnv *env, jobject thiz,
                                               ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateDigitalMultiplierLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mDigitalMultiplierMin", min);
            setField_int(env, thiz, "mDigitalMultiplierMax", max);
            setField_int(env, thiz, "mDigitalMultiplierDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetDigitalMultiplier(JNIEnv *env, jobject thiz,
                                       ID_TYPE id_camera, jint multiplier) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setDigitalMultiplier(multiplier);
    }
    RETURN(result, jint);
}

static jint nativeGetDigitalMultiplier(JNIEnv *env, jobject thiz,
                                       ID_TYPE id_camera) {

    jint result = 0;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getDigitalMultiplier();
    }
    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdateDigitalMultiplierLimitLimit(JNIEnv *env, jobject thiz,
                                                    ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateDigitalMultiplierLimitLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mDigitalMultiplierLimitMin", min);
            setField_int(env, thiz, "mDigitalMultiplierLimitMax", max);
            setField_int(env, thiz, "mDigitalMultiplierLimitDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetDigitalMultiplierLimit(JNIEnv *env, jobject thiz,
                                            ID_TYPE id_camera, jint multiplier_limit) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setDigitalMultiplierLimit(multiplier_limit);
    }
    RETURN(result, jint);
}

static jint nativeGetDigitalMultiplierLimit(JNIEnv *env, jobject thiz,
                                            ID_TYPE id_camera) {

    jint result = 0;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getDigitalMultiplierLimit();
    }
    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdateAnalogVideoStandardLimit(JNIEnv *env, jobject thiz,
                                                 ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateAnalogVideoStandardLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mAnalogVideoStandardMin", min);
            setField_int(env, thiz, "mAnalogVideoStandardMax", max);
            setField_int(env, thiz, "mAnalogVideoStandardDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetAnalogVideoStandard(JNIEnv *env, jobject thiz,
                                         ID_TYPE id_camera, jint standard) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setAnalogVideoStandard(standard);
    }
    RETURN(result, jint);
}

static jint nativeGetAnalogVideoStandard(JNIEnv *env, jobject thiz,
                                         ID_TYPE id_camera) {

    jint result = 0;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getAnalogVideoStandard();
    }
    RETURN(result, jint);
}

//======================================================================
// Java mnethod correspond to this function should not be a static mathod
static jint nativeUpdateAnalogVideoLockStateLimit(JNIEnv *env, jobject thiz,
                                                  ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updateAnalogVideoLockStateLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mAnalogVideoLockStateMin", min);
            setField_int(env, thiz, "mAnalogVideoLockStateMax", max);
            setField_int(env, thiz, "mAnalogVideoLockStateDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetAnalogVideoLockState(JNIEnv *env, jobject thiz,
                                          ID_TYPE id_camera, jint state) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setAnalogVideoLockState(state);
    }
    RETURN(result, jint);
}

static jint nativeGetAnalogVideoLockState(JNIEnv *env, jobject thiz,
                                          ID_TYPE id_camera) {

    jint result = 0;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getAnalogVideoLockState();
    }
    RETURN(result, jint);
}

//======================================================================
// Java method correspond to this function should not be a static mathod
static jint nativeUpdatePrivacyLimit(JNIEnv *env, jobject thiz,
                                     ID_TYPE id_camera) {
    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        int min, max, def;
        result = camera->updatePrivacyLimit(min, max, def);
        if (!result) {
            // Java側へ書き込む
            setField_int(env, thiz, "mPrivacyMin", min);
            setField_int(env, thiz, "mPrivacyMax", max);
            setField_int(env, thiz, "mPrivacyDef", def);
        }
    }
    RETURN(result, jint);
}

static jint nativeSetPrivacy(JNIEnv *env, jobject thiz,
                             ID_TYPE id_camera, jboolean privacy) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->setPrivacy(privacy ? 1 : 0);
    }
    RETURN(result, jint);
}

static jint nativeGetPrivacy(JNIEnv *env, jobject thiz,
                             ID_TYPE id_camera) {

    jint result = JNI_ERR;
    ENTER();
    UVCCamera *camera = reinterpret_cast<UVCCamera *>(id_camera);
    if (LIKELY(camera)) {
        result = camera->getPrivacy();
    }
    RETURN(result, jint);
}

//**********************************************************************
//
//**********************************************************************
jint registerNativeMethods(JNIEnv *env, const char *class_name, JNINativeMethod *methods,
                           int num_methods) {
    int result = 0;

    jclass clazz = env->FindClass(class_name);
    if (LIKELY(clazz)) {
        int result = env->RegisterNatives(clazz, methods, num_methods);
        if (UNLIKELY(result < 0)) {
            LOGE("registerNativeMethods failed(class=%s)", class_name);
        }
    } else {
        LOGE("registerNativeMethods: class'%s' not found", class_name);
    }
    return result;
}

static JNINativeMethod methods[] = {
        {"nativeCreate",                            "()J",                                       (void *) nativeCreate},
        {"nativeDestroy",                           "(J)V",                                      (void *) nativeDestroy},
        //
        {"nativeConnect",                           "(JIIIIILjava/lang/String;)I",               (void *) nativeConnect},
        {"nativeRelease",                           "(J)I",                                      (void *) nativeRelease},

        {"nativeSetStatusCallback",                 "(JLcom/serenegiant/usb/IStatusCallback;)I", (void *) nativeSetStatusCallback},
        {"nativeSetButtonCallback",                 "(JLcom/serenegiant/usb/IButtonCallback;)I", (void *) nativeSetButtonCallback},

        {"nativeGetSupportedSize",                  "(J)Ljava/lang/String;",                     (void *) nativeGetSupportedSize},
        {"nativeSetPreviewSize",                    "(JIIIIIF)I",                                (void *) nativeSetPreviewSize},
        {"nativeStartPreview",                      "(J)I",                                      (void *) nativeStartPreview},
        {"nativeStopPreview",                       "(J)I",                                      (void *) nativeStopPreview},
        {"nativeSetPreviewDisplay",                 "(JLandroid/view/Surface;)I",                (void *) nativeSetPreviewDisplay},
        {"nativeSetFrameCallback",                  "(JLcom/serenegiant/usb/IFrameCallback;I)I", (void *) nativeSetFrameCallback},

        {"nativeSetCaptureDisplay",                 "(JLandroid/view/Surface;)I",                (void *) nativeSetCaptureDisplay},

        {"nativeGetCtrlSupports",                   "(J)J",                                      (void *) nativeGetCtrlSupports},
        {"nativeGetProcSupports",                   "(J)J",                                      (void *) nativeGetProcSupports},

        {"nativeUpdateScanningModeLimit",           "(J)I",                                      (void *) nativeUpdateScanningModeLimit},
        {"nativeSetScanningMode",                   "(JI)I",                                     (void *) nativeSetScanningMode},
        {"nativeGetScanningMode",                   "(J)I",                                      (void *) nativeGetScanningMode},

        {"nativeUpdateExposureModeLimit",           "(J)I",                                      (void *) nativeUpdateExposureModeLimit},
        {"nativeSetExposureMode",                   "(JI)I",                                     (void *) nativeSetExposureMode},
        {"nativeGetExposureMode",                   "(J)I",                                      (void *) nativeGetExposureMode},

        {"nativeUpdateExposurePriorityLimit",       "(J)I",                                      (void *) nativeUpdateExposurePriorityLimit},
        {"nativeSetExposurePriority",               "(JI)I",                                     (void *) nativeSetExposurePriority},
        {"nativeGetExposurePriority",               "(J)I",                                      (void *) nativeGetExposurePriority},

        {"nativeUpdateExposureLimit",               "(J)I",                                      (void *) nativeUpdateExposureLimit},
        {"nativeSetExposure",                       "(JI)I",                                     (void *) nativeSetExposure},
        {"nativeGetExposure",                       "(J)I",                                      (void *) nativeGetExposure},

        {"nativeUpdateExposureRelLimit",            "(J)I",                                      (void *) nativeUpdateExposureRelLimit},
        {"nativeSetExposureRel",                    "(JI)I",                                     (void *) nativeSetExposureRel},
        {"nativeGetExposureRel",                    "(J)I",                                      (void *) nativeGetExposureRel},

        {"nativeUpdateAutoFocusLimit",              "(J)I",                                      (void *) nativeUpdateAutoFocusLimit},
        {"nativeSetAutoFocus",                      "(JZ)I",                                     (void *) nativeSetAutoFocus},
        {"nativeGetAutoFocus",                      "(J)I",                                      (void *) nativeGetAutoFocus},

        {"nativeUpdateFocusLimit",                  "(J)I",                                      (void *) nativeUpdateFocusLimit},
        {"nativeSetFocus",                          "(JI)I",                                     (void *) nativeSetFocus},
        {"nativeGetFocus",                          "(J)I",                                      (void *) nativeGetFocus},

        {"nativeUpdateFocusRelLimit",               "(J)I",                                      (void *) nativeUpdateFocusRelLimit},
        {"nativeSetFocusRel",                       "(JI)I",                                     (void *) nativeSetFocusRel},
        {"nativeGetFocusRel",                       "(J)I",                                      (void *) nativeGetFocusRel},

//	{ "nativeUpdateFocusSimpleLimit",	"(J)I", (void *) nativeUpdateFocusSimpleLimit },
//	{ "nativeSetFocusSimple",			"(JI)I", (void *) nativeSetFocusSimple },
//	{ "nativeGetFocusSimple",			"(J)I", (void *) nativeGetFocusSimple },

        {"nativeUpdateIrisLimit",                   "(J)I",                                      (void *) nativeUpdateIrisLimit},
        {"nativeSetIris",                           "(JI)I",                                     (void *) nativeSetIris},
        {"nativeGetIris",                           "(J)I",                                      (void *) nativeGetIris},

        {"nativeUpdateIrisRelLimit",                "(J)I",                                      (void *) nativeUpdateIrisRelLimit},
        {"nativeSetIrisRel",                        "(JI)I",                                     (void *) nativeSetIrisRel},
        {"nativeGetIrisRel",                        "(J)I",                                      (void *) nativeGetIrisRel},

        {"nativeUpdatePanLimit",                    "(J)I",                                      (void *) nativeUpdatePanLimit},
        {"nativeSetPan",                            "(JI)I",                                     (void *) nativeSetPan},
        {"nativeGetPan",                            "(J)I",                                      (void *) nativeGetPan},

        {"nativeUpdateTiltLimit",                   "(J)I",                                      (void *) nativeUpdateTiltLimit},
        {"nativeSetTilt",                           "(JI)I",                                     (void *) nativeSetTilt},
        {"nativeGetTilt",                           "(J)I",                                      (void *) nativeGetTilt},

        {"nativeUpdateRollLimit",                   "(J)I",                                      (void *) nativeUpdateRollLimit},
        {"nativeSetRoll",                           "(JI)I",                                     (void *) nativeSetRoll},
        {"nativeGetRoll",                           "(J)I",                                      (void *) nativeGetRoll},

        {"nativeUpdatePanRelLimit",                 "(J)I",                                      (void *) nativeUpdatePanRelLimit},
        {"nativeSetPanRel",                         "(JI)I",                                     (void *) nativeSetPanRel},
        {"nativeGetPanRel",                         "(J)I",                                      (void *) nativeGetPanRel},

        {"nativeUpdateTiltRelLimit",                "(J)I",                                      (void *) nativeUpdateTiltRelLimit},
        {"nativeSetTiltRel",                        "(JI)I",                                     (void *) nativeSetTiltRel},
        {"nativeGetTiltRel",                        "(J)I",                                      (void *) nativeGetTiltRel},

        {"nativeUpdateRollRelLimit",                "(J)I",                                      (void *) nativeUpdateRollRelLimit},
        {"nativeSetRollRel",                        "(JI)I",                                     (void *) nativeSetRollRel},
        {"nativeGetRollRel",                        "(J)I",                                      (void *) nativeGetRollRel},

        {"nativeUpdateAutoWhiteBlanceLimit",        "(J)I",                                      (void *) nativeUpdateAutoWhiteBlanceLimit},
        {"nativeSetAutoWhiteBlance",                "(JZ)I",                                     (void *) nativeSetAutoWhiteBlance},
        {"nativeGetAutoWhiteBlance",                "(J)I",                                      (void *) nativeGetAutoWhiteBlance},

        {"nativeUpdateAutoWhiteBlanceCompoLimit",   "(J)I",                                      (void *) nativeUpdateAutoWhiteBlanceCompoLimit},
        {"nativeSetAutoWhiteBlanceCompo",           "(JZ)I",                                     (void *) nativeSetAutoWhiteBlanceCompo},
        {"nativeGetAutoWhiteBlanceCompo",           "(J)I",                                      (void *) nativeGetAutoWhiteBlanceCompo},

        {"nativeUpdateWhiteBlanceLimit",            "(J)I",                                      (void *) nativeUpdateWhiteBlanceLimit},
        {"nativeSetWhiteBlance",                    "(JI)I",                                     (void *) nativeSetWhiteBlance},
        {"nativeGetWhiteBlance",                    "(J)I",                                      (void *) nativeGetWhiteBlance},

        {"nativeUpdateWhiteBlanceCompoLimit",       "(J)I",                                      (void *) nativeUpdateWhiteBlanceCompoLimit},
        {"nativeSetWhiteBlanceCompo",               "(JI)I",                                     (void *) nativeSetWhiteBlanceCompo},
        {"nativeGetWhiteBlanceCompo",               "(J)I",                                      (void *) nativeGetWhiteBlanceCompo},

        {"nativeUpdateBacklightCompLimit",          "(J)I",                                      (void *) nativeUpdateBacklightCompLimit},
        {"nativeSetBacklightComp",                  "(JI)I",                                     (void *) nativeSetBacklightComp},
        {"nativeGetBacklightComp",                  "(J)I",                                      (void *) nativeGetBacklightComp},

        {"nativeUpdateBrightnessLimit",             "(J)I",                                      (void *) nativeUpdateBrightnessLimit},
        {"nativeSetBrightness",                     "(JI)I",                                     (void *) nativeSetBrightness},
        {"nativeGetBrightness",                     "(J)I",                                      (void *) nativeGetBrightness},

        {"nativeUpdateCameraSizeCurLimit",          "(J)I",                                      (void *) nativeUpdateCameraSizeCurLimit},
        {"nativeSetResolution",                     "(JII)I",                                    (void *) nativeSetResolution},
        {"nativeSetDensity",                        "(JIIII)I",                                  (void *) nativeSetDensity},
        {"nativeSetClipLocationGridByCenterX",      "(JIIII)I",                                  (void *) nativeSetClipLocationGridByCenterX},
        {"nativeGetResolution",                     "(J)[I",                                     (void *) nativeGetResolution},
        {"nativeGetClipLocation",                   "(J)[S",                                     (void *) nativeGetClipLocation},
        {"nativeGetC1Version",                      "(J)[S",                                     (void *) nativeGetC1Version},
        {"nativeSetClipLocation",                   "(J)I",                                      (void *) nativeSetClipLocation},
        {"nativeSetClipLocationCameraIndex_360",    "(JI)I",                                     (void *) nativeSetClipLocationCameraIndex_360},
        {"nativeSetClipLocationStable_360",         "(JI)I",                                     (void *) nativeSetClipLocationStable_360},
        {"nativeSetClipLocationStable",             "(JI)I",                                     (void *) nativeSetClipLocationStable},
        {"nativeSetClipLocationScroll_360",         "(JI)I",                                     (void *) nativeSetClipLocationScroll_360},
        {"nativeSetClipLocation_PIP",               "(JIII)I",                                   (void *) nativeSetClipLocation_PIP},
        {"nativeSetClipLocationSpeaker360",         "(JI)I",                                     (void *) nativeSetClipLocationSpeaker360},
        {"nativeSetClipLocationSpeaker_360",         "(JI)I",                                     (void *) nativeSetClipLocationSpeaker_360},
        {"nativeSetClipLocationGrid",               "(J)I",                                      (void *) nativeSetClipLocationGrid},
        {"nativeSetClipMode",                       "(JI)I",                                     (void *) nativeSetClipMode},
        {"nativeSetClipLocation360",                "(J)I",                                      (void *) nativeSetClipLocation360},
        {"nativeGetC9Mode",                         "(J)I",                                      (void *) nativeGetC9Mode},

        {"nativeUpdateContrastLimit",               "(J)I",                                      (void *) nativeUpdateContrastLimit},
        {"nativeSetContrast",                       "(JI)I",                                     (void *) nativeSetContrast},
        {"nativeGetContrast",                       "(J)I",                                      (void *) nativeGetContrast},

        {"nativeUpdateAutoContrastLimit",           "(J)I",                                      (void *) nativeUpdateAutoContrastLimit},
        {"nativeSetAutoContrast",                   "(JZ)I",                                     (void *) nativeSetAutoContrast},
        {"nativeGetAutoContrast",                   "(J)I",                                      (void *) nativeGetAutoContrast},

        {"nativeUpdateSharpnessLimit",              "(J)I",                                      (void *) nativeUpdateSharpnessLimit},
        {"nativeSetSharpness",                      "(JI)I",                                     (void *) nativeSetSharpness},
        {"nativeGetSharpness",                      "(J)I",                                      (void *) nativeGetSharpness},

        {"nativeUpdateGainLimit",                   "(J)I",                                      (void *) nativeUpdateGainLimit},
        {"nativeSetGain",                           "(JI)I",                                     (void *) nativeSetGain},
        {"nativeGetGain",                           "(J)I",                                      (void *) nativeGetGain},

        {"nativeUpdateGammaLimit",                  "(J)I",                                      (void *) nativeUpdateGammaLimit},
        {"nativeSetGamma",                          "(JI)I",                                     (void *) nativeSetGamma},
        {"nativeGetGamma",                          "(J)I",                                      (void *) nativeGetGamma},

        {"nativeUpdateSaturationLimit",             "(J)I",                                      (void *) nativeUpdateSaturationLimit},
        {"nativeSetSaturation",                     "(JI)I",                                     (void *) nativeSetSaturation},
        {"nativeGetSaturation",                     "(J)I",                                      (void *) nativeGetSaturation},

        {"nativeUpdateHueLimit",                    "(J)I",                                      (void *) nativeUpdateHueLimit},
        {"nativeSetHue",                            "(JI)I",                                     (void *) nativeSetHue},
        {"nativeGetHue",                            "(J)I",                                      (void *) nativeGetHue},

        {"nativeUpdateAutoHueLimit",                "(J)I",                                      (void *) nativeUpdateAutoHueLimit},
        {"nativeSetAutoHue",                        "(JZ)I",                                     (void *) nativeSetAutoHue},
        {"nativeGetAutoHue",                        "(J)I",                                      (void *) nativeGetAutoHue},

        {"nativeUpdatePowerlineFrequencyLimit",     "(J)I",                                      (void *) nativeUpdatePowerlineFrequencyLimit},
        {"nativeSetPowerlineFrequency",             "(JI)I",                                     (void *) nativeSetPowerlineFrequency},
        {"nativeGetPowerlineFrequency",             "(J)I",                                      (void *) nativeGetPowerlineFrequency},

        {"nativeUpdateZoomLimit",                   "(J)I",                                      (void *) nativeUpdateZoomLimit},
        {"nativeSetZoom",                           "(JI)I",                                     (void *) nativeSetZoom},
        {"nativeGetZoom",                           "(J)I",                                      (void *) nativeGetZoom},

        {"nativeUpdateZoomRelLimit",                "(J)I",                                      (void *) nativeUpdateZoomRelLimit},
        {"nativeSetZoomRel",                        "(JI)I",                                     (void *) nativeSetZoomRel},
        {"nativeGetZoomRel",                        "(J)I",                                      (void *) nativeGetZoomRel},

        {"nativeUpdateDigitalMultiplierLimit",      "(J)I",                                      (void *) nativeUpdateDigitalMultiplierLimit},
        {"nativeSetDigitalMultiplier",              "(JI)I",                                     (void *) nativeSetDigitalMultiplier},
        {"nativeGetDigitalMultiplier",              "(J)I",                                      (void *) nativeGetDigitalMultiplier},

        {"nativeUpdateDigitalMultiplierLimitLimit", "(J)I",                                      (void *) nativeUpdateDigitalMultiplierLimitLimit},
        {"nativeSetDigitalMultiplierLimit",         "(JI)I",                                     (void *) nativeSetDigitalMultiplierLimit},
        {"nativeGetDigitalMultiplierLimit",         "(J)I",                                      (void *) nativeGetDigitalMultiplierLimit},

        {"nativeUpdateAnalogVideoStandardLimit",    "(J)I",                                      (void *) nativeUpdateAnalogVideoStandardLimit},
        {"nativeSetAnalogVideoStandard",            "(JI)I",                                     (void *) nativeSetAnalogVideoStandard},
        {"nativeGetAnalogVideoStandard",            "(J)I",                                      (void *) nativeGetAnalogVideoStandard},

        {"nativeUpdateAnalogVideoLockStateLimit",   "(J)I",                                      (void *) nativeUpdateAnalogVideoLockStateLimit},
        {"nativeSetAnalogVideoLoackState",          "(JI)I",                                     (void *) nativeSetAnalogVideoLockState},
        {"nativeGetAnalogVideoLoackState",          "(J)I",                                      (void *) nativeGetAnalogVideoLockState},

        {"nativeUpdatePrivacyLimit",                "(J)I",                                      (void *) nativeUpdatePrivacyLimit},
        {"nativeSetPrivacy",                        "(JZ)I",                                     (void *) nativeSetPrivacy},
        {"nativeGetPrivacy",                        "(J)I",                                      (void *) nativeGetPrivacy},
};

int register_uvccamera(JNIEnv *env) {
    LOGV("register_uvccamera:");
    if (registerNativeMethods(env,
                              "com/serenegiant/usb/UVCCamera",
                              methods, NUM_ARRAY_ELEMENTS(methods)) < 0) {
        return -1;
    }
    return 0;
}
