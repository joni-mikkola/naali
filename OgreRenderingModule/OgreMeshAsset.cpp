#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "MemoryLeakCheck.h"
#include "OgreMeshAsset.h"
#include "OgreConversionUtils.h"
#include "OgreRenderingModule.h"
#include "AssetAPI.h"
#include "OpenAssetImport.h"

#include <QFile>
#include <Ogre.h>

#include "LoggingFunctions.h"
DEFINE_POCO_LOGGING_FUNCTIONS("OgreMeshAsset")

OgreMeshAsset::~OgreMeshAsset()
{
    Unload();
}

bool OgreMeshAsset::DeserializeFromData(const u8 *data_, size_t numBytes)
{
    PROFILE(OgreMeshAsset_LoadFromFileInMemory);
    assert(data_);
    if (!data_)
        return false;
    
    /// Force an unload of this data first.
    Unload();



    bool colladaFile;
    OpenAssetImport meshLoader;
    int param = OpenAssetImport::LP_GENERATE_SINGLE_MESH;

    // if returns false then assume data points to ogre mesh
    colladaFile = meshLoader.convert(data_, numBytes, param, true);

    if (ogreMesh.isNull())
    {
        ogreMesh = Ogre::MeshManager::getSingleton().createManual(
            OgreRenderer::SanitateAssetIdForOgre(this->Name().toStdString()), Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
        if (ogreMesh.isNull())
        {
            LogError("Failed to create mesh " + Name().toStdString());
            return false;
        }
        ogreMesh->setAutoBuildEdgeLists(false);
    }

    std::vector<u8> tempData(data_, data_ + numBytes);
    Ogre::MeshSerializer serializer;

    if (colladaFile)
    {
        QString tempFilename = assetAPI->GenerateTemporaryNonexistingAssetFilename(Name());
        serializer.exportMesh(meshLoader.mMesh.get(), tempFilename.toStdString());
        LoadFileToVector(tempFilename.toStdString().c_str(), tempData);
        QFile::remove(tempFilename); // Delete the temporary file we used for serialization.
        numBytes = tempData.size();
    }

#include "DisableMemoryLeakCheck.h"
    Ogre::DataStreamPtr stream(new Ogre::MemoryDataStream((void*)&tempData[0], numBytes, false));
#include "EnableMemoryLeakCheck.h"
    serializer.importMesh(stream, ogreMesh.getPointer()); // Note: importMesh *adds* submeshes to an existing mesh. It doesn't replace old ones.

    // Generate tangents to mesh
    try
    {
        unsigned short src, dest;
        ///\bug Crashes if called for a mesh that has null or zero vertices in the vertex buffer, or null or zero indices in the index buffer.
        if (!ogreMesh->suggestTangentVectorBuildParams(Ogre::VES_TANGENT, src, dest))
            ogreMesh->buildTangentVectors(Ogre::VES_TANGENT, src, dest);
    }
    catch (...) {}
    
    
    // Generate extremity points to submeshes, 1 should be enough
    /*try
    {
        for(uint i = 0; i < ogreMesh->getNumSubMeshes(); ++i)
        {
            Ogre::SubMesh *smesh = ogreMesh->getSubMesh(i);
            if (smesh)
                smesh->generateExtremes(1);
        }
    }
    catch (...) {}*/
        
    try
    {
        // Assign default materials that won't complain
        SetDefaultMaterial();
        // Set asset references the mesh has
        //ResetReferences();
    }
    catch (Ogre::Exception &e)
    {
        OgreRenderer::OgreRenderingModule::LogError("Failed to create mesh " + this->Name().toStdString() + ": " + std::string(e.what()));
        Unload();
        return false;
    }

    //internal_name_ = SanitateAssetIdForOgre(id_);
    
    LogDebug("Ogre mesh " + this->Name().toStdString() + " created");
    return true;
}

void OgreMeshAsset::HandleLoadError(const QString &loadError)
{
    LogDebug(loadError.toStdString());
}

void OgreMeshAsset::DoUnload()
{
    if (ogreMesh.isNull())
        return;

    std::string meshName = ogreMesh->getName();
    ogreMesh.setNull();
    try
    {
        Ogre::MeshManager::getSingleton().remove(meshName);
    }
    catch (...) {}
}

void OgreMeshAsset::SetDefaultMaterial()
{
    if (ogreMesh.isNull())
        return;

//    originalMaterials.clear();
    for (uint i = 0; i < ogreMesh->getNumSubMeshes(); ++i)
    {
        Ogre::SubMesh *submesh = ogreMesh->getSubMesh(i);
        if (submesh)
        {
//            originalMaterials.push_back(submesh->getMaterialName().c_str());
            submesh->setMaterialName("LitTextured");
        }
    }
}

bool OgreMeshAsset::IsLoaded() const
{
    return ogreMesh.get() != 0;
}

bool OgreMeshAsset::SerializeTo(std::vector<u8> &data, const QString &serializationParameters) const
{
    if (ogreMesh.isNull())
    {
        OgreRenderer::OgreRenderingModule::LogWarning("Tried to export non-existing Ogre mesh " + Name().toStdString() + ".");
        return false;
    }
    try
    {
        Ogre::MeshSerializer serializer;
        QString tempFilename = assetAPI->GenerateTemporaryNonexistingAssetFilename(Name());
        // Ogre has a limitation of not supporting serialization to a file in memory, so serialize directly to a temporary file,
        // load it up and delete the temporary file.
        serializer.exportMesh(ogreMesh.get(), tempFilename.toStdString());
        bool success = LoadFileToVector(tempFilename.toStdString().c_str(), data);
        QFile::remove(tempFilename); // Delete the temporary file we used for serialization.
        if (!success)
            return false;
    } catch (std::exception &e)
    {
        OgreRenderer::OgreRenderingModule::LogError("Failed to export Ogre mesh " + Name().toStdString() + ":");
        if (e.what())
            OgreRenderer::OgreRenderingModule::LogError(e.what());
        return false;
    }
    return true;
}
