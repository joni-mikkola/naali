/**
 *  For conditions of distribution and use, see copyright notice in license.txt
 *
 *  @file   SupportedFileTypes.h
 *  @brief  Defines for file extensions supported by Tundra.
 */

#ifndef incl_SceneStructureModule_SupportedFileTypes_h
#define incl_SceneStructureModule_SupportedFileTypes_h

// Supported file extensions.
const QString cTundraXmlFileExtension(".txml");     ///< Tundra XML file extension.
const QString cTundraBinFileExtension(".tbin");     ///< Tundra binary file extension.
const QString cOgreMeshFileExtension(".mesh");      ///< OGRE mesh file extension.
const QString cColladaFileExtension(".dae");    ///< Collada file extension.
const QString cOgreSceneFileExtension(".scene");    ///< Tundra binary file extension.

// File filter definitions for supported files.
const QString cOgreSceneFileFilter(QApplication::translate("SceneTreeWidget", "OGRE scene (*.scene)")); ///< OGRE .scene
const QString cOgreMeshFileFilter(QApplication::translate("SceneTreeWidget", "OGRE mesh (*.mesh)"));    ///< OGRE .mesh

/// All supported mesh formats if Open Asset Import (http://assimp.sourceforge.net/) is used.
const QString cMeshFileFilter(QApplication::translate("SceneTreeWidget", "Mesh (*.txml *.3d *.b3d *.dae *.blend *.bvh *.3ds "
    "*.ase *.obj *.ply *.dxf *.nff *.smd *.vta *.mdl *.md2 *.md3 *.mdc *.md5mesh *.x *.q3o *.q3s *.raw *.ac *.stl *.irrmesh "
    "*.irr *.off *.ter *.mdl *.hmp *.ms3d *.lwo *.lws *.lxo *.csm *.ply *.cob *.scn)"));

const QString cTundraXmlFileFilter(QApplication::translate("SceneTreeWidget", "Tundra scene XML(*.txml)")); ///< Tundra XML file filter.
const QString cTundraBinaryFileFilter(QApplication::translate("SceneTreeWidget", "Tundra Binary Format (*.tbin)")); ///< Tundra binary file filter.

/// All supported file formats if Open Asset Import (http://assimp.sourceforge.net/) is used.
const QString cAllSupportedTypesFileFilter(QApplication::translate("SceneTreeWidget", "All supported types (*.scene *.mesh *.txml "
    "*.tbin *.xml *.dae *.blend *.bvh *.3ds *.ase *.obj *.ply *.dxf *.nff *.smd *.vta *.mdl *.md2 *.md3 *.mdc *.md5mesh *.x *.q3o *.q3s *.raw "
    "*.ac *.stl *.irrmesh *.irr *.off *.ter *.mdl *.hmp *.ms3d *.lwo *.lws *.lxo *.csm *.ply *.cob *.scn)"));

//const QString cAllSupportedTypesFileFilter(QApplication::translate("SceneTreeWidget", "All supported types (*.scene *.mesh *.txml *.tbin)"));


const QString cAllTypesFileFilter(QApplication::translate("SceneTreeWidget", "All types (*.*)"));   ///< All types file filter.

#endif
