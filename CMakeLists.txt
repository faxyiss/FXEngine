cmake_minimum_required(VERSION 3.20)

project(FXEngine
    VERSION 0.1.0
    DESCRIPTION "2D Game Engine + Editor"
    LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Global ayarlar
# ---------------------------------------------------------------------------
# C++17'yi "zorunlu" kiliyoruz. EXTENSIONS OFF => derleyiciye ozel eklentiler
# (GNU/MSVC uzantilari) kapali, boylece kod gercekten tasinabilir kalir.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Tum ciktilar (exe, dll, lib) tek yerde toplansin. Ozellikle Windows'ta
# SDL3.dll'in exe ile ayni klasorde olmasi gerekir; bu ayar o derdi cozer.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Visual Studio / Xcode gibi "multi-config" generator'larda yukaridaki
# degiskenler config alt klasoru ekler (bin/Debug, bin/Release). Onu duzlestir.
foreach(cfg IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${cfg} "${CMAKE_BINARY_DIR}/bin")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${cfg} "${CMAKE_BINARY_DIR}/bin")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${cfg} "${CMAKE_BINARY_DIR}/lib")
endforeach()

# compile_commands.json: clangd / VSCode C++ eklentisi bunu okur, IntelliSense
# ve "go to definition" duzgun calisir. Bedava kazanc.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    # /utf-8  : kaynak ve calisma zamani kodlamasi UTF-8 (uyari selini keser)
    # /MP     : cok cekirdekli derleme
    add_compile_options(/utf-8 /MP)
    # Visual Studio'yu actiginda dogrudan FXEditor calissin.
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT FXEditor)
endif()

# ---------------------------------------------------------------------------
# Bagimliliklar
# ---------------------------------------------------------------------------
include(cmake/Dependencies.cmake)

# ---------------------------------------------------------------------------
# Alt projeler
# ---------------------------------------------------------------------------
# SIRA ONEMLI: Editor, Engine target'ina link olacagi icin Engine once tanimlanmali.
add_subdirectory(Engine)
add_subdirectory(Editor)
