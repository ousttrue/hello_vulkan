gtk + vulkan + gstreamer したいので関連性が低めだがここに情報を記録する。

- [Windows上の Python-3.11 で動く Gtk4 や GStreamer をビルドする #gstreamer - Qiita](https://qiita.com/ousttrue/items/ac591be1654615e1b178)

# build dependency

依存の循環が2つある。

- glib -> gobject-introspection -> gobject
- freetype, fontconfig, cairo, pango, harfbuzz (なんかこの辺り)

## m4, flex, bison

- prebuilt m4
- winflex

## glib, pkg-config

- glib without gobject-introspection
- pkg-config

## glib, gobject-introspection, pygobject

- gobject-introspection
- glib with gobject-introction

## zlib, libintl, gettext, iconv, libxml, expat, xslt

## freetype, cairo, pango, harfbuzz

## libsoup

- sqlite3
- nghttp2
- tsl
- libpsl

## gstreamer

## gtk4

## memo

### vala

windows では build できなくもないが、ちゃんと動くかわからぬ。

### gtkmm

windows では build 困難。
perl でトラブルが起きて手に負えない。
