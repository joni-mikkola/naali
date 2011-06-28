/**
 *  For conditions of distribution and use, see copyright notice in license.txt
 *
 *  @file   ScriptMetaTypeDefines.cpp
 *  @brief  Registration of Naali Core API to Javascript.
 */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "MemoryLeakCheck.h"
#include "ScriptMetaTypeDefines.h"

#include "SceneAPI.h"
#include "SceneManager.h"
#include "ChangeRequest.h"
#include "Entity.h"
#include "EntityAction.h"

#include "AssetAPI.h"
#include "IAssetTransfer.h"
#include "IAssetUploadTransfer.h"
#include "IAssetStorage.h"
#include "AssetCache.h"

#include "InputAPI.h"
#include "InputContext.h"
#include "KeyEvent.h"
#include "MouseEvent.h"

#include "UiAPI.h"
#include "UiMainWindow.h"
#include "UiGraphicsView.h"
#include "UiProxyWidget.h"

#include "FrameAPI.h"
#include "ConfigAPI.h"
#include "ConsoleAPI.h"

#include "AudioAPI.h"
#include "SoundChannel.h"

#include "RenderServiceInterface.h"
#include "CommunicationsService.h"

#include "DevicesAPI.h"
#include "IDevice.h"
#include "IPositionalDevice.h"
#include "IControlDevice.h"

#include <QUiLoader>
#include <QFile>

#include "LoggingFunctions.h"
DEFINE_POCO_LOGGING_FUNCTIONS("Script")

//! Qt defines
Q_SCRIPT_DECLARE_QMETAOBJECT(QPushButton, QWidget*)
Q_SCRIPT_DECLARE_QMETAOBJECT(QWidget, QWidget*)
Q_SCRIPT_DECLARE_QMETAOBJECT(QTimer, QObject*);

///\todo Remove these two and move to Input API once NaaliCore is merged.
//! Naali input defines
Q_DECLARE_METATYPE(MouseEvent*)
Q_DECLARE_METATYPE(KeyEvent*)
Q_DECLARE_METATYPE(GestureEvent*)
Q_DECLARE_METATYPE(InputContext*)

//! Asset API defines
Q_DECLARE_METATYPE(AssetPtr);
Q_DECLARE_METATYPE(AssetTransferPtr);
Q_DECLARE_METATYPE(IAssetTransfer*);
Q_DECLARE_METATYPE(AssetUploadTransferPtr);
Q_DECLARE_METATYPE(IAssetUploadTransfer*);
Q_DECLARE_METATYPE(AssetStoragePtr);
Q_DECLARE_METATYPE(IAssetStorage*);
Q_DECLARE_METATYPE(AssetCache*);
Q_DECLARE_METATYPE(CookieJar*);

//! Naali Ui defines
Q_DECLARE_METATYPE(UiProxyWidget*);
Q_DECLARE_METATYPE(UiMainWindow*);
Q_DECLARE_METATYPE(UiGraphicsView*);
Q_SCRIPT_DECLARE_QMETAOBJECT(UiProxyWidget, QWidget*)

//! Naali Scene defines.
Q_DECLARE_METATYPE(SceneAPI*);
Q_DECLARE_METATYPE(Scene::SceneManager*);
Q_DECLARE_METATYPE(Scene::Entity*);
Q_DECLARE_METATYPE(EntityAction*);
Q_DECLARE_METATYPE(EntityAction::ExecutionType);
Q_DECLARE_METATYPE(AttributeChange*);
Q_DECLARE_METATYPE(ChangeRequest*);
Q_DECLARE_METATYPE(IComponent*);
Q_DECLARE_METATYPE(AttributeChange::Type);

//! Naali core API object defines.
Q_DECLARE_METATYPE(Foundation::Framework*);
Q_DECLARE_METATYPE(FrameAPI*);
Q_DECLARE_METATYPE(ConsoleAPI*);
Q_DECLARE_METATYPE(ConsoleCommand*);
Q_DECLARE_METATYPE(DelayedSignal*);
Q_DECLARE_METATYPE(DebugAPI*);

//! Naali Audio API object.
Q_DECLARE_METATYPE(AudioAPI*);
Q_DECLARE_METATYPE(SoundChannel*);

//! Naali Config API object.
Q_DECLARE_METATYPE(ConfigAPI*);

//! Naali renderer defines
Q_DECLARE_METATYPE(RaycastResult*);

//! DeviceAPI defined
Q_DECLARE_METATYPE(DevicesAPI*);
Q_DECLARE_METATYPE(IDevice*);
Q_DECLARE_METATYPE(IPositionalDevice*);
Q_DECLARE_METATYPE(IControlDevice*);
Q_DECLARE_METATYPE(AxisData*);

//! Communications metatype
Q_DECLARE_METATYPE(Communications::InWorldVoice::SessionInterface*);
Q_DECLARE_METATYPE(Communications::InWorldVoice::ParticipantInterface*);

