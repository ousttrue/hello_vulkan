## ImageView

```cpp
VkImageViewCreateInfo imageViewCreateInfo{
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                   .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                   .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                   .a = VK_COMPONENT_SWIZZLE_IDENTITY},
    //     colorViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    //     colorViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    //     colorViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
    //     colorViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                         .baseMipLevel = 0,
                         .levelCount = 1,
                         .baseArrayLayer = 0,
                         .layerCount = 1},
};
```
