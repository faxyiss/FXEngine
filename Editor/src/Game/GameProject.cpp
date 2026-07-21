#include "Game/GameProject.h"

#include <FXEngine/Core/Log.h>
#include <FXEngine/Core/Project.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

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

set(FXGAME_ASSETS_DIR "@ASSETS_DIR@")
set(FXGAME_OUT_DIR    "@OUT_DIR@")

# Motorun include yollari ve import kutuphanesi. Bu dosyayi motor kendi
# hedefinden URETIYOR (Engine/CMakeLists.txt) - burada elle yazilsaydi
# ikinci bir kopya olur ve ayrisirdi.
include("@ENGINE_CONFIG@")

# GLOB_RECURSE: assets/ AGACININ TAMAMINDAKI her .h bir script. Boylece
# kullanici script'lerini icerik panelinde istedigi klasorde tutabilir,
# hepsi tek Game.dll'e derlenir. CONFIGURE_DEPENDS: yeni dosya eklenince
# derleme sirasinda yeniden taraniyor (GLOB'un "yeni dosyayi fark etmeme"
# tehlikesi boylece ortadan kalkiyor).
#
# Konvansiyon: assets/ altindaki her .h, FXGame::<DosyaAdi> adinda bir
# FX::ScriptableEntity turevi icermeli - aksi halde derleme hata verir.
file(GLOB_RECURSE FXGAME_SCRIPT_HEADERS CONFIGURE_DEPENDS "${FXGAME_ASSETS_DIR}/*.h")

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

#include <FXEngine/Core/EngineABI.h>
#include <FXEngine/Scene/Scene.h>
#include <FXEngine/Scene/Entity.h>
#include <FXEngine/Scene/Components.h>

// Editorun DLL'de aradigi semboller. Isim bozulmasin diye extern "C":
// GetProcAddress'e C++ mangling'i ile ad vermek derleyiciye bagimli olurdu.

// Bu DLL'in HANGI motor surumuyle derlendigi. Derleme zamaninda gomulur;
// editor yuklemeden once kendi surumuyle karsilastirir. Uyusmazsa
// yuklemez: eski bir DLL'in script'ini calistirmak vtable uyusmazligi
// yuzunden cokme demek.
extern "C" __declspec(dllexport) int FXEngineABIVersion()
{
    return FX_ENGINE_ABI_VERSION;
}

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

    // Projenin varlik klasoru (script taramasi bunun ALTINI ozyinelemeli
    // gezerek yapiliyor - script'ler herhangi bir alt klasorde olabilir).
    static std::string AssetsDir()
    {
        if (!FX::Project::HasActive())
            return {};
        return Join(Root(), FX::Project::GetActive()->GetConfig().AssetDirectory.c_str());
    }

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
        cmakeLists = Substitute(cmakeLists, "@ASSETS_DIR@", AssetsDir());
        cmakeLists = Substitute(cmakeLists, "@OUT_DIR@", Join(root, ".fxbuild/out"));
        cmakeLists = Substitute(cmakeLists, "@ENGINE_CONFIG@", engineConfig);

        const fs::path build{ BuildDir() };

        return WriteIfChanged(build / "CMakeLists.txt", cmakeLists, error)
            && WriteIfChanged(build / "GameRegistrations.h.in",
                              kRegistrationsTemplate, error)
            && WriteIfChanged(build / "GameMain.cpp", kGameMain, error);
    }

    namespace
    {
        // Bir komutu calistirir, stdout+stderr'i output'a toplar, cikis
        // kodunu dondurur. CREATE_NO_WINDOW: derleme sirasinda konsol
        // penceresi acilip kapanmasin (editor pencereli bir uygulama).
        int RunCommand(const std::string& cmdLine, std::string& output)
        {
            SECURITY_ATTRIBUTES sa{};
            sa.nLength = sizeof(sa);
            sa.bInheritHandle = TRUE;

            HANDLE readPipe = nullptr, writePipe = nullptr;
            if (!::CreatePipe(&readPipe, &writePipe, &sa, 0))
            {
                output += "\n[editor] CreatePipe basarisiz.\n";
                return -1;
            }
            // Okuma ucunu cocuk MIRAS ALMASIN: aksi halde cocuk hic
            // kapanmadigi icin ReadFile EOF gormez ve kilitleniriz.
            ::SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

            STARTUPINFOA si{};
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESTDHANDLES;
            si.hStdOutput = writePipe;
            si.hStdError  = writePipe;
            si.hStdInput  = ::GetStdHandle(STD_INPUT_HANDLE);

            PROCESS_INFORMATION pi{};

            // CreateProcessA komut satirini DEGISTIREBILIR; kopya veriyoruz.
            std::string mutableCmd = cmdLine;
            const BOOL ok = ::CreateProcessA(
                nullptr, mutableCmd.data(), nullptr, nullptr, TRUE,
                CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);

            // Yazma ucunu ebeveyn kapatir: yoksa ReadFile hic EOF gormez.
            ::CloseHandle(writePipe);

            if (!ok)
            {
                ::CloseHandle(readPipe);
                output += "\n[editor] cmake calistirilamadi (PATH'te mi?).\n";
                return -1;
            }

            char buf[4096];
            DWORD read = 0;
            while (::ReadFile(readPipe, buf, sizeof(buf) - 1, &read, nullptr) && read > 0)
                output.append(buf, read);
            ::CloseHandle(readPipe);

            ::WaitForSingleObject(pi.hProcess, INFINITE);
            DWORD code = 1;
            ::GetExitCodeProcess(pi.hProcess, &code);
            ::CloseHandle(pi.hProcess);
            ::CloseHandle(pi.hThread);
            return static_cast<int>(code);
        }
    }

    int Build(const std::string& config, std::string& output)
    {
        output.clear();

        std::string writeErr;
        if (!WriteBuildFiles(&writeErr))
        {
            output = "Iskele kurulamadi: " + writeErr + "\n";
            return -1;
        }

        const std::string src   = BuildDir();
        const std::string bin   = CMakeBinaryDir();
        const std::string quotedSrc = "\"" + src + "\"";
        const std::string quotedBin = "\"" + bin + "\"";

        // Ilk kez ya da cache silinmisse configure gerekiyor.
        if (!fs::exists(fs::path{ bin } / "CMakeCache.txt"))
        {
            output += "> cmake configure...\n";
            const int cfg = RunCommand(
                "cmake -S " + quotedSrc + " -B " + quotedBin, output);
            if (cfg != 0)
            {
                output += "\nConfigure basarisiz (kod " + std::to_string(cfg) + ").\n";
                return cfg;
            }
            output += "\n";
        }

        output += "> cmake --build (" + config + ")...\n";
        const int rc = RunCommand(
            "cmake --build " + quotedBin + " --config " + config, output);

        output += (rc == 0)
            ? "\nDerleme basarili.\n"
            : "\nDerleme BASARISIZ (kod " + std::to_string(rc) + ").\n";
        return rc;
    }
}
