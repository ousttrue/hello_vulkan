#include "options.h"
#include "logger.h"
#include "to_string.h"
#include <vector>

bool EqualsIgnoreCase(const std::string &s1, const std::string &s2,
                      const std::locale &loc) {
  const std::ctype<char> &ctype = std::use_facet<std::ctype<char>>(loc);
  const auto compareCharLower = [&](char c1, char c2) {
    return ctype.tolower(c1) == ctype.tolower(c2);
  };
  return s1.size() == s2.size() &&
         std::equal(s1.begin(), s1.end(), s2.begin(), compareCharLower);
}

XrFormFactor GetXrFormFactor(const std::string &formFactorStr) {
  if (EqualsIgnoreCase(formFactorStr, "Hmd")) {
    return XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
  }
  if (EqualsIgnoreCase(formFactorStr, "Handheld")) {
    return XR_FORM_FACTOR_HANDHELD_DISPLAY;
  }
  throw std::invalid_argument(
      Fmt("Unknown form factor '%s'", formFactorStr.c_str()));
}

XrViewConfigurationType
GetXrViewConfigurationType(const std::string &viewConfigurationStr) {
  if (EqualsIgnoreCase(viewConfigurationStr, "Mono")) {
    return XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO;
  }
  if (EqualsIgnoreCase(viewConfigurationStr, "Stereo")) {
    return XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
  }
  throw std::invalid_argument(
      Fmt("Unknown view configuration '%s'", viewConfigurationStr.c_str()));
}

XrEnvironmentBlendMode
GetXrEnvironmentBlendMode(const std::string &environmentBlendModeStr) {
  if (EqualsIgnoreCase(environmentBlendModeStr, "Opaque")) {
    return XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
  }
  if (EqualsIgnoreCase(environmentBlendModeStr, "Additive")) {
    return XR_ENVIRONMENT_BLEND_MODE_ADDITIVE;
  }
  if (EqualsIgnoreCase(environmentBlendModeStr, "AlphaBlend")) {
    return XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND;
  }
  throw std::invalid_argument(Fmt("Unknown environment blend mode '%s'",
                                  environmentBlendModeStr.c_str()));
}

const char *
GetXrEnvironmentBlendModeStr(XrEnvironmentBlendMode environmentBlendMode) {
  switch (environmentBlendMode) {
  case XR_ENVIRONMENT_BLEND_MODE_OPAQUE:
    return "Opaque";
  case XR_ENVIRONMENT_BLEND_MODE_ADDITIVE:
    return "Additive";
  case XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND:
    return "AlphaBlend";
  default:
    throw std::invalid_argument(Fmt("Unknown environment blend mode '%s'",
                                    to_string(environmentBlendMode)));
  }
}

std::string GetXrVersionString(XrVersion ver) {
  return Fmt("%d.%d.%d", XR_VERSION_MAJOR(ver), XR_VERSION_MINOR(ver),
             XR_VERSION_PATCH(ver));
}

//
// Options
//
void Options::ParseStrings() {
  Parsed.FormFactor = GetXrFormFactor(FormFactor);
  Parsed.ViewConfigType = GetXrViewConfigurationType(ViewConfiguration);
  Parsed.EnvironmentBlendMode = GetXrEnvironmentBlendMode(EnvironmentBlendMode);
}

Vec4 Options::GetBackgroundClearColor() const {
  static const Vec4 SlateGrey{0.184313729f, 0.309803933f, 0.309803933f, 1.0f};
  static const Vec4 TransparentBlack{0.0f, 0.0f, 0.0f, 0.0f};
  static const Vec4 Black{0.0f, 0.0f, 0.0f, 1.0f};

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

void Options::SetEnvironmentBlendMode(
    XrEnvironmentBlendMode environmentBlendMode) {
  EnvironmentBlendMode = GetXrEnvironmentBlendModeStr(environmentBlendMode);
  Parsed.EnvironmentBlendMode = environmentBlendMode;
}

bool Options ::UpdateOptionsFromCommandLine(int argc, char *argv[]) {
  int i = 1; // Index 0 is the program name and is skipped.

  auto getNextArg = [argc, argv, &i] {
    if (i >= argc) {
      throw std::invalid_argument("Argument parameter missing");
    }

    return std::string(argv[i++]);
  };

  while (i < argc) {
    const std::string arg = getNextArg();
    if (EqualsIgnoreCase(arg, "--formfactor") || EqualsIgnoreCase(arg, "-ff")) {
      this->FormFactor = getNextArg();
    } else if (EqualsIgnoreCase(arg, "--viewconfig") ||
               EqualsIgnoreCase(arg, "-vc")) {
      this->ViewConfiguration = getNextArg();
    } else if (EqualsIgnoreCase(arg, "--blendmode") ||
               EqualsIgnoreCase(arg, "-bm")) {
      this->EnvironmentBlendMode = getNextArg();
    } else if (EqualsIgnoreCase(arg, "--space") ||
               EqualsIgnoreCase(arg, "-s")) {
      this->AppSpace = getNextArg();
    } else if (EqualsIgnoreCase(arg, "--verbose") ||
               EqualsIgnoreCase(arg, "-v")) {
      Log::SetLevel(Log::Level::Verbose);
    } else if (EqualsIgnoreCase(arg, "--help") || EqualsIgnoreCase(arg, "-h")) {
      return false;
    } else {
      throw std::invalid_argument(Fmt("Unknown argument: %s", arg.c_str()));
    }
  }

  try {
    this->ParseStrings();
  } catch (std::invalid_argument &ia) {
    Log::Write(Log::Level::Error, ia.what());
    return false;
  }
  return true;
}

#if XR_USE_PLATFORM_ANDROID
#include <sys/system_properties.h>

bool Options::UpdateOptionsFromSystemProperties() {
  std::vector<char> buf(PROP_VALUE_MAX);
  auto value = buf.data();

  if (__system_property_get("debug.xr.formFactor", value) != 0) {
    this->FormFactor = value;
  }

  if (__system_property_get("debug.xr.viewConfiguration", value) != 0) {
    this->ViewConfiguration = value;
  }

  if (__system_property_get("debug.xr.blendMode", value) != 0) {
    this->EnvironmentBlendMode = value;
  }

  try {
    this->ParseStrings();
  } catch (std::invalid_argument &ia) {
    Log::Write(Log::Level::Error, ia.what());
    return false;
  }
  return true;
}

#endif
