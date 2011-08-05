# =============================================================================
# per-dependency configuration macros
#
# All per-dependency configuration (or hacks) should go here. All per-module
# build instructions should go in <Module>/CMakeLists.txt. The rest should
# remain generic.

macro (configure_boost)
    if (MSVC)
	    set(Boost_USE_MULTITHREADED TRUE)
        set(Boost_USE_STATIC_LIBS TRUE)
    else ()
        set(Boost_USE_STATIC_LIBS FALSE)
    endif ()

    if (UNIX)
            set (BOOST_COMPONENTS boost_date_time boost_filesystem boost_system boost_thread boost_regex boost_program_options)
    else ()
            set (BOOST_COMPONENTS date_time filesystem system thread regex program_options)
    endif ()
 
    sagase_configure_package (BOOST 
        NAMES Boost boost
        COMPONENTS ${BOOST_COMPONENTS}
        PREFIXES ${ENV_NAALI_DEP_PATH})

    if (APPLE)
        set (BOOST_LIBRARY_DIRS ${ENV_NAALI_DEP_PATH}/lib)
        set (BOOST_INCLUDE_DIRS ${ENV_NAALI_DEP_PATH}/include)
    endif()

    # boost library naming is complex, and FindBoost.cmake is preferred to 
    # find the correct names. however on windows it appears to not find the
    # library directories correctly. find_path cannot be counted on to find
    # the libraries as component thread -> libboost_thread_vc90-mt.lib (etc.)

    if (MSVC)
        set (BOOST_INCLUDE_DIRS ${BOOST_INCLUDE_DIRS} ${ENV_NAALI_DEP_PATH}/Boost/include)
        set (BOOST_LIBRARY_DIRS ${BOOST_LIBRARY_DIRS} ${ENV_NAALI_DEP_PATH}/Boost/lib)
    endif ()

    sagase_configure_report (BOOST)
endmacro (configure_boost)

macro (configure_poco)
    sagase_configure_package (POCO 
        NAMES Poco PoCo poco
        COMPONENTS Poco PocoFoundation PocoNet PocoUtil PocoXML
        PREFIXES ${ENV_NAALI_DEP_PATH})

    # POCO autolinks on MSVC
    if (MSVC)
        set (POCO_LIBRARIES "")
        set (POCO_DEBUG_LIBRARIES "")
    endif ()

    sagase_configure_report (POCO)
endmacro (configure_poco)

macro (configure_qt4)

    sagase_configure_package (QT4 
        NAMES Qt4 4.7.0
        COMPONENTS QtCore QtGui QtWebkit QtScript QtScriptTools QtXml QtNetwork QtUiTools
        PREFIXES ${ENV_NAALI_DEP_PATH} ${ENV_QT_DIR})

    # FindQt4.cmake
    if (QT4_FOUND AND QT_USE_FILE)

        include (${QT_USE_FILE})

        set (QT4_INCLUDE_DIRS 
            ${QT_INCLUDE_DIR}
            ${QT_QTCORE_INCLUDE_DIR}
            ${QT_QTGUI_INCLUDE_DIR}
            ${QT_QTUITOOLS_INCLUDE_DIR}
            ${QT_QTNETWORK_INCLUDE_DIR}
            ${QT_QTXML_INCLUDE_DIR}
            ${QT_QTSCRIPT_INCLUDE_DIR}
            ${QT_QTSCRIPTTOOLS_INCLUDE_DIR}
            ${QT_QTWEBKIT_INCLUDE_DIR}
            ${QT_PHONON_INCLUDE_DIR}
            ${QT_QTDBUS_INCLUDE_DIR})

        set (QT4_LIBRARY_DIR  
            ${QT_LIBRARY_DIR})

        set (QT4_LIBRARIES 
            ${QT_LIBRARIES}
            ${QT_QTCORE_LIBRARY}
            ${QT_QTGUI_LIBRARY}
            ${QT_QTUITOOLS_LIBRARY}
            ${QT_QTNETWORK_LIBRARY}
            ${QT_QTXML_LIBRARY}
            ${QT_QTSCRIPT_LIBRARY}
            ${QT_QTSCRIPTTOOLS_LIBRARY}
            ${QT_QTWEBKIT_LIBRARY}
            ${QT_PHONON_LIBRARY}
            ${QT_QTDBUS_LIBRARY})

        set (QT4_MKSPECS ${QT_MKSPECS_DIR})

    endif ()
    
    sagase_configure_report (QT4)
endmacro (configure_qt4)

