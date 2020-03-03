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
using System;
using System.Collections;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using Unity.Collections.LowLevel.Unsafe;
using UnityEngine;

public unsafe class DracoMeshLoader
{
  // These values must be exactly the same as the values in draco_types.h.
  // Attribute data type.
  enum DataType {
    DT_INVALID = 0,
    DT_INT8,
    DT_UINT8,
    DT_INT16,
    DT_UINT16,
    DT_INT32,
    DT_UINT32,
    DT_INT64,
    DT_UINT64,
    DT_FLOAT32,
    DT_FLOAT64,
    DT_BOOL
  };

  // These values must be exactly the same as the values in
  // geometry_attribute.h.
  // Attribute type.
  enum AttributeType {
    INVALID = -1,
    POSITION = 0,
    NORMAL,
    COLOR,
    TEX_COORD,
    // A special id used to mark attributes that are not assigned to any known
    // predefined use case. Such attributes are often used for a shader specific
    // data.
    GENERIC
  };

  // The order must be consistent with C++ interface.
  [StructLayout (LayoutKind.Sequential)] public struct DracoData
  {
    public int dataType;
    public IntPtr data;
  }

  [StructLayout (LayoutKind.Sequential)] public struct DracoAttribute
  {
    public int attributeType;
    public int dataType;
    public int numComponents;
    public int uniqueId;
  }

  [StructLayout (LayoutKind.Sequential)] public struct DracoMesh
  {
    public int numFaces;
    public int numVertices;
    public int numAttributes;
  }

  // Release data associated with DracoMesh.
  [DllImport ("dracodec_unity")] private static extern void ReleaseDracoMesh(
      DracoMesh**mesh);
  // Release data associated with DracoAttribute.
  [DllImport ("dracodec_unity")] private static extern void
      ReleaseDracoAttribute(DracoAttribute**attr);
  // Release attribute data.
  [DllImport ("dracodec_unity")] private static extern void ReleaseDracoData(
      DracoData**data);

  // Decodes compressed Draco::Mesh in buffer to mesh. On input, mesh
  // must be null. The returned mesh must released with ReleaseDracoMesh.
  [DllImport ("dracodec_unity")] private static extern int DecodeDracoMesh(
      byte[] buffer, int length, DracoMesh**mesh);

  // Returns the DracoAttribute at index in mesh. On input, attribute must be
  // null. The returned attr must be released with ReleaseDracoAttribute.
  [DllImport ("dracodec_unity")] private static extern bool GetAttribute(
      DracoMesh* mesh, int index, DracoAttribute**attr);
  // Returns the DracoAttribute of type at index in mesh. On input, attribute
  // must be null. E.g. If the mesh has two texture coordinates then
  // GetAttributeByType(mesh, AttributeType.TEX_COORD, 1, &attr); will return
  // the second TEX_COORD attribute. The returned attr must be released with
  // ReleaseDracoAttribute.
  [DllImport ("dracodec_unity")] private static extern bool GetAttributeByType(
      DracoMesh* mesh, AttributeType type, int index, DracoAttribute**attr);
  // Returns the DracoAttribute with unique_id in mesh. On input, attribute
  // must be null.The returned attr must be released with
  // ReleaseDracoAttribute.
  [DllImport ("dracodec_unity")] private static extern bool
      GetAttributeByUniqueId(DracoMesh* mesh, int unique_id,
                             DracoAttribute**attr);

  // Returns an array of indices as well as the type of data in data_type. On
  // input, indices must be null. The returned indices must be released with
  // ReleaseDracoData.
  [DllImport ("dracodec_unity")] private static extern bool GetMeshIndices(
      DracoMesh* mesh, DracoData**indices);
  // Returns an array of attribute data as well as the type of data in
  // data_type. On input, data must be null. The returned data must be
  // released with ReleaseDracoData.
  [DllImport ("dracodec_unity")] private static extern bool GetAttributeData(
      DracoMesh* mesh, DracoAttribute* attr, DracoData**data);

  public int LoadMeshFromAsset(string assetName, ref List<Mesh> meshes)
  {
    TextAsset asset =
        Resources.Load(assetName, typeof(TextAsset)) as TextAsset;
    if (asset == null) {
      Debug.Log ("Didn't load file!");
      return -1;
    }
    byte[] encodedData = asset.bytes;
    Debug.Log(encodedData.Length.ToString());
    if (encodedData.Length == 0) {
      Debug.Log ("Didn't load encoded data!");
      return -1;
    }
    return ConvertDracoMeshToUnity(encodedData, ref meshes);
  }

