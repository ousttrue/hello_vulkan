/* Copyright (c) 2016-2017, ARM Limited and Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
 * OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef PLATFORM_ASSET_MANAGER_HPP
#define PLATFORM_ASSET_MANAGER_HPP

#include "common.hpp"
#include <stddef.h>
#include <stdint.h>
#include <vector>

#include <android_native_app_glue.h>

namespace MaliSDK {

/// @brief The asset manager reads data from a platform specific location.
/// This class is used internally to load binary data from disk.
class AssetManager {
public:
  ~AssetManager() = default;

  template <typename T>
  inline std::vector<T> readBinaryFile(const char *pPath) {
    auto raw = readBinaryFile(pPath);
    if (raw.empty() != RESULT_SUCCESS) {
      return {};
    }
    size_t numElements = raw.size() / sizeof(T);
    std::vector<T> buffer(numElements);
    memcpy(buffer.data(), raw.data(), raw.size());
    return buffer;
  }

  std::vector<uint8_t> readBinaryFile(const char *pPath);

  /// @brief Sets the asset manager to use. Called from platform.
  /// @param pAssetManager The asset manager.
  void setAssetManager(AAssetManager *pAssetManager) {
    pManager = pAssetManager;
  }

private:
  AAssetManager *pManager = nullptr;
};

} // namespace MaliSDK

#endif
