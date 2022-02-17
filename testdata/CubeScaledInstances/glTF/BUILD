# Description:
#   Shared test files.

package(
    default_hdrs_check = "strict",
    default_visibility = ["//visibility:public"],
)

licenses(["notice"])  # Apache 2.0

exports_files(glob([
    "*.bin",
    "*.gltf",
    "*.obj",
    "*.OBJ",
    "*.mtl",
    "*.ply",
    "*.png",
    "*.ptx",
    "*.drc",
]))

filegroup(
    name = "test_files",
    srcs = glob(["*.png"]) + glob(["*.bin"]) + glob(["*.gltf"]),
)
