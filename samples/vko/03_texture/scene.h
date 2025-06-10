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

  std::shared_ptr<vko::Buffer> vertexBuffer;
  VkVertexInputBindingDescription vertexInputBindingDescription = {
      .binding = 0,
      .stride = static_cast<uint32_t>(sizeof(Vertex)),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  };

  std::shared_ptr<vko::Buffer> indexBuffer;
  uint32_t indexDrawCount = 0;

  std::shared_ptr<Texture> texture;

  Scene(VkPhysicalDevice physicalDevice, VkDevice _device,
        uint32_t graphicsQueueFamilyIndex);
  ~Scene();
};
