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
using UnityEngine;

public unsafe class DracoMeshLoader
{
	// Must stay the order to be consistent with C++ interface.
	[StructLayout (LayoutKind.Sequential)] private struct DracoToUnityMesh
	{
		public int numFaces;
		public IntPtr indices;
		public int numVertices;
		public IntPtr position;
		public bool hasNormal;
		public IntPtr normal;
		public bool hasTexcoord;
		public IntPtr texcoord;
		public bool hasColor;
		public IntPtr color;
	}

	private struct DecodedMesh
	{
		public int[] faces;
		public Vector3[] vertices;
		public Vector3[] normals;
		public Vector2[] uvs;
		public Color[] colors;
	}

	[DllImport ("dracodec_unity")] private static extern int DecodeMeshForUnity (
		byte[] buffer, int length, DracoToUnityMesh**tmpMesh);

	static private int maxNumVerticesPerMesh = 60000;

	// Unity only support maximum 65534 vertices per mesh. So large meshes need
	// to be splitted.
	private void SplitMesh (DecodedMesh mesh, ref List<DecodedMesh> splittedMeshes)
	{
		int[] IndexNew = new int[maxNumVerticesPerMesh];
        int BaseIndex = 0;
        int FaceCount = mesh.faces.Length;
        int[] FaceIndex = new int[FaceCount];
        int[] NewFace = new int[FaceCount];


        for (int i = 0; i < FaceCount; i++)
        {
            FaceIndex[i] = -1;
        }

        while (BaseIndex < FaceCount)
        {
            int uniqueCornerId = 0;
            int UseIndex = 0;
            int AddNew = 0;
            int[] NewCorner = new int[3];
            for (; BaseIndex + UseIndex < FaceCount;)
            {
                AddNew = 0;
                for (int i = 0; i < 3; i++)
                {
                    if (FaceIndex[mesh.faces[BaseIndex + UseIndex + i]] == -1)
                    {
                        NewCorner[AddNew] = mesh.faces[BaseIndex + UseIndex + i];
                        AddNew++;
                    }
                }

                if (uniqueCornerId + AddNew > maxNumVerticesPerMesh)
                {
                    break;
                }

                for (int i = 0; i < AddNew; i++)
                {
                    FaceIndex[NewCorner[i]] = uniqueCornerId;
                    IndexNew[uniqueCornerId] = NewCorner[i];
                    uniqueCornerId++;
                }

                for (int i = 0; i < 3; i++)
                {
                    NewFace[UseIndex] = FaceIndex[mesh.faces[BaseIndex + UseIndex]];
                    UseIndex++;
                }
            }

            for (int i = 0; i < uniqueCornerId; i++)
            {
                FaceIndex[IndexNew[i]] = -1;
            }

            DecodedMesh subMesh = new DecodedMesh();
            subMesh.faces = new int[UseIndex];
            Array.Copy(NewFace, subMesh.faces, UseIndex);
            subMesh.vertices = new Vector3[uniqueCornerId];
            for (int i = 0; i < uniqueCornerId; i++)
            {
                subMesh.vertices[i] = mesh.vertices[IndexNew[i]];
            }
            if (mesh.normals != null)
            {
                subMesh.normals = new Vector3[uniqueCornerId];
                for (int i = 0; i < uniqueCornerId; i++)
                {
                    subMesh.normals[i] = mesh.normals[IndexNew[i]];
                }
            }

            if (mesh.colors != null)
            {
                subMesh.colors = new Color[uniqueCornerId];
                for (int i = 0; i < uniqueCornerId; i++)
                {
                    subMesh.colors[i] = mesh.colors[IndexNew[i]];
                }
            }

            if (mesh.uvs != null)
            {
                subMesh.uvs = new Vector2[uniqueCornerId];
                for (int i = 0; i < uniqueCornerId; i++)
                {
                    subMesh.uvs[i] = mesh.uvs[IndexNew[i]];
                }
            }

            splittedMeshes.Add(subMesh);

            BaseIndex += UseIndex;
        }
	}

	private float ReadFloatFromIntPtr (IntPtr data, int offset)
	{
		byte[] byteArray = new byte[4];
		for (int j = 0; j < 4; ++j) {
			byteArray [j] = Marshal.ReadByte (data, offset + j);
		}
		return BitConverter.ToSingle (byteArray, 0);
	}

	// TODO(zhafang): Add back LoadFromURL.
	public int LoadMeshFromAsset (string assetName, ref List<Mesh> meshes)
	{
		TextAsset asset = Resources.Load (assetName, typeof(TextAsset)) as TextAsset;
		if (asset == null) {
			Debug.Log ("Didn't load file!");
			return -1;
		}
		byte[] encodedData = asset.bytes;
		Debug.Log (encodedData.Length.ToString ());
		if (encodedData.Length == 0) {
			Debug.Log ("Didn't load encoded data!");
			return -1;
		}
		return DecodeMesh (encodedData, ref meshes);
	}

