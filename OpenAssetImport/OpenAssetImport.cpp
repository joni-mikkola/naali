#include "OpenAssetImport.h"
#include "assimp/DefaultLogger.h"
#include "OgreDataStream.h"
#include "OgreImage.h"
#include "OgreTexture.h"
#include "OgreTextureManager.h"
#include "OgreMaterial.h"
#include "OgreMaterialManager.h"
#include "OgreLog.h"
#include "OgreLogManager.h"
#include "OgreHardwareBuffer.h"
#include "OgreMesh.h"
#include "OgreSubMesh.h"
#include "OgreMatrix4.h"
#include "OgreDefaultHardwareBufferManager.h"
#include "OgreMeshManager.h"
#include "OgreSceneManager.h"
#include <OgreStringConverter.h>
#include <OgreSkeletonManager.h>
#include "OgreMeshSerializer.h"
#include "OgreSkeletonSerializer.h"
#include "OgreAnimation.h"
#include "OgreAnimationTrack.h"
#include "OgreKeyFrame.h"
#include <QString>
#include <boost/tuple/tuple.hpp>
//#include "OgreXMLSkeletonSerializer.h"

 static int meshNum = 0;

Ogre::String toString(const aiColor4D& colour)
{
    return	Ogre::StringConverter::toString(colour.r) + " " +
            Ogre::StringConverter::toString(colour.g) + " " +
            Ogre::StringConverter::toString(colour.b) + " " +
            Ogre::StringConverter::toString(colour.a);
}

int OpenAssetImport::msBoneCount = 0;

void fixMaterialReference(std::string &matRef, std::string addRef)
{
    addRef.replace(0, 7, "http://");

    for (uint i = 0; i < addRef.length(); ++i)
        if (addRef[i] == '_') addRef[i] = '/';

    size_t tmp = addRef.find_last_of('/')+1;
    addRef.erase(tmp, addRef.length() - tmp);

    size_t indx = matRef.find("texture ", 0);
    matRef.replace(indx+8, 0, addRef);
}

OpenAssetImport::OpenAssetImport()
{
    //mSkeletonRootNode = NULL;
}

OpenAssetImport::~OpenAssetImport()
{
}



