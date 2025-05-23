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
        text: "Reference",
        link: "/reference",
      },
      {
        text: "Development",
        items: [
          { text: "sdk", link: "/dev/sdk" },
          { text: "c++", link: "/dev/cpp" },
          { text: "cmake", link: "/dev/cmake" },
          { text: "android", link: "/dev/android" },
        ],
      },
      {
        text: "Vulkan",
        items: [
          {
            text: "初期化",
            link: "/vulkan/initialization",
            items: [
              { text: "instance", link: "/vulkan/initialization/instance" },
              { text: "surface", link: "/vulkan/initialization/surface" },
              { text: "device", link: "/vulkan/initialization/device" },
              { text: "pipeline", link: "/vulkan/initialization/pipeline" },
              { text: "swapchain", link: "/vulkan/initialization/swapchain" },
            ],
          },
          {
            text: "GraphicsPipeline",
            link: "/vulkan/pipeline",
            items: [
              { text: "command", link: "/vulkan/pipeline/command" },
              { text: "spv", link: "/vulkan/pipeline/spv" },
            ],
          },
        ],
      },
      {
        text: "Shader",
        link: "/shader",
        items: [],
      },
      {
        text: "OpenXR",
        items: [{ text: "初期化", link: "/openxr/initialization" }],
      },
    ],

    socialLinks: [
      { icon: "github", link: "https://github.com/vuejs/vitepress" },
    ],
  },
});
