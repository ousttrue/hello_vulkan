## Command Buffers

https://docs.vulkan.org/spec/latest/chapters/cmdbuffers.html

### VkCommand

- flag ?
- reset ?

### Pool

#### allocate

### record command

#### vkCmdCopyBufferToImage

## 例

### 使い捨て同期実行

- VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
- vkQueueWaitIdle

#### render

##### begin

- attachment
- clearColor
- viewport

```cpp
  VkCommandBufferBeginInfo beginInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = 0,
      .pInheritanceInfo = nullptr,
  };
  if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
    throw std::runtime_error("failed to begin recording command buffer!");
  }

  VkClearValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
  VkRenderPassBeginInfo renderPassInfo{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = _renderPass,
      .framebuffer = _swapchainFramebuffers[i],
      .renderArea =
          {
              .offset = {0, 0},
              .extent = swapchainExtent,
          },
      .clearValueCount = 1,
      .pClearValues = &clearColor,
  };
  vkCmdBeginRenderPass(commandBuffer, &renderPassInfo,
                       VK_SUBPASS_CONTENTS_INLINE);
```

##### bind & draw

```cpp
  // bind
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    _graphicsPipeline);

  VkBuffer vertexBuffers[] = {_vertexBuffer->buffer()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

  vkCmdBindIndexBuffer(commandBuffer, _indexBuffer->buffer(), 0,
                       VK_INDEX_TYPE_UINT16);

  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          _pipelineLayout, 0, 1, &_descriptorSets[i], 0,
                          nullptr);

  // Tell Vulkan to draw the triangle USING THE INDEX BUFFER!
  vkCmdDrawIndexed(commandBuffer, indices.size(), 1, 0, 0, 0);
```

##### end

```cpp
  vkCmdEndRenderPass(commandBuffer);

  if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
    throw std::runtime_error("failed to record command buffer!");
  }
```
