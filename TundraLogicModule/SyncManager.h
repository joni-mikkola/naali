// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_TundraLogicModule_SyncManager_h
#define incl_TundraLogicModule_SyncManager_h

#include "Foundation.h"
#include "IComponent.h"
#include "ForwardDefines.h"
#include "SyncState.h"

#include <QObject>
#include <map>
#include <set>

struct MsgCreateEntity;
struct MsgRemoveEntity;
struct MsgCreateComponents;
struct MsgUpdateComponents;
struct MsgRemoveComponents;
struct MsgEntityIDCollision;
struct MsgEntityAction;

namespace kNet
{
    class MessageConnection;
    typedef unsigned long message_id_t;
}

class UserConnection;

namespace TundraLogic
{

class TundraLogicModule;

struct RemovedComponent
{
    QString typename_;
    QString name_;
};

class SyncManager : public QObject
{
    Q_OBJECT
    
public:
    //! Constructor
    explicit SyncManager(TundraLogicModule* owner, const unsigned short attachedConnection);
    
    //! Destructor
    ~SyncManager();
    
    //! Register to entity/component change signals from a specific scene and start syncing them
    void RegisterToScene(Scene::ScenePtr scene);
    
    //! Accumulate time & send pending sync messages if enough time passed from last update
    void Update(f64 frametime);
    
    //! Create new replication state for user and dirty it (server operation only)
    void NewUserConnected(UserConnection* user);
    
    //! Handle Kristalli event
    void HandleKristalliEvent(event_id_t event_id, IEventData* data);
    
public slots:
    //! Set update period (seconds)
    void SetUpdatePeriod(float period);
    
    //! Get update period
    float GetUpdatePeriod() { return update_period_; }

    // Connected to server signal newUserConnected.
    void ProcessNewUserConnection(int, UserConnection*);
    
private slots:
    //! Trigger EC sync because of component attributes changing
    void OnAttributeChanged(IComponent* comp, IAttribute* attr, AttributeChange::Type change);
    
    //! Trigger EC sync because of component added to entity
    void OnComponentAdded(Scene::Entity* entity, IComponent* comp, AttributeChange::Type change);
    
    //! Trigger EC sync because of component removed from entity
    void OnComponentRemoved(Scene::Entity* entity, IComponent* comp, AttributeChange::Type change);
    
    //! Trigger sync of entity creation
    void OnEntityCreated(Scene::Entity* entity, AttributeChange::Type change);
    
    //! Trigger sync of entity removal
    void OnEntityRemoved(Scene::Entity* entity, AttributeChange::Type change);

    //! Trigger sync of entity action.
    void OnActionTriggered(Scene::Entity *entity, const QString &action, const QStringList &params, EntityAction::ExecutionType type);

    //! Trigger sync of entity action to specific user
    void OnUserActionTriggered(UserConnection* user, Scene::Entity *entity, const QString &action, const QStringList &params);

private:
    /// Handle a Kristalli protocol message
    void HandleKristalliMessage(kNet::MessageConnection* source, kNet::message_id_t id, const char* data, size_t numBytes);
    
    //! Handle create entity message
    void HandleCreateEntity(kNet::MessageConnection* source, const MsgCreateEntity& msg);
    
    //! Handle remove entity message
    void HandleRemoveEntity(kNet::MessageConnection* source, const MsgRemoveEntity& msg);
    
    //! Handle create components message
    void HandleCreateComponents(kNet::MessageConnection* source, const MsgCreateComponents& msg);
    
    //! Handle update components message
    void HandleUpdateComponents(kNet::MessageConnection* source, const MsgUpdateComponents& msg);
    
    //! Handle remove components message
    void HandleRemoveComponents(kNet::MessageConnection* source, const MsgRemoveComponents& msg);
    
    //! Handle entityID collision message
    void HandleEntityIDCollision(kNet::MessageConnection* source, const MsgEntityIDCollision& msg);

    //! Handle entity action message.
    void HandleEntityAction(kNet::MessageConnection* source, MsgEntityAction& msg);

    //! Process one sync state for changes in the scene
    /*! \todo For now, sends all changed entities/components. In the future, this shall be subject to interest management
        \param destination MessageConnection where to send the messages
        \param state Syncstate to process
     */
    void ProcessSyncState(kNet::MessageConnection* destination, SceneSyncState* state);
    
    //! Validate the scene manipulation action. If returns false, it is ignored
    /*! \param source Where the action came from
        \param messageID Network message id
        \param entityID What entity it affects
     */
    bool ValidateAction(kNet::MessageConnection* source, unsigned messageID, entity_id_t entityID);
    
    //! Send serializable components of an entity to a connection, using either a CreateEntity or UpdateComponents packet
    /*! \param connections MessageConnection(s) to use
        \param entity Entity
        \param createEntity Whether to use a CreateEntity packet. If false, use a UpdateComponents packet instead
        \param allComponents Whether to send all components, or only those that are dirty
        Note: This will not reset any changeflags in the components or attributes!
     */
    void SerializeAndSendComponents(const std::vector<kNet::MessageConnection*>& connections, Scene::EntityPtr entity, bool createEntity = false, bool allComponents = false);
    
    //! Get a syncstate that matches the messageconnection, for reflecting arrived changes back
    /*! For client, this will always be server_syncstate_.
     */
    SceneSyncState* GetSceneSyncState(kNet::MessageConnection* connection);

    Scene::ScenePtr GetRegisteredScene()
    {
        return scene_.lock();
    }

    
    //! Owning module
    TundraLogicModule* owner_;
    
    //! Framework pointer
    Foundation::Framework* framework_;
    
    //! Scene pointer
    Scene::SceneWeakPtr scene_;
    
    //! Time period for update, default 1/30th of a second
    float update_period_;
    //! Time accumulator for update
    float update_acc_;
    
    //! Server sync state (client operation only)
    SceneSyncState server_syncstate_;

    // This variable is initialized in constructor. This tells what messageConnection this particular syncManager is attached to
    // so it can get right connection through client->GetConnection(unsigned short)
    const unsigned short attachedConnection;

};

}

#endif

