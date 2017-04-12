
<p align="center">
<img src="docs/DracoLogo.jpeg" />
</p>

News
=======
### Version 0.10.0 released
This release brings improved mesh compression and faster decoding in browser:
* On average 10% better compression of triangular meshes (up to 20% for purely
  spatial meshes without any extra attributes).
* Up to 2X faster decoding in browsers with our newly provided WebAssembly
  decoder.
  * Supported in most modern browsers including Chrome, Firefox, and Edge.
  * Decoder size is about 50% smaller compared to the javascript version.
* New version is backward compatible with 0.9.x encoders.
  * Note that 0.10.0 is not forward compatible, i.e., files encoded with 0.10.0
    cannot be decoded with 0.9.x decoders.


Description
===========

Draco is a library for compressing and decompressing 3D geometric [meshes] and
[point clouds]. It is intended to improve the storage and transmission of 3D
graphics.

Draco was designed and built for compression efficiency and speed. The code
supports compressing points, connectivity information, texture coordinates,
color information, normals, and any other generic attributes associated with
geometry. With Draco, applications using 3D graphics can be significantly
smaller without compromising visual fidelity. For users, this means apps can
now be downloaded faster, 3D graphics in the browser can load quicker, and VR
and AR scenes can now be transmitted with a fraction of the bandwidth and
rendered quickly.

Draco is released as C++ source code that can be used to compress 3D graphics
as well as C++ and Javascript decoders for the encoded data.


