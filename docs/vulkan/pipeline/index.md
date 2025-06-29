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