  // Decodes a Draco mesh, creates a Unity mesh from the decoded data and
  // adds the Unity mesh to meshes. encodedData is the compressed Draco mesh.
  public unsafe int ConvertDracoMeshToUnity(byte[] encodedData,
                                            ref List<Mesh> meshes)
  {
    float startTime = Time.realtimeSinceStartup;
    DracoMesh *mesh = null;
    if (DecodeDracoMesh(encodedData, encodedData.Length, &mesh) <= 0) {
      Debug.Log("Failed: Decoding error.");
      return -1;
    }

    float decodeTimeMilli =
        (Time.realtimeSinceStartup - startTime) * 1000.0f;
    Debug.Log("decodeTimeMilli: " + decodeTimeMilli.ToString());

    Debug.Log("Num indices: " + mesh->numFaces.ToString());
    Debug.Log("Num vertices: " + mesh->numVertices.ToString());
    Debug.Log("Num attributes: " + mesh->numAttributes.ToString());

    Mesh unityMesh = CreateUnityMesh(mesh);
    UnityMeshToCamera(ref unityMesh);
    meshes.Add(unityMesh);

    int numFaces = mesh->numFaces;
    ReleaseDracoMesh(&mesh);
    return numFaces;
  }

  // Creates a Unity mesh from the decoded Draco mesh.
  public unsafe Mesh CreateUnityMesh(DracoMesh *dracoMesh)
  {
    float startTime = Time.realtimeSinceStartup;
    int numFaces = dracoMesh->numFaces;
    int[] newTriangles = new int[dracoMesh->numFaces * 3];
    Vector3[] newVertices = new Vector3[dracoMesh->numVertices];
    Vector2[] newUVs = null;
    Vector3[] newNormals = null;
    Color[] newColors = null;
    byte[] newGenerics = null;

    // Copy face indices.
    DracoData *indicesData;
    GetMeshIndices(dracoMesh, &indicesData);
    int elementSize =
        DataTypeSize((DracoMeshLoader.DataType)indicesData->dataType);
    int *indices = (int*)(indicesData->data);
    var indicesPtr = UnsafeUtility.AddressOf(ref newTriangles[0]);
    UnsafeUtility.MemCpy(indicesPtr, indices,
                         newTriangles.Length * elementSize);
    ReleaseDracoData(&indicesData);

    // Copy positions.
    DracoAttribute *attr = null;
    GetAttributeByType(dracoMesh, AttributeType.POSITION, 0, &attr);
    DracoData* posData = null;
    GetAttributeData(dracoMesh, attr, &posData);
    elementSize = DataTypeSize((DracoMeshLoader.DataType)posData->dataType) *
        attr->numComponents;
    var newVerticesPtr = UnsafeUtility.AddressOf(ref newVertices[0]);
    UnsafeUtility.MemCpy(newVerticesPtr, (void*)posData->data,
                         dracoMesh->numVertices * elementSize);
    ReleaseDracoData(&posData);
    ReleaseDracoAttribute(&attr);

    // Copy normals.
    if (GetAttributeByType(dracoMesh, AttributeType.NORMAL, 0, &attr)) {
      DracoData* normData = null;
      if (GetAttributeData(dracoMesh, attr, &normData)) {
        elementSize =
            DataTypeSize((DracoMeshLoader.DataType)normData->dataType) *
                attr->numComponents;
        newNormals = new Vector3[dracoMesh->numVertices];
        var newNormalsPtr = UnsafeUtility.AddressOf(ref newNormals[0]);
        UnsafeUtility.MemCpy(newNormalsPtr, (void*)normData->data,
                             dracoMesh->numVertices * elementSize);
        Debug.Log("Decoded mesh normals.");
        ReleaseDracoData(&normData);
        ReleaseDracoAttribute(&attr);
      }
    }

    // Copy texture coordinates.
    if (GetAttributeByType(dracoMesh, AttributeType.TEX_COORD, 0, &attr)) {
      DracoData* texData = null;
      if (GetAttributeData(dracoMesh, attr, &texData)) {
        elementSize =
            DataTypeSize((DracoMeshLoader.DataType)texData->dataType) *
            attr->numComponents;
        newUVs = new Vector2[dracoMesh->numVertices];
        var newUVsPtr = UnsafeUtility.AddressOf(ref newUVs[0]);
        UnsafeUtility.MemCpy(newUVsPtr, (void*)texData->data,
                             dracoMesh->numVertices * elementSize);
        Debug.Log("Decoded mesh texcoords.");
        ReleaseDracoData(&texData);
        ReleaseDracoAttribute(&attr);
      }
    }

    // Copy colors.
    if (GetAttributeByType(dracoMesh, AttributeType.COLOR, 0, &attr)) {
      DracoData* colorData = null;
      if (GetAttributeData(dracoMesh, attr, &colorData)) {
        elementSize =
            DataTypeSize((DracoMeshLoader.DataType)colorData->dataType) *
            attr->numComponents;
        newColors = new Color[dracoMesh->numVertices];
        var newColorsPtr = UnsafeUtility.AddressOf(ref newColors[0]);
        UnsafeUtility.MemCpy(newColorsPtr, (void*)colorData->data,
                             dracoMesh->numVertices * elementSize);
        Debug.Log("Decoded mesh colors.");
        ReleaseDracoData(&colorData);
        ReleaseDracoAttribute(&attr);
      }
    }

    // Copy generic data. This script does not do anyhting with the generic
    // data.
    if (GetAttributeByType(dracoMesh, AttributeType.GENERIC, 0, &attr)) {
      DracoData* genericData = null;
      if (GetAttributeData(dracoMesh, attr, &genericData)) {
        elementSize =
            DataTypeSize((DracoMeshLoader.DataType)genericData->dataType) *
                attr->numComponents;
        newGenerics = new byte[dracoMesh->numVertices * elementSize];
        var newGenericPtr = UnsafeUtility.AddressOf(ref newGenerics[0]);
        UnsafeUtility.MemCpy(newGenericPtr, (void*)genericData->data,
                             dracoMesh->numVertices * elementSize);
        Debug.Log("Decoded mesh generic data.");
        ReleaseDracoData(&genericData);
        ReleaseDracoAttribute(&attr);
      }
    }

    float copyDecodedDataTimeMilli =
        (Time.realtimeSinceStartup - startTime) * 1000.0f;
    Debug.Log("copyDecodedDataTimeMilli: " +
              copyDecodedDataTimeMilli.ToString());

    startTime = Time.realtimeSinceStartup;
    Mesh mesh = new Mesh();

#if UNITY_2017_3_OR_NEWER
    mesh.indexFormat = (newVertices.Length > System.UInt16.MaxValue)
        ? UnityEngine.Rendering.IndexFormat.UInt32
        : UnityEngine.Rendering.IndexFormat.UInt16;
#else
    if (newVertices.Length > System.UInt16.MaxValue) {
      throw new System.Exception("Draco meshes with more than 65535 vertices are only supported from Unity 2017.3 onwards.");
    }
#endif

    mesh.vertices = newVertices;
    mesh.SetTriangles(newTriangles, 0, true);
    if (newUVs != null) {
      mesh.uv = newUVs;
    }
    if (newNormals != null) {
      mesh.normals = newNormals;
    } else {
      mesh.RecalculateNormals();
      Debug.Log("Mesh doesn't have normals, recomputed.");
    }
    if (newColors != null) {
      mesh.colors = newColors;
    }

    float convertTimeMilli =
        (Time.realtimeSinceStartup - startTime) * 1000.0f;
    Debug.Log("convertTimeMilli: " + convertTimeMilli.ToString());
    return mesh;
  }

