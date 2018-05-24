# Description
Plugin to add support for Draco files (.drc) in Autodesk Maya.

# Features
The following `features` are offered:
* Support .drc format: TRIANGULAR MESH  (no POINT CLOUD)
* Import .drc file into Maya by "Import menu" and "Drag & Drop"
* Export from Maya into .drc file by "Export Selection menu"

With the following `constraints`:
* Import feature is limited to attributes: Vertices, Normals and Uvs
* Export feature is limited to attributes: Vertices, Normals and Uvs
* Meshes has to be made of Triangular Polygons

| Read [Release doc](./RELEASE.md) for details about what is implemented and what will be done next.

# Supported OS / Maya Version
Currently the plugin has been built for the following `OS`:
* Windows x64

and tested against Maya versions:
* Maya 2017
* Maya 2018

| Note: if you want you can build the plugin for your `OS / architecture`. Refer to `Build from Sources` section

# Installation
## On Windows
1. Copy the folder `draco_maya` under Maya pluging folder.
| E.g.: C:\Users\<USER>\Documents\maya\<VERSION>\plug-ins 
2. Open Maya go to menu:  `Windows -> Settings\Preferences -> Plug-in Manager`
3. Use `Browse` button, point to `draco_maya` folder and select `draco_maya_plugin.py` as plugin file

# Usage
Use the regular Maya Import/Export functionalities.

# Build From Source
You can build the plugins on your own for OSX/Win/Linux. Source code for the wrapper is here: [src/draco/maya/](../src/draco/maya). Following is detailed building instruction.

### Mac OS X
On Mac OS X, run the following command to generate Xcode projects:

~~~~~ bash
$ cmake path/to/draco -G Xcode -DBUILD_MAYA_PLUGIN=ON
~~~~~

Then open the project use Xcode and build.
You should be able to find the library under:

~~~~ bash
path/to/build/Debug(or Release)/draco_maya_wrapper.bundle
~~~~

The makefile generator will also work:

~~~~~ bash
$ cmake path/to/draco -DBUILD_MAYA_PLUGIN=ON && make
~~~~~

`draco_maya_wrapper.bundle` can be found in the directory where you generated
the build files and ran make.

### Windows
On Windows, run the following command to generate Visual Studio projects:

32-bit version:
~~~~~ bash
cmake path/to/draco -G "Visual Studio 15 2017" -DBUILD_MAYA_PLUGIN=ON -DBUILD_SHARED_LIBS=ON
~~~~~

64-bit version:
~~~~~ bash
cmake path/to/draco -G "Visual Studio 15 2017 Win64" -DBUILD_MAYA_PLUGIN=ON -DBUILD_SHARED_LIBS=ON
~~~~~

Then open the project use VS and build.
You should be able to find the library under:

~~~~ bash
path/to/build/Debug(or Release)/draco_maya_wrapper.dll
~~~~

### Linux
On Linux, run the following command to generate a Makefile build and build the
plugin.

~~~~~ bash
$ cmake path/to/draco -DBUILD_MAYA_PLUGIN=1 && make
~~~~~

Note: While the linux build completes successfully, the plugin has not been
tested.