bool OpenAssetImport::convert(unsigned char * fileData, size_t numBytes, int loaderParams, bool noMat, std::string addr)
{
    meshNum = 0;
    //dummyMatCount = 0;
    mLoaderParams = loaderParams;

    if (mLoaderParams & LP_USE_LAST_RUN_NODE_DERIVED_TRANSFORMS == false)
    {
        mNodeDerivedTransformByName.clear();
    }

    Assimp::DefaultLogger::create("asslogger.log",Assimp::Logger::VERBOSE);
    Assimp::DefaultLogger::get()->info("Logging asses");

    Ogre::LogManager::getSingleton().logMessage("*** Loading ass file... ***");
    //Ogre::LogManager::getSingleton().logMessage("Filename " + filename);

    const aiScene *scene;

    Assimp::Importer importer;

    //scene = importer.ReadFile( filename.c_str(), aiProcessPreset_TargetRealtime_Quality | aiProcess_TransformUVCoords | aiProcess_FlipUVs);

    // by default remove points and lines from the model, since these are usually
    // degenerate structures from bad modelling or bad import/export.  if they
    // are needed it can be turned on with IncludeLinesPoints
    int removeSortFlags = importer.GetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE);
    dontGenMaterials = noMat;
    removeSortFlags |= aiPrimitiveType_POINT | aiPrimitiveType_LINE;
    Ogre::LogManager::getSingleton().getDefaultLog()->setLogDetail(Ogre::LL_NORMAL);

    // And have it read the given file with some example postprocessing
    // Usually - if speed is not the most important aspect for you - you'll
    // propably to request more postprocessing than we do in this example.
    //Ogre::LogManager::getSingleton().getDefaultLog()->setLogDetail(Ogre::LL_NORMAL);
    scene = importer.ReadFileFromMemory(fileData, numBytes, 0
                                        |aiProcess_SplitLargeMeshes
                                        //aiProcess_MakeLeftHanded
                                        | aiProcess_Triangulate
                                        | aiProcess_FlipUVs
                                        | aiProcess_FindInvalidData
                                        //  | aiProcess_FlipWindingOrder
                                        | aiProcess_FixInfacingNormals
                                        //| aiProcess_TransformUVCoords
                                        // | aiProcess_GenUVCoords
                                        | aiProcess_JoinIdenticalVertices
                                        | aiProcess_PreTransformVertices
                                        | aiProcess_OptimizeMeshes
                                        | aiProcess_RemoveRedundantMaterials
                                        | aiProcess_ImproveCacheLocality
                                        | aiProcess_LimitBoneWeights
                                        //| aiProcess_FindDegenerates
                                        //| aiProcess_SortByPType
                                        );



    // If the import failed, report it
    if( !scene)
    {
        Ogre::LogManager::getSingleton().logMessage("AssImp importer failed with the following message:");
        Ogre::LogManager::getSingleton().logMessage(importer.GetErrorString() );
        return false;
    }

    grabNodeNamesFromNode(scene, scene->mRootNode);
    grabBoneNamesFromNode(scene, scene->mRootNode);

    computeNodesDerivedTransform(scene, scene->mRootNode, scene->mRootNode->mTransformation);

    if(mBonesByName.size())
    {
        mSkeleton = Ogre::SkeletonManager::getSingleton().create("conversion", Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);

        msBoneCount = 0;
        createBonesFromNode(scene, scene->mRootNode);
        msBoneCount = 0;
        createBoneHiearchy(scene, scene->mRootNode);

        if(scene->HasAnimations())
        {
            for(int i = 0; i < scene->mNumAnimations; ++i)
            {
                parseAnimation(scene, i, scene->mAnimations[i]);
            }
        }
    }

    loadDataFromNode(scene, scene->mRootNode, mPath);

    Ogre::LogManager::getSingleton().logMessage("*** Finished loading ass file ***");
    Assimp::DefaultLogger::kill();

    if(!mSkeleton.isNull())
    {
        unsigned short numBones = mSkeleton->getNumBones();
        unsigned short i;
        for (i = 0; i < numBones; ++i)
        {
            Ogre::Bone* pBone = mSkeleton->getBone(i);
            assert(pBone);
        }

        Ogre::SkeletonSerializer binSer;
        binSer.exportSkeleton(mSkeleton.getPointer(), mPath + mBasename + ".skeleton");
    }

    Ogre::MeshSerializer meshSer;
    for(MeshVector::iterator it = mMeshes.begin(); it != mMeshes.end(); ++it)
    {
        mMesh = *it;
        if(mBonesByName.size())
        {
            mMesh->setSkeletonName(mBasename + ".skeleton");
        }

        Ogre::Mesh::SubMeshIterator smIt = mMesh->getSubMeshIterator();
        while (smIt.hasMoreElements())
        {
            Ogre::SubMesh* sm = smIt.getNext();
            if (!sm->useSharedVertices)
            {
                // Automatic
                Ogre::VertexDeclaration* newDcl =
                        sm->vertexData->vertexDeclaration->getAutoOrganisedDeclaration(mMesh->hasSkeleton(), mMesh->hasVertexAnimation(), false);
                if (*newDcl != *(sm->vertexData->vertexDeclaration))
                {
                    // Usages don't matter here since we're only exporting
                    Ogre::BufferUsageList bufferUsages;
                    for (size_t u = 0; u <= newDcl->getMaxSource(); ++u)
                        bufferUsages.push_back(Ogre::HardwareBuffer::HBU_STATIC_WRITE_ONLY);
                    sm->vertexData->reorganiseBuffers(newDcl, bufferUsages);
                }
            }
        }

        //meshSer.exportMesh(mMesh.getPointer(), "/home/joni/QT/naali/bin/scenes/TestScene_2/test.mesh");

        //return true;
    }

    //matList.clear();
    //matNameList.clear();

    QString fileLocation = addr.c_str();

    if(!noMat)///*mLoaderParams & */!LP_GENERATE_MATERIALS_AS_CODE)
    {
        Ogre::MaterialSerializer ms;

        std::vector<Ogre::String> exportedNames;
        int tick = 0;
        for(MeshVector::iterator it = mMeshes.begin(); it != mMeshes.end(); ++it)
        {
            Ogre::MeshPtr mMesh = *it;

            // queue up the materials for serialise
            Ogre::MaterialManager *mmptr = Ogre::MaterialManager::getSingletonPtr();
            Ogre::Mesh::SubMeshIterator it = mMesh->getSubMeshIterator();

            while(it.hasMoreElements())
            {
                Ogre::SubMesh* sm = it.getNext();
                Ogre::String matName(sm->getMaterialName());
                if (std::find(exportedNames.begin(), exportedNames.end(), matName) == exportedNames.end())
                {
                    Ogre::MaterialPtr materialPtr = mmptr->getByName(matName);
                    ms.queueForExport(materialPtr, true);

                    std::string materialInfo = ms.getQueuedAsString();

                    if (materialInfo.find("texture ") != std::string::npos && addr.find("http", 0) != std::string::npos)
                        fixMaterialReference(materialInfo, addr);

                    QString parsedRef = fileLocation.remove(0, fileLocation.lastIndexOf("/") + 1);

                    Ogre::LogManager::getSingleton().logMessage("OAI: " + parsedRef.toStdString());
                     Ogre::LogManager::getSingleton().logMessage(materialInfo);

                    if (fileLocation.startsWith("http"))
                        matList[fileLocation.toStdString() + "#" + sm->getMaterialName() + ".material"] = materialInfo;
                    else
                        matList[parsedRef.toStdString() + "#" + sm->getMaterialName() + ".material"] = materialInfo;
                    matNameList.push_back(sm->getMaterialName() + ".material");

                }
            }
        }
    }

    //
    mMeshes.clear();
    mMaterialCode = "";
    mBonesByName.clear();
    mBoneNodesByName.clear();
    boneMap.clear();
    mSkeleton = Ogre::SkeletonPtr(NULL);
    mCustomAnimationName = "";
    // etc...

    Ogre::MeshManager::getSingleton().removeUnreferencedResources();
    Ogre::SkeletonManager::getSingleton().removeUnreferencedResources();

    return true;
}





typedef boost::tuple< aiVectorKey*, aiQuatKey*, aiVectorKey* > KeyframeData;
typedef std::map< float, KeyframeData > KeyframesMap;

template <int v>
        struct Int2Type
{
    enum { value = v };
};

// T should be a Loki::Int2Type<>
template< typename T > void GetInterpolationIterators(KeyframesMap& keyframes,
KeyframesMap::iterator it,
KeyframesMap::reverse_iterator& front,
KeyframesMap::iterator& back)
{
    front = KeyframesMap::reverse_iterator(it);

    front++;
    for(front; front != keyframes.rend(); front++)
    {
        if(boost::get< T::value >(front->second) != NULL)
        {
            break;
        }
    }

    back = it;
    back++;
    for(back; back != keyframes.end(); back++)
    {
        if(boost::get< T::value >(back->second) != NULL)
        {
            break;
        }
    }
}

