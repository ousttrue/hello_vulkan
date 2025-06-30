#include "../main_loop.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <json/json.h>

const char gltf[] = {
#embed "triangle.gltf"
};

class Scene {
public:
  void loadGltf(const char *gltf) {
    Json::Value root;
    Json::Reader reader;
    bool parsingSuccessful = reader.parse(gltf, root);
    if (parsingSuccessful) {
      // std::cout << root;
      vuloxr::Logger::Error("gltf json");
    } else {
      vuloxr::Logger::Error("Error parsing the string");
    }
  }
};

struct ImGuiVulkanResource {
  VkInstance instance;
  VkPhysicalDevice physicalDevice;
  uint32_t queueFamily;
  VkDevice device;
  VkQueue queue;
  VkRenderPass renderPass = VK_NULL_HANDLE;
  // VkPipelineCache pipelineCache = VK_NULL_HANDLE;
  VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

public:
  ImGuiVulkanResource(VkInstance _instance, VkPhysicalDevice _physicalDevice,
                      uint32_t _queueFamily, VkDevice _device, VkFormat format,
                      uint32_t imageCount)
      : instance(_instance), physicalDevice(_physicalDevice),
        queueFamily(_queueFamily), device(_device) {
    vkGetDeviceQueue(device, queueFamily, 0, &this->queue);

    {
      // Create Descriptor Pool
      // If you wish to load e.g. additional textures you may need to alter
      // pools sizes and maxSets.
      VkDescriptorPoolSize pool_sizes[] = {
          {
              .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount =
                  IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE,
          },
      };
      VkDescriptorPoolCreateInfo pool_info = {
          .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
          .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
          .maxSets = 0,
          .poolSizeCount = static_cast<uint32_t>(IM_ARRAYSIZE(pool_sizes)),
          .pPoolSizes = pool_sizes,
      };
      for (VkDescriptorPoolSize &pool_size : pool_sizes) {
        pool_info.maxSets += pool_size.descriptorCount;
      }
      vuloxr::vk::CheckVkResult(vkCreateDescriptorPool(
          this->device, &pool_info, nullptr, &this->descriptorPool));
    }

    {
      VkAttachmentDescription attachment = {
          .format = format,
          .samples = VK_SAMPLE_COUNT_1_BIT,
          .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
          .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
          .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
          .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
          .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
          .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      };
      VkAttachmentReference color_attachment = {
          .attachment = 0,
          .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      };
      VkSubpassDescription subpass = {
          .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
          .colorAttachmentCount = 1,
          .pColorAttachments = &color_attachment,
      };
      VkSubpassDependency dependency = {
          .srcSubpass = VK_SUBPASS_EXTERNAL,
          .dstSubpass = 0,
          .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          .srcAccessMask = 0,
          .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      };
      VkRenderPassCreateInfo info = {
          .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
          .attachmentCount = 1,
          .pAttachments = &attachment,
          .subpassCount = 1,
          .pSubpasses = &subpass,
          .dependencyCount = 1,
          .pDependencies = &dependency,
      };
      vuloxr::vk::CheckVkResult(
          vkCreateRenderPass(device, &info, nullptr, &renderPass));
    }

    {
      ImGui_ImplVulkan_InitInfo init_info = {
          .Instance = instance,
          .PhysicalDevice = physicalDevice,
          .Device = device,
          .QueueFamily = queueFamily,
          .Queue = queue,
          .DescriptorPool = descriptorPool,
          .RenderPass = renderPass,
          .MinImageCount = 2,
          .ImageCount = imageCount,
          .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
          // .PipelineCache = this->pipelineCache,
          .Subpass = 0,
          .Allocator = nullptr,
          .CheckVkResultFn = [](VkResult r) { vuloxr::vk::CheckVkResult(r); },
      };
      // init_info.ApiVersion = VK_API_VERSION_1_3;              // Pass in your
      // value of VkApplicationInfo::apiVersion, otherwise will default to
      // header version.
      ImGui_ImplVulkan_Init(&init_info);
    }
  }

  ~ImGuiVulkanResource() {
    vkDestroyRenderPass(this->device, this->renderPass, nullptr);
    vkDestroyDescriptorPool(this->device, this->descriptorPool, nullptr);
  }

