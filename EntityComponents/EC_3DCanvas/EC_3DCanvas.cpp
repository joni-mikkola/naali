// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "MemoryLeakCheck.h"
#include "EC_3DCanvas.h"

#include "Renderer.h"
#include "IModule.h"
#include "OgreMaterialUtils.h"
#include "Entity.h"
#include "RexTypes.h"

#include "EC_Mesh.h"
#include "EC_OgreCustomObject.h"

#include <QWidget>
#include <QPainter>

#include "LoggingFunctions.h"
DEFINE_POCO_LOGGING_FUNCTIONS("EC_3DCanvas")

#include <QDebug>

EC_3DCanvas::EC_3DCanvas(IModule *module) :
    IComponent(module->GetFramework()),
    widget_(0),
    update_internals_(false),
    refresh_timer_(0),
    update_interval_msec_(0),
    material_name_(""),
    texture_name_("")
{
    if (framework_->IsHeadless())
        return;

    boost::shared_ptr<OgreRenderer::Renderer> renderer = module->GetFramework()->GetServiceManager()->
        GetService<OgreRenderer::Renderer>(Service::ST_Renderer).lock();
    mesh_hooked_ = false;
    if (renderer)
    {
        // Create material
        material_name_ = renderer->GetUniqueObjectName("EC_3DCanvas_mat");
        Ogre::MaterialPtr material = OgreRenderer::GetOrCreateLitTexturedMaterial(material_name_);
        if (material.isNull())
            material_name_ = "";

        // Create texture
        texture_name_ = renderer->GetUniqueObjectName("EC_3DCanvas_tex");
        Ogre::TexturePtr texture = Ogre::TextureManager::getSingleton().createManual(
            texture_name_, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
            Ogre::TEX_TYPE_2D, 1, 1, 0, Ogre::PF_A8R8G8B8, 
            Ogre::TU_DYNAMIC_WRITE_ONLY_DISCARDABLE);
        if (texture.isNull())
            texture_name_ = "";
    }

    connect(this, SIGNAL(ParentEntitySet()), SLOT(ParentEntitySet()), Qt::UniqueConnection);
}

EC_3DCanvas::~EC_3DCanvas()
{
    if (framework_->IsHeadless())
        return;

    submeshes_.clear();
    widget_ = 0;

    if (refresh_timer_)
        refresh_timer_->stop();
    SAFE_DELETE(refresh_timer_);

    if (!material_name_.empty())
    {
        try
        {
            Ogre::MaterialManager::getSingleton().remove(material_name_);
        }
        catch (...) {}
    }

    if (!texture_name_.empty())
    {
        try
        {
            Ogre::TextureManager::getSingleton().remove(texture_name_);
        }
        catch (...) {}
    }
}

void EC_3DCanvas::Start()
{
    if (framework_->IsHeadless())
        return;

    update_internals_ = true;
    if (update_interval_msec_ != 0 && refresh_timer_)
    {
        if (refresh_timer_->isActive())
            refresh_timer_->stop();
        refresh_timer_->start(update_interval_msec_);
    }
    else
        Update();

    if(!mesh_hooked_)
    {
        Scene::Entity* entity = GetParentEntity();
        EC_Mesh* ec_mesh = entity->GetComponent<EC_Mesh>().get();
        if (ec_mesh)
        {
            connect(ec_mesh, SIGNAL(MaterialChanged(uint, const QString)), SLOT(MeshMaterialsUpdated(uint, const QString)));
            mesh_hooked_ = true;
        }
    }
}

void EC_3DCanvas::MeshMaterialsUpdated(uint index, const QString &material_name)
{
    if (framework_->IsHeadless())
        return;

    if (material_name_.empty())
        return;
    if(material_name.compare(QString(material_name_.c_str())) != 0 )
    {
        bool has_index = false;
        for (int i=0; i<submeshes_.length(); i++)
        {
            if (index == submeshes_[i])
                has_index = true;
        }
        if(has_index)
            UpdateSubmeshes();
    }
}

void EC_3DCanvas::Stop()
{
    if (framework_->IsHeadless())
        return;

    if (refresh_timer_)
        if (refresh_timer_->isActive())
            refresh_timer_->stop();
}

void EC_3DCanvas::Setup(QWidget *widget, const QList<uint> &submeshes, int refresh_per_second)
{
    if (framework_->IsHeadless())
        return;

    SetWidget(widget);
    SetSubmeshes(submeshes);
    SetRefreshRate(refresh_per_second);
}