aiVector3D getTranslate(aiNodeAnim* node_anim, KeyframesMap& keyframes, KeyframesMap::iterator it)
{
    aiVectorKey* translateKey = boost::get<0>(it->second);
    aiVector3D vect;
    if(translateKey)
    {
        vect = translateKey->mValue;
    }
    else
    {
        KeyframesMap::reverse_iterator front;
        KeyframesMap::iterator back;


        GetInterpolationIterators< Int2Type<0> > (keyframes, it, front, back);

        KeyframesMap::reverse_iterator rend = keyframes.rend();
        KeyframesMap::iterator end = keyframes.end();
        aiVectorKey* frontKey = NULL;
        aiVectorKey* backKey = NULL;

        if(front != rend)
            frontKey = boost::get<0>(front->second);

        if(back != end)
            backKey = boost::get<0>(back->second);

        // got 2 keys can interpolate
        if(frontKey && backKey)
        {
            float prop = (it->first - frontKey->mTime) / (backKey->mTime - frontKey->mTime);
            vect = ((backKey->mValue - frontKey->mValue) * prop) + frontKey->mValue;
        }

        else if(frontKey)
        {
            vect = frontKey->mValue;
        }
        else if(backKey)
        {
            vect = backKey->mValue;
        }
    }

    return vect;
}

aiQuaternion getRotate(aiNodeAnim* node_anim, KeyframesMap& keyframes, KeyframesMap::iterator it)
{
    aiQuatKey* rotationKey = boost::get<1>(it->second);
    aiQuaternion rot;
    if(rotationKey)
    {
        rot = rotationKey->mValue;
    }
    else
    {
        KeyframesMap::reverse_iterator front;
        KeyframesMap::iterator back;

        GetInterpolationIterators< Int2Type<1> > (keyframes, it, front, back);

        KeyframesMap::reverse_iterator rend = keyframes.rend();
        KeyframesMap::iterator end = keyframes.end();
        aiQuatKey* frontKey = NULL;
        aiQuatKey* backKey = NULL;

        if(front != rend)
            frontKey = boost::get<1>(front->second);

        if(back != end)
            backKey = boost::get<1>(back->second);

        // got 2 keys can interpolate
        if(frontKey && backKey)
        {
            float prop = (it->first - frontKey->mTime) / (backKey->mTime - frontKey->mTime);
            aiQuaternion::Interpolate(rot, frontKey->mValue, backKey->mValue, prop);
        }

        else if(frontKey)
        {
            rot = frontKey->mValue;
        }
        else if(backKey)
        {
            rot = backKey->mValue;
        }
    }

    return rot;
}

