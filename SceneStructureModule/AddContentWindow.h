/**
 *  For conditions of distribution and use, see copyright notice in license.txt
 *
 *  @file   AddContentWindow.h
 *  @brief  Window for adding new content and assets.
 */

#ifndef incl_SceneStructureModule_AddContentWindow_h
#define incl_SceneStructureModule_AddContentWindow_h

#include <QWidget>

#include "ForwardDefines.h"
#include "SceneFwd.h"
#include "AssetFwd.h"
#include "SceneDesc.h"
#include "Vector3D.h"

class QTreeWidget;
class QPushButton;
class QComboBox;
class QTreeWidgetItem;
class QProgressBar;
class QLabel;

class IAssetUploadTransfer;

/// Window for adding new content and assets.
/** The window is modal and is deleted when it's closed.
*/
class AddContentWindow : public QWidget
{
    Q_OBJECT

public:
    /// Constructs the window.
    /** @param fw Framework.
        @param dest Destination scene.
        @param parent Parent widget.
    */
    explicit AddContentWindow(Foundation::Framework *fw, const Scene::ScenePtr &dest, QWidget *parent = 0);

    /// Destructor.
    ~AddContentWindow();

public slots:
    /// Adds scene description to be shown in the window.
    /** @param desc Scene description.
    */
    void AddDescription(const SceneDesc &desc);

    /// Adds multiple scene descriptions to be shown in the window.
    /** @param desc Scene description.
    */
    //void AddDescriptions(const QList<SceneDesc> &descs);

    /// Adds files to be shown in the window.
    /** @param fileNames List of files.
    */
    void AddFiles(const QStringList &fileNames);

    /// Add offset position which will be applied to the created entities.
    /** @param pos Position.
    */
    void AddPosition(const Vector3df &pos) { position = pos; }

    /// Silently without showing ui commit everything as is and close the dialog. 
    /// This will not show anything to user and will do everything with default settings.
    /// Especially handy when you know there are no assets to upload (everything has web refs),
    /// and you just want the entities to be added to the scene silently eg. on a drag and drop event.
    void CommitEverythingAndClose();

    /// Returns if current dialog content has uploads that need processing.
    /** @return bool True if there are assets that are marked for download, false other wise.
    */
    bool HasAssetUploads() const;
    
signals:
    void Completed(bool contentAdded, const QString &uploadBaseUrl);

protected:
    void showEvent(QShowEvent *e);

private:
    Q_DISABLE_COPY(AddContentWindow)

    /// Creates entity items to the entity tree widget.
    /** @param entityDescs List of entity descriptions.
    */
    void AddEntities(const QList<EntityDesc> &entityDescs);

    /// Creates asset items to the asset tree widget.
    /** @param assetDescs List of assets descriptions.
    */
    void AddAssets(const SceneDesc::AssetMap &assetDescs);

    /// Rewrites values of AssetReference or AssetReferenceList attributes.
    /** @param sceneDesc Scene description.
        @param dest Destination asset storage.
        @param useDefaultStorage Do we want to use the default asset storage.
    */
    void RewriteAssetReferences(SceneDesc &sceneDesc, const AssetStoragePtr &dest, bool useDefaultStorage);

    /// Returns name of the currently selected asset storage.
    QString GetCurrentStorageName() const;

    /// Generates contents of asset storage combo box. Sets default storage selected as default.
    void GenerateStorageComboBoxContents();

    QTreeWidget *entityTreeWidget; ///< Tree widget showing entities.
    QTreeWidget *assetTreeWidget; ///< Tree widget showing asset references.
    Foundation::Framework *framework; ///< Framework.
    Scene::SceneWeakPtr scene; ///< Destination scene.
    SceneDesc sceneDesc; ///< Current scene description shown on the window.
    QPushButton *addContentButton; ///< Add content button.
    QPushButton *cancelButton; ///< Cancel/close button.
    QComboBox *storageComboBox; ///< Asset storage combo box.
    Vector3df position; ///< Centralization position for instantiated context (if used).

    // Uploading
    QLabel *uploadStatus_;
    QProgressBar *uploadProgress_;
    int progressStep_;
    int failedUploads_;
    int successfullUploads_;
    int totalUploads_;

    // Entities add
    QLabel *entitiesStatus_;
    QProgressBar *entitiesProgress_;

    // Parent widget
    QWidget *parentEntities_;
    QWidget *parentAssets_;

    // Selected entities and assets
    SceneDesc newDesc_;

    // Bool for completion
    bool contentAdded_;

    // Bool if we are in silent commit mode, see CommitEverythingAndClose().
    bool silentCommit_;

private slots:
    /// Checks all entity check boxes.
    void SelectAllEntities();

    /// Unchecks all entity check boxes.
    void DeselectAllEntities();

    /// Checks all asset check boxes.
    void SelectAllAssets();

    /// Unchecks all asset check boxes.
    void DeselectAllAssets();

    /// Start content creation and asset uploading.
    void AddContent();

    /// Create new scene description with ui check box selections
    bool CreateNewDesctiption();

    /// Start uploading
    bool UploadAssets();

    /// Add entities to scene (automatically called when all transfers finish)
    void AddEntities();

    /// Centers this window to the app main window
    void CenterToMainWindow();

    /// Set entity related widgets visibility.
    void SetEntitiesVisible(bool visible);

    /// Set assets related widgets visibility.
    void SetAssetsVisible(bool visible);

    /// Closes the window.
    void Close();

    /// Checks if tree widget column is editable.
    /** @param item Item which was double-clicked.
        @param column Column index.
    */
    void CheckIfColumnIsEditable(QTreeWidgetItem *item, int column);

    /// Rewrites the destination names of all assets in the UI accordingly to the selected asset storage.
    void RewriteDestinationNames();

    /// Handles completed upload asset transfer.
    /** @param transfer Completed transfer.
    */
    void HandleUploadCompleted(IAssetUploadTransfer *transfer);

    /// Handles failed upload asset transfer.
    /** @param transfer Failed transfer.
    */
    void HandleUploadFailed(IAssetUploadTransfer *trasnfer);

    void UpdateUploadStatus(bool successful, const QString &assetRef);

    void CheckUploadTotals();
};

#endif