	public unsafe int DecodeMesh (byte[] data, ref List<Mesh> meshes)
	{
		DracoToUnityMesh* tmpMesh;
		if (DecodeMeshForUnity (data, data.Length, &tmpMesh) <= 0) {
			Debug.Log ("Failed: Decoding error.");
			return -1;
		}

		Debug.Log ("Num indices: " + tmpMesh->numFaces.ToString ());
		Debug.Log ("Num vertices: " + tmpMesh->numVertices.ToString ());
		if (tmpMesh->hasNormal)
			Debug.Log ("Decoded mesh normals.");
		if (tmpMesh->hasTexcoord)
			Debug.Log ("Decoded mesh texcoords.");
		if (tmpMesh->hasColor)
			Debug.Log ("Decoded mesh colors.");

		int numFaces = tmpMesh->numFaces;
		int[] newTriangles = new int[tmpMesh->numFaces * 3];
		for (int i = 0; i < tmpMesh->numFaces; ++i) {
		byte* addr = (byte*)tmpMesh->indices + i * 3 * 4;
        newTriangles[i * 3] = *((int*)addr);
        newTriangles[i * 3 + 1] = *((int*)(addr + 4));
        newTriangles[i * 3 + 2] = *((int*)(addr + 8));
		}

		// For floating point numbers, there's no Marshal functions could directly
		// read from the unmanaged data.
		// TODO(zhafang): Find better way to read float numbers.
		Vector3[] newVertices = new Vector3[tmpMesh->numVertices];
		Vector2[] newUVs = new Vector2[0];
		if (tmpMesh->hasTexcoord)
			newUVs = new Vector2[tmpMesh->numVertices];
		Vector3[] newNormals = new Vector3[0];
		if (tmpMesh->hasNormal)
			newNormals = new Vector3[tmpMesh->numVertices];
        Color[] newColors = new Color[0];
		if (tmpMesh->hasColor)
			newColors = new Color[tmpMesh->numVertices];
		int byteStridePerValue = 4;
		/*
     * TODO(zhafang): Change to:
     * float[] pos = new float[3];
     * for (int i = 0; i < tmpMesh -> numVertices; ++i) {
     *       Marshal.Copy(tmpMesh->position, pos, 3 * i, 3);
     *             for (int j = 0; j < 3; ++j) {
     *                        newVertices[i][j] = pos[j];
     *             }
     * }
     */
	 
		byte* posaddr = (byte*)tmpMesh->position;
        byte* normaladdr = (byte*)tmpMesh->normal;
        byte* coloraddr = (byte*)tmpMesh->color;
        byte* uvaddr = (byte*)tmpMesh->texcoord;
		for (int i = 0; i < tmpMesh->numVertices; ++i) {
			int numValuePerVertex = 3;

            for (int j = 0; j < numValuePerVertex; ++j)
            {
                int byteStridePerVertex = byteStridePerValue * numValuePerVertex;
                int OffSet = i * byteStridePerVertex + byteStridePerValue * j;
                
                newVertices[i][j] = *((float*)(posaddr + OffSet));
                if (tmpMesh->hasNormal)
                {
                    newNormals[i][j] = *((float*)(normaladdr + OffSet));
                }
                if (tmpMesh->hasColor)
                {
                    newColors[i][j] = *((float*)(coloraddr + OffSet));
                }
            }


            if (tmpMesh->hasTexcoord)
            {
                numValuePerVertex = 2;
                for (int j = 0; j < numValuePerVertex; ++j)
                {
                    int byteStridePerVertex = byteStridePerValue * numValuePerVertex;
                    newUVs[i][j] = *((float*)(uvaddr + (i * byteStridePerVertex + byteStridePerValue * j)));
                }
            }
		}

		Marshal.FreeCoTaskMem (tmpMesh->indices);
		Marshal.FreeCoTaskMem (tmpMesh->position);
		if (tmpMesh->hasNormal)
			Marshal.FreeCoTaskMem (tmpMesh->normal);
		if (tmpMesh->hasTexcoord)
			Marshal.FreeCoTaskMem (tmpMesh->texcoord);
		if (tmpMesh->hasColor)
			Marshal.FreeCoTaskMem (tmpMesh->color);
		Marshal.FreeCoTaskMem ((IntPtr)tmpMesh);

		if (newVertices.Length > maxNumVerticesPerMesh) {
			// Unity only support maximum 65534 vertices per mesh. So large meshes
			// need to be splitted.

			DecodedMesh decodedMesh = new DecodedMesh ();
			decodedMesh.vertices = newVertices;
			decodedMesh.faces = newTriangles;
			if (newUVs.Length != 0)
				decodedMesh.uvs = newUVs;
			if (newNormals.Length != 0)
				decodedMesh.normals = newNormals;
			if (newColors.Length != 0)
				decodedMesh.colors = newColors;
			List<DecodedMesh> splittedMeshes = new List<DecodedMesh> ();

			SplitMesh (decodedMesh, ref splittedMeshes);
			for (int i = 0; i < splittedMeshes.Count; ++i) {
				Mesh mesh = new Mesh ();
				mesh.vertices = splittedMeshes [i].vertices;
				mesh.triangles = splittedMeshes [i].faces;
				if (splittedMeshes [i].uvs != null)
					mesh.uv = splittedMeshes [i].uvs;

				if (splittedMeshes [i].colors != null) {
                    mesh.colors = splittedMeshes[i].colors;
                }

				if (splittedMeshes [i].normals != null) {
					mesh.normals = splittedMeshes [i].normals;
				} else {
					Debug.Log ("Sub mesh doesn't have normals, recomputed.");
					mesh.RecalculateNormals ();
				}
				mesh.RecalculateBounds ();
				meshes.Add (mesh);
			}
		} else {
			Mesh mesh = new Mesh ();
			mesh.vertices = newVertices;
			mesh.triangles = newTriangles;
			if (newUVs.Length != 0)
				mesh.uv = newUVs;
			if (newNormals.Length != 0) {
				mesh.normals = newNormals;
			} else {
				mesh.RecalculateNormals ();
				Debug.Log ("Mesh doesn't have normals, recomputed.");
			}
			if (newColors.Length != 0) {
                mesh.colors = newColors;
            }

			mesh.RecalculateBounds ();
			meshes.Add (mesh);
		}
		// TODO(zhafang): Resize mesh to the a proper scale.

		return numFaces;
	}
}
