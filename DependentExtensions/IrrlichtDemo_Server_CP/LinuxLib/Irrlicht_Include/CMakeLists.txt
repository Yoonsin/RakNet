cmake_minimum_required(VERSION 3.10)
project(Irrlicht VERSION 1.7.2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Version components
set(VERSION_MAJOR 1)
set(VERSION_MINOR 7)
set(VERSION_PATCH 2)

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)


# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/zlib
    ${CMAKE_SOURCE_DIR}/jpeglib
    ${CMAKE_SOURCE_DIR}/libpng
)


# OpenGL 찾기
find_package(OpenGL REQUIRED)

# OpenGL 포함 디렉토리 추가
include_directories(${OPENGL_INCLUDE_DIR})

# X11 찾기
find_package(X11 REQUIRED)

# Xrandr 라이브러리 링크
#find_package(Xrandr REQUIRED)
include_directories(${X11_INCLUDE_DIR})

# Compiler definitions and flags
add_compile_definitions(IRRLICHT_EXPORTS=1)

if(CMAKE_COMPILER_IS_GNUCXX)
    add_compile_options(-fno-exceptions -fno-rtti -fstrict-aliasing)
elseif(CMAKE_COMPILER_IS_GNUCC)
    add_compile_options(-Wall -pipe -fno-exceptions -fstrict-aliasing)
endif()


if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_definitions(_DEBUG)
    add_compile_options(-g)
else()
    add_compile_options(-fexpensive-optimizations -O3)
endif()

# Collect source files
#file(GLOB_RECURSE IRRMESHLOADER src/CBSPMeshFileLoader.cpp src/CMD2MeshFileLoader.cpp ...)
#file(GLOB_RECURSE IRRMESHWRITER src/CColladaMeshWriter.cpp src/CIrrMeshWriter.cpp ...)
#file(GLOB_RECURSE IRROBJ src/CBillboardSceneNode.cpp src/CCameraSceneNode.cpp ...)
#file(GLOB_RECURSE IRRPARTICLEOBJ src/CParticle*.cpp)
#file(GLOB_RECURSE IRRANIMOBJ src/CSceneNodeAnimator*.cpp)
#file(GLOB_RECURSE IRRDRVROBJ src/CNullDriver.cpp src/COpenGL*.cpp ...)
#file(GLOB_RECURSE IRRIMAGEOBJ src/CColorConverter.cpp src/CImage*.cpp ...)
#file(GLOB_RECURSE IRRVIDEOOBJ src/CVideoModeList.cpp src/CFPSCounter.cpp ${IRRDRVROBJ} ${IRRIMAGEOBJ})
#file(GLOB_RECURSE IRRSWRENDEROBJ src/CSoftware*.cpp src/CZBuffer.cpp ...)
#file(GLOB_RECURSE IRRIOOBJ src/CFileList.cpp src/CFileSystem.cpp ...)
#file(GLOB_RECURSE IRROTHEROBJ src/CIrrDevice*.cpp src/CLogger.cpp ...)
#file(GLOB_RECURSE IRRGUIOBJ src/CGUI*.cpp src/CDefaultGUIElementFactory.cpp ...)

