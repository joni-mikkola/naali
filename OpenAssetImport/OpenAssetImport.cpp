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
#include "OgreVector3.h"
#include <QString>
#include <QStringList>
#include <boost/tuple/tuple.hpp>
//#include "OgreXMLSkeletonSerializer.h"

static int meshNum = 0;

inline Ogre::String toString(const aiColor4D& colour)
{
    return	Ogre::StringConverter::toString(colour.r) + " " +
            Ogre::StringConverter::toString(colour.g) + " " +
            Ogre::StringConverter::toString(colour.b) + " " +
            Ogre::StringConverter::toString(colour.a);
}

int OpenAssetImport::msBoneCount = 0;

void FixLocalReference(QString &matRef, QString addRef)
{
    Ogre::LogManager::getSingleton().logMessage(addRef.toStdString());
    addRef = addRef.remove(addRef.lastIndexOf('/'), addRef.length());
    addRef = addRef.remove(addRef.lastIndexOf('/'), addRef.length());
    addRef = addRef.remove(0, addRef.lastIndexOf('/')+1);
    addRef.insert(0, "local://");
    addRef.insert(addRef.length(), "/images/");

    size_t indx = matRef.indexOf("texture ", 0);
    matRef.replace(indx+8, 0, addRef + '/');
}

void FixHttpReference(QString &matRef, QString addRef)
{
    addRef.replace(0, 7, "http://");
    for (uint i = 0; i < addRef.length(); ++i)
        if (addRef[i].toAscii() == '_') addRef[i] = '/';

    size_t tmp = addRef.lastIndexOf('/')+1;
    addRef.remove(tmp, addRef.length() - tmp);

    addRef = addRef.remove(addRef.lastIndexOf('/'), addRef.length());
    addRef = addRef.remove(addRef.lastIndexOf('/'), addRef.length());
    addRef.insert(addRef.length(), "/images/");
    size_t indx = matRef.indexOf("texture ", 0);
    matRef.replace(indx+8, 0, addRef);
}

double degreeToRadian(double degree)
{
    double radian = 0;
    radian = degree * (Ogre::Math::PI/180);
    return radian;
}

void OpenAssetImport::linearScaleMesh(Ogre::MeshPtr mesh, int targetSize)
{
    Ogre::AxisAlignedBox mAAB = mMesh->getBounds();
    Ogre::Vector3 meshSize = mAAB.getSize();
    Ogre::Vector3 origSize = meshSize;
    while (meshSize.x >= targetSize || meshSize.y >= targetSize || meshSize.z >= targetSize)
        meshSize /= 2;

    Ogre::Real minCoefficient = meshSize.x / origSize.x;

    // Iterate thru submeshes
    Ogre::Mesh::SubMeshIterator smit = mesh->getSubMeshIterator();
    while( smit.hasMoreElements() )
    {
        Ogre::SubMesh* submesh = smit.getNext();
        if(submesh)
        {
            Ogre::VertexData* vertex_data = submesh->vertexData;
            if(vertex_data)
            {
                const Ogre::VertexElement* posElem = vertex_data->vertexDeclaration->findElementBySemantic(Ogre::VES_POSITION);
                Ogre::HardwareVertexBufferSharedPtr vbuf = vertex_data->vertexBufferBinding->getBuffer(posElem->getSource());
                unsigned char* vertex = static_cast<unsigned char*>(vbuf->lock(Ogre::HardwareBuffer::HBL_NORMAL));
                Ogre::Real* points;

                for(size_t j = 0; j < vertex_data->vertexCount; ++j, vertex += vbuf->getVertexSize())
                {
                    posElem->baseVertexPointerToElement(vertex, (void **)&points);
                    points[0] *= minCoefficient;
                    points[1] *= minCoefficient;
                    points[2] *= minCoefficient;
                }

                vbuf->unlock();
            }
        }
    }

    Ogre::Vector3 scale(minCoefficient, minCoefficient, minCoefficient);
    mAAB.scale(scale);

    mMesh->_setBounds(mAAB);
}

