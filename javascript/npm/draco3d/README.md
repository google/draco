
<p align="center">
<img src="https://github.com/google/draco/raw/master/docs/DracoLogo.jpeg" />
</p>

News
=======
### Version 1.0.0 release
The latest version of Draco brings many new enhancements to improve the
development experience:
* Stable API release
* Support for npm Javascript package management
* Javascript based encoder
* Generalized metadata for meshes and point clouds
  * Now supporting material properties included along with encoded file
* Improved compression rates:
  * 15% better compression on smaller models
  * 40% better compression of normals
* Performance improvements (~10% faster encoding, decoding)
* Reduced GPU memory usage:
  * Option to store decoded quantized attributes
  * Support for triangle strip connectivity on decoded meshes
* iOS 9 Javascript decoder
* Bitstream specification now available

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