void EC_3DCanvas::SetWidget(QWidget *widget)
{
    if (framework_->IsHeadless())
        return;

    if (widget_ != widget)
    {
        widget_ = widget;
        if (widget_)
            connect(widget_, SIGNAL(destroyed(QObject*)), SLOT(WidgetDestroyed(QObject *)), Qt::UniqueConnection);
    }
}

void EC_3DCanvas::SetRefreshRate(int refresh_per_second)
{
    if (framework_->IsHeadless())
        return;

    if (refresh_per_second < 0)
        refresh_per_second = 0;

    bool was_running = false;
    if (refresh_timer_)
        was_running = refresh_timer_->isActive();
    SAFE_DELETE(refresh_timer_);

    if (refresh_per_second != 0)
    {
        refresh_timer_ = new QTimer(this);
        connect(refresh_timer_, SIGNAL(timeout()), SLOT(Update()), Qt::UniqueConnection);

        int temp_msec = 1000 / refresh_per_second;
        if (update_interval_msec_ != temp_msec && was_running)
            refresh_timer_->start(temp_msec);
        update_interval_msec_ = temp_msec;
    }
    else
        update_interval_msec_ = 0;
}

void EC_3DCanvas::SetSubmesh(uint submesh)
{
    if (framework_->IsHeadless())
        return;

    submeshes_.clear();
    submeshes_.append(submesh);
    update_internals_ = true;
}

void EC_3DCanvas::SetSubmeshes(const QList<uint> &submeshes)
{
    if (framework_->IsHeadless())
        return;

    submeshes_.clear();
    submeshes_ = submeshes;
    update_internals_ = true;
}

void EC_3DCanvas::WidgetDestroyed(QObject *obj)
{
    if (framework_->IsHeadless())
        return;

    widget_ = 0;
    Stop();
    RestoreOriginalMeshMaterials();
    SAFE_DELETE(refresh_timer_);
}

void EC_3DCanvas::Update()
{
    if (framework_->IsHeadless())
        return;

    if (!widget_.data() || texture_name_.empty())
        return;
    if (widget_->width() <= 0 || widget_->height() <= 0)
        return;

    try
    {
        Ogre::TexturePtr texture = Ogre::TextureManager::getSingleton().getByName(texture_name_);
        if (texture.isNull())
            return;

        if (buffer_.size() != widget_->size())
            buffer_ = QImage(widget_->size(), QImage::Format_ARGB32_Premultiplied);
        if (buffer_.width() <= 0 || buffer_.height() <= 0)
            return;

        QPainter painter(&buffer_);
        widget_->render(&painter);

        // Set texture to material
        if (update_internals_ && !material_name_.empty())
        {
            Ogre::MaterialPtr material = Ogre::MaterialManager::getSingleton().getByName(material_name_);
            if (material.isNull())
                return;
            OgreRenderer::SetTextureUnitOnMaterial(material, texture_name_);
            UpdateSubmeshes();
            update_internals_ = false;
        }

        if ((int)texture->getWidth() != buffer_.width() || (int)texture->getHeight() != buffer_.height())
        {
            texture->freeInternalResources();
            texture->setWidth(buffer_.width());
            texture->setHeight(buffer_.height());
            texture->createInternalResources();
        }

        if (!texture->getBuffer().isNull())
        {
            Ogre::Box update_box(0,0, buffer_.width(), buffer_.height());
            Ogre::PixelBox pixel_box(update_box, Ogre::PF_A8R8G8B8, (void*)buffer_.bits());
            texture->getBuffer()->blitFromMemory(pixel_box, update_box);
        }
    }
    catch (Ogre::Exception &e) // inherits std::exception
    {
        LogError("Exception occurred while blitting texture data from memory: " + std::string(e.what()));
    }
    catch (...)
    {
        LogError("Unknown exception occurred while blitting texture data from memory.");
    }
}

