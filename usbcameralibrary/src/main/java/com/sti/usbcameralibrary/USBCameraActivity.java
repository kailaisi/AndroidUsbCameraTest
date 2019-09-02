package com.sti.usbcameralibrary;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.app.ComponentActivity;
import androidx.core.content.ContextCompat;


import android.Manifest;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.hardware.usb.UsbDevice;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Looper;
import android.util.Log;
import android.view.KeyEvent;
import android.view.Surface;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import com.jiangdg.usbcamera.UVCCameraHelper;
import com.jiangdg.usbcamera.utils.FileUtils;
import com.serenegiant.usb.USBMonitor;
import com.serenegiant.usb.common.AbstractUVCCameraHandler;
import com.serenegiant.usb.widget.CameraViewInterface;

import java.io.File;

import java.io.File;

public class USBCameraActivity extends AppCompatActivity implements CameraViewInterface.Callback {

    private static final String TAG = "USBCameraActivity";
    private static final int REQUEST_PERMISSION_CODE = 908;
    private View mTextureView;
    private View mBtntakePhoto;
    private ImageView mPhotoPreview;
    private ImageView mPhotoPreviewBtnOk;
    private ImageView mBtnBack;
    private ImageView mPhotoPreviewBtnClear;
    private TextView mTvCameraNoSignal;

    private UVCCameraHelper mCameraHelper;
    private CameraViewInterface mUVCCameraView;

    private boolean isRequest;
    private boolean isPreview;
    private boolean isCreated;

    private String tempImageSavePath;//当前照片的存储位置


    private UVCCameraHelper.OnMyDevConnectListener listener = new UVCCameraHelper.OnMyDevConnectListener() {
        @Override
        public void onAttachDev(UsbDevice device) {
            if (mCameraHelper == null || mCameraHelper.getUsbDeviceCount() == 0) {
                Log.i(TAG, "未检测到USB摄像头设备");
                return;
            }
            // 请求打开摄像头
            if (!isRequest) {
                isRequest = true;
                if (mCameraHelper != null) {
                    mCameraHelper.requestPermission(0);
                }
            }
        }

        @Override
        public void onDettachDev(UsbDevice device) {
            if (isRequest) {
                // 关闭摄像头
                isRequest = false;
                mCameraHelper.closeCamera();
                Log.i(TAG, device.getDeviceName() + "已拨出");
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        mTvCameraNoSignal.setVisibility(View.VISIBLE);
                    }
                });
            }
        }

        @Override
        public void onConnectDev(UsbDevice device, boolean isConnected) {
            if (!isConnected) {
                Log.i(TAG, "连接失败，请检查分辨率参数是否正确");
                isPreview = false;
            } else {
                isPreview = true;
                Log.i(TAG, "连接成功");
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        mTvCameraNoSignal.setVisibility(View.GONE);
                    }
                });
            }
        }

        @Override
        public void onDisConnectDev(UsbDevice device) {
            Log.i(TAG, "连接失败");
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_usbcamera);
        mTextureView = findViewById(R.id.camera_view);
        mBtntakePhoto = findViewById(R.id.btn_take_photo);
        mPhotoPreview = findViewById(R.id.iv_take_photo_preview_view);
        mPhotoPreviewBtnOk = findViewById(R.id.iv_btn_camera_ok);
        mPhotoPreviewBtnClear = findViewById(R.id.iv_btn_camera_clear);
        mTvCameraNoSignal = findViewById(R.id.tv_camera_no_signal);
        mBtnBack = findViewById(R.id.iv_btn_camera_back);

        mBtnBack.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                finish();
            }
        });

        mBtntakePhoto.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                if (mCameraHelper != null) {
                    Log.i(TAG, "申请拍照");
                    checkPermissionAndTakePhoto();
                }
            }
        });

        mPhotoPreviewBtnOk.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                mPhotoPreview.setImageBitmap(null);
                mPhotoPreview.setVisibility(View.GONE);
                mPhotoPreviewBtnClear.setVisibility(View.GONE);
                mBtntakePhoto.setEnabled(true);
                Intent intent = new Intent();
                intent.setData(Uri.fromFile(new File(tempImageSavePath)));
//                intent.putExtra("data", tempImageSavePath);
                setResult(RESULT_OK, intent);
                finish();
            }
        });


        mPhotoPreviewBtnClear.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                mPhotoPreview.setImageBitmap(null);
                mBtntakePhoto.setEnabled(true);
                mPhotoPreview.setVisibility(View.GONE);
                mPhotoPreviewBtnOk.setVisibility(View.GONE);
                mPhotoPreviewBtnClear.setVisibility(View.GONE);
                try {
                    new File(tempImageSavePath).delete();
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        });

        mUVCCameraView = (CameraViewInterface) mTextureView;

        mUVCCameraView.setCallback(this);

        // 初始化引擎
        mCameraHelper = UVCCameraHelper.getInstance();

