// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include <btBulletDynamicsCommon.h>
#include "MemoryLeakCheck.h"
#include "EC_RigidBody.h"
#include "ConvexHull.h"
#include "PhysicsModule.h"
#include "PhysicsUtils.h"
#include "PhysicsWorld.h"
#include "OgreMeshAsset.h"

#include "Entity.h"
#include "EC_Mesh.h"
#include "EC_Placeable.h"
#include "EC_Terrain.h"
#include "ServiceManager.h"
#include "AssetAPI.h"
#include "IAssetTransfer.h"
#include "LoggingFunctions.h"

DEFINE_POCO_LOGGING_FUNCTIONS("EC_RigidBody");

#include <BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h>

using namespace Physics;

static const float cForceThreshold = 0.0005f;
static const float cImpulseThreshold = 0.0005f;
static const float cTorqueThreshold = 0.0005f;

EC_RigidBody::EC_RigidBody(IModule* module) :
    IComponent(module->GetFramework()),
    mass(this, "Mass", 0.0f),
    shapeType(this, "Shape type", (int)Shape_Box),
    size(this, "Size", Vector3df(1,1,1)),
    collisionMeshRef(this, "Collision mesh ref"),
    friction(this, "Friction", 0.5f),
    restitution(this, "Restitution", 0.0f),
    linearDamping(this, "Linear damping", 0.0f),
    angularDamping(this, "Angular damping", 0.0f),
    linearFactor(this, "Linear factor", Vector3df(1,1,1)),
    angularFactor(this, "Angular factor", Vector3df(1,1,1)),
    linearVelocity(this, "Linear velocity", Vector3df(0,0,0)),
    angularVelocity(this, "Angular velocity", Vector3df(0,0,0)),
    gravityEnabled(this, "Gravity", true),
    phantom(this, "Phantom", false),
    drawDebug(this, "Draw Debug", true),
    body_(0),
    world_(0),
    shape_(0),
    heightField_(0),
    disconnected_(false),
    owner_(checked_static_cast<PhysicsModule*>(module)),
    cachedShapeType_(-1)
{
    static AttributeMetadata shapemetadata;
    static bool metadataInitialized = false;
    if(!metadataInitialized)
    {
        shapemetadata.enums[Shape_Box] = "Box";
        shapemetadata.enums[Shape_Sphere] = "Sphere";
        shapemetadata.enums[Shape_Cylinder] = "Cylinder";
        shapemetadata.enums[Shape_Capsule] = "Capsule";
        shapemetadata.enums[Shape_TriMesh] = "TriMesh";
        shapemetadata.enums[Shape_HeightField] = "HeightField";
        shapemetadata.enums[Shape_ConvexHull] = "ConvexHull";
        metadataInitialized = true;
    }
    shapeType.SetMetadata(&shapemetadata);

    // Note: we cannot create the body yet because we are not in an entity/scene yet (and thus don't know what physics world we belong to)
    // We will create the body when the scene is known.
    connect(this, SIGNAL(ParentEntitySet()), SLOT(UpdateSignals()));
    connect(this, SIGNAL(AttributeChanged(IAttribute*, AttributeChange::Type)), SLOT(OnAttributeUpdated(IAttribute*)));
}

EC_RigidBody::~EC_RigidBody()
{
    RemoveBody();
    RemoveCollisionShape();
}

bool EC_RigidBody::SetShapeFromVisibleMesh()
{
    Scene::Entity* parent = GetParentEntity();
    if (!parent)
        return false;
    EC_Mesh* mesh = parent->GetComponent<EC_Mesh>().get();
    if (!mesh)
        return false;
    mass.Set(0.0f, AttributeChange::Default);
    shapeType.Set(Shape_TriMesh, AttributeChange::Default);
    collisionMeshRef.Set(mesh->meshRef.Get().ref, AttributeChange::Default);
    return true;
}

void EC_RigidBody::SetLinearVelocity(const Vector3df& velocity)
{
    // Cannot modify server-authoritative physics object
    if (!HasAuthority())
        return;
    
    linearVelocity.Set(velocity, AttributeChange::Default);
}

void EC_RigidBody::SetAngularVelocity(const Vector3df& velocity)
{
    // Cannot modify server-authoritative physics object
    if (!HasAuthority())
        return;
    
    angularVelocity.Set(velocity, AttributeChange::Default);
}

