// For conditions of distribution and use, see copyright notice in license.txt

// A note about the filename: This file will be renamed to AssetCache_.h at the moment the old AssetCache.h from AssetModule is deleted.

#ifndef incl_Asset_AssetCache_h
#define incl_Asset_AssetCache_h

#include <QString>
#include <QNetworkCookieJar>
#include <QNetworkDiskCache>
#include <QNetworkCacheMetaData>
#include <QByteArray>
#include <QHash>
#include <QUrl>
#include <QDir>
#include <QObject>

#include "CoreTypes.h"
#include "AssetFwd.h"

class CookieJar;

/// An utility function that takes an assetRef and makes a string out of it that can safely be used as a part of a filename.
/// Replaces characters / \ : * ? " ' < > | with _
QString SanitateAssetRefForCache(QString assetRef);

/// Subclassing QNetworkDiskCache has the main goal of separating metadata from the raw asset data. The basic implementation of QNetworkDiskCache
/// will store both in the same file. That did not work very well with our asset system as we need absolute paths to loaded assets for various purpouses.
class AssetCache : public QNetworkDiskCache
{

Q_OBJECT

public:
    explicit AssetCache(AssetAPI *owner, QString assetCacheDirectory);

    /// Allocates new QFile*, it is the callers responsibility to free the memory once done with it.
    /// \note QNetworkDiskCache override. Don't call directly, used by QNetworkAccessManager.
    virtual QIODevice* data(const QUrl &url);
    
    /// Frees allocated QFile* that was prepared in prepare().
    /// \note QNetworkDiskCache override. Don't call directly, used by QNetworkAccessManager.
    virtual void insert(QIODevice* device);

    /// Allocates new QFile*, the data is freed in either insert() or remove(), 
    /// remove() cancels the preparation and insert() finishes it.
    /// \note QNetworkDiskCache override. Don't call directly, used by QNetworkAccessManager.
    virtual QIODevice* prepare(const QNetworkCacheMetaData &metaData);
    
    /// Frees allocated QFile* if one was prepared in prepare().
    /// \note QNetworkDiskCache override. Don't call directly, used by QNetworkAccessManager.
    virtual bool remove(const QUrl &url);

    /// Reads metadata file from hard drive.
    /// \note QNetworkDiskCache override. Don't call directly, used by QNetworkAccessManager.
    virtual QNetworkCacheMetaData metaData(const QUrl &url);

    /// Writes metadata file to hard drive, if metadata is different from known cache metadata.
    /// \note QNetworkDiskCache override. Don't call directly, used by QNetworkAccessManager.
    virtual void updateMetaData(const QNetworkCacheMetaData &metaData);

    /// Deletes all data and metadata files from the asset cache.
    /// \note QNetworkDiskCache override. Don't call directly, used by QNetworkAccessManager.
    virtual void clear();

    /// Checks if asset cache is currently over the maximum limit.
    /// This call is ignored untill we decide to limit the disk cache size.
    /// \note QNetworkDiskCache override. Don't call directly, used by QNetworkAccessManager.
    virtual qint64 expire();

public slots:
    /// Returns an absolute path to a disk source of the url.
    /// @param QString asset ref
    /// @return QString absolute path to the assets disk source. Return empty string if asset is not in the cache.
    /// @note this will always return an empty string for http/https assets. This will force the AssetAPI to check that it has the latest asset from the source.
    QString GetDiskSource(const QString &assetRef);

    /// Returns an absolute path to a disk source of the url.
    /// @param QUrl url of the asset ref
    /// @return QString absolute path to the assets disk source. Return empty string if asset is not in the cache.
    /// @note this will return you the disk source for http/https assets unlike the QString overload.
    QString GetDiskSource(const QUrl &assetUrl);

    /// Checks whether the asset cache contains an asset with the given content hash, and returns the absolute path name to it, if so.
    /// Otherwise returns an empty string.
    /// @todo Implement.
    QString GetDiskSourceByContentHash(const QString &contentHash);