bool OpenAssetImport::convert(const Ogre::String& filename, bool generateMaterials, QString addr)
{
    Ogre::LogManager::getSingleton().getDefaultLog()->setLogDetail(Ogre::LL_NORMAL);
    meshNum = 0;

    if (mLoaderParams & LP_USE_LAST_RUN_NODE_DERIVED_TRANSFORMS == false)
    {
        mNodeDerivedTransformByName.clear();
    }
    
    Assimp::DefaultLogger::create("asslogger.log",Assimp::Logger::VERBOSE);

    //Ogre::LogManager::getSingleton().logMessage("Filename " + filename);

    this->addr = addr;


    Assimp::Importer importer;

    // by default remove points and lines from the model, since these are usually
    // degenerate structures from bad modelling or bad import/export.  if they
    // are needed it can be turned on with IncludeLinesPoints
    int removeSortFlags = importer.GetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE);
    this->generateMaterials = generateMaterials;
    removeSortFlags |= aiPrimitiveType_POINT | aiPrimitiveType_LINE;

    /// NOTICE!!!
    // Some converted mesh might show up pretty messed up, it's happening because some formats might
    // contain unnecessary vertex information, lines and points. Uncomment the line below for fixing this issue.

    importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_LINE | aiPrimitiveType_POINT);
    /// END OF NOTICE

    // Limit triangles because for each mesh there's limited index memory (16bit)
    // ...which should be easy to just change to 32 bit but it didn't seem to be the case
    importer.SetPropertyInteger(AI_CONFIG_PP_SLM_TRIANGLE_LIMIT, 21845);

    importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS, aiComponent_CAMERAS|aiComponent_LIGHTS|aiComponent_TEXTURES|aiComponent_ANIMATIONS);
    //importer.SetPropertyInteger(AI_CONFIG_FAVOUR_SPEED,1);
    //importer.SetPropertyInteger(AI_CONFIG_GLOB_MEASURE_TIME,0);

    // And have it read the given file with some example postprocessing
    // Usually - if speed is not the most important aspect for you - you'll
    // propably to request more postprocessing than we do in this example.
    scene = importer.ReadFile(filename, 0 //aiProcessPreset_TargetRealtime_MaxQuality |  aiProcess_PreTransformVertices);/*
                              | aiProcess_SplitLargeMeshes
                              | aiProcess_FindInvalidData
                              | aiProcess_GenNormals
                              //| aiProcess_GenUVCoords
                              //| aiProcess_TransformUVCoords
                              | aiProcess_Triangulate
                              | aiProcess_FlipUVs
                              | aiProcess_JoinIdenticalVertices
                              | aiProcess_PreTransformVertices
                              | aiProcess_OptimizeMeshes
                              | aiProcess_RemoveRedundantMaterials
                              | aiProcess_ImproveCacheLocality
                              //| aiProcess_LimitBoneWeights
                              | aiProcess_SortByPType
                              );

    // If the import failed, report it
    if( !scene)
    {
        Ogre::LogManager::getSingleton().logMessage("AssImp importer failed with the following message:");
        Ogre::LogManager::getSingleton().logMessage(importer.GetErrorString() );
        return false;
    }

    grabNodeNamesFromNode(scene, scene->mRootNode);

#ifdef USE_SKELETONS
    grabBoneNamesFromNode(scene, scene->mRootNode);
#endif

    const struct aiNode *rootNode = scene->mRootNode;

    aiMatrix4x4 transform;
    transform.FromEulerAnglesXYZ(degreeToRadian(90), 0, degreeToRadian(180));

    computeNodesDerivedTransform(scene, scene->mRootNode, transform);

#ifdef USE_SKELETONS
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
#endif

    loadDataFromNode(scene, scene->mRootNode, mPath);
    Ogre::LogManager::getSingleton().logMessage("*** Finished loading ass file ***");
    Assimp::DefaultLogger::kill();
    
#ifdef USE_SKELETONS
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



                // AutogreMaterialic
#if OGRE_VERSION_MINOR >= 8 && OGRE_VERSION_MAJOR >= 1
                Ogre::VertexDeclaration* newDcl =
                        sm->vertexData->vertexDeclaration->getAutoOrganisedDeclaration(mMesh->hasSkeleton(), mMesh->hasVertexAnimation(), false);
