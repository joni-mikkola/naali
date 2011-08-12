if (!framework.IsHeadless())
{
    engine.ImportExtension("qt.core");
    engine.ImportExtension("qt.gui");

    var sceneAction = null;
    var assetAction = null;
    var createIcons = true;

    var mainwin = ui.MainWindow();

    // File
    var fileMenu = mainwin.AddMenu("&File");
    
    // Load and save scene
    var importWebAction = fileMenu.addAction(new QIcon("./data/ui/images/folder_closed.png"), "Import Web Scene");
    importWebAction.triggered.connect(OpenWebScene);
    var exportAction = fileMenu.addAction(new QIcon("./data/ui/images/resource.png"), "Save as...");
    exportAction.triggered.connect(SaveScene);
    fileMenu.addSeparator();
    
    if (framework.GetModuleQObj("UpdateModule"))
        fileMenu.addAction(new QIcon("./data/ui/images/icon/update.ico"), "Check Updates").triggered.connect(CheckForUpdates);
       
    // Reconnect menu items for client only
    if (!server.IsAboutToStart())
    {
        var disconnectAction = fileMenu.addAction(new QIcon("./data/ui/images/icon/disconnect.ico"), "Disconnect");
        disconnectAction.triggered.connect(Disconnect);
        client.Connected.connect(Connected);
        client.Disconnected.connect(Disconnected);
        Disconnected();
    }
    fileMenu.addAction(new QIcon("./data/ui/images/icon/system-shutdown.ico"), "Quit").triggered.connect(Quit);

    // View
    var viewMenu = mainwin.AddMenu("&View");

    if (framework.GetModuleQObj("SceneStructure"))
    {
        assetAction = viewMenu.addAction(new QIcon("./data/ui/images/fileIcons.png"), "Assets");
        assetAction.triggered.connect(OpenAssetsWindow);
        sceneAction = viewMenu.addAction(new QIcon("./data/ui/images/fileList.png"), "Scene");
        sceneAction.triggered.connect(OpenSceneWindow);
    }

    if (framework.GetModuleQObj("Console"))
        viewMenu.addAction("Console").triggered.connect(OpenConsoleWindow);

    if (framework.GetModuleQObj("DebugStats"))
        viewMenu.addAction("Profiler").triggered.connect(OpenProfilerWindow);

    if (framework.GetModuleQObj("Environment"))
    {
        viewMenu.addAction("Terrain Editor").triggered.connect(OpenTerrainEditor);
        viewMenu.addAction("Post-processing").triggered.connect(OpenPostProcessWindow);
    }

    if (framework.GetModuleQObj("PythonScript"))
        viewMenu.addAction("Python Console").triggered.connect(OpenPythonConsole);

    if (framework.GetModuleQObj("G3dwh"))
        viewMenu.addAction("Google 3D Warehouse").triggered.connect(OpenG3dwhWindow);
        
    // Settings
    if (framework.GetModuleQObj("CAVEStereo"))
    {
        var caveSettings = mainwin.AddMenuAction("&Settings", "Cave");
        caveSettings.triggered.connect(OpenCaveWindow);
        var stereoSettings = mainwin.AddMenuAction("&Settings", "Stereoscopy");
        stereoSettings.triggered.connect(OpenStereoscopyWindow);
    }
    
    // Help
    var helpMenu = mainwin.AddMenu("&Help");
    helpMenu.addAction(new QIcon("./data/ui/images/icon/browser.ico"), "Wiki").triggered.connect(OpenWikiUrl);
    helpMenu.addAction(new QIcon("./data/ui/images/icon/browser.ico"), "Doxygen").triggered.connect(OpenDoxygenUrl);
    helpMenu.addAction(new QIcon("./data/ui/images/icon/browser.ico"), "Mailing list").triggered.connect(OpenMailingListUrl);
    
    function NewScene() {
        scene.RemoveAllEntities();
    }

    function Reconnect() {
        client.Reconnect();
    }

    function Disconnect() {
        client.Logout(0, client.getActiveConnection());
    }

    function Connected() {
        disconnectAction.setEnabled(true);
        importWebAction.setEnabled(true);
        exportAction.setEnabled(true);
        if (createIcons)
		{
			ui.EmitAddAction(sceneAction);
                        ui.EmitAddAction(assetAction);
			createIcons = false;
		}
    }

    function Disconnected() {
        if (!client.hasConnections())
        {
            disconnectAction.setEnabled(false);
            importWebAction.setEnabled(false);
            exportAction.setEnabled(false);
                    createIcons = true;
        }
    }

    function Quit() {
        framework.Exit();
    }

    function CheckForUpdates() {
        if (framework.GetModuleQObj("UpdateModule"))
            framework.GetModuleQObj("UpdateModule").RunUpdater("/checknow");
    }

    function OpenMailingListUrl() {
        if (server.IsRunning())
            QDesktopServices.openUrl(new QUrl("http://groups.google.com/group/realxtend/"));
        else
            ui.EmitOpenUrl(new QUrl("http://groups.google.com/group/realxtend/"));
    }
    
    function OpenWikiUrl() {
        if (server.IsRunning())
            QDesktopServices.openUrl(new QUrl("http://wiki.realxtend.org/"));
        else
            ui.EmitOpenUrl(new QUrl("http://wiki.realxtend.org/"));
    }

    function OpenDoxygenUrl() {
        if (server.IsRunning())
            QDesktopServices.openUrl(new QUrl("http://www.realxtend.org/doxygen/"));
        else
            ui.EmitOpenUrl(new QUrl("http://www.realxtend.org/doxygen/"));
    }

    function OpenSceneWindow() {
        framework.GetModuleQObj("SceneStructure").ToggleSceneStructureWindow();
    }

    function OpenAssetsWindow() {
        framework.GetModuleQObj("SceneStructure").ToggleAssetsWindow();
    }

    function OpenProfilerWindow() {
        console.ExecuteCommand("prof");
    }

    function OpenTerrainEditor() {
        framework.GetModuleQObj("Environment").ShowTerrainWeightEditor();
    }

    function OpenPostProcessWindow() {
        framework.GetModuleQObj("Environment").ShowPostProcessWindow();
    }

    function OpenPythonConsole() {
        console.ExecuteCommand("pythonconsole");
    }

    function OpenConsoleWindow() {
        framework.GetModuleQObj("Console").ToggleConsole();
    }

    function OpenStereoscopyWindow() {
        framework.GetModuleQObj("CAVEStereo").ShowStereoscopyWindow();
    }

    function OpenCaveWindow() {
        framework.GetModuleQObj("CAVEStereo").ShowCaveWindow();
    }

    function OpenG3dwhWindow() {
        framework.GetModuleQObj("G3dwh").ShowG3dwhWindow();
    }
    
    function OpenLocalScene() {
        var currentScene = framework.Scene().GetDefaultSceneRaw();
        if (currentScene == null)
            return;
        
        var filename = QFileDialog.getOpenFileName(ui.MainWindow(), "Import Scene", QDir.currentPath() + "/scenes", "Tundra Scene (*.txml *.tbin)");
        if (filename == null || filename == "")
            return;
        if (!QFile.exists(filename))
            return;
            
        var fileninfo = new QFileInfo(filename);
        if (fileninfo.suffix() == "txml")
            currentScene.LoadSceneXML(filename, false, false, 3);
        else if (fileninfo.suffix() == "tbin")
            currentScene.LoadSceneBinary(filename, false, false, 3);
    }
    
    function OpenWebScene() {
        var webRef = QInputDialog.getText(ui.MainWindow(), "Import Web Scene", "Insert a txml or tbin scene url", QLineEdit.Normal, "http://", Qt.Dialog);
        if (webRef == null || webRef == "")
            return;
        var ext = webRef.substring(webRef.length-4);
        var qUrl = QUrl.fromUserInput(webRef);
        if (!qUrl.isValid())
            return;
        
        if (ext != "txml" && ext != "tbin")
            return;
        var transfer = asset.RequestAsset(qUrl.toString()).get();
        transfer.Loaded.connect(WebSceneLoaded);
    }
    
    function WebSceneLoaded(assetptr) {
        var currentScene = framework.Scene().GetDefaultSceneRaw();
        if (currentScene == null)
            return;
                   
        var asset = assetptr.get();
        var diskSource = asset.DiskSource();
        var fileninfo = new QFileInfo(diskSource);
        if (fileninfo.suffix() == "txml")
            currentScene.LoadSceneXML(diskSource, false, false, 3);
        else if (fileninfo.suffix() == "tbin")
            currentScene.LoadSceneBinary(diskSource, false, false, 3);
    }
    
    function SaveScene() {
        var currentScene = framework.Scene().GetDefaultSceneRaw();
        if (currentScene == null)
            return;
            
        var filename = QFileDialog.getSaveFileName(ui.MainWindow(), "Export Scene", QDir.currentPath() + "/scenes", "Tundra Scene (*.txml *.tbin)");
        if (filename == null || filename == "")
            return;
        var ext = new QFileInfo(filename).suffix();
        if (ext != "txml" && ext != "tbin")
        {
            QMessageBox.information(ui.MainWindow(), "Invalid extension", "Invalid Tundra scene file extension '" + ext + "'");
            return;
        }
           
        if (ext == "txml")
            currentScene.SaveSceneXML(filename);
        else if (ext == "tbin")
            currentScene.SaveSceneBinary(filename);
    }
}
