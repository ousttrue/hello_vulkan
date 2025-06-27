/*
 * Vulkan Samples
 *
 * Copyright (C) 2015-2016 Valve Corporation
 * Copyright (C) 2015-2016 LunarG, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <iostream>
#include <string>
#include <sstream>
#include <vector>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#ifdef _WIN32
#pragma comment(linker, "/subsystem:console")
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#ifndef NOMINMAX
#define NOMINMAX /* Don't let Windows define min() or max() */
#endif
#define APP_NAME_STR_LEN 80
#elif defined(__ANDROID__)
// Include files for Android
#include <unistd.h>
#include <android/log.h>
#include "vulkan_wrapper.h" // Include Vulkan_wrapper and dynamically load symbols.
#elif defined(__IPHONE_OS_VERSION_MAX_ALLOWED) || defined(__MAC_OS_X_VERSION_MAX_ALLOWED)
#include <MoltenVK/mvk_vulkan.h>
#include <unistd.h>
#else
#include <unistd.h>
#include "vulkan/vk_sdk_platform.h"
#endif

#include <vulkan/vulkan.h>

/* Number of descriptor sets needs to be the same at alloc,       */
/* pipeline layout creation, and descriptor set layout creation   */
#define NUM_DESCRIPTOR_SETS 1

/* Number of samples needs to be the same at image creation,      */
/* renderpass creation and pipeline creation.                     */
#define NUM_SAMPLES VK_SAMPLE_COUNT_1_BIT

/* Number of viewports and number of scissors have to be the same */
/* at pipeline creation and in any call to set them dynamically   */
/* They also have to be the same as each other                    */
#define NUM_VIEWPORTS 1
#define NUM_SCISSORS NUM_VIEWPORTS

/* Amount of time, in nanoseconds, to wait for a command buffer to complete */
#define FENCE_TIMEOUT 100000000

