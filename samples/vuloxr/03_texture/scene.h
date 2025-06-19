#include <vko/vko_pipeline.h>

struct Texture {
  VkDevice device;
  vko::Image image;
  VkImageView imageView = VK_NULL_HANDLE;
  VkSampler sampler = VK_NULL_HANDLE;

  Texture(VkPhysicalDevice physicalDevice, VkDevice _device,
          uint32_t graphicsQueueFamilyIndex, uint32_t width, uint32_t height,
          const uint8_t *pixels);
  ~Texture();
};

struct Vec2 {
  float x;
  float y;
};

struct Vec3 {
  float x;
  float y;
  float z;
};

struct Vertex {
  Vec2 pos;
  Vec3 color;
  Vec2 texCoord;
};

struct Scene {
  VkDevice device;

  vko::IndexedMesh mesh = {
      .inputBindingDescriptions =
          {
              {
                  .binding = 0,
                  .stride = static_cast<uint32_t>(sizeof(Vertex)),
                  .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
              },
          },
      .attributeDescriptions = {
          // describes position
          {
              .location = 0,
              .binding = 0,
              .format = VK_FORMAT_R32G32_SFLOAT,
              .offset = offsetof(Vertex, pos),
          },
          // describes color
          {
              .location = 1,
              .binding = 0,
              .format = VK_FORMAT_R32G32B32_SFLOAT,
              .offset = offsetof(Vertex, color),
          },
          // uv
          {
              .location = 2,
              .binding = 0,
              .format = VK_FORMAT_R32G32_SFLOAT,
              .offset = offsetof(Vertex, texCoord),
          },
      }};

  std::shared_ptr<Texture> texture;

  Scene(VkPhysicalDevice physicalDevice, VkDevice _device,
        uint32_t graphicsQueueFamilyIndex);
  ~Scene();
};
