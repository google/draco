// Copyright 2019 The Draco Authors.
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

// #define DRACO_VERBOSE

using System;
using System.Collections;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Profiling;
using UnityEngine.Events;
using Unity.Collections;
using Unity.Collections.LowLevel.Unsafe;
using Unity.Jobs;

public unsafe class DracoMeshLoader
{
	#if UNITY_EDITOR_OSX || UNITY_WEBGL || UNITY_IOS
        public const string DRACODEC_UNITY_LIB = "__Internal";
    #elif UNITY_ANDROID || UNITY_STANDALONE
        public const string DRACODEC_UNITY_LIB = "dracodec_unity";
    #endif

	const Allocator defaultAllocator = Allocator.Persistent;

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

	struct DracoJob : IJob {

		[ReadOnly]
		public NativeArray<byte> data;

		public NativeArray<IntPtr> outMesh;

		public NativeArray<int> result;

		public void Execute() {
			DracoToUnityMesh* tmpMesh;
			result[0] = DecodeMeshForUnity (data.GetUnsafeReadOnlyPtr(), data.Length, &tmpMesh);
			outMesh[0] = (IntPtr) tmpMesh;
		}
	}

	[DllImport (DRACODEC_UNITY_LIB)] private static extern int DecodeMeshForUnity (
		void* buffer, int length, DracoToUnityMesh**tmpMesh);

	[DllImport(DRACODEC_UNITY_LIB)] private static extern int ReleaseUnityMesh(DracoToUnityMesh** tmpMesh);

	private float ReadFloatFromIntPtr (IntPtr data, int offset)
	{
		byte[] byteArray = new byte[4];
		for (int j = 0; j < 4; ++j) {
			byteArray [j] = Marshal.ReadByte (data, offset + j);
		}
		return BitConverter.ToSingle (byteArray, 0);
	}

	public UnityAction<Mesh> onMeshesLoaded;

	public IEnumerator DecodeMesh(NativeArray<byte> data) {

		Profiler.BeginSample("JobPrepare");
		var job = new DracoJob();

		job.data = data;
		job.result = new NativeArray<int>(1,defaultAllocator);
		job.outMesh = new NativeArray<IntPtr>(1,defaultAllocator);

		var jobHandle = job.Schedule();
		Profiler.EndSample();

		while(!jobHandle.IsCompleted) {
			yield return null;
		}
		jobHandle.Complete();

		int result = job.result[0];
		IntPtr dracoMesh = job.outMesh[0];

		job.result.Dispose();
		job.outMesh.Dispose();

		if (result <= 0) {
			Debug.LogError ("Failed: Decoding error.");
			yield break;
		}

		var mesh = CreateMesh(dracoMesh);

		if(onMeshesLoaded!=null) {
			onMeshesLoaded(mesh);
		}
	}

#if UNITY_EDITOR
	/// <summary>
	/// Synchronous (non-threaded) version of DecodeMesh for Edtior purpose.
	/// </summary>
	/// <param name="data">Drace file data</param>
	/// <returns>The Mesh</returns>
	public Mesh DecodeMeshSync(NativeArray<byte> data) {

		Profiler.BeginSample("JobPrepare");
		var job = new DracoJob();

		job.data = data;
		job.result = new NativeArray<int>(1,defaultAllocator);
		job.outMesh = new NativeArray<IntPtr>(1,defaultAllocator);

		job.Run();
		Profiler.EndSample();

		int result = job.result[0];
		IntPtr dracoMesh = job.outMesh[0];

		job.result.Dispose();
		job.outMesh.Dispose();

		if (result <= 0) {
			Debug.LogError ("Failed: Decoding error.");
			return null;
		}

		return CreateMesh(dracoMesh);
	}
#endif

