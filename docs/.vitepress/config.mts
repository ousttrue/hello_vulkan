import { defineConfig } from 'vitepress'

// https://vitepress.dev/reference/site-config
export default defineConfig({
  title: "hello_vulkan",
  description: "vulan on windows and android",
  themeConfig: {
    // https://vitepress.dev/reference/default-theme-config
    nav: [
      { text: 'Home', link: '/' },
      { text: 'Examples', link: '/markdown-examples' }
    ],

    sidebar: [
      {
        text: 'build',
        items: [
          { text: 'build', link: '/cmake' },
          { text: 'android apk', link: '/apk' }
        ]
      },
      {
        text: 'vulkan',
        items: [
          { text: 'instance', link: '/vulkan/instance' },
          { text: 'device', link: '/vulkan/instance' },
          { text: 'swapchain', link: '/vulkan/swapchain' },
          { text: 'command', link: '/vulkan/command' },
          { text: 'spv', link: '/vulkan/spv' },
          { text: 'pipeline', link: '/vulkan/pipeline' },
        ]
      },
    ],

    socialLinks: [
      { icon: 'github', link: 'https://github.com/vuejs/vitepress' }
    ]
  }
})
