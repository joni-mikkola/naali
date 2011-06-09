// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_Foundation_Framework_h
#define incl_Foundation_Framework_h

// Application name is statically defined here
#define APPLICATION_NAME "realXtend"

#include "Profiler.h"
#include "ModuleManager.h"
#include "ServiceManager.h"

#include <boost/smart_ptr.hpp>
#include <boost/program_options.hpp>

class UiAPI;
class FrameAPI;
class InputAPI;
class AudioAPI;
class AssetAPI;
class ConsoleAPI;
class DebugAPI;
class SceneAPI;
class ConfigAPI;
class DevicesAPI;
class Application;
class ApiVersionInfo;
class ApplicationVersionInfo;

namespace Poco
{
    class SplitterChannel;
    class Channel;
    class Formatter;
}

namespace Foundation
{

    /// Contains entry point for the framework.
    /*! Allows access to the core API objects, various managers and services. The standard way of using
        the framework is by first creating the framework and then calling Framework::Go()
        which will then load / initialize all modules and enters the main loop which
        automatically updates all loaded modules.

        There are other ways of using the framework. To f.ex. run without the main loop,
        see Framework::PostInitialize(). All the modules need to be updated manually then.

        The constructor initializes the framework. Config or logging should not be used
        usually without first initializing the framework.

        \ingroup Foundation_group
    */
    class Framework : public QObject
    {
    
    Q_OBJECT

    public:
        /// Constructs and initializes the framework.
        /** @param arcc Command line argument count as provided by the operating system.
            @param arcv Command line arguments as provided by the operating system.
        */
        Framework(int argc, char** argv);

        /// destructor
        ~Framework();

        /// Parse program options from command line arguments
        /*! For internal use. Should be called immediately after creating the framework,
            so all options will be taken in effect properly.
        */
        void ParseProgramOptions();

        /// Returns the command line options specified as command-line options when starting up Naali.
        boost::program_options::variables_map &ProgramOptions() { return commandLineVariables; }

        /// Do post-initialization steps. No need to call if using Framework::Go().
        /*! This function can be used if you wish to use the framework without main loop.
            It does
            In that case the correct order is:
                Foundation::Framework fw;                  // create the framework
                fw.GetModuleManager()->ExcludeModule(...)  // optional step for excluding certain modules
                ...                                        // other initialization steps
                fw.PostInitialize()
                ...                                        // continue program execution without framework's main loop
        */
        void PostInitialize();

        /// Entry point for the framework.
        /// \note This function call will block as long as Application exits.
        void Go();

        /// Calculate the main loop frametime for updates
        /// \return double Frametime
        double CalculateFrametime(tick_t currentClockTime);

        /// Update modules
        /// \param double Frametime
        void UpdateModules(double frametime);

        /// Update APIs
        /// \param double Frametime
        void UpdateAPIs(double frametime);

        /// Update rendering
        /// \param double Frametime
        void UpdateRendering(double frametime);

        /// Returns component manager.
        ComponentManagerPtr GetComponentManager() const;

        /// Returns module manager.
        ModuleManagerPtr GetModuleManager() const;

        /// Returns service manager.
        ServiceManagerPtr GetServiceManager() const;

        /// Returns event manager.
        EventManagerPtr GetEventManager() const;

        /// Returns platform abstraction object.
        PlatformPtr GetPlatform() const;

        /// Returns config manager.
        ConfigurationManagerPtr GetConfigManager();

        /// Returns thread task manager.
        ThreadTaskManagerPtr GetThreadTaskManager();

        /// Cancel a pending exit
        void CancelExit();

        /// Force immediate exit, with no possibility to cancel it
        void ForceExit();

        /// Returns true if framework is in the process of exiting (will exit at next possible opportunity)
        bool IsExiting() const { return exit_signal_; }

        /// Returns true if framework is properly initialized and Go() can be called.
        bool Initialized() const { return initialized_; }