void EC_RigidBody::ApplyForce(const Vector3df& force, const Vector3df& position)
{
    // Cannot modify server-authoritative physics object
    if (!HasAuthority())
        return;
    
    // If force is very small, do not wake up the body and apply
    if (force.getLength() < cForceThreshold)
        return;
    
    if (!body_)
        CreateBody();
    if (body_)
    {
        Activate();
        if (position == Vector3df::ZERO)
            body_->applyCentralForce(ToBtVector3(force));
        else
            body_->applyForce(ToBtVector3(force), ToBtVector3(position));
    }
}

void EC_RigidBody::ApplyTorque(const Vector3df& torque)
{
    // Cannot modify server-authoritative physics object
    if (!HasAuthority())
        return;
    
    // If torque is very small, do not wake up the body and apply
    if (torque.getLength() < cTorqueThreshold)
        return;
        
    if (!body_)
        CreateBody();
    if (body_)
    {
        Activate();
        body_->applyTorque(ToBtVector3(torque));
    }
}

void EC_RigidBody::ApplyImpulse(const Vector3df& impulse, const Vector3df& position)
{
    // Cannot modify server-authoritative physics object
    if (!HasAuthority())
        return;
    
    // If impulse is very small, do not wake up the body and apply
    if (impulse.getLength() < cImpulseThreshold)
        return;
    
    if (!body_)
        CreateBody();
    if (body_)
    {
        Activate();
        if (position == Vector3df::ZERO)
            body_->applyCentralImpulse(ToBtVector3(impulse));
        else
            body_->applyImpulse(ToBtVector3(impulse), ToBtVector3(position));
    }
}

void EC_RigidBody::ApplyTorqueImpulse(const Vector3df& torqueImpulse)
{
    // Cannot modify server-authoritative physics object
    if (!HasAuthority())
        return;
    
    // If impulse is very small, do not wake up the body and apply
    if (torqueImpulse.getLength() < cTorqueThreshold)
        return;
        
    if (!body_)
        CreateBody();
    if (body_)
    {
        Activate();
        body_->applyTorqueImpulse(ToBtVector3(torqueImpulse));
    }
}

void EC_RigidBody::Activate()
{
    // Cannot modify server-authoritative physics object
    if (!HasAuthority())
        return;
    
    if (!body_)
        CreateBody();
    if (body_)
        body_->activate();
}

bool EC_RigidBody::IsActive()
{
    if (body_)
        return body_->isActive();
    else
        return false;
}

void EC_RigidBody::ResetForces()
{
    // Cannot modify server-authoritative physics object
    if (!HasAuthority())
        return;
    
    if (!body_)
        CreateBody();
    if (body_)
        body_->clearForces();
}

void EC_RigidBody::UpdateSignals()
{
    Scene::Entity* parent = GetParentEntity();
    if (!parent)
        return;
    
    connect(parent, SIGNAL(ComponentAdded(IComponent*, AttributeChange::Type)), this, SLOT(CheckForPlaceableAndTerrain()));
    
    Scene::SceneManager* scene = parent->GetScene();
    world_ = owner_->GetPhysicsWorldForScene(scene);
}

void EC_RigidBody::CheckForPlaceableAndTerrain()
{
    Scene::Entity* parent = GetParentEntity();
    if (!parent)
        return;
    
    if (!placeable_.lock())
    {
        boost::shared_ptr<EC_Placeable> placeable = parent->GetComponent<EC_Placeable>();
        if (placeable)
        {
            placeable_ = placeable;
            connect(placeable.get(), SIGNAL(AttributeChanged(IAttribute*, AttributeChange::Type)), this, SLOT(PlaceableUpdated(IAttribute*)));
        }
    }
    if (!terrain_.lock())
    {
        boost::shared_ptr<Environment::EC_Terrain> terrain = parent->GetComponent<Environment::EC_Terrain>();
        if (terrain)
        {
            terrain_ = terrain;
            connect(terrain.get(), SIGNAL(TerrainRegenerated()), this, SLOT(OnTerrainRegenerated()));
            connect(terrain.get(), SIGNAL(AttributeChanged(IAttribute*, AttributeChange::Type)), this, SLOT(TerrainUpdated(IAttribute*)));
        }
    }
}

