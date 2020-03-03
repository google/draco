import unittest
import os
import sys
dir_path = os.path.dirname(os.path.realpath(__file__))
root_path = os.path.join(dir_path, '../draco_maya')
sys.path.insert(0, root_path)

def file_del(file_path):
	if os.path.isfile(file_path): 
		os.remove(file_path)


from draco_maya_wrapper import Draco, DrcMesh

class DracoTest(unittest.TestCase):
	
	def setUp(self): 
		self.drc = Draco()

	def test_decode_bunny_drc(self):
		mesh = self.drc.decode(os.path.join(dir_path, 'res/bunny.drc'))
		# Faces check
		self.assertEqual(69451, mesh.faces_num, 'Number of faces')
		self.assertEqual(208353, mesh.faces_len,'Length of faces array precalculated')
		self.assertEqual(208353, len(mesh.faces),'Length of faces array by len')
		# Vertices check
		self.assertEqual(34834, mesh.vertices_num, 'Number of vertices')
		self.assertEqual(104502, mesh.vertices_len,'Length of vertices array precalculated')
		self.assertEqual(104502, len(mesh.vertices),'Length of vertices array by len')
		# Normals check
		self.assertEqual(0, mesh.normals_num, 'Number of normals')
		self.assertEqual(0, mesh.normals_len,'Length of normals array precalculated')
		self.assertEqual(0, len(mesh.normals),'Length of normals array by len')
		# Uvs check
		self.assertEqual(0, mesh.uvs_num, 'Number of uvs')
		self.assertEqual(0, mesh.uvs_len,'Length of uvs ')
		self.assertEqual(0, len(mesh.uvs),'Length of uvs array by len')

	def test_decode_trooper_drc(self):
		mesh = self.drc.decode(os.path.join(dir_path, 'res/stormtrooper.drc'))
		# Faces check
		self.assertEqual(6518, mesh.faces_num, 'Number of faces')
		self.assertEqual(19554, mesh.faces_len,'Length of faces array precalculated')
		self.assertEqual(19554, len(mesh.faces),'Length of faces array by len')
		# Vertices check
		self.assertEqual(5176, mesh.vertices_num, 'Number of vertices')
		self.assertEqual(15528, mesh.vertices_len,'Length of vertices array precalculated')
		self.assertEqual(15528, len(mesh.vertices),'Length of vertices array by len')
		# Normals check
		self.assertEqual(5176, mesh.normals_num, 'Number of normals')
		self.assertEqual(15528, mesh.normals_len, 'Length of normals array precalculated')
		self.assertEqual(15528, len(mesh.normals),'Length of normals array by len')
		# Uvs check
		self.assertEqual(5176, mesh.uvs_num, 'Number of uvs')
		self.assertEqual(10352, mesh.uvs_len, 'Length of uvs array')
		self.assertEqual(10352, len(mesh.uvs), 'Length of uvs array by len')

	def test_decode_unexistent_drc(self):
		self.assertRaises(Exception, self.drc.decode, 'res/unexistent.drc')

	def test_encode_triangle_mesh(self):
		mesh = DrcMesh()
		mesh.faces = [0, 1, 2]
		mesh.faces_num = 1
		mesh.vertices = [0, 0, 0, 1, 1, 1, 2, 2, 2]
		mesh.vertices_num = 3
		
		file = os.path.join(dir_path,'triangle.drc')
		file_del(file)
		
		self.drc.encode(mesh, file)
		self.assertTrue(os.path.isfile(file), 'File should exists!')
		
		file_del(file)
		
	def test_encode_and_decode_triangle_mesh(self):
		mesh = DrcMesh()
		mesh.faces = [0, 1, 2]
		mesh.faces_num = 1
		mesh.vertices = [0, 0, 0, 1, 1, 1, 2, 2, 2]
		mesh.vertices_num = 3
		file = os.path.join(dir_path, 'res/triangle.drc')
		file_del(file)
		self.drc.encode(mesh, file)
		
		dmesh = self.drc.decode(file)
		# Faces check
		self.assertEqual(1, dmesh.faces_num, 'Number of faces')
		self.assertEqual(3, dmesh.faces_len,'Length of faces array precalculated')
		self.assertEqual([0, 1, 2], dmesh.faces, 'Face Array')
		# Vertices check
		self.assertEqual(3, dmesh.vertices_num, 'Number of vertices')
		self.assertEqual(9, dmesh.vertices_len,'Length of vertices array precalculated')
		self.assertEqual([0, 0, 0, 1, 1, 1, 2, 2, 2], dmesh.vertices, 'Vertex Array')

		file_del(file)

	def test_decode_and_encode_stoormtrup_drc(self):
		# Step1: decode
		mesh = self.drc.decode(os.path.join(dir_path, 'res/stormtrooper.drc'))
		# Step2: encode
		file = os.path.join(dir_path, 'res/stormtrooper_copy.drc')
		file_del(file)
		self.drc.encode(mesh, file)
		# Step3: re-decode and test
		dmesh = self.drc.decode(file)
		#    Faces check
		self.assertEqual(6518, dmesh.faces_num, 'Number of faces')
		self.assertEqual(19554, dmesh.faces_len,'Length of faces array precalculated')
		self.assertEqual(19554, len(dmesh.faces),'Length of faces array by len')
		#    Vertices check
		self.assertEqual(5176, dmesh.vertices_num, 'Number of vertices')
		self.assertEqual(15528, dmesh.vertices_len,'Length of vertices array precalculated')
		self.assertEqual(15528, len(dmesh.vertices),'Length of vertices array by len')
		#    Normals check
		self.assertEqual(5176, dmesh.normals_num, 'Number of normals')
		self.assertEqual(15528, dmesh.normals_len, 'Length of normals array precalculated')
		self.assertEqual(15528, len(dmesh.normals),'Length of normals array by len')

		file_del(file)

	
if __name__ == '__main__':
    unittest.main()