#else
                Ogre::VertexDeclaration* newDcl =
                        sm->vertexData->vertexDeclaration->getAutoOrganisedDeclaration(mMesh->hasSkeleton(), mMesh->hasVertexAnimation());
#endif
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
    }
#endif

    QString fileLocation = addr;

    if(generateMaterials)
    {
        Ogre::MaterialSerializer ms;
        for(MeshVector::iterator it = mMeshes.begin(); it != mMeshes.end(); ++it)
        {
            mMesh = *it;

            // queue up the materials for serialise
            Ogre::MaterialManager *mmptr = Ogre::MaterialManager::getSingletonPtr();
            Ogre::Mesh::SubMeshIterator it = mMesh->getSubMeshIterator();

            while(it.hasMoreElements())
            {
                Ogre::SubMesh* sm = it.getNext();

                Ogre::String matName(sm->getMaterialName());
                Ogre::MaterialPtr materialPtr = mmptr->getByName(matName);

                ms.queueForExport(materialPtr, true);

                QString materialInfo = ms.getQueuedAsString().c_str();

                if (materialInfo.contains("texture "))
                {
                    if (addr.startsWith("http"))
                        FixHttpReference(materialInfo, addr);
                    else
                        if (!scene->HasTextures())
                            FixLocalReference(materialInfo, addr);
                }

                if (fileLocation.startsWith("http"))
                    matList[fileLocation + "#" + sm->getMaterialName().c_str() + ".material"] = materialInfo;
                else
                {
                    QStringList parsedRef = fileLocation.split("/");
                    int length=parsedRef.length();
                    QString output=parsedRef[length-3] + "_" + parsedRef[length-2] + "_" + parsedRef[length-1];

                    matList[output + "#" + sm->getMaterialName().c_str() + ".material"] = materialInfo;
                }

                QString tmp = sm->getMaterialName().c_str();
                matNameList.push_back(tmp + ".material");
            }
        }
    }

    //Scale mesh scale (xyz) below 10 units
    linearScaleMesh(mMesh, 10);

    mMeshes.clear();
    mMaterialCode = "";
    mBonesByName.clear();
    mBoneNodesByName.clear();
    boneMap.clear();
    mSkeleton = Ogre::SkeletonPtr(NULL);
    mCustomAnimationName = "";
    // etc...

    // Ogre::MaterialManager::getSingleton().
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
                    Ogre::Vector3 scale(1,1,1);	//   ignore scale for now

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