macro (configure_python)
    sagase_configure_package (PYTHON
        NAMES PythonLibs Python python Python26 python26 Python2.6 python2.6
        COMPONENTS Python python Python26 python Python2.6 python2.6
        PREFIXES ${ENV_NAALI_DEP_PATH})

    # FindPythonLibs.cmake
    if (PYTHONLIBS_FOUND)
        set (PYTHON_LIBRARIES ${PYTHON_LIBRARY})
        set (PYTHON_INCLUDE_DIRS ${PYTHON_INCLUDE_PATH})
        #unset (PYTHON_DEBUG_LIBRARIES ${PYTHON_DEBUG_LIBRARY})
    endif ()
	
    # FindPythonLibs.cmake prefers the system-wide Python, which does not
    # include debug libraries, so we force to NAALI_DEP_PATH.

	if (MSVC)
		set (PYTHON_LIBRARY_DIRS ${ENV_NAALI_DEP_PATH}/Python/lib)
		set (PYTHON_INCLUDE_DIRS ${ENV_NAALI_DEP_PATH}/Python/include)
		set (PYTHON_LIBRARIES python26)
		set (PYTHON_DEBUG_LIBRARIES python26_d)
	endif()
    
    sagase_configure_report (PYTHON)
endmacro (configure_python)

macro (configure_python_qt)
    sagase_configure_package (PYTHON_QT
        NAMES PythonQt
        COMPONENTS PythonQt PythonQt_QtAll
        PREFIXES ${ENV_NAALI_DEP_PATH})

    sagase_configure_report (PYTHON_QT)
endmacro (configure_python_qt)

macro (configure_ogre)
   
    if (NOT WIN32)
        # Mac
        if (APPLE)
    	  FIND_LIBRARY (OGRE_LIBRARY NAMES Ogre)
    	  set (OGRE_INCLUDE_DIRS ${OGRE_LIBRARY}/Headers)
    	  set (OGRE_LIBRARIES ${OGRE_LIBRARY})
        # Linux
        else ()
            sagase_configure_package (OGRE 
                NAMES Ogre OgreSDK ogre OGRE
                COMPONENTS Ogre ogre OGRE OgreMain 
                PREFIXES ${ENV_OGRE_HOME} ${ENV_NAALI_DEP_PATH})
        endif ()
        
        sagase_configure_report (OGRE)
        
    else ()
        # Find ogre
        if (DirectX_FOUND)
            set (TUNDRA_OGRE_NEEDED_COMPONENTS Ogre ogre OGRE OgreMain RenderSystem_Direct3D9)
        else ()
            set (TUNDRA_OGRE_NEEDED_COMPONENTS Ogre ogre OGRE OgreMain)
        endif()
        
        sagase_configure_package (OGRE 
            NAMES Ogre OgreSDK ogre OGRE
            COMPONENTS ${TUNDRA_OGRE_NEEDED_COMPONENTS}
            PREFIXES ${ENV_OGRE_HOME} ${ENV_NAALI_DEP_PATH})

        # Report ogre then search check directx
        sagase_configure_report (OGRE)
        
        # DirectX SDK found, use DX9 surface blitting
        message (STATUS "** Configuring DirectX")
        if (DirectX_FOUND)
            message (STATUS "-- Include Directories:")
            message (STATUS "       " ${DirectX_INCLUDE_DIR})
            message (STATUS "-- Library Directories:")
            message (STATUS "       " ${DirectX_LIBRARY_DIR})
            message (STATUS "-- Defines:")
            message (STATUS "        USE_D3D9_SUBSURFACE_BLIT")
            
            add_definitions (-DUSE_D3D9_SUBSURFACE_BLIT)
            include_directories (${DirectX_INCLUDE_DIR})
            link_directories (${DirectX_LIBRARY_DIR})
        else ()
            message (STATUS "DirectX not found!")
            message (STATUS "-- Install DirectX SDK to enable additional features. If you already have the DirectX SDK installed")
            message (STATUS "   please set DIRECTX_ROOT env variable as your installation directory.")
        endif()
        message (STATUS "")
    endif ()
endmacro (configure_ogre)

macro(link_ogre)
    if (OGRE_LIBRARIES)
        link_package(OGRE)
    else()    
        target_link_libraries(${TARGET_NAME} debug OgreMain_d.lib)
        target_link_libraries(${TARGET_NAME} optimized OgreMain.lib)
        if (WIN32)
            target_link_libraries(${TARGET_NAME} debug RenderSystem_Direct3D9_d.lib)
            target_link_libraries(${TARGET_NAME} optimized RenderSystem_Direct3D9.lib)
        endif()
    endif()
endmacro()

macro (configure_caelum)
    sagase_configure_package (CAELUM 
        NAMES Caelum caelum CAELUM
        COMPONENTS Caelum caelum CAELUM
        PREFIXES ${ENV_NAALI_DEP_PATH})
    
    sagase_configure_report (CAELUM)
