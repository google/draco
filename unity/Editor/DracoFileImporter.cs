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
using UnityEditor;
using UnityEngine;

public class DracoFileImporter : AssetPostprocessor {
  static void OnPostprocessAllAssets(string[] importedAssets, string[] deletedAssets,
      string[] movedAssets, string[] movedFromAssetPaths) {
    foreach(string str in importedAssets) {
      // Compressed file must be renamed to ".drc.bytes".
      if (str.IndexOf(".drc.bytes") == -1) {
        return;
      }

      // If the original mesh exceeds the limit of number of verices, the
      // loader will split it to a list of smaller meshes.
      List<Mesh> meshes = new List<Mesh>();
      DracoMeshLoader dracoLoader = new DracoMeshLoader();

      // The decoded mesh will be named without ".drc.bytes"
      str.LastIndexOf('/');
      int length = str.Length - ".drc.bytes".Length - str.LastIndexOf('/') - 1;
      string fileName = str.Substring(str.LastIndexOf('/') + 1, length);

      int numFaces = dracoLoader.LoadMeshFromAsset(fileName + ".drc", ref meshes);
      if (numFaces > 0) {
        // Create mesh assets. Combine the smaller meshes to a single asset.
        // TODO: Figure out how to combine to an unseen object as .obj files.
        AssetDatabase.CreateAsset (meshes [0], "Assets/Resources/" + fileName + ".asset");
        AssetDatabase.SaveAssets ();
        for (int i = 1; i < meshes.Count; ++i) {
          AssetDatabase.AddObjectToAsset(meshes [i], meshes [0]);
          AssetDatabase.ImportAsset(AssetDatabase.GetAssetPath (meshes [i]));
        }

        // Also create a Prefab for easy usage.
        GameObject newAsset = new GameObject();
        newAsset.hideFlags = HideFlags.HideInHierarchy;
        for (int i = 0; i < meshes.Count; ++i) {
          GameObject subObject = new GameObject();
          subObject.hideFlags = HideFlags.HideInHierarchy;
          subObject.AddComponent<MeshFilter>();
          subObject.AddComponent<MeshRenderer>();
          subObject.GetComponent<MeshFilter>().mesh =
            UnityEngine.Object.Instantiate(meshes[i]);
          subObject.transform.parent = newAsset.transform;
        }
        PrefabUtility.CreatePrefab("Assets/Resources/" + fileName + ".prefab", newAsset);
      } else {
        // TODO: Throw exception?
        Debug.Log("Error: Decodeing Draco file failed.");
      }
    }
  }
}
