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
        # Get the selection and create a selection list of all the nodes meshes
        selection = OpenMaya.MSelectionList()
        selection = OpenMaya.MGlobal.getActiveSelectionList()

        # Create an MItSelectionList class to iterate over the selection
        # Use the MFn class to as a filter to filter node types
        iter = OpenMaya.MItSelectionList(selection, OpenMaya.MFn.kGeometric)

        # This uses built in functions of the MItSelectionList class to loop through the list of objects
        # NB: isn't a basic array, you must use the built in functions
        while not iter.isDone():  # iterate selection

            vertexList = []
            normalList = []
            edgeList = []
            polytriVertsList = []
            polyList = []
            connectedPolyList = []

            # get dag path of current iterated selection
            dagPath = OpenMaya.MDagPath()
            dagPath = iter.getDagPath()

            # get the selection as an MObject
            mObj = OpenMaya.MObject()
            mObj = iter.getDependNode()

            # create iterator of current mesh polygons
            polygonsIterator = OpenMaya.MItMeshPolygon(mObj)

            # Iterate through polygons on current mesh
            while not polygonsIterator.isDone():

                # Get current polygons index
                polyList.append(polygonsIterator.index())

                # Get current polygons vertices
                verts = OpenMaya.MIntArray()
                verts = polygonsIterator.getVertices()

                normalIndices = []
                # Append the current polygons vertex indices
                for i in range(len(verts)):
                    vertexList.append(verts[i])

                # NOT VALID CODE
                # for i in range(0,len(vertexList), 3):
                # need vertex this is a float
                # vertex = OpenMaya.MFloatPoint(vertexList[i], vertexList[i+1], vertexList[i+2])
                # normalIndices.append(polygonsIterator.normalIndex(vertex))  #return the index in the normals buffer

                # Get current polygons edges
                edges = OpenMaya.MIntArray()
                edges = polygonsIterator.getEdges()

                # Append the current polygons edge indices
                for i in range(len(edges)):
                    edgeList.append(edges[i])

                # Get current polygons connected faces
                indexConnectedFaces = OpenMaya.MIntArray()
                indexConnectedFaces = polygonsIterator.getConnectedFaces()

                # Append the connected polygons indices
                for i in range(len(indexConnectedFaces)):
                    connectedPolyList.append(indexConnectedFaces[i])

                # Get current polygons triangles
                pointArray = OpenMaya.MPointArray()
                intArray = OpenMaya.MIntArray()
                space = OpenMaya.MSpace.kObject

                # Get the vertices and vertex positions of all the triangles in the current face's triangulation.
                pointArray, intArray = polygonsIterator.getTriangles(space)

                # Append vertices that are part of the triangles
                for i in range(len(intArray)):
                    polytriVertsList.append(intArray[i])

                # next poligon
                polygonsIterator.next(None)  # idk what arguments i need to pass here

            drcMesh = DrcMesh()
            drcMesh.faces_num = len(polytriVertsList) / 3
            drcMesh.faces_len = len(polytriVertsList)
            drcMesh.faces = polytriVertsList

            drcMesh.vertices_num = len(vertexList) / 3
            drcMesh.vertices_len = len(vertexList)
            drcMesh.vertices = vertexList

            draco = Draco()
            draco.encode(drcMesh, fileObject)

            # print data for current selection being iterated on,
            print ("Object name: {}".format(dagPath.fullPathName()))
            print ("Vertex list: {} size: {}".format(vertexList, len(vertexList)))
            print ("Edge list: {} size: {}".format(edgeList, len(edgeList)))
            print ("Poly Triangle Vertices: ***INDEX BUFFER*** {} size: {}".format(polytriVertsList,
                                                                                   len(polytriVertsList)))
            print ("Polygon index list: {} size: {}".format(polyList, len(polyList)))
            print ("Connected Polygons list: {} size: {}".format(connectedPolyList, len(connectedPolyList)))

            # next selected
            iter.next()

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
