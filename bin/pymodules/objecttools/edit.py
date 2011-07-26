from __future__ import division

from circuits import Component
import mathutils as mu

from PythonQt.QtUiTools import QUiLoader
from PythonQt.QtCore import QFile, Qt, QRect
from PythonQt.QtGui import QVector3D, QAction, QIcon
from PythonQt.QtGui import QQuaternion as QQuaternion

import rexviewer as r
import naali
from naali import renderer #naali.renderer for FrustumQuery, hopefully all ex-rexviewer things soon

try:
    #window
    transform
except: #first run
    try:
        #import window
        import transform
    except ImportError, e:
        print "couldn't load window and transform:", e
else:
    #window = reload(window)
    transform= reload(transform)

# note: difference from develop: in tundra there is no EC_OpenSimPrim, so we assume
# everything that is placeable is editable
def editable(ent):
    return ent.HasComponent('EC_Placeable')
    
class ObjectEdit(Component):
    EVENTHANDLED = False
 
    UPDATE_INTERVAL = 0.05 #how often the networkUpdate will be sent
    
    MANIPULATE_FREEMOVE = 0
    MANIPULATE_MOVE = 1
    MANIPULATE_SCALE = 2
    MANIPULATE_ROTATE = 3
    CREATE_ICONS = True
    
    def __init__(self):
        self.sels = []  
        self.selmasses = {}
        Component.__init__(self)
        self.resetValues()
        self.worldstream = r.getServerConnection()
        self.usingManipulator = False
        self.useLocalTransform = False
        #self.cpp_python_handler = None
        self.left_button_down = False
        self.keypressed = False
        self.editing = False
                
        self.editingKeyTrigger = (Qt.Key_M, Qt.ShiftModifier)
        self.shortcuts = {
            self.editingKeyTrigger : self.toggleEditingKeyTrigger,
            (Qt.Key_R, Qt.NoModifier) : self.rotateObject,
            (Qt.Key_S, Qt.NoModifier) : self.scaleObject,
            (Qt.Key_G, Qt.NoModifier) : self.translateObject,
            (Qt.Key_Tab, Qt.NoModifier) : self.cycleManipulator,
            (Qt.Key_Z, Qt.ControlModifier) : self.undo,
            (Qt.Key_Delete, Qt.NoModifier) : self.deleteObject,
            (Qt.Key_L, Qt.AltModifier) : self.linkObjects,
            (Qt.Key_L, Qt.ControlModifier|Qt.ShiftModifier) : self.unlinkObjects,
        }

        # Connect to key pressed signal from input context
        self.edit_inputcontext = naali.createInputContext("object-edit", 100)
        self.edit_inputcontext.SetTakeMouseEventsOverQt(True)
        #print "connecting to",self.edit_inputcontext
        self.edit_inputcontext.connect('KeyPressed(KeyEvent*)', self.on_keypressed)

        # Connect to mouse events
        self.edit_inputcontext.connect('MouseScroll(MouseEvent*)', self.on_mousescroll)
        self.edit_inputcontext.connect('MouseLeftPressed(MouseEvent*)', self.on_mouseleftpressed)
        self.edit_inputcontext.connect('MouseLeftReleased(MouseEvent*)', self.on_mouseleftreleased)
        self.edit_inputcontext.connect('MouseMove(MouseEvent*)', self.on_mousemove)
        
        self.resetManipulators()
        
        self.selection_rect = QRect()
        self.selection_rect_startpos = None
        
        r.c = self #this is for using objectedit from command.py

        self.selection_box_entity = None
        self.selection_box = None
        self.selection_box_inited = False
        
        self.menuToggleAction = None
        mainWindow = naali.ui.MainWindow()
        if mainWindow:
            self.menuToggleAction = QAction(QIcon("./data/ui/images/worldbuilding/transform-move.png"), "Toggle Object Manipulation", 0)
            self.menuToggleAction.connect("triggered()", self.toggleEditingKeyTrigger)
            if naali.server.IsAboutToStart() == False:
                naali.client.connect("Connected()", self.on_connected_tundra)
                naali.client.connect("Disconnected()", self.on_disconnected_tundra)
            else:
                editMenu = mainWindow.AddMenu("Edit")
                editMenu.addAction(self.menuToggleAction)
        self.toggleEditing(False)
        
        """
        # Get world building modules python handler, is not present in tundra
        self.cpp_python_handler = r.getQWorldBuildingHandler()
        if self.cpp_python_handler != None:
            # Connect signals
            self.cpp_python_handler.connect('ActivateEditing(bool)', self.on_activate_editing)
            self.cpp_python_handler.connect('ManipulationMode(int)', self.on_manupulation_mode_change)
            self.cpp_python_handler.connect('RemoveHightlight()', self.deselect_all)
            self.cpp_python_handler.connect('RotateValuesToNetwork(int, int, int)', self.changerot_cpp)
            self.cpp_python_handler.connect('ScaleValuesToNetwork(double, double, double)', self.changescale_cpp)
            self.cpp_python_handler.connect('PosValuesToNetwork(double, double, double)', self.changepos_cpp)
            self.cpp_python_handler.connect('CreateObject()', self.createObject)
            self.cpp_python_handler.connect('DuplicateObject()', self.duplicate)
            self.cpp_python_handler.connect('DeleteObject()', self.deleteObject)
            # Check if build mode is active, required on python restarts
            self.on_activate_editing(self.cpp_python_handler.IsBuildingActive())
        """
        
    def on_disconnected_tundra(self):
        if naali.client.hasConnections() == False:
            self.CREATE_ICONS = True

    def on_connected_tundra(self):
        if self.CREATE_ICONS:
            naali.ui.EmitAddAction(self.menuToggleAction); self.CREATE_ICONS = False

    def toggleEditingKeyTrigger(self):
        self.toggleEditing(not self.editing)
        
    def toggleEditing(self, editing):
        self.editing = editing
        if not self.editing:
            self.deselect_all()
            self.hideManipulator()
            self.resetValues()
    """
        if self.menuToggleAction != None:
            if self.editing:
                self.menuToggleAction.setToolTip("Disable Object Manipulation")
                self.menuToggleAction.setText("Disable Object Manipulation")
            else:
                self.menuToggleAction.setToolTip("Enable Object Manipulation")
                self.menuToggleAction.setText("Disable Object Manipulation")
    """
    
    def rotateObject(self):
        self.changeManipulator(self.MANIPULATE_ROTATE)

    def scaleObject(self):
        self.changeManipulator(self.MANIPULATE_SCALE)

    def translateObject(self):
        self.changeManipulator(self.MANIPULATE_MOVE)

    def on_keypressed(self, k):
        #print "on_keypressed",k,k.keyCode,k.modifiers
        trigger = (k.keyCodeInt(), k.modifiers)
        # if not in editing mode ignore other key combinations than to enable editing
        if not self.editing and trigger != self.editingKeyTrigger:
            return
        # update manip for constant size
        self.manipulator.showManipulator(self.sels)
        # check to see if a shortcut we understand was pressed, if so, trigger it
        if trigger in self.shortcuts:
            self.keypressed = True
            self.shortcuts[trigger]()

    def on_mousescroll(self, m):
        if not self.editing:
            return
        #self.manipulator.showManipulator(self.sels) # what is this supposed to do?
        self.cycleManipulator()
        
    def resetValues(self):
        for ent in self.selmasses.iterkeys():
            if ent.rigidbody == None:
                continue
            try:
                mass = self.selmasses[ent]
                ent.rigidbody.mass = mass
            except:
                continue
        self.selmasses.clear()
        self.left_button_down = False
        self.sel_activated = False #to prevent the selection to be moved on the intial click
        self.prev_mouse_abs_x = 0
        self.prev_mouse_abs_y = 0
        self.dragging = False
        self.time = 0
        self.keypressed = False
        self.windowActive = False
        self.windowActiveStoredState = None
        self.canmove = False
        self.selection_rect_startpos = None
    
    def resetManipulators(self):
        self.manipulatorsInit = False
        self.manipulators = {}
        self.manipulators[self.MANIPULATE_MOVE] =  transform.MoveManipulator()
        self.manipulators[self.MANIPULATE_SCALE] =  transform.ScaleManipulator()
        self.manipulators[self.MANIPULATE_FREEMOVE] =  transform.FreeMoveManipulator()
        self.manipulators[self.MANIPULATE_ROTATE] =  transform.RotationManipulator()
        self.manipulator = self.manipulators[self.MANIPULATE_FREEMOVE]
 
    def getCurrentManipType(self):
        for (type, manip) in self.manipulators.iteritems():
            if manip == self.manipulator:
                return type
        return self.MANIPULATE_FREEMOVE
        
    def baseselect(self, ent):
        self.sel_activated = False
        self.highlight(ent)
        self.ec_selected(ent)
        self.changeManipulator(self.getCurrentManipType())
        return ent
        
    def select(self, ent):        
        self.deselect_all()
        ent = self.baseselect(ent)
        self.sels.append(ent)
        self.canmove = True
        try:
            rigid = ent.rigidbody
            if not ent in self.selmasses or rigid.mass != 0:
                self.selmasses[ent] = rigid.mass
                ent.rigidbody.mass = 0
        except:
            pass

    def multiselect(self, ent):
        self.sels.append(ent)
        ent = self.baseselect(ent)
        try:
            rigid = ent.rigidbody
            if not ent in self.selmasses or rigid.mass != 0:
                self.selmasses[ent] = rigid.mass
                ent.rigidbody.mass = 0
        except:
            pass
    
    def deselect(self, ent, valid=True):
        if valid: #the ent is still there, not already deleted by someone else
            self.remove_highlight(ent)
            self.remove_selected(ent)
        if ent in self.sels:
            self.sels.remove(ent)

    def deselect_all(self):
        if len(self.sels) > 0:
            for ent in self.sels:
                self.remove_highlight(ent)
                self.remove_selected(ent)
                #try:
                #    self.worldstream.SendObjectDeselectPacket(ent.id)
                #except ValueError:
                #    r.logInfo("objectedit.deselect_all: entity doesn't exist anymore")
            self.sels = []
            self.hideManipulator()
            self.prev_mouse_abs_x = 0
            self.prev_mouse_abs_y = 0
            self.canmove = False
            #self.window.deselected()
            
    def highlight(self, ent):
        try:
            ent.highlight
        except AttributeError:
            ent.GetOrCreateComponentRaw("EC_Highlight")

        h = ent.highlight
    
        if not h.IsVisible():
            h.Show()
        else:
            r.logInfo("objectedit.highlight called for an already hilited entity: %d" % ent.id)
    
    # todo rename to something more sane, or check if this can be merged with def select() elsewhere
    def ec_selected(self, ent):
        try:
            s = ent.selected
        except AttributeError:
            s = ent.GetOrCreateComponentRaw("EC_Selected")
            
    def remove_selected(self, ent):
        try:
            s = ent.selected
        except:
            try:
                r.logInfo("objectedit.remove_selected called for a non-selected entity: %d" % ent.id)
            except ValueError:
                r.logInfo("objectedit.remove_selected called, but entity already removed")
        else:
            ent.RemoveComponentRaw(s)
    
            
    def remove_highlight(self, ent):
        try:
            h = ent.highlight
        except AttributeError:
            try:
                r.logInfo("objectedit.remove_highlight called for a non-hilighted entity: %d" % ent.id)
            except ValueError:
                r.logInfo("objectedit.remove_highlight called, but entity already removed")
        else:
            ent.RemoveComponentRaw(h)

    def cycleManipulator(self):
        if self.manipulator == self.manipulators[self.MANIPULATE_FREEMOVE]:
            self.changeManipulator(self.MANIPULATE_MOVE)
        elif self.manipulator == self.manipulators[self.MANIPULATE_MOVE]:
            self.changeManipulator(self.MANIPULATE_SCALE)
        elif self.manipulator == self.manipulators[self.MANIPULATE_SCALE]:
            self.changeManipulator(self.MANIPULATE_ROTATE)
        elif self.manipulator == self.manipulators[self.MANIPULATE_ROTATE]:
            self.changeManipulator(self.MANIPULATE_FREEMOVE)

    def changeManipulator(self, id):
        newmanipu = self.manipulators[id]
        if newmanipu.NAME != self.manipulator.NAME:
            #r.logInfo("was something completely different")
            self.manipulator.hideManipulator()
            self.manipulator = newmanipu
        self.manipulator.showManipulator(self.sels)
    
    def hideManipulator(self):
        if self.manipulator:
            self.manipulator.hideManipulator()

    def getSelectedObjectIds(self):
        ids = []
        for ent in self.sels:
            qprim = ent.prim
            children = qprim.GetChildren()
            for child_id in children:
                #child =  naali.getEntity(int(child_id))
                id = int(child_id)
                if id not in ids:
                    ids.append(id)
            ids.append(ent.id)
        return ids
    
    def linkObjects(self):
        ids = self.getSelectedObjectIds()
        #self.worldstream.SendObjectLinkPacket(ids)
        self.deselect_all()
        
    def unlinkObjects(self):
        ids = self.getSelectedObjectIds()
        #self.worldstream.SendObjectDelinkPacket(ids)
        self.deselect_all()

    def init_selection_box(self):
        # create a temporary selection box, we dont want to sync this for now
        # as there is no deletion code when you leave the server!
        self.selection_box_entity = naali.createEntity(comptypes = ["EC_SelectionBox", "EC_Name"], localonly = True, temporary = True)
        self.selection_box_entity.SetName("Python Selection Box")
        self.selection_box = self.selection_box_entity.GetOrCreateComponentRaw('EC_SelectionBox')
        self.selection_box.Hide()
        self.selection_box_inited = True

    def on_mouseleftpressed(self, mouseinfo):
        if not self.editing:
            return       
        if mouseinfo.IsItemUnderMouse():
            return
        if not self.selection_box_inited:
            self.init_selection_box()
        if mouseinfo.HasShiftModifier() and not mouseinfo.HasCtrlModifier() and not mouseinfo.HasAltModifier():
            self.on_multiselect(mouseinfo)
            return
            
        self.dragStarted(mouseinfo) #need to call this to enable working dragging
        self.left_button_down = True

        results = []
        results = r.rayCast(mouseinfo.x, mouseinfo.y)
        ent = None
        if results is not None and results[0] != 0:
            id = results[0]
            ent = naali.getEntity(id)

        if not self.manipulatorsInit:
            self.manipulatorsInit = True
            for manipulator in self.manipulators.values():
                manipulator.initVisuals()

        if ent is not None:
            if editable(ent):
                r.eventhandled = self.EVENTHANDLED
                if self.manipulator.compareIds(ent.id): # don't start selection box when manipulator is hit
                    self.manipulator.initManipulation(ent, results, self.sels)
                    self.usingManipulator = True
                else:
                    if self.active is None or self.active.id != ent.id: #a diff ent than prev sel was changed  
                        if not ent in self.sels:
                            self.select(ent)
                    elif self.active.id == ent.id: #canmove is the check for click and then another click for moving, aka. select first, then start to manipulate
                        self.canmove = True
        else:
            self.hideManipulator()
            # Start box select if nothing was hit? :P cant follow the logic here...
            self.selection_rect_startpos = (mouseinfo.x, mouseinfo.y)
            self.selection_box.Show()
            self.canmove = False
            self.deselect_all()
            
    def dragStarted(self, mouseinfo):
        width, height = renderer.GetWindowWidth(), renderer.GetWindowHeight()
        normalized_width = 1 / width
        normalized_height = 1 / height
        mouse_abs_x = normalized_width * mouseinfo.x
        mouse_abs_y = normalized_height * mouseinfo.y
        self.prev_mouse_abs_x = mouse_abs_x
        self.prev_mouse_abs_y = mouse_abs_y

    def on_mouseleftreleased(self, mouseinfo):
        if not self.editing:
            return
        self.left_button_down = False
        if self.active: #XXX something here?
            self.sel_activated = True
        
        if self.dragging:
            self.dragging = False

        if self.selection_rect_startpos is not None:
            hits = renderer.FrustumQuery(self.selection_rect)
            
            for hit in hits:
                if hit in self.sels: continue
                if not editable(hit): continue
                try:
                    self.multiselect(hit)
                except ValueError:
                    pass

            self.selection_rect_startpos = None
            self.selection_rect.setRect(0,0,0,0)
            self.selection_box.Hide()
            
        self.manipulator.stopManipulating()
        self.manipulator.showManipulator(self.sels)
        self.usingManipulator = False
    
    def selectionRectDimensions(self, mouseinfo):
        rectx = self.selection_rect_startpos[0]
        recty = self.selection_rect_startpos[1]
        
        rectwidth = (mouseinfo.x - rectx)
        rectheight = (mouseinfo.y - recty)
        
        if rectwidth < 0:
            rectx += rectwidth
            rectwidth *= -1
            
        if rectheight < 0:
            recty += rectheight
            rectheight *= -1
            
        return rectx, recty, rectwidth, rectheight

    def on_multiselect(self, mouseinfo):
        if not self.editing:
            return
        results = []
        results = r.rayCast(mouseinfo.x, mouseinfo.y)
        ent = None
        if results is not None and results[0] != 0:
            id = results[0]
            ent = naali.getEntity(id)
            
        found = False
        if ent is not None:                
            for entity in self.sels:
                if entity.id == ent.id:
                    found = True
            if not found:
                self.multiselect(ent)
            else:
                self.deselect(ent)
            self.canmove = True
            
    def on_mousemove(self, mouseinfo):
        if not self.editing:
            return
        """Handle mouse move events. When no button is pressed, just check
        for hilites necessity in manipulators. When a button is pressed, handle
        drag logic."""
        if self.left_button_down:
            self.on_mousedrag(mouseinfo)
        else:
            # check for manipulator hilites
            results = []
            results = r.rayCast(mouseinfo.x, mouseinfo.y)
            if results is not None and results[0] != 0:
                id = results[0]
                
                if self.manipulator.compareIds(id):
                    self.manipulator.highlight(results)
            else:
                self.manipulator.resethighlight()

    def on_mousedrag(self, mouseinfo):
        if not self.editing:
            return
        """dragging objects around - now free movement based on view,
        dragging different axis etc in the manipulator to be added."""
        width, height = renderer.GetWindowWidth(), renderer.GetWindowHeight()
        normalized_width = 1 / width
        normalized_height = 1 / height
        mouse_abs_x = normalized_width * mouseinfo.x
        mouse_abs_y = normalized_height * mouseinfo.y
                            
        movedx = mouse_abs_x - self.prev_mouse_abs_x
        movedy = mouse_abs_y - self.prev_mouse_abs_y
        
        if self.left_button_down:
            if self.selection_rect_startpos is not None:
                rectx, recty, rectwidth, rectheight = self.selectionRectDimensions(mouseinfo)
                self.selection_rect.setRect(rectx, recty, rectwidth, rectheight)
                self.selection_box.SetBoundingBox(self.selection_rect)
                # TODO: test if hit update here makes sense

            else:
                ent = self.active
                if ent is not None and self.sel_activated and self.canmove:
                    self.dragging = True

                    self.manipulator.manipulate(self.sels, movedx, movedy)
                    
                    self.prev_mouse_abs_x = mouse_abs_x
                    self.prev_mouse_abs_y = mouse_abs_y
   
    def on_inboundnetwork(self, evid, name):
        #print "editgui got an inbound network event:", id, name
        return False

    def undo(self):
        ent = self.active
        if ent is not None:
            #self.worldstream.SendObjectUndoPacket(ent.prim.FullId)
            #self.window.update_guivals(ent)
            self.modified = False
            self.deselect_all()

    #~ def redo(self):
        #~ #print "redo clicked"
        #~ ent = self.sel
        #~ if ent is not None:
            #~ #print ent.uuid
            #~ #worldstream = r.getServerConnection()
            #~ self.worldstream.SendObjectRedoPacket(ent.uuid)
            #~ #self.sel = []
            #~ self.update_guivals()
            #~ self.modified = False
            
    def duplicate(self):
        #print "duplicate clicked"
        #ent = self.active
        #if ent is not None:
        for ent in self.sels:
            pass
            #self.worldstream.SendObjectDuplicatePacket(ent.id, ent.prim.UpdateFlags, 1, 1, 0) #nasty hardcoded offset
        
    def createObject(self):
        avatar = naali.getUserAvatar()
        pos = avatar.placeable.position

        # TODO determine what is right in front of avatar and use that instead
        start_x = pos.x() + .7
        start_y = pos.y() + .7
        start_z = pos.z()

        #self.worldstream.SendObjectAddPacket(start_x, start_y, start_z)

    def deleteObject(self):
        if self.active is not None:
            self.manipulator.hideManipulator()
            self.deselect_all()
            self.sels = []
            
    def float_equal(self, a,b):
        if abs(a-b)<0.01:
            return True
        else:
            return False

    def changepos(self, i, v):
        #XXX NOTE / API TODO: exceptions in qt slots (like this) are now eaten silently
        #.. apparently they get shown upon viewer exit. must add some qt exc thing somewhere
        ent = self.active
        if ent is not None:
            qpos = QVector3D(ent.placeable.position)
            if i == 0:
                qpos.setX(v)
            elif i == 1:
                qpos.setY(v)
            elif i == 2:
                qpos.setZ(v)

            ent.placeable.position = qpos
            #ent.network.Position = qpos
            self.manipulator.moveTo(self.sels)

            #if not self.dragging:
            #    r.networkUpdate(ent.id)
            self.modified = True
            
    def changescale(self, i, v):
        ent = self.active
        if ent is not None:
            qscale = ent.placeable.scale
            #oldscale = list((qscale.x(), qscale.y(), qscale.z()))
            scale = list((qscale.x(), qscale.y(), qscale.z()))
                
            if not self.float_equal(scale[i],v):
                scale[i] = v
