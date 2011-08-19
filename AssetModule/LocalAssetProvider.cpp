// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "OgreTextureManager.h"
#include "DebugOperatorNew.h"
#include "MemoryLeakCheck.h"
#include "AssetModule.h"
#include "CoreTypes.h"
#include "LocalAssetProvider.h"
#include "Framework.h"
#include "EventManager.h"
#include "ServiceManager.h"
#include "ConfigurationManager.h"
#include "LocalAssetStorage.h"
#include "IAssetUploadTransfer.h"
#include "IAssetTransfer.h"
#include "AssetAPI.h"
#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>

namespace Asset
{

LocalAssetProvider::LocalAssetProvider(Foundation::Framework* framework_)
:framework(framework_)
{
}

LocalAssetProvider::~LocalAssetProvider()
{
}

bool LoadFileFromOgreResource(QString &ref, std::vector<u8> &dst)
{
    Ogre::TextureManager & manager = Ogre::TextureManager::getSingleton();
    manager.load(ref.toStdString().c_str(), Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
    Ogre::TexturePtr tex = (Ogre::TexturePtr)manager.getByName(ref.toStdString().c_str());

    if (!tex.isNull())
    {
        Ogre::Image img;
        tex->convertToImage(img);
        Ogre::DataStreamPtr konv = img.encode("png");
        std::string test = konv->getAsString();

        for (int i = 0; i < test.length(); i++)
            dst.push_back(test[i]);

        konv->close();
        return true;
    }
    else
        return false;
}

QString LocalAssetProvider::Name()
{
    static const QString name("Local");
    
    return name;
}

bool LocalAssetProvider::IsValidRef(QString asset_id, QString asset_type)
{
    QString ref = asset_id.toStdString().c_str();
    ref = ref.trimmed();
    if (ref.length() == 0)
        return false;
    if (ref.startsWith("file://") || ref.startsWith("local://"))
        return true;

    if (!ref.contains("://")) // If the ref doesn't contain a protocol specifier (we do this simple check for it), try to directly find the given local file.
    {
        QString path = GetPathForAsset(ref, 0);
        return path.length() > 0;
    }
    else
        return false;
}

AssetTransferPtr LocalAssetProvider::RequestAsset(QString assetRef, QString assetType)
{
    if (assetRef.isEmpty())
        return AssetTransferPtr();

    AssetTransferPtr transfer = AssetTransferPtr(new IAssetTransfer);
    transfer->source.ref = assetRef.trimmed();
    transfer->assetType = assetType;

    pendingDownloads.push_back(transfer);

    return transfer;
}

QString LocalAssetProvider::GetPathForAsset(const QString &assetname, LocalAssetStoragePtr *storage)
{
    // Check first all subdirs without recursion, because recursion is potentially slow
    for(size_t i = 0; i < storages.size(); ++i)
    {
        QString path = storages[i]->GetFullPathForAsset(assetname.toStdString().c_str(), false);
        if (path != "")
        {
            if (storage)
                *storage = storages[i];
            return path;
        }
    }

    for(size_t i = 0; i < storages.size(); ++i)
    {
        QString path = storages[i]->GetFullPathForAsset(assetname.toStdString().c_str(), true);
        if (path != "")
        {
            if (storage)
                *storage = storages[i];
            return path;
        }
    }
    
    if (storage)
        *storage = LocalAssetStoragePtr();
    return "";
}

void LocalAssetProvider::Update(f64 frametime)
{
    ///\note It is *very* important that below we first complete all uploads, and then the downloads.
    /// This is because it is a rather common code flow to upload an asset for an entity, and immediately after that
    /// generate a entity in the scene that refers to that asset, which means we do both an upload and a download of the
    /// asset into the same asset storage. If the download request was processed before the upload request, the download
    /// request would fail on missing file, and the entity would erroneously get an "asset not found" result.
    CompletePendingFileUploads();
    CompletePendingFileDownloads();
}

void LocalAssetProvider::DeleteAssetFromStorage(QString assetRef)
{
    if (!assetRef.isEmpty())
        QFile::remove(assetRef); ///\todo Check here that the assetRef points to one of the accepted storage directories, and don't allow deleting anything else.

    AssetModule::LogInfo("LocalAssetProvider::DeleteAssetFromStorage: Deleted asset file \"" + assetRef.toStdString() + "\" from disk.");
}

AssetStoragePtr LocalAssetProvider::AddStorage(const QString &location, const QString &name)
{
    QString ref = location;

    // Strip file: trims asset provider id (f.ex. 'file://') and potential mesh name inside the file (everything after last slash)
    if (ref.startsWith("file://"))
        ref = ref.mid(7);
    if (ref.startsWith("local://"))
        ref = ref.mid(8);

    int lastSlash = ref.lastIndexOf('/');
    if (lastSlash != -1)
        ref = ref.left(lastSlash);

    return AddStorageDirectory(ref.toStdString(), name.toStdString(), false);
}

AssetStoragePtr LocalAssetProvider::AddStorageDirectory(const std::string &directory, const std::string &storageName, bool recursive)
{
    ///\todo Check first if the given directory exists as a storage, and don't add it as a duplicate if so.

    LocalAssetStoragePtr storage = LocalAssetStoragePtr(new LocalAssetStorage());
    storage->directory = directory.c_str();
    storage->name = storageName.c_str();
    storage->recursive = recursive;
    storage->provider = shared_from_this();
    storage->SetupWatcher(); // Start listening on file change notifications.
//    connect(storage->changeWatcher, SIGNAL(directoryChanged(QString)), this, SLOT(FileChanged(QString)));
//    connect(storage->changeWatcher, SIGNAL(fileChanged(QString)), this, SLOT(FileChanged(QString)));

    storages.push_back(storage);
    return storage;
}

std::vector<AssetStoragePtr> LocalAssetProvider::GetStorages() const
{
    std::vector<AssetStoragePtr> stores;
    for(size_t i = 0; i < storages.size(); ++i)
        stores.push_back(storages[i]);
    return stores;
}

AssetUploadTransferPtr LocalAssetProvider::UploadAssetFromFileInMemory(const u8 *data, size_t numBytes, AssetStoragePtr destination, const char *assetName)
{
    assert(data);
    if (!data)
    {
        AssetModule::LogError("LocalAssetProvider::UploadAssetFromFileInMemory: Null source data pointer passed to function!");
        return AssetUploadTransferPtr();
    }

    LocalAssetStorage *storage = dynamic_cast<LocalAssetStorage*>(destination.get());
    if (!storage)
    {
        AssetModule::LogError("LocalAssetProvider::UploadAssetFromFileInMemory: Invalid destination asset storage type! Was not of type LocalAssetStorage!");
        return AssetUploadTransferPtr();
    }

    AssetUploadTransferPtr transfer = AssetUploadTransferPtr(new IAssetUploadTransfer());
    transfer->sourceFilename = "";
    transfer->destinationName = assetName;
    transfer->destinationStorage = destination;
    transfer->assetData.insert(transfer->assetData.end(), data, data + numBytes);

    pendingUploads.push_back(transfer);

    return transfer;
}

void LocalAssetProvider::CompletePendingFileDownloads()
{
    while(pendingDownloads.size() > 0)
    {
        AssetTransferPtr transfer = pendingDownloads.back();
        pendingDownloads.pop_back();

        QString ref = transfer->source.ref;

        // Strip file: trims asset provider id (f.ex. 'file://') and potential mesh name inside the file (everything after last slash)
        if (ref.startsWith("file://"))
        {
            ref = ref.mid(7);
            int lastSlash = ref.lastIndexOf('/');
            if (lastSlash != -1)
               ref = ref.left(lastSlash);
        }

        // Don't cut string when using local://-providers
        if (ref.startsWith("local://"))
            ref = ref.mid(8);

        LocalAssetStoragePtr storage;
        QString path = GetPathForAsset(ref, &storage);

        // Check is reference is found from Ogre::ResourceManager
        Ogre::TextureManager & manager = Ogre::TextureManager::getSingleton();
        bool exist = manager.resourceExists(ref.toStdString().c_str());

        if (path.isEmpty() && !IsAssimpMaterial(ref) && !exist)
        {
            QString reason = "Failed to find local asset with filename \"" + ref + "\"!";
//            AssetModule::LogWarning(reason.toStdString());
            framework->Asset()->AssetTransferFailed(transfer.get(), reason);
            continue;
        }
    
        QFileInfo file(GuaranteeTrailingSlash(path) + ref);
        QString absoluteFilename = file.absoluteFilePath();

        bool success;

        if (IsAssimpMaterial(ref))
        {
#ifdef ASSIMP_ENABLED
            success = LoadMaterialInfo(ref, transfer->rawAssetData, framework->Asset()->materialMap);
#endif
        }
        else
            success = LoadFileToVector(absoluteFilename.toStdString().c_str(), transfer->rawAssetData);

        // Try to load data from Ogre ResourceManager
        if (!success && exist)
            success = LoadFileFromOgreResource(ref, transfer->rawAssetData);

        if (!success)
        {
            QString reason = "Failed to read asset data for asset \"" + ref + "\" from file \"" + absoluteFilename + "\"";
//            AssetModule::LogError(reason.toStdString())
            framework->Asset()->AssetTransferFailed(transfer.get(), reason);
            continue;
        }

        
        // Tell the Asset API that this asset should not be cached into the asset cache, and instead the original filename should be used
        // as a disk source, rather than generating a cache file for it.
        transfer->SetCachingBehavior(false, absoluteFilename.toStdString().c_str());

        transfer->storage = storage;
//        AssetModule::LogDebug("Downloaded asset \"" + ref.toStdString() + "\" from file " + absoluteFilename.toStdString());

        // Signal the Asset API that this asset is now successfully downloaded.
        framework->Asset()->AssetTransferCompleted(transfer.get());
    }
}

void LocalAssetProvider::CompletePendingFileUploads()
{
    while(pendingUploads.size() > 0)
    {
        AssetUploadTransferPtr transfer = pendingUploads.back();
        pendingUploads.pop_back();

        LocalAssetStoragePtr storage = boost::dynamic_pointer_cast<LocalAssetStorage>(transfer->destinationStorage.lock());
        if (!storage)
        {
            AssetModule::LogError("Invalid IAssetStorage specified for file upload in LocalAssetProvider!");
            transfer->EmitTransferFailed();
            continue;
        }

        if (transfer->sourceFilename.length() == 0 && transfer->assetData.size() == 0)
        {
            AssetModule::LogError("No source data present when trying to upload asset to LocalAssetProvider!");
            continue;
        }

        QString fromFile = transfer->sourceFilename;
        QString toFile = GuaranteeTrailingSlash(storage->directory) + transfer->destinationName;

        bool success;
        if (fromFile.length() == 0)
            success = SaveAssetFromMemoryToFile(&transfer->assetData[0], transfer->assetData.size(), toFile.toStdString().c_str());
        else
            success = CopyAssetFile(fromFile.toStdString().c_str(), toFile.toStdString().c_str());

        if (!success)
        {
            AssetModule::LogError(("Asset upload failed in LocalAssetProvider: CopyAsset from \"" + fromFile + "\" to \"" + toFile + "\" failed!").toStdString());
            transfer->EmitTransferFailed();
            /// \todo Jukka lis�� failure-notifikaatio.
        }
        else
        {
            framework->Asset()->AssetUploadTransferCompleted(transfer.get());
        }
    }
}

void LocalAssetProvider::FileChanged(const QString &path)
{
    AssetModule::LogInfo(("File " + path + " changed.").toStdString());
}

} // ~Asset
