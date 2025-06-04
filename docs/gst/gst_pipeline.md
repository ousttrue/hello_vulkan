# cli

https://gstreamer.freedesktop.org/documentation/tools/gst-launch.html

```sh
> gst-launch-1.0 videotestsrc pattern=snow ! videoconvert ! autovideosink
```

# gst_parse_launch

- [Hello Library World: GStreamer でプログラミング 1 (Pipeline と Parse )](https://hellolibraryworld.blogspot.com/2015/12/gstreamer-1-pipeline-parse.html)

`gst-launch-1.0` 風の`gst-launch-1.0` 風の文字列から pipeline を生成する。

```c
pipeline = gst_parse_launch("videotestsrc ! autovideosink", &error);
```

# gst_pipeline_new

https://gstreamer.freedesktop.org/documentation/gstreamer/gstpipeline.html?gi-language=c

- [GStreamer を使ったアプリケーションの作成 | 株式会社ピクセラ](https://www.pixela.co.jp/products/pickup/dev/gstreamer/gst_2_write_a_simple_playback_application.html)

```c++
auto pipeline = gst_pipeline_new("pipeline_name");
```
