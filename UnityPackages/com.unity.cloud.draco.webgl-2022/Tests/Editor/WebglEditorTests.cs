using System.IO;
using NUnit.Framework;
using UnityEngine;
using File = UnityEngine.Windows.File;

namespace KtxUnity.Webgl.Editor.Tests
{
    class WebglEditorTests
    {
        const string k_PackagePrefix = "Packages/com.unity.cloud.draco.webgl-2022/Runtime/Plugins/WebGL";

        static readonly string[] k_WebglBinaries = {
            $"{k_PackagePrefix}/libdraco_unity.a"
        };

        [Test]
        public void CheckNativeLibSize()
        {
            foreach (var webglBinary in k_WebglBinaries)
            {
                Assert.IsTrue(File.Exists(webglBinary));
                var fi = new FileInfo(webglBinary);
                // In source (GIT) the native WebGL library files are placeholders with a few bytes of text in them.
                // The CI will replace them with actual binaries, all significantly bigger than 1024 bytes.
                Assert.Greater(fi.Length, 1024);
            }
        }
    }
}
