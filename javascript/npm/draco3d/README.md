
<p align="center">
<img src="https://github.com/google/draco/raw/master/docs/DracoLogo.jpeg" />
</p>

News
=======
### Version 1.2.5 release
* On average 10% faster decoding
* Improved Javascript metadata API
* Bug fixes

### Version 1.2.4 release
* Up to 20% faster decoding
* Added support for integer attributes to our Javascript Encoder
* Fixed issues with THREE.DracoLoader not releasing memory associated with the Draco module
* OBJ decoder can now be used to parse pure point clouds
* Added Unity plugins to support runtime loading and design-time importing of encoded Draco files

### Version 1.2.3 release
* Fixed Visual Studio building issue

### Version 1.2.2 release
The latest version of Draco brings a number of small bug fixes
* Fixed issues when parsing ill-formatted .obj files

### Version 1.2.1 release
The latest version of Draco brings a number of enhancements to reduce decoder size and various other fixes
* Javascript and WebAssembly decoder size reduced by 35%
* Added specialized Javascript and Webassembly decoders for GLTF (size reduction about 50% compared to the previous version)

Description
===========

[Draco] is a library for compressing and decompressing 3D geometric [meshes] and
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

[Draco] is released as C++/Javascript source code that can be used to compress 3D
graphics as well as decoders for the encoded data.

NPM Package
===========

The code shows a simple example of using Draco encoder and decoder with Node.js.
`draco_encoder_node.js` and `draco_decoder_node.js` are modified Javascript
encoding/decoding files that are compatible with Node.js.
`draco_nodejs_example.js` has the example code for usage.

How to run the code:

(1) Install draco3d package :

~~~~~ bash
$ npm install draco3d
~~~~~

(2) Run example code to test:

~~~~~ bash
$ cp node_modules/draco3d/draco_nodejs_example.js .
$ cp node_modules/draco3d/bunny.drc .
$ node draco_nodejs_example.js
~~~~~

The code loads the [Bunny] model, it will first decode to a mesh
and then encode it with different settings.

References
==========
[Draco]: https://github.com/google/draco
[meshes]: https://en.wikipedia.org/wiki/Polygon_mesh
[point clouds]: https://en.wikipedia.org/wiki/Point_cloud
[Bunny]: https://graphics.stanford.edu/data/3Dscanrep/

Bunny model from Stanford's graphic department <https://graphics.stanford.edu/data/3Dscanrep/>
