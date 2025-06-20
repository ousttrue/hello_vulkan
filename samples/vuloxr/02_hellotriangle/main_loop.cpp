#include "../main_loop.h"
#include "../glsl_to_spv.h"
#include "vuloxr/vk.h"
#include "vuloxr/vk/pipeline.h"

// Load our SPIR-V shaders.
const char VS[] = {
#embed "triangle.vert"
    , 0, 0, 0, 0};

const char FS[] = {
#embed "triangle.frag"
    , 0, 0, 0, 0};

struct Vertex {
  float position[4];
  float color[4];
};
// A simple counter-clockwise triangle.
// We specify the positions directly in clip space.
static const Vertex data[] = {
    {
        {-0.5f, -0.5f, 0.0f, 1.0f},
        {1.0f, 0.0f, 0.0f, 1.0f},
    },
    {
        {-0.5f, +0.5f, 0.0f, 1.0f},
        {0.0f, 1.0f, 0.0f, 1.0f},
    },
    {
        {+0.5f, -0.5f, 0.0f, 1.0f},
        {0.0f, 0.0f, 1.0f, 1.0f},
    },
};

class Buffer {
  VkDevice _device;
  VkBuffer _buffer;
  VkDeviceMemory _memory;

public:
  Buffer(VkDevice device, const VkPhysicalDeviceMemoryProperties &props,
         const void *pInitialData, size_t size, VkFlags usage)
      : _device(device) {
    VkBufferCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
    };

    vuloxr::vk::CheckVkResult(vkCreateBuffer(device, &info, nullptr, &_buffer));

    // Ask device about its memory requirements.
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, _buffer, &memReqs);

    VkMemoryAllocateInfo alloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        // We want host visible and coherent memory to simplify things.
        .memoryTypeIndex = findMemoryTypeFromRequirements(
            props, memReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
    };

    // Allocate memory.
    vuloxr::vk::CheckVkResult(
        vkAllocateMemory(device, &alloc, nullptr, &_memory));

    // Buffers are not backed by memory, so bind our memory explicitly to the
    // buffer.
    vkBindBufferMemory(device, _buffer, _memory, 0);

    // Map the memory and dump data in there.
    if (pInitialData) {
      void *pData;
      if (vkMapMemory(device, _memory, 0, size, 0, &pData) != VK_SUCCESS) {
        vuloxr::Logger::Error("vkMapMemory");
        abort();
      }
      memcpy(pData, pInitialData, size);
      vkUnmapMemory(device, _memory);
    }
  }

  ~Buffer() {
    vkFreeMemory(_device, _memory, nullptr);
    vkDestroyBuffer(_device, _buffer, nullptr);
  }

  void bind(VkCommandBuffer cmd, VkDeviceSize offset) {
    vkCmdBindVertexBuffers(cmd, 0, 1, &_buffer, &offset);
  }

private:
  // To create a buffer, both the device and application have requirements from
  // the buffer object.
  // Vulkan exposes the different types of buffers the device can allocate, and
  // we have to find a suitable one. deviceRequirements is a bitmask expressing
  // which memory types can be used for a buffer object. The different memory
  // types' properties must match with what the application wants.
  static uint32_t
  findMemoryTypeFromRequirements(const VkPhysicalDeviceMemoryProperties &props,
                                 uint32_t deviceRequirements,
                                 uint32_t hostRequirements) {
    for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++) {
      if (deviceRequirements & (1u << i)) {
        if ((props.memoryTypes[i].propertyFlags & hostRequirements) ==
            hostRequirements) {
          return i;
        }
      }
    }

    vuloxr::Logger::Error("Failed to obtain suitable memory type.\n");
    abort();
  }
};

