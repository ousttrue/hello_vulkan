import { defineConfig } from "vitepress";

// https://vitepress.dev/reference/site-config
export default defineConfig({
  title: "hello_vulkan",
  description: "vulan on windows and android",
  base: "/hello_vulkan/",
  themeConfig: {
    // https://vitepress.dev/reference/default-theme-config
    nav: [
      { text: "Home", link: "/" },
      { text: "Examples", link: "/markdown-examples" },
    ],
    sidebar: [
      {
        text: "Development",
        items: [
          { text: "sdk", link: "/dev/sdk" },
          { text: "c++", link: "/dev/cpp" },
          { text: "android", link: "/dev/android" },
        ],
      },
      {
        text: "Vulkan",
        items: [
          { text: "Vulkan samples", link: "/vulkan/reference" },
          {
            text: "device",
            link: "/vulkan/device/",
            items: [
              { text: "instance", link: "/vulkan/device/instance" },
              { text: "surface", link: "/vulkan/device/surface" },
              {
                text: "physical device",
                link: "/vulkan/device/physical_device",
              },
              { text: "logical device", link: "/vulkan/device/logical_device" },
            ],
          },
          {
            text: "pipeline",
            link: "/vulkan/pipeline/",
            items: [
              { text: "record", link: "/vulkan/pipeline/record" },
              {
                text: "layout", items: [
                  { text: "descriptor", link: "/vulkan/pipeline/layout/descriptor" },
                  { text: "ubo", link: "/vulkan/pipeline/layout/ubo" },
                ]
              },
              {
                text: "graphics",
                link: "/vulkan/pipeline/graphics/",
                items: [
                  { text: "renderpass", link: "/vulkan/pipeline/graphics/renderpass" },
                  { text: "image", link: "/vulkan/pipeline/graphics/image" },
                  { text: "vertex", link: "/vulkan/pipeline/graphics/vertex" },
                ]
              },
            ],
          },
          {
            text: "swapchain",
            link: "/vulkan/swapchain/",
            items: [
              { text: "command", link: "/vulkan/swapchain/command" },
              { text: "swapchain", link: "/vulkan/swapchain/swapchain" },
            ],
          },
          {
            text: "shader",
            link: "/vulkan/shader/",
            items: [{ text: "spv", link: "/vulkan/pipeline/spv" }],
          },
        ],
      },
      // {
      //   text: "Shader",
      //   link: "/shader",
      //   items: [],
      // },
      {
        text: "OpenXR",
        items: [
          { text: "OpenXR samples", link: "/openxr/reference" },
          { text: "Initialize", link: "/openxr/initialization" },
          { text: "Swapchain", link: "openxr/swapchain" },
          { text: "Composition", link: "openxr/composition" },
          {
            text: "Extensions", items: [
              { text: "Hand Tracking", link: "/openxr/extensions/hand_tracking" },
              { text: "Spatial Entities", link: "/openxr/extensions/spatial_entities" },
              { text: "Android XR", link: "/openxr/extensions/android_xr" },
            ],
          },
        ],
      },
      {
        text: "Gtk",
        items: [{ text: "Gtk", link: "/gtk/" }],
      },
      {
        text: "Gstreamer",
        items: [
          { text: "Gstreamer", link: "/gst/" },
          {
            text: "samples",
            items: [
              { text: "gstreamer android", link: "/gst/gst_android" },
              { text: "gstreamer vulkan", link: "/gst/gst_vulkan" },
              { text: "gstreamer opengl", link: "/gst/gst_opengl" },
            ],
          },
          { text: "gst-launch-1.0", link: "/gst/gst-launch" },
          { text: "GstPipeline", link: "/gst/gst_pipeline" },
          { text: "GstSample", link: "/gst/gst_sample" },
          { text: "GstScreenCapture", link: "/gst/gst_screencapture" },
          { text: "app plugin", link: "/gst/app-plugin" },
        ],
      },
    ],
    socialLinks: [
      { icon: "github", link: "https://github.com/ousttrue/hello_vulkan" },
    ],
  },
});
