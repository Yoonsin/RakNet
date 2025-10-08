cmake_minimum_required(VERSION 3.10)

project(IrrlichtDemo_Server_CP)



# X11과 OpenGL 포함 디렉토리 추가
include_directories(
    ${CMAKE_CURRENT_LIST_DIR}/LinuxLib/Irrlicht_Include
    ${CMAKE_CURRENT_LIST_DIR}/LinuxLib/RakNet_Include
    ${CMAKE_CURRENT_LIST_DIR}
    ${OPENGL_INCLUDE_DIR}
    ${X11_INCLUDE_DIR}
    
)

message(STATUS "${X11_INCLUDE_DIR}")
# Compiler definitions and flags
add_compile_definitions(IRRLICHT_EXPORTS=1)

if(CMAKE_COMPILER_IS_GNUCXX)
    add_compile_options(-fno-exceptions -fstrict-aliasing)
elseif(CMAKE_COMPILER_IS_GNUCC)
    add_compile_options(-Wall -pipe -fno-exceptions -fstrict-aliasing)
endif()

if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_definitions(_DEBUG)
    add_compile_options(-g)
else()
    add_compile_options(-fexpensive-optimizations -O3)
endif()

# 소스 파일들
file(GLOB IRR_SOURCES
    CDemo.cpp
    CMainMenu.cpp
    RakNetStuff.cpp
    main.cpp
    shared_mem.cpp
)

# 실행 파일 만들기
add_executable(IrrlichtDemo_Server_CP ${IRR_SOURCES})

# OpenGL과 X11 라이브러리 찾기
find_package(X11 REQUIRED)
set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)

target_link_libraries(
    IrrlichtDemo_Server_CP

    ${CMAKE_CURRENT_LIST_DIR}/LinuxLib/libIrrlicht.a

    ${X11_LIBRARIES}
    ${OPENGL_LIBRARIES}
    X11
    Xext
    Xrandr
    GL
    GLX

    ${CMAKE_CURRENT_LIST_DIR}/LinuxLib/libRakNetLibStatic.a
    ${CMAKE_CURRENT_LIST_DIR}/LinuxLib/libRakNetDLL.so
)