endmacro (configure_caelum)

macro (configure_qtpropertybrowser)
    sagase_configure_package (QT_PROPERTY_BROWSER 
        NAMES QtPropertyBrowser QtSolutions_PropertyBrowser-2.5
        COMPONENTS QtPropertyBrowser QtSolutions_PropertyBrowser-2.5
        PREFIXES ${ENV_NAALI_DEP_PATH})
    
    sagase_configure_report (QT_PROPERTY_BROWSER)
endmacro (configure_qtpropertybrowser)

macro (configure_hydrax)
    sagase_configure_package (HYDRAX 
        NAMES Hydrax
        COMPONENTS Hydrax
        PREFIXES ${ENV_NAALI_DEP_PATH})
    
    sagase_configure_report (HYDRAX)
endmacro (configure_hydrax)

macro (configure_xmlrpc)
    if (APPLE)
        sagase_configure_package (XMLRPC 
            NAMES xmlrpc xmlrpcepi xmlrpc-epi
            COMPONENTS xmlrpc xmlrpcepi xmlrpc-epi xmlrpcepid iconv
            PREFIXES ${ENV_NAALI_DEP_PATH}
                ${ENV_NAALI_DEP_PATH}/xmlrpc-epi/src
                ${ENV_NAALI_DEP_PATH}/xmlrpc-epi/Debug
                ${ENV_NAALI_DEP_PATH}/xmlrpc-epi/Release)
    else()
        sagase_configure_package (XMLRPC 
            NAMES xmlrpc xmlrpcepi xmlrpc-epi
            COMPONENTS xmlrpc xmlrpcepi xmlrpc-epi xmlrpcepid
            PREFIXES ${ENV_NAALI_DEP_PATH}
                ${ENV_NAALI_DEP_PATH}/xmlrpc-epi/src
                ${ENV_NAALI_DEP_PATH}/xmlrpc-epi/Debug
		${ENV_NAALI_DEP_PATH}/xmlrpc-epi/Release)
    endif()
   
	if (MSVC)
		set (XMLRPC_LIBRARIES xmlrpcepi)
		set (XMLRPC_DEBUG_LIBRARIES xmlrpcepid)
	endif()
	
    sagase_configure_report (XMLRPC)
endmacro (configure_xmlrpc)

macro (configure_curl)
    sagase_configure_package (CURL 
        NAMES Curl curl libcurl
        COMPONENTS curl libcurl_imp libcurld_imp
        PREFIXES ${ENV_NAALI_DEP_PATH}
        ${ENV_NAALI_DEP_PATH}/libcurl/lib/DLL-Debug 
        ${ENV_NAALI_DEP_PATH}/libcurl/lib/DLL-Release)		
    
	if (MSVC)
		set (CURL_LIBRARIES libcurl_imp)
		set (CURL_DEBUG_LIBRARIES libcurld_imp)
	endif()

	if (APPLE)
		set (CURL_LIBRARIES curl)
	endif()
	
    sagase_configure_report (CURL)
endmacro (configure_curl)

macro (configure_openjpeg)
    sagase_configure_package (OPENJPEG
        NAMES OpenJpeg OpenJPEG openjpeg
        COMPONENTS OpenJpeg OpenJPEG openjpeg
        PREFIXES ${ENV_NAALI_DEP_PATH}
        ${ENV_NAALI_DEP_PATH}/OpenJpeg/libopenjpeg
        ${ENV_NAALI_DEP_PATH}/OpenJpeg/Debug
        ${ENV_NAALI_DEP_PATH}/OpenJpeg/Release)
    
    sagase_configure_report (OPENJPEG)
endmacro (configure_openjpeg)

macro (configure_telepathy_qt4)
    sagase_configure_package (TELEPATHY_QT4 
        NAMES QtTelepathy Telepathy-QT4 telepathy-qt4 TelepathyQt4 telepathy-1.0
        COMPONENTS QtTelepathyCore QtTelepathyCommon QtTelepathyClient telepathy-qt4 telepathy-qt4-farsight connection # connection added here to help sagase to find include folder with connection.h file
        PREFIXES ${ENV_NAALI_DEP_PATH} )
    sagase_configure_report (TELEPATHY_QT4)
endmacro (configure_telepathy_qt4)

macro (configure_gstreamer)
    sagase_configure_package (GSTREAMER
        NAMES gstreamer gst gstreamer-0.10
        COMPONENTS gstreamer gstfarsight gstinterfaces gst # gst added to help sagese to find include folder with gst.h file
        PREFIXES ${ENV_NAALI_DEP_PATH})
    sagase_configure_report (GSTREAMER)