        /// Returns the default configuration
        ConfigurationManager &GetDefaultConfig();

        /// Returns pointer to the default configuration
        ConfigurationManager *GetDefaultConfigPtr();

        /// Shortcut for retrieving a service. See ServiceManager::GetService() for more info
        template <class T>
        __inline boost::weak_ptr<T> GetService(service_type_t type) { return service_manager_->GetService<T>(type); }

        /// Shortcut for retrieving a service. See ServiceManager::GetService() for more info
        template <class T>
        __inline const boost::weak_ptr<T> GetService(service_type_t type) const { return service_manager_->GetService<T>(type); }

#ifdef PROFILING
        /// Returns the default profiler used by all normal profiling blocks. For profiling code, use PROFILE-macro.
        Profiler &GetProfiler();
#endif
        /// Add a new log listener for poco log
        void AddLogChannel(Poco::Channel *channel);

        /// Remove existing log listener from poco log
        void RemoveLogChannel(Poco::Channel *channel);

        /// load and init module
        ConsoleCommandResult ConsoleLoadModule(const StringVector &params);

        /// uninit and unload a module
        ConsoleCommandResult ConsoleUnloadModule(const StringVector &params);

        /// List all loaded modules
        ConsoleCommandResult ConsoleListModules(const StringVector &params);

        /// send event
        ConsoleCommandResult ConsoleSendEvent(const StringVector &params);

        /// Output profiling data
        ConsoleCommandResult ConsoleProfile(const StringVector &params);

        /// limit frames
        ConsoleCommandResult ConsoleLimitFrames(const StringVector &params);

        /// Returns name of the configuration group used by the framework
        /*! The group name is used with ConfigurationManager, for framework specific
            settings. Alternatively a class may use it's own name as the name of the
            configuration group, if it so chooses.

            For internal use only.
        */
        static const std::string &ConfigurationGroup()
        {
            static std::string group("Foundation");
            return group;
        }

        /// Loads all available modules. Do not call normally.
        void LoadModules();

        /// Unloads all available modules. Do not call normally.
        void UnloadModules();

        /// Get main QApplication
        Application *GetApplication() const;

        /// Returns module by class T.
        /** @param T class type of the module.
            @return The module, or null if the module doesn't exist. Always remember to check for null pointer.
            @note Do not store the returned raw module pointer anywhere or make a boost::weak_ptr/shared_ptr out of it.
         */
        template <class T>
        T *GetModule() { return GetModuleManager()->GetModule<T>().lock().get(); }

        /// Returns service by class T.
        /** @param T class type of the service.
            @return The service, or null if the service doesn't exist. Always remember to check for null pointer.
            @note Do not store the returned raw module pointer anywhere or make a boost::weak_ptr/shared_ptr out of it.
         */
        template <class T>
        T *GetService() { return GetServiceManager()->GetService<T>().lock().get(); }

    public slots:
        /// Returns the core API UI object.
        /** @note Never returns a null pointer. Use IsHeadless() to check out if we're running the headless mode or not. */
        UiAPI *Ui() const;

        /// Returns the core API Input object.
        InputAPI *Input() const;

        /// Returns the core API Frame object.
        FrameAPI *Frame() const;

        /// Returns the core API Console object.
        ConsoleAPI *Console() const;

        /// Returns the core API Audio object.
        AudioAPI *Audio() const;

        /// Returns core API Asset object.
        AssetAPI *Asset() const;

        /// Returns core API Debug object.
        DebugAPI *Debug() const;

        /// Returns core API Scene object.
        SceneAPI *Scene() const;

        /// Returns core API Config object.
        ConfigAPI *Config() const;

        /// Return core API Devices object.
        DevicesAPI *Devices() const;

        /// Returns Tundra API version info object.
        ApiVersionInfo *ApiVersion() const;

        /// Returns Tundra application version info object.
        ApplicationVersionInfo *ApplicationVersion() const;
    
        /// Returns if we're running the application in headless or not.
        bool IsHeadless() const { return headless_; }