void OpenAssetImport::parseAnimation (const aiScene* mScene, int index, aiAnimation* anim)
{
    // DefBonePose a matrix that represents the local bone transform (can build from Ogre bone components)
    // PoseToKey a matrix representing the keyframe translation
    // What assimp stores aiNodeAnim IS the decomposed form of the transform (DefBonePose * PoseToKey)
    // To get PoseToKey which is what Ogre needs we'ed have to build the transform from components in
    // aiNodeAnim and then DefBonePose.Inverse() * aiNodeAnim(generated transform) will be the right transform

    Ogre::String animName;
    if(mCustomAnimationName != "")
    {
        animName = mCustomAnimationName;
        if(index >= 1)
        {
            animName += Ogre::StringConverter::toString(index);
        }
    }
    else
    {
        animName = Ogre::String(anim->mName.data);
    }
    if(animName.length() < 1)
    {
        animName = "Animation" + Ogre::StringConverter::toString(index);
    }

    Ogre::LogManager::getSingleton().logMessage("Animation name = '" + animName + "'");
    Ogre::LogManager::getSingleton().logMessage("duration = " + Ogre::StringConverter::toString(Ogre::Real(anim->mDuration)));
    Ogre::LogManager::getSingleton().logMessage("tick/sec = " + Ogre::StringConverter::toString(Ogre::Real(anim->mTicksPerSecond)));
    Ogre::LogManager::getSingleton().logMessage("channels = " + Ogre::StringConverter::toString(anim->mNumChannels));

    Ogre::Animation* animation;

    float cutTime = 0.0;
    if(mLoaderParams & LP_CUT_ANIMATION_WHERE_NO_FURTHER_CHANGE)
    {
        for (int i = 1; i < (int)anim->mNumChannels; i++)
        {
            aiNodeAnim* node_anim = anim->mChannels[i];

            // times of the equality check
            float timePos = 0.0;
            float timeRot = 0.0;

            for(int i = 1; i < node_anim->mNumPositionKeys; i++)
            {
                if( node_anim->mPositionKeys[i] != node_anim->mPositionKeys[i-1])
                {
                    timePos = node_anim->mPositionKeys[i].mTime;
                }
            }

            for(int i = 1; i < node_anim->mNumRotationKeys; i++)
            {
                if( node_anim->mRotationKeys[i] != node_anim->mRotationKeys[i-1])
                {
                    timeRot = node_anim->mRotationKeys[i].mTime;
                }
            }

            if(timePos > cutTime){ cutTime = timePos; }
            if(timeRot > cutTime){ cutTime = timeRot; }
        }

        animation = mSkeleton->createAnimation(Ogre::String(animName), Ogre::Real(cutTime));
    }
    else
    {
        cutTime = Ogre::Math::POS_INFINITY;
        animation = mSkeleton->createAnimation(Ogre::String(animName), Ogre::Real(anim->mDuration));
    }

    animation->setInterpolationMode(Ogre::Animation::IM_LINEAR);

    Ogre::LogManager::getSingleton().logMessage("Cut Time " + Ogre::StringConverter::toString(cutTime));

    for (int i = 0; i < (int)anim->mNumChannels; i++)
    {
        Ogre::TransformKeyFrame* keyframe;

        aiNodeAnim* node_anim = anim->mChannels[i];
        Ogre::LogManager::getSingleton().logMessage("Channel " + Ogre::StringConverter::toString(i));
        Ogre::LogManager::getSingleton().logMessage("affecting node: " + Ogre::String(node_anim->mNodeName.data));
        //Ogre::LogManager::getSingleton().logMessage("position keys: " + Ogre::StringConverter::toString(node_anim->mNumPositionKeys));
        //Ogre::LogManager::getSingleton().logMessage("rotation keys: " + Ogre::StringConverter::toString(node_anim->mNumRotationKeys));
        //Ogre::LogManager::getSingleton().logMessage("scaling keys: " + Ogre::StringConverter::toString(node_anim->mNumScalingKeys));

        Ogre::String boneName = Ogre::String(node_anim->mNodeName.data);

        if(mSkeleton->hasBone(boneName))
        {
            Ogre::Bone* bone = mSkeleton->getBone(boneName);
            Ogre::Matrix4 defBonePoseInv;
            defBonePoseInv.makeInverseTransform(bone->getPosition(), bone->getScale(), bone->getOrientation());

            Ogre::NodeAnimationTrack* track = animation->createNodeTrack(i, bone);

            // Ogre needs translate rotate and scale for each keyframe in the track
            KeyframesMap keyframes;

            for(int i = 0; i < node_anim->mNumPositionKeys; i++)
            {
                keyframes[ node_anim->mPositionKeys[i].mTime ] = KeyframeData( &(node_anim->mPositionKeys[i]), NULL, NULL);
            }

            for(int i = 0; i < node_anim->mNumRotationKeys; i++)
            {
                KeyframesMap::iterator it = keyframes.find(node_anim->mRotationKeys[i].mTime);
                if(it != keyframes.end())
                {
                    boost::get<1>(it->second) = &(node_anim->mRotationKeys[i]);
                }
                else
                {
                    keyframes[ node_anim->mRotationKeys[i].mTime ] = KeyframeData( NULL, &(node_anim->mRotationKeys[i]), NULL );
                }
            }

            for(int i = 0; i < node_anim->mNumScalingKeys; i++)
            {
                KeyframesMap::iterator it = keyframes.find(node_anim->mScalingKeys[i].mTime);
                if(it != keyframes.end())
                {
                    boost::get<2>(it->second) = &(node_anim->mScalingKeys[i]);
                }
                else
                {
                    keyframes[ node_anim->mRotationKeys[i].mTime ] = KeyframeData( NULL, NULL, &(node_anim->mScalingKeys[i]) );
                }
            }

            KeyframesMap::iterator it = keyframes.begin();
            KeyframesMap::iterator it_end = keyframes.end();
            for(it; it != it_end; ++it)
            {
                if(it->first < cutTime)	// or should it be <=
                {
                    aiVector3D aiTrans = getTranslate( node_anim, keyframes, it );

                    Ogre::Vector3 trans(aiTrans.x, aiTrans.y, aiTrans.z);

                    aiQuaternion aiRot = getRotate(node_anim, keyframes, it);
                    Ogre::Quaternion rot(aiRot.w, aiRot.x, aiRot.y, aiRot.z);
                    Ogre::Vector3 scale(1,1,1);	// ignore scale for now

                    Ogre::Vector3 transCopy = trans;

                    Ogre::Matrix4 fullTransform;
                    fullTransform.makeTransform(trans, scale, rot);

                    Ogre::Matrix4 poseTokey = defBonePoseInv * fullTransform;
                    //poseTokey.decomposition(trans, scale, rot);

                    keyframe = track->createNodeKeyFrame(Ogre::Real(it->first));

                    // weirdness with the root bone, But this seems to work
                    if(mSkeleton->getRootBone()->getName() == boneName)
                    {
                        trans = transCopy - bone->getPosition();
                    }

                    keyframe->setTranslate(trans);
                    keyframe->setRotation(rot);
                }
            }

        } // if bone exists

    } // loop through channels

    mSkeleton->optimiseAllAnimations();

}



void OpenAssetImport::markAllChildNodesAsNeeded(const aiNode *pNode)
{
    flagNodeAsNeeded(pNode->mName.data);
    // Traverse all child nodes of the current node instance
    for ( int childIdx=0; childIdx<pNode->mNumChildren; ++childIdx )
    {
        const aiNode *pChildNode = pNode->mChildren[ childIdx ];
        markAllChildNodesAsNeeded(pChildNode);
    }
}

void OpenAssetImport::grabNodeNamesFromNode(const aiScene* mScene, const aiNode* pNode)
{
    boneNode bNode;
    bNode.node = const_cast<aiNode*>(pNode);
    if(NULL != pNode->mParent)
    {
        bNode.parent = const_cast<aiNode*>(pNode->mParent);
    }
    bNode.isNeeded = false;
    boneMap.insert(std::pair<Ogre::String, boneNode>(Ogre::String(pNode->mName.data), bNode));
    mBoneNodesByName[pNode->mName.data] = pNode;
    Ogre::LogManager::getSingleton().logMessage("Node " + Ogre::String(pNode->mName.data) + " found.");

    // Traverse all child nodes of the current node instance
    for ( int childIdx=0; childIdx<pNode->mNumChildren; ++childIdx )
    {
        const aiNode *pChildNode = pNode->mChildren[ childIdx ];
        grabNodeNamesFromNode(mScene, pChildNode);
    }
}


void OpenAssetImport::computeNodesDerivedTransform(const aiScene* mScene,  const aiNode *pNode, const aiMatrix4x4 accTransform)
{
    if(mNodeDerivedTransformByName.find(pNode->mName.data) == mNodeDerivedTransformByName.end())
    {
        mNodeDerivedTransformByName[pNode->mName.data] = accTransform;
    }
    for ( int childIdx=0; childIdx<pNode->mNumChildren; ++childIdx )
    {
        const aiNode *pChildNode = pNode->mChildren[ childIdx ];
        computeNodesDerivedTransform(mScene, pChildNode, accTransform * pChildNode->mTransformation);
    }
}