bool OpenAssetImport::IsSupportedExtension(QString extension)
{
    const char *openAssImpFileTypes[] = { ".3d", ".b3d", ".blend", ".dae", ".bvh", ".3ds", ".ase", ".obj", ".ply", ".dxf",
                                          ".nff", ".smd", ".vta", ".mdl", ".md2", ".md3", ".mdc", ".md5mesh", ".x", ".q3o", ".q3s", ".raw", ".ac",
                                          ".stl", ".irrmesh", ".irr", ".off", ".ter", ".mdl", ".hmp", ".ms3d", ".lwo", ".lws", ".lxo", ".csm",
                                          ".ply", ".cob", ".scn" };

    int numSuffixes = NUMELEMS(openAssImpFileTypes);

    for(int i = 0;i < numSuffixes; ++i)
        if (extension.contains(openAssImpFileTypes[i]))
            return true;

    return false;
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

        // above should be the same as
        aiMatrix4x4 aiM = pNode->mTransformation;

        aiM.Decompose(scale, rot, pos);

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
    assert(pNode);
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
    static int texCount = 0;
    // extreme fallback texture -- 2x2 hot pink
    Ogre::uint8 s_RGB[] = {0, 0, 0, 128, 0, 255, 128, 0, 255, 128, 0, 255};

    std::ostringstream matname;
    Ogre::MaterialManager* ogreMaterialMgr =  Ogre::MaterialManager::getSingletonPtr();
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
    if(szPath.length < 1 && generateMaterials)
    {
        Ogre::LogManager::getSingleton().logMessage("Didn't find any texture units...");
        szPath = Ogre::String("dummyMat" + Ogre::StringConverter::toString(dummyMatCount)).c_str();
        dummyMatCount++;
    }

    Ogre::String basename;
    Ogre::String outPath;
    Ogre::StringUtil::splitFilename(Ogre::String(szPath.data), basename, outPath);
    Ogre::LogManager::getSingleton().logMessage("Creating " + addr.toStdString());
    
    if (scene->HasTextures() && szPath.length > 0)
        basename.insert(0, addr.right(addr.length() - (addr.lastIndexOf('/')+1)).toStdString());

    Ogre::ResourceManager::ResourceCreateOrRetrieveResult status = ogreMaterialMgr->createOrRetrieve(ReplaceSpaces(basename), "General", true);
    Ogre::MaterialPtr ogreMaterial = status.first;
    if (!status.second)
        return ogreMaterial;

    // ambient
    aiColor4D clr(1.0f, 1.0f, 1.0f, 1.0);
    //Ambient is usually way too low! FIX ME!
    if (mat->GetTexture(type, 0, &path) != AI_SUCCESS)
        aiGetMaterialColor(mat, AI_MATKEY_COLOR_AMBIENT,  &clr);
    ogreMaterial->setAmbient(clr.r, clr.g, clr.b);

    // diffuse
    clr = aiColor4D(1.0f, 1.0f, 1.0f, 1.0f);
    if(AI_SUCCESS == aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &clr))
    {
        ogreMaterial->setDiffuse(clr.r, clr.g, clr.b, clr.a);
    }

    // specular
    clr = aiColor4D(1.0f, 1.0f, 1.0f, 1.0f);
    if(AI_SUCCESS == aiGetMaterialColor(mat, AI_MATKEY_COLOR_SPECULAR, &clr))
    {
        ogreMaterial->setSpecular(clr.r, clr.g, clr.b, clr.a);
    }

    // emissive
    clr = aiColor4D(1.0f, 1.0f, 1.0f, 1.0f);
    if(AI_SUCCESS == aiGetMaterialColor(mat, AI_MATKEY_COLOR_EMISSIVE, &clr))
    {
        ogreMaterial->setSelfIllumination(clr.r, clr.g, clr.b);
    }

    float fShininess;
    if(AI_SUCCESS == aiGetMaterialFloat(mat, AI_MATKEY_SHININESS, &fShininess))
    {
        ogreMaterial->setShininess(Ogre::Real(fShininess));
    }

    int two_sided;
    aiGetMaterialInteger(mat, AI_MATKEY_TWOSIDED, &two_sided);

    if (two_sided != 0)
        ogreMaterial->setCullingMode(Ogre::CULL_NONE);

    if (mat->GetTexture(type, 0, &path) == AI_SUCCESS)
    {

        //hack for showing back and front faces when loading material containing texture
        //it's working for 3d warehouse models though the problem is probably in the models and how the faces has been set by the modeler
        //ogreMaterial->setCullingMode(Ogre::CULL_NONE);

        //set texture info into the ogreMaterial
        if (!scene->HasTextures())
            Ogre::TextureUnitState* texUnitState = ogreMaterial->getTechnique(0)->getPass(0)->createTextureUnitState(basename);
        else
        {
            // If data[0] is *, assume texture index is given
            if (path.data[0] == '*')
            {
                Ogre::Image img;

                int texIndex;
                std::string parsedReference = path.data;
                parsedReference.erase(0, 1);
                std::stringstream str(parsedReference);

                // Parse number after * char in path.data
                str >> texIndex;

                // Hint for compressed texture format (e.g. "jpg", "png" etc)
                std::string format = scene->mTextures[texIndex]->achFormatHint;
                parsedReference.append("." + format);

                QString modelFile = addr.mid(addr.lastIndexOf('/') + 1, addr.length() - addr.lastIndexOf('/'));
                modelFile = modelFile.left(modelFile.lastIndexOf('.'));
                parsedReference.insert(0, modelFile.toStdString());

                // Create datastream from scene->mTextures[texIndex] where texIndex points for each compressed texture data
                // mWidth stores texture size in bytes
                Ogre::DataStreamPtr altStrm(OGRE_NEW Ogre::MemoryDataStream((unsigned char*)scene->mTextures[texIndex]->pcData, scene->mTextures[texIndex]->mWidth, false));
                // Load image to Ogre::Image
                img.load(altStrm);
                // Load image to Ogre Resourcemanager
                Ogre::TextureManager::getSingleton().loadImage(parsedReference.c_str(), Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, img, Ogre::TEX_TYPE_2D, 0);

                // Png format might contain alpha data so allow alpha blending
                if (format == "png")
                {
                    // Alpha blending needs fixing
                    ogreMaterial->setSceneBlending(Ogre::SBT_TRANSPARENT_ALPHA);
                    ogreMaterial->setDepthWriteEnabled(false);
                    ogreMaterial->setDepthCheckEnabled(true);
                    //ogreMaterial->setDepthBias(0.01f, 1.0f);
                    ogreMaterial->setDepthFunction(Ogre::CMPF_LESS );
                }

                //Set loaded image as a texture reference. So Tundra knows name should be loaded from ResourceManager
                ogreMaterial->getTechnique(0)->getPass(0)->createTextureUnitState(parsedReference.c_str());
            }

        }

    }
    ogreMaterial->load();
    return ogreMaterial;
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

    if(generateMaterials)
        matptr = createMaterial(mesh->mMaterialIndex, mat, mDir);

    // now begin the object definition
    // We create a submesh per material
    Ogre::SubMesh* submesh = mMesh->createSubMesh(name + Ogre::StringConverter::toString(index));

    // We must create the vertex data, indicating how many vertices there will be
    submesh->useSharedVertices = false;
