  inline void addExternalLayer(const char *pName) {
    externalLayers.push_back(pName);
  }
  inline void setExternalDebugCallback(PFN_vkDebugReportCallbackEXT callback,
                                       void *pUserData) {
    externalDebugCallback = callback;
    pExternalDebugCallbackUserData = pUserData;
  }
  inline PFN_vkDebugReportCallbackEXT getExternalDebugCallback() const {
    return externalDebugCallback;
  }
  inline void *getExternalDebugCallbackUserData() const {
    return pExternalDebugCallbackUserData;
  }

