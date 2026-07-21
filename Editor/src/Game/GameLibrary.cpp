#include "Game/GameLibrary.h"

#include <FXEngine/Core/Log.h>
#include <FXEngine/Scene/Scene.h>
#include <FXEngine/Scene/Entity.h>
#include <FXEngine/Scene/Components.h>
#include <FXEngine/Scene/ScriptRegistry.h>

#include <filesystem>

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace FXEd
{
    namespace
    {
        using RegisterFn = void (*)();
        using SelfTestFn = int  (*)(FX::Scene*, unsigned long long);
    }

    GameLibrary::~GameLibrary()
    {
        Unload();
    }

    void GameLibrary::Unload()
    {
        // Sira B-5'te (Play koruması) onemli olacak; simdilik kayitlari
        // once temizlemek dogru: bir sonraki Load temiz bir tablo bekler.
        FX::ScriptRegistry::Clear();

        if (m_Handle)
        {
            ::FreeLibrary(static_cast<HMODULE>(m_Handle));
            m_Handle = nullptr;
        }
    }

    bool GameLibrary::Load(const std::string& dllPath, std::string* error)
    {
        Unload();

        namespace fs = std::filesystem;

        if (!fs::exists(dllPath))
        {
            if (error)
                *error = "Game.dll yok: " + dllPath;
            return false;
        }

        // GOLGE KOPYA (B-4): Windows yuklu bir DLL'i kilitler; uzerine
        // yeniden derlenemez. Bir kopyasini yukleyip orijinali serbest
        // birakiyoruz. Kopya ayni ADLA bir alt klasorde: MSVC .dll icine
        // gomdugu pdb yolunu "Game.pdb" olarak yazar; ayni adla kopyalarsak
        // debugger sembolleri bulur (VS'ten editore attach calisir).
        std::string loadPath = dllPath;
        {
            const fs::path original{ dllPath };
            const fs::path shadowDir = original.parent_path() / "loaded";

            std::error_code ec;
            fs::create_directories(shadowDir, ec);

            const fs::path shadowDll = shadowDir / original.filename();
            fs::copy_file(original, shadowDll,
                          fs::copy_options::overwrite_existing, ec);
            if (ec)
            {
                if (error)
                    *error = "Golge kopya basarisiz: " + ec.message();
                return false;
            }

            // Sembol dosyasi varsa o da: breakpoint'ler icin.
            const fs::path pdb = fs::path{ original }.replace_extension(".pdb");
            if (fs::exists(pdb))
            {
                std::error_code pec;
                fs::copy_file(pdb, shadowDir / pdb.filename(),
                              fs::copy_options::overwrite_existing, pec);
            }

            m_ShadowPath = shadowDll.string();
            loadPath = m_ShadowPath;
        }

        HMODULE handle = ::LoadLibraryA(loadPath.c_str());
        if (!handle)
        {
            if (error)
                *error = "LoadLibrary basarisiz (kod "
                       + std::to_string(::GetLastError()) + "): " + loadPath;
            return false;
        }

        auto registerFn = reinterpret_cast<RegisterFn>(
            ::GetProcAddress(handle, "FXRegisterScripts"));
        if (!registerFn)
        {
            ::FreeLibrary(handle);
            if (error)
                *error = "FXRegisterScripts sembolu bulunamadi.";
            return false;
        }

        m_Handle = handle;
        registerFn();
        return true;
    }

    bool GameLibrary::RunSelfTest(std::string* detail)
    {
        if (!m_Handle)
        {
            if (detail)
                *detail = "DLL yuklu degil.";
            return false;
        }

        auto selfTest = reinterpret_cast<SelfTestFn>(
            ::GetProcAddress(static_cast<HMODULE>(m_Handle), "FXEngineSelfTest"));
        if (!selfTest)
        {
            if (detail)
                *detail = "FXEngineSelfTest sembolu yok (eski DLL?).";
            return false;
        }

        // Editor bir entity + TransformComponent(x=42) yaratir, DLL'e
        // gonderir; DLL 42 gorup 99 yazmali. Bu tek cagri hem tip kimligini
        // (get<TransformComponent> dogru storage'i buluyor mu) hem de ortak
        // bellek erisimini (ayni heap) olcer.
        FX::Scene probe;
        FX::Entity e = probe.CreateEntity("__fx_selftest");
        // CreateEntity TransformComponent'i zaten ekliyor; mevcut olana yaz.
        e.AddOrReplaceIfMissing<FX::TransformComponent>().Translation.x = 42.0f;

        const unsigned long long uuid =
            static_cast<std::uint64_t>(e.GetUUID());

        const int result = selfTest(&probe, uuid);
        const float readBack = e.GetComponent<FX::TransformComponent>().Translation.x;

        const bool ok = (result == 1) && (readBack == 99.0f);
        if (detail)
        {
            *detail = ok
                ? "EnTT tip kimligi ve veri erisimi DLL sinirinda dogrulandi."
                : ("UYUSMADI (donus=" + std::to_string(result)
                   + ", geri okunan x=" + std::to_string(readBack) + ").");
        }
        return ok;
    }
}