	unsafe Mesh CreateMesh (IntPtr dracoMesh)
	{
		Profiler.BeginSample("CreateMesh");
		DracoToUnityMesh* tmpMesh = (DracoToUnityMesh*) dracoMesh;

		Log ("Num indices: " + tmpMesh->numFaces.ToString ());
		Log ("Num vertices: " + tmpMesh->numVertices.ToString ());

		Profiler.BeginSample("CreateMeshAlloc");
		int[] newTriangles = new int[tmpMesh->numFaces * 3];
		Vector3[] newVertices = new Vector3[tmpMesh->numVertices];
		Profiler.EndSample();

		Vector2[] newUVs = null;
		Vector3[] newNormals = null;
		Color[] newColors = null;

		Profiler.BeginSample("CreateMeshIndices");
		byte* indicesSrc = (byte*)tmpMesh->indices;
		var indicesPtr = UnsafeUtility.AddressOf(ref newTriangles[0]);
		UnsafeUtility.MemCpy(indicesPtr,indicesSrc,newTriangles.Length*4);
		Profiler.EndSample();

		Profiler.BeginSample("CreateMeshPositions");
		byte* posaddr = (byte*)tmpMesh->position;
		/// TODO(atteneder): check if we can avoid mem copies with new Mesh API (2019.3?)
		/// by converting void* to NativeArray via
		/// NativeArrayUnsafeUtility.ConvertExistingDataToNativeArray
		var newVerticesPtr = UnsafeUtility.AddressOf(ref newVertices[0]);
		UnsafeUtility.MemCpy(newVerticesPtr,posaddr,tmpMesh->numVertices * 12 );
		Profiler.EndSample();

		if (tmpMesh->hasTexcoord) {
			Profiler.BeginSample("CreateMeshUVs");
			Log ("Decoded mesh texcoords.");
			newUVs = new Vector2[tmpMesh->numVertices];
			byte* uvaddr = (byte*)tmpMesh->texcoord;
			var newUVsPtr = UnsafeUtility.AddressOf(ref newUVs[0]);
			UnsafeUtility.MemCpy(newUVsPtr,uvaddr,tmpMesh->numVertices * 8 );
			Profiler.EndSample();
		}
		if (tmpMesh->hasNormal) {
			Profiler.BeginSample("CreateMeshNormals");
			Log ("Decoded mesh normals.");
			newNormals = new Vector3[tmpMesh->numVertices];
			byte* normaladdr = (byte*)tmpMesh->normal;
			var newNormalsPtr = UnsafeUtility.AddressOf(ref newNormals[0]);
			UnsafeUtility.MemCpy(newNormalsPtr,normaladdr,tmpMesh->numVertices * 12 );
			Profiler.EndSample();
		}
		if (tmpMesh->hasColor) {
			Profiler.BeginSample("CreateMeshColors");
			Log ("Decoded mesh colors.");
			newColors = new Color[tmpMesh->numVertices];
			byte* coloraddr = (byte*)tmpMesh->color;
			var newColorsPtr = UnsafeUtility.AddressOf(ref newColors[0]);
			UnsafeUtility.MemCpy(newColorsPtr,coloraddr,tmpMesh->numVertices * 16 );
			Profiler.EndSample();
		}

		Profiler.BeginSample("CreateMeshRelease");
		ReleaseUnityMesh (&tmpMesh);
		Profiler.EndSample();

		Profiler.BeginSample("CreateMeshFeeding");
		Mesh mesh = new Mesh ();
#if UNITY_2017_3_OR_NEWER
		mesh.indexFormat =  (newVertices.Length > System.UInt16.MaxValue)
		? UnityEngine.Rendering.IndexFormat.UInt32
		: UnityEngine.Rendering.IndexFormat.UInt16;
#else
		if(newVertices.Length > System.UInt16.MaxValue) {
			throw new System.Exception("Draco meshes with more than 65535 vertices are only supported from Unity 2017.3 onwards.");
		}
#endif

		mesh.vertices = newVertices;
		mesh.SetTriangles(newTriangles,0,true);
		if (newUVs!=null) {
			mesh.uv = newUVs;
		}
		if (newNormals!=null) {
			mesh.normals = newNormals;
		} else {
			mesh.RecalculateNormals ();
			Log ("Mesh doesn't have normals, recomputed.");
		}
		if (newColors!=null) {
			mesh.colors = newColors;
		}

		Profiler.EndSample();
		return mesh;
	}

	[System.Diagnostics.Conditional("DRACO_VERBOSE")]
	static void Log(string format, params object[] args) {
		Debug.LogFormat(format,args);
	}
}