QScriptValue findChild(QScriptContext *ctx, QScriptEngine *eng)
{
    if(ctx->argumentCount() == 2)
    {
        QObject *object = qscriptvalue_cast<QObject*>(ctx->argument(0));
        QString childName = qscriptvalue_cast<QString>(ctx->argument(1));
        if(object)
        {
            QObject *obj = object->findChild<QObject*>(childName);
            if (obj)
                return eng->newQObject(obj);
        }
    }
    return QScriptValue();
}

// Helper function. Added because new'ing a QPixmap in script seems to lead into growing memory use
QScriptValue setPixmapToLabel(QScriptContext *ctx, QScriptEngine *eng)
{
    if(ctx->argumentCount() == 2)
    {
        QObject *object = qscriptvalue_cast<QObject*>(ctx->argument(0));
        QString filename = qscriptvalue_cast<QString>(ctx->argument(1));
        
        QLabel *label = dynamic_cast<QLabel *>(object);
        if (label && QFile::exists(filename))
            label->setPixmap(QPixmap(filename));
    }
    return QScriptValue();
}

void ExposeQtMetaTypes(QScriptEngine *engine)
{
    assert(engine);
    if (!engine)
        return;

    QScriptValue object = engine->scriptValueFromQMetaObject<QPushButton>();
    engine->globalObject().setProperty("QPushButton", object);
    object = engine->scriptValueFromQMetaObject<QWidget>();
    engine->globalObject().setProperty("QWidget", object);
    object = engine->scriptValueFromQMetaObject<QTimer>();
    engine->globalObject().setProperty("QTimer", object);
    engine->globalObject().setProperty("findChild", engine->newFunction(findChild));
    engine->globalObject().setProperty("setPixmapToLabel", engine->newFunction(setPixmapToLabel));   
/*
    engine->importExtension("qt.core");
    engine->importExtension("qt.gui");
    engine->importExtension("qt.network");
    engine->importExtension("qt.uitools");
    engine->importExtension("qt.xml");
    engine->importExtension("qt.xmlpatterns");
*/
//  Our deps contain these plugins as well, but we don't use them (for now at least).
//    engine->importExtension("qt.opengl");
//    engine->importExtension("qt.phonon");
//    engine->importExtension("qt.webkit"); //cvetan hacked this to build with msvc, patch is somewhere

}

template<typename T>
static QScriptValue DereferenceBoostSharedPtr(QScriptContext *context, QScriptEngine *engine)
{
    boost::shared_ptr<T> ptr = context->thisObject().toVariant().value<boost::shared_ptr<T> >();
    return engine->newQObject(ptr.get());
}

template<typename T>
QScriptValue qScriptValueFromBoostSharedPtr(QScriptEngine *engine, const boost::shared_ptr<T> &ptr)
{
    QScriptValue v = engine->newVariant(QVariant::fromValue<boost::shared_ptr<T> >(ptr));
    v.setProperty("get", engine->newFunction(DereferenceBoostSharedPtr<T>), QScriptValue::ReadOnly | QScriptValue::Undeletable);
    return v;
}

template<typename T>
void qScriptValueToBoostSharedPtr(const QScriptValue &value, boost::shared_ptr<T> &ptr)
{
    ptr = value.toVariant().value<boost::shared_ptr<T> >();
}

Q_DECLARE_METATYPE(SoundChannelPtr);
Q_DECLARE_METATYPE(InputContextPtr);