endmacro (configure_gstreamer)

macro (configure_dbus)
    sagase_configure_package (DBUS
        NAMES dbus dbus-1
        COMPONENTS dbus-1 dbus # dbus added to help sagese to find include folder with dbus.h file
        PREFIXES ${ENV_NAALI_DEP_PATH})
    sagase_configure_report (DBUS)
endmacro (configure_dbus)

macro (configure_glib)
    sagase_configure_package (GLIB
        NAMES glib-2.0 glib Glib
        COMPONENTS glib-2.0 gobject-2.0 glib # glib added to help sagese to find include folder with glib.h file
        PREFIXES ${ENV_NAALI_DEP_PATH})
    sagase_configure_report (GLIB)
endmacro (configure_glib)

macro (configure_telepathy_glib)
    sagase_configure_package (TELEPATHY_GLIB
        NAMES telepathy-glib Telepathy-Glib
        COMPONENTS telepathy-glib connection # connection added to help sagese to find include folder with connection.h file
        PREFIXES ${ENV_NAALI_DEP_PATH})
    sagase_configure_report (TELEPATHY_GLIB)
endmacro (configure_telepathy_glib)

macro (configure_telepathy_farsight)
    sagase_configure_package (TELEPATHY_FARSIGHT 
        NAMES telepathy-farsight Telepathy-Farsight
        COMPONENTS telepathy-farsight stream # stream added to help sagese to find include folder with stream.h file
        PREFIXES ${ENV_NAALI_DEP_PATH})
    sagase_configure_report (TELEPATHY_FARSIGHT)
endmacro (configure_telepathy_farsight)

macro (configure_farsight2)
    sagase_configure_package (FARSIGHT2
        NAMES farsight2 farsight2-0.10
        COMPONENTS gstfarsight fs-interfaces # fs-interfaces added to help sagese to find include folder with fs-interfaces.h file
        PREFIXES ${ENV_NAALI_DEP_PATH}/farsight2)
    sagase_configure_report (FARSIGHT2)
endmacro (configure_farsight2)

macro (configure_dbus_glib)
    sagase_configure_package (DBUS_GLIB
        NAMES dbus-glib dbus
        COMPONENTS dbus-glib
        PREFIXES ${ENV_NAALI_DEP_PATH}/dbus-glib)
    sagase_configure_report (DBUS_GLIB)
endmacro (configure_dbus_glib)

macro (configure_openal)
    sagase_configure_package(OPENAL
        NAMES OpenAL openal
        COMPONENTS al OpenAL32
        PREFIXES ${ENV_NAALI_DEP_PATH}/OpenAL ${ENV_NAALI_DEP_PATH}/OpenAL/libs/Win32)

        if (OPENAL_FOUND)
            set (OPENAL_LIBRARIES ${OPENAL_LIBRARY})
            set (OPENAL_INCLUDE_DIRS ${OPENAL_INCLUDE_DIR})
        endif()

        # Force include dir on MSVC
        if (MSVC)
  		   set (OPENAL_INCLUDE_DIRS ${ENV_NAALI_DEP_PATH}/OpenAL/include)
        endif ()
    sagase_configure_report (OPENAL)
endmacro (configure_openal)

macro (configure_ogg)
    sagase_configure_package(OGG
        NAMES ogg libogg
        COMPONENTS ogg libogg
        PREFIXES ${ENV_NAALI_DEP_PATH}/libogg)
        
        # Force include dir on MSVC
        if (MSVC)
  		   set (OGG_INCLUDE_DIRS ${ENV_NAALI_DEP_PATH}/libogg/include)
        endif ()
    sagase_configure_report (OGG)
endmacro (configure_ogg)

macro (configure_vorbis)
if (APPLE)
    sagase_configure_package(VORBIS
        NAMES vorbisfile vorbis libvorbis libvorbisfile
        COMPONENTS vorbis libvorbis vorbisfile libvorbisfile
        PREFIXES ${ENV_NAALI_DEP_PATH}/libvorbis)
else()
    sagase_configure_package(VORBIS
        NAMES vorbisfile vorbis libvorbis
        COMPONENTS vorbis libvorbis libvorbisfile
        PREFIXES ${ENV_NAALI_DEP_PATH}/libvorbis)
endif()
        # Force include dir on MSVC
        if (MSVC)
  		   set (VORBIS_INCLUDE_DIRS ${ENV_NAALI_DEP_PATH}/libvorbis/include)
        endif ()
    sagase_configure_report (VORBIS)
endmacro (configure_vorbis)

