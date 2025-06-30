[VulkanのPushConstantとDescriptorSetの使い分け | Tech Notes](https://chaosplant.tech/tech-notes/page45/)

## DescriptorSet

texture など大きいもの向け

### 複雑な UBO

[Vulkan で複雑なシェーダを使うときのディスクリプタ定義 #Vulkan - Qiita](https://qiita.com/lriki/items/934804030d56fd88dcc8)

## PushConstants

128byte か 256byte ?

https://registry.khronos.org/vulkan/specs/latest/man/html/vkCmdPushConstants.html

```c
void vkCmdPushConstants(
    VkCommandBuffer                             commandBuffer,
    VkPipelineLayout                            layout,
    VkShaderStageFlags                          stageFlags,
    uint32_t                                    offset,
    uint32_t                                    size,
    const void*                                 pValues);
```

UBO など小さもの向け
