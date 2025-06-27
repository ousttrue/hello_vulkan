#define WIN32

// https://github.com/LunarG/VulkanSamples/blob/master/API-Samples/15-draw_cube/15-draw_cube.cpp
#include "../main_loop.h"
#include "cube_data.h"
#include <assert.h>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string.h>
#include <thread>
#include <vuloxr/vk.h>
#include <vuloxr/vk/shaderc.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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
  {                                                                            \
    info.fp##entrypoint =                                                      \
        (PFN_vk##entrypoint)vkGetInstanceProcAddr(inst, "vk" #entrypoint);     \
    if (info.fp##entrypoint == NULL) {                                         \
      std::cout << "vkGetDeviceProcAddr failed to find vk" #entrypoint;        \
      exit(-1);                                                                \
    }                                                                          \
  }

#define GET_DEVICE_PROC_ADDR(dev, entrypoint)                                  \
  {                                                                            \
    info.fp##entrypoint =                                                      \
        (PFN_vk##entrypoint)vkGetDeviceProcAddr(dev, "vk" #entrypoint);        \
    if (info.fp##entrypoint == NULL) {                                         \
      std::cout << "vkGetDeviceProcAddr failed to find vk" #entrypoint;        \
      exit(-1);                                                                \
    }                                                                          \
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

#ifdef __ANDROID__
// Android specific definitions & helpers.
#define LOGI(...)                                                              \
  ((void)__android_log_print(ANDROID_LOG_INFO, "VK-SAMPLE", __VA_ARGS__))
#define LOGE(...)                                                              \
  ((void)__android_log_print(ANDROID_LOG_ERROR, "VK-SAMPLE", __VA_ARGS__))
// Replace printf to logcat output.
#define printf(...)                                                            \
  __android_log_print(ANDROID_LOG_DEBUG, "VK-SAMPLE", __VA_ARGS__);

bool Android_process_command();
ANativeWindow *AndroidGetApplicationWindow();
FILE *AndroidFopen(const char *fname, const char *mode);
void AndroidGetWindowSize(int32_t *width, int32_t *height);
bool AndroidLoadFile(const char *filePath, std::string *data);

#ifndef VK_API_VERSION_1_0
// On Android, NDK would include slightly older version of headers that is
// missing the definition.
#define VK_API_VERSION_1_0 VK_API_VERSION
#endif

#endif

// Make sure functions start with init, execute, or destroy to assist codegen

VkResult init_global_extension_properties(layer_properties &layer_props);

VkResult init_device_extension_properties(struct sample_info &info,
                                          layer_properties &layer_props);

VkBool32 demo_check_layers(const std::vector<layer_properties> &layer_props,
                           const std::vector<const char *> &layer_names);
void init_queue_family_index(struct sample_info &info);
void init_presentable_image(struct sample_info &info);
void execute_queue_cmdbuf(struct sample_info &info,
                          const VkCommandBuffer *cmd_bufs, VkFence &fence);
void execute_pre_present_barrier(struct sample_info &info);
void execute_present_image(struct sample_info &info);

void init_command_pool(struct sample_info &info);
void init_command_buffer(struct sample_info &info);
void execute_begin_command_buffer(struct sample_info &info);
void execute_end_command_buffer(struct sample_info &info);
void execute_queue_command_buffer(struct sample_info &info);
void init_device_queue(struct sample_info &info);
void init_depth_buffer(struct sample_info &info);
void init_uniform_buffer(struct sample_info &info);
void init_descriptor_and_pipeline_layouts(
    struct sample_info &info, bool use_texture,
    VkDescriptorSetLayoutCreateFlags descSetLayoutCreateFlags = 0);
void init_renderpass(
    struct sample_info &info, bool include_depth, bool clear = true,
    VkImageLayout finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED);
void init_vertex_buffer(struct sample_info &info, const void *vertexData,
                        uint32_t dataSize, uint32_t dataStride,
                        bool use_texture);
void init_framebuffers(struct sample_info &info, bool include_depth);
void init_descriptor_pool(struct sample_info &info, bool use_texture);
void init_descriptor_set(struct sample_info &info, bool use_texture);
void init_shaders(struct sample_info &info,
                  const VkShaderModuleCreateInfo *vertShaderCI,
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

// For timestamp code (get_milliseconds)
// #ifdef WIN32
// #include <Windows.h>
// #else
// #include <sys/time.h>
// #endif

using namespace std;

void extract_version(uint32_t version, uint32_t &major, uint32_t &minor,
                     uint32_t &patch) {
  major = version >> 22;
  minor = (version >> 12) & 0x3ff;
  patch = version & 0xfff;
}

string get_file_name(const string &s) {
  char sep = '/';

#ifdef _WIN32
  sep = '\\';
#endif

  // cout << "in get_file_name\n";
  size_t i = s.rfind(sep, s.length());
  if (i != string::npos) {
    return (s.substr(i + 1, s.length() - i));
  }

  return ("");
}

// #if !defined(VK_USE_PLATFORM_METAL_EXT)
// // iOS & macOS: get_base_data_dir() implemented externally to allow access to
// Objective-C components std::string get_base_data_dir() { #ifdef __ANDROID__
//     return "";
// #else
//     return std::string(VULKAN_SAMPLES_BASE_DIR) + "/API-Samples/data/";
// #endif
// }
// #endif

// std::string get_data_dir(std::string filename) {
//     std::string basedir = get_base_data_dir();
//     // get the base filename
//     std::string fname = get_file_name(filename);
//
//     // get the prefix of the base filename, i.e. the part before the dash
//     stringstream stream(fname);
//     std::string prefix;
//     getline(stream, prefix, '-');
//     std::string ddir = basedir + prefix;
//     return ddir;
// }

bool memory_type_from_properties(struct sample_info &info, uint32_t typeBits,
                                 VkFlags requirements_mask,
                                 uint32_t *typeIndex) {
  // Search memtypes to find first index with those properties
  for (uint32_t i = 0; i < info.memory_properties.memoryTypeCount; i++) {
    if ((typeBits & 1) == 1) {
      // Type is available, does it match user properties?
      if ((info.memory_properties.memoryTypes[i].propertyFlags &
           requirements_mask) == requirements_mask) {
        *typeIndex = i;
        return true;
      }
    }
    typeBits >>= 1;
  }
  // No memory types matched, return failure
  return false;
}

void set_image_layout(struct sample_info &info, VkImage image,
                      VkImageAspectFlags aspectMask,
                      VkImageLayout old_image_layout,
                      VkImageLayout new_image_layout,
                      VkPipelineStageFlags src_stages,
                      VkPipelineStageFlags dest_stages) {
  /* DEPENDS on info.cmd and info.queue initialized */

  assert(info.cmd != VK_NULL_HANDLE);
  assert(info.graphics_queue != VK_NULL_HANDLE);

  VkImageMemoryBarrier image_memory_barrier = {};
  image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  image_memory_barrier.pNext = NULL;
  image_memory_barrier.srcAccessMask = 0;
  image_memory_barrier.dstAccessMask = 0;
  image_memory_barrier.oldLayout = old_image_layout;
  image_memory_barrier.newLayout = new_image_layout;
  image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  image_memory_barrier.image = image;
  image_memory_barrier.subresourceRange.aspectMask = aspectMask;
  image_memory_barrier.subresourceRange.baseMipLevel = 0;
  image_memory_barrier.subresourceRange.levelCount = 1;
  image_memory_barrier.subresourceRange.baseArrayLayer = 0;
  image_memory_barrier.subresourceRange.layerCount = 1;

  switch (old_image_layout) {
  case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
    image_memory_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    break;

  case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
    image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    break;

  case VK_IMAGE_LAYOUT_PREINITIALIZED:
    image_memory_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    break;

  default:
    break;
  }

  switch (new_image_layout) {
  case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
    image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    break;

  case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
    image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    break;

  case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
    image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    break;

  case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
    image_memory_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    break;

  case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
    image_memory_barrier.dstAccessMask =
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    break;

  default:
    break;
  }

  vkCmdPipelineBarrier(info.cmd, src_stages, dest_stages, 0, 0, NULL, 0, NULL,
                       1, &image_memory_barrier);
}

bool read_ppm(char const *const filename, int &width, int &height,
              uint64_t rowPitch, unsigned char *dataPtr) {
  // PPM format expected from http://netpbm.sourceforge.net/doc/ppm.html
  //  1. magic number
  //  2. whitespace
  //  3. width
  //  4. whitespace
  //  5. height
  //  6. whitespace
  //  7. max color value
  //  8. whitespace
  //  7. data

  // Comments are not supported, but are detected and we kick out
  // Only 8 bits per channel is supported
  // If dataPtr is nullptr, only width and height are returned

  // Read in values from the PPM file as characters to check for comments
  char magicStr[3] = {}, heightStr[6] = {}, widthStr[6] = {}, formatStr[6] = {};

#ifndef __ANDROID__
  FILE *fPtr = fopen(filename, "rb");
#else
  FILE *fPtr = AndroidFopen(filename, "rb");
#endif
  if (!fPtr) {
    printf("Bad filename in read_ppm: %s\n", filename);
    return false;
  }

  // Read the four values from file, accounting with any and all whitepace
  int U_ASSERT_ONLY count =
      fscanf(fPtr, "%s %s %s %s ", magicStr, widthStr, heightStr, formatStr);
  assert(count == 4);

  // Kick out if comments present
  if (magicStr[0] == '#' || widthStr[0] == '#' || heightStr[0] == '#' ||
      formatStr[0] == '#') {
    printf("Unhandled comment in PPM file\n");
    return false;
  }

  // Only one magic value is valid
  if (strncmp(magicStr, "P6", sizeof(magicStr))) {
    printf("Unhandled PPM magic number: %s\n", magicStr);
    return false;
  }

  width = atoi(widthStr);
  height = atoi(heightStr);

  // Ensure we got something sane for width/height
  static const int saneDimension = 32768; //??
  if (width <= 0 || width > saneDimension) {
    printf("Width seems wrong.  Update read_ppm if not: %u\n", width);
    return false;
  }
  if (height <= 0 || height > saneDimension) {
    printf("Height seems wrong.  Update read_ppm if not: %u\n", height);
    return false;
  }

  if (dataPtr == nullptr) {
    // If no destination pointer, caller only wanted dimensions
    return true;
  }

  // Now read the data
  for (int y = 0; y < height; y++) {
    unsigned char *rowPtr = dataPtr;
    for (int x = 0; x < width; x++) {
      count = fread(rowPtr, 3, 1, fPtr);
      assert(count == 1);
      rowPtr[3] = 255; /* Alpha of 1 */
      rowPtr += 4;
    }
    dataPtr += rowPitch;
  }
  fclose(fPtr);

  return true;
}

#if (defined(VK_USE_PLATFORM_IOS_MVK) || defined(VK_USE_PLATFORM_MACOS_MVK))

void init_glslang() {}

void finalize_glslang() {}

bool GLSLtoSPV(const VkShaderStageFlagBits shader_type, const char *pshader,
               std::vector<unsigned int> &spirv) {
  MVKGLSLConversionShaderStage shaderStage;
  switch (shader_type) {
  case VK_SHADER_STAGE_VERTEX_BIT:
    shaderStage = kMVKGLSLConversionShaderStageVertex;
    break;
  case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
    shaderStage = kMVKGLSLConversionShaderStageTessControl;
    break;
  case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
    shaderStage = kMVKGLSLConversionShaderStageTessEval;
    break;
  case VK_SHADER_STAGE_GEOMETRY_BIT:
    shaderStage = kMVKGLSLConversionShaderStageGeometry;
    break;
  case VK_SHADER_STAGE_FRAGMENT_BIT:
    shaderStage = kMVKGLSLConversionShaderStageFragment;
    break;
  case VK_SHADER_STAGE_COMPUTE_BIT:
    shaderStage = kMVKGLSLConversionShaderStageCompute;
    break;
  default:
    shaderStage = kMVKGLSLConversionShaderStageAuto;
    break;
  }

  mvk::GLSLToSPIRVConverter glslConverter;
  glslConverter.setGLSL(pshader);
  bool wasConverted = glslConverter.convert(shaderStage, false, false);
  if (wasConverted) {
    spirv = glslConverter.getSPIRV();
  }
  return wasConverted;
}

#endif // IOS or macOS

void wait_seconds(int seconds) {
  std::this_thread::sleep_for(std::chrono::seconds(seconds));
  // #ifdef WIN32
  //   Sleep(seconds * 1000);
  // #elif defined(__ANDROID__)
  //   sleep(seconds);
  // #else
  //   sleep(seconds);
  // #endif
}

// timestamp_t get_milliseconds() {
// #ifdef WIN32
//   LARGE_INTEGER frequency;
//   BOOL useQPC = QueryPerformanceFrequency(&frequency);
//   if (useQPC) {
//     LARGE_INTEGER now;
//     QueryPerformanceCounter(&now);
//     return (1000LL * now.QuadPart) / frequency.QuadPart;
//   } else {
//     return GetTickCount();
//   }
// #else
//   struct timeval now;
//   gettimeofday(&now, NULL);
//   return (now.tv_usec / 1000) + (timestamp_t)now.tv_sec;
// #endif
// }

void print_UUID(uint8_t *pipelineCacheUUID) {
  for (int j = 0; j < VK_UUID_SIZE; ++j) {
    std::cout << std::setw(2) << (uint32_t)pipelineCacheUUID[j];
    if (j == 3 || j == 5 || j == 7 || j == 9) {
      std::cout << '-';
    }
  }
}
static bool optionMatch(const char *option, char *optionLine) {
  if (strncmp(option, optionLine, strlen(option)) == 0)
    return true;
  else
    return false;
}

void write_ppm(struct sample_info &info, const char *basename) {
  string filename;
  int x, y;
  VkResult res;

  VkImageCreateInfo image_create_info = {};
  image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_create_info.pNext = NULL;
  image_create_info.imageType = VK_IMAGE_TYPE_2D;
  image_create_info.format = info.format;
  image_create_info.extent.width = info.width;
  image_create_info.extent.height = info.height;
  image_create_info.extent.depth = 1;
  image_create_info.mipLevels = 1;
  image_create_info.arrayLayers = 1;
  image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_create_info.tiling = VK_IMAGE_TILING_LINEAR;
  image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_create_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  image_create_info.queueFamilyIndexCount = 0;
  image_create_info.pQueueFamilyIndices = NULL;
  image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_create_info.flags = 0;

  VkMemoryAllocateInfo mem_alloc = {};
  mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  mem_alloc.pNext = NULL;
  mem_alloc.allocationSize = 0;
  mem_alloc.memoryTypeIndex = 0;

  VkImage mappableImage;
  VkDeviceMemory mappableMemory;

  /* Create a mappable image */
  res = vkCreateImage(info.device, &image_create_info, NULL, &mappableImage);
  assert(res == VK_SUCCESS);

  VkMemoryRequirements mem_reqs;
  vkGetImageMemoryRequirements(info.device, mappableImage, &mem_reqs);

  mem_alloc.allocationSize = mem_reqs.size;

  /* Find the memory type that is host mappable */
  bool U_ASSERT_ONLY pass =
      memory_type_from_properties(info, mem_reqs.memoryTypeBits,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                  &mem_alloc.memoryTypeIndex);
  assert(pass && "No mappable, coherent memory");

  /* allocate memory */
  res = vkAllocateMemory(info.device, &mem_alloc, NULL, &(mappableMemory));
  assert(res == VK_SUCCESS);

  /* bind memory */
  res = vkBindImageMemory(info.device, mappableImage, mappableMemory, 0);
  assert(res == VK_SUCCESS);

  VkCommandBufferBeginInfo cmd_buf_info = {};
  cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  cmd_buf_info.pNext = NULL;
  cmd_buf_info.flags = 0;
  cmd_buf_info.pInheritanceInfo = NULL;

  res = vkBeginCommandBuffer(info.cmd, &cmd_buf_info);
  set_image_layout(
      info, mappableImage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT);

  set_image_layout(
      info, info.buffers[info.current_buffer].image, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

  VkImageCopy copy_region;
  copy_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy_region.srcSubresource.mipLevel = 0;
  copy_region.srcSubresource.baseArrayLayer = 0;
  copy_region.srcSubresource.layerCount = 1;
  copy_region.srcOffset.x = 0;
  copy_region.srcOffset.y = 0;
  copy_region.srcOffset.z = 0;
  copy_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy_region.dstSubresource.mipLevel = 0;
  copy_region.dstSubresource.baseArrayLayer = 0;
  copy_region.dstSubresource.layerCount = 1;
  copy_region.dstOffset.x = 0;
  copy_region.dstOffset.y = 0;
  copy_region.dstOffset.z = 0;
  copy_region.extent.width = info.width;
  copy_region.extent.height = info.height;
  copy_region.extent.depth = 1;

  /* Put the copy command into the command buffer */
  vkCmdCopyImage(info.cmd, info.buffers[info.current_buffer].image,
                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mappableImage,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

  set_image_layout(info, mappableImage, VK_IMAGE_ASPECT_COLOR_BIT,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
                   VK_PIPELINE_STAGE_HOST_BIT);

  res = vkEndCommandBuffer(info.cmd);
  assert(res == VK_SUCCESS);
  const VkCommandBuffer cmd_bufs[] = {info.cmd};
  VkFenceCreateInfo fenceInfo;
  VkFence cmdFence;
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.pNext = NULL;
  fenceInfo.flags = 0;
  vkCreateFence(info.device, &fenceInfo, NULL, &cmdFence);

  VkSubmitInfo submit_info[1] = {};
  submit_info[0].pNext = NULL;
  submit_info[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info[0].waitSemaphoreCount = 0;
  submit_info[0].pWaitSemaphores = NULL;
  submit_info[0].pWaitDstStageMask = NULL;
  submit_info[0].commandBufferCount = 1;
  submit_info[0].pCommandBuffers = cmd_bufs;
  submit_info[0].signalSemaphoreCount = 0;
  submit_info[0].pSignalSemaphores = NULL;

  /* Queue the command buffer for execution */
  res = vkQueueSubmit(info.graphics_queue, 1, submit_info, cmdFence);
  assert(res == VK_SUCCESS);

  /* Make sure command buffer is finished before mapping */
  do {
    res = vkWaitForFences(info.device, 1, &cmdFence, VK_TRUE, FENCE_TIMEOUT);
  } while (res == VK_TIMEOUT);
  assert(res == VK_SUCCESS);

  vkDestroyFence(info.device, cmdFence, NULL);

  filename.append(basename);
  filename.append(".ppm");

  VkImageSubresource subres = {};
  subres.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  subres.mipLevel = 0;
  subres.arrayLayer = 0;
  VkSubresourceLayout sr_layout;
  vkGetImageSubresourceLayout(info.device, mappableImage, &subres, &sr_layout);

  char *ptr;
  res = vkMapMemory(info.device, mappableMemory, 0, mem_reqs.size, 0,
                    (void **)&ptr);
  assert(res == VK_SUCCESS);

  ptr += sr_layout.offset;
  std::ofstream file(filename.c_str(), ios::binary);

  file << "P6\n";
  file << info.width << " ";
  file << info.height << "\n";
  file << 255 << "\n";

  for (y = 0; y < info.height; y++) {
    const int *row = (const int *)ptr;
    int swapped;

    if (info.format == VK_FORMAT_B8G8R8A8_UNORM ||
        info.format == VK_FORMAT_B8G8R8A8_SRGB) {
      for (x = 0; x < info.width; x++) {
        swapped = (*row & 0xff00ff00) | (*row & 0x000000ff) << 16 |
                  (*row & 0x00ff0000) >> 16;
        file.write((char *)&swapped, 3);
        row++;
      }
    } else if (info.format == VK_FORMAT_R8G8B8A8_UNORM) {
      for (x = 0; x < info.width; x++) {
        file.write((char *)row, 3);
        row++;
      }
    } else {
      printf("Unrecognized image format - will not write image files");
      break;
    }

    ptr += sr_layout.rowPitch;
  }

  file.close();
  vkUnmapMemory(info.device, mappableMemory);
  vkDestroyImage(info.device, mappableImage, NULL);
  vkFreeMemory(info.device, mappableMemory, NULL);
}

std::string get_file_directory() {
#ifndef __ANDROID__
  return "";
#else
  assert(Android_application != nullptr);
  return Android_application->activity->externalDataPath;
#endif
}

#ifdef __ANDROID__
//
// Android specific helper functions.
//

// Helpder class to forward the cout/cerr output to logcat derived from:
// http://stackoverflow.com/questions/8870174/is-stdcout-usable-in-android-ndk
class AndroidBuffer : public std::streambuf {
public:
  AndroidBuffer(android_LogPriority priority) {
    priority_ = priority;
    this->setp(buffer_, buffer_ + kBufferSize - 1);
  }

private:
  static const int32_t kBufferSize = 128;
  int32_t overflow(int32_t c) {
    if (c == traits_type::eof()) {
      *this->pptr() = traits_type::to_char_type(c);
      this->sbumpc();
    }
    return this->sync() ? traits_type::eof() : traits_type::not_eof(c);
  }

  int32_t sync() {
    int32_t rc = 0;
    if (this->pbase() != this->pptr()) {
      char writebuf[kBufferSize + 1];
      memcpy(writebuf, this->pbase(), this->pptr() - this->pbase());
      writebuf[this->pptr() - this->pbase()] = '\0';

      rc = __android_log_write(priority_, "std", writebuf) > 0;
      this->setp(buffer_, buffer_ + kBufferSize - 1);
    }
    return rc;
  }

  android_LogPriority priority_ = ANDROID_LOG_INFO;
  char buffer_[kBufferSize];
};

void Android_handle_cmd(android_app *app, int32_t cmd) {
  switch (cmd) {
  case APP_CMD_INIT_WINDOW:
    // The window is being shown, get it ready.
    sample_main(0, nullptr);
    LOGI("\n");
    LOGI("=================================================");
    LOGI("          The sample ran successfully!!");
    LOGI("=================================================");
    LOGI("\n");
    break;
  case APP_CMD_TERM_WINDOW:
    // The window is being hidden or closed, clean it up.
    break;
  default:
    LOGI("event not handled: %d", cmd);
  }
}

bool Android_process_command() {
  assert(Android_application != nullptr);
  int events;
  android_poll_source *source;
  // Poll all pending events.
  if (ALooper_pollAll(0, NULL, &events, (void **)&source) >= 0) {
    // Process each polled events
    if (source != NULL)
      source->process(Android_application, source);
  }
  return Android_application->destroyRequested;
}

void android_main(struct android_app *app) {
  // Set static variables.
  Android_application = app;
  // Set the callback to process system events
  app->onAppCmd = Android_handle_cmd;

  // Forward cout/cerr to logcat.
  std::cout.rdbuf(new AndroidBuffer(ANDROID_LOG_INFO));
  std::cerr.rdbuf(new AndroidBuffer(ANDROID_LOG_ERROR));

  // Main loop
  do {
    Android_process_command();
  } // Check if system requested to quit the application
  while (app->destroyRequested == 0);

  return;
}

ANativeWindow *AndroidGetApplicationWindow() {
  assert(Android_application != nullptr);
  return Android_application->window;
}

bool AndroidFillShaderMap(
    const char *path,
    std::unordered_map<std::string, std::string> *map_shaders) {
  assert(Android_application != nullptr);
  auto directory =
      AAssetManager_openDir(Android_application->activity->assetManager, path);

  const char *name = nullptr;
  while (1) {
    name = AAssetDir_getNextFileName(directory);
    if (name == nullptr) {
      break;
    }

    std::string file_name = name;
    if (file_name.find(".frag") != std::string::npos ||
        file_name.find(".vert") != std::string::npos) {
      // Add path to the filename.
      file_name = std::string(path) + "/" + file_name;
      std::string shader;
      if (!AndroidLoadFile(file_name.c_str(), &shader)) {
        continue;
      }
      // Remove \n to make the lookup more robust.
      while (1) {
        auto ret_pos = shader.find("\n");
        if (ret_pos == std::string::npos) {
          break;
        }
        shader.erase(ret_pos, 1);
      }

      auto pos = file_name.find_last_of(".");
      if (pos == std::string::npos) {
        // Invalid file nmae.
        continue;
      }
      // Generate filename for SPIRV binary.
      std::string spirv_name = file_name.replace(pos, 1, "-") + ".spirv";
      // Store the SPIRV file name with GLSL contents as a key.
      // The file contents can be long, but as we are using unordered map, it
      // wouldn't take much storage space. Put the file into the map.
      (*map_shaders)[shader] = spirv_name;
    }
  };

  AAssetDir_close(directory);
  return true;
}

bool AndroidLoadFile(const char *filePath, std::string *data) {
  assert(Android_application != nullptr);
  AAsset *file = AAssetManager_open(Android_application->activity->assetManager,
                                    filePath, AASSET_MODE_BUFFER);
  size_t fileLength = AAsset_getLength(file);
  LOGI("Loaded file:%s size:%zu", filePath, fileLength);
  if (fileLength == 0) {
    return false;
  }
  data->resize(fileLength);
  AAsset_read(file, &(*data)[0], fileLength);
  return true;
}

void AndroidGetWindowSize(int32_t *width, int32_t *height) {
  // On Android, retrieve the window size from the native window.
  assert(Android_application != nullptr);
  *width = ANativeWindow_getWidth(Android_application->window);
  *height = ANativeWindow_getHeight(Android_application->window);
}

// Android fopen stub described at
// http://www.50ply.com/blog/2013/01/19/loading-compressed-android-assets-with-file-pointer/#comment-1850768990
static int android_read(void *cookie, char *buf, int size) {
  return AAsset_read((AAsset *)cookie, buf, size);
}

static int android_write(void *cookie, const char *buf, int size) {
  return EACCES; // can't provide write access to the apk
}

static fpos_t android_seek(void *cookie, fpos_t offset, int whence) {
  return AAsset_seek((AAsset *)cookie, offset, whence);
}

static int android_close(void *cookie) {
  AAsset_close((AAsset *)cookie);
  return 0;
}

FILE *AndroidFopen(const char *fname, const char *mode) {
  if (mode[0] == 'w') {
    return NULL;
  }

  assert(Android_application != nullptr);
  AAsset *asset =
      AAssetManager_open(Android_application->activity->assetManager, fname, 0);
  if (!asset) {
    return NULL;
  }

  return funopen(asset, android_read, android_write, android_seek,
                 android_close);
}
#endif

using namespace std;

/*
 * TODO: function description here
 */
VkResult init_global_extension_properties(layer_properties &layer_props) {
  VkExtensionProperties *instance_extensions;
  uint32_t instance_extension_count;
  VkResult res;
  char *layer_name = NULL;

  layer_name = layer_props.properties.layerName;

  do {
    res = vkEnumerateInstanceExtensionProperties(
        layer_name, &instance_extension_count, NULL);
    if (res)
      return res;

    if (instance_extension_count == 0) {
      return VK_SUCCESS;
    }

    layer_props.instance_extensions.resize(instance_extension_count);
    instance_extensions = layer_props.instance_extensions.data();
    res = vkEnumerateInstanceExtensionProperties(
        layer_name, &instance_extension_count, instance_extensions);
  } while (res == VK_INCOMPLETE);

  return res;
}

VkResult init_device_extension_properties(struct sample_info &info,
                                          layer_properties &layer_props) {
  VkExtensionProperties *device_extensions;
  uint32_t device_extension_count;
  VkResult res;
  char *layer_name = NULL;

  layer_name = layer_props.properties.layerName;

  do {
    res = vkEnumerateDeviceExtensionProperties(info.gpus[0], layer_name,
                                               &device_extension_count, NULL);
    if (res)
      return res;

    if (device_extension_count == 0) {
      return VK_SUCCESS;
    }

    layer_props.device_extensions.resize(device_extension_count);
    device_extensions = layer_props.device_extensions.data();
    res = vkEnumerateDeviceExtensionProperties(
        info.gpus[0], layer_name, &device_extension_count, device_extensions);
  } while (res == VK_INCOMPLETE);

  return res;
}

/*
 * Return 1 (true) if all layer names specified in check_names
 * can be found in given layer properties.
 */
VkBool32 demo_check_layers(const std::vector<layer_properties> &layer_props,
                           const std::vector<const char *> &layer_names) {
  uint32_t check_count = layer_names.size();
  uint32_t layer_count = layer_props.size();
  for (uint32_t i = 0; i < check_count; i++) {
    VkBool32 found = 0;
    for (uint32_t j = 0; j < layer_count; j++) {
      if (!strcmp(layer_names[i], layer_props[j].properties.layerName)) {
        found = 1;
      }
    }
    if (!found) {
      std::cout << "Cannot find layer: " << layer_names[i] << std::endl;
      return 0;
    }
  }
  return 1;
}

void init_queue_family_index(struct sample_info &info) {
  /* This routine simply finds a graphics queue for a later vkCreateDevice,
   * without consideration for which queue family can present an image.
   * Do not use this if your intent is to present later in your sample,
   * instead use the init_connection, init_window, init_swapchain_extension,
   * init_device call sequence to get a graphics and present compatible queue
   * family
   */

  vkGetPhysicalDeviceQueueFamilyProperties(info.gpus[0],
                                           &info.queue_family_count, NULL);
  assert(info.queue_family_count >= 1);

  info.queue_props.resize(info.queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(
      info.gpus[0], &info.queue_family_count, info.queue_props.data());
  assert(info.queue_family_count >= 1);

  bool U_ASSERT_ONLY found = false;
  for (unsigned int i = 0; i < info.queue_family_count; i++) {
    if (info.queue_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      info.graphics_queue_family_index = i;
      found = true;
      break;
    }
  }
  assert(found);
}

VkResult init_debug_report_callback(struct sample_info &info,
                                    PFN_vkDebugReportCallbackEXT dbgFunc) {
  VkResult res;
  VkDebugReportCallbackEXT debug_report_callback;

  info.dbgCreateDebugReportCallback =
      (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(
          info.inst, "vkCreateDebugReportCallbackEXT");
  if (!info.dbgCreateDebugReportCallback) {
    std::cout << "GetInstanceProcAddr: Unable to find "
                 "vkCreateDebugReportCallbackEXT function."
              << std::endl;
    return VK_ERROR_INITIALIZATION_FAILED;
  }
  std::cout << "Got dbgCreateDebugReportCallback function\n";

  info.dbgDestroyDebugReportCallback =
      (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(
          info.inst, "vkDestroyDebugReportCallbackEXT");
  if (!info.dbgDestroyDebugReportCallback) {
    std::cout << "GetInstanceProcAddr: Unable to find "
                 "vkDestroyDebugReportCallbackEXT function."
              << std::endl;
    return VK_ERROR_INITIALIZATION_FAILED;
  }
  std::cout << "Got dbgDestroyDebugReportCallback function\n";

  VkDebugReportCallbackCreateInfoEXT create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
  create_info.pNext = NULL;
  create_info.flags =
      VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
  create_info.pfnCallback = dbgFunc;
  create_info.pUserData = NULL;

  res = info.dbgCreateDebugReportCallback(info.inst, &create_info, NULL,
                                          &debug_report_callback);
  switch (res) {
  case VK_SUCCESS:
    std::cout << "Successfully created debug report callback object\n";
    info.debug_report_callbacks.push_back(debug_report_callback);
    break;
  case VK_ERROR_OUT_OF_HOST_MEMORY:
    std::cout << "dbgCreateDebugReportCallback: out of host memory pointer\n"
              << std::endl;
    return VK_ERROR_INITIALIZATION_FAILED;
    break;
  default:
    std::cout << "dbgCreateDebugReportCallback: unknown failure\n" << std::endl;
    return VK_ERROR_INITIALIZATION_FAILED;
    break;
  }
  return res;
}

void destroy_debug_report_callback(struct sample_info &info) {
  while (info.debug_report_callbacks.size() > 0) {
    info.dbgDestroyDebugReportCallback(
        info.inst, info.debug_report_callbacks.back(), NULL);
    info.debug_report_callbacks.pop_back();
  }
}

#if defined(VK_USE_PLATFORM_WAYLAND_KHR)

static void handle_ping(void *data, wl_shell_surface *shell_surface,
                        uint32_t serial) {
  wl_shell_surface_pong(shell_surface, serial);
}

static void handle_configure(void *data, wl_shell_surface *shell_surface,
                             uint32_t edges, int32_t width, int32_t height) {}

static void handle_popup_done(void *data, wl_shell_surface *shell_surface) {}

static const wl_shell_surface_listener shell_surface_listener = {
    handle_ping, handle_configure, handle_popup_done};

static void registry_handle_global(void *data, wl_registry *registry,
                                   uint32_t id, const char *interface,
                                   uint32_t version) {
  sample_info *info = (sample_info *)data;
  // pickup wayland objects when they appear
  if (strcmp(interface, "wl_compositor") == 0) {
    info->compositor = (wl_compositor *)wl_registry_bind(
        registry, id, &wl_compositor_interface, 1);
  } else if (strcmp(interface, "wl_shell") == 0) {
    info->shell =
        (wl_shell *)wl_registry_bind(registry, id, &wl_shell_interface, 1);
  }
}

static void registry_handle_global_remove(void *data, wl_registry *registry,
                                          uint32_t name) {}

static const wl_registry_listener registry_listener = {
    registry_handle_global, registry_handle_global_remove};

#endif

void init_depth_buffer(struct sample_info &info) {
  VkResult U_ASSERT_ONLY res;
  bool U_ASSERT_ONLY pass;
  VkImageCreateInfo image_info = {};
  VkFormatProperties props;

  /* allow custom depth formats */
#ifdef __ANDROID__
  // Depth format needs to be VK_FORMAT_D24_UNORM_S8_UINT on Android (if
  // available).
  vkGetPhysicalDeviceFormatProperties(info.gpus[0], VK_FORMAT_D24_UNORM_S8_UINT,
                                      &props);
  if ((props.linearTilingFeatures &
       VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) ||
      (props.optimalTilingFeatures &
       VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
    info.depth.format = VK_FORMAT_D24_UNORM_S8_UINT;
  else
    info.depth.format = VK_FORMAT_D16_UNORM;
#elif defined(VK_USE_PLATFORM_IOS_MVK)
  if (info.depth.format == VK_FORMAT_UNDEFINED)
    info.depth.format = VK_FORMAT_D32_SFLOAT;
#else
  if (info.depth.format == VK_FORMAT_UNDEFINED)
    info.depth.format = VK_FORMAT_D16_UNORM;
#endif

  const VkFormat depth_format = info.depth.format;
  vkGetPhysicalDeviceFormatProperties(info.gpus[0], depth_format, &props);
  if (props.linearTilingFeatures &
      VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
    image_info.tiling = VK_IMAGE_TILING_LINEAR;
  } else if (props.optimalTilingFeatures &
             VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  } else {
    /* Try other depth formats? */
    std::cout << "depth_format " << depth_format << " Unsupported.\n";
    exit(-1);
  }

  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.pNext = NULL;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.format = depth_format;
  image_info.extent.width = info.width;
  image_info.extent.height = info.height;
  image_info.extent.depth = 1;
  image_info.mipLevels = 1;
  image_info.arrayLayers = 1;
  image_info.samples = NUM_SAMPLES;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_info.queueFamilyIndexCount = 0;
  image_info.pQueueFamilyIndices = NULL;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  image_info.flags = 0;

  VkMemoryAllocateInfo mem_alloc = {};
  mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  mem_alloc.pNext = NULL;
  mem_alloc.allocationSize = 0;
  mem_alloc.memoryTypeIndex = 0;

  VkImageViewCreateInfo view_info = {};
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.pNext = NULL;
  view_info.image = VK_NULL_HANDLE;
  view_info.format = depth_format;
  view_info.components.r = VK_COMPONENT_SWIZZLE_R;
  view_info.components.g = VK_COMPONENT_SWIZZLE_G;
  view_info.components.b = VK_COMPONENT_SWIZZLE_B;
  view_info.components.a = VK_COMPONENT_SWIZZLE_A;
  view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  view_info.subresourceRange.baseMipLevel = 0;
  view_info.subresourceRange.levelCount = 1;
  view_info.subresourceRange.baseArrayLayer = 0;
  view_info.subresourceRange.layerCount = 1;
  view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_info.flags = 0;

  if (depth_format == VK_FORMAT_D16_UNORM_S8_UINT ||
      depth_format == VK_FORMAT_D24_UNORM_S8_UINT ||
      depth_format == VK_FORMAT_D32_SFLOAT_S8_UINT) {
    view_info.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
  }

  VkMemoryRequirements mem_reqs;

  /* Create image */
  res = vkCreateImage(info.device, &image_info, NULL, &info.depth.image);
  assert(res == VK_SUCCESS);

  vkGetImageMemoryRequirements(info.device, info.depth.image, &mem_reqs);

  mem_alloc.allocationSize = mem_reqs.size;
  /* Use the memory properties to determine the type of memory required */
  pass = memory_type_from_properties(info, mem_reqs.memoryTypeBits,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                     &mem_alloc.memoryTypeIndex);
  assert(pass);

  /* Allocate memory */
  res = vkAllocateMemory(info.device, &mem_alloc, NULL, &info.depth.mem);
  assert(res == VK_SUCCESS);

  /* Bind memory */
  res = vkBindImageMemory(info.device, info.depth.image, info.depth.mem, 0);
  assert(res == VK_SUCCESS);

  /* Create image view */
  view_info.image = info.depth.image;
  res = vkCreateImageView(info.device, &view_info, NULL, &info.depth.view);
  assert(res == VK_SUCCESS);
}

/* Use this surface format if it's available.  This ensures that generated
 * images are similar on different devices and with different drivers.
 */
#define PREFERRED_SURFACE_FORMAT VK_FORMAT_B8G8R8A8_UNORM

void init_presentable_image(struct sample_info &info) {
  /* DEPENDS on init_swap_chain() */

  VkResult U_ASSERT_ONLY res;
  VkSemaphoreCreateInfo imageAcquiredSemaphoreCreateInfo;
  imageAcquiredSemaphoreCreateInfo.sType =
      VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  imageAcquiredSemaphoreCreateInfo.pNext = NULL;
  imageAcquiredSemaphoreCreateInfo.flags = 0;

  res = vkCreateSemaphore(info.device, &imageAcquiredSemaphoreCreateInfo, NULL,
                          &info.imageAcquiredSemaphore);
  assert(!res);

  // Get the index of the next available swapchain image:
  res = vkAcquireNextImageKHR(info.device, info.swap_chain, UINT64_MAX,
                              info.imageAcquiredSemaphore, VK_NULL_HANDLE,
                              &info.current_buffer);
  // TODO: Deal with the VK_SUBOPTIMAL_KHR and VK_ERROR_OUT_OF_DATE_KHR
  // return codes
  assert(!res);
}

void execute_queue_cmdbuf(struct sample_info &info,
                          const VkCommandBuffer *cmd_bufs, VkFence &fence) {
  VkResult U_ASSERT_ONLY res;

  VkPipelineStageFlags pipe_stage_flags =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submit_info[1] = {};
  submit_info[0].pNext = NULL;
  submit_info[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info[0].waitSemaphoreCount = 1;
  submit_info[0].pWaitSemaphores = &info.imageAcquiredSemaphore;
  submit_info[0].pWaitDstStageMask = NULL;
  submit_info[0].commandBufferCount = 1;
  submit_info[0].pCommandBuffers = cmd_bufs;
  submit_info[0].pWaitDstStageMask = &pipe_stage_flags;
  submit_info[0].signalSemaphoreCount = 0;
  submit_info[0].pSignalSemaphores = NULL;

  /* Queue the command buffer for execution */
  res = vkQueueSubmit(info.graphics_queue, 1, submit_info, fence);
  assert(!res);
}
void execute_pre_present_barrier(struct sample_info &info) {
  /* DEPENDS on init_swap_chain() */
  /* Add mem barrier to change layout to present */

  VkImageMemoryBarrier prePresentBarrier = {};
  prePresentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  prePresentBarrier.pNext = NULL;
  prePresentBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  prePresentBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  prePresentBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  prePresentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  prePresentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  prePresentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  prePresentBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  prePresentBarrier.subresourceRange.baseMipLevel = 0;
  prePresentBarrier.subresourceRange.levelCount = 1;
  prePresentBarrier.subresourceRange.baseArrayLayer = 0;
  prePresentBarrier.subresourceRange.layerCount = 1;
  prePresentBarrier.image = info.buffers[info.current_buffer].image;
  vkCmdPipelineBarrier(info.cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0,
                       NULL, 1, &prePresentBarrier);
}
void execute_present_image(struct sample_info &info) {
  /* DEPENDS on init_presentable_image() and init_swap_chain()*/
  /* Present the image in the window */

  VkResult U_ASSERT_ONLY res;
  VkPresentInfoKHR present;
  present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present.pNext = NULL;
  present.swapchainCount = 1;
  present.pSwapchains = &info.swap_chain;
  present.pImageIndices = &info.current_buffer;
  present.pWaitSemaphores = NULL;
  present.waitSemaphoreCount = 0;
  present.pResults = NULL;

  res = vkQueuePresentKHR(info.present_queue, &present);
  // TODO: Deal with the VK_SUBOPTIMAL_WSI and VK_ERROR_OUT_OF_DATE_WSI
  // return codes
  assert(!res);
}

void init_uniform_buffer(struct sample_info &info) {
  VkResult U_ASSERT_ONLY res;
  bool U_ASSERT_ONLY pass;
  float fov = glm::radians(45.0f);
  if (info.width > info.height) {
    fov *= static_cast<float>(info.height) / static_cast<float>(info.width);
  }
  info.Projection = glm::perspective(
      fov, static_cast<float>(info.width) / static_cast<float>(info.height),
      0.1f, 100.0f);
  info.View = glm::lookAt(
      glm::vec3(-5, 3, -10), // Camera is at (-5,3,-10), in World Space
      glm::vec3(0, 0, 0),    // and looks at the origin
      glm::vec3(0, -1, 0)    // Head is up (set to 0,-1,0 to look upside-down)
  );
  info.Model = glm::mat4(1.0f);
  // Vulkan clip space has inverted Y and half Z.
  info.Clip = glm::mat4(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f,
                        0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f, 1.0f);

  info.MVP = info.Clip * info.Projection * info.View * info.Model;

  /* VULKAN_KEY_START */
  VkBufferCreateInfo buf_info = {};
  buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buf_info.pNext = NULL;
  buf_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  buf_info.size = sizeof(info.MVP);
  buf_info.queueFamilyIndexCount = 0;
  buf_info.pQueueFamilyIndices = NULL;
  buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  buf_info.flags = 0;
  res = vkCreateBuffer(info.device, &buf_info, NULL, &info.uniform_data.buf);
  assert(res == VK_SUCCESS);

  VkMemoryRequirements mem_reqs;
  vkGetBufferMemoryRequirements(info.device, info.uniform_data.buf, &mem_reqs);

  VkMemoryAllocateInfo alloc_info = {};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.pNext = NULL;
  alloc_info.memoryTypeIndex = 0;

  alloc_info.allocationSize = mem_reqs.size;
  pass = memory_type_from_properties(info, mem_reqs.memoryTypeBits,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                     &alloc_info.memoryTypeIndex);
  assert(pass && "No mappable, coherent memory");

  res = vkAllocateMemory(info.device, &alloc_info, NULL,
                         &(info.uniform_data.mem));
  assert(res == VK_SUCCESS);

  uint8_t *pData;
  res = vkMapMemory(info.device, info.uniform_data.mem, 0, mem_reqs.size, 0,
                    (void **)&pData);
  assert(res == VK_SUCCESS);

  memcpy(pData, &info.MVP, sizeof(info.MVP));

  vkUnmapMemory(info.device, info.uniform_data.mem);

  res = vkBindBufferMemory(info.device, info.uniform_data.buf,
                           info.uniform_data.mem, 0);
  assert(res == VK_SUCCESS);

  info.uniform_data.buffer_info.buffer = info.uniform_data.buf;
  info.uniform_data.buffer_info.offset = 0;
  info.uniform_data.buffer_info.range = sizeof(info.MVP);
}

void init_descriptor_and_pipeline_layouts(
    struct sample_info &info, bool use_texture,
    VkDescriptorSetLayoutCreateFlags descSetLayoutCreateFlags) {
  VkDescriptorSetLayoutBinding layout_bindings[2];
  layout_bindings[0].binding = 0;
  layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  layout_bindings[0].descriptorCount = 1;
  layout_bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  layout_bindings[0].pImmutableSamplers = NULL;

  if (use_texture) {
    layout_bindings[1].binding = 1;
    layout_bindings[1].descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layout_bindings[1].descriptorCount = 1;
    layout_bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layout_bindings[1].pImmutableSamplers = NULL;
  }

  /* Next take layout bindings and use them to create a descriptor set layout
   */
  VkDescriptorSetLayoutCreateInfo descriptor_layout = {};
  descriptor_layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  descriptor_layout.pNext = NULL;
  descriptor_layout.flags = descSetLayoutCreateFlags;
  descriptor_layout.bindingCount = use_texture ? 2 : 1;
  descriptor_layout.pBindings = layout_bindings;

  VkResult U_ASSERT_ONLY res;

  info.desc_layout.resize(NUM_DESCRIPTOR_SETS);
  res = vkCreateDescriptorSetLayout(info.device, &descriptor_layout, NULL,
                                    info.desc_layout.data());
  assert(res == VK_SUCCESS);

  /* Now use the descriptor layout to create a pipeline layout */
  VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
  pPipelineLayoutCreateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pPipelineLayoutCreateInfo.pNext = NULL;
  pPipelineLayoutCreateInfo.pushConstantRangeCount = 0;
  pPipelineLayoutCreateInfo.pPushConstantRanges = NULL;
  pPipelineLayoutCreateInfo.setLayoutCount = NUM_DESCRIPTOR_SETS;
  pPipelineLayoutCreateInfo.pSetLayouts = info.desc_layout.data();

  res = vkCreatePipelineLayout(info.device, &pPipelineLayoutCreateInfo, NULL,
                               &info.pipeline_layout);
  assert(res == VK_SUCCESS);
}

void init_renderpass(struct sample_info &info, bool include_depth, bool clear,
                     VkImageLayout finalLayout, VkImageLayout initialLayout) {
  /* DEPENDS on init_swap_chain() and init_depth_buffer() */

  assert(clear || (initialLayout != VK_IMAGE_LAYOUT_UNDEFINED));

  VkResult U_ASSERT_ONLY res;
  /* Need attachments for render target and depth buffer */
  VkAttachmentDescription attachments[2];
  attachments[0].format = info.format;
  attachments[0].samples = NUM_SAMPLES;
  attachments[0].loadOp =
      clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
  attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[0].initialLayout = initialLayout;
  attachments[0].finalLayout = finalLayout;
  attachments[0].flags = 0;

  if (include_depth) {
    attachments[1].format = info.depth.format;
    attachments[1].samples = NUM_SAMPLES;
    attachments[1].loadOp =
        clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[1].flags = 0;
  }

  VkAttachmentReference color_reference = {};
  color_reference.attachment = 0;
  color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depth_reference = {};
  depth_reference.attachment = 1;
  depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.flags = 0;
  subpass.inputAttachmentCount = 0;
  subpass.pInputAttachments = NULL;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_reference;
  subpass.pResolveAttachments = NULL;
  subpass.pDepthStencilAttachment = include_depth ? &depth_reference : NULL;
  subpass.preserveAttachmentCount = 0;
  subpass.pPreserveAttachments = NULL;

  // Subpass dependency to wait for wsi image acquired semaphore before starting
  // layout transition
  VkSubpassDependency subpass_dependency = {};
  subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  subpass_dependency.dstSubpass = 0;
  subpass_dependency.srcStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  subpass_dependency.dstStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  subpass_dependency.srcAccessMask = 0;
  subpass_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  subpass_dependency.dependencyFlags = 0;

  VkRenderPassCreateInfo rp_info = {};
  rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  rp_info.pNext = NULL;
  rp_info.attachmentCount = include_depth ? 2 : 1;
  rp_info.pAttachments = attachments;
  rp_info.subpassCount = 1;
  rp_info.pSubpasses = &subpass;
  rp_info.dependencyCount = 1;
  rp_info.pDependencies = &subpass_dependency;

  res = vkCreateRenderPass(info.device, &rp_info, NULL, &info.render_pass);
  assert(res == VK_SUCCESS);
}

void init_framebuffers(struct sample_info &info, bool include_depth) {
  /* DEPENDS on init_depth_buffer(), init_renderpass() and
   * init_swapchain_extension() */

  VkResult U_ASSERT_ONLY res;
  VkImageView attachments[2];
  attachments[1] = info.depth.view;

  VkFramebufferCreateInfo fb_info = {};
  fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  fb_info.pNext = NULL;
  fb_info.renderPass = info.render_pass;
  fb_info.attachmentCount = include_depth ? 2 : 1;
  fb_info.pAttachments = attachments;
  fb_info.width = info.width;
  fb_info.height = info.height;
  fb_info.layers = 1;

  uint32_t i;

  info.framebuffers =
      (VkFramebuffer *)malloc(info.swapchainImageCount * sizeof(VkFramebuffer));

  for (i = 0; i < info.swapchainImageCount; i++) {
    attachments[0] = info.buffers[i].view;
    res =
        vkCreateFramebuffer(info.device, &fb_info, NULL, &info.framebuffers[i]);
    assert(res == VK_SUCCESS);
  }
}
void init_command_pool(struct sample_info &info) {
  /* DEPENDS on init_swapchain_extension() */
  VkResult U_ASSERT_ONLY res;

  VkCommandPoolCreateInfo cmd_pool_info = {};
  cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  cmd_pool_info.pNext = NULL;
  cmd_pool_info.queueFamilyIndex = info.graphics_queue_family_index;
  cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  res = vkCreateCommandPool(info.device, &cmd_pool_info, NULL, &info.cmd_pool);
  assert(res == VK_SUCCESS);
}

void init_command_buffer(struct sample_info &info) {
  /* DEPENDS on init_swapchain_extension() and init_command_pool() */
  VkResult U_ASSERT_ONLY res;

  VkCommandBufferAllocateInfo cmd = {};
  cmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cmd.pNext = NULL;
  cmd.commandPool = info.cmd_pool;
  cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cmd.commandBufferCount = 1;

  res = vkAllocateCommandBuffers(info.device, &cmd, &info.cmd);
  assert(res == VK_SUCCESS);
}
void execute_begin_command_buffer(struct sample_info &info) {
  /* DEPENDS on init_command_buffer() */
  VkResult U_ASSERT_ONLY res;

  VkCommandBufferBeginInfo cmd_buf_info = {};
  cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  cmd_buf_info.pNext = NULL;
  cmd_buf_info.flags = 0;
  cmd_buf_info.pInheritanceInfo = NULL;

  res = vkBeginCommandBuffer(info.cmd, &cmd_buf_info);
  assert(res == VK_SUCCESS);
}

void execute_end_command_buffer(struct sample_info &info) {
  VkResult U_ASSERT_ONLY res;

  res = vkEndCommandBuffer(info.cmd);
  assert(res == VK_SUCCESS);
}

void execute_queue_command_buffer(struct sample_info &info) {
  VkResult U_ASSERT_ONLY res;

  /* Queue the command buffer for execution */
  const VkCommandBuffer cmd_bufs[] = {info.cmd};
  VkFenceCreateInfo fenceInfo;
  VkFence drawFence;
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.pNext = NULL;
  fenceInfo.flags = 0;
  vkCreateFence(info.device, &fenceInfo, NULL, &drawFence);

  VkPipelineStageFlags pipe_stage_flags =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submit_info[1] = {};
  submit_info[0].pNext = NULL;
  submit_info[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info[0].waitSemaphoreCount = 0;
  submit_info[0].pWaitSemaphores = NULL;
  submit_info[0].pWaitDstStageMask = &pipe_stage_flags;
  submit_info[0].commandBufferCount = 1;
  submit_info[0].pCommandBuffers = cmd_bufs;
  submit_info[0].signalSemaphoreCount = 0;
  submit_info[0].pSignalSemaphores = NULL;

  res = vkQueueSubmit(info.graphics_queue, 1, submit_info, drawFence);
  assert(res == VK_SUCCESS);

  do {
    res = vkWaitForFences(info.device, 1, &drawFence, VK_TRUE, FENCE_TIMEOUT);
  } while (res == VK_TIMEOUT);
  assert(res == VK_SUCCESS);

  vkDestroyFence(info.device, drawFence, NULL);
}

void init_device_queue(struct sample_info &info) {
  /* DEPENDS on init_swapchain_extension() */

  vkGetDeviceQueue(info.device, info.graphics_queue_family_index, 0,
                   &info.graphics_queue);
  if (info.graphics_queue_family_index == info.present_queue_family_index) {
    info.present_queue = info.graphics_queue;
  } else {
    vkGetDeviceQueue(info.device, info.present_queue_family_index, 0,
                     &info.present_queue);
  }
}

void init_vertex_buffer(struct sample_info &info, const void *vertexData,
                        uint32_t dataSize, uint32_t dataStride,
                        bool use_texture) {
  VkResult U_ASSERT_ONLY res;
  bool U_ASSERT_ONLY pass;

  VkBufferCreateInfo buf_info = {};
  buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buf_info.pNext = NULL;
  buf_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  buf_info.size = dataSize;
  buf_info.queueFamilyIndexCount = 0;
  buf_info.pQueueFamilyIndices = NULL;
  buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  buf_info.flags = 0;
  res = vkCreateBuffer(info.device, &buf_info, NULL, &info.vertex_buffer.buf);
  assert(res == VK_SUCCESS);

  VkMemoryRequirements mem_reqs;
  vkGetBufferMemoryRequirements(info.device, info.vertex_buffer.buf, &mem_reqs);

  VkMemoryAllocateInfo alloc_info = {};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.pNext = NULL;
  alloc_info.memoryTypeIndex = 0;

  alloc_info.allocationSize = mem_reqs.size;
  pass = memory_type_from_properties(info, mem_reqs.memoryTypeBits,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                     &alloc_info.memoryTypeIndex);
  assert(pass && "No mappable, coherent memory");

  res = vkAllocateMemory(info.device, &alloc_info, NULL,
                         &(info.vertex_buffer.mem));
  assert(res == VK_SUCCESS);
  info.vertex_buffer.buffer_info.range = mem_reqs.size;
  info.vertex_buffer.buffer_info.offset = 0;

  uint8_t *pData;
  res = vkMapMemory(info.device, info.vertex_buffer.mem, 0, mem_reqs.size, 0,
                    (void **)&pData);
  assert(res == VK_SUCCESS);

  memcpy(pData, vertexData, dataSize);

  vkUnmapMemory(info.device, info.vertex_buffer.mem);

  res = vkBindBufferMemory(info.device, info.vertex_buffer.buf,
                           info.vertex_buffer.mem, 0);
  assert(res == VK_SUCCESS);

  info.vi_binding.binding = 0;
  info.vi_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  info.vi_binding.stride = dataStride;

  info.vi_attribs[0].binding = 0;
  info.vi_attribs[0].location = 0;
  info.vi_attribs[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
  info.vi_attribs[0].offset = 0;
  info.vi_attribs[1].binding = 0;
  info.vi_attribs[1].location = 1;
  info.vi_attribs[1].format =
      use_texture ? VK_FORMAT_R32G32_SFLOAT : VK_FORMAT_R32G32B32A32_SFLOAT;
  info.vi_attribs[1].offset = 16;
}

void init_descriptor_pool(struct sample_info &info, bool use_texture) {
  /* DEPENDS on init_uniform_buffer() and
   * init_descriptor_and_pipeline_layouts() */

  VkResult U_ASSERT_ONLY res;
  VkDescriptorPoolSize type_count[2];
  type_count[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  type_count[0].descriptorCount = 1;
  if (use_texture) {
    type_count[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    type_count[1].descriptorCount = 1;
  }

  VkDescriptorPoolCreateInfo descriptor_pool = {};
  descriptor_pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptor_pool.pNext = NULL;
  descriptor_pool.maxSets = 1;
  descriptor_pool.poolSizeCount = use_texture ? 2 : 1;
  descriptor_pool.pPoolSizes = type_count;

  res = vkCreateDescriptorPool(info.device, &descriptor_pool, NULL,
                               &info.desc_pool);
  assert(res == VK_SUCCESS);
}

void init_descriptor_set(struct sample_info &info, bool use_texture) {
  /* DEPENDS on init_descriptor_pool() */

  VkResult U_ASSERT_ONLY res;

  VkDescriptorSetAllocateInfo alloc_info[1];
  alloc_info[0].sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info[0].pNext = NULL;
  alloc_info[0].descriptorPool = info.desc_pool;
  alloc_info[0].descriptorSetCount = NUM_DESCRIPTOR_SETS;
  alloc_info[0].pSetLayouts = info.desc_layout.data();

  info.desc_set.resize(NUM_DESCRIPTOR_SETS);
  res = vkAllocateDescriptorSets(info.device, alloc_info, info.desc_set.data());
  assert(res == VK_SUCCESS);

  VkWriteDescriptorSet writes[2];

  writes[0] = {};
  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].pNext = NULL;
  writes[0].dstSet = info.desc_set[0];
  writes[0].descriptorCount = 1;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[0].pBufferInfo = &info.uniform_data.buffer_info;
  writes[0].dstArrayElement = 0;
  writes[0].dstBinding = 0;

  if (use_texture) {
    writes[1] = {};
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = info.desc_set[0];
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].pImageInfo = &info.texture_data.image_info;
    writes[1].dstArrayElement = 0;
  }

  vkUpdateDescriptorSets(info.device, use_texture ? 2 : 1, writes, 0, NULL);
}

void init_shaders(struct sample_info &info,
                  const VkShaderModuleCreateInfo *vertShaderCI,
                  const VkShaderModuleCreateInfo *fragShaderCI) {
  VkResult U_ASSERT_ONLY res;

  if (vertShaderCI) {
    info.shaderStages[0].sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.shaderStages[0].pNext = NULL;
    info.shaderStages[0].pSpecializationInfo = NULL;
    info.shaderStages[0].flags = 0;
    info.shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    info.shaderStages[0].pName = "main";
    res = vkCreateShaderModule(info.device, vertShaderCI, NULL,
                               &info.shaderStages[0].module);
    assert(res == VK_SUCCESS);
  }

  if (fragShaderCI) {
    std::vector<unsigned int> vtx_spv;
    info.shaderStages[1].sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.shaderStages[1].pNext = NULL;
    info.shaderStages[1].pSpecializationInfo = NULL;
    info.shaderStages[1].flags = 0;
    info.shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    info.shaderStages[1].pName = "main";
    res = vkCreateShaderModule(info.device, fragShaderCI, NULL,
                               &info.shaderStages[1].module);
    assert(res == VK_SUCCESS);
  }
}

void init_pipeline_cache(struct sample_info &info) {
  VkResult U_ASSERT_ONLY res;

  VkPipelineCacheCreateInfo pipelineCache;
  pipelineCache.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  pipelineCache.pNext = NULL;
  pipelineCache.initialDataSize = 0;
  pipelineCache.pInitialData = NULL;
  pipelineCache.flags = 0;
  res = vkCreatePipelineCache(info.device, &pipelineCache, NULL,
                              &info.pipelineCache);
  assert(res == VK_SUCCESS);
}

void init_pipeline(struct sample_info &info, VkBool32 include_depth,
                   VkBool32 include_vi) {
  VkResult U_ASSERT_ONLY res;

  VkDynamicState dynamicStateEnables[2]; // Viewport + Scissor
  VkPipelineDynamicStateCreateInfo dynamicState = {};
  memset(dynamicStateEnables, 0, sizeof dynamicStateEnables);
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.pNext = NULL;
  dynamicState.pDynamicStates = dynamicStateEnables;
  dynamicState.dynamicStateCount = 0;

  VkPipelineVertexInputStateCreateInfo vi;
  memset(&vi, 0, sizeof(vi));
  vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  if (include_vi) {
    vi.pNext = NULL;
    vi.flags = 0;
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &info.vi_binding;
    vi.vertexAttributeDescriptionCount = 2;
    vi.pVertexAttributeDescriptions = info.vi_attribs;
  }
  VkPipelineInputAssemblyStateCreateInfo ia;
  ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  ia.pNext = NULL;
  ia.flags = 0;
  ia.primitiveRestartEnable = VK_FALSE;
  ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineRasterizationStateCreateInfo rs;
  rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rs.pNext = NULL;
  rs.flags = 0;
  rs.polygonMode = VK_POLYGON_MODE_FILL;
  rs.cullMode = VK_CULL_MODE_BACK_BIT;
  rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rs.depthClampEnable = VK_FALSE;
  rs.rasterizerDiscardEnable = VK_FALSE;
  rs.depthBiasEnable = VK_FALSE;
  rs.depthBiasConstantFactor = 0;
  rs.depthBiasClamp = 0;
  rs.depthBiasSlopeFactor = 0;
  rs.lineWidth = 1.0f;

  VkPipelineColorBlendStateCreateInfo cb;
  cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  cb.flags = 0;
  cb.pNext = NULL;
  VkPipelineColorBlendAttachmentState att_state[1];
  att_state[0].colorWriteMask = 0xf;
  att_state[0].blendEnable = VK_FALSE;
  att_state[0].alphaBlendOp = VK_BLEND_OP_ADD;
  att_state[0].colorBlendOp = VK_BLEND_OP_ADD;
  att_state[0].srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
  att_state[0].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
  att_state[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  att_state[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  cb.attachmentCount = 1;
  cb.pAttachments = att_state;
  cb.logicOpEnable = VK_FALSE;
  cb.logicOp = VK_LOGIC_OP_NO_OP;
  cb.blendConstants[0] = 1.0f;
  cb.blendConstants[1] = 1.0f;
  cb.blendConstants[2] = 1.0f;
  cb.blendConstants[3] = 1.0f;

  VkPipelineViewportStateCreateInfo vp = {};
  vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  vp.pNext = NULL;
  vp.flags = 0;
#ifndef __ANDROID__
  vp.viewportCount = NUM_VIEWPORTS;
  dynamicStateEnables[dynamicState.dynamicStateCount++] =
      VK_DYNAMIC_STATE_VIEWPORT;
  vp.scissorCount = NUM_SCISSORS;
  dynamicStateEnables[dynamicState.dynamicStateCount++] =
      VK_DYNAMIC_STATE_SCISSOR;
  vp.pScissors = NULL;
  vp.pViewports = NULL;
#else
  // Temporary disabling dynamic viewport on Android because some of drivers
  // doesn't support the feature.
  VkViewport viewports;
  viewports.minDepth = 0.0f;
  viewports.maxDepth = 1.0f;
  viewports.x = 0;
  viewports.y = 0;
  viewports.width = info.width;
  viewports.height = info.height;
  VkRect2D scissor;
  scissor.extent.width = info.width;
  scissor.extent.height = info.height;
  scissor.offset.x = 0;
  scissor.offset.y = 0;
  vp.viewportCount = NUM_VIEWPORTS;
  vp.scissorCount = NUM_SCISSORS;
  vp.pScissors = &scissor;
  vp.pViewports = &viewports;
#endif
  VkPipelineDepthStencilStateCreateInfo ds;
  ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  ds.pNext = NULL;
  ds.flags = 0;
  ds.depthTestEnable = include_depth;
  ds.depthWriteEnable = include_depth;
  ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  ds.depthBoundsTestEnable = VK_FALSE;
  ds.stencilTestEnable = VK_FALSE;
  ds.back.failOp = VK_STENCIL_OP_KEEP;
  ds.back.passOp = VK_STENCIL_OP_KEEP;
  ds.back.compareOp = VK_COMPARE_OP_ALWAYS;
  ds.back.compareMask = 0;
  ds.back.reference = 0;
  ds.back.depthFailOp = VK_STENCIL_OP_KEEP;
  ds.back.writeMask = 0;
  ds.minDepthBounds = 0;
  ds.maxDepthBounds = 0;
  ds.stencilTestEnable = VK_FALSE;
  ds.front = ds.back;

  VkPipelineMultisampleStateCreateInfo ms;
  ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  ms.pNext = NULL;
  ms.flags = 0;
  ms.pSampleMask = NULL;
  ms.rasterizationSamples = NUM_SAMPLES;
  ms.sampleShadingEnable = VK_FALSE;
  ms.alphaToCoverageEnable = VK_FALSE;
  ms.alphaToOneEnable = VK_FALSE;
  ms.minSampleShading = 0.0;

  VkGraphicsPipelineCreateInfo pipeline;
  pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline.pNext = NULL;
  pipeline.layout = info.pipeline_layout;
  pipeline.basePipelineHandle = VK_NULL_HANDLE;
  pipeline.basePipelineIndex = 0;
  pipeline.flags = 0;
  pipeline.pVertexInputState = &vi;
  pipeline.pInputAssemblyState = &ia;
  pipeline.pRasterizationState = &rs;
  pipeline.pColorBlendState = &cb;
  pipeline.pTessellationState = NULL;
  pipeline.pMultisampleState = &ms;
  pipeline.pDynamicState = &dynamicState;
  pipeline.pViewportState = &vp;
  pipeline.pDepthStencilState = &ds;
  pipeline.pStages = info.shaderStages;
  pipeline.stageCount = 2;
  pipeline.renderPass = info.render_pass;
  pipeline.subpass = 0;

  res = vkCreateGraphicsPipelines(info.device, info.pipelineCache, 1, &pipeline,
                                  NULL, &info.pipeline);
  assert(res == VK_SUCCESS);
}

void init_sampler(struct sample_info &info, VkSampler &sampler) {
  VkResult U_ASSERT_ONLY res;

  VkSamplerCreateInfo samplerCreateInfo = {};
  samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
  samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
  samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerCreateInfo.mipLodBias = 0.0;
  samplerCreateInfo.anisotropyEnable = VK_FALSE;
  samplerCreateInfo.maxAnisotropy = 1;
  samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
  samplerCreateInfo.minLod = 0.0;
  samplerCreateInfo.maxLod = 0.0;
  samplerCreateInfo.compareEnable = VK_FALSE;
  samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

  /* create sampler */
  res = vkCreateSampler(info.device, &samplerCreateInfo, NULL, &sampler);
  assert(res == VK_SUCCESS);
}
void init_buffer(struct sample_info &info, texture_object &texObj) {
  VkResult U_ASSERT_ONLY res;
  bool U_ASSERT_ONLY pass;

  VkBufferCreateInfo buffer_create_info = {};
  buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_create_info.pNext = NULL;
  buffer_create_info.flags = 0;
  buffer_create_info.size = texObj.tex_width * texObj.tex_height * 4;
  buffer_create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  buffer_create_info.queueFamilyIndexCount = 0;
  buffer_create_info.pQueueFamilyIndices = NULL;
  res = vkCreateBuffer(info.device, &buffer_create_info, NULL, &texObj.buffer);
  assert(res == VK_SUCCESS);

  VkMemoryAllocateInfo mem_alloc = {};
  mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  mem_alloc.pNext = NULL;
  mem_alloc.allocationSize = 0;
  mem_alloc.memoryTypeIndex = 0;

  VkMemoryRequirements mem_reqs;
  vkGetBufferMemoryRequirements(info.device, texObj.buffer, &mem_reqs);
  mem_alloc.allocationSize = mem_reqs.size;
  texObj.buffer_size = mem_reqs.size;

  VkFlags requirements = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  pass = memory_type_from_properties(info, mem_reqs.memoryTypeBits,
                                     requirements, &mem_alloc.memoryTypeIndex);
  assert(pass && "No mappable, coherent memory");

  /* allocate memory */
  res =
      vkAllocateMemory(info.device, &mem_alloc, NULL, &(texObj.buffer_memory));
  assert(res == VK_SUCCESS);

  /* bind memory */
  res = vkBindBufferMemory(info.device, texObj.buffer, texObj.buffer_memory, 0);
  assert(res == VK_SUCCESS);
}

void init_image(struct sample_info &info, texture_object &texObj,
                const char *textureName, VkImageUsageFlags extraUsages,
                VkFormatFeatureFlags extraFeatures) {
  VkResult U_ASSERT_ONLY res;
  bool U_ASSERT_ONLY pass;
  std::string filename = ""; // get_base_data_dir();

  if (textureName == nullptr)
    filename.append("lunarg.ppm");
  else
    filename.append(textureName);

  if (!read_ppm(filename.c_str(), texObj.tex_width, texObj.tex_height, 0,
                NULL)) {
    std::cout << "Try relative path\n";
    filename = "../../API-Samples/data/";
    if (textureName == nullptr)
      filename.append("lunarg.ppm");
    else
      filename.append(textureName);
    if (!read_ppm(filename.c_str(), texObj.tex_width, texObj.tex_height, 0,
                  NULL)) {
      std::cout << "Could not read texture file " << filename;
      exit(-1);
    }
  }

  VkFormatProperties formatProps;
  vkGetPhysicalDeviceFormatProperties(info.gpus[0], VK_FORMAT_R8G8B8A8_UNORM,
                                      &formatProps);

  /* See if we can use a linear tiled image for a texture, if not, we will
   * need a staging buffer for the texture data */
  VkFormatFeatureFlags allFeatures =
      (VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | extraFeatures);
  texObj.needs_staging =
      ((formatProps.linearTilingFeatures & allFeatures) != allFeatures);

  if (texObj.needs_staging) {
    assert((formatProps.optimalTilingFeatures & allFeatures) == allFeatures);
    init_buffer(info, texObj);
    extraUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  } else {
    texObj.buffer = VK_NULL_HANDLE;
    texObj.buffer_memory = VK_NULL_HANDLE;
  }

  VkImageCreateInfo image_create_info = {};
  image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_create_info.pNext = NULL;
  image_create_info.imageType = VK_IMAGE_TYPE_2D;
  image_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
  image_create_info.extent.width = texObj.tex_width;
  image_create_info.extent.height = texObj.tex_height;
  image_create_info.extent.depth = 1;
  image_create_info.mipLevels = 1;
  image_create_info.arrayLayers = 1;
  image_create_info.samples = NUM_SAMPLES;
  image_create_info.tiling =
      texObj.needs_staging ? VK_IMAGE_TILING_OPTIMAL : VK_IMAGE_TILING_LINEAR;
  image_create_info.initialLayout = texObj.needs_staging
                                        ? VK_IMAGE_LAYOUT_UNDEFINED
                                        : VK_IMAGE_LAYOUT_PREINITIALIZED;
  image_create_info.usage = (VK_IMAGE_USAGE_SAMPLED_BIT | extraUsages);
  image_create_info.queueFamilyIndexCount = 0;
  image_create_info.pQueueFamilyIndices = NULL;
  image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_create_info.flags = 0;

  VkMemoryAllocateInfo mem_alloc = {};
  mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  mem_alloc.pNext = NULL;
  mem_alloc.allocationSize = 0;
  mem_alloc.memoryTypeIndex = 0;

  VkMemoryRequirements mem_reqs;

  res = vkCreateImage(info.device, &image_create_info, NULL, &texObj.image);
  assert(res == VK_SUCCESS);

  vkGetImageMemoryRequirements(info.device, texObj.image, &mem_reqs);

  mem_alloc.allocationSize = mem_reqs.size;

  VkFlags requirements = texObj.needs_staging
                             ? 0
                             : (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  pass = memory_type_from_properties(info, mem_reqs.memoryTypeBits,
                                     requirements, &mem_alloc.memoryTypeIndex);
  assert(pass);

  /* allocate memory */
  res = vkAllocateMemory(info.device, &mem_alloc, NULL, &(texObj.image_memory));
  assert(res == VK_SUCCESS);

  /* bind memory */
  res = vkBindImageMemory(info.device, texObj.image, texObj.image_memory, 0);
  assert(res == VK_SUCCESS);

  res = vkEndCommandBuffer(info.cmd);
  assert(res == VK_SUCCESS);
  const VkCommandBuffer cmd_bufs[] = {info.cmd};
  VkFenceCreateInfo fenceInfo;
  VkFence cmdFence;
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.pNext = NULL;
  fenceInfo.flags = 0;
  vkCreateFence(info.device, &fenceInfo, NULL, &cmdFence);

  VkSubmitInfo submit_info[1] = {};
  submit_info[0].pNext = NULL;
  submit_info[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info[0].waitSemaphoreCount = 0;
  submit_info[0].pWaitSemaphores = NULL;
  submit_info[0].pWaitDstStageMask = NULL;
  submit_info[0].commandBufferCount = 1;
  submit_info[0].pCommandBuffers = cmd_bufs;
  submit_info[0].signalSemaphoreCount = 0;
  submit_info[0].pSignalSemaphores = NULL;

  /* Queue the command buffer for execution */
  res = vkQueueSubmit(info.graphics_queue, 1, submit_info, cmdFence);
  assert(res == VK_SUCCESS);

  VkImageSubresource subres = {};
  subres.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  subres.mipLevel = 0;
  subres.arrayLayer = 0;

  VkSubresourceLayout layout = {};
  void *data;
  if (!texObj.needs_staging) {
    /* Get the subresource layout so we know what the row pitch is */
    vkGetImageSubresourceLayout(info.device, texObj.image, &subres, &layout);
  }

  /* Make sure command buffer is finished before mapping */
  do {
    res = vkWaitForFences(info.device, 1, &cmdFence, VK_TRUE, FENCE_TIMEOUT);
  } while (res == VK_TIMEOUT);
  assert(res == VK_SUCCESS);

  vkDestroyFence(info.device, cmdFence, NULL);

  if (texObj.needs_staging) {
    res = vkMapMemory(info.device, texObj.buffer_memory, 0, texObj.buffer_size,
                      0, &data);
  } else {
    res = vkMapMemory(info.device, texObj.image_memory, 0, mem_reqs.size, 0,
                      &data);
  }
  assert(res == VK_SUCCESS);

  /* Read the ppm file into the mappable image's memory */
  if (!read_ppm(filename.c_str(), texObj.tex_width, texObj.tex_height,
                texObj.needs_staging ? (texObj.tex_width * 4) : layout.rowPitch,
                (unsigned char *)data)) {
    std::cout << "Could not load texture file lunarg.ppm\n";
    exit(-1);
  }

  vkUnmapMemory(info.device, texObj.needs_staging ? texObj.buffer_memory
                                                  : texObj.image_memory);

  VkCommandBufferBeginInfo cmd_buf_info = {};
  cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  cmd_buf_info.pNext = NULL;
  cmd_buf_info.flags = 0;
  cmd_buf_info.pInheritanceInfo = NULL;

  res = vkResetCommandBuffer(info.cmd, 0);
  res = vkBeginCommandBuffer(info.cmd, &cmd_buf_info);
  assert(res == VK_SUCCESS);

  if (!texObj.needs_staging) {
    /* If we can use the linear tiled image as a texture, just do it */
    texObj.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    set_image_layout(info, texObj.image, VK_IMAGE_ASPECT_COLOR_BIT,
                     VK_IMAGE_LAYOUT_PREINITIALIZED, texObj.imageLayout,
                     VK_PIPELINE_STAGE_HOST_BIT,
                     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
  } else {
    /* Since we're going to blit to the texture image, set its layout to
     * DESTINATION_OPTIMAL */
    set_image_layout(
        info, texObj.image, VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    VkBufferImageCopy copy_region;
    copy_region.bufferOffset = 0;
    copy_region.bufferRowLength = texObj.tex_width;
    copy_region.bufferImageHeight = texObj.tex_height;
    copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_region.imageSubresource.mipLevel = 0;
    copy_region.imageSubresource.baseArrayLayer = 0;
    copy_region.imageSubresource.layerCount = 1;
    copy_region.imageOffset.x = 0;
    copy_region.imageOffset.y = 0;
    copy_region.imageOffset.z = 0;
    copy_region.imageExtent.width = texObj.tex_width;
    copy_region.imageExtent.height = texObj.tex_height;
    copy_region.imageExtent.depth = 1;

    /* Put the copy command into the command buffer */
    vkCmdCopyBufferToImage(info.cmd, texObj.buffer, texObj.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &copy_region);

    /* Set the layout for the texture image from DESTINATION_OPTIMAL to
     * SHADER_READ_ONLY */
    texObj.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    set_image_layout(info, texObj.image, VK_IMAGE_ASPECT_COLOR_BIT,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texObj.imageLayout,
                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
  }

  VkImageViewCreateInfo view_info = {};
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.pNext = NULL;
  view_info.image = VK_NULL_HANDLE;
  view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_info.format = VK_FORMAT_R8G8B8A8_UNORM;
  view_info.components.r = VK_COMPONENT_SWIZZLE_R;
  view_info.components.g = VK_COMPONENT_SWIZZLE_G;
  view_info.components.b = VK_COMPONENT_SWIZZLE_B;
  view_info.components.a = VK_COMPONENT_SWIZZLE_A;
  view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  view_info.subresourceRange.baseMipLevel = 0;
  view_info.subresourceRange.levelCount = 1;
  view_info.subresourceRange.baseArrayLayer = 0;
  view_info.subresourceRange.layerCount = 1;

  /* create image view */
  view_info.image = texObj.image;
  res = vkCreateImageView(info.device, &view_info, NULL, &texObj.view);
  assert(res == VK_SUCCESS);
}

void init_texture(struct sample_info &info, const char *textureName,
                  VkImageUsageFlags extraUsages,
                  VkFormatFeatureFlags extraFeatures) {
  struct texture_object texObj;

  /* create image */
  init_image(info, texObj, textureName, extraUsages, extraFeatures);

  /* create sampler */
  init_sampler(info, texObj.sampler);

  info.textures.push_back(texObj);

  /* track a description of the texture */
  info.texture_data.image_info.imageView = info.textures.back().view;
  info.texture_data.image_info.sampler = info.textures.back().sampler;
  info.texture_data.image_info.imageLayout =
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void init_viewports(struct sample_info &info) {
#ifdef __ANDROID__
// Disable dynamic viewport on Android. Some drive has an issue with the dynamic
// viewport feature.
#else
  info.viewport.height = (float)info.height;
  info.viewport.width = (float)info.width;
  info.viewport.minDepth = (float)0.0f;
  info.viewport.maxDepth = (float)1.0f;
  info.viewport.x = 0;
  info.viewport.y = 0;
  vkCmdSetViewport(info.cmd, 0, NUM_VIEWPORTS, &info.viewport);
#endif
}

void init_scissors(struct sample_info &info) {
#ifdef __ANDROID__
// Disable dynamic viewport on Android. Some drive has an issue with the dynamic
// scissors feature.
#else
  info.scissor.extent.width = info.width;
  info.scissor.extent.height = info.height;
  info.scissor.offset.x = 0;
  info.scissor.offset.y = 0;
  vkCmdSetScissor(info.cmd, 0, NUM_SCISSORS, &info.scissor);
#endif
}

void init_fence(struct sample_info &info, VkFence &fence) {
  VkFenceCreateInfo fenceInfo;
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.pNext = NULL;
  fenceInfo.flags = 0;
  vkCreateFence(info.device, &fenceInfo, NULL, &fence);
}

void init_submit_info(struct sample_info &info, VkSubmitInfo &submit_info,
                      VkPipelineStageFlags &pipe_stage_flags) {
  submit_info.pNext = NULL;
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = &info.imageAcquiredSemaphore;
  submit_info.pWaitDstStageMask = &pipe_stage_flags;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &info.cmd;
  submit_info.signalSemaphoreCount = 0;
  submit_info.pSignalSemaphores = NULL;
}

void init_present_info(struct sample_info &info, VkPresentInfoKHR &present) {
  present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present.pNext = NULL;
  present.swapchainCount = 1;
  present.pSwapchains = &info.swap_chain;
  present.pImageIndices = &info.current_buffer;
  present.pWaitSemaphores = NULL;
  present.waitSemaphoreCount = 0;
  present.pResults = NULL;
}

void init_clear_color_and_depth(struct sample_info &info,
                                VkClearValue *clear_values) {
  clear_values[0].color.float32[0] = 0.2f;
  clear_values[0].color.float32[1] = 0.2f;
  clear_values[0].color.float32[2] = 0.2f;
  clear_values[0].color.float32[3] = 0.2f;
  clear_values[1].depthStencil.depth = 1.0f;
  clear_values[1].depthStencil.stencil = 0;
}

void init_render_pass_begin_info(struct sample_info &info,
                                 VkRenderPassBeginInfo &rp_begin) {
  rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rp_begin.pNext = NULL;
  rp_begin.renderPass = info.render_pass;
  rp_begin.framebuffer = info.framebuffers[info.current_buffer];
  rp_begin.renderArea.offset.x = 0;
  rp_begin.renderArea.offset.y = 0;
  rp_begin.renderArea.extent.width = info.width;
  rp_begin.renderArea.extent.height = info.height;
  rp_begin.clearValueCount = 0;
  rp_begin.pClearValues = nullptr;
}

void destroy_pipeline(struct sample_info &info) {
  vkDestroyPipeline(info.device, info.pipeline, NULL);
}

void destroy_pipeline_cache(struct sample_info &info) {
  vkDestroyPipelineCache(info.device, info.pipelineCache, NULL);
}

void destroy_uniform_buffer(struct sample_info &info) {
  vkDestroyBuffer(info.device, info.uniform_data.buf, NULL);
  vkFreeMemory(info.device, info.uniform_data.mem, NULL);
}

void destroy_descriptor_and_pipeline_layouts(struct sample_info &info) {
  for (int i = 0; i < NUM_DESCRIPTOR_SETS; i++)
    vkDestroyDescriptorSetLayout(info.device, info.desc_layout[i], NULL);
  vkDestroyPipelineLayout(info.device, info.pipeline_layout, NULL);
}

void destroy_descriptor_pool(struct sample_info &info) {
  vkDestroyDescriptorPool(info.device, info.desc_pool, NULL);
}

void destroy_shaders(struct sample_info &info) {
  vkDestroyShaderModule(info.device, info.shaderStages[0].module, NULL);
  vkDestroyShaderModule(info.device, info.shaderStages[1].module, NULL);
}

void destroy_command_buffer(struct sample_info &info) {
  VkCommandBuffer cmd_bufs[1] = {info.cmd};
  vkFreeCommandBuffers(info.device, info.cmd_pool, 1, cmd_bufs);
}

void destroy_command_pool(struct sample_info &info) {
  vkDestroyCommandPool(info.device, info.cmd_pool, NULL);
}

void destroy_depth_buffer(struct sample_info &info) {
  vkDestroyImageView(info.device, info.depth.view, NULL);
  vkDestroyImage(info.device, info.depth.image, NULL);
  vkFreeMemory(info.device, info.depth.mem, NULL);
}

void destroy_vertex_buffer(struct sample_info &info) {
  vkDestroyBuffer(info.device, info.vertex_buffer.buf, NULL);
  vkFreeMemory(info.device, info.vertex_buffer.mem, NULL);
}

void destroy_swap_chain(struct sample_info &info) {
  for (uint32_t i = 0; i < info.swapchainImageCount; i++) {
    vkDestroyImageView(info.device, info.buffers[i].view, NULL);
  }
  vkDestroySwapchainKHR(info.device, info.swap_chain, NULL);
}

void destroy_framebuffers(struct sample_info &info) {
  for (uint32_t i = 0; i < info.swapchainImageCount; i++) {
    vkDestroyFramebuffer(info.device, info.framebuffers[i], NULL);
  }
  free(info.framebuffers);
}

void destroy_renderpass(struct sample_info &info) {
  vkDestroyRenderPass(info.device, info.render_pass, NULL);
}

void destroy_textures(struct sample_info &info) {
  for (size_t i = 0; i < info.textures.size(); i++) {
    vkDestroySampler(info.device, info.textures[i].sampler, NULL);
    vkDestroyImageView(info.device, info.textures[i].view, NULL);
    vkDestroyImage(info.device, info.textures[i].image, NULL);
    vkFreeMemory(info.device, info.textures[i].image_memory, NULL);
    vkDestroyBuffer(info.device, info.textures[i].buffer, NULL);
    vkFreeMemory(info.device, info.textures[i].buffer_memory, NULL);
  }
}

/* We've setup cmake to process 15-draw_cube.vert and 15-draw_cube.frag */
/* files containing the glsl shader code for this sample.  The generate-spirv
 * script uses */
/* glslangValidator to compile the glsl into spir-v and places the spir-v into a
 * struct   */
/* into a generated header file */

void main_loop(const std::function<bool()> &runLoop,
               const vuloxr::vk::Instance &instance,
               vuloxr::vk::Swapchain &swapchain,
               const vuloxr::vk::PhysicalDevice &physicalDevice,
               const vuloxr::vk::Device &device, void *window) {
  struct sample_info info = {
      .inst = instance,
      .gpus = {physicalDevice},
      .device = device,
      .memory_properties = physicalDevice.memoryProps,
      .width = static_cast<int>(swapchain.createInfo.imageExtent.width),
      .height = static_cast<int>(swapchain.createInfo.imageExtent.height),
      .format = swapchain.createInfo.imageFormat,
      .swapchainImageCount = static_cast<uint32_t>(swapchain.images.size()),
      .swap_chain = swapchain,
  };
  for (uint32_t i = 0; i < info.swapchainImageCount; i++) {
    swap_chain_buffer sc_buffer;

    VkImageViewCreateInfo color_image_view = {};
    color_image_view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    color_image_view.pNext = NULL;
    color_image_view.format = info.format;
    color_image_view.components.r = VK_COMPONENT_SWIZZLE_R;
    color_image_view.components.g = VK_COMPONENT_SWIZZLE_G;
    color_image_view.components.b = VK_COMPONENT_SWIZZLE_B;
    color_image_view.components.a = VK_COMPONENT_SWIZZLE_A;
    color_image_view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    color_image_view.subresourceRange.baseMipLevel = 0;
    color_image_view.subresourceRange.levelCount = 1;
    color_image_view.subresourceRange.baseArrayLayer = 0;
    color_image_view.subresourceRange.layerCount = 1;
    color_image_view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    color_image_view.flags = 0;

    sc_buffer.image = swapchain.images[i];

    color_image_view.image = sc_buffer.image;

    auto res = vkCreateImageView(info.device, &color_image_view, NULL,
                            &sc_buffer.view);
    info.buffers.push_back(sc_buffer);
    assert(res == VK_SUCCESS);
  }
  info.current_buffer = 0;

  init_command_pool(info);
  init_command_buffer(info);
  execute_begin_command_buffer(info);
  init_device_queue(info);

  const bool depthPresent = true;
  init_depth_buffer(info);
  init_uniform_buffer(info);
  init_descriptor_and_pipeline_layouts(info, false);
  init_renderpass(info, depthPresent);

  const char VS[] = {
#embed "15-draw_cube.vert"
      , 0, 0, 0, 0,
  };
  auto vs = vuloxr::vk::glsl_vs_to_spv(VS);
  VkShaderModuleCreateInfo vert_info = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = static_cast<uint32_t>(vs.size() * 4),
      .pCode = vs.data(),
  };

  const char FS[] = {
#embed "15-draw_cube.frag"
      , 0, 0, 0, 0,
  };
  auto fs = vuloxr::vk::glsl_fs_to_spv(FS);
  VkShaderModuleCreateInfo frag_info = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = static_cast<uint32_t>(fs.size() * 4),
      .pCode = fs.data(),
  };
  init_shaders(info, &vert_info, &frag_info);
  init_framebuffers(info, depthPresent);
  init_vertex_buffer(info, g_vb_solid_face_colors_Data,
                     sizeof(g_vb_solid_face_colors_Data),
                     sizeof(g_vb_solid_face_colors_Data[0]), false);
  init_descriptor_pool(info, false);
  init_descriptor_set(info, false);
  init_pipeline_cache(info);
  init_pipeline(info, depthPresent);

  /* VULKAN_KEY_START */

  VkClearValue clear_values[2];
  clear_values[0].color.float32[0] = 0.2f;
  clear_values[0].color.float32[1] = 0.2f;
  clear_values[0].color.float32[2] = 0.2f;
  clear_values[0].color.float32[3] = 0.2f;
  clear_values[1].depthStencil.depth = 1.0f;
  clear_values[1].depthStencil.stencil = 0;

  VkSemaphore imageAcquiredSemaphore;
  VkSemaphoreCreateInfo imageAcquiredSemaphoreCreateInfo;
  imageAcquiredSemaphoreCreateInfo.sType =
      VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  imageAcquiredSemaphoreCreateInfo.pNext = NULL;
  imageAcquiredSemaphoreCreateInfo.flags = 0;

  auto res = vkCreateSemaphore(info.device, &imageAcquiredSemaphoreCreateInfo,
                               NULL, &imageAcquiredSemaphore);
  assert(res == VK_SUCCESS);

  while (runLoop()) {

    // Get the index of the next available swapchain image:
    res = vkAcquireNextImageKHR(info.device, info.swap_chain, UINT64_MAX,
                                imageAcquiredSemaphore, VK_NULL_HANDLE,
                                &info.current_buffer);
    // TODO: Deal with the VK_SUBOPTIMAL_KHR and VK_ERROR_OUT_OF_DATE_KHR
    // return codes
    assert(res == VK_SUCCESS);

    VkRenderPassBeginInfo rp_begin;
    rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin.pNext = NULL;
    rp_begin.renderPass = info.render_pass;
    rp_begin.framebuffer = info.framebuffers[info.current_buffer];
    rp_begin.renderArea.offset.x = 0;
    rp_begin.renderArea.offset.y = 0;
    rp_begin.renderArea.extent.width = info.width;
    rp_begin.renderArea.extent.height = info.height;
    rp_begin.clearValueCount = 2;
    rp_begin.pClearValues = clear_values;

    vkCmdBeginRenderPass(info.cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, info.pipeline);
    vkCmdBindDescriptorSets(info.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            info.pipeline_layout, 0, NUM_DESCRIPTOR_SETS,
                            info.desc_set.data(), 0, NULL);

    const VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(info.cmd, 0, 1, &info.vertex_buffer.buf, offsets);

    init_viewports(info);
    init_scissors(info);

    vkCmdDraw(info.cmd, 12 * 3, 1, 0, 0);
    vkCmdEndRenderPass(info.cmd);
    res = vkEndCommandBuffer(info.cmd);
    const VkCommandBuffer cmd_bufs[] = {info.cmd};
    VkFenceCreateInfo fenceInfo;
    VkFence drawFence;
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.pNext = NULL;
    fenceInfo.flags = 0;
    vkCreateFence(info.device, &fenceInfo, NULL, &drawFence);

    VkPipelineStageFlags pipe_stage_flags =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info[1] = {};
    submit_info[0].pNext = NULL;
    submit_info[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info[0].waitSemaphoreCount = 1;
    submit_info[0].pWaitSemaphores = &imageAcquiredSemaphore;
    submit_info[0].pWaitDstStageMask = &pipe_stage_flags;
    submit_info[0].commandBufferCount = 1;
    submit_info[0].pCommandBuffers = cmd_bufs;
    submit_info[0].signalSemaphoreCount = 0;
    submit_info[0].pSignalSemaphores = NULL;

    /* Queue the command buffer for execution */
    res = vkQueueSubmit(info.graphics_queue, 1, submit_info, drawFence);
    assert(res == VK_SUCCESS);

    /* Now present the image in the window */

    VkPresentInfoKHR present;
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.pNext = NULL;
    present.swapchainCount = 1;
    present.pSwapchains = &info.swap_chain;
    present.pImageIndices = &info.current_buffer;
    present.pWaitSemaphores = NULL;
    present.waitSemaphoreCount = 0;
    present.pResults = NULL;

    /* Make sure command buffer is finished before presenting */
    do {
      res = vkWaitForFences(info.device, 1, &drawFence, VK_TRUE, FENCE_TIMEOUT);
    } while (res == VK_TIMEOUT);

    assert(res == VK_SUCCESS);
    res = vkQueuePresentKHR(info.present_queue, &present);
    assert(res == VK_SUCCESS);

    // wait_seconds(1);
    /* VULKAN_KEY_END */
    // if (info.save_images)
    //   write_ppm(info, "15-draw_cube");

    vkDestroyFence(info.device, drawFence, NULL);
  }

  vkDestroySemaphore(info.device, imageAcquiredSemaphore, NULL);
  destroy_pipeline(info);
  destroy_pipeline_cache(info);
  destroy_descriptor_pool(info);
  destroy_vertex_buffer(info);
  destroy_framebuffers(info);
  destroy_shaders(info);
  destroy_renderpass(info);
  destroy_descriptor_and_pipeline_layouts(info);
  destroy_uniform_buffer(info);
  destroy_depth_buffer(info);
  destroy_swap_chain(info);
  destroy_command_buffer(info);
  destroy_command_pool(info);
}
