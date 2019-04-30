using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class DracoEncodingMesh : MonoBehaviour {
    public Mesh EncodingData;
    public string SavePath;
	
    [ContextMenu("Encoding Mesh")]
	void Encoding()
    {
		if(EncodingData == null || string.IsNullOrEmpty(SavePath))
        {
            Debug.LogError("Encoding Data Error");
            return;
        }

        DracoMeshLoader dracoLoader = new DracoMeshLoader();

        byte[] dracoData = dracoLoader.EncodeUnityMesh(EncodingData);

        try
        {
            if (dracoData != null && dracoData.Length > 0)
            {
                using (System.IO.FileStream saveFile = System.IO.File.Open(SavePath, System.IO.FileMode.Create))
                {
                    saveFile.Write(dracoData, 0, dracoData.Length);
                    saveFile.Flush();
                }
            }
            Debug.Log("Encode Finish");
            return; 
        }
        catch(System.Exception e)
        {
            Debug.LogError(e.ToString());
        }

        Debug.LogError("Encode Faile");
    }

    [ContextMenu("Decoding SaveFile")]
    void Decoding()
    {
        if(string.IsNullOrEmpty(SavePath) || !System.IO.File.Exists(SavePath))
        {
            Debug.LogError("Decoding Data Error");
            return;
        }

        DracoMeshLoader dracoLoader = new DracoMeshLoader();

        byte[] dracoData = null;
        try
        {
            using (System.IO.FileStream saveFile = System.IO.File.Open(SavePath, System.IO.FileMode.Open))
            {
                dracoData = new byte[saveFile.Length];
                saveFile.Read(dracoData, 0, dracoData.Length);
            }
            
            if(dracoData != null)
            {
                List<Mesh> TestMeshList = new List<Mesh>();
                dracoLoader.DecodeMesh(dracoData, ref TestMeshList);

                if(TestMeshList.Count > 0)
                {
                    foreach(Mesh decMesh in TestMeshList)
                    {
                        MeshFilter showMeshCom = gameObject.AddComponent<MeshFilter>();
                        showMeshCom.mesh = decMesh;
                    }
                }
            }

            Debug.Log("Decode Finish");
            return;
        }
        catch (System.Exception e)
        {
            Debug.LogError(e.ToString());
        }

        Debug.LogError("Decode Faile");
    }
}
