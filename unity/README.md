Description
===========

This folder contains resources for using Draco within Unity development.
Currently we support two types of usages:
* Import Draco compressed mesh as assets during design time.
* Load/decode Draco files in runtime.

Prerequisite
============

To start, you need to have the Draco unity plugin. You can either use the
prebuilt libraries provided in this folder or build from source.
Note that the plugin library for different platforms has different file extension.

| Platform | Library name |
| -------- | ------------ |
| Mac OS | dracodec_unity.bundle |
| Android | libdracodec_unity.so |
| Windows | dracodec_unity.dll |

Prebuilt Library
----------------

We have built library for several platforms:

| Platform | Tested Environment |
| -------- | ------------------ |
| .bundle  | macOS Sierra + Xcode 8.3.3 |
| armeabi-v7a(.so) | Android 8.1.0 |
| .dll | Win10 + Visual Studio 2017 |

Build From Source
-----------------
You can build the plugins on your own for OSX/Win/Android. Source code for the wrapper is here: [src/draco/unity/](../src/draco/unity). Following is detailed building instruction.

Mac OS X
--------
On Mac OS X, run the following command to generate Xcode projects. It is the same as building Draco but with the addition of `-DBUILD_UNITY_PLUGIN=ON` flag:

~~~~~ bash
$ cmake path/to/draco -G Xcode -DBUILD_UNITY_PLUGIN=ON
~~~~~

Then open the project use Xcode and build.
You should be able to find the library under:

~~~~ bash
path/to/build/Debug(or Release)/dracodec_unity.bundle
~~~~

Windows
-------
Similar to OS X build, you need to build Draco with the additional `-DBUILD_UNITY_PLUGIN=ON` flag, for example:

~~~~~ bash
C:\Users\nobody> cmake path/to/draco -G "Visual Studio 14 2015" -DBUILD_UNITY_PLUGIN=ON
~~~~~

Or to generate 64-bit version:

~~~~~ bash
C:\Users\nobody> cmake path/to/draco -G "Visual Studio 14 2015 Win64" -DBUILD_UNITY_PLUGIN=ON
~~~~~

Android
-------

You should first follow the steps in [Android Native Builds](../README.md#native-android-builds) to build Draco for Android. Then, to build the plugin for Unity, add the following text to your cmake command line "-DBUILD_UNITY_PLUGIN=ON".

~~~~ bash
cmake path/to/draco -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/armv7-android-ndk-libcpp.cmake -DBUILD_UNITY_PLUGIN=ON
~~~~

Change file extension
---------------------
Because Unity can only recognize file extensions known to Unity, you need to change your compressed .drc file to .bytes so that Unity will recognize it as a binary file. For example, if you have file `bunny.drc` then change the file name to `bunny.drc.bytes`.

Copy Library to Your Project
----------------------------
Copy the plugin library to your Unity project in `Assets/Plugins/`.
For Android:

~~~~ bash
cp path/to/your/libdracodec_unity.so path/to/your/Unity/Project/Assets/Plugins/Android/
~~~~

For Mac:

~~~~ bash
cp path/to/your/dracodec_unity.bundle path/to/your/Unity/Project/Assets/Plugins/
~~~~

For Win:

~~~~ bash
cp path/to/your/dracodec_unity.dll path/to/your/Unity/Project/Assets/Plugins/
~~~~


Copy Unity Script to Your Project
---------------------------------
Copy the scripts in this folder to your project.
Copy wrapper:

~~~~ bash
cp DracoDecodingObject.cs path/to/your/Unity/Project/Assets/
cp DracoMeshLoader.cs path/to/your/Unity/Project/Assets/
~~~~

---

Load Draco Assets in Runtime
============================
For example, please see [DracoDecodingObject.cs](DracoDecodingObject.cs) for usage. To start, you can create an empty GameObject and attach this script to it. [DracoDecodingObject.cs](DracoDecodingObject.cs) will load `bunny.drc.bytes` by default.

Enable Library in Script Debugging
----------------------------------
If you have library for the platform you are working on, e.g. `dracodec_unity.bundle` for Mac or `dracodec_unity.dll` for  Windows. You should be able to use the plugin in debugging mode.

---

Import Compressed Draco Assets
==============================
In this section we will describe how to import Draco files (.drc) to Unity as
other 3D formats at design time, e.g. obj, fbx.
Note that importing Draco files doesn't mean the Unity project will export models as Draco files.

Copy [DracoFileImporter.cs](Editor/DracoFileImporter.cs) which enables loading (This file is only used for import Draco files):

~~~~ bash
cp DracoFileImporter.cs path/to/your/Unity/Project/Assets/Editor/
~~~~

If you have followed the previous steps, you just need to copy your asset, e.g. `bunny.drc.bytes`, to `Your/Unity/Project/Assets/Resources`, the project will automatically load the file and add the models to the project.

---
