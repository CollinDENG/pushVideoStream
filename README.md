# pushVideoStream

该项目基于GStreamer和大疆OSDK4.0的h264接口推流

## 运行方法

1.备份 Onboard-SDK4.0\sample\platform\linux\advanced-sensing\camera_h264_callback_sample 文件夹下的main.cpp和cmakelist文件；

2.将上面三个代码文件放在该目录下，运行

```shell
g++ udpsrc.cpp -o udpsrc `pkg-config --cflags --libs gstreamer-1.0 opencv4`
```

第三步完成后，把udpsrc可执行文件拷贝到Onboard-SDK4.0\build\bin目录下

3.在 Onboard-SDK4.0\build 目录下运行

```shell
cmake ..
make
```

4.在 Onboard-SDK4.0\build\bin 目录下运行

```
接收端：./udpsrc
发送端：./djiosdk-liveview-sample
```

## 效果

视频偶尔卡顿、花屏，延时1.2秒左右；

udpsrc的CPU占用约110%，djiosdk-liveview-sample的CPU占用约40%；

待优化。