//        mCameraHelper.setDefaultPreviewSize(1280,720);

        mCameraHelper.setDefaultFrameFormat(UVCCameraHelper.FRAME_FORMAT_MJPEG);
        mCameraHelper.initUSBMonitor(this, mUVCCameraView, listener);
        mCameraHelper.updateResolution(640, 480);
        mCameraHelper.setOnPreviewFrameListener(new AbstractUVCCameraHandler.OnPreViewResultListener() {
            @Override
            public void onPreviewResult(byte[] nv21Yuv) {
                Log.d(TAG, "onPreviewResult: " + nv21Yuv.length);
            }
        });
    }

    private void takePhoto() {
        String picPath = USBCameraLib.IMG_SAVE_ROOT_PATH + File.separator + System.currentTimeMillis() + UVCCameraHelper.SUFFIX_JPEG;
        mCameraHelper.capturePicture(picPath, new AbstractUVCCameraHandler.OnCaptureListener() {
            @Override
            public void onCaptureResult(final String picPath) {
                Log.i(TAG, "save path：" + picPath);
                tempImageSavePath = picPath;
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        mPhotoPreview.setImageURI(Uri.fromFile(new File(picPath)));
                        mBtntakePhoto.setEnabled(false);
                        mPhotoPreview.setVisibility(View.VISIBLE);
                        mPhotoPreviewBtnOk.setVisibility(View.VISIBLE);
                        mPhotoPreviewBtnClear.setVisibility(View.VISIBLE);
                    }
                });
            }
        });
    }


    @Override
    protected void onStart() {
        super.onStart();
        // step.2 register USB event broadcast
        if (mCameraHelper != null) {
            mCameraHelper.registerUSB();
        }
    }


    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (mCameraHelper != null) {
            mCameraHelper.unregisterUSB();
        }
        if (mCameraHelper != null) {
            mCameraHelper.release();
        }
    }

    private void showShortMsg(String msg) {
        Toast.makeText(this, msg, Toast.LENGTH_SHORT).show();
    }

    @Override
    public void onSurfaceCreated(CameraViewInterface view, Surface surface) {
        if (!isPreview && mCameraHelper.isCameraOpened()) {
            showShortMsg("开始预览");
            Log.e(TAG, "开始预览");
            mCameraHelper.startPreview(mUVCCameraView);
            isPreview = true;
            isCreated = true;
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
//        if (requestCode == REQUEST_PERMISSION_CODE) {
//            checkPermissionAndTakePhoto();
//        }

        switch (requestCode) {
            case REQUEST_PERMISSION_CODE: {
                // If request is cancelled, the result arrays are empty.
                if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {

                    // permission was granted, yay! Do the
                    // contacts-related task you need to do.
                    checkPermissionAndTakePhoto();

                } else {

                    // permission denied, boo! Disable the
                    // functionality that depends on this permission.
//                    checkPermissionAndTakePhoto();
                    Toast.makeText(this, "权限不足", Toast.LENGTH_SHORT).show();
                    finish();
                }
                return;
            }
        }
    }

    private void checkPermissionAndTakePhoto() {


        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M && (ActivityCompat.checkSelfPermission(USBCameraActivity.this, Manifest.permission.READ_EXTERNAL_STORAGE) == PackageManager.PERMISSION_DENIED
                || ActivityCompat.checkSelfPermission(USBCameraActivity.this, Manifest.permission.WRITE_EXTERNAL_STORAGE) == PackageManager.PERMISSION_DENIED)) {

            if (ActivityCompat.shouldShowRequestPermissionRationale(this, Manifest.permission.READ_EXTERNAL_STORAGE) || ActivityCompat.shouldShowRequestPermissionRationale(this, Manifest.permission.WRITE_EXTERNAL_STORAGE)) {

//                Toast.makeText(this, "存储权限不足，请授权后使用", Toast.LENGTH_LONG).show();

                AlertDialog dialog = new AlertDialog.Builder(this).setMessage("存储权限不足，是否立即授权？").setNegativeButton("确定", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialogInterface, int i) {
//                        Toast.makeText(USBCameraActivity.this, "打开设置进行授权", Toast.LENGTH_LONG).show();
                        dialogInterface.dismiss();

                        Intent mIntent = new Intent();
                        mIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                        mIntent.setAction("android.settings.APPLICATION_DETAILS_SETTINGS");
                        mIntent.setData(Uri.fromParts("package", getPackageName(), null));
                        startActivity(mIntent);
                    }
                }).setPositiveButton("取消", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialogInterface, int i) {
                        dialogInterface.dismiss();
                    }
                }).show();

            } else {

                ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.READ_CONTACTS}, REQUEST_PERMISSION_CODE);

            }


            Log.i(TAG, "权限不足");
        } else {
            takePhoto();
        }
    }

    @Override
    public void onSurfaceChanged(CameraViewInterface view, Surface surface, int width, int height) {

    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        Log.i(TAG, "按键：" + event.getKeyCode() + "  " + event.getAction());
        return super.dispatchKeyEvent(event);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        Log.i(TAG, "按键：" + keyCode + "  " + event.getAction());
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public void onSurfaceDestroy(CameraViewInterface view, Surface surface) {
        Log.e(TAG, "结束预览");
        if (isPreview && mCameraHelper.isCameraOpened()) {
            mCameraHelper.stopPreview();
            isPreview = false;
            isCreated = false;
        }
    }
}