void EC_RigidBody::CreateCollisionShape()
{
    RemoveCollisionShape();
    
    Vector3df sizeVec = size.Get();
    // Sanitize the size
    if (sizeVec.x < 0)
        sizeVec.x = 0;
    if (sizeVec.y < 0)
        sizeVec.y = 0;
    if (sizeVec.z < 0)
        sizeVec.z = 0;
        
    switch (shapeType.Get())
    {
    case Shape_Box:
        // Note: Bullet uses box halfsize
        shape_ = new btBoxShape(btVector3(sizeVec.x * 0.5f, sizeVec.y * 0.5f, sizeVec.z * 0.5f));
        break;
    case Shape_Sphere:
        shape_ = new btSphereShape(sizeVec.x * 0.5f);
        break;
    case Shape_Cylinder:
        shape_ = new btCylinderShapeZ(btVector3(sizeVec.x * 0.5f, sizeVec.y * 0.5f, sizeVec.z * 0.5f));
        break;
    case Shape_Capsule:
        shape_ = new btCapsuleShapeZ(sizeVec.x * 0.5f, sizeVec.z * 0.5f);
        break;
    case Shape_TriMesh:
        if (triangleMesh_)
            shape_ = new btBvhTriangleMeshShape(triangleMesh_.get(), true, true);
    case Shape_HeightField:
        CreateHeightFieldFromTerrain();
        break;
    case Shape_ConvexHull:
        CreateConvexHullSetShape();
        break;
    }
    
    UpdateScale();
    
    // If body already exists, set the new collision shape, and remove/readd the body to the physics world to make sure Bullet's internal representations are updated
    ReaddBody();
}

void EC_RigidBody::RemoveCollisionShape()
{
    if (shape_)
    {
        if (body_)
            body_->setCollisionShape(0);
        delete shape_;
        shape_ = 0;
    }
    if (heightField_)
    {
        delete heightField_;
        heightField_ = 0;
    }
}

void EC_RigidBody::CreateBody()
{
    if ((!world_) || (!GetParentEntity()) || (body_))
        return;
    
    CheckForPlaceableAndTerrain();
    
    CreateCollisionShape();
    
    btVector3 localInertia;
    float m;
    int collisionFlags;
    
    GetProperties(localInertia, m, collisionFlags);
    
    body_ = new btRigidBody(m, this, shape_, localInertia);
    body_->setUserPointer(this);
    body_->setCollisionFlags(collisionFlags);
    world_->GetWorld()->addRigidBody(body_);
    body_->activate();

    gravity_ = body_->getGravity();
}

void EC_RigidBody::ReaddBody()
{
    if ((!world_) || (!GetParentEntity()) || (!body_))
        return;
    
    btVector3 localInertia;
    float m;
    int collisionFlags;
    
    GetProperties(localInertia, m, collisionFlags);
    
    body_->setCollisionShape(shape_);
    body_->setMassProps(m, localInertia);
    body_->setCollisionFlags(collisionFlags);
    
    world_->GetWorld()->removeRigidBody(body_);
    world_->GetWorld()->addRigidBody(body_);
    body_->clearForces();
    body_->setLinearVelocity(btVector3(0.0f, 0.0f, 0.0f));
    body_->setAngularVelocity(btVector3(0.0f, 0.0f, 0.0f));
    body_->activate();
}

void EC_RigidBody::RemoveBody()
{
    if ((body_) && (world_))
    {
        world_->GetWorld()->removeRigidBody(body_);
        delete body_;
        body_ = 0;
    }
}

void EC_RigidBody::getWorldTransform(btTransform &worldTrans) const
{
    EC_Placeable* placeable = placeable_.lock().get();
    if (!placeable)
        return;
        
    const Transform& trans = placeable->transform.Get();
    const Vector3df& position = trans.position;
    Quaternion orientation(DEGTORAD * trans.rotation.x, DEGTORAD * trans.rotation.y, DEGTORAD * trans.rotation.z);
    
    worldTrans.setOrigin(ToBtVector3(position));
    worldTrans.setRotation(ToBtQuaternion(orientation));
}

