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
	${CMAKE_SOURCE_DIR}/lzma
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

file(GLOB IRR_SOURCES 
    *.cpp
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

file(GLOB_RECURSE LZMAOBJ 
lzma/LzmaDec.c)

# Platform-specific settings for Windows
if(WIN32)
    add_compile_definitions(USE_SETMODE)
endif()


set(LINKOBJ
   ${IRR_SOURCES} ${ZLIBOBJ} ${JPEGLIBOBJ} ${LIBPNGOBJ} ${LIBAESGM} ${BZIP2OBJ} ${LZMAOBJ}
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
