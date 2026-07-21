#include "Game/GameProject.h"

#include <FXEngine/Core/Log.h>
#include <FXEngine/Core/Project.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace FXEd::GameProject
{
    namespace
    {
        std::string Root()
        {
            if (!FX::Project::HasActive())
                return {};
            return fs::path{ FX::Project::GetActive()->GetDirectory() }
                       .generic_string();
        }

        std::string Join(const std::string& root, const char* tail)
        {
            if (root.empty())
                return {};
            return (fs::path{ root } / tail).generic_string();
        }

        // Ayni icerik varsa dokunma: her yazma dosya zaman damgasini
        // degistirir, CMake de bunu "yeniden configure et" diye okur.
        bool WriteIfChanged(const fs::path& path, const std::string& content,
                            std::string* error)
        {
            std::error_code ec;
            if (fs::exists(path))
            {
                std::ifstream in{ path, std::ios::binary };
                std::stringstream ss;
                ss << in.rdbuf();
                if (ss.str() == content)
                    return true;
            }

            fs::create_directories(path.parent_path(), ec);

            std::ofstream out{ path, std::ios::binary | std::ios::trunc };
            if (!out)
            {
                if (error)
                    *error = "Yazilamadi: " + path.generic_string();
                return false;
            }
            out << content;
            return true;
        }

        const char* kCMakeLists = R"FXCMK(# ===========================================================================
# URETILEN DOSYA - FXEditor tarafindan yazildi (Asama B-2).
# Elle duzenleme: editor bir sonraki acilista ustune yazar.
#
# Oyun kodu ayri bir DLL. Sebep: script'i degistirip editoru kapatmadan
# yeniden yuklemek. Motor (FXEngine.dll) surec genelinde TEK.
# ===========================================================================
cmake_minimum_required(VERSION 3.20)
project(FXGame LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8 /MP)
endif()

set(FXGAME_SCRIPTS_DIR "@SCRIPTS_DIR@")
set(FXGAME_OUT_DIR     "@OUT_DIR@")

# Motorun include yollari ve import kutuphanesi. Bu dosyayi motor kendi
# hedefinden URETIYOR (Engine/CMakeLists.txt) - burada elle yazilsaydi
# ikinci bir kopya olur ve ayrisirdi.
include("@ENGINE_CONFIG@")

# CONFIGURE_DEPENDS: yeni bir script dosyasi eklendiginde derleme
# sirasinda klasor yeniden taraniyor. GLOB'un normalde tehlikeli olma
# sebebi (yeni dosyayi fark etmemesi) boylece ortadan kalkiyor.
file(GLOB FXGAME_SCRIPT_HEADERS CONFIGURE_DEPENDS "${FXGAME_SCRIPTS_DIR}/*.h")

set(FX_SCRIPT_INCLUDES "")
set(FX_SCRIPT_REGISTRATIONS "")

foreach(script_header ${FXGAME_SCRIPT_HEADERS})
    get_filename_component(script_name ${script_header} NAME_WE)
    string(APPEND FX_SCRIPT_INCLUDES "#include \"${script_header}\"\n")
    string(APPEND FX_SCRIPT_REGISTRATIONS
           "        FX::ScriptRegistry::Register<FXGame::${script_name}>(\"${script_name}\");\n")
endforeach()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/GameRegistrations.h.in"
               "${CMAKE_CURRENT_BINARY_DIR}/generated/GameRegistrations.h"
               @ONLY)

add_library(Game SHARED GameMain.cpp ${FXGAME_SCRIPT_HEADERS})

target_include_directories(Game PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(Game PRIVATE FXEngine)

# Cikti tek yerde dursun; multi-config generator'lar config alt klasoru
# eklemesin - editor DLL'i sabit bir yolda ariyor.
set_target_properties(Game PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${FXGAME_OUT_DIR}")
foreach(cfg IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(Game PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${cfg} "${FXGAME_OUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_${cfg} "${FXGAME_OUT_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY_${cfg} "${FXGAME_OUT_DIR}")
endforeach()
)FXCMK";

        const char* kRegistrationsTemplate = R"FXH(#pragma once

// URETILEN DOSYA - .fxbuild/CMakeLists.txt tarafindan uretiliyor.
//
// KURAL: assets/scripts/ altindaki her `<Ad>.h` icinde `FXGame::<Ad>`
// adinda bir FX::ScriptableEntity turevi bulunmali. Kayit satirini elle
// yazmak zorunda kalsaydik unutmanin cezasi SESSIZ olurdu: script
// Inspector listesinde hic gorunmezdi.

@FX_SCRIPT_INCLUDES@
#include <FXEngine/Scene/ScriptRegistry.h>

namespace FXGame
{
    inline void RegisterScripts()
    {
@FX_SCRIPT_REGISTRATIONS@    }
}
)FXH";

        const char* kGameMain = R"FXCPP(// URETILEN DOSYA - FXEditor tarafindan yazildi (Asama B-2/B-3).

#include <GameRegistrations.h>

#include <FXEngine/Scene/Scene.h>
#include <FXEngine/Scene/Entity.h>
#include <FXEngine/Scene/Components.h>

// Editorun DLL'de aradigi semboller. Isim bozulmasin diye extern "C":
// GetProcAddress'e C++ mangling'i ile ad vermek derleyiciye bagimli olurdu.

extern "C" __declspec(dllexport) void FXRegisterScripts()
{
    FXGame::RegisterScripts();
}

// EnTT tip kimliginin DLL sinirinda tuttugunu ve DLL'in editorun yarattigi
// component verisine ERISEBILDIGINI olcer (Asama B en ciddi riski). Editor
// bir entity + TransformComponent (x=42) yaratip buraya gonderir; bu kod
// component'i DLL tarafindan okur, 42 gorurse 99 yazar. Editor 99'u geri
// okuyabiliyorsa gidis-donus calisiyor demektir.
extern "C" __declspec(dllexport) int FXEngineSelfTest(FX::Scene* scene,
                                                      unsigned long long uuid)
{
    if (!scene)
        return 0;
    FX::Entity e = scene->FindEntityByUUID(uuid);
    if (!e || !e.HasComponent<FX::TransformComponent>())
        return 0;
    auto& t = e.GetComponent<FX::TransformComponent>();
    if (t.Translation.x != 42.0f)
        return 0;
    t.Translation.x = 99.0f;
    return 1;
}
)FXCPP";

        std::string Substitute(std::string text, const char* key,
                               const std::string& value)
        {
            for (std::size_t at = text.find(key); at != std::string::npos;
                 at = text.find(key, at + value.size()))
                text.replace(at, std::strlen(key), value);
            return text;
        }
    }

    std::string ScriptsDir()     { return Join(Root(), "assets/scripts"); }
    std::string BuildDir()       { return Join(Root(), ".fxbuild"); }
    std::string CMakeBinaryDir() { return Join(Root(), ".fxbuild/cmake"); }
    std::string DllPath()        { return Join(Root(), ".fxbuild/out/Game.dll"); }

    bool WriteBuildFiles(std::string* error)
    {
        const std::string root = Root();
        if (root.empty())
        {
            if (error)
                *error = "Acik proje yok.";
            return false;
        }

        std::error_code ec;
        fs::create_directories(ScriptsDir(), ec);

        const std::string engineConfig =
            std::string{ FX_ENGINE_BUILD_DIR } + "/FXEngineConfig-"
            + FX_BUILD_CONFIG + ".cmake";

        if (!fs::exists(engineConfig))
        {
            if (error)
                *error = "Motor tarifi bulunamadi: " + engineConfig;
            return false;
        }

        std::string cmakeLists = kCMakeLists;
        cmakeLists = Substitute(cmakeLists, "@SCRIPTS_DIR@", ScriptsDir());
        cmakeLists = Substitute(cmakeLists, "@OUT_DIR@", Join(root, ".fxbuild/out"));
        cmakeLists = Substitute(cmakeLists, "@ENGINE_CONFIG@", engineConfig);

        const fs::path build{ BuildDir() };

        return WriteIfChanged(build / "CMakeLists.txt", cmakeLists, error)
            && WriteIfChanged(build / "GameRegistrations.h.in",
                              kRegistrationsTemplate, error)
            && WriteIfChanged(build / "GameMain.cpp", kGameMain, error);
    }
}
