# Pipeline. Vertex, UBO and Image. Descriptor

## ComputePipeline

- VkPipeline
  - VkPipelineShaderStageCreateInfo
  - VkPipelineLayout
    - VkDescriptorSetLayout[]
    - VkPushConstntRange[]

## GraphicsPipeline

- VkPipeline

  - VkPipelineShaderStageCreateInfo

  * VertexInput
  * InputAssembly
  * Tesselation
  * Viewport
  * Rasterrization
  * Mltisample
  * DepthStencil
  * ColorBlend
  * Dynamic

  - VkPipelineLayout
    - VkDescriptorSetLayout[]
    - VkPushConstntRange[]

  * VkRenderPass
    - VkAttachmentDescription[]
    - VkSubpassDescription[]
    - VkSubpassDependency[]


# VkPipeline

## dependencies

- VkDevice


```cpp
typedef struct VkGraphicsPipelineCreateInfo {
    VkStructureType                                  sType;
    const void*                                      pNext;
    VkPipelineCreateFlags                            flags;
    uint32_t                                         stageCount;
    const VkPipelineShaderStageCreateInfo*           pStages;
    const VkPipelineVertexInputStateCreateInfo*      pVertexInputState;
    const VkPipelineInputAssemblyStateCreateInfo*    pInputAssemblyState;
    const VkPipelineTessellationStateCreateInfo*     pTessellationState;
    const VkPipelineViewportStateCreateInfo*         pViewportState;
    const VkPipelineRasterizationStateCreateInfo*    pRasterizationState;
    const VkPipelineMultisampleStateCreateInfo*      pMultisampleState;
    const VkPipelineDepthStencilStateCreateInfo*     pDepthStencilState;
    const VkPipelineColorBlendStateCreateInfo*       pColorBlendState;
    const VkPipelineDynamicStateCreateInfo*          pDynamicState;
    VkPipelineLayout                                 layout;
    VkRenderPass                                     renderPass;
    uint32_t                                         subpass;
    VkPipeline                                       basePipelineHandle;
    int32_t                                          basePipelineIndex;
} VkGraphicsPipelineCreateInfo;
```
