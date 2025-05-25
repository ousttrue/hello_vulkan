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
#ifndef META_SIMULTANEOUS_HANDS_AND_CONTROLLERS_H_
#define META_SIMULTANEOUS_HANDS_AND_CONTROLLERS_H_ 1

/**********************
This file is @generated from the OpenXR XML API registry.
Language    :   C99
Copyright   :   (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.
***********************/

#include <openxr/openxr.h>

#ifdef __cplusplus
extern "C" {
#endif


#ifndef XR_META_simultaneous_hands_and_controllers

// XR_META_simultaneous_hands_and_controllers is a preprocessor guard. Do not pass it to API calls.
#define XR_META_simultaneous_hands_and_controllers 1
#define XR_META_simultaneous_hands_and_controllers_SPEC_VERSION 1
#define XR_META_SIMULTANEOUS_HANDS_AND_CONTROLLERS_EXTENSION_NAME "XR_META_simultaneous_hands_and_controllers"
static const XrStructureType XR_TYPE_SYSTEM_SIMULTANEOUS_HANDS_AND_CONTROLLERS_PROPERTIES_META = (XrStructureType) 1000532001;
static const XrStructureType XR_TYPE_SIMULTANEOUS_HANDS_AND_CONTROLLERS_TRACKING_RESUME_INFO_META = (XrStructureType) 1000532002;
static const XrStructureType XR_TYPE_SIMULTANEOUS_HANDS_AND_CONTROLLERS_TRACKING_PAUSE_INFO_META = (XrStructureType) 1000532003;
typedef struct XrSystemSimultaneousHandsAndControllersPropertiesMETA {
    XrStructureType       type;
    void* XR_MAY_ALIAS    next;
    XrBool32              supportsSimultaneousHandsAndControllers;
} XrSystemSimultaneousHandsAndControllersPropertiesMETA;

typedef struct XrSimultaneousHandsAndControllersTrackingResumeInfoMETA {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
} XrSimultaneousHandsAndControllersTrackingResumeInfoMETA;

typedef struct XrSimultaneousHandsAndControllersTrackingPauseInfoMETA {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
} XrSimultaneousHandsAndControllersTrackingPauseInfoMETA;

typedef XrResult (XRAPI_PTR *PFN_xrResumeSimultaneousHandsAndControllersTrackingMETA)(XrSession session, const XrSimultaneousHandsAndControllersTrackingResumeInfoMETA* resumeInfo);
typedef XrResult (XRAPI_PTR *PFN_xrPauseSimultaneousHandsAndControllersTrackingMETA)(XrSession session, const XrSimultaneousHandsAndControllersTrackingPauseInfoMETA* pauseInfo);

#ifndef XR_NO_PROTOTYPES
#ifdef XR_EXTENSION_PROTOTYPES
XRAPI_ATTR XrResult XRAPI_CALL xrResumeSimultaneousHandsAndControllersTrackingMETA(
    XrSession                                   session,
    const XrSimultaneousHandsAndControllersTrackingResumeInfoMETA* resumeInfo);

XRAPI_ATTR XrResult XRAPI_CALL xrPauseSimultaneousHandsAndControllersTrackingMETA(
    XrSession                                   session,
    const XrSimultaneousHandsAndControllersTrackingPauseInfoMETA* pauseInfo);
#endif /* XR_EXTENSION_PROTOTYPES */
#endif /* !XR_NO_PROTOTYPES */
#endif /* XR_META_simultaneous_hands_and_controllers */

#ifdef __cplusplus
}
#endif

#endif