  // Scale and translate the decoded mesh so it will be visible to
  // a new camera's default settings.
  public unsafe void UnityMeshToCamera(ref Mesh mesh)
  {
    float startTime = Time.realtimeSinceStartup;
    mesh.RecalculateBounds();

    float scale = 0.5f / mesh.bounds.extents.x;
    if (0.5f / mesh.bounds.extents.y < scale) {
      scale = 0.5f / mesh.bounds.extents.y;
    }
    if (0.5f / mesh.bounds.extents.z < scale) {
      scale = 0.5f / mesh.bounds.extents.z;
    }

    Vector3[] vertices = mesh.vertices;
    int i = 0;
    while (i < vertices.Length) {
      vertices[i] *= scale;
      i++;
    }

    mesh.vertices = vertices;
    mesh.RecalculateBounds();

    Vector3 translate = mesh.bounds.center;
    translate.x = 0 - mesh.bounds.center.x;
    translate.y = 0 - mesh.bounds.center.y;
    translate.z = 2 - mesh.bounds.center.z;

    i = 0;
    while (i < vertices.Length) {
      vertices[i] += translate;
      i++;
    }
    mesh.vertices = vertices;
    float transformTimeMilli =
        (Time.realtimeSinceStartup - startTime) * 1000.0f;
    Debug.Log("transformTimeMilli: " + transformTimeMilli.ToString());
  }

  private int DataTypeSize(DataType dt) {
    switch (dt) {
      case DataType.DT_INT8:
      case DataType.DT_UINT8:
        return 1;
      case DataType.DT_INT16:
      case DataType.DT_UINT16:
        return 2;
      case DataType.DT_INT32:
      case DataType.DT_UINT32:
        return 4;
      case DataType.DT_INT64:
      case DataType.DT_UINT64:
        return 8;
      case DataType.DT_FLOAT32:
        return 4;
      case DataType.DT_FLOAT64:
        return 8;
      case DataType.DT_BOOL:
        return 1;
      default:
        return -1;
    }
  }
}