void ExposeCoreApiMetaTypes(QScriptEngine *engine)
{
    // Input metatypes.
    qScriptRegisterQObjectMetaType<MouseEvent*>(engine);
    qScriptRegisterQObjectMetaType<KeyEvent*>(engine);
    qScriptRegisterQObjectMetaType<GestureEvent*>(engine);
    qScriptRegisterQObjectMetaType<InputContext*>(engine);
    qRegisterMetaType<InputContextPtr>("InputContextPtr");
    qScriptRegisterMetaType(engine, qScriptValueFromBoostSharedPtr<InputContext>,
                            qScriptValueToBoostSharedPtr<InputContext>);
    qRegisterMetaType<KeyEvent::EventType>("KeyEvent::EventType");
    qRegisterMetaType<MouseEvent::EventType>("MouseEvent::EventType");
    qRegisterMetaType<MouseEvent::MouseButton>("MouseEvent::MouseButton");
    qRegisterMetaType<GestureEvent::EventType>("GestureEvent::EventType");

    // Scene metatypes.
    qScriptRegisterQObjectMetaType<SceneAPI*>(engine);
    qScriptRegisterQObjectMetaType<Scene::SceneManager*>(engine);
    qScriptRegisterQObjectMetaType<Scene::Entity*>(engine);
    qScriptRegisterQObjectMetaType<EntityAction*>(engine);
    qScriptRegisterQObjectMetaType<AttributeChange*>(engine);
    qScriptRegisterQObjectMetaType<ChangeRequest*>(engine);
    qScriptRegisterQObjectMetaType<IComponent*>(engine);
    //qRegisterMetaType<AttributeChange::Type>("AttributeChange::Type");
    qScriptRegisterMetaType(engine, toScriptValueEnum<AttributeChange::Type>, fromScriptValueEnum<AttributeChange::Type>);
    //qRegisterMetaType<EntityAction::ExecutionType>("EntityAction::ExecutionType");
    qScriptRegisterMetaType(engine, toScriptValueEnum<EntityAction::ExecutionType>, fromScriptValueEnum<EntityAction::ExecutionType>);

    // Framework metatype
    qScriptRegisterQObjectMetaType<Foundation::Framework*>(engine);
    
    // Console metatypes.
    qScriptRegisterQObjectMetaType<ConsoleAPI*>(engine);
    qScriptRegisterQObjectMetaType<ConsoleCommand*>(engine);

    // Frame metatypes.
    qScriptRegisterQObjectMetaType<FrameAPI*>(engine);
    qScriptRegisterQObjectMetaType<DelayedSignal*>(engine);

    // Config metatypes.
    qScriptRegisterQObjectMetaType<ConfigAPI*>(engine);

    // Devices metatype.
    qScriptRegisterQObjectMetaType<DevicesAPI*>(engine);
    qScriptRegisterQObjectMetaType<IDevice*>(engine);
    qScriptRegisterQObjectMetaType<IPositionalDevice*>(engine);
    qScriptRegisterQObjectMetaType<IControlDevice*>(engine);
    qScriptRegisterQObjectMetaType<AxisData*>(engine);

    // Asset API
    qRegisterMetaType<AssetPtr>("AssetPtr");
    qScriptRegisterMetaType(engine, qScriptValueFromBoostSharedPtr<IAsset>, qScriptValueToBoostSharedPtr<IAsset>);

    qRegisterMetaType<AssetTransferPtr>("AssetTransferPtr");
    qScriptRegisterQObjectMetaType<IAssetTransfer*>(engine);
    qScriptRegisterMetaType(engine, qScriptValueFromBoostSharedPtr<IAssetTransfer>, qScriptValueToBoostSharedPtr<IAssetTransfer>);

    qRegisterMetaType<AssetUploadTransferPtr>("AssetUploadTransferPtr");
    qScriptRegisterQObjectMetaType<IAssetUploadTransfer*>(engine);
    qScriptRegisterMetaType(engine, qScriptValueFromBoostSharedPtr<IAssetUploadTransfer>, qScriptValueToBoostSharedPtr<IAssetUploadTransfer>);

    qRegisterMetaType<AssetStoragePtr>("AssetStoragePtr");
    qScriptRegisterQObjectMetaType<IAssetStorage*>(engine);
    qScriptRegisterMetaType(engine, qScriptValueFromBoostSharedPtr<IAssetStorage>, qScriptValueToBoostSharedPtr<IAssetStorage>);

    qScriptRegisterQObjectMetaType<AssetCache*>(engine);
    qScriptRegisterQObjectMetaType<CookieJar*>(engine);

    // Ui metatypes.
    qScriptRegisterQObjectMetaType<UiMainWindow*>(engine);
    qScriptRegisterQObjectMetaType<UiGraphicsView*>(engine);
    qScriptRegisterQObjectMetaType<UiProxyWidget*>(engine);
    qScriptRegisterQObjectMetaType<QGraphicsScene*>(engine);

    // Add support to create proxy widgets in javascript side.
    QScriptValue object = engine->scriptValueFromQMetaObject<UiProxyWidget>();
    engine->globalObject().setProperty("UiProxyWidget", object);
    
    // Sound metatypes.
    qRegisterMetaType<sound_id_t>("sound_id_t");
    qRegisterMetaType<SoundChannel::SoundState>("SoundState");
    qRegisterMetaType<SoundChannelPtr>("SoundChannelPtr");
    qScriptRegisterQObjectMetaType<SoundChannel*>(engine);
    qScriptRegisterMetaType(engine, qScriptValueFromBoostSharedPtr<SoundChannel>, qScriptValueToBoostSharedPtr<SoundChannel>);
    
    qRegisterMetaType<SoundChannel::SoundState>("SoundChannel::SoundState");
    qRegisterMetaType<SoundChannel::SoundType>("SoundType");
    qRegisterMetaType<SoundChannel::SoundType>("SoundChannel::SoundType");

    // Renderer metatypes
    qScriptRegisterQObjectMetaType<RaycastResult*>(engine);

    // Communications metatypes
    qScriptRegisterQObjectMetaType<Communications::InWorldVoice::SessionInterface*>(engine);
    qScriptRegisterQObjectMetaType<Communications::InWorldVoice::ParticipantInterface*>(engine);
}