void main_loop(const std::function<bool()> &runLoop,
               const vuloxr::vk::Instance &instance,
               vuloxr::vk::Swapchain &swapchain,
               const vuloxr::vk::PhysicalDevice &physicalDevice,
               const vuloxr::vk::Device &device, void *) {

  VkPhysicalDeviceMemoryProperties props;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &props);
  auto vertexBuffer = std::make_shared<Buffer>(
      device, props, data, sizeof(data), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

  auto renderPass = vuloxr::vk::createColorRenderPass(
      device, swapchain.createInfo.imageFormat);

  auto pipelineLayout = vuloxr::vk::createEmptyPipelineLayout(device);

  auto vs = vuloxr::vk::ShaderModule::createVertexShader(
      device, glsl_vs_to_spv(VS), "main");
  auto fs = vuloxr::vk::ShaderModule::createFragmentShader(
      device, glsl_fs_to_spv(FS), "main");

  VkVertexInputBindingDescription bindings[] = {{
      .binding = 0,
      .stride = sizeof(Vertex),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  }};

  VkVertexInputAttributeDescription attributes[] = {
      {
          .location = 0,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = 0,
      },
      {
          .location = 1,
          .binding = 0,
          .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = 4 * sizeof(float),
      },
  };

  vuloxr::vk::PipelineBuilder builder;
  builder.rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  auto pipeline =
      builder.create(device, renderPass, pipelineLayout,
                     {
                         vs.pipelineShaderStageCreateInfo,
                         fs.pipelineShaderStageCreateInfo,
                     },
                     bindings, attributes, {}, {},
                     {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});

  std::vector<std::shared_ptr<vuloxr::vk::SwapchainFramebuffer>> backbuffers(
      swapchain.images.size());
  vuloxr::vk::FlightManager flightManager(
      device, physicalDevice.graphicsFamilyIndex, swapchain.images.size());

  vuloxr::FrameCounter counter;
  while (runLoop()) {
    auto acquireSemaphore = flightManager.getOrCreateSemaphore();
    auto acquired = swapchain.acquireNextImage(acquireSemaphore);
    if (acquired.result == VK_SUCCESS) {
      auto backbuffer = backbuffers[acquired.imageIndex];
      if (!backbuffer) {
        backbuffer = std::make_shared<vuloxr::vk::SwapchainFramebuffer>(
            device, acquired.image, swapchain.createInfo.imageExtent,
            swapchain.createInfo.imageFormat, renderPass);
        backbuffers[acquired.imageIndex] = backbuffer;
      }

      // All queue submissions get a fence that CPU will wait
      // on for synchronization purposes.
      auto [cmd, flight] = flightManager.sync(acquireSemaphore);

      {
        // We will only submit this once before it's recycled.
        VkCommandBufferBeginInfo beginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        vkBeginCommandBuffer(cmd, &beginInfo);

        // Set clear color values.
        VkClearValue clearValue{
            .color =
                {
                    .float32 =
                        {
                            0.1f,
                            0.1f,
                            0.2f,
                            1.0f,
                        },
                },
        };

        // Begin the render pass.
        VkRenderPassBeginInfo rpBegin = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = renderPass,
            .framebuffer = backbuffer->framebuffer,
            .renderArea = {.extent = swapchain.createInfo.imageExtent},
            .clearValueCount = 1,
            .pClearValues = &clearValue,
        };

        // We will add draw commands in the same command buffer.
        vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

        // pipeline->render(cmd, backbuffer->framebuffer,
        //                  swapchain.createInfo.imageExtent);
        // Bind the graphics pipeline.
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        // Set up dynamic state.
        // Viewport
        VkViewport vp = {
            .x = 0.0f,
            .y = 0.0f,
            .width = float(swapchain.createInfo.imageExtent.width),
            .height = float(swapchain.createInfo.imageExtent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        vkCmdSetViewport(cmd, 0, 1, &vp);

        // Scissor box
        VkRect2D scissor{
            .extent = swapchain.createInfo.imageExtent,
        };
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        // Bind vertex buffer.
        vertexBuffer->bind(cmd, 0);

        // Draw three vertices with one instance.
        vkCmdDraw(cmd, 3, 1, 0, 0);

        // Complete render pass.
        vkCmdEndRenderPass(cmd);

        // Complete the command buffer.
        if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
          vuloxr::Logger::Error("vkEndCommandBuffer");
          abort();
        }
      }

      vuloxr::vk::CheckVkResult(device.submit(
          cmd, acquireSemaphore, flight.submitSemaphore, flight.submitFence));
      vuloxr::vk::CheckVkResult(
          swapchain.present(acquired.imageIndex, flight.submitSemaphore));

    } else if (acquired.result == VK_SUBOPTIMAL_KHR ||
               acquired.result == VK_ERROR_OUT_OF_DATE_KHR) {
      vuloxr::Logger::Error("[RESULT_ERROR_OUTDATED_SWAPCHAIN]");
      vkQueueWaitIdle(swapchain.presentQueue);
      flightManager.reuseSemaphore(acquireSemaphore);
      // TODO:
      // swapchain = {};
      // return true;

    } else {
      // error ?
      vuloxr::Logger::Error("Unrecoverable swapchain error.");
      vkQueueWaitIdle(swapchain.presentQueue);
      flightManager.reuseSemaphore(acquireSemaphore);
      return;
    }

    counter.frameEnd();
  }

  vkDeviceWaitIdle(device);
}