void EC_RigidBody::setWorldTransform(const btTransform &worldTrans)
{
    // Cannot modify server-authoritative physics object, rather get the transform changes through placeable attributes
    if (!HasAuthority())
        return;
    
    EC_Placeable* placeable = placeable_.lock().get();
    if (!placeable)
        return;
    
    // Important: disconnect our own response to attribute changes to not create an endless loop!
    disconnected_ = true;
    
    // Set transform
    Vector3df position = ToVector3(worldTrans.getOrigin());
    Quaternion orientation = ToQuaternion(worldTrans.getRotation());
    Transform newTrans = placeable->transform.Get();
    Vector3df euler;
    orientation.toEuler(euler);
    newTrans.SetPos(position.x, position.y, position.z);
    newTrans.SetRot(euler.x * RADTODEG, euler.y * RADTODEG, euler.z * RADTODEG);
    placeable->transform.Set(newTrans, AttributeChange::Default);
    
    // Set linear & angular velocity
    if (body_)
    {
        linearVelocity.Set(ToVector3(body_->getLinearVelocity()), AttributeChange::Default);
        const btVector3& angular = body_->getAngularVelocity();
        angularVelocity.Set(Vector3df(angular.x() * RADTODEG, angular.y() * RADTODEG, angular.z() * RADTODEG), AttributeChange::Default);
    }
    
    disconnected_ = false;
}

void EC_RigidBody::OnTerrainRegenerated()
{
    if (shapeType.Get() == Shape_HeightField)
        CreateCollisionShape();
}

void EC_RigidBody::OnCollisionMeshAssetLoaded(AssetPtr asset)
{
    OgreMeshAsset *meshAsset = dynamic_cast<OgreMeshAsset*>(asset.get());
    if (!meshAsset || !meshAsset->ogreMesh.get())
        LogError("EC_RigidBody::OnCollisionMeshAssetLoaded: Mesh asset load finished for asset \"" + asset->Name().toStdString() + "\", but Ogre::Mesh pointer was null!");

    Ogre::Mesh *mesh = meshAsset->ogreMesh.get();

    if (mesh)
    {
        if (shapeType.Get() == Shape_TriMesh)
        {
            triangleMesh_ = owner_->GetTriangleMeshFromOgreMesh(mesh);
            CreateCollisionShape();
        }
        if (shapeType.Get() == Shape_ConvexHull)
        {
            convexHullSet_ = owner_->GetConvexHullSetFromOgreMesh(mesh);
            CreateCollisionShape();
        }

        cachedShapeType_ = shapeType.Get();
        cachedSize_ = size.Get();
    }
}

void EC_RigidBody::OnAttributeUpdated(IAttribute* attribute)
{
    if (disconnected_)
        return;
    
    // Create body now if does not exist yet
    if (!body_)
        CreateBody();
    // If body was not created (we do not actually have a physics world), exit
    if (!body_)
        return;
    
    if (attribute == &mass)
        // Readd body to the world in case static/dynamic classification changed
        ReaddBody();
    
    if (attribute == &friction)
        body_->setFriction(friction.Get());
    
    if (attribute == &restitution)
        body_->setRestitution(friction.Get());
    
    if ((attribute == &linearDamping) || (attribute == &angularDamping))
         body_->setDamping(linearDamping.Get(), angularDamping.Get());
    
    if (attribute == &linearFactor)
        body_->setLinearFactor(ToBtVector3(linearFactor.Get()));
    
    if (attribute == &angularFactor)
        body_->setAngularFactor(ToBtVector3(angularFactor.Get()));
    
    if ((attribute == &shapeType) || (attribute == &size))
    {
        if ((shapeType.Get() != cachedShapeType_) || (size.Get() != cachedSize_))
        {
            // If shape does not involve mesh, can create it directly. Otherwise request the mesh
            if ((shapeType.Get() != Shape_TriMesh) && (shapeType.Get() != Shape_ConvexHull))
            {
                CreateCollisionShape();
                cachedShapeType_ = shapeType.Get();
                cachedSize_ = size.Get();
            }
            else
                RequestMesh();
        }
    }
    
    // Request mesh if its id changes
    if ((attribute == &collisionMeshRef) && ((shapeType.Get() == Shape_TriMesh) || (shapeType.Get() == Shape_ConvexHull)))
        RequestMesh();
    
    if (attribute == &phantom)
        // Readd body to the world in case phantom classification changed
        ReaddBody();
    
    if (attribute == &drawDebug)
        // Readd body to the world in case phantom classification changed
        ReaddBody();
        
    if (attribute == &linearVelocity)
    {
        body_->setLinearVelocity(ToBtVector3(linearVelocity.Get()));
        body_->activate();
    }
    
    if (attribute == &angularVelocity)
    {
        const Vector3df& angular = angularVelocity.Get();
        body_->setAngularVelocity(btVector3(angular.x * DEGTORAD, angular.y * DEGTORAD, angular.z * DEGTORAD));
        body_->activate();
    }

    if (attribute == &gravityEnabled)
    {
        // Cannot modify server-authoritative physics object
        if (!HasAuthority())
            return;

        if (gravityEnabled.Get())
            body_->setGravity(gravity_);
        else
            body_->setGravity(btVector3(0,0,0));
    }
}

