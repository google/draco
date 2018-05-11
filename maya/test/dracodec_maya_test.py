import unittest
import os
import sys
dir_path = os.path.dirname(os.path.realpath(__file__))
root_path = os.path.join(dir_path, '..')
sys.path.insert(0, root_path)


from dracodec_maya import Draco

class DracoTest(unittest.TestCase):
	
	def setUp(self): 
		self.drc = Draco()

	def test_valid_bunny_drc(self):
		mesh = self.drc.decode(os.path.join(dir_path, 'bunny.drc'))
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

	def test_valid_trooper_drc(self):
		mesh = self.drc.decode(os.path.join(dir_path, 'stormtrooper.drc'))
		# Normal check
		self.assertGreater(mesh.normals_num, 0, 'Number of normals')
		self.assertGreater(mesh.normals_len, 0,'Length of normals array')
		self.assertGreater(len(mesh.normals), 0,'Length of normals array by len')
		# Uvs check
		self.assertGreater(mesh.uvs_num, 0, 'Number of uvs')
		self.assertGreater(mesh.uvs_len, 0,'Length of uvs array')
		self.assertGreater(len(mesh.uvs), 0,'Length of uvs array by len')

	def test_unexistent_drc(self):
		self.assertRaises(Exception, self.drc.decode, 'unexistent.drc')
		
if __name__ == '__main__':
    unittest.main()