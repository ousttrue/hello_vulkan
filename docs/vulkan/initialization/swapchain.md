# VkSwapchain

Backbuffer の VkFrameBuffer が VkRenderPass を要求するので、
Rendering 直前に必用に応じて作成する。

## VkSwapchain
### dependency

- VkDevice
- VkQueue(graphics)
- VkQueue(presentation)

#### graphics queue と presentation queue が同一の場合

#### graphics queue と presentation queue が異なる場合

今のところ遭遇したこと無し。

## Backbuffer

Swapchain から VkImage (複数) を得て VkFramebuffer を作成し、 VkSemaphore などで同期システムを構築する。

### dependency

- VkSwapchain
- VkRenderPass