void EC_3DCanvas::UpdateSubmeshes()
{
    if (framework_->IsHeadless())
        return;

    Scene::Entity* entity = GetParentEntity();
    
    if (material_name_.empty() || !entity)
        return;

    int draw_type = -1;
    uint submesh_count = 0;
    EC_Mesh* ec_mesh = entity->GetComponent<EC_Mesh>().get();
    EC_OgreCustomObject* ec_custom_object = entity->GetComponent<EC_OgreCustomObject>().get();

    if (ec_mesh)
    {
        draw_type = RexTypes::DRAWTYPE_MESH;
        submesh_count = ec_mesh->GetNumMaterials();
    }
    else if (ec_custom_object)
    {
        draw_type = RexTypes::DRAWTYPE_PRIM;
        submesh_count = ec_custom_object->GetNumMaterials();
    }

    if (draw_type == -1)
        return;

    // Iterate trough sub meshes
    for (uint index = 0; index < submesh_count; ++index)
    {
        // Store original materials
        std::string submesh_material_name;
        if (draw_type == RexTypes::DRAWTYPE_MESH)
            submesh_material_name = ec_mesh->GetMaterialName(index);
        else if (draw_type == RexTypes::DRAWTYPE_PRIM)
            submesh_material_name = ec_custom_object->GetMaterialName(index);
        else
            return;

        if (submesh_material_name != material_name_)
            restore_materials_[index] = submesh_material_name;

        if (submeshes_.contains(index))
        {
            if (draw_type == RexTypes::DRAWTYPE_MESH)
                ec_mesh->SetMaterial(index, material_name_);
            else if (draw_type == RexTypes::DRAWTYPE_PRIM)
                ec_custom_object->SetMaterial(index, material_name_);
            else
                return;
        }
        else 
        {
            // If submesh not contained, restore the original material
            if (draw_type == RexTypes::DRAWTYPE_MESH)
            {
                if (ec_mesh->GetMaterialName(index) == material_name_)
                    if (restore_materials_.contains(index))
                        ec_mesh->SetMaterial(index, restore_materials_[index]);
            }
            else if(draw_type == RexTypes::DRAWTYPE_PRIM)
            {
                if (ec_custom_object->GetMaterialName(index) == material_name_)
                    if (restore_materials_.contains(index))
                        ec_custom_object->SetMaterial(index, restore_materials_[index]);
            }
            else 
                return;
        }
    }
}

void EC_3DCanvas::RestoreOriginalMeshMaterials()
{
    if (framework_->IsHeadless())
        return;

    if (restore_materials_.empty())
    {
        update_internals_ = true;
        return;
    }

    Scene::Entity* entity = GetParentEntity();

    if (material_name_.empty() || !entity)
        return;

    int draw_type = -1;
    uint submesh_count = 0;
    EC_Mesh* ec_mesh = entity->GetComponent<EC_Mesh>().get();
    EC_OgreCustomObject* ec_custom_object = entity->GetComponent<EC_OgreCustomObject>().get();

    if (ec_mesh)
    {
        draw_type = RexTypes::DRAWTYPE_MESH;
        submesh_count = ec_mesh->GetNumMaterials();
    }
    else if (ec_custom_object)
    {
        draw_type = RexTypes::DRAWTYPE_PRIM;
        submesh_count = ec_custom_object->GetNumMaterials();
    }

    if (draw_type == -1)
        return;

    // Iterate trough sub meshes
    for (uint index = 0; index < submesh_count; ++index)
    {
        // If submesh not contained, restore the original material
        if (draw_type == RexTypes::DRAWTYPE_MESH)
        {
            if (ec_mesh->GetMaterialName(index) == material_name_)
                if (restore_materials_.contains(index))
                    ec_mesh->SetMaterial(index, restore_materials_[index]);
        }
        else if(draw_type == RexTypes::DRAWTYPE_PRIM)
        {
            if (ec_custom_object->GetMaterialName(index) == material_name_)
                if (restore_materials_.contains(index))
                    ec_custom_object->SetMaterial(index, restore_materials_[index]);
        }
    }

    restore_materials_.clear();
    update_internals_ = true;
}

void EC_3DCanvas::ParentEntitySet()
{
    if (framework_->IsHeadless())
        return;

    if (GetParentEntity())
        connect(GetParentEntity(), SIGNAL(ComponentRemoved(IComponent*, AttributeChange::Type)), SLOT(ComponentRemoved(IComponent*, AttributeChange::Type)), Qt::UniqueConnection);
}

void EC_3DCanvas::ComponentRemoved(IComponent *component, AttributeChange::Type change)
{
    if (framework_->IsHeadless())
        return;

    if (component == this)
    {
        Stop();
        RestoreOriginalMeshMaterials();
        SetWidget(0);
        submeshes_.clear();
    }
}