#define GET_INSTANCE_PROC_ADDR(inst, entrypoint)                               \
    {                                                                          \
        info.fp##entrypoint =                                                  \
            (PFN_vk##entrypoint)vkGetInstanceProcAddr(inst, "vk" #entrypoint); \
        if (info.fp##entrypoint == NULL) {                                     \
            std::cout << "vkGetDeviceProcAddr failed to find vk" #entrypoint;  \
            exit(-1);                                                          \
        }                                                                      \
    }

#define GET_DEVICE_PROC_ADDR(dev, entrypoint)                                  \
    {                                                                          \
        info.fp##entrypoint =                                                  \
            (PFN_vk##entrypoint)vkGetDeviceProcAddr(dev, "vk" #entrypoint);    \
        if (info.fp##entrypoint == NULL) {                                     \
            std::cout << "vkGetDeviceProcAddr failed to find vk" #entrypoint;  \
            exit(-1);                                                          \
        }                                                                      \
    }

#if defined(NDEBUG) && defined(__GNUC__)
#define U_ASSERT_ONLY __attribute__((unused))
#else
#define U_ASSERT_ONLY
#endif

// std::string get_base_data_dir();
// std::string get_data_dir(std::string filename);

/*
 * structure to track all objects related to a texture.
 */
struct texture_object {
    VkSampler sampler;

    VkImage image;
    VkImageLayout imageLayout;

    bool needs_staging;
    VkBuffer buffer;
    VkDeviceSize buffer_size;

    VkDeviceMemory image_memory;
    VkDeviceMemory buffer_memory;
    VkImageView view;
    int32_t tex_width, tex_height;
};

/*
 * Keep each of our swap chain buffers' image, command buffer and view in one
 * spot
 */
typedef struct _swap_chain_buffers {
    VkImage image;
    VkImageView view;
} swap_chain_buffer;

/*
 * A layer can expose extensions, keep track of those
 * extensions here.
 */
typedef struct {
    VkLayerProperties properties;
    std::vector<VkExtensionProperties> instance_extensions;
    std::vector<VkExtensionProperties> device_extensions;
} layer_properties;

/*
 * Structure for tracking information used / created / modified
 * by utility functions.
 */
struct sample_info {
#ifdef _WIN32
#define APP_NAME_STR_LEN 80
    HINSTANCE connection;        // hInstance - Windows Instance
    char name[APP_NAME_STR_LEN]; // Name to put on the window/icon
    HWND window;                 // hWnd - window handle
#elif defined(VK_USE_PLATFORM_METAL_EXT)
    void *caMetalLayer;
#elif defined(__ANDROID__)
    PFN_vkCreateAndroidSurfaceKHR fpCreateAndroidSurfaceKHR;
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
    wl_display *display;
    wl_registry *registry;
    wl_compositor *compositor;
    wl_surface *window;
    wl_shell *shell;
    wl_shell_surface *shell_surface;
#else
    xcb_connection_t *connection;
    xcb_screen_t *screen;
    xcb_window_t window;
    xcb_intern_atom_reply_t *atom_wm_delete_window;
#endif // _WIN32
    VkSurfaceKHR surface;
    bool prepared;
    bool use_staging_buffer;
    bool save_images;

    std::vector<const char *> instance_layer_names;
    std::vector<const char *> instance_extension_names;
    std::vector<layer_properties> instance_layer_properties;
    std::vector<VkExtensionProperties> instance_extension_properties;
    VkInstance inst;

    std::vector<const char *> device_extension_names;
    std::vector<VkExtensionProperties> device_extension_properties;
    std::vector<VkPhysicalDevice> gpus;
    VkDevice device;
    VkQueue graphics_queue;
    VkQueue present_queue;
    uint32_t graphics_queue_family_index;
    uint32_t present_queue_family_index;
    VkPhysicalDeviceProperties gpu_props;
    std::vector<VkQueueFamilyProperties> queue_props;
    VkPhysicalDeviceMemoryProperties memory_properties;

    VkFramebuffer *framebuffers;
    int width, height;
    VkFormat format;

    uint32_t swapchainImageCount;
    VkSwapchainKHR swap_chain;
    std::vector<swap_chain_buffer> buffers;
    VkSemaphore imageAcquiredSemaphore;

    VkCommandPool cmd_pool;

    struct {
        VkFormat format;

        VkImage image;
        VkDeviceMemory mem;
        VkImageView view;
    } depth;

    std::vector<struct texture_object> textures;

    struct {
        VkBuffer buf;
        VkDeviceMemory mem;
        VkDescriptorBufferInfo buffer_info;
    } uniform_data;

    struct {
        VkDescriptorImageInfo image_info;
    } texture_data;

    struct {
        VkBuffer buf;
        VkDeviceMemory mem;
        VkDescriptorBufferInfo buffer_info;
    } vertex_buffer;
    VkVertexInputBindingDescription vi_binding;
    VkVertexInputAttributeDescription vi_attribs[2];

    glm::mat4 Projection;
    glm::mat4 View;
    glm::mat4 Model;
    glm::mat4 Clip;
    glm::mat4 MVP;

    VkCommandBuffer cmd; // Buffer for initialization commands
    VkPipelineLayout pipeline_layout;
    std::vector<VkDescriptorSetLayout> desc_layout;
    VkPipelineCache pipelineCache;
    VkRenderPass render_pass;
    VkPipeline pipeline;

    VkPipelineShaderStageCreateInfo shaderStages[2];

    VkDescriptorPool desc_pool;
    std::vector<VkDescriptorSet> desc_set;

    PFN_vkCreateDebugReportCallbackEXT dbgCreateDebugReportCallback;
    PFN_vkDestroyDebugReportCallbackEXT dbgDestroyDebugReportCallback;
    PFN_vkDebugReportMessageEXT dbgBreakCallback;
    std::vector<VkDebugReportCallbackEXT> debug_report_callbacks;

    uint32_t current_buffer;
    uint32_t queue_family_count;

    VkViewport viewport;
    VkRect2D scissor;
};
void process_command_line_args(struct sample_info &info, int argc,
                               char *argv[]);
bool memory_type_from_properties(struct sample_info &info, uint32_t typeBits,
                                 VkFlags requirements_mask,
                                 uint32_t *typeIndex);

void set_image_layout(struct sample_info &demo, VkImage image,
                      VkImageAspectFlags aspectMask,
                      VkImageLayout old_image_layout,
                      VkImageLayout new_image_layout,
                      VkPipelineStageFlags src_stages,
                      VkPipelineStageFlags dest_stages);

bool read_ppm(char const *const filename, int &width, int &height,
              uint64_t rowPitch, unsigned char *dataPtr);
void write_ppm(struct sample_info &info, const char *basename);
void extract_version(uint32_t version, uint32_t &major, uint32_t &minor,
                     uint32_t &patch);
bool GLSLtoSPV(const VkShaderStageFlagBits shader_type, const char *pshader,
               std::vector<unsigned int> &spirv);
void init_glslang();
void finalize_glslang();
void wait_seconds(int seconds);
void print_UUID(uint8_t *pipelineCacheUUID);
std::string get_file_directory();

typedef unsigned long long timestamp_t;
timestamp_t get_milliseconds();

// Main entry point of samples
int sample_main(int argc, char *argv[]);

#ifdef __ANDROID__
// Android specific definitions & helpers.
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "VK-SAMPLE", __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "VK-SAMPLE", __VA_ARGS__))
// Replace printf to logcat output.
#define printf(...) __android_log_print(ANDROID_LOG_DEBUG, "VK-SAMPLE", __VA_ARGS__);

bool Android_process_command();
ANativeWindow* AndroidGetApplicationWindow();
FILE* AndroidFopen(const char* fname, const char* mode);
void AndroidGetWindowSize(int32_t *width, int32_t *height);
bool AndroidLoadFile(const char* filePath, std::string *data);

#ifndef VK_API_VERSION_1_0
// On Android, NDK would include slightly older version of headers that is missing the definition.
#define VK_API_VERSION_1_0 VK_API_VERSION
#endif

