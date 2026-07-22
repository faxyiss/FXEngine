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
# Secenekler
# ---------------------------------------------------------------------------
# Testler VARSAYILAN OLARAK ACIK ama kapatilabilir: CI'da ve gelistirirken
# calissinlar, motoru sadece derlemek isteyen biri Catch2 indirmek
# zorunda kalmasin.
option(FXENGINE_BUILD_TESTS "Birim testlerini derle (Catch2)" ON)

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

if(FXENGINE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()

# ---------------------------------------------------------------------------
# Kurulum (cmake --install) - tasinabilir dagitim agaci
# ---------------------------------------------------------------------------
# Iki bolum:
#   bin/  calisan editor: exe + FXEngine.dll + SDL3.dll + assets
#   sdk/  oyun projelerinin Game.dll derlemesi icin gerekenler: motor
#         basliklari, header-only bagimliliklar, import lib ve TASINABILIR
#         motor tarifi. Editor calisma zamaninda once exe/../sdk/'ya bakar
#         (GameProject.cpp), bulamazsa gelistirme build agacina duser.
#
# Hedef makinede yine de CMake + MSVC Build Tools gerekir: script'ler C++
# ve Game.dll orada derleniyor (Unreal'in Visual Studio sartiyla ayni durum).
install(TARGETS FXEditor RUNTIME DESTINATION bin)
install(TARGETS FXEngine
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION sdk/lib)
install(FILES $<TARGET_FILE:SDL3::SDL3> DESTINATION bin)
install(DIRECTORY Editor/assets/ DESTINATION bin/assets)

install(DIRECTORY Engine/include/ DESTINATION sdk/include)
install(FILES cmake/FXEngineConfigInstalled.cmake
    DESTINATION sdk RENAME FXEngineConfig.cmake)

# Motor basliklarinin icerdigi header-only bagimliliklar. Yalnizca baslik
# dosyalari kopyalanir (test/CMake artiklari dagitima girmesin).
install(DIRECTORY ${glm_SOURCE_DIR}/glm DESTINATION sdk/third_party/glm
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h" PATTERN "*.inl")
install(DIRECTORY ${entt_SOURCE_DIR}/src/entt DESTINATION sdk/third_party/entt
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")
if(EXISTS "${nlohmann_json_SOURCE_DIR}/single_include/nlohmann/json_fwd.hpp")
    install(DIRECTORY ${nlohmann_json_SOURCE_DIR}/single_include/nlohmann
        DESTINATION sdk/third_party/json FILES_MATCHING PATTERN "*.hpp")
else()
    install(DIRECTORY ${nlohmann_json_SOURCE_DIR}/include/nlohmann
        DESTINATION sdk/third_party/json FILES_MATCHING PATTERN "*.hpp")
endif()

# MSVC calisma zamani DLL'leri exe'nin yanina: hedef makine vcredist
# kurmak zorunda kalmasin (yalnizca Release calisma zamani dagitilabilir).
set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION bin)
include(InstallRequiredSystemLibraries)

# ---------------------------------------------------------------------------
# CPack - dagitim paketi (varsayilan ZIP; NSIS kuruluysa setup.exe de olur)
# ---------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME "FXEngine")
set(CPACK_PACKAGE_VENDOR "FXEngine")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "FXEngine")
set(CPACK_GENERATOR "ZIP")
set(CPACK_NSIS_DISPLAY_NAME "FXEngine Editor")
set(CPACK_NSIS_EXECUTABLES_DIRECTORY "bin")
set(CPACK_PACKAGE_EXECUTABLES "FXEditor" "FXEngine Editor")
include(CPack)