_**Contents**_

  * [Building](#building)
    * [CMake Basics](#cmake-basics)
    * [Mac OS X](#mac-os-x)
    * [Windows](#windows)
    * [CMake Build Configuration](#cmake-build-config)
      * [Debugging and Optimization](#debugging-and-optimization)
      * [Googletest Integration](#googletest-integration)
      * [Javascript Decoder](#javascript-decoder)
    * [Android Studio Project Integration](#android-studio-project-integration)
  * [Usage](#usage)
    * [Command Line Applications](#command-line-applications)
    * [Encoding Tool](#encoding-tool)
    * [Encoding Point Clouds](#encoding-point-clouds)
    * [Decoding Tool](#decoding-tool)
    * [C++ Decoder API](#c-decoder-api)
    * [Javascript Decoder API](#javascript-decoder-api)
    * [Javascript Decoder Performance](#javascript-decoder-performance)
    * [three.js Renderer Example](#threejs-renderer-example)
  * [Support](#support)
  * [License](#license)
  * [References](#references)


Building
========
For all platforms, you must first generate the project/make files and then
compile the examples.

CMake Basics
------------

To generate project/make files for the default toolchain on your system, run
`cmake` from a directory where you would like to generate build files, and pass
it the path to your Draco repository.

~~~~~ bash
$ cmake path/to/draco
~~~~~

On Windows, the above command will produce Visual Studio project files for the
newest Visual Studio detected on the system. On Mac OS X and Linux systems,
the above command will produce a `makefile`.

To control what types of projects are generated, add the `-G` parameter to the
`cmake` command. This argument must be followed by the name of a generator.
Running `cmake` with the `--help` argument will list the available
generators for your system.

Mac OS X
---------

On Mac OS X, run the following command to generate Xcode projects:

~~~~~ bash
$ cmake path/to/draco -G Xcode
~~~~~

Windows
-------

On a Windows box you would run the following command to generate Visual Studio
2015 projects:

~~~~~ bash
C:\Users\nobody> cmake path/to/draco -G "Visual Studio 14 2015"
~~~~~

To generate 64-bit Windows Visual Studio 2015 projects:

~~~~~ bash
C:\Users\nobody> cmake path/to/draco -G "Visual Studio 14 2015 Win64"
~~~~~


CMake Build Configuration
-------------------------

Debugging and Optimization
--------------------------

Unlike Visual Studio and Xcode projects, the build configuration for make
builds is controlled when you run `cmake`. The following examples demonstrate
various build configurations.

Omitting the build type produces makefiles that use release build flags
by default:

~~~~~ bash
$ cmake path/to/draco
~~~~~

A makefile using release (optimized) flags is produced like this:

~~~~~ bash
$ cmake path/to/draco -DCMAKE_BUILD_TYPE=release
~~~~~

A release build with debug info can be produced as well:

~~~~~ bash
$ cmake path/to/draco -DCMAKE_BUILD_TYPE=relwithdebinfo
~~~~~

And your standard debug build will be produced using:

~~~~~ bash
$ cmake path/to/draco -DCMAKE_BUILD_TYPE=debug
~~~~~


Googletest Integration
----------------------

Draco includes testing support built using Googletest. To enable Googletest unit
test support the ENABLE_TESTS cmake variable must be turned on at cmake
generation time:

~~~~~ bash
$ cmake path/to/draco -DENABLE_TESTS=ON
~~~~~

When cmake is used as shown in the above example the Draco cmake file assumes
that the Googletest source directory is a sibling of the Draco repository. To
change the location to something else use the GTEST_SOURCE_DIR cmake variable:

~~~~~ bash
$ cmake path/to/draco -DENABLE_TESTS=ON -DGTEST_SOURCE_DIR=path/to/googletest
~~~~~

To run the tests just execute `draco_tests` from your toolchain's build output
directory.


Javascript Decoder
------------------

The javascript decoder can be built using the existing cmake build file by
passing the path the Emscripten's cmake toolchain file at cmake generation time
in the CMAKE_TOOLCHAIN_FILE variable.
In addition, the EMSCRIPTEN environment variable must be set to the local path
of the parent directory of the Emscripten tools directory.

~~~~~ bash
# Make the path to emscripten available to cmake.
$ export EMSCRIPTEN=/path/to/emscripten/tools/parent

# Emscripten.cmake can be found within your Emscripten installation directory,
# it should be the subdir: cmake/Modules/Platform/Emscripten.cmake
$ cmake path/to/draco -DCMAKE_TOOLCHAIN_FILE=/path/to/Emscripten.cmake

# Build the Javascript decoder.
$ make
~~~~~

WebAssembly Decoder
-------------------

The WebAssembly decoder can be built using the existing cmake build file by
passing the path the Emscripten's cmake toolchain file at cmake generation time
in the CMAKE_TOOLCHAIN_FILE variable and enabling the WASM build option.
In addition, the EMSCRIPTEN environment variable must be set to the local path
of the parent directory of the Emscripten tools directory.

Make sure to have the correct version of Emscripten installed for WebAssembly
builds. See https://developer.mozilla.org/en-US/docs/WebAssembly.

~~~~~ bash
# Make the path to emscripten available to cmake.
$ export EMSCRIPTEN=/path/to/emscripten/tools/parent

# Emscripten.cmake can be found within your Emscripten installation directory,
# it should be the subdir: cmake/Modules/Platform/Emscripten.cmake
$ cmake path/to/draco -DCMAKE_TOOLCHAIN_FILE=/path/to/Emscripten.cmake -DENABLE_WASM=ON

# Build the WebAssembly decoder.
$ make

# Run the Javascript wrapper through Closure.
$ java -jar closure.jar --compilation_level SIMPLE --js draco_decoder.js --js_output_file draco_wasm_wrapper.js

~~~~~

WebAssembly Mesh Only Decoder
-----------------------------

~~~~~ bash

# cmake command line for mesh only WebAssembly decoder.
$ cmake path/to/draco -DCMAKE_TOOLCHAIN_FILE=/path/to/Emscripten.cmake -DENABLE_WASM=ON -DENABLE_POINT_CLOUD_COMPRESSION=OFF

~~~~~

WebAssembly Point Cloud Only Decoder
-----------------------------

~~~~~ bash

# cmake command line for point cloud only WebAssembly decoder.
$ cmake path/to/draco -DCMAKE_TOOLCHAIN_FILE=/path/to/Emscripten.cmake -DENABLE_WASM=ON -DENABLE_MESH_COMPRESSION=OFF

~~~~~


Android Studio Project Integration
----------------------------------

To include Draco in an existing or new Android Studio project, reference it
from the `cmake` file of an existing native project that has a minimum SDK
version of 18 or higher. To add Draco to your project:

  1. Add the following somewhere within the `CMakeLists.txt` for your project
     before the `add_library()` for your project's native-lib:

     ~~~~~ cmake
     # Note "/path/to/draco" must be changed to the path where you have cloned
     # the Draco sources.

     add_subdirectory(/path/to/draco
                      ${CMAKE_BINARY_DIR}/draco_build)
     include_directories("${CMAKE_BINARY_DIR}" /path/to/draco)
     ~~~~~

  2. Add the library target "draco" to the `target_link_libraries()` call for
     your project's native-lib. The `target_link_libraries()` call for an
     empty activity native project looks like this after the addition of
     Draco:

     ~~~~~ cmake
     target_link_libraries( # Specifies the target library.
                            native-lib

                            # Tells cmake this build depends on libdraco.
                            draco

                            # Links the target library to the log library
                            # included in the NDK.
                            ${log-lib} )
     ~~~~~

Usage
======

Command Line Applications
------------------------

The default target created from the build files will be the `draco_encoder`
and `draco_decoder` command line applications. For both applications, if you
run them without any arguments or `-h`, the applications will output usage and
options.

Encoding Tool
-------------

`draco_encoder` will read OBJ or PLY files as input, and output Draco-encoded
files. We have included Stanford's [Bunny] mesh for testing. The basic command
line looks like this:

~~~~~ bash
./draco_encoder -i testdata/bun_zipper.ply -o out.drc
~~~~~

A value of `0` for the quantization parameter will not perform any quantization
on the specified attribute. Any value other than `0` will quantize the input
values for the specified attribute to that number of bits. For example:

~~~~~ bash
./draco_encoder -i testdata/bun_zipper.ply -o out.drc -qp 14
~~~~~

will quantize the positions to 14 bits (default for the position coordinates).

In general, the more you quantize your attributes the better compression rate
you will get. It is up to your project to decide how much deviation it will
tolerate. In general, most projects can set quantizations values of about `14`
without any noticeable difference in quality.

The compression level (`-cl`) parameter turns on/off different compression
features.

~~~~~ bash
./draco_encoder -i testdata/bun_zipper.ply -o out.drc -cl 8
~~~~~

In general, the highest setting, `10`, will have the most compression but
worst decompression speed. `0` will have the least compression, but best
decompression speed. The default setting is `5`.

Encoding Point Clouds
---------------------

You can encode point cloud data with `draco_encoder` by specifying the
`point_cloud parameter`. If you specify the `point_cloud parameter` with a mesh
input file, `draco_encoder` will ignore the connectivity data and encode the
positions from the mesh file.

~~~~~ bash
./draco_encoder -point_cloud -i testdata/bun_zipper.ply -o out.drc
~~~~~

This command line will encode the mesh input as a point cloud, even though the
input might not produce compression that is representative of other point
clouds. Specifically, one can expect much better compression rates for larger
and denser point clouds.

Decoding Tool
-------------

`draco_decoder` will read Draco files as input, and output OBJ or PLY files.
The basic command line looks like this:

~~~~~ bash
./draco_decoder -i in.drc -o out.obj
~~~~~

C++ Decoder API
-------------

If you'd like to add decoding to your applications you will need to include
the `draco_dec` library. In order to use the Draco decoder you need to
initialize a `DecoderBuffer` with the compressed data. Then call
`DecodeMeshFromBuffer()` to return a decoded mesh object or call
`DecodePointCloudFromBuffer()` to return a decoded `PointCloud` object. For
example:

~~~~~ cpp
draco::DecoderBuffer buffer;
buffer.Init(data.data(), data.size());

const draco::EncodedGeometryType geom_type =
    draco::GetEncodedGeometryType(&buffer);
if (geom_type == draco::TRIANGULAR_MESH) {
  unique_ptr<draco::Mesh> mesh = draco::DecodeMeshFromBuffer(&buffer);
} else if (geom_type == draco::POINT_CLOUD) {
  unique_ptr<draco::PointCloud> pc = draco::DecodePointCloudFromBuffer(&buffer);
}
~~~~~

Please see `mesh/mesh.h` for the full Mesh class interface and
`point_cloud/point_cloud.h` for the full `PointCloud` class interface.

Javascript Decoder API
----------------------

The Javascript decoder is located in `javascript/draco_decoder.js`. The
Javascript decoder can decode mesh and point cloud. In order to use the
decoder, you must first create an instance of `DracoModule`. The instance is
then used to create `DecoderBuffer` and `WebIDLWrapper` objects. Set
the encoded data in the `DecoderBuffer`. Then call `GetEncodedGeometryType()`
to identify the type of geometry, e.g. mesh or point cloud. Then call either
`DecodeMeshFromBuffer()` or `DecodePointCloudFromBuffer()`, which will return
a Mesh object or a point cloud. For example:

~~~~~ js
const dracoDecoder = DracoModule();
const buffer = new dracoDecoder.DecoderBuffer();
buffer.Init(encFileData, encFileData.length);

const wrapper = new dracoDecoder.WebIDLWrapper();
const geometryType = wrapper.GetEncodedGeometryType(buffer);
let outputGeometry;
if (geometryType == dracoDecoder.TRIANGULAR_MESH) {
  outputGeometry = wrapper.DecodeMeshFromBuffer(buffer);
} else {
  outputGeometry = wrapper.DecodePointCloudFromBuffer(buffer);
}

dracoDecoder.destroy(outputGeometry);
dracoDecoder.destroy(wrapper);
dracoDecoder.destroy(buffer);
~~~~~

Please see `javascript/emscripten/draco_web.idl` for the full API.

Javascript Decoder Performance
------------------------------

The Javascript decoder is built with dynamic memory. This will let the decoder
work with all of the compressed data. But this option is not the fastest.
Pre-allocating the memory sees about a 2x decoder speed improvement. If you
know all of your project's memory requirements, you can turn on static memory
by changing `Makefile.emcc` and running `make -f Makefile.emcc`.

three.js Renderer Example
-------------------------

Here's an [example] of a geometric compressed with Draco loaded via a
Javascript decoder using the `three.js` renderer.

Please see the `javascript/example/README` file for more information.

Support
=======

For questions/comments please email <draco-3d-discuss@googlegroups.com>

If you have found an error in this library, please file an issue at
<https://github.com/google/draco/issues>

Patches are encouraged, and may be submitted by forking this project and
submitting a pull request through GitHub. See [CONTRIBUTING] for more detail.

License
=======
Licensed under the Apache License, Version 2.0 (the "License"); you may not
use this file except in compliance with the License. You may obtain a copy of
the License at

<http://www.apache.org/licenses/LICENSE-2.0>

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
License for the specific language governing permissions and limitations under
the License.

References
==========
[example]:https://storage.googleapis.com/demos.webmproject.org/draco/draco_loader_throw.html
[meshes]: https://en.wikipedia.org/wiki/Polygon_mesh
[point clouds]: https://en.wikipedia.org/wiki/Point_cloud
[Bunny]: https://graphics.stanford.edu/data/3Dscanrep/
[CONTRIBUTING]: https://raw.githubusercontent.com/google/draco/master/CONTRIBUTING

Bunny model from Stanford's graphic department <https://graphics.stanford.edu/data/3Dscanrep/>
