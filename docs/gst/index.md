https://gstreamer.freedesktop.org/documentation/index.html

https://gitlab.freedesktop.org/dabrain34/GstPipelineStudio

- [Hello Library World: GStreamer でテキスト処理 1: Core機能だけで cat, cp, dd, & tee](https://hellolibraryworld.blogspot.com/2016/12/gstreamer-1-core-cat-cp-dd-tee.html)

# cli

- [GStreamerの使い方 - 入門編](https://zenn.dev/hidenori3/articles/541cc59c0106b7)

- [GStreamer を使おう！ その1 ～GStreamer の世界へようこそ編～](https://blog.ibextech.jp/posts/2024/gstreamer/welcome-to-gstreamer/)

```sh
> gst-launch-1.0 videotestsrc pattern=snow ! videoconvert ! autovideosink

> gst-launch-1.0 videotestsrc pattern=ball flip=true ! videoconvert ! autovideosink

> gst-launch-1.0 videotestsrc ! videoconvert ! vertigotv ! videoconvert ! autovideosink

> gst-launch-1.0 videotestsrc ! video/x-raw,width=1920,height=1080 ! videoconvert ! vertigotv ! videoconvert ! autovideosink
```

```sh
> gst-launch-1.0 audiotestsrc freq=20 ! audioconvert ! wavescope ! autovideosink

> gst-launch-1.0 audiotestsrc wave=pink-noise ! audioconvert ! synaescope ! autovideosink
```

# udpsink

- [GStreamer を使用した端末間での映像転送](https://www.gclue.jp/2022/06/gstreamer.html)

# desktop

```sh
> gst-launch-1.0 d3d11screencapturesrc ! videoscale ! video/x-raw,width=[1,1920],height=[1,1080],pixel-aspect-ratio=1/1 ! autovideosink
```

# webcam

https://gstreamer.freedesktop.org/documentation/directshow/index.html

```sh
> gst-launch-1.0 dshowvideosrc ! autovideosink
```

# 環境変数

- [今日からから使える GStreamer の環境変数 #Linux - Qiita](https://qiita.com/tetsukuz/items/43b9fff280c7d7e81d1d)

`GST_DEBUG_DUMP_DOT_DIR` 

