// Copyright 2017 The Draco Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
'use_strict';

const fs = require('fs');
const draco3d = require('./draco3d');
const decoderModule = draco3d.createDecoderModule({});
const encoderModule = draco3d.createEncoderModule({});

fs.readFile('./bunny.drc', function(err, data) {
  if (err) {
    return console.log(err);
  }
  console.log("Decoding file of size " + data.byteLength + " ..");
  // Decode mesh
  const decoder = new decoderModule.Decoder();
  let decodedGeometry = decodeDracoData(data, decoder);
  // Encode mesh
  encodeMeshToFile(decodedGeometry, decoder);

  decoderModule.destroy(decoder);
  decoderModule.destroy(decodedGeometry);
});

function decodeDracoData(rawBuffer, decoder) {
  const buffer = new decoderModule.DecoderBuffer();
  buffer.Init(new Int8Array(rawBuffer), rawBuffer.byteLength);
  const geometryType = decoder.GetEncodedGeometryType(buffer);

  let dracoGeometry;
  let status;
  if (geometryType === decoderModule.TRIANGULAR_MESH) {
    dracoGeometry = new decoderModule.Mesh();
    status = decoder.DecodeBufferToMesh(buffer, dracoGeometry);
  } else if (geometryType === decoderModule.POINT_CLOUD) {
    dracoGeometry = new decoderModule.PointCloud();
    status = decoder.DecodeBufferToPointCloud(buffer, dracoGeometry);
  } else {
    const errorMsg = 'Error: Unknown geometry type.';
    console.error(errorMsg);
  }
  decoderModule.destroy(buffer);

  return dracoGeometry;
}

