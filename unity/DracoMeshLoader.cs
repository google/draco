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
        public int numSubmesh;
        public IntPtr submesh;
    }

	private struct DecodedMesh
	{
		public int[] indices;
		public Vector3[] vertices;
		public Vector3[] normals;
		public Vector2[] uvs;
		public Color[] colors;
        public int[][] submesh;
    }

    [DllImport("dracodec_unity")]
    private static extern int DecodeMeshForUnity(
        byte[] buffer, int length, DracoToUnityMesh** tmpMesh);

    [DllImport("dracodec_unity")]
    private static extern int EncodeMeshForUnity(
        IntPtr* buffer, DracoToUnityMesh** tmpMesh);

    [DllImport("dracodec_unity")]
    private static extern int EncodeMeshForObjBuff(
        IntPtr* buffer, IntPtr BuffData, int buffSize);

    [DllImport("dracodec_unity")]
    private static extern int FreeDracoMesh(IntPtr* buffer);

    [DllImport("dracodec_unity")]
    private static extern int ReleaseUnityMesh(DracoToUnityMesh** tmpMesh);

    static private int maxNumVerticesPerMesh = 60000;

	// Unity only support maximum 65534 vertices per mesh. So large meshes need
	// to be splitted.
	private void SplitMesh (DecodedMesh mesh, ref List<DecodedMesh> splittedMeshes)
	{
		// Map between new indices on a splitted mesh and old indices on the
		// original mesh.
		int[] newToOldIndexMap = new int[maxNumVerticesPerMesh];

		// Index of the first unprocessed corner.
		int baseCorner = 0;
		int indicesCount = mesh.indices.Length;

		// Map between old indices of the original mesh and indices on the currently
		// processed sub-mesh. Inverse of |newToOldIndexMap|.
		int[] oldToNewIndexMap = new int[indicesCount];
		int[] newIndices = new int[indicesCount];


		// Set mapping between existing vertex indices and new vertex indices to
		// a default value.
		for (int i = 0; i < indicesCount; i++)
		{
			oldToNewIndexMap[i] = -1;
		}

		// Number of added vertices for the currently processed sub-mesh.
		int numAddedVertices = 0;

		// Process all corners (faces) of the original mesh.
		while (baseCorner < indicesCount)
		{
			// Reset the old to new indices map that may have been set by previously
			// processed sub-meshes.
			for (int i = 0; i < numAddedVertices; i++)
			{
				oldToNewIndexMap[newToOldIndexMap[i]] = -1;
			}
			numAddedVertices = 0;

			// Number of processed corners on the current sub-mesh.
			int numProcessedCorners = 0;

			// Local storage for indices added to the new sub-mesh for a currently
			// processed face.
			int[] newlyAddedIndices = new int[3];

			// Sub-mesh processing starts here.
			for (; baseCorner + numProcessedCorners < indicesCount;)
			{
				// Number of vertices that we need to add to the current sub-mesh.
				int verticesAdded = 0;
				for (int i = 0; i < 3; i++)
				{
					if (oldToNewIndexMap[mesh.indices[baseCorner + numProcessedCorners + i]] == -1)
					{
						newlyAddedIndices[verticesAdded] = mesh.indices[baseCorner + numProcessedCorners + i];
						verticesAdded++;
					}
				}

				// If the number of new vertices that we need to add is larger than the
				// allowed limit, we need to stop processing the current sub-mesh.
				// The current face will be processed again for the next sub-mesh.
				if (numAddedVertices + verticesAdded > maxNumVerticesPerMesh)
				{
					break;
				}

				// Update mapping between old an new vertex indices.
				for (int i = 0; i < verticesAdded; i++)
				{
					oldToNewIndexMap[newlyAddedIndices[i]] = numAddedVertices;
					newToOldIndexMap[numAddedVertices] = newlyAddedIndices[i];
					numAddedVertices++;
				}

				for (int i = 0; i < 3; i++)
				{
					newIndices[numProcessedCorners] = oldToNewIndexMap[mesh.indices[baseCorner + numProcessedCorners]];
					numProcessedCorners++;
				}
			}
			// Sub-mesh processing done.
			DecodedMesh subMesh = new DecodedMesh();
			subMesh.indices = new int[numProcessedCorners];
			Array.Copy(newIndices, subMesh.indices, numProcessedCorners);
			subMesh.vertices = new Vector3[numAddedVertices];
			for (int i = 0; i < numAddedVertices; i++)
			{
				subMesh.vertices[i] = mesh.vertices[newToOldIndexMap[i]];
			}
			if (mesh.normals != null)
			{
				subMesh.normals = new Vector3[numAddedVertices];
				for (int i = 0; i < numAddedVertices; i++)
				{
					subMesh.normals[i] = mesh.normals[newToOldIndexMap[i]];
				}
			}

			if (mesh.colors != null)
			{
				subMesh.colors = new Color[numAddedVertices];
				for (int i = 0; i < numAddedVertices; i++)
				{
					subMesh.colors[i] = mesh.colors[newToOldIndexMap[i]];
				}
			}

			if (mesh.uvs != null)
			{
				subMesh.uvs = new Vector2[numAddedVertices];
				for (int i = 0; i < numAddedVertices; i++)
				{
					subMesh.uvs[i] = mesh.uvs[newToOldIndexMap[i]];
				}
			}

			splittedMeshes.Add(subMesh);
			baseCorner += numProcessedCorners;
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
        if (tmpMesh->numSubmesh > 0)
            Debug.Log("Decoded mesh submesh.");

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
        int[][] newSubmesh = null;
            

		byte* posaddr = (byte*)tmpMesh->position;
		byte* normaladdr = (byte*)tmpMesh->normal;
		byte* coloraddr = (byte*)tmpMesh->color;
		byte* uvaddr = (byte*)tmpMesh->texcoord;
        for (int i = 0; i < tmpMesh->numVertices; ++i)
		{
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
			}

			if (tmpMesh->hasColor)
			{
				numValuePerVertex = 4;
				for (int j = 0; j < numValuePerVertex; ++j)
				{
					int byteStridePerVertex = byteStridePerValue * numValuePerVertex;
					newColors[i][j] = *((float*)(coloraddr + (i * byteStridePerVertex + byteStridePerValue * j)));
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

        if (tmpMesh->numSubmesh > 0)
        {
            newSubmesh = new int[tmpMesh->numSubmesh][];
            byte* submeshaddr = (byte*)tmpMesh->submesh;
            for (int i = 0; i < tmpMesh->numSubmesh; ++i)
            {
                int submeshIndexCount = ((int**)submeshaddr)[i][0];
                newSubmesh[i] = new int[submeshIndexCount];
                for(int j = 0; j < submeshIndexCount;j++)
                {
                    newSubmesh[i][j] = ((int**)submeshaddr)[i][j + 1];
                }
            }
        }

        ReleaseUnityMesh (&tmpMesh);

		if (newVertices.Length > maxNumVerticesPerMesh) {
			// Unity only support maximum 65534 vertices per mesh. So large meshes
			// need to be splitted.

            if(newSubmesh != null)
            {
                int submeshCount = newSubmesh.Length;
                for (int submeshIndex = 0; submeshIndex < newSubmesh.Length; submeshIndex++)
                {
                   if(newSubmesh[submeshIndex].Length > maxNumVerticesPerMesh)
                    {
                        
                        HashSet<int> submeshVertxSet = new HashSet<int>();
                        int submeshIndexCount = newSubmesh[submeshIndex].Length;
                        for (int submeshFaceIndex = 0; submeshFaceIndex < submeshIndexCount; submeshFaceIndex++)
                        {
                            int meshVecterIndex = newSubmesh[submeshIndex][submeshFaceIndex];
                            if (!submeshVertxSet.Contains(meshVecterIndex))
                            {
                                submeshVertxSet.Add(meshVecterIndex);
                            }
                        }

                        DecodedMesh decodedMesh = new DecodedMesh();
                        decodedMesh.vertices = new Vector3[submeshVertxSet.Count];
                        if (newUVs.Length != 0)
                            decodedMesh.uvs = new Vector2[submeshVertxSet.Count];
                        if (newNormals.Length != 0)
                            decodedMesh.normals = new Vector3[submeshVertxSet.Count];
                        if (newColors.Length != 0)
                            decodedMesh.colors = new Color[submeshVertxSet.Count];

                        int[] verticesMap = new int[submeshVertxSet.Count];
                        int decodedMeshVertexIndex = 0;
                        foreach (int faceIndex in submeshVertxSet)
                        {
                            decodedMesh.vertices[decodedMeshVertexIndex] = newVertices[faceIndex];
                            verticesMap[decodedMeshVertexIndex] = faceIndex;
                            if (decodedMesh.uvs != null)
                                decodedMesh.uvs[decodedMeshVertexIndex] = newUVs[faceIndex];
                            if (decodedMesh.normals != null)
                                decodedMesh.normals[decodedMeshVertexIndex] = newNormals[faceIndex];
                            if (decodedMesh.colors != null)
                                decodedMesh.colors[decodedMeshVertexIndex] = newColors[faceIndex];

                            decodedMeshVertexIndex++;
                        }

                        decodedMesh.indices = new int[submeshIndexCount];
                        for (int submeshFaceIndex = 0; submeshFaceIndex < submeshIndexCount; submeshFaceIndex++)
                        {
                            decodedMesh.indices[submeshFaceIndex] = verticesMap[newSubmesh[submeshIndex][submeshFaceIndex]];
                        }


                        List<DecodedMesh> splittedMeshes = new List<DecodedMesh>();
                        SplitMesh(decodedMesh, ref splittedMeshes);
                        BuildSplitMesh(splittedMeshes, meshes);
                    }
                }
            }
            else
            {
                DecodedMesh decodedMesh = new DecodedMesh();
                decodedMesh.vertices = newVertices;
                decodedMesh.indices = newTriangles;
                if (newUVs.Length != 0)
                    decodedMesh.uvs = newUVs;
                if (newNormals.Length != 0)
                    decodedMesh.normals = newNormals;
                if (newColors.Length != 0)
                    decodedMesh.colors = newColors;
                List<DecodedMesh> splittedMeshes = new List<DecodedMesh>();

                SplitMesh(decodedMesh, ref splittedMeshes);
                BuildSplitMesh(splittedMeshes, meshes);
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

            if(newSubmesh != null)
            {
                mesh.subMeshCount = newSubmesh.Length;
                for(int submeshIndex = 0; submeshIndex < newSubmesh.Length; submeshIndex++)
                {
                    //mesh.SetTriangles(newSubmesh[submeshIndex], submeshIndex);
                }
            }

			// Scale and translate the decoded mesh so it would be visible to
			// a new camera's default settings.
			float scale = 0.5f / mesh.bounds.extents.x;
			if (0.5f / mesh.bounds.extents.y < scale)
				scale = 0.5f / mesh.bounds.extents.y;
			if (0.5f / mesh.bounds.extents.z < scale)
				scale = 0.5f / mesh.bounds.extents.z;

			Vector3[] vertices = mesh.vertices;
			int i = 0;
			while (i < vertices.Length) {
				vertices[i] *= scale;
				i++;
			}

			mesh.vertices = vertices;
			mesh.RecalculateBounds ();

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
			mesh.RecalculateBounds ();
			meshes.Add (mesh);
		}

		return numFaces;
	}

    void BuildSplitMesh(List<DecodedMesh> splittedMeshes, List<Mesh> meshes)
    {
        for (int i = 0; i < splittedMeshes.Count; ++i)
        {
            Mesh mesh = new Mesh();
            mesh.vertices = splittedMeshes[i].vertices;
            mesh.triangles = splittedMeshes[i].indices;
            if (splittedMeshes[i].uvs != null)
                mesh.uv = splittedMeshes[i].uvs;

            if (splittedMeshes[i].colors != null)
            {
                mesh.colors = splittedMeshes[i].colors;
            }

            if (splittedMeshes[i].normals != null)
            {
                mesh.normals = splittedMeshes[i].normals;
            }
            else
            {
                Debug.Log("Sub mesh doesn't have normals, recomputed.");
                mesh.RecalculateNormals();
            }

            if(splittedMeshes[i].submesh != null)
            {
                mesh.subMeshCount = splittedMeshes[i].submesh.Length;
                for (int submeshIndex = 0; submeshIndex < splittedMeshes[i].submesh.Length; submeshIndex++)
                {
                    mesh.SetTriangles(splittedMeshes[i].submesh[submeshIndex], submeshIndex);
                }
            }

            mesh.RecalculateBounds();
            meshes.Add(mesh);
        }
    }

    public byte[] EncodeUnityMesh(Mesh meshData)
    {
        byte[] compresMeshBuffer = null;
        if(meshData == null)
        {
            return compresMeshBuffer;
        }

        DracoMeshLoader.DracoToUnityMesh dracoMesh;
        UnityEngine.Vector3[] vertex = meshData.vertices;
        int[] face = meshData.triangles;
        UnityEngine.Vector2[] uv = meshData.uv;

        dracoMesh.numVertices = vertex.Length;
        dracoMesh.position = System.Runtime.InteropServices.Marshal.AllocHGlobal(vertex.Length * 3 * sizeof(float));
        dracoMesh.numFaces = face.Length / 3;
        dracoMesh.indices = System.Runtime.InteropServices.Marshal.AllocHGlobal(face.Length * sizeof(int));

        for (int vIndex = 0; vIndex < dracoMesh.numVertices; vIndex++)
        {
            byte* addr = (byte*)dracoMesh.position + vIndex * 3 * 4;
            *((float*)addr) = vertex[vIndex].x;
            *((float*)(addr + 4)) = vertex[vIndex].y;
            *((float*)(addr + 8)) = vertex[vIndex].z;
        }

        for (int faceIndex = 0; faceIndex < face.Length; faceIndex++)
        {
            byte* addr = (byte*)dracoMesh.indices + faceIndex * 4;
            *((int*)addr) = face[faceIndex];
        }

        if (uv != null)
        {
            dracoMesh.hasTexcoord = true;
            dracoMesh.texcoord = System.Runtime.InteropServices.Marshal.AllocHGlobal(vertex.Length * 2 * sizeof(float));
            for (int uvIndex = 0; uvIndex < dracoMesh.numVertices; uvIndex++)
            {
                byte* addr = (byte*)dracoMesh.texcoord + uvIndex * 2 * 4;
                *((float*)addr) = uv[uvIndex].x;
                *((float*)(addr + 4)) = uv[uvIndex].y;
            }
        }

        if(meshData.subMeshCount > 1)
        {
            dracoMesh.numSubmesh = meshData.subMeshCount;
            dracoMesh.submesh = System.Runtime.InteropServices.Marshal.AllocHGlobal(meshData.subMeshCount * sizeof(int*));
            for (int submeshIndex = 0; submeshIndex < meshData.subMeshCount; submeshIndex++)
            {
                int[] submeshArray = meshData.GetTriangles(submeshIndex);
                ((IntPtr*)dracoMesh.submesh)[submeshIndex] = System.Runtime.InteropServices.Marshal.AllocHGlobal((submeshArray.Length + 1) * sizeof(int));
                int* addr = (int*)((IntPtr*)dracoMesh.submesh)[submeshIndex];
                addr[0] = submeshArray.Length;
                for (int i = 0; i < submeshArray.Length; i++)
                {
                    addr[i + 1] = submeshArray[i];
                }
            }
        }


        System.IntPtr dracoBuffer = new System.IntPtr();
        DracoMeshLoader.DracoToUnityMesh* pMesh = &dracoMesh;

        int BuffSize = DracoMeshLoader.EncodeMeshForUnity(&dracoBuffer, &pMesh);
        if (BuffSize > 0)
        {
            compresMeshBuffer = new byte[BuffSize];
            System.Runtime.InteropServices.Marshal.Copy(dracoBuffer, compresMeshBuffer, 0, BuffSize);
        }

        System.Runtime.InteropServices.Marshal.FreeHGlobal(dracoMesh.position);
        System.Runtime.InteropServices.Marshal.FreeHGlobal(dracoMesh.indices);
        System.Runtime.InteropServices.Marshal.FreeHGlobal(dracoMesh.texcoord);
        if (meshData.subMeshCount > 1)
        {
            for (int submeshIndex = 0; submeshIndex < meshData.subMeshCount; submeshIndex++)
            {
                System.Runtime.InteropServices.Marshal.FreeHGlobal(((IntPtr*)dracoMesh.submesh)[submeshIndex]);
            }
            System.Runtime.InteropServices.Marshal.FreeHGlobal(dracoMesh.submesh);
        }
        DracoMeshLoader.FreeDracoMesh(&dracoBuffer);
        return compresMeshBuffer;
    }
}
