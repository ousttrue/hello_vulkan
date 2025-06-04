# D3D11 + JPEG

## src

```sh
> gst-launch-1.0 d3d11screencapturesrc ! videoscale ! video/x-raw,width=[1,1920],height=[1,1080],pixel-aspect-ratio=1/1 ! videoconvert ! video/x-raw,format=I420 ! jpegenc ! rtpjpegpay ! udpsink host=127.0.0.1 port=7001
```

::: warning
4k だと動かない?
:::

## rcv

```sh
> gst-launch-1.0 -v udpsrc port=7001 ! application/x-rtp,encoding-name=JPEG ! rtpjpegdepay ! jpegdec ! autovideosink
```

# D3D11 + nvenc H264(4k)

TODO:
