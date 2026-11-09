cmake_minimum_required(VERSION 3.20)
project(SoundKey_AfterEffects_Plugin LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ścieżka do Adobe After Effects SDK (przekazywana przez parametr lub zmienną środowiskową)
if(NOT DEFINED AE_SDK_PATH)
    set(AE_SDK_PATH "$ENV{AESDK_ROOT}" CACHE PATH "Path to Adobe After Effects SDK Root")
endif()

message(STATUS "--------------------------------------------------------")
message(STATUS "Konfiguracja SoundKey Plugin C++20")
message(STATUS "Ścieżka After Effects SDK: ${AE_SDK_PATH}")
message(STATUS "Kompilator: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "--------------------------------------------------------")

include_directories(
    ${AE_SDK_PATH}/Examples/Headers
    ${AE_SDK_PATH}/Examples/Headers/SP
    ${AE_SDK_PATH}/Examples/Util
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set(SOURCE_FILES
    src/SoundKey_Plugin.cpp
    src/SoundKey_AEGP.cpp
    src/SoundKey_Plugin.h
    src/SoundKey_AEGP.h
    src/SoundKey_AudioEngine.h
    ${AE_SDK_PATH}/Examples/Util/AEGP_SuiteHandler.cpp
    ${AE_SDK_PATH}/Examples/Util/MissingSuiteError.cpp
)

if(WIN32)
    # Konfiguracja Windows x64: Kompilacja biblioteki współdzielonej .aex
    add_library(SoundKey SHARED ${SOURCE_FILES} res/SoundKey_PiPL.rc)
    
    set_target_properties(SoundKey PROPERTIES
        SUFFIX ".aex"
        PREFIX ""
        COMPILE_DEFINITIONS "MSWindows;WIN32;_WINDOWS;_USRDLL;AE_OS_WIN"
    )
    
    target_compile_options(SoundKey PRIVATE 
        /W3 /O2 /EHsc /fp:fast /permissive- /std:c++20
    )

    message(STATUS "Docelowy plik: SoundKey.aex (Windows 64-bit)")

elseif(APPLE)
    # Konfiguracja macOS: Universal Binary Bundle (.plugin dla arm64 + x86_64)
    add_library(SoundKey MODULE ${SOURCE_FILES})
    
    set_target_properties(SoundKey PROPERTIES
        BUNDLE TRUE
        BUNDLE_EXTENSION "plugin"
        OUTPUT_NAME "SoundKey"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.soundkey.native"
        OSX_ARCHITECTURES "arm64;x86_64"
        COMPILE_DEFINITIONS "AE_OS_MAC"
    )
    
    target_compile_options(SoundKey PRIVATE -O3 -Wall -std=c++20)

    message(STATUS "Docelowy plik: SoundKey.plugin (macOS Universal Binary)")
endif()
