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

public class DracoDecodingObject : MonoBehaviour {

  // This function will be used when the GameObject is initialized.
  void Start() {

    // If the original mesh exceeds the limit of number of verices, the
    // loader will split it to a list of smaller meshes.
    List<Mesh> mesh = new List<Mesh>();
    DracoMeshLoader dracoLoader = new DracoMeshLoader();
    /*
     * Here we use the compressed Bunny model as example.
     * It's in unity/Resources/bunny.drc.bytes.
     * Please see README.md for details.
     */
    int numFaces = dracoLoader.LoadMeshFromAsset("bunny", ref mesh);

    /* Note: You need to add MeshFilter (and MeshRenderer) to your GameObject.
     * Or you can do something like the following in script:
     * AddComponent<MeshFilter>();
     */
    if (numFaces > 0) {
      GetComponent<MeshFilter>().mesh = mesh[0];
    }
  }
}
