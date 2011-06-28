// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "OgreRenderingModule.h"
#include "Renderer.h"
#include "EC_Placeable.h"
#include "EC_ColladaMesh.h"
#include "EC_Mesh.h"
#include "EC_OgreSky.h"
#include "EC_OgreCustomObject.h"
#include "EC_OgreMovableTextOverlay.h"
#include "EC_AnimationController.h"
#include "EC_OgreEnvironment.h"
#include "EC_OgreCamera.h"
#include "EC_OgreCompositor.h"
#include "EC_RttTarget.h"
#include "EC_BillboardWidget.h"
#include "EC_SelectionBox.h"
#include "InputEvents.h"
#include "SceneEvents.h"
#include "NetworkEvents.h"
#include "Entity.h"
#include "RendererSettings.h"
#include "ConfigurationManager.h"
#include "EventManager.h"
#include "AssetAPI.h"
#include "AssetCache.h"
#include "GenericAssetFactory.h"
#include "OgreMeshAsset.h"
#include "OgreParticleAsset.h"
#include "OgreSkeletonAsset.h"
#include "OgreMaterialAsset.h"
#include "TextureAsset.h"
#include "ConsoleAPI.h"
#include "ConsoleCommandUtils.h"
#include "VersionInfo.h"

#include "MemoryLeakCheck.h"

namespace OgreRenderer
{
    std::string OgreRenderingModule::type_name_static_ = "OgreRendering";
    std::string OgreRenderingModule::CACHE_RESOURCE_GROUP = "CACHED_ASSETS_GROUP";

    OgreRenderingModule::OgreRenderingModule() :
        IModule(type_name_static_),
        input_event_category_(0),
        scene_event_category_(0)
    {
    }

    OgreRenderingModule::~OgreRenderingModule()
    {
    }

    // virtual
    void OgreRenderingModule::Load()
    {
        DECLARE_MODULE_EC(EC_Placeable);
        DECLARE_MODULE_EC(EC_ColladaMesh);
        DECLARE_MODULE_EC(EC_Mesh);
        DECLARE_MODULE_EC(EC_OgreSky);
        DECLARE_MODULE_EC(EC_OgreCustomObject);
        DECLARE_MODULE_EC(EC_OgreMovableTextOverlay);
        DECLARE_MODULE_EC(EC_AnimationController);
        DECLARE_MODULE_EC(EC_OgreEnvironment);
        DECLARE_MODULE_EC(EC_OgreCamera);
        DECLARE_MODULE_EC(EC_BillboardWidget);
        DECLARE_MODULE_EC(EC_OgreCompositor);
        DECLARE_MODULE_EC(EC_RttTarget);
        DECLARE_MODULE_EC(EC_SelectionBox);

        // Create asset type factories for each asset OgreRenderingModule provides to the system.
        framework_->Asset()->RegisterAssetTypeFactory(AssetTypeFactoryPtr(new GenericAssetFactory<TextureAsset>("Texture")));
        framework_->Asset()->RegisterAssetTypeFactory(AssetTypeFactoryPtr(new GenericAssetFactory<TextureAsset>("OgreTexture"))); // deprecated/old style.
        framework_->Asset()->RegisterAssetTypeFactory(AssetTypeFactoryPtr(new GenericAssetFactory<OgreMeshAsset>("OgreMesh")));
        framework_->Asset()->RegisterAssetTypeFactory(AssetTypeFactoryPtr(new GenericAssetFactory<OgreParticleAsset>("OgreParticle")));
        framework_->Asset()->RegisterAssetTypeFactory(AssetTypeFactoryPtr(new GenericAssetFactory<OgreSkeletonAsset>("OgreSkeleton")));
        framework_->Asset()->RegisterAssetTypeFactory(AssetTypeFactoryPtr(new GenericAssetFactory<OgreMaterialAsset>("OgreMaterial")));
    }

    void OgreRenderingModule::PreInitialize()
    {
        std::string ogre_config_filename = "ogre.cfg";
#if defined (_WINDOWS) && (_DEBUG)
        std::string plugins_filename = "pluginsd.cfg";
#elif defined (_WINDOWS)
        std::string plugins_filename = "plugins.cfg";
#elif defined(__APPLE__)
        std::string plugins_filename = "plugins-mac.cfg";
#else
        std::string plugins_filename = "plugins-unix.cfg";
#endif

        // Create renderer here, so it can be accessed in uninitialized state by other module's PreInitialize()
        renderer_ = OgreRenderer::RendererPtr(new OgreRenderer::Renderer(framework_, ogre_config_filename, plugins_filename, framework_->ApplicationVersion()->toString().toStdString()));
    }