#include "DisableMemoryLeakCheck.h"
    submesh->vertexData = new Ogre::VertexData();
#include "EnableMemoryLeakCheck.h"
    submesh->vertexData->vertexStart = 0;
    submesh->vertexData->vertexCount = mesh->mNumVertices;
    Ogre::VertexData *data = submesh->vertexData;

    // Vertex declarations
    size_t offset = 0;
    Ogre::VertexDeclaration* decl = submesh->vertexData->vertexDeclaration;
    offset += decl->addElement(0,offset,Ogre::VET_FLOAT3,Ogre::VES_POSITION).getSize();
    offset += decl->addElement(0,offset,Ogre::VET_FLOAT3,Ogre::VES_NORMAL).getSize();

    offset = 0;
    for (int tn=0 ; tn<AI_MAX_NUMBER_OF_TEXTURECOORDS ; ++tn)
    {
        if (mesh->mTextureCoords[tn])
        {
            if (mesh->mNumUVComponents[tn] == 3)
            {
                decl->addElement(1, offset, Ogre::VET_FLOAT3, Ogre::VES_TEXTURE_COORDINATES, tn);
                offset += Ogre::VertexElement::getTypeSize(Ogre::VET_FLOAT3);
            } else
            {
                decl->addElement(1, offset, Ogre::VET_FLOAT2, Ogre::VES_TEXTURE_COORDINATES, tn);
                offset += Ogre::VertexElement::getTypeSize(Ogre::VET_FLOAT2);
            }
        }
    }
    if (mesh->HasTangentsAndBitangents())
    {
        decl->addElement(1, offset, Ogre::VET_FLOAT3, Ogre::VES_TANGENT);
        offset += Ogre::VertexElement::getTypeSize(Ogre::VET_FLOAT3);
        decl->addElement(1, offset, Ogre::VET_FLOAT3, Ogre::VES_BINORMAL);
    }

    // Write vertex data to buffer
    Ogre::HardwareVertexBufferSharedPtr vbuf = Ogre::HardwareBufferManager::getSingleton().createVertexBuffer(
            decl->getVertexSize(0), // This value is the size of a vertex in memory
            data->vertexCount, // The number of vertices you'll put into this buffer
            Ogre::HardwareBuffer::HBU_DYNAMIC // Properties
            );
    Ogre::Real *vbData = static_cast<Ogre::Real*>(vbuf->lock(Ogre::HardwareBuffer::HBL_NORMAL));

    aiMatrix4x4 aiM = mNodeDerivedTransformByName.find(pNode->mName.data)->second;

    offset = 0;
    aiVector3D vect;
    for (unsigned int n=0 ; n<data->vertexCount ; ++n)
    {
        if (mesh->mVertices != NULL)
        {
            vect.x = mesh->mVertices[n].x;
            vect.y = mesh->mVertices[n].y;
            vect.z = mesh->mVertices[n].z;
            vect *= aiM;

            Ogre::Vector3 position( vect.x, vect.y, vect.z );
            vbData[offset++] = vect.x;
            vbData[offset++] = vect.y;
            vbData[offset++] = vect.z;

            mAAB.merge(position);
        }

        if (mesh->mNormals != NULL)
        {
            vect.x = mesh->mNormals[n].x;
            vect.y = mesh->mNormals[n].y;
            vect.z = mesh->mNormals[n].z;
            vect *= aiM;

            vbData[offset++] = vect.x;
            vbData[offset++] = vect.y;
            vbData[offset++] = vect.z;
        }
    }

    vbuf->unlock();
    data->vertexBufferBinding->setBinding(0, vbuf);

    if (mesh->HasTextureCoords(0))
    {
        vbuf = Ogre::HardwareBufferManager::getSingleton().createVertexBuffer(
                decl->getVertexSize(1), // This value is the size of a vertex in memory
                data->vertexCount, // The number of vertices you'll put into this buffer
                Ogre::HardwareBuffer::HBU_DYNAMIC // Properties
                );
        vbData = static_cast<Ogre::Real*>(vbuf->lock(Ogre::HardwareBuffer::HBL_NORMAL));

        offset = 0;
        for (unsigned int n=0 ; n<data->vertexCount ; ++n)
        {
            for (int tn=0 ; tn<AI_MAX_NUMBER_OF_TEXTURECOORDS ; ++tn)
            {
                if (mesh->mTextureCoords[tn])
                {
                    if (mesh->mNumUVComponents[tn] == 3)
                    {
                        vbData[offset++] = mesh->mTextureCoords[tn][n].x;
                        vbData[offset++] = mesh->mTextureCoords[tn][n].y;
                        vbData[offset++] = mesh->mTextureCoords[tn][n].z;
                    } else
                    {
                        vbData[offset++] = mesh->mTextureCoords[tn][n].x;
                        vbData[offset++] = mesh->mTextureCoords[tn][n].y;
                    }
                }
            }

            if (mesh->HasTangentsAndBitangents())
            {
                vbData[offset++] = mesh->mTangents[n].x;
                vbData[offset++] = mesh->mTangents[n].y;
                vbData[offset++] = mesh->mTangents[n].z;

                vbData[offset++] = mesh->mBitangents[n].x;
                vbData[offset++] = mesh->mBitangents[n].y;
                vbData[offset++] = mesh->mBitangents[n].z;
            }
        }
        vbuf->unlock();
        data->vertexBufferBinding->setBinding(1, vbuf);
    }

    size_t numIndices = mesh->mNumFaces * 3;            // support only triangles, so 3 indices per face

    Ogre::HardwareIndexBufferSharedPtr ibuf = Ogre::HardwareBufferManager::getSingleton().createIndexBuffer(
            Ogre::HardwareIndexBuffer::IT_16BIT, // You can use several different value types here
            numIndices, // The number of indices you'll put in that buffer
            Ogre::HardwareBuffer::HBU_DYNAMIC // Properties
            );

    Ogre::uint16 *idxData = static_cast<Ogre::uint16*>(ibuf->lock(Ogre::HardwareBuffer::HBL_NORMAL));
    offset = 0;
    for (int n=0 ; n<mesh->mNumFaces ; ++n)
    {
        idxData[offset++] = mesh->mFaces[n].mIndices[0];
        idxData[offset++] = mesh->mFaces[n].mIndices[1];
        idxData[offset++] = mesh->mFaces[n].mIndices[2];
    }

    ibuf->unlock();

    submesh->indexData->indexBuffer = ibuf; // The pointer to the index buffer
    submesh->indexData->indexCount = numIndices; // The number of indices we'll use
    submesh->indexData->indexStart = 0;

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
    if (generateMaterials)
        submesh->setMaterialName(matptr->getName());

    return true;
}

void OpenAssetImport::loadDataFromNode(const aiScene* mScene,  const aiNode *pNode, const Ogre::String& mDir)
{
    if(pNode->mNumMeshes > 0)
    {
        Ogre::MeshPtr mesh;
        Ogre::AxisAlignedBox mAAB;

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


