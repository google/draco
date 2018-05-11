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
# print(mesh.normals[0:10])  This mesh has no normals
