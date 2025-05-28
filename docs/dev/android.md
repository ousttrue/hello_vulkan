## cmake

| Name         | Version                     | path                                | note            |
| ------------ | --------------------------- | ----------------------------------- | --------------- |
| Build tools  | 35.0.1                      | %ANDROID_HOME%/build-tools/35.0.1   |                 |
| SDK platform | Android-14.0 / API Level 34 | %ANDROID_HOME%/platforms/android-34 |                 |
| NDK          | 29.0.13113456               | %ANDROID_HOME%/ndk/29.0.13113456    | cmake toolchain |

```sh
# https://github.com/bjornbytes/lovr/blob/dev/.github/workflows/build.yml
> cmake ..
          -D CMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake
          -D ANDROID_SDK=$ANDROID_HOME
          -D ANDROID_ABI=${{ matrix.abi }}
          -D ANDROID_STL=c++_shared
          -D ANDROID_NATIVE_API_LEVEL=29
          -D ANDROID_BUILD_TOOLS_VERSION=34.0.0
          -D ANDROID_KEYSTORE=key.keystore
          -D ANDROID_KEYSTORE_PASS=pass:hunter2
          -D LOVR_VERSION_HASH=${GITHUB_SHA::6}
```

## NativeActivity

### android_native_app_glue

https://developer.android.com/reference/games/game-activity/group/android-native-app-glue
