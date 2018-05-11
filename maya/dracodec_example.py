from dracodec_maya import Draco

drc = Draco()
mesh = drc.decode('bunny.drc')

print("\n==== FACES ====")
print(mesh.faces_num)
print(mesh.faces_len)
print(mesh.faces[0:10])
print("\n==== VERTICES ====")
print(mesh.vertices_num)
print(mesh.vertices_len)
print(mesh.vertices[0:10])
print("\n==== NORMALS ====")
print(mesh.normals_num)
print(mesh.normals_len)
print(mesh.normals)  #This mesh has no normals
print("\n==== UVS ====")
print(mesh.uvs_num)
print(mesh.uvs_len)
print(mesh.uvs)  #This mesh has no uvs