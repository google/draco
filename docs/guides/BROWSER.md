# Using Draco in the Browser
This guide will show you how to use Draco files from the browser.

## JavaScript Decode

This is an example to show how to use the Draco JavaScript deocder to
decode Draco files in the browser.

This is the minimal amount of code to decode a Draco file:
~~~
// Create the Draco decoder.
const decoderModule = DracoDecoderModule();
const buffer = new decoderModule.DecoderBuffer();
buffer.Init(byteArray, byteArray.length);

// Create a buffer to hold the encoded data.
const decoder = new decoderModule.Decoder();
const geometryType = decoder.GetEncodedGeometryType(buffer);

// Decode the encoded geometry.
let outputGeometry;
let status;
if (geometryType == decoderModule.TRIANGULAR_MESH) {
  outputGeometry = new decoderModule.Mesh();
  status = decoder.DecodeBufferToMesh(buffer, outputGeometry);
} else {
  outputGeometry = new decoderModule.PointCloud();
  status = decoder.DecodeBufferToPointCloud(buffer, outputGeometry);
}

// You must explicitly delete objects created from the DracoDecoderModule
// or Decoder.
decoderModule.destroy(outputGeometry);
decoderModule.destroy(decoder);
decoderModule.destroy(buffer);
~~~

Create a web page to decode a Draco file and just output the number of
points decoded. Save this file as `DracoDecode.html`.
~~~
<!DOCTYPE html>
<html>
<head>
  <title>Draco Decoder - Simple</title>
  <script src="https://rawgit.com/google/draco/master/javascript/draco_decoder.js"></script>
  <script>
    'use strict';

    // Decode an encoded Draco mesh. byteArray is the encoded mesh as
    // an Uint8Array.
    function decodeMesh(byteArray) {
      // Create the Draco decoder.
      const decoderModule = DracoDecoderModule();
      const buffer = new decoderModule.DecoderBuffer();
      buffer.Init(byteArray, byteArray.length);

      // Create a buffer to hold the encoded data.
      const decoder = new decoderModule.Decoder();
      const geometryType = decoder.GetEncodedGeometryType(buffer);

      // Decode the encoded geometry.
      let outputGeometry;
      let status;
      if (geometryType == decoderModule.TRIANGULAR_MESH) {
        outputGeometry = new decoderModule.Mesh();
        status = decoder.DecodeBufferToMesh(buffer, outputGeometry);
      } else {
        outputGeometry = new decoderModule.PointCloud();
        status = decoder.DecodeBufferToPointCloud(buffer, outputGeometry);
      }

      alert('Num points = ' + outputGeometry.num_points());

      // You must explicitly delete objects created from the DracoDecoderModule
      // or Decoder.
      decoderModule.destroy(outputGeometry);
      decoderModule.destroy(decoder);
      decoderModule.destroy(buffer);
    }

    // Download and decode the Draco encoded geometry.
    function downloadEncodedMesh(filename) {
      // Download the encoded file.
      const xhr = new XMLHttpRequest();
      xhr.open("GET", filename, true);
      xhr.responseType = "arraybuffer";

      xhr.onload = function(event) {
        const arrayBuffer = xhr.response;
        if (arrayBuffer) {
          const byteArray = new Uint8Array(arrayBuffer);
          decodeMesh(byteArray);
        }
      };

      xhr.send(null);
    }

    downloadEncodedMesh('bunny.drc');
  </script>
</head>
<body>
</body>
</html>
~~~

Copy `bunny.drc` to the same folder as `DracoDecode.html`. Serve `DracoDecode.html` from a webserver, such as
```
python -m SimpleHTTPServer
```

Open `DracoDecode.html` in a browser, you should see an alert message with 34834 points decoded.

