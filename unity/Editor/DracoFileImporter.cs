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
using System.IO;
using System.Globalization;
using UnityEditor;
using UnityEngine;
using Unity.Collections;

public class DracoFileImporter : AssetPostprocessor {

  const string FILE_EXT = ".drc";
  const string FILE_EXT_BYTES = ".drc.bytes";

  static void OnPostprocessAllAssets(string[] importedAssets, string[] deletedAssets,
      string[] movedAssets, string[] movedFromAssetPaths) {
    foreach(string str in importedAssets) {
      // Compressed file must be renamed to ".drc.bytes".
      var extIndex = str.EndsWith(FILE_EXT,true,CultureInfo.InvariantCulture);
      var extBytesIndex = str.EndsWith(FILE_EXT_BYTES,true,CultureInfo.InvariantCulture);

      if ( !(extIndex||extBytesIndex) ) {
        return;
      }

      DracoMeshLoader dracoLoader = new DracoMeshLoader();

      int length = str.Length - (extIndex?4:6) - str.LastIndexOf('/') - 1;
      string fileName = str.Substring(str.LastIndexOf('/') + 1, length);

      var data = File.ReadAllBytes(str);
      var nativeData = new NativeArray<byte>(data.Length,Allocator.TempJob);
      nativeData.CopyFrom(data);
      var mesh = dracoLoader.DecodeMeshSync(nativeData);
      nativeData.Dispose();

      if (mesh!=null) {
        mesh.name = fileName;
        // Create mesh assets. Combine the smaller meshes to a single asset.
        // TODO: Figure out how to combine to an unseen object as .obj files.
        var dir = Path.GetDirectoryName(str);
        AssetDatabase.CreateAsset (mesh, Path.Combine(dir, fileName + ".asset" ));
        AssetDatabase.SaveAssets ();
        
        // Also create a Prefab for easy usage.
        GameObject newAsset = new GameObject();
        newAsset.hideFlags = HideFlags.HideInHierarchy;
        var mf = newAsset.AddComponent<MeshFilter>();
        mf.mesh = mesh;
        newAsset.AddComponent<MeshRenderer>();
        
        bool success;
        PrefabUtility.SaveAsPrefabAsset(newAsset, Path.Combine(dir, fileName + ".prefab"), out success);

        Object.DestroyImmediate(newAsset);

        if(!success) {
          Debug.LogError("Creating Draco Prefab failed!");
        }
      } else {
        // TODO: Throw exception?
        Debug.LogError("Decodeing Draco file failed.");
      }
    }
  }
}
