# appsink

## new-sample

- [app plugin でより深いコードを書く #Linux - Qiita](https://qiita.com/tetsukuz/items/88c5e98e8a5aef3dcb6d)

```c
g_signal_connect(appsink, "new-sample", G_CALLBACK(cb_new_sample), NULL);
```

## gst_app_sink_try_pull_sample

```c
GstSample *videosample = gst_app_sink_try_pull_sample(GST_APP_SINK(this->_appsink), 10 * GST_MSECOND);
```