  void render(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer,
              VkExtent2D extent, const ImVec4 &clear_color,
              VkSemaphore acquireSemaphore, VkSemaphore submitSemaphore,
              VkFence submitFence, ImDrawData *draw_data) {

    {
      VkCommandBufferBeginInfo info = {
          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
          .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      };
      vuloxr::vk::CheckVkResult(vkBeginCommandBuffer(commandBuffer, &info));
    }

    {
      VkClearValue clearValue = {
          .color =
              {
                  .float32 =
                      {
                          clear_color.x * clear_color.w,
                          clear_color.y * clear_color.w,
                          clear_color.z * clear_color.w,
                          clear_color.w,
                      },
              },
      };
      VkRenderPassBeginInfo info = {
          .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
          .renderPass = this->renderPass,
          .framebuffer = framebuffer,
          .renderArea = {.extent = extent},
          .clearValueCount = 1,
          .pClearValues = &clearValue,
      };
      vkCmdBeginRenderPass(commandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    }

    // Record dear imgui primitives into command buffer
    ImGui_ImplVulkan_RenderDrawData(draw_data, commandBuffer);

    // Submit command buffer
    vkCmdEndRenderPass(commandBuffer);
    vuloxr::vk::CheckVkResult(vkEndCommandBuffer(commandBuffer));
  }
};

void main_loop(const std::function<bool()> &runLoop,
               const vuloxr::vk::Instance &instance,
               vuloxr::vk::Swapchain &swapchain,
               const vuloxr::vk::PhysicalDevice &physicalDevice,
               const vuloxr::vk::Device &device, void *window) {

  Scene scene;
  scene.loadGltf(gltf);

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  // ImGui::StyleColorsLight();

  // Setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForVulkan((GLFWwindow *)window, true);

  // Create Framebuffers
  // auto [w, h] = glfw.framebufferSize();
  ImGuiVulkanResource imvulkan(instance, physicalDevice, device.queueFamily,
                               device, swapchain.createInfo.imageFormat,
                               swapchain.images.size());

  // Our state
  bool show_demo_window = true;
  bool show_another_window = false;
  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  vuloxr::vk::SwapchainNoDepthFramebufferList backbuffers(
      device, imvulkan.renderPass, swapchain.createInfo.imageExtent,
      swapchain.createInfo.imageFormat, swapchain.images);
  vuloxr::vk::FlightManager flightManager(device, swapchain.images.size());
  vuloxr::vk::CommandBufferPool pool(device, physicalDevice.graphicsFamilyIndex,
                                     swapchain.images.size());

  // Main loop
  while (runLoop()) {
    // if (glfw.isIconified()) {
    //   ImGui_ImplGlfw_Sleep(10);
    //   continue;
    // }

    // Start the Dear ImGui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // 1. Show the big demo window (Most of the sample code is in
    // ImGui::ShowDemoWindow()! You can browse its code to learn more about
    // Dear ImGui!).
    if (show_demo_window)
      ImGui::ShowDemoWindow(&show_demo_window);

    // 2. Show a simple window that we create ourselves. We use a Begin/End
    // pair to create a named window.
    {
      static float f = 0.0f;
      static int counter = 0;

      ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!"
                                     // and append into it.

      ImGui::Text("This is some useful text."); // Display some text (you can
                                                // use a format strings too)
      ImGui::Checkbox("Demo Window",
                      &show_demo_window); // Edit bools storing our window
                                          // open/close state
      ImGui::Checkbox("Another Window", &show_another_window);

      ImGui::SliderFloat("float", &f, 0.0f,
                         1.0f); // Edit 1 float using a slider from 0.0f to 1.0f
      ImGui::ColorEdit3(
          "clear color",
          (float *)&clear_color); // Edit 3 floats representing a color

      if (ImGui::Button("Button")) // Buttons return true when clicked (most
                                   // widgets return true when edited/activated)
        counter++;
      ImGui::SameLine();
      ImGui::Text("counter = %d", counter);

      ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                  1000.0f / io.Framerate, io.Framerate);
      ImGui::End();
    }

    // 3. Show another simple window.
    if (show_another_window) {
      ImGui::Begin(
          "Another Window",
          &show_another_window); // Pass a pointer to our bool variable (the
                                 // window will have a closing button that
                                 // will clear the bool when clicked)
      ImGui::Text("Hello from another window!");
      if (ImGui::Button("Close Me"))
        show_another_window = false;
      ImGui::End();
    }

    // Rendering
    ImGui::Render();
    ImDrawData *draw_data = ImGui::GetDrawData();
    const bool is_minimized =
        (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
    if (!is_minimized) {

      auto acquireSemaphore = flightManager.getOrCreateSemaphore();
      auto [res, acquired] = swapchain.acquireNextImage(acquireSemaphore);
      if (res == VK_SUCCESS) {
        auto backbuffer = &backbuffers[acquired.imageIndex];

        // All queue submissions get a fence that CPU will wait
        // on for synchronization purposes.
        auto [index, flight] = flightManager.sync(acquireSemaphore);
        auto cmd = pool.commandBuffers[index];
        vkResetCommandBuffer(cmd, 0);

        imvulkan.render(cmd, backbuffer->framebuffer,
                        swapchain.createInfo.imageExtent, clear_color,
                        flight.acquireSemaphore, flight.submitSemaphore,
                        flight.submitFence, draw_data);

        const VkPipelineStageFlags waitStage =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = static_cast<uint32_t>(
                acquireSemaphore != VK_NULL_HANDLE ? 1 : 0),
            .pWaitSemaphores = &acquireSemaphore,
            .pWaitDstStageMask = &waitStage,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmd,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &flight.submitSemaphore,
        };
        vuloxr::vk::CheckVkResult(
            vkQueueSubmit(device.queue, 1, &info, flight.submitFence));

        VkPresentInfoKHR present = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &flight.submitSemaphore,
            .swapchainCount = 1,
            .pSwapchains = &swapchain.swapchain,
            .pImageIndices = &acquired.imageIndex,
            .pResults = nullptr,
        };
        vuloxr::vk::CheckVkResult(vkQueuePresentKHR(device.queue, &present));

      } else if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR) {
        vuloxr::Logger::Error("[RESULT_ERROR_OUTDATED_SWAPCHAIN]");
        vkQueueWaitIdle(swapchain.presentQueue);
        flightManager.reuseSemaphore(acquireSemaphore);
        // TODO:
        // swapchain = {};
        // return true;

      } else {
        // error ?
        vuloxr::Logger::Error("Unrecoverable swapchain error.\n");
        vkQueueWaitIdle(swapchain.presentQueue);
        flightManager.reuseSemaphore(acquireSemaphore);
        break;
      }
    }
  }

  // Cleanup
  vuloxr::vk::CheckVkResult(vkDeviceWaitIdle(device));
  ImGui_ImplVulkan_Shutdown();

  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}
