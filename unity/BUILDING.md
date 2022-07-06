# Building Draco Unity Support

All sections here contain the raw build commands Draco project members use to
produce plugin binaries at release time. This includes the archival steps that
produce the archives in the repository.

### iOS

```bash
cmake path/to/draco -G Xcode \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CONFIGURATION_TYPES=Release \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_ARCHITECTURES=armv7\;armv7s\;arm64 \
  -DCMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH=NO \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=10.0 \
  -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED=NO \
  -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY="" \
  -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO \
  -DDRACO_UNITY_PLUGIN=1

xcodebuild
cd Release-iphoneos
tar cjvf libdracodec_unity_ios.tar.bz *.a
```

### MacOS

```bash
cmake path/to/draco -G Xcode \
  -DDRACO_UNITY_PLUGIN=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CONFIGURATION_TYPES=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64\;x86_64
xcodebuild
cd Release
tar cjvf libdracodec_unity_macos.tar.bz dracodec_unity.bundle
```

### Windows

```bash
cmake ../ -G "Visual Studio 17 2022" -A x64 -DDRACO_UNITY_PLUGIN=ON \
  -DCMAKE_INSTALL_PREFIX=.
cmake --build . --config Release --target install -- /M:36
cd lib
tar cjvf libdracodec_unity_windows.tar.bz dracodec_unity.dll
```

### Android

#### 1. Build the armv7 plugin.

```bash
# NOTE: YOU MUST UPDATE DRACO_ANDROID_NDK_PATH FOR YOUR ENVIRONMENT.
export DRACO_ANDROID_NDK_PATH=$HOME/ndks/android-ndk-r20
mkdir armeabi-v7a && cd armeabi-v7a
cmake ../ \
  -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/armv7-android-ndk-libcpp.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DDRACO_UNITY_PLUGIN=ON -DCMAKE_INSTALL_PREFIX=. \
  -DDRACO_ANDROID_NDK_PATH=${DRACO_ANDROID_NDK_PATH}

make -j install
```

#### 2. Build the arm64 plugin.

```bash
# NOTE: YOU MUST UPDATE DRACO_ANDROID_NDK_PATH FOR YOUR ENVIRONMENT.
export DRACO_ANDROID_NDK_PATH=$HOME/ndks/android-ndk-r20
mkdir arm64-v8a && cd arm64-v8a
cmake ../ \
  -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/arm64-android-ndk-libcpp.cmake \
  -DDRACO_UNITY_PLUGIN=ON -DCMAKE_INSTALL_PREFIX=. \
  -DCMAKE_BUILD_TYPE=Release \
  -DDRACO_ANDROID_NDK_PATH=${DRACO_ANDROID_NDK_PATH}
make -j install
```

#### 3. Archive the plugins.

```bash
tar cjvf libdracodec_unity_android.tar.bz \
  armeabi-v7a/libdracodec_unity.so \
  arm64-v8a/libdracodec_unity.so
```