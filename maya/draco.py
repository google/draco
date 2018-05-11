'''
Usage Example:

from draco import Draco

drc = Draco()
mesh = drc.decode('path-to-file.drc')

print("")
print("==== FACES ====")
print(mesh.faces_num)
print(mesh.faces[0:10])
print("")
print("==== VERTICES ====")
print(mesh.vertices_num)
print(mesh.vertices[0:10])
print("==== NORMALS ====")
print(mesh.normals_num)
print(mesh.normals[0:10])
'''

import os 
import ctypes

class Drc2PyMesh(ctypes.Structure):
    _fields_ = [
				("faces_num", ctypes.c_uint), 
				("faces", ctypes.POINTER(ctypes.c_uint)),
				("vertices_num", ctypes.c_uint),
				("vertices", ctypes.POINTER(ctypes.c_float)),
				("normals_num", ctypes.c_uint),
				("normals", ctypes.POINTER(ctypes.c_float))
			   ]

# TODO: Add integration for UNIX
class Draco:
	def __init__(self):
		dir_path = os.path.dirname(os.path.realpath(__file__))
		lib_path = os.path.join(dir_path, 'dracodec_maya')
		self.drc_lib = ctypes.CDLL(lib_path)
		self.drc_decode = self.drc_lib.drc2py_decode
		self.drc_decode.argtype = [ ctypes.POINTER(ctypes.c_char), ctypes.c_uint, ctypes.POINTER(ctypes.POINTER(Drc2PyMesh)) ]

	def decode(self, file_path):
		file = None
		bytes = None
		try:
			file = open(file_path, 'rb')
			bytes = file.read()
		except IOError as err:
			print('[ERROR] Failure opening file: ' + repr(err))
			return None
		finally:
			if file: file.close()
		
		size = len(bytes)
		mesh_ptr = ctypes.POINTER(Drc2PyMesh)()
		self.drc_decode(bytes, size, ctypes.byref(mesh_ptr))
		mesh = mesh_ptr.contents
		return mesh;
