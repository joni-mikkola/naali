#ifndef incl_OpenAssetImport_h
#define incl_OpenAssetImport_h

#include <string>
#include <OgreMesh.h>
#include <OgreMeshSerializer.h>

#include <assimp/assimp.hpp>
#include <assimp/aiScene.h>
#include <assimp/aiPostProcess.h>
#include <map>
#include <QString>
//#include "GOOFSharedFrameworkData.h"

#define NUMELEMS(x) (sizeof(x)/sizeof(x[0]))

//TODO: only need a bool ?
struct boneNode
{
    aiNode* node;
    aiNode* parent;
    bool isNeeded;
};

class OpenAssetImport //: public GOOF::SharedFrameworkData
{
public:
    enum LoaderParams
    {
        LP_GENERATE_SINGLE_MESH = 1<<0,

        // See the two possible methods for material gneration
        LP_GENERATE_MATERIALS_AS_CODE = 1<<1,

        // 3ds max exports the animation over a longer time frame than the animation actually plays for
        // this is a fix for that
        LP_CUT_ANIMATION_WHERE_NO_FURTHER_CHANGE = 1<<2,

        // when 3ds max exports as DAE it gets some of the transforms wrong, get around this by using
        // this option and a prior run with of the model exported as ASE
        LP_USE_LAST_RUN_NODE_DERIVED_TRANSFORMS = 1<<3
    };


    // customAnimationName is only applied if the skeleton only has one animation

    bool convert(const Ogre::String& filename, bool generateMaterials, QString addr = "");

    Ogre::MeshPtr mMesh;
    std::map<QString, QString> matList;
    std::vector<QString> matNameList;
    const Ogre::String& getBasename(){ return mBasename; }
    bool IsSupportedExtension(QString extension);


private:
    QString addr;
    bool generateMaterials;
    void linearScaleMesh(Ogre::MeshPtr mesh, int targetSize);
    bool createSubMesh(const Ogre::String& name, int index, const aiNode* pNode, const aiMesh *mesh, const aiMaterial* mat, Ogre::MeshPtr pMesh, Ogre::AxisAlignedBox& mAAB, const Ogre::String& mDir);
    Ogre::MaterialPtr createMaterial(int index, const aiMaterial* mat, const Ogre::String& mDir);
    Ogre::MaterialPtr createMaterialByScript(int index, const aiMaterial* mat);
    void grabNodeNamesFromNode(const aiScene* mScene,  const aiNode* pNode);
    void grabBoneNamesFromNode(const aiScene* mScene,  const aiNode* pNode);
    void computeNodesDerivedTransform(const aiScene* mScene,  const aiNode *pNode, const aiMatrix4x4 accTransform);
    void createBonesFromNode(const aiScene* mScene,  const aiNode* pNode);
    void createBoneHiearchy(const aiScene* mScene,  const aiNode *pNode);
    void loadDataFromNode(const aiScene* mScene,  const aiNode *pNode, const Ogre::String& mDir);
    void markAllChildNodesAsNeeded(const aiNode *pNode);
    void flagNodeAsNeeded(const char* name);
    bool isNodeNeeded(const char* name);
    void parseAnimation (const aiScene* mScene, int index, aiAnimation* anim);
    typedef std::map<Ogre::String, boneNode> boneMapType;
    boneMapType boneMap;
    //aiNode* mSkeletonRootNode;
    int mLoaderParams;
    Ogre::String mBasename;
    Ogre::String mPath;
    Ogre::String mMaterialCode;
    Ogre::String mCustomAnimationName;

    typedef std::map<Ogre::String, const aiNode*> BoneNodeMap;
    BoneNodeMap mBoneNodesByName;

    typedef std::map<Ogre::String, const aiBone*> BoneMap;
    BoneMap mBonesByName;

    typedef std::map<Ogre::String, aiMatrix4x4> NodeTransformMap;
    NodeTransformMap mNodeDerivedTransformByName;

    typedef std::vector<Ogre::MeshPtr> MeshVector;
    MeshVector mMeshes;

    Ogre::SkeletonPtr mSkeleton;

    static int msBoneCount;
};

#endif // __OpenAssetImport_h__