file(GLOB_RECURSE IRR_SOURCES 
    burning_shader_color.cpp 
    C3DSMeshFileLoader.cpp 
    CAnimatedMeshHalfLife.cpp 
    CAnimatedMeshMD2.cpp 
    CAnimatedMeshMD3.cpp 
    CAnimatedMeshSceneNode.cpp 
    CAttributes.cpp 
    CB3DMeshFileLoader.cpp 
    CB3DMeshWriter.cpp 
    CBillboardSceneNode.cpp 
    CBoneSceneNode.cpp 
    CBSPMeshFileLoader.cpp 
    CBurningShader_Raster_Reference.cpp 
    CCameraSceneNode.cpp 
    CColladaFileLoader.cpp 
    CColladaMeshWriter.cpp 
    CColorConverter.cpp 
    CCSMLoader.cpp 
    CCubeSceneNode.cpp 
    CD3D9Driver.cpp 
    CD3D9HLSLMaterialRenderer.cpp 
    CD3D9NormalMapRenderer.cpp 
    CD3D9ParallaxMapRenderer.cpp 
    CD3D9ShaderMaterialRenderer.cpp 
    CD3D9Texture.cpp 
    CDefaultGUIElementFactory.cpp 
    CDefaultSceneNodeAnimatorFactory.cpp 
    CDefaultSceneNodeFactory.cpp 
    CDepthBuffer.cpp 
    CDMFLoader.cpp 
    CDummyTransformationSceneNode.cpp 
    CEmptySceneNode.cpp 
    CFileList.cpp 
    CFileSystem.cpp 
    CFPSCounter.cpp 
    leakHunter.cpp 
    CGeometryCreator.cpp 
    CGUIButton.cpp 
    CGUICheckBox.cpp 
    CGUIColorSelectDialog.cpp 
    CGUIComboBox.cpp 
    CGUIContextMenu.cpp 
    CGUIEditBox.cpp 
    CGUIEnvironment.cpp 
    CGUIFileOpenDialog.cpp 
    CGUIFont.cpp 
    CGUIImage.cpp 
    CGUIImageList.cpp 
    CGUIInOutFader.cpp 
    CGUIListBox.cpp 
    CGUIMenu.cpp 
    CGUIMeshViewer.cpp 
    CGUIMessageBox.cpp 
    CGUIModalScreen.cpp 
    CGUIScrollBar.cpp 
    CGUISkin.cpp 
    CGUISpinBox.cpp 
    CGUISpriteBank.cpp 
    CGUIStaticText.cpp 
    CGUITabControl.cpp 
    CGUITable.cpp 
    CGUIToolBar.cpp 
    CGUITreeView.cpp 
    CGUIWindow.cpp 
    CGUIProfiler.cpp 
    CImage.cpp 
    CImageLoaderBMP.cpp 
    CImageLoaderDDS.cpp 
    CImageLoaderJPG.cpp 
    CImageLoaderPCX.cpp 
    CImageLoaderPNG.cpp 
    CImageLoaderPPM.cpp 
    CImageLoaderPSD.cpp 
    CImageLoaderRGB.cpp 
    CImageLoaderTGA.cpp 
    CImageLoaderWAL.cpp 
    CImageWriterBMP.cpp 
    CImageWriterJPG.cpp 
    CImageWriterPCX.cpp 
    CImageWriterPNG.cpp 
    CImageWriterPPM.cpp 
    CImageWriterPSD.cpp 
    CImageWriterTGA.cpp 
    CImageLoaderPVR.cpp 
    CIrrDeviceConsole.cpp 
    CIrrDeviceFB.cpp 
    CIrrDeviceLinux.cpp 
    CIrrDeviceSDL.cpp 
    CIrrDeviceStub.cpp 
    CIrrDeviceWin32.cpp 
    CIrrMeshFileLoader.cpp 
    CIrrMeshWriter.cpp 
    CLightSceneNode.cpp 
    CLimitReadFile.cpp 
    CLMTSMeshFileLoader.cpp 
    CLogger.cpp 
    CLWOMeshFileLoader.cpp 
    CMD2MeshFileLoader.cpp 
    CMD3MeshFileLoader.cpp 
    CMemoryFile.cpp 
    CMeshCache.cpp 
    CMeshManipulator.cpp 
    CMeshSceneNode.cpp 
    CMeshTextureLoader.cpp 
    CMetaTriangleSelector.cpp 
    CMountPointReader.cpp 
    CMS3DMeshFileLoader.cpp 
    CMY3DMeshFileLoader.cpp 
    CNPKReader.cpp 
    CNullDriver.cpp 
    COBJMeshFileLoader.cpp 
    COBJMeshWriter.cpp 
    COCTLoader.cpp 
    COctreeSceneNode.cpp 
    COctreeTriangleSelector.cpp 
    CEGLManager.cpp 
    COGLES2Driver.cpp 
    COGLES2ExtensionHandler.cpp 
    COGLES2MaterialRenderer.cpp 
    COGLES2FixedPipelineRenderer.cpp 
    COGLES2NormalMapRenderer.cpp 
    COGLES2ParallaxMapRenderer.cpp 
    COGLES2Renderer2D.cpp 
    COGLESDriver.cpp 
    COGLESExtensionHandler.cpp 
    COgreMeshFileLoader.cpp 
    COpenGLCacheHandler.cpp 
    COpenGLDriver.cpp 
    COpenGLExtensionHandler.cpp 
    COpenGLNormalMapRenderer.cpp 
    COpenGLParallaxMapRenderer.cpp 
    COpenGLShaderMaterialRenderer.cpp 
    COpenGLSLMaterialRenderer.cpp 
    COSOperator.cpp 
    CPakReader.cpp 
    CParticleAnimatedMeshSceneNodeEmitter.cpp 
    CParticleAttractionAffector.cpp 
    CParticleBoxEmitter.cpp 
    CParticleCylinderEmitter.cpp 
    CParticleFadeOutAffector.cpp 
    CParticleGravityAffector.cpp 
    CParticleMeshEmitter.cpp 
    CParticlePointEmitter.cpp 
    CParticleRingEmitter.cpp 
    CParticleRotationAffector.cpp 
    CParticleScaleAffector.cpp 
    CParticleSphereEmitter.cpp 
    CParticleSystemSceneNode.cpp 
    CPLYMeshFileLoader.cpp 
    CPLYMeshWriter.cpp 
    CProfiler.cpp 
    CQ3LevelMesh.cpp 
    CQuake3ShaderSceneNode.cpp 
    CReadFile.cpp 
    CSceneCollisionManager.cpp 
    CSceneLoaderIrr.cpp 
    CSceneManager.cpp 
    CSceneNodeAnimatorCameraFPS.cpp 
    CSceneNodeAnimatorCameraMaya.cpp 
    CSceneNodeAnimatorCollisionResponse.cpp 
    CSceneNodeAnimatorDelete.cpp 
    CSceneNodeAnimatorFlyCircle.cpp 
    CSceneNodeAnimatorFlyStraight.cpp 
    CSceneNodeAnimatorFollowSpline.cpp 
    CSceneNodeAnimatorRotation.cpp 
    CSceneNodeAnimatorTexture.cpp 
    CShadowVolumeSceneNode.cpp 
    CSkinnedMesh.cpp 
    CSkyBoxSceneNode.cpp 
    CSkyDomeSceneNode.cpp 
    CSMFMeshFileLoader.cpp 
    CSoftwareDriver.cpp 
    CSoftwareDriver2.cpp 
    CSoftwareTexture.cpp 
    CSoftwareTexture2.cpp 
    CSphereSceneNode.cpp 
    CSTLMeshFileLoader.cpp 
    CSTLMeshWriter.cpp 
    CTarReader.cpp 
    CTerrainSceneNode.cpp 
    CTerrainTriangleSelector.cpp 
    CTextSceneNode.cpp 
    CTRFlat.cpp 
    CTRFlatWire.cpp 
    CTRGouraud.cpp 
    CTRGouraud2.cpp 
    CTRGouraudAlphaNoZ2.cpp 
    CTRGouraudWire.cpp 
    CTriangleBBSelector.cpp 
    CTriangleSelector.cpp 
    CTRGouraudNoZ2.cpp 
    CTRNormalMap.cpp 
    CTRParallaxMap.cpp 
    CTRStencilShadow.cpp 
    CTRTextureBlend.cpp 
    CTRTextureDetailMap2.cpp 
    CTRTextureFlat.cpp 
    CTRTextureFlatWire.cpp 
    CTRTextureGouraud.cpp 
    CTRTextureGouraud2.cpp 
    CTRTextureGouraudAdd.cpp 
    CTRTextureGouraudAdd2.cpp 
    CTRTextureGouraudAddNoZ2.cpp 
    CTRTextureGouraudAlpha.cpp 
    CTRTextureGouraudAlphaNoZ.cpp 
    CTRTextureGouraudNoZ.cpp 
    CTRTextureGouraudNoZ2.cpp 
    CTRTextureGouraudVertexAlpha2.cpp 
    CTRTextureGouraudWire.cpp 
    CTRTextureLightMap2_Add.cpp 
    CTRTextureLightMap2_M1.cpp 
    CTRTextureLightMap2_M2.cpp 
    CTRTextureLightMap2_M4.cpp 
    CTRTextureLightMapGouraud2_M4.cpp 
    CTRTextureWire2.cpp 
    CTR_transparent_reflection_2_layer.cpp 
    CVideoModeList.cpp 
    CVolumeLightSceneNode.cpp 
    CWADReader.cpp 
    CWaterSurfaceSceneNode.cpp 
    CWriteFile.cpp 
    CXMeshFileLoader.cpp 
    CXMLReader.cpp 
    CXMLWriter.cpp 
    CZBuffer.cpp 
    CZipReader.cpp 
    IBurningShader.cpp 
    Irrlicht.cpp 
    irrXML.cpp 
    os.cpp
)

