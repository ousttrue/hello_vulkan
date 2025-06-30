
## VkAttachmentDescription

```cpp
  VkAttachmentDescription attachments[] = {
      {
          .format = colorFormat,
          .samples = VK_SAMPLE_COUNT_1_BIT,
          .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, // 👈
          .storeOp = VK_ATTACHMENT_STORE_OP_STORE, // 👈
          .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
          .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
          // .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, // 👈
          // .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, // 👈
          //                 VkImageLayout initialLayout =
          //                 VK_IMAGE_LAYOUT_UNDEFINED for clear VkImageLayout
          //                 finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      },
      {
          .format = depthFormat,
          .samples = VK_SAMPLE_COUNT_1_BIT,
          .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, // 👈
          .storeOp = VK_ATTACHMENT_STORE_OP_STORE, // 👈
          .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
          .stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
          .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, // 👈
          .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, // 👈
      },
  };
```

