#include <vulkan/vulkan.h>

#include <iostream>
#include <vector>

class VulkanFramework {
  std::string _appName;
  std::string _engineName;
  VkApplicationInfo ApplicationInfo{};
  VkInstanceCreateInfo InstanceCreateInfo{};
  VkInstance Instance = VK_NULL_HANDLE;

public:
  VulkanFramework(const char *appName, const char *engineName) {
    _appName = appName;
    _engineName = engineName;
    std::cout << "VulkanFramewor::VulkanFramework" << std::endl;
  }
  ~VulkanFramework() {
    std::cout << "VulkanFramewor::~VulkanFramework" << std::endl;
    cleanup();
  }

  bool initialize() {
    // if (enableValidationLayers && !checkValidationLayerSupport()) {
    //   throw std::runtime_error("validation layers requested, but not
    //   available!");
    // }
    // auto extensions = getRequiredExtensions();
    //   populateDebugMessengerCreateInfo(debugCreateInfo);
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (!createInstance({}, {}, &debugCreateInfo)) {
      return false;
    }
    return true;
  }

  void cleanup() {
    if (Instance != VK_NULL_HANDLE) {
      vkDestroyInstance(Instance, nullptr);
    }
  }

private:
  bool createInstance(const std::vector<const char *> &extensionNames,
                      const std::vector<const char *> &layerNames,
                      const VkDebugUtilsMessengerCreateInfoEXT
                          *pDebugUtilsMessengerCreateInfoEXT) {
    ApplicationInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = _appName.c_str(),
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = _engineName.c_str(),
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0,
    };

    InstanceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = pDebugUtilsMessengerCreateInfoEXT,
        .pApplicationInfo = &ApplicationInfo,
        .enabledLayerCount = static_cast<uint32_t>(layerNames.size()),
        .ppEnabledLayerNames = layerNames.data(),
        .enabledExtensionCount = static_cast<uint32_t>(extensionNames.size()),
        .ppEnabledExtensionNames = extensionNames.data(),
    };
    if (vkCreateInstance(&InstanceCreateInfo, nullptr, &Instance) !=
        VK_SUCCESS) {
      return false;
    }
    return true;
  }
};

int main(int argc, char **argv) {
  VulkanFramework vulfwk("vulfwk", "No Engine");
  if (!vulfwk.initialize()) {
    std::cout << "failed" << std::endl;
    return 1;
  }

  return 0;
}
