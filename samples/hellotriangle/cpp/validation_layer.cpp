  std::vector<std::string> externalLayers;
  PFN_vkDebugReportCallbackEXT externalDebugCallback = nullptr;
  void *pExternalDebugCallbackUserData = nullptr;
  VkDebugUtilsMessengerEXT DebugUtilsMessengerEXT = VK_NULL_HANDLE;



  inline void
  addExternalLayers(std::vector<const char *> &activeLayers,
                    const std::vector<VkLayerProperties> &supportedLayers) {
    for (auto &layer : externalLayers) {
      for (auto &supportedLayer : supportedLayers) {
        if (layer == supportedLayer.layerName) {
          activeLayers.push_back(supportedLayer.layerName);
          LOGI("Found external layer: %s\n", supportedLayer.layerName);
          break;
        }
      }
    }
  }



#if ENABLE_VALIDATION_LAYERS
  uint32_t instanceLayerCount;
  VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr));
  vector<VkLayerProperties> instanceLayers(instanceLayerCount);
  VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerCount,
                                              instanceLayers.data()));

  // A layer could have VK_EXT_debug_report extension.
  for (auto &layer : instanceLayers) {
    uint32_t count;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(layer.layerName, &count,
                                                    nullptr));
    vector<VkExtensionProperties> extensions(count);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(layer.layerName, &count,
                                                    extensions.data()));
    for (auto &ext : extensions)
      instanceExtensions.push_back(ext);
  }

  // On desktop, the LunarG loader exposes a meta-layer that combines all
  // relevant validation layers.
  vector<const char *> activeLayers;

  // On Android, add all relevant layers one by one.
  if (activeLayers.empty()) {
    addSupportedLayers(activeLayers, instanceLayers, pValidationLayers,
                       NELEMS(pValidationLayers));
  }

  if (activeLayers.empty())
    LOGI("Did not find validation layers.\n");
  else
    LOGI("Found validation layers!\n");

  addExternalLayers(activeLayers, instanceLayers);
#endif

#if ENABLE_VALIDATION_LAYERS
  if (!activeLayers.empty()) {
    instanceInfo.enabledLayerCount = activeLayers.size();
    instanceInfo.ppEnabledLayerNames = activeLayers.data();
    LOGI("Using Vulkan instance validation layers.\n");
  }
#endif

  VkDebugUtilsMessengerCreateInfoEXT DebugUtilsMessengerCreateInfoEXT{
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
      .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
      .pfnUserCallback = debugCallback,
  };

  if (activeLayers.size()) {
    if (CreateDebugUtilsMessengerEXT(instance,
                                     &DebugUtilsMessengerCreateInfoEXT, nullptr,
                                     &DebugUtilsMessengerEXT) != VK_SUCCESS) {
      LOGE("failed to set up debug messenger!");
      return MaliSDK::RESULT_ERROR_GENERIC;
    }
  }

#if ENABLE_VALIDATION_LAYERS
  uint32_t deviceLayerCount;
  VK_CHECK(vkEnumerateDeviceLayerProperties(gpu, &deviceLayerCount, nullptr));
  vector<VkLayerProperties> deviceLayers(deviceLayerCount);
  VK_CHECK(vkEnumerateDeviceLayerProperties(gpu, &deviceLayerCount,
                                            deviceLayers.data()));

  activeLayers.clear();
  // On desktop, the LunarG loader exposes a meta-layer that combines all
  // relevant validation layers.

  // On Android, add all relevant layers one by one.
  if (activeLayers.empty()) {
    addSupportedLayers(activeLayers, deviceLayers, pValidationLayers,
                       NELEMS(pValidationLayers));
  }
  addExternalLayers(activeLayers, deviceLayers);
#endif

#if ENABLE_VALIDATION_LAYERS
  if (!activeLayers.empty()) {
    deviceInfo.enabledLayerCount = activeLayers.size();
    deviceInfo.ppEnabledLayerNames = activeLayers.data();
    LOGI("Using Vulkan device validation layers.\n");
  }
#endif


