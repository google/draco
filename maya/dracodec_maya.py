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
				("normals", ctypes.POINTER(ctypes.c_float))
			   ]

# TODO: Add integration for UNIX
class Draco:
	def __init__(self):
		# Lib loading
		dir_path = os.path.dirname(os.path.realpath(__file__))
		lib_path = os.path.join(dir_path, 'dracodec_maya')
		self.drc_lib = ctypes.CDLL(lib_path)
		# Mapping free funct
		self.drc_decode = self.drc_lib.drc2py_decode
		self.drc_decode.argtype = [ ctypes.POINTER(ctypes.c_char), ctypes.c_uint, ctypes.POINTER(ctypes.POINTER(Drc2PyMesh)) ]
		self.drc_decode.restype = ctypes.c_uint
		# Mapping free funct
		self.drc_free = self.drc_lib.drc2py_free
		self.drc_free.argtype = ctypes.POINTER(ctypes.POINTER(Drc2PyMesh))
		self.drc_free.restype = None
		
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
		
		# Free memory allocated by the lib
		self.drc_free(ctypes.byref(mesh_ptr))
		mesh_ptr = None
		return result;