function encodeMeshToFile(mesh, decoder) {
  const encoder = new encoderModule.Encoder();
  const meshBuilder = new encoderModule.MeshBuilder();
  // Create a mesh object for storing mesh data.
  const newMesh = new encoderModule.Mesh();

  const numFaces = mesh.num_faces();
  const numIndices = numFaces * 3;
  const numPoints = mesh.num_points();
  const numVertexCoord = numPoints * 3;
  console.log("Number of faces " + numFaces);
  console.log("Number of vertices " + numPoints);
  let indices = new Uint32Array(numIndices);

  // Add Faces to mesh
  const ia = new decoderModule.DracoInt32Array();
  for (let i = 0; i < numFaces; ++i) {
    decoder.GetFaceFromMesh(mesh, i, ia);
    const index = i * 3;
    indices[index] = ia.GetValue(0);
    indices[index + 1] = ia.GetValue(1);
    indices[index + 2] = ia.GetValue(2);
  }
  decoderModule.destroy(ia);
  meshBuilder.AddFacesToMesh(newMesh, numFaces, indices);

  // Add position data to mesh.
  const posAttId = decoder.GetAttributeId(mesh, decoderModule.POSITION);
  if (posAttId === -1) {
    console.log("No position attribute found.");
    encoderModule.destroy(newMesh);
    encoderModule.destroy(encoder);
    return;
  }
  const posAttribute = decoder.GetAttribute(mesh, posAttId);
  const posAttributeData = new decoderModule.DracoFloat32Array();
  decoder.GetAttributeFloatForAllPoints(
      mesh, posAttribute, posAttributeData);
  let vertices = new Float32Array(numVertexCoord);
  for (let i = 0; i < numVertexCoord; i += 3) {
    vertices[i] = posAttributeData.GetValue(i);
    vertices[i + 1] = posAttributeData.GetValue(i + 1);
    vertices[i + 2] = posAttributeData.GetValue(i + 2);
  }
  decoderModule.destroy(posAttributeData);
  meshBuilder.AddFloatAttributeToMesh(newMesh, encoderModule.POSITION,
                                      numPoints, 3, vertices);

  const normalAttId = decoder.GetAttributeId(mesh, decoderModule.NORMAL);
  if (normalAttId > -1) {
    console.log("Adding normal attribute.");
    const norAttribute = decoder.GetAttribute(mesh, normalAttId);
    const norAttributeData = new decoderModule.DracoFloat32Array();
    decoder.GetAttributeFloatForAllPoints(mesh, norAttribute,
        norAttributeData);
    const normals = new Float32Array(numVertexCoord);
    for (let i = 0; i < numVertexCoord; i += 3) {
      normals[i] = norAttributeData.GetValue(i);
      normals[i + 1] = norAttributeData.GetValue(i + 1);
      normals[i + 2] = norAttributeData.GetValue(i + 2);
    }
    decoderModule.destroy(norAttributeData);
    meshBuilder.AddFloatAttributeToMesh(newMesh, encoderModule.NORMAL,
        numPoints, 3, normals);
  }

  const texAttId = decoder.GetAttributeId(mesh, decoderModule.TEX_COORD);
  if (texAttId > -1) {
    const texAttribute = decoder.GetAttribute(mesh, texAttId);
    const texAttributeData = new decoderModule.DracoFloat32Array();
    decoder.GetAttributeFloatForAllPoints(mesh, texAttribute,
        texAttributeData);
    assertEquals(numVertexCoord, texAttributeData.size());
    const texcoords = new Float32Array(numVertexCoord);
    for (let i = 0; i < numVertexCoord; i += 3) {
      texcoords[i] = texAttributeData.GetValue(i);
      texcoords[i + 1] = texAttributeData.GetValue(i + 1);
      texcoords[i + 2] = texAttributeData.GetValue(i + 2);
    }
    decoderModule.destroy(texAttributeData);
    meshBuilder.AddFloatAttributeToMesh(newMesh, encoderModule.TEX_COORD,
        numPoints, 3, normals);
  }

  const colorAttId = decoder.GetAttributeId(mesh, decoderModule.COLOR);
  if (colorAttId > -1) {
    const colAttribute = decoder.GetAttribute(mesh, colorAttId);
    const colAttributeData = new decoderModule.DracoFloat32Array();
    decoder.GetAttributeFloatForAllPoints(mesh, colAttribute,
        colAttributeData);
    assertEquals(numVertexCoord, colAttributeData.size());
    const colors = new Float32Array(numVertexCoord);
    for (let i = 0; i < numVertexCoord; i += 3) {
      colors[i] = colAttributeData.GetValue(i);
      colors[i + 1] = colAttributeData.GetValue(i + 1);
      colors[i + 2] = colAttributeData.GetValue(i + 2);
    }
    decoderModule.destroy(colAttributeData);
    meshBuilder.AddFloatAttributeToMesh(newMesh, encoderModule.COLOR,
        numPoints, 3, normals);
  }

  let encodedData = new encoderModule.DracoInt8Array();
  // Set encoding options.
  encoder.SetSpeedOptions(5, 5);
  encoder.SetAttributeQuantization(encoderModule.POSITION, 10);
  encoder.SetEncodingMethod(encoderModule.MESH_EDGEBREAKER_ENCODING);

  // Encoding.
  console.log("Encoding...");
  const encodedLen = encoder.EncodeMeshToDracoBuffer(newMesh,
                                                     encodedData);
  encoderModule.destroy(newMesh);

  if (encodedLen > 0) {
    console.log("Encoded size is " + encodedLen);
  } else {
    console.log("Error: Encoding failed.");
  }
  // Copy encoded data to buffer.
  let outputBuffer = new ArrayBuffer(encodedLen);
  let outputData = new Int8Array(outputBuffer);
  for (let i = 0; i < encodedLen; ++i) {
    outputData[i] = encodedData.GetValue(i);
  }
  encoderModule.destroy(encodedData);
  encoderModule.destroy(encoder);
  encoderModule.destroy(meshBuilder);
  // Write to file. You can view the the file using webgl_loader_draco.html
  // example.
  fs.writeFile("bunny_10.drc", Buffer(outputBuffer), "binary", function(err) {
    if(err) {
        console.log(err);
    } else {
        console.log("The file was saved!");
    }
  });
}

