"""namespace config, 'cause the c++ side doesn't do it too nicely"""

from __main__ import _pythonscriptmodule
from __main__ import _naali
import rexviewer as r #the old module is still used , while porting away from it
import sys #for stdout redirecting
#from _naali import *

# for core types like PythonQt.private.AttributeChange
import PythonQt

#\todo: do we have version num info somewhere?
#version = XXX

"""
Core API
"""
frame = _naali.Frame()
console = _naali.Console()
input = _naali.Input()
scene = _naali.Scene()
audio = _naali.Audio()
ui = _naali.Ui()
debug = _naali.Debug()
devices = _naali.Devices()

"""Aka. Tundra
The API object is there also in client mode, used to check if is running as server or not""" 
server = _naali.server 

client = _naali.client #now for web login ui
framework = _naali #idea was that 'framework' is not in core api, but is used anyhow? (someone added this)

"""some basic types"""
from PythonQt.private import Vector3df, Quaternion, AttributeChange, DelayedSignal, KeyEvent, Transform
#NOTE: could say 'from PythonQt.private import *' but that would bring e.g. Core API classes, which are not singletons

class Logger:
    def __init__(self):
        self.buf = ""

    def write(self, msg):
        self.buf += msg
        while '\n' in self.buf:
            line, self.buf = self.buf.split("\n", 1)
            try:
                r.logInfo(line)
            except ValueError:
                pass #somehow this isn't a string always
                r.logInfo("logging prob from py print: is not a string? " + str(type(line)))

sys.stdout = Logger()

#XXX do we actually want these style changes,
#or is it better to just call the slots directly
# module methods
runjs = _pythonscriptmodule.RunJavascriptString

def getScene(name):
    return _pythonscriptmodule.GetScene(name)
    
def getDefaultScene():
    return _naali.Scene().GetDefaultSceneRaw()
    
def createEntity(comptypes = [], localonly = False, sync = True, temporary = False):
    s = getDefaultScene()
    # create entity
    if localonly:
        ent = s.CreateEntityLocalRaw(comptypes)
    else:
        ent = s.CreateEntityRaw(0, comptypes, AttributeChange.Replicate, sync)
    # set temporary
    if temporary:
        ent.SetTemporary(True)
    # create components
    for comptype in comptypes:
        comp = ent.GetOrCreateComponentRaw(comptype)
        if temporary:
            comp.SetTemporary(True)
    s.EmitEntityCreatedRaw(ent)
    return ent

def createMeshEntity(meshname, raycastprio=1, additional_comps = [], localonly = False, sync = True, temporary = False):
    components = ["EC_Placeable", "EC_Mesh", "EC_Name"] + additional_comps
    ent = createEntity(components, localonly, sync, temporary)
    ent.placeable.SelectPriority = raycastprio
    ent.mesh.SetPlaceable(ent.placeable)
    ent.mesh.SetMesh(meshname)
    return ent

#XXX check how to do with SceneManager
def removeEntity(entity):
    r.removeEntity(entity.id)
    
# using the scene manager, note above
def deleteEntity(entity):
    s = getDefaultScene()
    s.DeleteEntityById(entity.id)

def createInputContext(name, priority = 100):
    return _pythonscriptmodule.CreateInputContext(name, priority)

# module variables
renderer = _pythonscriptmodule.GetRenderer()
worldlogic = _pythonscriptmodule.GetWorldLogic()
inputcontext = _pythonscriptmodule.GetInputContext()
mediaplayerservice = _pythonscriptmodule.GetMediaPlayerService()

# the comms service is optional. If it's not present we'll leave it
# undefined.
try:
    communicationsservice = _pythonscriptmodule.GetCommunicationsService()
except AttributeError:
    pass

# Returns EntityAction pointer by the name
def action(self, name):
    return self.qent.Action(name)
    
def getEntity(entid):
    qent = getDefaultScene().GetEntityRaw(entid)
    if qent is None:
        raise ValueError, "No entity with id %d" % entid
    return qent

#helper funcs to hide api indirections/inconsistenties that were hard to fix,
#-- these allow to remove the old corresponding hand written c funcs in pythonscriptmodule.cpp
def _getAsPyEntity(qentget, errmsg):
    qent = qentget()
    if qent is not None:
        #print qent.id
        return qent
    else:
        raise ValueError, errmsg

def getUserAvatar():
    return _getAsPyEntity(worldlogic.GetUserAvatarEntityRaw, "No avatar. No scene, not logged in?")
        
def getCamera():
    return _getAsPyEntity(_pythonscriptmodule.GetActiveCamera, "No default camera. No scene?")
