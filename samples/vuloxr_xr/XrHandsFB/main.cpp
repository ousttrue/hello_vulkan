/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * Licensed under the Oculus SDK License Agreement (the "License");
 * you may not use the Oculus SDK except in compliance with the License,
 * which is provided at the time of installation or download, or which
 * otherwise accompanies this software in either electronic or hard copy form.
 *
 * You may obtain a copy of the License at
 * https://developer.oculus.com/licenses/oculussdk/
 *
 * Unless required by applicable law or agreed to in writing, the Oculus SDK
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*******************************************************************************

Filename    :   Main.cpp
Content     :   Simple test app to test openxr hands
Created     :   Sept 2020
Authors     :   Federico Schliemann
Language    :   C++
Copyright:  Copyright (c) Facebook Technologies, LLC and its affiliates. All
rights reserved.

*******************************************************************************/

#include "OXR.h"
#include "XrApp.h"

#include "Renderer.h"

static inline XrTime ToXrTime(const double timeInSeconds) {
  return (timeInSeconds * 1e9);
}

#define FORCE_ONLY_SIMPLE_CONTROLLER_PROFILE

class XrHandsApp : public OVRFW::XrApp {

  static constexpr std::string_view kSampleExplanation =
      "OpenXR Hand Tracking:                                              \n"
      "                                                                   \n"
      "Press the buttons under the Left and Right hand labels to:         \n"
      "Mesh: toggle rendering the hand mesh data.                         \n"
      "Joints: toggle rendering the hand joints as spheres.               \n"
      "Capsules: toggle rendering the hand capsules as rounded cylinders. \n";

  Renderer m_renderer;

  /// Hands - extension functions
  PFN_xrCreateHandTrackerEXT xrCreateHandTrackerEXT_ = nullptr;
  PFN_xrDestroyHandTrackerEXT xrDestroyHandTrackerEXT_ = nullptr;
  PFN_xrLocateHandJointsEXT xrLocateHandJointsEXT_ = nullptr;
  /// Hands - tracker handles
  XrHandTrackerEXT handTrackerL_ = XR_NULL_HANDLE;
  XrHandTrackerEXT handTrackerR_ = XR_NULL_HANDLE;
  /// Hands - data buffers
  XrHandJointLocationEXT jointLocationsL_[XR_HAND_JOINT_COUNT_EXT];
  XrHandJointLocationEXT jointLocationsR_[XR_HAND_JOINT_COUNT_EXT];
  XrHandJointVelocityEXT jointVelocitiesL_[XR_HAND_JOINT_COUNT_EXT];
  XrHandJointVelocityEXT jointVelocitiesR_[XR_HAND_JOINT_COUNT_EXT];

  bool handTrackedL_ = false;
  bool handTrackedR_ = false;

  bool lastFrameClickedL_ = false;
  bool lastFrameClickedR_ = false;

  OVR::Vector4f jointColor_{0.4, 0.5, 0.2, 0.5};
  OVR::Vector4f capsuleColor_{0.4, 0.2, 0.2, 0.5};

  bool renderMeshL_ = true;
  bool renderMeshR_ = true;
  bool renderJointsL_ = false;
  bool renderJointsR_ = false;
  bool renderCapsulesL_ = false;
  bool renderCapsulesR_ = false;

public:
  XrHandsApp() : OVRFW::XrApp() {
    BackgroundColor = OVR::Vector4f(0.60f, 0.95f, 0.4f, 1.0f);
  }

  // Returns a list of OpenXr extensions needed for this app
  virtual std::vector<const char *> GetExtensions() override {
    std::vector<const char *> extensions = XrApp::GetExtensions();
    extensions.push_back(XR_EXT_HAND_TRACKING_EXTENSION_NAME);
    extensions.push_back(XR_FB_HAND_TRACKING_MESH_EXTENSION_NAME);
    extensions.push_back(XR_FB_HAND_TRACKING_AIM_EXTENSION_NAME);
    extensions.push_back(XR_FB_HAND_TRACKING_CAPSULES_EXTENSION_NAME);
    return extensions;
  }

#ifdef FORCE_ONLY_SIMPLE_CONTROLLER_PROFILE
  // Returns a map from interaction profile paths to vectors of suggested
  // bindings. xrSuggestInteractionProfileBindings() is called once for each
  // interaction profile path in the returned map. Apps are encouraged to
  // suggest bindings for every device/interaction profile they support.
  // Override this for custom action bindings, or modify the default bindings.
  std::unordered_map<XrPath, std::vector<XrActionSuggestedBinding>>
  GetSuggestedBindings(XrInstance instance) override {
    // Get base suggested bindings
    std::unordered_map<XrPath, std::vector<XrActionSuggestedBinding>>
        allSuggestedBindings = XrApp::GetSuggestedBindings(instance);

    std::unordered_map<XrPath, std::vector<XrActionSuggestedBinding>>
        onlySimpleSuggestedBindings{};

    XrPath simpleInteractionProfile = XR_NULL_PATH;
    OXR(xrStringToPath(instance, "/interaction_profiles/khr/simple_controller",
                       &simpleInteractionProfile));

    // Only copy over suggested bindings for the simple interaction profile
    onlySimpleSuggestedBindings[simpleInteractionProfile] =
        allSuggestedBindings[simpleInteractionProfile];

    return onlySimpleSuggestedBindings;
  }
#endif