void OpenAssetImport::createBonesFromNode(const aiScene* mScene,  const aiNode *pNode)
{
    if(isNodeNeeded(pNode->mName.data))
    {
        Ogre::Bone* bone = mSkeleton->createBone(Ogre::String(pNode->mName.data), msBoneCount);

        aiQuaternion rot;
        aiVector3D pos;
        aiVector3D scale;

        /*
                aiMatrix4x4 aiM = mNodeDerivedTransformByName.find(pNode->mName.data)->second;

                const aiNode* parentNode = NULL;
                {
                        boneMapType::iterator it = boneMap.find(pNode->mName.data);
                        if(it != boneMap.end())
                        {
                                parentNode = it->second.parent;
                        }
                }
                if(parentNode)
                {
                        aiMatrix4x4 aiMParent = mNodeDerivedTransformByName.find(parentNode->mName.data)->second;
                        aiM = aiMParent.Inverse() * aiM;
                }
                */

        // above should be the same as
        aiMatrix4x4 aiM = pNode->mTransformation;

        aiM.Decompose(scale, rot, pos);


        /*
                // debug render
                Ogre::SceneNode* sceneNode = NULL;
                if(parentNode)
                {
                        Ogre::SceneNode* parent = static_cast<Ogre::SceneNode*>(
                                GOOF::NodeUtils::GetNodeMatch(getSceneManager()->getRootSceneNode(), parentNode->mName.data, false));
                        assert(parent);
                        sceneNode = parent->createChildSceneNode(pNode->mName.data);
                }
                else
                {
                        sceneNode = getSceneManager()->getRootSceneNode()->createChildSceneNode(pNode->mName.data);
                }

                sceneNode->setScale(scale.x, scale.y, scale.z);
                sceneNode->setPosition(pos.x, pos.y, pos.z);
                sceneNode->setOrientation(rot.w, rot.x, rot.y, rot.z);

                sceneNode = sceneNode->createChildSceneNode();
                sceneNode->setScale(0.01, 0.01, 0.01);
                sceneNode->attachObject(getSceneManager()->createEntity("Box1m.mesh"));
                */

        if (!aiM.IsIdentity())
        {
            bone->setPosition(pos.x, pos.y, pos.z);
            bone->setOrientation(rot.w, rot.x, rot.y, rot.z);
        }

        Ogre::LogManager::getSingleton().logMessage(Ogre::StringConverter::toString(msBoneCount) + ") Creating bone '" + Ogre::String(pNode->mName.data) + "'");
        msBoneCount++;
    }
    // Traverse all child nodes of the current node instance
    for ( int childIdx=0; childIdx<pNode->mNumChildren; ++childIdx )
    {
        const aiNode *pChildNode = pNode->mChildren[ childIdx ];
        createBonesFromNode(mScene, pChildNode);
    }
}

void OpenAssetImport::createBoneHiearchy(const aiScene* mScene,  const aiNode *pNode)
{
    if(isNodeNeeded(pNode->mName.data))
    {
        Ogre::Bone* parent = 0;
        Ogre::Bone* child = 0;
        if(pNode->mParent)
        {
            if(mSkeleton->hasBone(pNode->mParent->mName.data))
            {
                parent = mSkeleton->getBone(pNode->mParent->mName.data);
            }
        }
        if(mSkeleton->hasBone(pNode->mName.data))
        {
            child = mSkeleton->getBone(pNode->mName.data);
        }
        if(parent && child)
        {
            parent->addChild(child);
        }
    }
    // Traverse all child nodes of the current node instance
    for ( int childIdx=0; childIdx<pNode->mNumChildren; childIdx++ )
    {
        const aiNode *pChildNode = pNode->mChildren[ childIdx ];
        createBoneHiearchy(mScene, pChildNode);
    }
}

void OpenAssetImport::flagNodeAsNeeded(const char* name)
{
    boneMapType::iterator iter = boneMap.find(Ogre::String(name));
    if( iter != boneMap.end())
    {
        iter->second.isNeeded = true;
    }
}

bool OpenAssetImport::isNodeNeeded(const char* name)
{
    boneMapType::iterator iter = boneMap.find(Ogre::String(name));
    if( iter != boneMap.end())
    {
        return iter->second.isNeeded;
    }
    return false;
}

void OpenAssetImport::grabBoneNamesFromNode(const aiScene* mScene,  const aiNode *pNode)
{

    meshNum++;
    if(pNode->mNumMeshes > 0)
    {
        for ( int idx=0; idx<pNode->mNumMeshes; ++idx )
        {
            aiMesh *pAIMesh = mScene->mMeshes[ pNode->mMeshes[ idx ] ];

            if(pAIMesh->HasBones())
            {
                for ( Ogre::uint32 i=0; i < pAIMesh->mNumBones; ++i )
                {
                    aiBone *pAIBone = pAIMesh->mBones[ i ];
                    if ( NULL != pAIBone )
                    {
                        mBonesByName[pAIBone->mName.data] = pAIBone;

                        Ogre::LogManager::getSingleton().logMessage(Ogre::StringConverter::toString(i) + ") REAL BONE with name : " + Ogre::String(pAIBone->mName.data));

                        // flag this node and all parents of this node as needed, until we reach the node holding the mesh, or the parent.
                        aiNode* node = mScene->mRootNode->FindNode(pAIBone->mName.data);
                        while(node)
                        {
                            if(node->mName.data == pNode->mName.data)
                            {
                                // flagNodeAsNeeded(node->mName.data);
                                // Set mSkeletonRootNode to this node, which is the same node as the one holding the mesh
                                //mSkeletonRootNode = node;
                                break;
                            }
                            if(node->mName.data == pNode->mParent->mName.data)
                            {
                                //flagNodeAsNeeded(node->mName.data);
                                // Set mSkeletonRootNode to this node, which is the parent node to the node holding the mesh
                                //mSkeletonRootNode = node;
                                break;
                            }

                            // Not a root node, flag this as needed and continue to the parent
                            flagNodeAsNeeded(node->mName.data);
                            node = node->mParent;
                        }

                        // Flag all children of this node as needed
                        node = mScene->mRootNode->FindNode(pAIBone->mName.data);
                        markAllChildNodesAsNeeded(node);

                    } // if we have a valid bone
                } // loop over bones
            } // if this mesh has bones
        } // loop over meshes
    } // if this node has meshes

    // Traverse all child nodes of the current node instance
    for ( int childIdx=0; childIdx<pNode->mNumChildren; childIdx++ )
    {
        const aiNode *pChildNode = pNode->mChildren[ childIdx ];
        grabBoneNamesFromNode(mScene, pChildNode);
    }
}

