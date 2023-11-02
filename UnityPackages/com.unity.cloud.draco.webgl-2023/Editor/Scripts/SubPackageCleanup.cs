using System.Threading.Tasks;
using UnityEditor;
using UnityEditor.PackageManager;
using UnityEngine;
using UnityEngine.Assertions;
using System.Linq;

namespace SubPackage
{
    static class SubPackageCleanup
    {

        const string k_Name = "com.unity.cloud.draco.webgl-2023";
        const string k_DisplayName = "Draco for Unity WebGL 2023";

        static readonly string[] k_MainPackageNames = {
            "com.unity.cloud.draco",
            "com.atteneder.draco"
        };

#if !DISABLE_SUB_PACKAGE_CHECK
        [InitializeOnLoadMethod]
#endif
        static async void InitializeOnLoad()
        {
            var request = Client.List(offlineMode: true, includeIndirectDependencies: false);

            while (!request.IsCompleted)
                await Task.Yield();

            Assert.AreEqual(StatusCode.Success, request.Status);

            var mainPackageInManifest = request.Result.Select(package => package.name).Intersect(k_MainPackageNames).Any();

            if (!mainPackageInManifest)
            {
                Debug.LogWarning($"Package {k_DisplayName} ({k_Name}) is missing its main package <a href=\"https://docs.unity3d.com/Packages/com.unity.cloud.draco@latest/\">Draco for Unity</a> and should get removed.");
            }
        }
    }
}
