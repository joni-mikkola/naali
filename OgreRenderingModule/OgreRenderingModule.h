// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_OgreRenderingModule_OgreRenderingModule_h
#define incl_OgreRenderingModule_OgreRenderingModule_h

#include "IModule.h"
#include "ModuleLoggingFunctions.h"
#include "OgreModuleApi.h"

namespace Foundation
{
    class Framework;
}

namespace OgreRenderer
{
    class Renderer;
    typedef boost::shared_ptr<Renderer> RendererPtr;
    class RendererSettings;
    typedef boost::shared_ptr<RendererSettings> RendererSettingsPtr;

    //! \bug Ogre assert fail when viewing a mesh that contains a reference to non-existing skeleton.
    
    /** \defgroup OgreRenderingModuleClient OgreRenderingModule Client Interface
        This page lists the public interface of the OgreRenderingModule.

        For details on how to use the public interface, see \ref OgreRenderingModule "Using the Ogre renderer module"
    */

    //! A renderer module using Ogre
    class OGRE_MODULE_API OgreRenderingModule : public IModule
    {
    public:
        OgreRenderingModule();
        virtual ~OgreRenderingModule();

        virtual void Load();
        virtual void PreInitialize();
        virtual void Initialize();
        virtual void PostInitialize();
        virtual void Uninitialize();
        virtual void Update(f64 frametime);
        virtual bool HandleEvent(event_category_id_t category_id, event_id_t event_id, IEventData* data);

        //! returns renderer
        RendererPtr GetRenderer() const { return renderer_; }

        MODULE_LOGGING_FUNCTIONS;

        //! returns name of this module. Needed for logging.
        static const std::string &NameStatic() { return type_name_static_; }

        //! callback for console command
        ConsoleCommandResult ConsoleStats(const StringVector &params);

        //! Ogre resource group for cache files.
        static std::string CACHE_RESOURCE_GROUP;

    private:
        //! Type name of the module.
        static std::string type_name_static_;

        //! renderer
        RendererPtr renderer_;

        //! renderer settings
        RendererSettingsPtr renderer_settings_;

        //! input event category
        event_category_id_t input_event_category_;

        //! scene event category
        event_category_id_t scene_event_category_;

        //! network state category
        event_category_id_t network_state_event_category_;
    };
}

#endif