  // Must return true if the application initializes successfully.
  virtual bool AppInit(const xrJava *context) override {
    m_renderer.appInit(GetInstance());

    // Inspect hand tracking system properties
    XrSystemHandTrackingPropertiesEXT handTrackingSystemProperties{
        .type = XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT,
    };
    XrSystemProperties systemProperties{
        .type = XR_TYPE_SYSTEM_PROPERTIES,
        .next = &handTrackingSystemProperties,
    };
    OXR(xrGetSystemProperties(GetInstance(), GetSystemId(), &systemProperties));
    if (!handTrackingSystemProperties.supportsHandTracking) {
      // The system does not support hand tracking
      ALOG("xrGetSystemProperties XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT "
           "FAILED.");
      return false;
    } else {
      ALOG("xrGetSystemProperties XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT "
           "OK - initiallizing hand tracking...");
    }

    /// Hook up extensions for hand tracking
    OXR(xrGetInstanceProcAddr(
        GetInstance(), "xrCreateHandTrackerEXT",
        (PFN_xrVoidFunction *)(&xrCreateHandTrackerEXT_)));
    OXR(xrGetInstanceProcAddr(
        GetInstance(), "xrDestroyHandTrackerEXT",
        (PFN_xrVoidFunction *)(&xrDestroyHandTrackerEXT_)));
    OXR(xrGetInstanceProcAddr(GetInstance(), "xrLocateHandJointsEXT",
                              (PFN_xrVoidFunction *)(&xrLocateHandJointsEXT_)));

    return true;
  }

  virtual void AppShutdown(const xrJava *context) override {
    /// unhook extensions for hand tracking
    xrCreateHandTrackerEXT_ = nullptr;
    xrDestroyHandTrackerEXT_ = nullptr;
    xrLocateHandJointsEXT_ = nullptr;

    OVRFW::XrApp::AppShutdown(context);
    m_renderer.appShutdown();
  }

  virtual bool SessionInit() override {
    m_renderer.sessionInit();

    /// Hand Trackers
    if (xrCreateHandTrackerEXT_) {
      XrHandTrackerCreateInfoEXT createInfo{
          XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT};
      createInfo.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;
      createInfo.hand = XR_HAND_LEFT_EXT;
      OXR(xrCreateHandTrackerEXT_(GetSession(), &createInfo, &handTrackerL_));
      createInfo.hand = XR_HAND_RIGHT_EXT;
      OXR(xrCreateHandTrackerEXT_(GetSession(), &createInfo, &handTrackerR_));

      ALOG("xrCreateHandTrackerEXT handTrackerL_=%llx",
           (long long)handTrackerL_);
      ALOG("xrCreateHandTrackerEXT handTrackerR_=%llx",
           (long long)handTrackerR_);

      /// Setup skinning meshes for both hands
      for (int handIndex = 0; handIndex < 2; ++handIndex) {
        /// Alias everything for initialization
        const bool isLeft = (handIndex == 0);
        auto &handTracker = isLeft ? handTrackerL_ : handTrackerR_;
        auto *jointLocations = isLeft ? jointLocationsL_ : jointLocationsR_;
        m_renderer.sessionInitHand(handIndex, handTracker, jointLocations);
      }
    }

    return true;
  }

  virtual void SessionEnd() override {
    /// Hand Tracker
    if (xrDestroyHandTrackerEXT_) {
      OXR(xrDestroyHandTrackerEXT_(handTrackerL_));
      OXR(xrDestroyHandTrackerEXT_(handTrackerR_));
    }
    m_renderer.sessionEnd();
  }