macro (configure_mumbleclient)
    # Not in use currently, remove later if deemed unnecessary.
    sagase_configure_package(MUMBLECLIENT
        NAMES mumbleclient
        COMPONENTS mumbleclient client
        PREFIXES ${ENV_NAALI_DEP_PATH}/libmumbleclient)
    sagase_configure_report (MUMBLECLIENT)
endmacro (configure_mumbleclient)

macro(use_package_mumbleclient)
    message (STATUS "** Configuring mumbleclient")
    if (WIN32)
        # This is a hack, would be better like it was earlier
        # with configure_mumbleclient and use_package/link_package(MUMBLECLIENT)
        sagase_configure_package(MUMBLECLIENT
            NAMES mumbleclient
            COMPONENTS mumbleclient client
            PREFIXES ${ENV_NAALI_DEP_PATH}/libmumbleclient)
        sagase_configure_report (MUMBLECLIENT)
        
        include_directories(${MUMBLECLIENT_INCLUDE_DIRS})
        link_directories(${MUMBLECLIENT_LIBRARY_DIRS})
    else()
        if ("$ENV{MUMBLECLIENT_DIR}" STREQUAL "")
            set(MUMBLECLIENT_DIR ${ENV_NAALI_DEP_PATH})
        endif()
        message (STATUS "-- Include Directories:")
        message (STATUS "       " ${MUMBLECLIENT_DIR}/include/mumbleclient)
        include_directories(${MUMBLECLIENT_DIR}/include/mumbleclient)
        message (STATUS "-- Library Directories:")
        message (STATUS "       " ${MUMBLECLIENT_DIR}/lib)
        link_directories(${MUMBLECLIENT_DIR}/lib)
    endif()
endmacro()

macro(link_package_mumbleclient)
    target_link_libraries(${TARGET_NAME} optimized mumbleclient)
    if (WIN32)
        target_link_libraries(${TARGET_NAME} debug mumbleclientd)
    endif ()
endmacro()

macro (configure_openssl)
    sagase_configure_package(OPENSSL
        NAMES openssl
        COMPONENTS libeay32 ssleay32 ssl
        PREFIXES ${ENV_NAALI_DEP_PATH}/OpenSSL)
    # remove 'NOTFOUND' entry which makes to linking impossible	
    if (MSVC)
        list(REMOVE_ITEM OPENSSL_LIBRARIES debug optimized SSL_EAY_RELEASE-NOTFOUND LIB_EAY_RELEASE-NOTFOUND SSL_EAY_DEBUG-NOTFOUND LIB_EAY_DEBUG-NOTFOUND NOTFOUND)
    message(----------)
    message(${OPENSSL_LIBRARIES})
    message(----------)
    endif ()		
    sagase_configure_report (OPENSSL)
endmacro (configure_openssl)

macro (configure_protobuf)
    sagase_configure_package(PROTOBUF
        NAMES google protobuf
        COMPONENTS protobuf libprotobuf
        PREFIXES ${ENV_NAALI_DEP_PATH}/protobuf)
    # Force include dir and libraries on MSVC
    if (MSVC)
  	    set (PROTOBUF_INCLUDE_DIRS ${ENV_NAALI_DEP_PATH}/protobuf/include)
    endif ()
    sagase_configure_report (PROTOBUF)
	
endmacro (configure_protobuf)

macro (configure_celt)
    sagase_configure_package(CELT
        NAMES celt
        COMPONENTS libcelt  # for libcelt
                   celt0    # for old celt0 name (linux?)
                   celt     # for celt.h
        PREFIXES ${ENV_NAALI_DEP_PATH}/celt)
    sagase_configure_report (CELT)
endmacro (configure_celt)

macro(use_package_knet)
    if ("$ENV{KNET_DIR}" STREQUAL "")
       set(KNET_DIR ${ENV_NAALI_DEP_PATH}/kNet)
    else()           
       set(KNET_DIR $ENV{KNET_DIR})
    endif()
    include_directories(${KNET_DIR}/include)
    link_directories(${KNET_DIR}/lib)
    if (UNIX)    
        add_definitions(-DUNIX)
    endif()
endmacro()

macro(link_package_knet)
    target_link_libraries(${TARGET_NAME} debug kNet)
    target_link_libraries(${TARGET_NAME} optimized kNet)
endmacro()

macro(use_package_bullet)
    if (WIN32)
        if ("$ENV{BULLET_DIR}" STREQUAL "")
            set(BULLET_DIR ${ENV_NAALI_DEP_PATH}/Bullet)
        endif()
        include_directories(${BULLET_DIR}/include)
        include_directories(${BULLET_DIR}/include/ConvexDecomposition)
        link_directories(${BULLET_DIR}/lib)
    else() # Linux, note: mac will also come here..
        if ("$ENV{BULLET_DIR}" STREQUAL "")
            set(BULLET_DIR ${ENV_NAALI_DEP_PATH})
        endif()
        include_directories(${BULLET_DIR}/include/bullet)
        include_directories(${BULLET_DIR}/include/bullet/ConvexDecomposition)
        link_directories(${BULLET_DIR}/lib)
    endif()
