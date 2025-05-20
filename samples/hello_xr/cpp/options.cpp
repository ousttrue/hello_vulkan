#include "options.h"

struct Options {
  std::string FormFactor{"Hmd"};

  std::string ViewConfiguration{"Stereo"};

  std::string EnvironmentBlendMode{"Opaque"};

  std::string AppSpace{"Local"};

  struct {
    XrFormFactor FormFactor{XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY};

    XrViewConfigurationType ViewConfigType{
        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO};

    XrEnvironmentBlendMode EnvironmentBlendMode{
        XR_ENVIRONMENT_BLEND_MODE_OPAQUE};
  } Parsed;

  void ParseStrings() {
    Parsed.FormFactor = GetXrFormFactor(FormFactor);
    Parsed.ViewConfigType = GetXrViewConfigurationType(ViewConfiguration);
    Parsed.EnvironmentBlendMode =
        GetXrEnvironmentBlendMode(EnvironmentBlendMode);
  }

  std::array<float, 4> GetBackgroundClearColor() const {
    static const std::array<float, 4> SlateGrey{0.184313729f, 0.309803933f,
                                                0.309803933f, 1.0f};
    static const std::array<float, 4> TransparentBlack{0.0f, 0.0f, 0.0f, 0.0f};
    static const std::array<float, 4> Black{0.0f, 0.0f, 0.0f, 1.0f};

    switch (Parsed.EnvironmentBlendMode) {
    case XR_ENVIRONMENT_BLEND_MODE_OPAQUE:
      return SlateGrey;
    case XR_ENVIRONMENT_BLEND_MODE_ADDITIVE:
      return Black;
    case XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND:
      return TransparentBlack;
    default:
      return SlateGrey;
    }
  }

  void SetEnvironmentBlendMode(XrEnvironmentBlendMode environmentBlendMode) {
    EnvironmentBlendMode = GetXrEnvironmentBlendModeStr(environmentBlendMode);
    Parsed.EnvironmentBlendMode = environmentBlendMode;
  }
};
