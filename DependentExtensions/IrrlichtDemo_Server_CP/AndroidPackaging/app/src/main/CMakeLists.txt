cmake_minimum_required(VERSION 3.10)  # 최소 CMake 버전 설정
project(IrrlichtDemo_Server_CP)  # 🔹 프로젝트 이름 설정

set(CMAKE_SYSTEM_NAME Android)
set(CMAKE_SYSTEM_VERSION 30)  # Android API 레벨
set(CMAKE_ANDROID_ARCH_ABI "arm64-v8a" )  # 사용할 ABI #arm64-v8a #x86_64

# 추가적으로 사용되지 않는 변수를 올바르게 설정
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 프로젝트 루트 경로 설정
set(IRRLICHT_PROJECT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(ASSETS_DIR ${IRRLICHT_PROJECT_PATH}/assets)

add_library( # Sets the name of the library.
             app-glue
 
             # Sets the library as a shared library.
             STATIC
             
             C:/GitHub/RakNet/DependentExtensions/IrrlichtDemo_Server_CP/AndroidPackaging/app/src/main/cpp/android_native_app_glue.c )
 
# now build app's shared lib
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions -frtti")

# Export ANativeActivity_onCreate(),
# Refer to: https://github.com/android-ndk/ndk/issues/381.
set(CMAKE_SHARED_LINKER_FLAGS
    "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

add_library( # Sets the name of the library.
             native-lib
 
             # Sets the library as a shared library.
             SHARED
 
             C:/GitHub/RakNet/DependentExtensions/IrrlichtDemo_Server_CP/CInGame.cpp
             C:/GitHub/RakNet/DependentExtensions/IrrlichtDemo_Server_CP/CMainMenu.cpp
             C:/GitHub/RakNet/DependentExtensions/IrrlichtDemo_Server_CP/CollisionManager.cpp
             C:/GitHub/RakNet/DependentExtensions/IrrlichtDemo_Server_CP/HUDManager.cpp
             C:/GitHub/RakNet/DependentExtensions/IrrlichtDemo_Server_CP/InputController.cpp
             C:/GitHub/RakNet/DependentExtensions/IrrlichtDemo_Server_CP/MethodManager.cpp
             C:/GitHub/RakNet/DependentExtensions/IrrlichtDemo_Server_CP/NetLogManager.cpp
             C:/GitHub/RakNet/DependentExtensions/IrrlichtDemo_Server_CP/NetworkManager.cpp
             C:/GitHub/RakNet/DependentExtensions/IrrlichtDemo_Server_CP/PacketHandler.cpp
             C:/GitHub/RakNet/DependentExtensions/IrrlichtDemo_Server_CP/Replicas.cpp
             C:/GitHub/RakNet/DependentExtensions/IrrlichtDemo_Server_CP/SceneManager.cpp
             C:/GitHub/RakNet/DependentExtensions/IrrlichtDemo_Server_CP/AndroidPackaging/app/src/main/cpp/AndroidDemo.cpp
             C:/GitHub/RakNet/DependentExtensions/IrrlichtDemo_Server_CP/AndroidPackaging/app/src/main/cpp/android_tools.cpp
)

# Specifies a path to native header files.
#add_library에 소스파일, include_directories에 헤더파일 경로 추가
include_directories(C:/GitHub/CommonLibraries/Irrlicht/include/)
include_directories(C:/GitHub/CommonLibraries/IrrlichtExtensions/)
include_directories(C:/GitHub/CommonLibraries/Common/)
include_directories(C:/GitHub/RakNet/DependentExtensions/IrrlichtDemo_Server_CP/)
include_directories(C:/GitHub/RakNet/Source/)
include_directories(C:/GitHub/RakNet/DependentExtensions/IrrlichtDemo_Server_CP/AndroidPackaging/app/src/main/cpp/)

find_library( # Sets the name of the path variable.
              log-lib       
 
              # Specifies the name of the NDK library that
              # you want CMake to locate.
              log )

target_link_libraries(  
                       native-lib
                       C:/GitHub/CommonLibraries/Irrlicht/source/Irrlicht/Android/obj/local/${CMAKE_ANDROID_ARCH_ABI}/libIrrlicht.a
                       C:/GitHub/CommonLibraries/Common/Android/obj/local/${CMAKE_ANDROID_ARCH_ABI}/libCommon.a
                       C:/GitHub/CommonLibraries/IrrlichtExtensions/android/obj/local/${CMAKE_ANDROID_ARCH_ABI}/libIrrlichtExtension.a
                       C:/GitHub/CommonLibraries/IrrlichtExtensions/android/obj/local/${CMAKE_ANDROID_ARCH_ABI}/libandroid_native_app_glue.a
                       app-glue
                       android
                       EGL
                       log
                       GLESv1_CM
                       GLESv2
                       z
                       m
                       ${log-lib}       
                       C:/GitHub/RakNet/Lib/LibStatic/Debug/Android-${CMAKE_ANDROID_ARCH_ABI}/${CMAKE_ANDROID_ARCH_ABI}/libRakNet_VS2008.so
)
