- Windows11(64bit)
- compiler => MSVC
- prefix => C:/gnome
- vulkan_sdk => C:/VulkanSDK
- python => C:/Python313

きれいな glib をビルドするのが肝なのでそこから。

- [pygobject を使う #gstreamer - Qiita](https://qiita.com/ousttrue/items/52af61491fcf92c989f4)

きれいな glib とは、gobject-instropection が完備されていて python と連携できる、つまり pyobject が動く。
flex, bison, iconv, libintl/gettest, pkg-config あたりのインフラの整備でもある。


