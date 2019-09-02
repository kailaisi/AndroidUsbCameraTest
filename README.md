# AndroidUsbCameraTestProject

#### 介绍
Android USBCamera UVC 项目

基于[该项目](https://github.com/jiangdongguo/AndroidUSBCamera)，在此基础上修复了Android P之后部分设备在拔出USB后导致应用崩溃的问题。

#### 实现功能

1. 预览
2. 拍照
3. 其余功能参考[该项目](https://github.com/jiangdongguo/AndroidUSBCamera)

#### 使用说明

1. 未提交至Jcenter，因此请手动下载集成

#### 其他说明

1. UVC so库采用[saki4510t/UVCCamera](https://github.com/saki4510t/UVCCamera),参考其中usbCameraTest5项目
2. so库中的libusb100.so库存在异常，会导致Android P 部分机型拔出设备崩溃。如你项目引用同样的so库，可直接下载本项目中的so文件进行替换。
3. 可直接使用libusbcamera库进行开发