file(GLOB_RECURSE ZLIBOBJ zlib/adler32.c  zlib/crc32.c    zlib/gzclose.c  zlib/gzread.c   zlib/infback.c  zlib/inflate.c   zlib/trees.c    zlib/zutil.c
zlib/compress.c  zlib/deflate.c  zlib/gzlib.c    zlib/gzwrite.c  zlib/inffast.c  zlib/inftrees.c  zlib/uncompr.c)

# Exclude jmemdos.c and jmemmac.c from JPEGLIBOBJ for Linux
if(NOT UNIX)
    file(GLOB_RECURSE JPEGLIBOBJ jpeglib/*.c)
else()
    # On Linux, exclude jmemdos.c and jmemmac.c
    file(GLOB_RECURSE JPEGLIBOBJ jpeglib/jaricom.c 
					jpeglib/jcapimin.c 
					jpeglib/jcapistd.c 
					jpeglib/jcarith.c 
					jpeglib/jccoefct.c 
					jpeglib/jccolor.c 
					jpeglib/jcdctmgr.c 
					jpeglib/jchuff.c 
					jpeglib/jcinit.c 
					jpeglib/jcmainct.c
					jpeglib/jcmarker.c 
					jpeglib/jcmaster.c 
					jpeglib/jcomapi.c 
					jpeglib/jcparam.c 
					jpeglib/jcprepct.c 
					jpeglib/jcsample.c 
					jpeglib/jctrans.c 
					jpeglib/jdapimin.c 
					jpeglib/jdapistd.c 
					jpeglib/jdarith.c 
					jpeglib/jdatadst.c 
					jpeglib/jdatasrc.c 
					jpeglib/jdcoefct.c 
					jpeglib/jdcolor.c 
					jpeglib/jddctmgr.c 
					jpeglib/jdhuff.c 
					jpeglib/jdinput.c 
					jpeglib/jdmainct.c 
					jpeglib/jdmarker.c 
					jpeglib/jdmaster.c 
					jpeglib/jdmerge.c 
					jpeglib/jdpostct.c 
					jpeglib/jdsample.c 
					jpeglib/jdtrans.c 
					jpeglib/jerror.c 
					jpeglib/jfdctflt.c 
					jpeglib/jfdctfst.c 
					jpeglib/jfdctint.c 
					jpeglib/jidctflt.c 
					jpeglib/jidctfst.c 
					jpeglib/jidctint.c 
					jpeglib/jmemmgr.c 
					jpeglib/jmemnobs.c 
					jpeglib/jquant1.c 
					jpeglib/jquant2.c 
					jpeglib/jutils.c )
    
endif()

file(GLOB_RECURSE  LIBPNGOBJ libpng/png.c 
					libpng/pngerror.c 
					libpng/pngget.c 
					libpng/pngmem.c 
					libpng/pngpread.c 
					libpng/pngread.c 
					libpng/pngrio.c 
					libpng/pngrtran.c 
					libpng/pngrutil.c 
					libpng/pngset.c 
					libpng/pngtrans.c 
					libpng/pngwio.c 
					libpng/pngwrite.c 
					libpng/pngwtran.c 
					libpng/pngwutil.c )

file(GLOB_RECURSE LIBAESGM aesGladman/aescrypt.cpp 
					aesGladman/aeskey.cpp 
					aesGladman/aestab.cpp 
					aesGladman/fileenc.cpp 
					aesGladman/hmac.cpp 
					aesGladman/prng.cpp 
					aesGladman/pwd2key.cpp 
					aesGladman/sha1.cpp 
					aesGladman/sha2.cpp )

file(GLOB_RECURSE BZIP2OBJ bzip2/blocksort.c 
					bzip2/bzcompress.c 
					bzip2/bzlib.c 
					bzip2/crctable.c 
					bzip2/decompress.c 
					bzip2/huffman.c 
					bzip2/randtable.c )

# Platform-specific settings for Windows
if(WIN32)
    add_compile_definitions(USE_SETMODE)
endif()


set(LINKOBJ
   ${IRR_SOURCES} ${ZLIBOBJ} ${JPEGLIBOBJ} ${LIBPNGOBJ} ${LIBAESGM} ${BZIP2OBJ}
)


# Build static library
add_library(Irrlicht STATIC ${LINKOBJ})

#Xrandr
target_link_libraries(Irrlicht 
${OPENGL_LIBRARIES} 
${X11_LIBRARIES} 

)

# Build shared library (optional)
option(BUILD_SHARED "Build shared library" OFF)
if(BUILD_SHARED)
    add_library(IrrlichtShared SHARED ${LINKOBJ})
    set_target_properties(IrrlichtShared PROPERTIES OUTPUT_NAME "Irrlicht")
    target_link_libraries(IrrlichtShared GL Xxf86vm)
endif()

# Install headers and libraries (optional)
#install(DIRECTORY ../../include/ DESTINATION include/irrlicht FILES_MATCHING PATTERN "*.h")
#install(TARGETS Irrlicht ARCHIVE DESTINATION lib)
#if(BUILD_SHARED)
 #   install(TARGETS IrrlichtShared LIBRARY DESTINATION lib)
#endif()