#                if self.window.mainTab.scale_lock.checked:
#                    #XXX BUG does wrong thing - the idea was to maintain aspect ratio
#                    diff = scale[i] - oldscale[i]
#                    for index in range(len(scale)):
#                        #print index, scale[index], index == i
#                        if index != i:
#                            scale[index] += diff
                
                ent.placeable.scale = QVector3D(scale[0], scale[1], scale[2])
                
                #if not self.dragging:
                #    r.networkUpdate(ent.id)
                self.modified = True
                
    def changerot(self, i, v):
        #XXX NOTE / API TODO: exceptions in qt slots (like this) are now eaten silently
        #.. apparently they get shown upon viewer exit. must add some qt exc thing somewhere
        #print "pos index %i changed to: %f" % (i, v[i])
        ent = self.active
        if ent is not None and not self.usingManipulator:
            ort = mu.euler_to_quat(v)
            ent.placeable.orientation = ort
            #ent.network.Orientation = ort
            #if not self.dragging:
            #    r.networkUpdate(ent.id)
                
            self.modified = True

    def changerot_cpp(self, x, y, z):
        self.changerot(0, (x, y, z))
        
    def changescale_cpp(self, x, y, z):
        self.changescale(0, x)
        self.changescale(1, y)
        self.changescale(2, z)
        
    def changepos_cpp(self, x, y, z):
        self.changepos(0, x)
        self.changepos(1, y)
        self.changepos(2, z)
        
    def getActive(self):
        if len(self.sels) > 0:
            ent = self.sels[-1]
            return ent
        return None
        
    active = property(getActive)
    
    def on_exit(self):
        r.logInfo("Object Edit exiting..")
        # remove selection box component and entity
        # - no need its a temporary item
        #if self.selection_box_entity is not None and self.selection_box is not None:
        #    self.selection_box_entity.RemoveComponentRaw(self.selection_box)
        #    naali.removeEntity(self.selection_box_entity)
        # Connect to key pressed signal from input context
        self.edit_inputcontext.disconnectAll()
        self.deselect_all()
        
        """
        # Disconnect cpp python handler
        self.cpp_python_handler.disconnect('ActivateEditing(bool)', self.on_activate_editing)
        self.cpp_python_handler.disconnect('ManipulationMode(int)', self.on_manupulation_mode_change)
        self.cpp_python_handler.disconnect('RemoveHightlight()', self.deselect_all)
        self.cpp_python_handler.disconnect('RotateValuesToNetwork(int, int, int)', self.changerot_cpp)
        self.cpp_python_handler.disconnect('ScaleValuesToNetwork(double, double, double)', self.changescale_cpp)
        self.cpp_python_handler.disconnect('PosValuesToNetwork(double, double, double)', self.changepos_cpp)
        self.cpp_python_handler.disconnect('CreateObject()', self.createObject)
        self.cpp_python_handler.disconnect('DuplicateObject()', self.duplicate)
        self.cpp_python_handler.disconnect('DeleteObject()', self.deleteObject)
        # Clean widgets
        self.cpp_python_handler.CleanPyWidgets()
        """
        
        r.logInfo(".. done")

    def on_hide(self, shown):
        #print "on_hide", shown
        self.sels = []
        try:
            self.manipulator.hideManipulator()
        except RuntimeError, e:
            r.logDebug("on_hide: scene not found")
        else:
            self.deselect_all()
        self.toggleEditing(False)
 
    def on_activate_editing(self, activate):
        r.logDebug("on_active_editing")
        # Restore stored state when exiting build mode
        if activate == False and self.windowActiveStoredState != None:
            self.windowActive = self.windowActiveStoredState
            self.windowActiveStoredState = None
            if self.windowActive == False:
                self.deselect_all()
                for ent in self.sels:
                    self.remove_highlight(ent)

        # Store the state before build scene activated us
        if activate == True and self.windowActiveStoredState == None:
            self.windowActiveStoredState = self.windowActive
            self.windowActive = True
        
    def on_manupulation_mode_change(self, mode):
        self.changeManipulator(mode)
            
    def update(self, time):
        #print "here", time
        self.time += time
        if self.sels:
            ent = self.active
            #try:
            #    ent.prim
            #except ValueError:
            #that would work also, but perhaps this is nicer:
            s = naali.getDefaultScene()
            if not s.HasEntityId(ent.id):
                #my active entity was removed from the scene by someone else
                self.deselect(ent, valid=False)
                return
                
            if self.time > self.UPDATE_INTERVAL:
                try:
                    #sel_pos = self.selection_box.placeable.Position
                    arr_pos = self.manipulator.getManipulatorPosition()
                    ent_pos = ent.placeable.position
                    #if sel_pos != ent_pos:
                    self.time = 0 #XXX NOTE: is this logic correct?
                    #    self.selection_box.placeable.Position = ent_pos
                    if arr_pos != ent_pos:
                        self.manipulator.moveTo(self.sels)
                except RuntimeError, e:
                    r.logDebug("update: scene not found")
   
    def on_logout(self, id):
        r.logInfo("Object Edit resetting due to logout")
        self.deselect_all()
        self.sels = []
        self.selection_box = None
        self.resetValues()
        self.resetManipulators()

    def on_worldstreamready(self, id):
        r.logInfo("Worldstream ready")
        self.worldstream = r.getServerConnection()
        return False # return False, we don't want to consume the event and not have it for others available
        
    def setUseLocalTransform(self, local):
        self.useLocalTransform = local