endmacro()

macro(link_package_bullet)
    target_link_libraries(${TARGET_NAME} optimized LinearMath optimized BulletDynamics optimized BulletCollision optimized ConvexDecomposition)
    if (WIN32)
        target_link_libraries(${TARGET_NAME} debug LinearMath_d debug BulletDynamics_d debug BulletCollision_d debug ConvexDecomposition_d)
    endif()
endmacro()

macro(use_package_assimp)
    if (WIN32)
        if ("$ENV{ASSIMP_DIR}" STREQUAL "")
           set(ASSIMP_DIR ${ENV_NAALI_DEP_PATH}/assimp)
        endif()
        include_directories(${ASSIMP_DIR}/include)
        link_directories(${ASSIMP_DIR}/lib) # VC10 deps way, no subfolders, dynamic
        link_directories(${ASSIMP_DIR}/lib/assimp_debug_Win32) # VC9 deps way, in subfolder, static
        link_directories(${ASSIMP_DIR}/lib/assimp_release_Win32)
    else() # Linux, note: mac will also come here..
        if ("$ENV{ASSIMP_DIR}" STREQUAL "")
           set(ASSIMP_DIR ${ENV_NAALI_DEP_PATH})
        endif()
        include_directories(${ASSIMP_DIR}/include/assimp)
        link_directories(${ASSIMP_DIR}/lib)
    endif()
endmacro()

macro(link_package_assimp)
    target_link_libraries(${TARGET_NAME} optimized assimp)
    if (WIN32)
        target_link_libraries(${TARGET_NAME} debug assimpd)
    endif()
endmacro()

macro(use_package_quazip)
    if (WIN32)
        if ("$ENV{QUAZIP_DIR}" STREQUAL "")
           set(QUAZIP_DIR ${ENV_NAALI_DEP_PATH})
        endif()
        include_directories(${QUAZIP_DIR}/include)
        link_directories(${QUAZIP_DIR}/lib) # VC10 deps way, no subfolders, dynamic
        link_directories(${QUAZIP_DIR}/lib/quazip_debug_Win32) # VC9 deps way, in subfolder, static
        link_directories(${QUAZIP_DIR}/lib/quazip_release_Win32)
    else() # Linux, note: mac will also come here..
        if ("$ENV{QUAZIP_DIR}" STREQUAL "")
           set(QUAZIP_DIR ${ENV_NAALI_DEP_PATH})
        endif()
        include_directories(${QUAZIP_DIR}/include/)
        link_directories(${QUAZIP_DIR}/lib)
    endif()
endmacro()

macro(link_package_quazip)
    target_link_libraries(${TARGET_NAME} optimized quazip)
    if (WIN32)
        target_link_libraries(${TARGET_NAME} debug quazipd)
    endif()
endmacro()