#endif

// Make sure functions start with init, execute, or destroy to assist codegen

VkResult init_global_extension_properties(layer_properties &layer_props);

VkResult init_global_layer_properties(sample_info &info);

VkResult init_device_extension_properties(struct sample_info &info,
                                          layer_properties &layer_props);

void init_instance_extension_names(struct sample_info &info);
VkResult init_instance(struct sample_info &info,
                       char const *const app_short_name, const void *pNext);
void init_device_extension_names(struct sample_info &info);
VkResult init_device(struct sample_info &info);
VkResult init_enumerate_device(struct sample_info &info,
                               uint32_t gpu_count = 1);
VkBool32 demo_check_layers(const std::vector<layer_properties> &layer_props,
                           const std::vector<const char *> &layer_names);
void init_connection(struct sample_info &info);
void init_window(struct sample_info &info);
void init_queue_family_index(struct sample_info &info);
void init_presentable_image(struct sample_info &info);
void execute_queue_cmdbuf(struct sample_info &info,
                          const VkCommandBuffer *cmd_bufs, VkFence &fence);
void execute_pre_present_barrier(struct sample_info &info);
void execute_present_image(struct sample_info &info);
void init_swapchain_extension(struct sample_info &info);
void init_command_pool(struct sample_info &info);
void init_command_buffer(struct sample_info &info);
void execute_begin_command_buffer(struct sample_info &info);
void execute_end_command_buffer(struct sample_info &info);
void execute_queue_command_buffer(struct sample_info &info);
void init_device_queue(struct sample_info &info);
void init_swap_chain(
    struct sample_info &info,
    VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                   VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
void init_depth_buffer(struct sample_info &info);
void init_uniform_buffer(struct sample_info &info);
void init_descriptor_and_pipeline_layouts(struct sample_info &info, bool use_texture,
                                          VkDescriptorSetLayoutCreateFlags descSetLayoutCreateFlags = 0);
void init_renderpass(struct sample_info &info, bool include_depth, bool clear = true,
                     VkImageLayout finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                     VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED);
void init_vertex_buffer(struct sample_info &info, const void *vertexData,
                        uint32_t dataSize, uint32_t dataStride,
                        bool use_texture);
void init_framebuffers(struct sample_info &info, bool include_depth);
void init_descriptor_pool(struct sample_info &info, bool use_texture);
void init_descriptor_set(struct sample_info &info, bool use_texture);
void init_shaders(struct sample_info &info, const VkShaderModuleCreateInfo *vertShaderCI,
                  const VkShaderModuleCreateInfo *fragShaderCI);
void init_pipeline_cache(struct sample_info &info);
void init_pipeline(struct sample_info &info, VkBool32 include_depth,
                   VkBool32 include_vi = true);
void init_sampler(struct sample_info &info, VkSampler &sampler);
void init_image(struct sample_info &info, texture_object &texObj,
                const char *textureName, VkImageUsageFlags extraUsages = 0,
                VkFormatFeatureFlags extraFeatures = 0);
void init_texture(struct sample_info &info, const char *textureName = nullptr,
                  VkImageUsageFlags extraUsages = 0,
                  VkFormatFeatureFlags extraFeatures = 0);
void init_viewports(struct sample_info &info);
void init_scissors(struct sample_info &info);
void init_fence(struct sample_info &info, VkFence &fence);
void init_submit_info(struct sample_info &info, VkSubmitInfo &submit_info,
                      VkPipelineStageFlags &pipe_stage_flags);
void init_present_info(struct sample_info &info, VkPresentInfoKHR &present);
void init_clear_color_and_depth(struct sample_info &info,
                                VkClearValue *clear_values);
void init_render_pass_begin_info(struct sample_info &info,
                                 VkRenderPassBeginInfo &rp_begin);
void init_window_size(struct sample_info &info, int32_t default_width,
                      int32_t default_height);

VkResult init_debug_report_callback(struct sample_info &info,
                                    PFN_vkDebugReportCallbackEXT dbgFunc);
void destroy_debug_report_callback(struct sample_info &info);
void destroy_pipeline(struct sample_info &info);
void destroy_pipeline_cache(struct sample_info &info);
void destroy_descriptor_pool(struct sample_info &info);
void destroy_vertex_buffer(struct sample_info &info);
void destroy_textures(struct sample_info &info);
void destroy_framebuffers(struct sample_info &info);
void destroy_shaders(struct sample_info &info);
void destroy_renderpass(struct sample_info &info);
void destroy_descriptor_and_pipeline_layouts(struct sample_info &info);
void destroy_uniform_buffer(struct sample_info &info);
void destroy_depth_buffer(struct sample_info &info);
void destroy_swap_chain(struct sample_info &info);
void destroy_command_buffer(struct sample_info &info);
void destroy_command_pool(struct sample_info &info);
void destroy_device(struct sample_info &info);
void destroy_instance(struct sample_info &info);
void destroy_window(struct sample_info &info);


