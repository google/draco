import sys

import maya.api.OpenMaya as OpenMaya
import maya.OpenMayaMPx as OpenMayaMPx

from draco_maya_wrapper import Draco
from draco_maya_wrapper import DrcMesh

__author__ = "Mattia Pezzano, Federico de Felici, Duccio Lenkowicz"
__version__ = "0.1"

class DracoTranslator(OpenMayaMPx.MPxFileTranslator):
    def __init__(self):
        OpenMayaMPx.MPxFileTranslator.__init__(self)

    def maya_useNewAPI(self):
        return True

    def haveWriteMethod(self):
        return True

    def haveReadMethod(self):
        return True

    def filter(self):
        return "*.drc"

    def defaultExtension(self):
        return "drc"

    def writer(self, fileObject, optionString, accessMode):
        exportUVs = True
        exportNormals = True

        selection = OpenMaya.MGlobal.getActiveSelectionList()
        dagIterator = OpenMaya.MItSelectionList(selection, OpenMaya.MFn.kGeometric)

        drcIndices = []
        drcVertices = []
        drcNormals = []
        drcUvs = []
        indicesOffset = 0

        while not dagIterator.isDone():  
            dagPath = dagIterator.getDagPath()
            try:
                fnMesh = OpenMaya.MFnMesh(dagPath)
            except Exception as e:
                dagIterator.next()
                continue

            meshPoints = fnMesh.getPoints(OpenMaya.MSpace.kWorld)
            polygonsIterator = OpenMaya.MItMeshPolygon(dagPath)

            meshNormals = normals = u = v = uvSetsNames = uvs = None

            if exportNormals:
                meshNormals = fnMesh.getNormals()
                normals = {}    

            if exportUVs:
                uvSetsNames = fnMesh.getUVSetNames()
                u, v = fnMesh.getUVs(uvSetsNames[0])
                uvs = {}

            while not polygonsIterator.isDone():
                if not polygonsIterator.hasValidTriangulation():
                    raise ValueError("The mesh has not valid triangulation")
                polygonVertices = polygonsIterator.getVertices()
                numTriangles = polygonsIterator.numTriangles()
                
                localindices = []
                for i in range(numTriangles):
                    points, indices = polygonsIterator.getTriangle(i)
                    drcIndices.append(indicesOffset)
                    indicesOffset += 1
                    drcIndices.append(indicesOffset)
                    indicesOffset += 1
                    drcIndices.append(indicesOffset)
                    indicesOffset += 1
                    localindices.append(indices[0])
                    localindices.append(indices[1])
                    localindices.append(indices[2])

                    localIndex = []
                    for gt in range(len(indices)):
                        for gv in range(len(polygonVertices)):
                            if indices[gt] == polygonVertices[gv]:
                                localIndex.append(gv)
                                break
                    if exportNormals:
                        normals[indices[0]] = meshNormals[polygonsIterator.normalIndex(localIndex[0])]
                        normals[indices[1]] = meshNormals[polygonsIterator.normalIndex(localIndex[1])]
                        normals[indices[2]] = meshNormals[polygonsIterator.normalIndex(localIndex[2])]

                    if exportUVs and polygonsIterator.hasUVs():
                        uvID = [0, 0, 0]
                        for vtxInPolygon in range(3):
                            uvID[vtxInPolygon] = polygonsIterator.getUVIndex(localIndex[vtxInPolygon], uvSetsNames[0])
                        uvs[indices[0]] = (u[uvID[0]], v[uvID[0]])
                        uvs[indices[1]] = (u[uvID[1]], v[uvID[1]])
                        uvs[indices[2]] = (u[uvID[2]], v[uvID[2]])

                for i in localindices:
                    drcVertices.append(meshPoints[i].x)
                    drcVertices.append(meshPoints[i].y)
                    drcVertices.append(meshPoints[i].z)
                    if exportNormals:
                        drcNormals.append(normals[i][0])
                        drcNormals.append(normals[i][1])
                        drcNormals.append(normals[i][2])
                    if exportUVs and polygonsIterator.hasUVs():
                        drcUvs.append(uvs[i][0])
                        drcUvs.append(uvs[i][1])
                    
                polygonsIterator.next(None)
            dagIterator.next()


        drcMesh = DrcMesh()
        drcMesh.faces_num = len(drcIndices) / 3
        drcMesh.faces_len = len(drcIndices)
        drcMesh.faces = drcIndices

        drcMesh.vertices_num = len(drcVertices) / 3
        drcMesh.vertices_len = len(drcVertices)
        drcMesh.vertices = drcVertices

        if exportNormals:
            drcMesh.normals = drcNormals
            drcMesh.normals_len = len(drcNormals)
            drcMesh.normals_num = len(drcVertices) / 3

        if exportUVs:
            drcMesh.uvs = drcUvs
            drcMesh.uvs_len = len(drcUvs)
            drcMesh.uvs_num = len(drcVertices) / 3

        draco = Draco()
        draco.encode(drcMesh, fileObject.fullName())

    def reader(self, fileObject, optionString, accessMode):
        drc = Draco()
        mesh = drc.decode(fileObject.fullName())

        # vertices, normals, uvs
        vertices = []
        normals = []
        us = OpenMaya.MFloatArray()
        vs = OpenMaya.MFloatArray()
        poly_count = [3] * mesh.faces_num
        for n in range(mesh.vertices_num):
            i = 3 * n
            vertices.append(OpenMaya.MPoint(mesh.vertices[i], mesh.vertices[i + 1], mesh.vertices[i + 2]))
            if mesh.normals:
                normals.append(OpenMaya.MFloatVector(mesh.normals[i], mesh.normals[i + 1], mesh.normals[i + 2]))
            if mesh.uvs:
                i = 2 * n
                us.append(mesh.uvs[i])
                vs.append(mesh.uvs[i + 1])

        #create mesh
        fnMesh = OpenMaya.MFnMesh()
        newMesh = fnMesh.create(vertices, poly_count, mesh.faces)

        if mesh.normals:
            fnMesh.setVertexNormals(normals, range(len(vertices)))
        if mesh.uvs:
            uvSetsNames = fnMesh.getUVSetNames()
            fnMesh.setUVs(us, vs, uvSetsNames[0])
            fnMesh.assignUVs(poly_count, mesh.faces)
            
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
