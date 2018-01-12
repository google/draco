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
		public Vector3[] colors;
	}

	[DllImport ("dracodec_unity")] private static extern int DecodeMeshForUnity (
		byte[] buffer, int length, DracoToUnityMesh**tmpMesh);

	static private int maxNumVerticesPerMesh = 60000;

	// Unity only support maximum 65534 vertices per mesh. So large meshes need
	// to be splitted.
	private void SplitMesh (DecodedMesh mesh, ref List<DecodedMesh> splittedMeshes)
	{
		List<int> facesLeft = new List<int> ();
		for (int i = 0; i < mesh.faces.Length; ++i) {
			facesLeft.Add (mesh.faces [i]);
		}
		int numSubMeshes = 0;

		List<int> newCorners = new List<int> ();
		Dictionary<int, int> indexToNewIndex = new Dictionary<int, int> ();

		List<int> facesExtracted = new List<int> ();
		List<Vector3> verticesExtracted = new List<Vector3> ();
		List<Vector2> uvsExtracted = new List<Vector2> ();
		List<Vector3> normalsExtracted = new List<Vector3> ();
		List<Vector3> colorsExtracted = new List<Vector3> ();

		while (facesLeft.Count > 0) {
			numSubMeshes++;
			List<int> tmpLeftFaces = new List<int> ();
			facesExtracted.Clear ();
			verticesExtracted.Clear ();
			uvsExtracted.Clear ();
			normalsExtracted.Clear ();
			colorsExtracted.Clear ();

			int uniqueCornerId = 0;
			indexToNewIndex.Clear ();
			for (int face = 0; face < facesLeft.Count / 3; ++face) {
				newCorners.Clear ();
				// If all indices has appeared or there's still space for more vertices.
				for (int corner = 0; corner < 3; ++corner) {
					if (!indexToNewIndex.ContainsKey (facesLeft [face * 3 + corner])) {
						newCorners.Add (facesLeft [face * 3 + corner]);
					}
				}
				if (newCorners.Count + uniqueCornerId > maxNumVerticesPerMesh) {
					// Save face for the next sub-mesh.
					for (int corner = 0; corner < 3; ++corner) {
						tmpLeftFaces.Add (facesLeft [face * 3 + corner]);
					}
				} else {
					// Add new corners.
					for (int i = 0; i < newCorners.Count; ++i) {
						indexToNewIndex.Add (newCorners [i], uniqueCornerId);
						verticesExtracted.Add (mesh.vertices [newCorners [i]]);
						if (mesh.normals != null)
							normalsExtracted.Add (mesh.normals [newCorners [i]]);
						if (mesh.colors != null)
							colorsExtracted.Add (mesh.colors [newCorners [i]]);
						if (mesh.uvs != null)
							uvsExtracted.Add (mesh.uvs [newCorners [i]]);

						uniqueCornerId++;
					}
					// Add face to this sub-mesh.
					for (int corner = 0; corner < 3; ++corner) {
						facesExtracted.Add (
							indexToNewIndex [facesLeft [face * 3 + corner]]);
					}
				}
			}

			DecodedMesh subMesh = new DecodedMesh ();
			subMesh.faces = facesExtracted.ToArray ();
			subMesh.vertices = verticesExtracted.ToArray ();
			if (normalsExtracted.Count > 0) {
				subMesh.normals = normalsExtracted.ToArray ();
			}
			if (uvsExtracted.Count > 0)
				subMesh.uvs = uvsExtracted.ToArray ();
			if (colorsExtracted.Count > 0)
				subMesh.colors = colorsExtracted.ToArray ();
			splittedMeshes.Add (subMesh);

			facesLeft = tmpLeftFaces;
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
			newTriangles [i * 3] = Marshal.ReadInt32 (tmpMesh->indices, i * 3 * 4);
			newTriangles [i * 3 + 1] =
        Marshal.ReadInt32 (tmpMesh->indices, i * 3 * 4 + 4);
			newTriangles [i * 3 + 2] =
        Marshal.ReadInt32 (tmpMesh->indices, i * 3 * 4 + 8);
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
		Vector3[] newColors = new Vector3[0];
		if (tmpMesh->hasColor)
			newColors = new Vector3[tmpMesh->numVertices];
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
		for (int i = 0; i < tmpMesh->numVertices; ++i) {
			int numValuePerVertex = 3;
			for (int j = 0; j < numValuePerVertex; ++j) {
				int byteStridePerVertex = byteStridePerValue * numValuePerVertex;
				newVertices [i] [j] =
					ReadFloatFromIntPtr (
					tmpMesh->position,
					i * byteStridePerVertex + byteStridePerValue * j);
				if (tmpMesh->hasNormal)
					newNormals [i] [j] =
						ReadFloatFromIntPtr (
						tmpMesh->normal,
						i * byteStridePerVertex + byteStridePerValue * j);
				if (tmpMesh->hasColor)
					newColors [i] [j] =
						ReadFloatFromIntPtr (
						tmpMesh->color,
						i * byteStridePerVertex + byteStridePerValue * j);
			}
			if (tmpMesh->hasTexcoord) {
				numValuePerVertex = 2;
				for (int j = 0; j < numValuePerVertex; ++j) {
					int byteStridePerVertex = byteStridePerValue * numValuePerVertex;
					newUVs [i] [j] =
						ReadFloatFromIntPtr (
						tmpMesh->texcoord,
						i * byteStridePerVertex + byteStridePerValue * j);
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
          mesh.colors = new Color[splittedMeshes [i].colors.Length];
          for (int j = 0; j < splittedMeshes [i].colors.Length; ++j) {
            mesh.colors [j] = new Color (splittedMeshes [i].colors [j] [0],
                splittedMeshes [i].colors [j] [1], splittedMeshes [i].colors [j] [2]);
          }
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
				mesh.colors = new Color[newColors.Length];
				for (int i = 0; i < newColors.Length; ++i) {
					mesh.colors [i] = new Color (newColors [i] [0],
						newColors [i] [1], newColors [i] [2]);
				}
			}

			mesh.RecalculateBounds ();
			meshes.Add (mesh);
		}
		// TODO(zhafang): Resize mesh to the a proper scale.

		return numFaces;
	}
}
