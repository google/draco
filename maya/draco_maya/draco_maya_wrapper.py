"""
Draco wrapper for Maya extensions
"""

import os 
import ctypes

__author__ = "Mattia Pezzano, Federico de Felici, Duccio Lenkowicz"
__version__ = "0.1"

class Drc2PyMesh(ctypes.Structure):
    _fields_ = [
				("faces_num", ctypes.c_uint), 
				("faces", ctypes.POINTER(ctypes.c_uint)),
				("vertices_num", ctypes.c_uint),
				("vertices", ctypes.POINTER(ctypes.c_float)),
				("normals_num", ctypes.c_uint),
				("normals", ctypes.POINTER(ctypes.c_float)),
				("uvs_num", ctypes.c_uint),
				("uvs_real_num", ctypes.c_uint),
				("uvs", ctypes.POINTER(ctypes.c_float))
			   ]

class DrcMesh:
	def __init__(self):
		self.faces = []
		self.faces_len = 0
		self.faces_num = 0
		
		self.vertices = []
		self.vertices_len = 0
		self.vertices_num = 0
		
		self.normals = []
		self.normals_len = 0
		self.normals_num = 0
		
		self.uvs = []
		self.uvs_len = 0
		self.uvs_num = 0

def array_to_ptr(array, size, ctype):
	carr = (ctype * size)(*array)
	return ctypes.cast(carr, ctypes.POINTER(ctype))

# TODO: Add integration for reading Linux lib
class Draco:
	def __init__(self):
		# Lib loading
		dir_path = os.path.dirname(os.path.realpath(__file__))
		lib_path = os.path.join(dir_path, 'draco_maya_wrapper')
		self.drc_lib = ctypes.CDLL(lib_path)
		# Mapping decode funct
		self.drc_decode = self.drc_lib.drc2py_decode
		self.drc_decode.argtype = [ ctypes.POINTER(ctypes.c_char), ctypes.c_uint, ctypes.POINTER(ctypes.POINTER(Drc2PyMesh)) ]
		self.drc_decode.restype = ctypes.c_uint
		# Mapping free funct
		self.drc_free = self.drc_lib.drc2py_free
		self.drc_free.argtype = ctypes.POINTER(ctypes.POINTER(Drc2PyMesh))
		self.drc_free.restype = None
		# Mapping encode funct
		self.drc_encode = self.drc_lib.drc2py_encode
		self.drc_encode.argtype = [ ctypes.POINTER(Drc2PyMesh), ctypes.c_char_p ]
		self.drc_encode.restype = ctypes.c_uint
		
	def decode(self, file_path):
		# Open drc file
		file = None
		bytes = None
		try:
			file = open(file_path, 'rb')
			bytes = file.read()
		except IOError as err:
			raise Exception('[ERROR] Failure opening file: ['+ os.path.realpath(file_path) + '] => ' + repr(err))
			return None
		finally:
			if file: file.close()
		
		# Decode drc file
		size = len(bytes)
		mesh_ptr = ctypes.POINTER(Drc2PyMesh)()
		self.drc_decode(bytes, size, ctypes.byref(mesh_ptr))
		mesh = mesh_ptr.contents
		
		# Preparing result to decouple from ctypes types
		result = type('', (), {})() 
		result.faces = mesh.faces[0:mesh.faces_num * 3]
		result.faces_len = mesh.faces_num * 3
		result.faces_num = mesh.faces_num
		
		result.vertices = mesh.vertices[0:mesh.vertices_num * 3]
		result.vertices_len = mesh.vertices_num * 3
		result.vertices_num = mesh.vertices_num
		
		result.normals = mesh.normals[0:mesh.normals_num * 3]
		result.normals_len = mesh.normals_num * 3
		result.normals_num = mesh.normals_num
		
		result.uvs = mesh.uvs[0:mesh.uvs_num * 2]
		result.uvs_len = mesh.uvs_num * 2
		result.uvs_num = mesh.uvs_num
		
		# Free memory allocated by the lib
		self.drc_free(ctypes.byref(mesh_ptr))
		mesh_ptr = None
		return result

	def encode(self, mesh_data, file):
		mesh = Drc2PyMesh()
		
		mesh.faces = array_to_ptr(mesh_data.faces, mesh_data.faces_num * 3, ctypes.c_uint)
		mesh.faces_num = ctypes.c_uint(mesh_data.faces_num)
		mesh.vertices = array_to_ptr(mesh_data.vertices, mesh_data.vertices_num * 3, ctypes.c_float)
		mesh.vertices_num = ctypes.c_uint(mesh_data.vertices_num)
		mesh.normals = array_to_ptr(mesh_data.normals, mesh_data.normals_num * 3, ctypes.c_float)
		mesh.normals_num = ctypes.c_uint(mesh_data.normals_num)
		mesh.uvs = array_to_ptr(mesh_data.uvs, mesh_data.uvs_num * 3, ctypes.c_float)
		mesh.uvs_num = ctypes.c_uint(mesh_data.uvs_num)
		
		mesh_ptr = ctypes.byref(mesh)
		file_ptr = ctypes.c_char_p(file.encode())
		self.drc_encode(mesh_ptr, file_ptr)

