import maya.api.OpenMaya as OpenMaya
import maya.OpenMayaMPx as OpenMayaMPx
import sys

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
        print(fileObject.fullName())
        vertices = [OpenMaya.MPoint(10, 10, 10), OpenMaya.MPoint(-10, -10, -10), OpenMaya.MPoint(0, 10, 0)]
        indices = [0, 1, 2]
        polyCount = [3]

        fnMesh = OpenMaya.MFnMesh()
        newMesh = fnMesh.create(vertices, polyCount, indices)
        fnMesh.updateSurface()

def translatorCreator():
    return OpenMayaMPx.asMPxPtr(DracoTranslator())
    

def initializePlugin(mobject):
    print('init')
    mplugin = OpenMayaMPx.MFnPlugin(
        mobject, "Autodesk", '0.1', "Any")

    try:
        mplugin.registerFileTranslator(
            'drc',
            None,
            translatorCreator)  # ,
        # kPluginTranslatorDefaultOptions)

    except Exception as e:
        sys.stderr.write("Failed to register command: %s\n" %
                        'drc')
        raise

def uninitializePlugin(mobject):
print('uninit')