Ogre::String ReplaceSpaces(const Ogre::String& s)
{
    Ogre::String res(s);
    replace(res.begin(), res.end(), ' ', '_');

    return res;
}

Ogre::MaterialPtr OpenAssetImport::createMaterial(int index, const aiMaterial* mat, const Ogre::String& mDir)
{
    static int dummyMatCount = 0;

    // extreme fallback texture -- 2x2 hot pink
    Ogre::uint8 s_RGB[] = {128, 0, 255, 128, 0, 255, 128, 0, 255, 128, 0, 255};

    std::ostringstream matname;
    Ogre::MaterialManager* omatMgr =  Ogre::MaterialManager::getSingletonPtr();
    enum aiTextureType type = aiTextureType_DIFFUSE;
    aiString path;
    aiTextureMapping mapping = aiTextureMapping_UV;       // the mapping (should be uv for now)
    unsigned int uvindex = 0;                             // the texture uv index channel
    float blend = 1.0f;                                   // blend
    aiTextureOp op = aiTextureOp_Multiply;                // op
    aiTextureMapMode mapmode[2] =  { aiTextureMapMode_Wrap, aiTextureMapMode_Wrap };    // mapmode
    std::ostringstream texname;

    aiString szPath;


    if(AI_SUCCESS == aiGetMaterialString(mat, AI_MATKEY_TEXTURE_DIFFUSE(0), &szPath))
    {
        Ogre::LogManager::getSingleton().logMessage("Using aiGetMaterialString : Found texture " + Ogre::String(szPath.data) + " for channel " + Ogre::StringConverter::toString(uvindex));
    }
    if(szPath.length < 1 && !dontGenMaterials)
    {
        Ogre::LogManager::getSingleton().logMessage("Didn't find any texture units...");
        szPath = Ogre::String("dummyMat" + Ogre::StringConverter::toString(dummyMatCount)).c_str();
        dummyMatCount++;
    }




    Ogre::String basename;
    Ogre::String outPath;
    Ogre::StringUtil::splitFilename(Ogre::String(szPath.data), basename, outPath);
    Ogre::LogManager::getSingleton().logMessage("Creating " + basename);

    Ogre::ResourceManager::ResourceCreateOrRetrieveResult status = omatMgr->createOrRetrieve(ReplaceSpaces(basename), "General", true);
    Ogre::MaterialPtr omat = status.first;
    if (!status.second)
        return omat;

    // ambient
    aiColor4D clr(1.0f, 1.0f, 1.0f, 1.0);
    //Ambient is usually way too low! FIX ME!
    if (mat->GetTexture(type, 0, &path) != AI_SUCCESS)
        aiGetMaterialColor(mat, AI_MATKEY_COLOR_AMBIENT,  &clr);
    omat->setAmbient(clr.r, clr.g, clr.b);


    // diffuse
    clr = aiColor4D(1.0f, 1.0f, 1.0f, 1.0f);
    if(AI_SUCCESS == aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &clr))
    {
        omat->setDiffuse(clr.r, clr.g, clr.b, clr.a);
    }

    // specular
    clr = aiColor4D(1.0f, 1.0f, 1.0f, 1.0f);
    if(AI_SUCCESS == aiGetMaterialColor(mat, AI_MATKEY_COLOR_SPECULAR, &clr))
    {
        omat->setSpecular(clr.r, clr.g, clr.b, clr.a);
    }

    // emissive
    clr = aiColor4D(1.0f, 1.0f, 1.0f, 1.0f);
    if(AI_SUCCESS == aiGetMaterialColor(mat, AI_MATKEY_COLOR_EMISSIVE, &clr))
    {
        omat->setSelfIllumination(clr.r, clr.g, clr.b);
    }



    float fShininess;
    if(AI_SUCCESS == aiGetMaterialFloat(mat, AI_MATKEY_SHININESS, &fShininess))
    {
        omat->setShininess(Ogre::Real(fShininess));
    }

    if (mat->GetTexture(type, 0, &path) == AI_SUCCESS)
    {
        Ogre::LogManager::getSingleton().logMessage("Found texture " + Ogre::String(path.data) + " for channel " + Ogre::StringConverter::toString(uvindex));
        if(AI_SUCCESS == aiGetMaterialString(mat, AI_MATKEY_TEXTURE_DIFFUSE(0), &szPath))
        {
            Ogre::LogManager::getSingleton().logMessage("Using aiGetMaterialString : Found texture " + Ogre::String(szPath.data) + " for channel " + Ogre::StringConverter::toString(uvindex));
        }

        // attempt to load the image
        Ogre::Image image;

        // possibly if we fail to actually find it, pop up a box?
        Ogre::String pathname(mDir + "\\" + path.data);

        std::ifstream imgstream;
        imgstream.open(path.data, std::ios::binary);
        if(!imgstream.is_open())
            imgstream.open(Ogre::String(mDir + Ogre::String("\\") + Ogre::String(path.data)).c_str(), std::ios::binary);

        if (imgstream.is_open())
        {
            // Wrap as a stream
            Ogre::DataStreamPtr strm(OGRE_NEW Ogre::FileStreamDataStream(path.data, &imgstream, false));

            if (!strm->size() || strm->size() == 0xffffffff)
            {
                // fall back to our very simple and very hardcoded hot-pink version
                Ogre::DataStreamPtr altStrm(OGRE_NEW Ogre::MemoryDataStream(s_RGB, sizeof(s_RGB)));
                image.loadRawData(altStrm, 2, 2, Ogre::PF_R8G8B8);
                Ogre::LogManager::getSingleton().logMessage("Could not load texture, falling back to hotpink");
            } else
            {
                // extract extension from filename
                size_t pos = pathname.find_last_of('.');
                Ogre::String ext = pathname.substr(pos+1);
                image.load(strm, ext);
                imgstream.close();
            }
        } else {
            // fall back to our very simple and very hardcoded hot-pink version
            Ogre::DataStreamPtr altStrm(OGRE_NEW Ogre::MemoryDataStream(s_RGB, sizeof(s_RGB)));
            image.loadRawData(altStrm, 2, 2, Ogre::PF_R8G8B8);
            Ogre::LogManager::getSingleton().logMessage("Could not load texture, falling back to hotpink - 2");
        }
        //szPath.Set("/home/joni/AssImpTesting/SkeletalAnimation/boy_10.tga");
        Ogre::TextureManager::getSingleton().loadImage(Ogre::String(szPath.data), Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, image);
        //TODO: save this to materials/textures ?
        Ogre::TextureUnitState* texUnitState = omat->getTechnique(0)->getPass(0)->createTextureUnitState(basename);

    }
    omat->load();
    return omat;
}





