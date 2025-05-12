if(TOOLCHAIN_PREFIX)
  set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc)
  set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
endif(TOOLCHAIN_PREFIX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif(NOT CMAKE_BUILD_TYPE)

set(CMAKE_CXX_STANDARD 11)

if(CMAKE_COMPILER_IS_GNUCXX OR (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang"))
  message("Compiler is GNU compatible.")
  set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-function -Wno-unused-variable -DVK_PROTOTYPES=1 -pthread"
  )
elseif(MSVC)
  message("Compiler is MSVC.")
  set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} /D_CRT_SECURE_NO_WARNINGS /wd4267 /wd4244 /wd4146")
endif()

if(NOT PLATFORM)
  if(ANDROID_NDK)
    set(PLATFORM android)
  else()
    set(PLATFORM png)
  endif()
endif(NOT PLATFORM)

add_subdirectory(framework)
add_subdirectory(platform)
add_subdirectory(stub)

# set(sources-os) if (UNIX AND (NOT ANDROID)) set(sources-os
# platform/os/linux.cpp platform/os/linux.hpp) elseif(WIN32) set(sources-os
# platform/os/windows.cpp platform/os/windows.hpp) endif(UNIX AND (NOT ANDROID))
#
# add_library(vulkan-sdk STATIC ${sources-os}) target_link_libraries(vulkan-sdk
# platform) if (UNIX) target_link_libraries(vulkan-sdk -ldl -pthread)
# endif(UNIX)

# add_sample(hellotriangle hellotriangle.cpp) For Android, always emit
# libnative.so since we only build one sample per APK. Otherwise, we want to
# control our output folder so we can pick up assets without any problems.
set(TARGET hellotriangle)
set(SOURCES hellotriangle.cpp)
if(ANDROID)
  add_library(${TARGET} SHARED ${SOURCES})
  target_link_libraries(${TARGET} platform)
  set_target_properties(${TARGET} PROPERTIES LIBRARY_OUTPUT_NAME native)
else(ANDROID)
  add_executable(${TARGET} ${SOURCES})
  target_link_libraries(${TARGET} platform)
  set_target_properties(
    ${TARGET} PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                         "${CMAKE_BINARY_DIR}/samples/${TARGET}")
endif(ANDROID)