void EC_RigidBody::PlaceableUpdated(IAttribute* attribute)
{
    // Do not respond to our own change
    if ((disconnected_) || (!body_))
        return;
    
    EC_Placeable* placeable = checked_static_cast<EC_Placeable*>(sender());
    if (attribute == &placeable->transform)
    {
        const Transform& trans = placeable->transform.Get();
        const Vector3df& position = trans.position;
        Quaternion orientation(DEGTORAD * trans.rotation.x, DEGTORAD * trans.rotation.y, DEGTORAD * trans.rotation.z);
        
        btTransform& worldTrans = body_->getWorldTransform();
        worldTrans.setOrigin(ToBtVector3(position));
        worldTrans.setRotation(ToBtQuaternion(orientation));
        
        // When we forcibly set the physics transform, also set the interpolation transform to prevent jerky motion
        btTransform interpTrans = body_->getInterpolationWorldTransform();
        interpTrans.setOrigin(worldTrans.getOrigin());
        interpTrans.setRotation(worldTrans.getRotation());
        body_->setInterpolationWorldTransform(interpTrans);
        
        body_->activate();
        
        UpdateScale();
    }
}

void EC_RigidBody::SetRotation(const Vector3df& rotation)
{
    // Cannot modify server-authoritative physics object
    if (!HasAuthority())
        return;
    
    disconnected_ = true;
    
    EC_Placeable* placeable = placeable_.lock().get();
    if (placeable)
    {
        Transform trans = placeable->transform.Get();
        trans.rotation = rotation;
        placeable->transform.Set(trans, AttributeChange::Default);
        
        if (body_)
        {
            Quaternion orientation(DEGTORAD * trans.rotation.x, DEGTORAD * trans.rotation.y, DEGTORAD * trans.rotation.z);
            
            btTransform& worldTrans = body_->getWorldTransform();
            btTransform interpTrans = body_->getInterpolationWorldTransform();
            worldTrans.setRotation(ToBtQuaternion(orientation));
            interpTrans.setRotation(worldTrans.getRotation());
            body_->setInterpolationWorldTransform(interpTrans);
        }
    }
    
    disconnected_ = false;
}

void EC_RigidBody::Rotate(const Vector3df& rotation)
{
    // Cannot modify server-authoritative physics object
    if (!HasAuthority())
        return;
    
    disconnected_ = true;
    
    EC_Placeable* placeable = placeable_.lock().get();
    if (placeable)
    {
        Transform trans = placeable->transform.Get();
        trans.rotation += rotation;
        placeable->transform.Set(trans, AttributeChange::Default);
        
        if (body_)
        {
            Quaternion orientation(DEGTORAD * trans.rotation.x, DEGTORAD * trans.rotation.y, DEGTORAD * trans.rotation.z);
            
            btTransform& worldTrans = body_->getWorldTransform();
            btTransform interpTrans = body_->getInterpolationWorldTransform();
            worldTrans.setRotation(ToBtQuaternion(orientation));
            interpTrans.setRotation(worldTrans.getRotation());
            body_->setInterpolationWorldTransform(interpTrans);
        }
    }
    
    disconnected_ = false;
}

Vector3df EC_RigidBody::GetLinearVelocity()
{
    if (body_)
        return ToVector3(body_->getLinearVelocity());
    else 
        return linearVelocity.Get();
}

