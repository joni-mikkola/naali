// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "TundraLogicModule.h"
#include "Client.h"
#include "Server.h"
#include "TundraEvents.h"
#include "SceneImporter.h"
#include "SyncManager.h"

#include "SceneAPI.h"
#include "AssetAPI.h"
#include "IAssetTransfer.h"
#include "IAsset.h"

#include "SceneManager.h"
#include "ConsoleCommandUtils.h"
#include "EventManager.h"
#include "ModuleManager.h"
#include "KristalliProtocolModule.h"
#include "KristalliProtocolModuleEvents.h"
#include "CoreStringUtils.h"
#include "LocalAssetProvider.h"
#include "AssetAPI.h"
#include "ConsoleAPI.h"

#include "MemoryLeakCheck.h"

namespace TundraLogic
{

std::string TundraLogicModule::type_name_static_ = "TundraLogic";

static const unsigned short cDefaultPort = 2345;

TundraLogicModule::TundraLogicModule() : IModule(type_name_static_),
    autostartserver_(false),
    autostartserver_port_(cDefaultPort),
    activeSyncManager("")
{
    syncManagers_.clear();
}

TundraLogicModule::~TundraLogicModule()
{
}

void TundraLogicModule::PreInitialize()
{
}

void TundraLogicModule::Initialize()
{
    tundraEventCategory_ = framework_->GetEventManager()->RegisterEventCategory("Tundra");
    
    client_ = boost::shared_ptr<Client>(new Client(this));
    server_ = boost::shared_ptr<Server>(new Server(this));
    
    connect(framework_->Scene(), SIGNAL(SceneAdded(QString)), this, SLOT(AttachSyncManagerToScene(QString)));
    connect(framework_->Scene(), SIGNAL(SceneRemoved(QString)), this, SLOT(RemoveSyncManagerFromScene(QString)));
    // Multiconnection specific. When syncManager is created/deleted these are run.
    connect(this, SIGNAL(createOgre(QString)), client_.get(), SLOT(emitCreateOgreSignal(QString)));
    connect(this, SIGNAL(deleteOgre(QString)), client_.get(), SLOT(emitDeleteOgreSignal(QString)));
    connect(this, SIGNAL(setOgre(QString)), client_.get(), SLOT(emitSetOgreSignal(QString)));
    connect(client_.get(), SIGNAL(aboutToDisconnect(QString)), this, SLOT(changeScene(QString)));
    connect(client_.get(), SIGNAL(changeScene(QString)), this, SLOT(changeScene(QString)));

    framework_->RegisterDynamicObject("client", client_.get());
    framework_->RegisterDynamicObject("server", server_.get());
}

void TundraLogicModule::PostInitialize()
{
    kristalliEventCategory_ = framework_->GetEventManager()->QueryEventCategory("Kristalli");
    frameworkEventCategory_ = framework_->GetEventManager()->QueryEventCategory("Framework");
    
    framework_->Console()->RegisterCommand(CreateConsoleCommand("startserver", 
        "Starts a server. Usage: startserver(port)",
        ConsoleBind(this, &TundraLogicModule::ConsoleStartServer)));
    framework_->Console()->RegisterCommand(CreateConsoleCommand("stopserver", 
        "Stops the server",
        ConsoleBind(this, &TundraLogicModule::ConsoleStopServer)));
    framework_->Console()->RegisterCommand(CreateConsoleCommand("connect", 
        "Connects to a server. Usage: connect(address,port,username,password)",
        ConsoleBind(this, &TundraLogicModule::ConsoleConnect)));
    framework_->Console()->RegisterCommand(CreateConsoleCommand("disconnect",
        "Disconnects from a server. Usage: disconnect(conNumber) or just disconnect",
        ConsoleBind(this, &TundraLogicModule::ConsoleDisconnect)));

    framework_->Console()->RegisterCommand(CreateConsoleCommand("savescene",
        "Saves scene into XML or binary. Usage: savescene(filename,binary)",
        ConsoleBind(this, &TundraLogicModule::ConsoleSaveScene)));
    framework_->Console()->RegisterCommand(CreateConsoleCommand("loadscene",
        "Loads scene from XML or binary. Usage: loadscene(filename,binary)",
        ConsoleBind(this, &TundraLogicModule::ConsoleLoadScene)));
    
    framework_->Console()->RegisterCommand(CreateConsoleCommand("importscene",
        "Loads scene from a dotscene file. Optionally clears the existing scene."
        "Replace-mode can be optionally disabled. Usage: importscene(filename,clearscene=false,replace=true)",
        ConsoleBind(this, &TundraLogicModule::ConsoleImportScene)));
    
    framework_->Console()->RegisterCommand(CreateConsoleCommand("importmesh",
        "Imports a single mesh as a new entity. Position can be specified optionally."
        "Usage: importmesh(filename,x,y,z,xrot,yrot,zrot,xscale,yscale,zscale)",
        ConsoleBind(this, &TundraLogicModule::ConsoleImportMesh)));
        
    framework_->Console()->RegisterCommand(CreateConsoleCommand("listcons",
        "Lists all established connections.",
        ConsoleBind(this, &TundraLogicModule::ConsoleListConnections)));
        
    framework_->Console()->RegisterCommand(CreateConsoleCommand("changecon",
        "Change primary view to another connection already established. Meant to be used without webkit UI.",
        ConsoleBind(this, &TundraLogicModule::ConsoleChangeConnection)));
        
    // Take a pointer to KristalliProtocolModule so that we don't have to take/check it every time
    kristalliModule_ = framework_->GetModuleManager()->GetModule<KristalliProtocol::KristalliProtocolModule>().lock();
    if (!kristalliModule_)
    {
        throw Exception("Fatal: could not get KristalliProtocolModule");
    }
    
    // Check whether server should be autostarted
    const boost::program_options::variables_map &programOptions = framework_->ProgramOptions();
    if (programOptions.count("startserver"))
    {
        autostartserver_ = true;
        autostartserver_port_ = programOptions["startserver"].as<int>();
        if (!autostartserver_port_)
            autostartserver_port_ = cDefaultPort;
    }
}

void TundraLogicModule::Uninitialize()
{
    client_.reset();
    server_.reset();
    kristalliModule_.reset();
    foreach (SyncManager *sm, syncManagers_)
        delete sm;
    syncManagers_.clear();
}

void TundraLogicModule::AttachSyncManagerToScene(const QString &name)
{
    unsigned short attachedConnection = Grep(name);

    // Tell syncManager 'attachedConnection' is the magic number when using client_->GetConnection(X)
    SyncManager *sm = new SyncManager(this, attachedConnection);
    sm->RegisterToScene(framework_->Scene()->GetScene(name));
    LogInfo("Registered SyncManager to scene " + name.toStdString());
    syncManagers_.insert(name, sm);
    activeSyncManager = name;
    // When syncmanager is created we also want to create new Ogre sceneManager
    emit createOgre(name);
    if (!IsServer())
        emit setClientActiveConnection(name, attachedConnection);

}

void TundraLogicModule::RemoveSyncManagerFromScene(const QString &name)
{
    SyncManager *sm = syncManagers_.take(name);
    delete sm;
    emit deleteOgre(name);
    TundraLogicModule::LogInfo("Removed SyncManager from scene " + name.toStdString());
}

void TundraLogicModule::changeScene(const QString &name)
{
    unsigned short connection = Grep(name);

    // If we already have this scene selected, do nothing.
    if (framework_->Scene()->GetDefaultScene() == framework_->Scene()->GetScene(name))
        return;
    else
    {
        emit setOgre(name);
        framework_->Scene()->SetDefaultScene(name);
        activeSyncManager = name;
        if (!IsServer())
            emit setClientActiveConnection(name, connection);
    }
}

unsigned short TundraLogicModule::Grep(const QString name)
{
    // Grep number from scenename; list[0] = TundraClient/TundraServer and list[1] = 0, 1, 2, ..., n: n � Z+
    QStringList list = name.split("_");
    QString number = list[1];
    return number.toInt();
}

// This method is used by server to get syncmanager registered to it's scene
SyncManager *TundraLogicModule::GetSyncManager()
{
    return syncManagers_["TundraServer_0"];
}

void TundraLogicModule::Update(f64 frametime)
{
    {
        PROFILE(TundraLogicModule_Update);
        
        static bool check_default_server_start = true;
        if (check_default_server_start)
        {
            if (autostartserver_)
            {
                LogInfo("Started server by default");
                server_->Start(autostartserver_port_);
            }

            // Load startup scene here (if we have one)
            LoadStartupScene();
            
            check_default_server_start = false;
        }

        static bool check_login_start = true;
        if (check_login_start)
        {
            // Web login handling, if we are on a server the request will be ignored down the chain.
            const boost::program_options::variables_map &options = GetFramework()->ProgramOptions();
            if (options.count("login") > 0)
            {
                LogInfo(QString::fromStdString(options["login"].as<std::string>()).toStdString());
                QUrl loginUrl(QString::fromStdString(options["login"].as<std::string>()), QUrl::TolerantMode);
                if (loginUrl.isValid())
                    client_->Login(loginUrl);
            }

            check_login_start = false;
        }
        
        // Update client & server
        if (client_)
            client_->Update(frametime);
        if (server_)
            server_->Update(frametime);
        // Run scene sync
        if (syncManagers_[activeSyncManager])
            syncManagers_[activeSyncManager]->Update(frametime);
        // Run scene interpolation
        Scene::ScenePtr scene = GetFramework()->Scene()->GetDefaultScene();
        if (scene)
            scene->UpdateAttributeInterpolations(frametime);
    }
    
    RESETPROFILER;
}

void TundraLogicModule::LoadStartupScene()
{
    Scene::ScenePtr scene = GetFramework()->Scene()->GetDefaultScene();
    if (!scene)
        return;
    
    const boost::program_options::variables_map &options = GetFramework()->ProgramOptions();
    if (options.count("file") == 0)
        return; // No startup scene specified, ignore.

    std::string startupScene = QString(options["file"].as<std::string>().c_str()).trimmed().toStdString();
    if (startupScene.empty())
        return; // No startup scene specified, ignore.

    // At this point, if we have a LocalAssetProvider, it has already also parsed the --file command line option
    // and added the appropriate path as a local asset storage. Here we assume that is the case, so that the
    // scene we now load will be able to refer to local:// assets in its subfolders.
    if (AssetAPI::ParseAssetRefType(QString::fromStdString(startupScene)) != AssetAPI::AssetRefLocalPath)
    {
        AssetTransferPtr sceneTransfer = framework_->Asset()->RequestAsset(startupScene);
        if (!sceneTransfer.get())
        {
            LogError("Asset transfer initialization failed for scene file " + startupScene + " failed");
            return;
        }
        connect(sceneTransfer.get(), SIGNAL(Loaded(AssetPtr)), SLOT(StartupSceneLoaded(AssetPtr)));
        connect(sceneTransfer.get(), SIGNAL(Failed(IAssetTransfer*, QString)), SLOT(StartupSceneTransferFailed(IAssetTransfer*, QString)));
        LogInfo("Loading startup scene from " + startupScene);
    }
    else
    {
        bool useBinary = startupScene.find(".tbin") != std::string::npos;
        if (!useBinary)
            scene->LoadSceneXML(startupScene, true/*clearScene*/, false/*replaceOnConflict*/, AttributeChange::Default);
        else
            scene->LoadSceneBinary(startupScene, true/*clearScene*/, false/*replaceOnConflict*/, AttributeChange::Default);
    }
}

void TundraLogicModule::StartupSceneLoaded(AssetPtr asset)
{
    Scene::ScenePtr scene = GetFramework()->Scene()->GetDefaultScene();
    if (!scene)
        return;

    QString sceneDiskSource = asset->DiskSource();
    if (!sceneDiskSource.isEmpty())
    {
        bool useBinary = sceneDiskSource.endsWith(".tbin");
        if (!useBinary)
            scene->LoadSceneXML(sceneDiskSource.toStdString(), true/*clearScene*/, false/*replaceOnConflict*/, AttributeChange::Default);
        else
            scene->LoadSceneBinary(sceneDiskSource.toStdString(), true/*clearScene*/, false/*replaceOnConflict*/, AttributeChange::Default);
    }
    else
        LogError("Could not resolve disk source for loaded scene file " + asset->Name().toStdString());
}

void TundraLogicModule::StartupSceneTransferFailed(IAssetTransfer *transfer, QString reason)
{
    LogError("Failed to load startup scene from " + transfer->GetSourceUrl().toStdString() + " reason: " + reason.toStdString());
}

ConsoleCommandResult TundraLogicModule::ConsoleStartServer(const StringVector& params)
{
    unsigned short port = cDefaultPort;
    
    try
    {
        if (params.size() > 0)
            port = ParseString<int>(params[0]);
    }
    catch (...) {}
    
    server_->Start(port);
    
    return ConsoleResultSuccess();
}

ConsoleCommandResult TundraLogicModule::ConsoleStopServer(const StringVector& params)
{
    server_->Stop();
    
    return ConsoleResultSuccess();
}

ConsoleCommandResult TundraLogicModule::ConsoleConnect(const StringVector& params)
{
    if (params.size() < 1)
        return ConsoleResultFailure("No address specified");
    
    unsigned short port = cDefaultPort;
    std::string username = "test";
    std::string password = "test";
    
    try
    {
        if (params.size() > 1)
            port = ParseString<int>(params[1]);
        if (params.size() > 2)
            username = params[2];
        if (params.size() > 3)
            password = params[3];
    }
    catch (...) {}
    
    client_->Login(QString::fromStdString(params[0]), port, QString::fromStdString(username), QString::fromStdString(password));
    
    return ConsoleResultSuccess();
}

ConsoleCommandResult TundraLogicModule::ConsoleDisconnect(const StringVector& params)
{
    if (params.size() < 1)
        client_->Logout(false, client_.get()->getActiveConnection());

    else if (params.size() >= 1)
    {
        int conNumber;
        try {
            conNumber = ParseString<int>(params[0]);
        }
        catch (std::exception &e)
        {
            return ConsoleResultInvalidParameters();
        }

        client_->Logout(false, conNumber);
    }
    
    return ConsoleResultSuccess();
}

ConsoleCommandResult TundraLogicModule::ConsoleListConnections(const StringVector &params)
{
    client_->printConnections();

    return ConsoleResultSuccess();
}

ConsoleCommandResult TundraLogicModule::ConsoleChangeConnection(const StringVector &params)
{
    QString sceneName = "TundraClient_";
    int conNumber = 0;

    try {
        conNumber = ParseString<int>(params[0]);
    }
    catch (std::exception &e)
    {
        return ConsoleResultInvalidParameters();
    }

    sceneName.append(QString("%1").arg(conNumber));
    client_->emitChangeSceneSignal(sceneName);

    return ConsoleResultSuccess();
}

ConsoleCommandResult TundraLogicModule::ConsoleSaveScene(const StringVector &params)
{
    Scene::ScenePtr scene = GetFramework()->Scene()->GetDefaultScene();
    if (!scene)
        return ConsoleResultFailure("No active scene found.");
    if (params.size() < 1)
        return ConsoleResultFailure("No filename given.");
    
    bool useBinary = false;
    if ((params.size() > 1) && (params[1] == "binary"))
        useBinary = true;
    
    bool success;
    if (!useBinary)
        success = scene->SaveSceneXML(params[0]);
    else
        success = scene->SaveSceneBinary(params[0]);
    
    if (success)
        return ConsoleResultSuccess();
    else
        return ConsoleResultFailure("Failed to save the scene.");
}

ConsoleCommandResult TundraLogicModule::ConsoleLoadScene(const StringVector &params)
{
    ///\todo Add loadScene parameter
    Scene::ScenePtr scene = GetFramework()->Scene()->GetDefaultScene();
    if (!scene)
        return ConsoleResultFailure("No active scene found.");
    if (params.size() < 1)
        return ConsoleResultFailure("No filename given.");
    
    bool useBinary = false;
    if ((params.size() > 1) && (params[1] == "binary"))
        useBinary = true;
    
    QList<Scene::Entity *> entities;
    if (!useBinary)
        entities = scene->LoadSceneXML(params[0], true/*clearScene*/, false/*replaceOnConflcit*/, AttributeChange::Default);
    else
        entities = scene->LoadSceneBinary(params[0], true/*clearScene*/, false/*replaceOnConflcit*/, AttributeChange::Default);
    
    if (!entities.empty())
    {
        return ConsoleResultSuccess();
    }
    else
        return ConsoleResultFailure("Failed to load the scene.");
}

ConsoleCommandResult TundraLogicModule::ConsoleImportScene(const StringVector &params)
{
    Scene::ScenePtr scene = GetFramework()->Scene()->GetDefaultScene();
    if (!scene)
        return ConsoleResultFailure("No active scene found.");
    if (params.size() < 1)
        return ConsoleResultFailure("No filename given.");
    bool clearscene = false;
    bool replace = true;
    if (params.size() > 1)
        clearscene = ParseBool(params[1]);
    if (params.size() > 2)
        replace = ParseBool(params[2]);
    
    std::string filename = params[0];
    boost::filesystem::path path(filename);
    std::string dirname = path.branch_path().string();
    
    SceneImporter importer(scene);
    QList<Scene::Entity *> entities = importer.Import(filename, dirname, Transform(),
        "local://", AttributeChange::Default, clearscene, replace);
    if (!entities.empty())
    {
        return ConsoleResultSuccess();
    }
    else
        return ConsoleResultFailure("Failed to import the scene.");
}

ConsoleCommandResult TundraLogicModule::ConsoleImportMesh(const StringVector &params)
{
    Scene::ScenePtr scene = GetFramework()->Scene()->GetDefaultScene();
    if (!scene)
        return ConsoleResultFailure("No active scene found.");
    if (params.size() < 1)
        return ConsoleResultFailure("No filename given.");
    
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float xr = 0.0f, yr = 0.0f, zr = 0.0f;
    float xs = 1.0f, ys = 1.0f,zs = 1.0f;
    if (params.size() >= 4)
    {
        x = ParseString<float>(params[1], 0.0f);
        y = ParseString<float>(params[2], 0.0f);
        z = ParseString<float>(params[3], 0.0f);
    }
    if (params.size() >= 7)
    {
        xr = ParseString<float>(params[4], 0.0f);
        yr = ParseString<float>(params[5], 0.0f);
        zr = ParseString<float>(params[6], 0.0f);
    }
    if (params.size() >= 10)
    {
        xs = ParseString<float>(params[7], 1.0f);
        ys = ParseString<float>(params[8], 1.0f);
        zs = ParseString<float>(params[9], 1.0f);
    }
    
    std::string filename = params[0];
    boost::filesystem::path path(filename);
    std::string dirname = path.branch_path().string();
    
    SceneImporter importer(scene);
    Scene::EntityPtr entity = importer.ImportMesh(filename, dirname, Transform(Vector3df(x,y,z),
        Vector3df(xr,yr,zr), Vector3df(xs,ys,zs)), std::string(), "local://", AttributeChange::Default, true);
    
    return ConsoleResultSuccess();
}

bool TundraLogicModule::IsServer() const
{
    return kristalliModule_->IsServer();
}

// virtual
bool TundraLogicModule::HandleEvent(event_category_id_t category_id, event_id_t event_id, IEventData* data)
{
    if (category_id == tundraEventCategory_)
    {
        // Handle login request from the loginwindow
        if (event_id == Events::EVENT_TUNDRA_LOGIN)
        {
            Events::TundraLoginEventData* event_data = checked_static_cast<Events::TundraLoginEventData*>(data);
            if (client_)
                client_->Login(QString::fromStdString(event_data->address_), event_data->port_ ? event_data->port_ : cDefaultPort, QString::fromStdString(event_data->username_), QString::fromStdString(event_data->password_), QString::fromStdString(event_data->protocol_));
        }
    }
    
    if (category_id == kristalliEventCategory_)
    {
        if (client_)
            client_->HandleKristalliEvent(event_id, data);
        if (server_)
            server_->HandleKristalliEvent(event_id, data);
        foreach (SyncManager *sm, syncManagers_)
            if (sm)
                sm->HandleKristalliEvent(event_id, data);
    }
    
    return false;
}

}

extern "C" void POCO_LIBRARY_API SetProfiler(Foundation::Profiler *profiler);
void SetProfiler(Foundation::Profiler *profiler)
{
    Foundation::ProfilerSection::SetProfiler(profiler);
}

using namespace TundraLogic;

POCO_BEGIN_MANIFEST(IModule)
   POCO_EXPORT_CLASS(TundraLogicModule)
POCO_END_MANIFEST