bool OpenAssetImport::createSubMesh(const Ogre::String& name, int index, const aiNode* pNode, const aiMesh *mesh, const aiMaterial* mat, Ogre::MeshPtr mMesh, Ogre::AxisAlignedBox& mAAB, const Ogre::String& mDir)
{
    // if animated all submeshes must have bone weights
    if(mBonesByName.size() && !mesh->HasBones())
    {
        Ogre::LogManager::getSingleton().logMessage("Skipping Mesh " + Ogre::String(mesh->mName.data) + "with no bone weights");
        return false;
    }

    Ogre::MaterialPtr matptr;

    if(mLoaderParams & !LP_GENERATE_MATERIALS_AS_CODE)
    {
        matptr = createMaterial(mesh->mMaterialIndex, mat, mDir);
    }
    else
    {
        matptr = createMaterial(mesh->mMaterialIndex, mat, mDir);
        //matptr = createMaterialByScript(mesh->mMaterialIndex, mat);
    }

    // now begin the object definition
    // We create a submesh per material
    Ogre::SubMesh* submesh = mMesh->createSubMesh(name + Ogre::StringConverter::toString(index));

    // prime pointers to vertex related data
    aiVector3D *vec = mesh->mVertices;
    aiVector3D *norm = mesh->mNormals;
    aiVector3D *uv = mesh->mTextureCoords[0];
    //aiColor4D *col = mesh->mColors[0];

    // We must create the vertex data, indicating how many vertices there will be
    submesh->useSharedVertices = false;
    submesh->vertexData = new Ogre::VertexData();
    submesh->vertexData->vertexStart = 0;
    submesh->vertexData->vertexCount = mesh->mNumVertices;

    // We must now declare what the vertex data contains
    Ogre::VertexDeclaration* declaration = submesh->vertexData->vertexDeclaration;
    const unsigned short source = 0;
    size_t offset = 0;
    offset += declaration->addElement(source,offset,Ogre::VET_FLOAT3,Ogre::VES_POSITION).getSize();

    //mLog->logMessage((boost::format(" %d vertices ") % m->mNumVertices).str());
    Ogre::LogManager::getSingleton().logMessage(Ogre::StringConverter::toString(mesh->mNumVertices) + " vertices");
    if (norm)
    {
        Ogre::LogManager::getSingleton().logMessage(Ogre::StringConverter::toString(mesh->mNumVertices) + " normals");
        //mLog->logMessage((boost::format(" %d normals ") % m->mNumVertices).str() );
        offset += declaration->addElement(source,offset,Ogre::VET_FLOAT3,Ogre::VES_NORMAL).getSize();
    }

    if (uv)
    {
        Ogre::LogManager::getSingleton().logMessage(Ogre::StringConverter::toString(mesh->mNumVertices) + " uvs");
        //mLog->logMessage((boost::format(" %d uvs ") % m->mNumVertices).str() );
        offset += declaration->addElement(source,offset,Ogre::VET_FLOAT2,Ogre::VES_TEXTURE_COORDINATES).getSize();
    }

    /*
        if (col)
        {
                Ogre::LogManager::getSingleton().logMessage(Ogre::StringConverter::toString(mesh->mNumVertices) + " colours");
                //mLog->logMessage((boost::format(" %d colours ") % m->mNumVertices).str() );
                offset += declaration->addElement(source,offset,Ogre::VET_FLOAT3,Ogre::VES_DIFFUSE).getSize();
        }
        */


    // We create the hardware vertex buffer
    Ogre::HardwareVertexBufferSharedPtr vbuffer =
            Ogre::HardwareBufferManager::getSingleton().createVertexBuffer(declaration->getVertexSize(source), // == offset
                                                                           submesh->vertexData->vertexCount,   // == nbVertices
                                                                           Ogre::HardwareBuffer::HBU_STATIC_WRITE_ONLY);

    aiMatrix4x4 aiM = mNodeDerivedTransformByName.find(pNode->mName.data)->second;


    // Now we get access to the buffer to fill it.  During so we record the bounding box.
    float* vdata = static_cast<float*>(vbuffer->lock(Ogre::HardwareBuffer::HBL_DISCARD));
    for (size_t i=0;i < mesh->mNumVertices; ++i)
    {
        // Position
        aiVector3D vect;
        vect.x = vec->x;
        vect.y = vec->y;
        vect.z = vec->z;

        vect *= aiM;

        /*
        if(NULL != mSkeletonRootNode)
        {
            vect *= mSkeletonRootNode->mTransformation;
        }
                */

        Ogre::Vector3 position( vect.x, vect.y, vect.z );
        *vdata++ = vect.x;
        *vdata++ = vect.y;
        *vdata++ = vect.z;
        mAAB.merge(position);
        vec++;

        // Normal
        if (norm)
        {
            vect.x = norm->x;
            vect.y = norm->y;
            vect.z = norm->z;

            vect *= aiM;

            *vdata++ = vect.x;
            *vdata++ = vect.y;
            *vdata++ = vect.z;
            norm++;

            //*vdata++ = norm->x;
            //*vdata++ = norm->y;
            //*vdata++ = norm->z;
            //norm++;
        }

        // uvs
        if (uv)
        {
            *vdata++ = uv->x;
            *vdata++ = uv->y;
            uv++;
        }

        /*
                if (col)
                {
                        *vdata++ = col->r;
                        *vdata++ = col->g;
                        *vdata++ = col->b;
                        //*vdata++ = col->a;
                        //col++;
                }
                */
    }

    vbuffer->unlock();
    submesh->vertexData->vertexBufferBinding->setBinding(source,vbuffer);

    Ogre::LogManager::getSingleton().logMessage(Ogre::StringConverter::toString(mesh->mNumFaces) + " faces");
    aiFace *f = mesh->mFaces;

    // Creates the index data
    submesh->indexData->indexStart = 0;
    submesh->indexData->indexCount = mesh->mNumFaces * 3;
    submesh->indexData->indexBuffer =
            Ogre::HardwareBufferManager::getSingleton().createIndexBuffer(Ogre::HardwareIndexBuffer::IT_16BIT,
                                                                          submesh->indexData->indexCount,
                                                                          Ogre::HardwareBuffer::HBU_STATIC_WRITE_ONLY);
    Ogre::uint16* idata = static_cast<Ogre::uint16*>(submesh->indexData->indexBuffer->lock(Ogre::HardwareBuffer::HBL_DISCARD));

    // poke in the face data
    for (size_t i=0; i < mesh->mNumFaces;++i)
    {
        //		wxASSERT(f->mNumIndices == 3);
        *idata++ = f->mIndices[0];
        *idata++ = f->mIndices[1];
        *idata++ = f->mIndices[2];
        f++;
    }
    submesh->indexData->indexBuffer->unlock();

    // set bone weigths
    if(mesh->HasBones())
    {
        for ( Ogre::uint32 i=0; i < mesh->mNumBones; i++ )
        {
            aiBone *pAIBone = mesh->mBones[ i ];
            if ( NULL != pAIBone )
            {
                Ogre::String bname = pAIBone->mName.data;
                for ( Ogre::uint32 weightIdx = 0; weightIdx < pAIBone->mNumWeights; weightIdx++ )
                {
                    aiVertexWeight aiWeight = pAIBone->mWeights[ weightIdx ];

                    Ogre::VertexBoneAssignment vba;
                    vba.vertexIndex = aiWeight.mVertexId;
                    vba.boneIndex = mSkeleton->getBone(bname)->getHandle();
                    vba.weight= aiWeight.mWeight;

                    submesh->addBoneAssignment(vba);
                }
            }
        }
    } // if mesh has bones

    // Finally we set a material to the submesh
    submesh->setMaterialName(matptr->getName());

    return true;
}