Vector3df EC_RigidBody::GetAngularVelocity()
{
    if (body_)
        return ToVector3(body_->getAngularVelocity()) * RADTODEG;
    else
        return angularVelocity.Get();
}

void EC_RigidBody::GetAabbox(Vector3df &outAabbMin, Vector3df &outAabbMax)
{
    btVector3 aabbMin, aabbMax;
    body_->getAabb(aabbMin, aabbMax);
    outAabbMin.set(aabbMin.x(), aabbMin.y(), aabbMin.z());
    outAabbMax.set(aabbMax.x(), aabbMax.y(), aabbMax.z());
}

bool EC_RigidBody::HasAuthority() const
{
    if ((!world_) || ((world_->IsClient()) && (!GetParentEntity()->IsLocal())))
        return false;
    
    return true;
}

void EC_RigidBody::TerrainUpdated(IAttribute* attribute)
{
    Environment::EC_Terrain* terrain = terrain_.lock().get();
    if (!terrain)
        return;
    //! \todo It is suboptimal to regenerate the whole heightfield when just the terrain's transform changes
    if ((attribute == &terrain->nodeTransformation) && (shapeType.Get() == Shape_HeightField))
        CreateCollisionShape();
}

void EC_RigidBody::RequestMesh()
{    
    Scene::Entity *parent = GetParentEntity();

    QString collisionMesh = collisionMeshRef.Get().ref.trimmed();
    if (collisionMesh.isEmpty() && parent) // We use the mesh ref in EC_Mesh as the collision mesh ref if no collision mesh is set in EC_RigidBody.
    {
        boost::shared_ptr<EC_Mesh> mesh = parent->GetComponent<EC_Mesh>();
        if (!mesh)
            return;
        collisionMesh = mesh->meshRef.Get().ref.trimmed();
    }

    if (!collisionMesh.isEmpty())
    {
        // Do not create shape right now, but request the mesh resource
        AssetTransferPtr transfer = GetFramework()->Asset()->RequestAsset(collisionMesh);
        if (transfer)
            connect(transfer.get(), SIGNAL(Loaded(AssetPtr)), SLOT(OnCollisionMeshAssetLoaded(AssetPtr)), Qt::UniqueConnection);
    }
}

void EC_RigidBody::UpdateScale()
{
   Vector3df sizeVec = size.Get();
    // Sanitize the size
    if (sizeVec.x < 0)
        sizeVec.x = 0;
    if (sizeVec.y < 0)
        sizeVec.y = 0;
    if (sizeVec.z < 0)
        sizeVec.z = 0;
    
    // If placeable exists, set local scaling from its scale
    /*! \todo Evil hack: we currently have an adjustment node for Ogre->OpenSim coordinate space conversion,
        but Ogre scaling of child nodes disregards the rotation,
        so have to swap y/z axes here to have meaningful controls. Hopefully removed in the future.
    */
    EC_Placeable* placeable = placeable_.lock().get();
    if ((placeable) && (shape_))
    {
        const Transform& trans = placeable->transform.Get();
        // Trianglemesh does not have scaling of its own, so use the size
        if ((!triangleMesh_) || (shapeType.Get() != Shape_TriMesh))
            shape_->setLocalScaling(btVector3(trans.scale.x, trans.scale.z, trans.scale.y));
        else
            shape_->setLocalScaling(btVector3(sizeVec.x * trans.scale.x, sizeVec.y * trans.scale.z, sizeVec.z * trans.scale.y));
    }
}

