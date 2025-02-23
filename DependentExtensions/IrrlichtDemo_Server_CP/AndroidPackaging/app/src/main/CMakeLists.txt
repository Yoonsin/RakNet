cmake_minimum_required(VERSION 3.10)  # 최소 CMake 버전 설정
project(IrrlichtDemo_Server_CP)  # 🔹 프로젝트 이름 설정

set(CMAKE_SYSTEM_NAME Android)
set(CMAKE_SYSTEM_VERSION 30)  # Android API 레벨
set(CMAKE_ANDROID_ARCH_ABI "x86_64")  # 사용할 ABI

# 추가적으로 사용되지 않는 변수를 올바르게 설정
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

 
# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds it for you.
# Gradle automatically packages shared libraries with your APK.
add_library( # Sets the name of the library.
             app-glue
 
             # Sets the library as a shared library.
             STATIC
 
             # Provides a relative path to your source file(s).
             # Associated headers in the same location as their source
             # file are automatically included.
            # C:/android-sdk/ndk/23.2.8568313/sources/android/native_app_glue/android_native_app_glue.c
C:/GitHub/RakNet/DependentExtensions/IrrlichtDemo_Server_CP/AndroidPackaging/app/src/main/cpp/android_native_app_glue.c )
 

# now build app's shared lib
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")

# Export ANativeActivity_onCreate(),
# Refer to: https://github.com/android-ndk/ndk/issues/381.
set(CMAKE_SHARED_LINKER_FLAGS
    "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")


add_library( # Sets the name of the library.
             native-lib
 
             # Sets the library as a shared library.
             SHARED
 
             # Provides a relative path to your source file(s).
             # Associated headers in the same location as their source
             # file are automatically included.
             C:/GitHub/RakNet/DependentExtensions/IrrlichtDemo_Server_CP/CDemo.cpp
             C:/GitHub/RakNet/DependentExtensions/IrrlichtDemo_Server_CP/CMainMenu.cpp
              C:/GitHub/RakNet/DependentExtensions/IrrlichtDemo_Server_CP/RakNetStuff.cpp
             C:/GitHub/RakNet/DependentExtensions/IrrlichtDemo_Server_CP/AndroidPackaging/app/src/main/cpp/AndroidDemo.cpp
             C:/GitHub/RakNet/DependentExtensions/IrrlichtDemo_Server_CP/AndroidPackaging/app/src/main/cpp/android_tools.cpp
)

# Specifies a path to native header files.
#add_library에 소스파일, include_directories에 헤더파일 경로 추가
#include_directories(C:/android-sdk/ndk/23.2.8568313/sources/android/native_app_glue/)
include_directories(C:/irrlicht-ogl-es/include/)
include_directories(C:/GitHub/RakNet/DependentExtensions/IrrlichtDemo_Server_CP/)
include_directories(C:/GitHub/RakNet/Source/)
include_directories(C:/GitHub/RakNet/DependentExtensions/IrrlichtDemo_Server_CP/AndroidPackaging/app/src/main/cpp/)

#>>>    NOTE: The above should be YOUR Irrlicht include directory. This is mine.
#>>>          The path starts in the "app" directory of your project.
 
# Searches for a specified prebuilt library and stores the path as a
# variable. Because system libraries are included in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.
 
find_library( # Sets the name of the path variable.
              log-lib
 
              # Specifies the name of the NDK library that
              # you want CMake to locate.
              log )
 
# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in the
# build script, prebuilt third-party libraries, or system libraries.
 
target_link_libraries( 
                       native-lib
                       app-glue
                       android
                       EGL
                       log
                       GLESv1_CM
                       GLESv2
                       z
                       m
                       ${log-lib}
                       C:/irrlicht-ogl-es/source/Irrlicht/Android/obj/local/${CMAKE_ANDROID_ARCH_ABI}/libIrrlicht.a
                       C:/GitHub/RakNet/Lib/LibStatic/Debug/Android-x86_64/x86_64/libRakNet_VS2008.so
)
#>>>    NOTE: Just like above, this should be your path. This one is mine.
#>>>          Links to the appropriate ABI. CAREFUL: You must have >ALL< Irrlicht ABIs built ("arm64-v8a", "armeabi", "armeabi-v7a", "mips64", "x86", "x86_64") otherwise you may run into installation errors on your virtual device.