    /// Get the cache directory. Returned path is guaranteed to have a trailing slash /.
    /// @return QString absolute path to the caches data directory
    QString GetCacheDirectory() const;

    /// Saves the given asset to cache.
    /// @return QString the absolute path name to the asset cache entry. If not successful returns an empty string.
    QString StoreAsset(AssetPtr asset);

    /// Saves the specified data to the asset cache.
    /// @return QString the absolute path name to the asset cache entry. If not successful returns an empty string.
    QString StoreAsset(const u8 *data, size_t numBytes, const QString &assetName, const QString &assetContentHash);

    /// Deletes the asset with the given assetRef from the cache, if it exists.
    /// @param QString asset reference.
    void DeleteAsset(const QString &assetRef);

    /// Deletes the asset with the given assetRef from the cache, if it exists.
    /// @param QUrl asset reference url.
    void DeleteAsset(const QUrl &assetUrl);

    /// Deletes all data and metadata files from the asset cache.
    /// Will not clear subfolders in the cache folders, or remove any folders.
    void ClearAssetCache();

    /// Creates a new cookie jar that implements disk writing and reading. Can be used with any QNetworkAccessManager with setCookieJar() function.
    /// \note AssetCache will be the CookieJars parent and it will destroyed by it, don't take ownerwhip of the returned CookieJar.
    /// \param QString File path to the file the jar will read/write cookies to/from.
    CookieJar *NewCookieJar(const QString &cookieDiskFile);

private slots:
    /// Writes metadata into a file. Helper function for the QNetworkDiskCache overrides.
    bool WriteMetadata(const QString &filePath, const QNetworkCacheMetaData &metaData);

    /// If the metadata for this cache item has Content-MD5 digest, compare them with the data file we are about to pass onwards.
    /// \note If the metadata does not contain the header Content-MD5 this function will do nothing and return true. All http servers don't send this header eg. disabled by default on apache.
    /// \note In the case that corruption is detected this function will remove the data and metadata files from disk and return false. This will trigger a full fetch for the asset data.
    /// \param QString Absolute path to data file. You should validate that the file actually exists.
    /// \param QNetworkCacheMetaData The assets metadata.
    /// \return False if digest did not match with data file, true otherwise.
    bool VerifyCacheContentDigest(const QString &absoluteDataFilePath, const QNetworkCacheMetaData &metaData);

    /// Genrates the absolute path to an asset cache entry. Helper function for the QNetworkDiskCache overrides.
    QString GetAbsoluteFilePath(bool isMetaData, const QUrl &url);

    /// Genrates the absolute path to an data asset cache entry.
    QString GetAbsoluteDataFilePath(const QString &filename);

    /// Removes all files from a directory. Will not delete the folder itself or any subfolders it has.
    void ClearDirectory(const QString &absoluteDirPath);

private:
    /// Cache directory, passed here from AssetAPI in the ctor.
    QString cacheDirectory;

    /// AssetAPI ptr.
    AssetAPI *assetAPI;

    /// Asset data dir.
    QDir assetDataDir;

    /// Asset metadata dir.
    QDir assetMetaDataDir;

    /// Internal tracking of prepared QUrl to QIODevice pairs.
    QHash<QString, QFile*> preparedItems;
};

/*! CookieJar is a subclass of QNetworkCookieJar. This is the only way to do disk storage for cookies with Qt.
    Part reason why this is declared here is because JavaScript cannot do proper inheritance of QNetworkCookieJar to access the protected functions.
    
    \todo If you feel that there is a more appropriate place for this, please move the class but be sure to check depending 3rd party code.
    \note You can ask AssetCache::NewCookiJar to make you a new CookieJar.
*/
class CookieJar : public QNetworkCookieJar
{

Q_OBJECT

public:
    CookieJar(QObject *parent, const QString &cookieDiskFile = QString());
    ~CookieJar();

public slots:
    void SetDataFile(const QString &cookieDiskFile);
    void ClearCookies();
    void ReadCookies();
    void StoreCookies();

private:
    QString cookieDiskFile_;

};

#endif