void EC_RigidBody::CreateHeightFieldFromTerrain()
{
    CheckForPlaceableAndTerrain();
    
    Environment::EC_Terrain* terrain = terrain_.lock().get();
    if (!terrain)
        return;
    
    int width = terrain->PatchWidth() * Environment::EC_Terrain::cPatchSize;
    int height = terrain->PatchHeight() * Environment::EC_Terrain::cPatchSize;
    
    if ((!width) || (!height))
        return;
    
    heightValues_.resize(width * height);
    
    float xySpacing = 1.0f;
    float zSpacing = 1.0f;
    float minZ = 1000000000;
    float maxZ = -1000000000;
    for (uint y = 0; y < height; ++y)
    {
        for (uint x = 0; x < width; ++x)
        {
            float value = terrain->GetPoint(x, y);
            if (value < minZ)
                minZ = value;
            if (value > maxZ)
                maxZ = value;
            heightValues_[y * width + x] = value;
        }
    }
    
    Vector3df scale = terrain->nodeTransformation.Get().scale;
    Vector3df bbMin(0, 0, minZ);
    Vector3df bbMax(xySpacing * (width - 1), xySpacing * (height - 1), maxZ);
    Vector3df bbCenter = scale * (bbMin + bbMax) * 0.5f;
    
    heightField_ = new btHeightfieldTerrainShape(width, height, &heightValues_[0], zSpacing, minZ, maxZ, 2, PHY_FLOAT, false);
    
    /*! \todo EC_Terrain uses its own transform that is independent of the placeable. It is not nice to support, since rest of EC_RigidBody assumes
        the transform is in the placeable. Right now, we only support position & scaling. Here, we also counteract Bullet's nasty habit to center 
        the heightfield on its own. Also, Bullet's collisionshapes generally do not support arbitrary transforms, so we must construct a "compound shape"
        and add the heightfield as its child, to be able to specify the transform.
     */
    heightField_->setLocalScaling(ToBtVector3(scale));
    
    Vector3df positionAdjust = terrain->nodeTransformation.Get().position;
    positionAdjust += bbCenter;
    
    btCompoundShape* compound = new btCompoundShape();
    shape_ = compound;
    compound->addChildShape(btTransform(btQuaternion(0,0,0,1), ToBtVector3(positionAdjust)), heightField_);
}

void EC_RigidBody::CreateConvexHullSetShape()
{
    if (!convexHullSet_)
        return;
    btCompoundShape* compound = new btCompoundShape();
    shape_ = compound;
    for (uint i = 0; i < convexHullSet_->hulls_.size(); ++i)
        compound->addChildShape(btTransform(btQuaternion(0,0,0,1), ToBtVector3(convexHullSet_->hulls_[i].position_)), convexHullSet_->hulls_[i].hull_.get());
}

void EC_RigidBody::GetProperties(btVector3& localInertia, float& m, int& collisionFlags)
{
    localInertia = btVector3(0.0f, 0.0f, 0.0f);
    m = mass.Get();
    if (m < 0.0f)
        m = 0.0f;
    // Trimesh shape can not move
    if (shapeType.Get() == Shape_TriMesh)
        m = 0.0f;
    // On client, all server-side entities become static to not desync or try to send updates we should not
    if (!HasAuthority())
        m = 0.0f;
    
    if ((shape_) && (m > 0.0f))
        shape_->calculateLocalInertia(m, localInertia);
    
    bool isDynamic = m > 0.0f;
    bool isPhantom = phantom.Get();
    collisionFlags = 0;
    if (!isDynamic)
        collisionFlags |= btCollisionObject::CF_STATIC_OBJECT;
    if (isPhantom)
        collisionFlags |= btCollisionObject::CF_NO_CONTACT_RESPONSE;
    if (!drawDebug.Get())
        collisionFlags |= btCollisionObject::CF_DISABLE_VISUALIZE_OBJECT;
}

void EC_RigidBody::EmitPhysicsCollision(Scene::Entity* otherEntity, const Vector3df& position, const Vector3df& normal, float distance, float impulse, bool newCollision)
{
    emit PhysicsCollision(otherEntity, position, normal, distance, impulse, newCollision);
}

void EC_RigidBody::InterpolateUpward()
{
    btVector3 linearVelocity, angularVelocity;
    btTransform fromA, toA;

    body_->getMotionState()->getWorldTransform(fromA);
    btQuaternion pointUp(btVector3(0.f, 1.f, 0.f), 0.0f);
    btMatrix3x3 fromMat = fromA.getBasis();
    btQuaternion orientation;

    fromA.getBasis().getRotation(orientation);
    pointUp *= orientation;

    btMatrix3x3 upMat(pointUp);

    toA.setBasis(upMat);

    btTransformUtil::calculateVelocity(fromA, toA, btScalar(1.0f), linearVelocity, angularVelocity);

    body_->setAngularVelocity(angularVelocity);
}