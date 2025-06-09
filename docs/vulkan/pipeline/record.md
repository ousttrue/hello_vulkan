# begin

```cpp
    // VkCommandBuffer commandBuffer;
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
```

# end