macro(configure_package_opencv)
    # \todo Dont do this on linux systems
    SET(OPENCV_ROOT ${ENV_NAALI_DEP_PATH}/OpenCV-2.2.0)

    # Include directories to add to the user project:
    SET(OpenCV_INCLUDE_DIR ${OPENCV_ROOT}/include)
    SET(OpenCV_INCLUDE_DIRS ${OPENCV_ROOT}/include ${OPENCV_ROOT}/include/opencv)
    INCLUDE_DIRECTORIES(${OpenCV_INCLUDE_DIRS})

    # Link directories to add to the user project:
    SET(OpenCV_LIB_DIR ${OPENCV_ROOT}/lib)
    LINK_DIRECTORIES(${OpenCV_LIB_DIR})

    # Link to libraries, todo modify the list, we dont need all this stuff
    # Full list of available libs from opencv are listed below. If you need there to our dependencies please contact Jonne Nauha aka Pforce on IRC #realxtend-dev @ freenode
    #set(OPENCV_LIB_COMPONENTS opencv_core opencv_imgproc opencv_features2d opencv_gpu opencv_calib3d opencv_objdetect opencv_video opencv_highgui opencv_ml opencv_legacy opencv_contrib opencv_flann)
    set(OPENCV_LIB_COMPONENTS opencv_core opencv_highgui)
    SET(OpenCV_LIBS "")
    foreach(__CVLIB ${OPENCV_LIB_COMPONENTS})
        # CMake>=2.6 supports the notation "debug XXd optimized XX"
        if (CMAKE_MAJOR_VERSION GREATER 2  OR  CMAKE_MINOR_VERSION GREATER 4)
            # Modern CMake:
            SET(OpenCV_LIBS ${OpenCV_LIBS} debug ${__CVLIB}220d optimized ${__CVLIB}220)
        else(CMAKE_MAJOR_VERSION GREATER 2  OR  CMAKE_MINOR_VERSION GREATER 4)
            # Old CMake:
            SET(OpenCV_LIBS ${OpenCV_LIBS} ${__CVLIB}220)
        endif(CMAKE_MAJOR_VERSION GREATER 2  OR  CMAKE_MINOR_VERSION GREATER 4)
    endforeach(__CVLIB)

    foreach(__CVLIB ${OPENCV_LIB_COMPONENTS})
        # We only need the "core",... part here: "opencv_core" -> "core"
        STRING(REGEX REPLACE "opencv_(.*)" "\\1" MODNAME ${__CVLIB})
        # \todo This wont work on linux as win deps ship all the headers in OPENCV_ROOT/include
        # see how the headers are in /urs/include and implement the else() part here
        if (WIN32)
            INCLUDE_DIRECTORIES(${OpenCV_INCLUDE_DIR}/${MODNAME})
        else()
            # \todo Impl linux module folder includes
            # something like INCLUDE_DIRECTORIES("/usr/include/opencv/${MODNAME}/include") ??
        endif()
    endforeach(__CVLIB)

    # Link to the 3rdparty libs as well
    if(WIN32)
        LINK_DIRECTORIES(${OPENCV_ROOT}/3rdparty/lib)
    else()
        # \todo Verify this works in linux
        LINK_DIRECTORIES(${OPENCV_ROOT}/share/opencv/3rdparty/lib)
    endif()    

    set(OpenCV_LIBS comctl32;gdi32;ole32;vfw32 ${OpenCV_LIBS})
    set(OPENCV_EXTRA_COMPONENTS libjpeg libpng libtiff libjasper zlib opencv_lapack)

    if (CMAKE_MAJOR_VERSION GREATER 2  OR  CMAKE_MINOR_VERSION GREATER 4)
        foreach(__EXTRA_LIB ${OPENCV_EXTRA_COMPONENTS})
            set(OpenCV_LIBS ${OpenCV_LIBS}
                debug ${__EXTRA_LIB}d
                optimized ${__EXTRA_LIB})
        endforeach(__EXTRA_LIB)
    else(CMAKE_MAJOR_VERSION GREATER 2  OR  CMAKE_MINOR_VERSION GREATER 4)
        set(OpenCV_LIBS ${OpenCV_LIBS} ${OPENCV_EXTRA_COMPONENTS})
    endif(CMAKE_MAJOR_VERSION GREATER 2  OR  CMAKE_MINOR_VERSION GREATER 4)
endmacro()

macro(link_package_opencv)
    TARGET_LINK_LIBRARIES(${TARGET_NAME} ${OpenCV_LIBS})
endmacro()