    void OgreRenderingModule::Initialize()
    {
        assert (renderer_);
        assert (!renderer_->IsInitialized());
        renderer_->Initialize();

        framework_->GetServiceManager()->RegisterService(Service::ST_Renderer, renderer_);
        framework_->RegisterDynamicObject("renderer", renderer_.get());

        // Add our asset cache file directory as its own resource group to ogre to support threaded loading.
        std::string cacheResourceDir = GetFramework()->Asset()->GetAssetCache()->GetCacheDirectory().toStdString();
        if (!Ogre::ResourceGroupManager::getSingleton().resourceLocationExists(cacheResourceDir, CACHE_RESOURCE_GROUP))
            Ogre::ResourceGroupManager::getSingleton().addResourceLocation(cacheResourceDir, "FileSystem", CACHE_RESOURCE_GROUP);
    }

    void OgreRenderingModule::PostInitialize()
    {
        EventManagerPtr event_manager = framework_->GetEventManager();
        input_event_category_ = event_manager->QueryEventCategory("Input");
        scene_event_category_ = event_manager->QueryEventCategory("Scene");
        network_state_event_category_ = event_manager->QueryEventCategory("NetworkState");

        renderer_->PostInitialize();

        framework_->Console()->RegisterCommand(CreateConsoleCommand(
                "RenderStats", "Prints out render statistics.", 
                ConsoleBind(this, &OgreRenderingModule::ConsoleStats)));
        renderer_settings_ = RendererSettingsPtr(new RendererSettings(framework_));
    }

    bool OgreRenderingModule::HandleEvent(event_category_id_t category_id, event_id_t event_id, IEventData* data)
    {
        PROFILE(OgreRenderingModule_HandleEvent);
        if (!renderer_)
            return false;

        if (category_id == input_event_category_ && event_id == InputEvents::INWORLD_CLICK)
        {
            // do raycast into the world when user clicks mouse button
            InputEvents::Movement *movement = checked_static_cast<InputEvents::Movement*>(data);
            assert(movement);
            RaycastResult* result = renderer_->Raycast(movement->x_.abs_, movement->y_.abs_);

            Scene::Entity *entity = result->entity_;
            if (entity)
            {
                Scene::Events::RaycastEventData event_data(entity->GetId());
                event_data.pos = result->pos_, event_data.submesh = result->submesh_, event_data.u = result->u_, event_data.v = result->v_; 
                framework_->GetEventManager()->SendEvent(scene_event_category_, Scene::Events::EVENT_ENTITY_GRAB, &event_data);

                //Semantically same as above but sends the entity pointer
                Scene::Events::EntityClickedData clicked_event_data(entity);
                framework_->GetEventManager()->SendEvent(scene_event_category_, Scene::Events::EVENT_ENTITY_CLICKED, &clicked_event_data);
            }
            else
                framework_->GetEventManager()->SendEvent(scene_event_category_, Scene::Events::EVENT_ENTITY_NONE_CLICKED, 0);
        }

        if (network_state_event_category_)
        {
            if (event_id == ProtocolUtilities::Events::EVENT_USER_DISCONNECTED)
                renderer_->ResetImageRendering();
            return false;
        }

        return false;
    }

    void OgreRenderingModule::Uninitialize()
    {
        // We're shutting down. Force a release of all loaded asset objects from the Asset API so that 
        // no refs to Ogre assets remain - below 'renderer_.reset()' is going to delete Ogre::Root.
        framework_->Asset()->ForgetAllAssets();

        framework_->GetServiceManager()->UnregisterService(renderer_);

        // Explicitly remove log listener now, because the service has been
        // unregistered now, and no-one can get at the renderer to remove themselves
        if (renderer_)
            renderer_->RemoveLogListener();

        renderer_settings_.reset();
        renderer_.reset();
    }

    void OgreRenderingModule::Update(f64 frametime)
    {
        {
            PROFILE(OgreRenderingModule_Update);
            renderer_->Update(frametime);
        }
        RESETPROFILER;
    }

    ConsoleCommandResult OgreRenderingModule::ConsoleStats(const StringVector &params)
    {
        if (renderer_)
        {
            ConsoleAPI *c = framework_->Console();
            const Ogre::RenderTarget::FrameStats& stats = renderer_->GetCurrentRenderWindow()->getStatistics();
            c->Print("Average FPS: " + QString::number(stats.avgFPS));
            c->Print("Worst FPS: " + QString::number(stats.worstFPS));
            c->Print("Best FPS: " + QString::number(stats.bestFPS));
            c->Print("Triangles: " + QString::number(stats.triangleCount));
            c->Print("Batches: " + QString::number(stats.batchCount));
            return ConsoleResultSuccess();
        }

        return ConsoleResultFailure("No renderer found.");
    }
}

extern "C" void POCO_LIBRARY_API SetProfiler(Foundation::Profiler *profiler);
void SetProfiler(Foundation::Profiler *profiler)
{
    Foundation::ProfilerSection::SetProfiler(profiler);
}

using namespace OgreRenderer;

POCO_BEGIN_MANIFEST(IModule)
   POCO_EXPORT_CLASS(OgreRenderingModule)
POCO_END_MANIFEST

