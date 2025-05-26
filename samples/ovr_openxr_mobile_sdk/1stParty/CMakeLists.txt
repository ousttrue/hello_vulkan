add_library(OVR INTERFACE)
target_include_directories(
  OVR INTERFACE ${CMAKE_CURRENT_LIST_DIR}/OVR/Include
                ${CMAKE_CURRENT_LIST_DIR}/utilities/Include)