macro(use_package_qtmobility)
        message (STATUS "** Configuring QtMobility")
        
        # Use the mobility.prf for determining QtMobility installation location
        # \todo Verify this works in Windows
        if(EXISTS "${QT4_MKSPECS}/features/mobility.prf")
        
            message (STATUS "-- Using mkspecs file")
            message (STATUS "       " ${QT4_MKSPECS}/features/mobility.prf)
        
            file (READ ${QT_MKSPECS_DIR}/features/mobility.prf MOBILITY_CONFIG_FILE)
            
            string( REGEX MATCH "MOBILITY_PREFIX=([^\n]+)" QTMOBILITY_PREFIX "${MOBILITY_CONFIG_FILE}")
            set (QTMOBILITY_PREFIX ${CMAKE_MATCH_1})
            
            string( REGEX MATCH "MOBILITY_INCLUDE=([^\n]+)" QTMOBILITY_INCLUDE "${MOBILITY_CONFIG_FILE}")
            set (QTMOBILITY_INCLUDE ${CMAKE_MATCH_1})
            
            string( REGEX MATCH "MOBILITY_LIB=([^\n]+)" QTMOBILITY_LIBRARY "${MOBILITY_CONFIG_FILE}")
            set (QTMOBILITY_LIBRARY ${CMAKE_MATCH_1})
            
            message (STATUS "-- Include Directories:")
            
            message (STATUS "       " ${QTMOBILITY_INCLUDE}/QtPublishSubscribe)
            include_directories(${QTMOBILITY_INCLUDE}/QtPublishSubscribe)
            message (STATUS "       " ${QTMOBILITY_INCLUDE}/QtLocation)
            include_directories(${QTMOBILITY_INCLUDE}/QtLocation)
            message (STATUS "       " ${QTMOBILITY_INCLUDE}/QtServiceFramework)
            include_directories(${QTMOBILITY_INCLUDE}/QtServiceFramework)
            message (STATUS "       " ${QTMOBILITY_INCLUDE}/QtSystemInfo)
            include_directories(${QTMOBILITY_INCLUDE}/QtSystemInfo)
            message (STATUS "       " ${QTMOBILITY_INCLUDE}/QtMultimediaKit)
            include_directories(${QTMOBILITY_INCLUDE}/QtMultimediaKit)
            message (STATUS "       " ${QTMOBILITY_INCLUDE}/QtContacts)
            include_directories(${QTMOBILITY_INCLUDE}/QtContacts)
            message (STATUS "       " ${QTMOBILITY_INCLUDE}/QtVersit)
            include_directories(${QTMOBILITY_INCLUDE}/QtVersit)
            message (STATUS "       " ${QTMOBILITY_INCLUDE}/QtSensors)
            include_directories(${QTMOBILITY_INCLUDE}/QtSensors)
            message (STATUS "       " ${QTMOBILITY_INCLUDE}/QtMobility)
            include_directories(${QTMOBILITY_INCLUDE}/QtMobility)
            
            message (STATUS "-- Library Directories:")
            
            message (STATUS "       " ${QTMOBILITY_LIBRARY})
            link_directories(${QTMOBILITY_LIBRARY})
            
            message (STATUS "")
            
        else ()
            
            # mkspecs file for QtMobility not found, halt.
            message (FATAL_ERROR "!! Unable to locate QtMobility mkspecs file")
            
        endif ()
endmacro()

macro(link_package_qtmobility)
    target_link_libraries(${TARGET_NAME} optimized QtPublishSubscribe optimized QtLocation
                          optimized QtServiceFramework optimized QtSystemInfo optimized QtMultimediaKit
                          optimized QtContacts optimized QtVersit optimized QtSensors)
endmacro()

macro(use_package_qtdeclarative)
    message (STATUS "** Configuring QtDeclarative")
    if (${QT_QTDECLARATIVE_FOUND})
        message (STATUS "-- Include Directories:")
        message (STATUS "       " ${QT_QTDECLARATIVE_INCLUDE_DIR})
        include_directories(${QT_QTDECLARATIVE_INCLUDE_DIR})
        message (STATUS "-- Libries:")
        message (STATUS "       " ${QT_QTDECLARATIVE_LIBRARY})
        message (STATUS "")
    else ()
        message (FATAL_ERROR "-- QtDeclarative not found when configuting Qt!")
    endif ()
endmacro()

macro(link_package_qtdeclarative)
    if (${QT_QTDECLARATIVE_FOUND})
        target_link_libraries(${TARGET_NAME} ${QT_QTDECLARATIVE_LIBRARY})
    endif ()
endmacro()

macro (configure_kinect)
    if (WIN32 AND VC100)
        message (STATUS "")
        message (STATUS "** Configuring KINECT")
        
        # If your kinect sdk is not found set KINECT_ROOT to your installation
        set (_KINECT_FIND_PATHS 
            "$ENV{KINECT_ROOT}/inc"
            "$ENV{KINECT_ROOT}/lib"
            "C:/Program Files (x86)/Microsoft*Kinect*/inc"
            "C:/Program Files (x86)/Microsoft*Kinect*/lib"
            "C:/Program Files/Microsoft*Kinect*/inc"
            "C:/Program Files/Microsoft*Kinect*/lib"
        )
        
        find_path (KINECT_INCLUDE_DIRS MSR_NuiApi.h PATHS ${_KINECT_FIND_PATHS})   
        find_path (KINECT_LIBRARY_DIRS MSRKinectNUI.lib PATHS ${_KINECT_FIND_PATHS})
        find_library (KINECT_LIBRARIES MSRKinectNUI PATHS ${_KINECT_FIND_PATHS})

        if (${KINECT_INCLUDE_DIRS} STREQUAL "KINECT_INCLUDE_DIRS-NOTFOUND" OR
            ${KINECT_LIBRARY_DIRS} STREQUAL "KINECT_LIBRARY_DIRS-NOTFOUND" OR
            ${KINECT_LIBRARIES} STREQUAL "KINECT_LIBRARIES-NOTFOUND")
            message (WARNING "-- Could not automatically find Microsoft Kinect SDK, set KINECT_ROOT to your install directory.")
        endif ()
        
        message (STATUS "")
        sagase_configure_report (KINECT)
    endif ()    
endmacro ()
