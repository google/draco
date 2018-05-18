# Description
Plugin which add support for Draco files (.drc) in Autodesk Maya.

# Features
The following feature are offered:
* Support .drc format: TRIANGULAR MESH  (no POINT CLOUD)
* Import .drc file into Maya by Import menu
* Import .drc file into Maya by Drag & Drop
* Export from Maya into .drc file by Export Selection menu

With the following contraints:
* Import attributes limited to: Vertices, Normals and Uvs

# Supported OS / Maya Version
Currently the plugin works on the following OS:
* Windows x64

and tested against Maya versions:
* Maya 2017
* Maya 2018

# Installation
Copy the files draco_maya_plugin.py, draco_maya_wrapper.py and draco_maya_wrapper.dll in the appropriate maya folder
For example on Windows C:\Users\username\Documents\maya\2018\plug-ins for Maya 2018


# Usage
Use the regular Maya import/export functionalities


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
[TODO]