        /// Returns the given module, if it is loaded into the system, and if it derives from QObject.
        QObject *GetModuleQObj(const QString &name);

        /// Stores the given QObject as a dynamic property into the Framework.
        /** This is done to implement easier script access for QObject-based interface objects.
            @param name The name to use for the property. Fails if name is an empty string.
            @param object The object to register as a property. The property will be a QVariant containing this QObject.
            @return If the registration succeeds, this returns true. Otherwise either 'object' pointer was null,
                   or a property with that name was registered already.
            @note There is no unregister option. It can be implemented if someone finds it useful, but at this point
             we are going with a "unload-only-on-close" behavior.
        */
        bool RegisterDynamicObject(QString name, QObject *object);

        /// Signals the framework to exit
        void Exit();

    private:
        Q_DISABLE_COPY(Framework)

        /// Registers framework specific console commands
        /// Should be called after modules are loaded and initialized
        void RegisterConsoleCommands();

        /// Creates logging system.
        void CreateLoggingSystem();
        ModuleManagerPtr module_manager_; ///< Module manager.
        ComponentManagerPtr component_manager_; ///< Component manager.
        ServiceManagerPtr service_manager_; ///< Service manager.
        EventManagerPtr event_manager_; ///< Event manager.
        PlatformPtr platform_; ///< Platform.
        ThreadTaskManagerPtr thread_task_manager_; ///< Thread task manager.
        ConfigurationManagerPtr config_manager_; ///< Default configuration
        bool exit_signal_; ///< If true, exit application.
        std::vector<Poco::Channel*> log_channels_; ///< Logger channels
        Poco::Formatter *log_formatter_; ///< Logger default formatter
#ifdef PROFILING
        Profiler profiler_; ///< Profiler.
#endif
        boost::program_options::variables_map commandLineVariables; ///< program options
        boost::program_options::options_description commandLineDescriptions; ///< program option descriptions
        bool initialized_; ///< Is the framework is properly initialized.
        bool headless_; ///< Are we running in the headless mode.
        Poco::SplitterChannel *splitterchannel; ///< Sends log prints for multiple channels.
        
        Application *application; ///< Naali implementation of the main QApplication object.
        FrameAPI *frame; ///< The Frame API.
        ConsoleAPI *console; ///< The console API.
        UiAPI *ui; ///< The UI API.
        InputAPI *input; ///< The Input API.
        AssetAPI *asset; ///< The Asset API.
        AudioAPI *audio; ///< The Audio API.
        DebugAPI *debug; ///< The Debug API.
        SceneAPI *scene; ///< The Scene API.
        ConfigAPI *config; ///< The Config API.
        DevicesAPI *devices; ///< The Devices API.

        /// The Tundra API version info of this build. May differ from the end user application version of the default distribution, i.e. app may change when api stays same.
        ApiVersionInfo *api_versioninfo_;

        /// The Tundra application version info for this build.
        ApplicationVersionInfo *application_versioninfo_;

        int argc_; ///< Command line argument count as supplied by the operating system.
        char **argv_; ///< Command line arguments as supplied by the operating system.
    };

    ///\todo Refactor-remove these. -jj.
    namespace
    {
        const event_id_t NETWORKING_REGISTERED = 2;
        const event_id_t WORLD_STREAM_READY = 3;
//        const event_id_t WEB_LOGIN_DATA_RECEIVED = 4;
    }

    ///\todo (Re)move, doesn't belong to framework.
/*    class WebLoginDataEvent : public IEventData
    {
        WebLoginDataEvent();
    public:
        WebLoginDataEvent(const QString &first, const QString &last, const QString &avAddr, const QString &worldAddr) :
            firstname_(first), lastname_(last), avatar_address_(avAddr), world_address_(worldAddr) { }
        QString firstname_;
        QString lastname_;
        QString avatar_address_;
        QString world_address_;
    };
*/
}

#endif