void OpenAssetImport::loadDataFromNode(const aiScene* mScene,  const aiNode *pNode, const Ogre::String& mDir)
{
    if(pNode->mNumMeshes > 0)
    {
        Ogre::MeshPtr mesh;
        Ogre::AxisAlignedBox mAAB;

        if(mLoaderParams & LP_GENERATE_SINGLE_MESH)
        {
            if(mMeshes.size() == 0)
            {
                mesh = Ogre::MeshManager::getSingleton().createManual("ROOTMesh", "General");
                mMeshes.push_back(mesh);
            }
            else
            {
                mesh = mMeshes[0];
                mAAB = mesh->getBounds();
            }
        }

        for ( int idx=0; idx<pNode->mNumMeshes; ++idx )
        {
            aiMesh *pAIMesh = mScene->mMeshes[ pNode->mMeshes[ idx ] ];
            //if pAIMesh->
            Ogre::LogManager::getSingleton().logMessage("SubMesh " + Ogre::StringConverter::toString(idx) + " for mesh '" + Ogre::String(pNode->mName.data) + "'");

            // Create a material instance for the mesh.
            const aiMaterial *pAIMaterial = mScene->mMaterials[ pAIMesh->mMaterialIndex ];
            createSubMesh(pNode->mName.data, idx, pNode, pAIMesh, pAIMaterial, mesh, mAAB, mDir);
        }

        // We must indicate the bounding box
        mesh->_setBounds(mAAB);
        mesh->_setBoundingSphereRadius((mAAB.getMaximum()- mAAB.getMinimum()).length()/2.0);
    }

    // Traverse all child nodes of the current node instance
    for ( int childIdx=0; childIdx<pNode->mNumChildren; childIdx++ )
    {
        const aiNode *pChildNode = pNode->mChildren[ childIdx ];
        loadDataFromNode(mScene, pChildNode, mDir);
    }
}


