project(wii-banner-player)

include_directories(Externals/dolphin-emu/Source)

find_package(SFML 2.5 COMPONENTS graphics audio REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(Threads REQUIRED)

set(CMAKE_CXX_FLAGS "-Wall -std=c++0x")

add_executable(wii-banner-player 

Externals/dolphin-emu/Source/Blob.cpp
Externals/dolphin-emu/Source/CDUtils.cpp
#Externals/dolphin-emu/Source/CompressedBlob.cpp
Externals/dolphin-emu/Source/FileBlob.cpp
Externals/dolphin-emu/Source/FileHandlerARC.cpp
#Externals/dolphin-emu/Source/Filesystem.cpp
#Externals/dolphin-emu/Source/FileUtil.cpp
Externals/dolphin-emu/Source/TextureDecoder.cpp

Source/Animator.cpp
Source/Banner.cpp
Source/Endian.cpp
Source/Font.cpp
Source/Layout.cpp
Source/Main.cpp
Source/Material.cpp
Source/Pane.cpp
Source/Picture.cpp
Source/Sound.cpp
Source/Textbox.cpp
Source/Texture.cpp
Source/Window.cpp
Source/WrapGx.cpp
)

target_link_libraries(wii-banner-player sfml-graphics sfml-audio ${OPENGL_LIBRARIES} ${GLEW_LIBRARIES} Threads::Threads)
