#pragma once
#include "InputState.h"
#include "xr_check.h"
#include "xr_linear.h"
#include <common/FloatTypes.h>
#include <common/fmt.h>
#include <common/logger.h>
#include <vector>

struct CubeScene {
  std::vector<Cube> cubes;

  static Cube MakeCube(XrPosef pose, XrVector3f scale) {
    return Cube{
        .Translaton =
            {
                pose.position.x,
                pose.position.y,
                pose.position.z,
            },
        .Rotation =
            {
                pose.orientation.x,
                pose.orientation.y,
                pose.orientation.z,
                pose.orientation.w,
            },
        .Scaling =
            {
                scale.x,
                scale.y,
                scale.z,
            },
    };
  }

  // For each locatable space that we want to visualize, render a 25cm cube.
  void addSpaceCubes(XrSpace appSpace, XrTime predictedDisplayTime,
                     const std::vector<XrSpace> &visualizedSpaces) {
    for (XrSpace visualizedSpace : visualizedSpaces) {
      XrSpaceLocation spaceLocation{XR_TYPE_SPACE_LOCATION};
      auto res = xrLocateSpace(visualizedSpace, appSpace, predictedDisplayTime,
                               &spaceLocation);
      CHECK_XRRESULT(res, "xrLocateSpace");
      if (XR_UNQUALIFIED_SUCCESS(res)) {
        if ((spaceLocation.locationFlags &
             XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 &&
            (spaceLocation.locationFlags &
             XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0) {
          cubes.push_back(MakeCube(spaceLocation.pose, {0.25f, 0.25f, 0.25f}));
        }
      } else {
        Log::Write(Log::Level::Verbose, Fmt("Unable to locate a visualized "
                                            "reference space in app space: %d",
                                            res));
      }
    }
  }

  void addHandCubes(XrSpace appSpace, XrTime predictedDisplayTime,
                    const InputState &input) {
    // Render a 10cm cube scaled by grabAction for each hand. Note renderHand
    // will only be true when the application has focus.
    for (auto hand : {Side::LEFT, Side::RIGHT}) {
      XrSpaceLocation spaceLocation{XR_TYPE_SPACE_LOCATION};
      auto res = xrLocateSpace(input.handSpace[hand], appSpace,
                               predictedDisplayTime, &spaceLocation);
      CHECK_XRRESULT(res, "xrLocateSpace");
      if (XR_UNQUALIFIED_SUCCESS(res)) {
        if ((spaceLocation.locationFlags &
             XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 &&
            (spaceLocation.locationFlags &
             XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0) {
          float scale = 0.1f * input.handScale[hand];
          cubes.push_back(MakeCube(spaceLocation.pose, {scale, scale, scale}));
        }
      } else {
        // Tracking loss is expected when the hand is not active so only log a
        // message if the hand is active.
        if (input.handActive[hand] == XR_TRUE) {
          const char *handName[] = {"left", "right"};
          Log::Write(
              Log::Level::Verbose,
              Fmt("Unable to locate %s hand action space in app space: %d",
                  handName[hand], res));
        }
      }
    }
  }

  std::vector<Mat4> CalcCubeMatrices(const XrMatrix4x4f &vp) {
    std::vector<Mat4> matrices;
    for (auto &cube : cubes) {
      // Compute the model-view-projection transform and push it.
      XrMatrix4x4f model;
      XrMatrix4x4f_CreateTranslationRotationScale(
          &model, (XrVector3f *)&cube.Translaton,
          (XrQuaternionf *)&cube.Rotation, (XrVector3f *)&cube.Scaling);
      Mat4 mvp;
      XrMatrix4x4f_Multiply((XrMatrix4x4f *)&mvp, &vp, &model);
      matrices.push_back(mvp);
    }
    return matrices;
  }
};