  // Update state
  virtual void Update(const OVRFW::ovrApplFrameIn &in) override {
    // ui_.HitTestDevices().clear();

    if ((in.AllButtons & OVRFW::ovrApplFrameIn::kButtonY) != 0) {
      ALOG("Y button is pressed!");
    }
    if ((in.AllButtons & OVRFW::ovrApplFrameIn::kButtonMenu) != 0) {
      ALOG("Menu button is pressed!");
    }
    if ((in.AllButtons & OVRFW::ovrApplFrameIn::kButtonA) != 0) {
      ALOG("A button is pressed!");
    }
    if ((in.AllButtons & OVRFW::ovrApplFrameIn::kButtonB) != 0) {
      ALOG("B button is pressed!");
    }
    if ((in.AllButtons & OVRFW::ovrApplFrameIn::kButtonX) != 0) {
      ALOG("X button is pressed!");
    }

    /// Hands
    if (xrLocateHandJointsEXT_) {
      /// L
      XrHandTrackingScaleFB scaleL{XR_TYPE_HAND_TRACKING_SCALE_FB};
      scaleL.next = nullptr;
      scaleL.sensorOutput = 1.0f;
      scaleL.currentOutput = 1.0f;
      scaleL.overrideValueInput = 1.00f;
      scaleL.overrideHandScale = XR_FALSE; // XR_TRUE;
      XrHandTrackingCapsulesStateFB capsuleStateL{
          XR_TYPE_HAND_TRACKING_CAPSULES_STATE_FB};
      capsuleStateL.next = &scaleL;
      XrHandTrackingAimStateFB aimStateL{XR_TYPE_HAND_TRACKING_AIM_STATE_FB};
      aimStateL.next = &capsuleStateL;
      XrHandJointVelocitiesEXT velocitiesL{XR_TYPE_HAND_JOINT_VELOCITIES_EXT};
      velocitiesL.next = &aimStateL;
      velocitiesL.jointCount = XR_HAND_JOINT_COUNT_EXT;
      velocitiesL.jointVelocities = jointVelocitiesL_;
      XrHandJointLocationsEXT locationsL{XR_TYPE_HAND_JOINT_LOCATIONS_EXT};
      locationsL.next = &velocitiesL;
      locationsL.jointCount = XR_HAND_JOINT_COUNT_EXT;
      locationsL.jointLocations = jointLocationsL_;

      XrHandJointsLocateInfoEXT locateInfoL{
          .type = XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT,
          .baseSpace = GetStageSpace(),
          .time = ToXrTime(in.PredictedDisplayTime),
      };
      OXR(xrLocateHandJointsEXT_(handTrackerL_, &locateInfoL, &locationsL));

      /// R
      XrHandTrackingScaleFB scaleR{
          .type = XR_TYPE_HAND_TRACKING_SCALE_FB,
          .next = nullptr,
          .sensorOutput = 1.0f,
          .currentOutput = 1.0f,
          .overrideHandScale = XR_FALSE, // XR_TRUE;
          .overrideValueInput = 1.00f,
      };
      XrHandTrackingCapsulesStateFB capsuleStateR{
          .type = XR_TYPE_HAND_TRACKING_CAPSULES_STATE_FB,
          .next = &scaleR,
      };
      XrHandTrackingAimStateFB aimStateR{
          .type = XR_TYPE_HAND_TRACKING_AIM_STATE_FB,
          .next = &capsuleStateR,
      };
      XrHandJointVelocitiesEXT velocitiesR{
          .type = XR_TYPE_HAND_JOINT_VELOCITIES_EXT,
          .next = &aimStateR,
          .jointCount = XR_HAND_JOINT_COUNT_EXT,
          .jointVelocities = jointVelocitiesR_,
      };
      XrHandJointLocationsEXT locationsR{
          .type = XR_TYPE_HAND_JOINT_LOCATIONS_EXT,
          .next = &velocitiesR,
          .jointCount = XR_HAND_JOINT_COUNT_EXT,
          .jointLocations = jointLocationsR_,
      };
      XrHandJointsLocateInfoEXT locateInfoR{
          .type = XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT,
          .baseSpace = GetStageSpace(),
          .time = ToXrTime(in.PredictedDisplayTime),
      };
      OXR(xrLocateHandJointsEXT_(handTrackerR_, &locateInfoR, &locationsR));

      std::vector<OVR::Posef> handJointsL;
      std::vector<OVR::Posef> handJointsR;

      // Determine which joints are actually tracked
      // XrSpaceLocationFlags isTracked =
      // XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT
      //    | XR_SPACE_LOCATION_POSITION_TRACKED_BIT;

      // Tracked joints and computed joints can all be valid
      XrSpaceLocationFlags isValid = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT |
                                     XR_SPACE_LOCATION_POSITION_VALID_BIT;

      handTrackedL_ = false;
      handTrackedR_ = false;

      if (locationsL.isActive) {
        for (int i = 0; i < XR_FB_HAND_TRACKING_CAPSULE_COUNT; ++i) {
          const OVR::Vector3f p0 =
              FromXrVector3f(capsuleStateL.capsules[i].points[0]);
          const OVR::Vector3f p1 =
              FromXrVector3f(capsuleStateL.capsules[i].points[1]);
          const OVR::Vector3f d = (p1 - p0);
          const OVR::Quatf look = OVR::Quatf::LookRotation(d, {0, 1, 0});
          /// apply inverse scale here
          const float h = d.Length() / scaleL.currentOutput;
          const OVR::Vector3f start =
              p0 + look.Rotate(OVR::Vector3f(0, 0, -h / 2));
          // OVRFW::GeometryRenderer& gr = handCapsuleRenderersL_[i];
          // gr.SetScale(OVR::Vector3f(scaleL.currentOutput));
          // gr.SetPose(OVR::Posef(look, start));
          // gr.Update();
        }
        for (int i = 0; i < XR_HAND_JOINT_COUNT_EXT; ++i) {
          if ((jointLocationsL_[i].locationFlags & isValid) != 0) {
            const auto p = FromXrPosef(jointLocationsL_[i].pose);
            handJointsL.push_back(p);
            handTrackedL_ = true;
            // OVRFW::GeometryRenderer& gr = handJointRenderersL_[i];
            // gr.SetScale(OVR::Vector3f(scaleL.currentOutput));
            // gr.SetPose(p);
            // gr.Update();
          }
        }
        m_renderer.updateHand(0, jointLocationsL_);
        const bool didPinch = (aimStateL.status &
                               XR_HAND_TRACKING_AIM_INDEX_PINCHING_BIT_FB) != 0;
        // ui_.AddHitTestRay(FromXrPosef(aimStateL.aimPose), didPinch &&
        // !lastFrameClickedL_);
        lastFrameClickedL_ = didPinch;
      }
      if (locationsR.isActive) {
        for (int i = 0; i < XR_FB_HAND_TRACKING_CAPSULE_COUNT; ++i) {
          const OVR::Vector3f p0 =
              FromXrVector3f(capsuleStateR.capsules[i].points[0]);
          const OVR::Vector3f p1 =
              FromXrVector3f(capsuleStateR.capsules[i].points[1]);
          const OVR::Vector3f d = (p1 - p0);
          const OVR::Quatf look = OVR::Quatf::LookRotation(d, {0, 1, 0});
          /// apply inverse scale here
          const float h = d.Length() / scaleR.currentOutput;
          const OVR::Vector3f start =
              p0 + look.Rotate(OVR::Vector3f(0, 0, -h / 2));
          // OVRFW::GeometryRenderer& gr = handCapsuleRenderersR_[i];
          // gr.SetScale(OVR::Vector3f(scaleR.currentOutput));
          // gr.SetPose(OVR::Posef(look, start));
          // gr.Update();
        }
        for (int i = 0; i < XR_HAND_JOINT_COUNT_EXT; ++i) {
          if ((jointLocationsR_[i].locationFlags & isValid) != 0) {
            const auto p = FromXrPosef(jointLocationsR_[i].pose);
            handJointsR.push_back(p);
            handTrackedR_ = true;
            // OVRFW::GeometryRenderer& gr = handJointRenderersR_[i];
            // gr.SetScale(OVR::Vector3f(scaleR.currentOutput));
            // gr.SetPose(p);
            // gr.Update();
          }
        }
        // handRendererR_.Update(&jointLocationsR_[0]);
        const bool didPinch = (aimStateR.status &
                               XR_HAND_TRACKING_AIM_INDEX_PINCHING_BIT_FB) != 0;

        // ui_.AddHitTestRay(FromXrPosef(aimStateR.aimPose), didPinch &&
        // !lastFrameClickedR_);
        lastFrameClickedR_ = didPinch;
      }
      // axisRendererL_.Update(handJointsL);
      // axisRendererR_.Update(handJointsR);
    }

    if (in.LeftRemoteTracked && !handTrackedL_) {
      // controllerRenderL_.Update(in.LeftRemotePose);
      // const bool didPinch = in.LeftRemoteIndexTrigger > 0.5f;
      // ui_.AddHitTestRay(in.LeftRemotePointPose, didPinch);
    }
    if (in.RightRemoteTracked && !handTrackedR_) {
      // controllerRenderR_.Update(in.RightRemotePose);
      // const bool didPinch = in.RightRemoteIndexTrigger > 0.5f;
      // ui_.AddHitTestRay(in.RightRemotePointPose, didPinch);
    }

    // ui_.Update(in);
    // beamRenderer_.Update(in, ui_.HitTestDevices());
  }

  // Render eye buffers while running
  virtual void Render(const OVRFW::ovrApplFrameIn &in,
                      OVRFW::ovrRendererOutput &out) override {
    m_renderer.render(in, out, handTrackedL_, handTrackedR_);
  }
};

// ENTRY_POINT(XrHandsApp)
int main(int argc, char **argv) {
  XrHandsApp appl;
  appl.Run();
  return 0;
}
