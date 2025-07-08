# composition

- https://developer.mozilla.org/en-US/docs/Web/API/XRCompositionLayer
- https://github.com/KHeresy/openxr-simple-example

## XrFrameEndInfo

https://registry.khronos.org/OpenXR/specs/1.0/man/html/XrFrameEndInfo.html

```cpp
typedef struct XrFrameEndInfo {
    XrStructureType                               type;
    const void*                                   next;
    XrTime                                        displayTime;
    XrEnvironmentBlendMode                        environmentBlendMode;
    uint32_t                                      layerCount;
    const XrCompositionLayerBaseHeader* const*    layers;
} XrFrameEndInfo;
```

## `XrCompositionLayerBaseHeader*`

- https://learn.microsoft.com/ja-jp/windows/mixed-reality/develop/native/openxr-best-practices

### XrCompositionLayerProjection

### XrCompositionLayerQuad

- https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrCompositionLayerQuad.html

```cpp
typedef struct XrCompositionLayerQuad {
    XrStructureType            type;
    const void*                next;
    XrCompositionLayerFlags    layerFlags;
    XrSpace                    space;
    XrEyeVisibility            eyeVisibility;
    XrSwapchainSubImage        subImage;
    XrPosef                    pose;
    XrExtent2Df                size;
} XrCompositionLayerQuad;
```

- https://docs.godotengine.org/en/stable/classes/class_openxrcompositionlayerquad.html

### XrCompositionLayerCubeKHR

### XrCompositionLayerCylinderKHR

### XrCompositionLayerEquirect2KHR

### XrCompositionLayerEquirectKHR

### XrCompositionLayerPassthroughHTC
