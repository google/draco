import sys

import maya.api.OpenMaya as OpenMaya
import maya.OpenMayaMPx as OpenMayaMPx

from draco_maya_wrapper import Draco

__author__ = "Mattia Pezzano, Federico de Felici, Duccio Lenkowicz"
__version__ = "0.1"

class DracoTranslator(OpenMayaMPx.MPxFileTranslator):
    def __init__(self):
        OpenMayaMPx.MPxFileTranslator.__init__(self)

    def maya_useNewAPI():
        pass

    def haveWriteMethod(self):
        return False

    def haveReadMethod(self):
        return True

    def filter(self):
        return "*.drc"

    def defaultExtension(self):
        return "drc"

    def reader(self, fileObject, optionString, accessMode):
        drc = Draco()
        mesh = drc.decode(fileObject.fullName())

        # vertices, normals, uvs
        vertices = []
        normals = []
        us = OpenMaya.MFloatArray()
        vs = OpenMaya.MFloatArray()
        newMesh = None
        fnMesh = OpenMaya.MFnMesh()
        for n in range(mesh.vertices_num):
            i = 3 * n
            vertices.append(OpenMaya.MPoint(mesh.vertices[i], mesh.vertices[i + 1], mesh.vertices[i + 2]))
            if mesh.normals:
                normals.append(OpenMaya.MFloatVector(mesh.normals[i], mesh.normals[i + 1], mesh.normals[i + 2]))
            if mesh.uvs:
                i = 2 * n
                us.append(mesh.uvs[i])
                vs.append(mesh.uvs[i + 1])

        #indices
        indices = []
        for index in mesh.faces:
            indices.append(index)
        poly_count = [3] * mesh.faces_num

        #create mesh
        if mesh.uvs:
            #TODO: ensure the mesh actually uses the uvs inside maya
            newMesh = fnMesh.create(vertices, poly_count, indices, uValues=us, vValues=vs)
        else:
            newMesh = fnMesh.create(vertices, poly_count, indices)

        if mesh.normals:
            fnMesh.setVertexNormals(normals, range(len(vertices)))
            
        fnMesh.updateSurface()

        slist = OpenMaya.MGlobal.getSelectionListByName("initialShadingGroup")
        initialSG = slist.getDependNode(0)

        fnSG = OpenMaya.MFnSet(initialSG)
        if fnSG.restriction() == OpenMaya.MFnSet.kRenderableOnly:
            fnSG.addMember(newMesh)


def translatorCreator():
    return OpenMayaMPx.asMPxPtr(DracoTranslator())
    

def initializePlugin(mobject):
    print('init')
    mplugin = OpenMayaMPx.MFnPlugin(
        mobject, "Autodesk", __version__, "Any")

    try:
        mplugin.registerFileTranslator(
            'drc',
            None,
            translatorCreator)  

    except Exception as e:
        sys.stderr.write("Failed to register command: drc\n")
        raise

def uninitializePlugin(mobject):
    print('uninit')
