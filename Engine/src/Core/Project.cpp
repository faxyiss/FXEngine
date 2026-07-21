#include "FXEngine/Core/Project.h"
#include "FXEngine/Core/FileSystem.h"
#include "FXEngine/Core/Log.h"
#include "FXEngine/Asset/AssetManager.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iomanip>

namespace FX
{
    using json = nlohmann::json;

    std::shared_ptr<Project> Project::s_Active;

    namespace
    {
        // Surum 2: StartScene artik yol degil GUID (0.6). Surum 1
        // dosyalari aciliyor ve ilk acilista sessizce donusturuluyor.
        // Surum 3: TargetWidth/TargetHeight eklendi. Eski dosyalar
        // varsayilani aliyor - yeni ALAN eklemek surum artirmazdi ama
        // Game View'in davranisi buna bagli oldugu icin surumu
        // isaretliyoruz.
        constexpr int kProjectVersion = 3;

        std::string WithTrailingSlash(std::string p)
        {
            if (!p.empty() && p.back() != '/' && p.back() != '\\')
                p += '/';
            return p;
        }
    }

    void Project::SetActive(const std::shared_ptr<Project>& project)
    {
        s_Active = project;

        // Proje ile FileSystem'in koku BIRLIKTE degismeli. Ayri ayri
        // yonetilseydi ikisi kacinilmaz olarak birbirinden ayrisirdi ve
        // "sahne acildi ama texture'lari bulunamiyor" gibi izi zor
        // surulen hatalar cikardi.
        FileSystem::SetProjectDirectory(project ? project->m_Directory : "");
    }

    void Project::CloseActive()
    {
        s_Active.reset();
        FileSystem::SetProjectDirectory("");

        // Tablo eski projenin yollarini tutuyor; birakmak "GUID var ama
        // dosya baska projede" durumuna yol acardi.
        AssetManager::Clear();
    }

    std::shared_ptr<Project> Project::Load(const std::string& filepath)
    {
        std::ifstream in(filepath);
        if (!in)
        {
            FX_CORE_ERROR("Proje acilamadi: %s", filepath.c_str());
            return nullptr;
        }

        json j;
        try
        {
            in >> j;
        }
        catch (const std::exception& e)
        {
            FX_CORE_ERROR("Proje dosyasi bozuk: %s", filepath.c_str());
            FX_CORE_ERROR("  %s", e.what());
            return nullptr;
        }

        auto project = std::shared_ptr<Project>(new Project());

        project->m_FilePath = filepath;

        // KOK = .fxproject'in bulundugu klasor. Dosyanin icinde bir kok
        // yolu SAKLAMIYORUZ: saklasaydik proje tasindiginda o yol
        // yanlis olurdu. Konumu dosyanin kendi yeri soyluyor.
        project->m_Directory = WithTrailingSlash(
            std::filesystem::path(filepath).parent_path().generic_string());

        auto& cfg = project->m_Config;
        cfg.Name           = j.value("Name", std::string("Isimsiz Proje"));
        cfg.AssetDirectory = j.value("AssetDirectory", std::string("assets"));

        if (j.contains("TargetResolution") && j["TargetResolution"].is_array() &&
            j["TargetResolution"].size() >= 2)
        {
            const auto& r = j["TargetResolution"];
            cfg.TargetWidth  = r[0].get<std::uint32_t>();
            cfg.TargetHeight = r[1].get<std::uint32_t>();
        }

        // Sifir bolme ve anlamsiz oran uretmesin.
        if (cfg.TargetWidth  == 0) cfg.TargetWidth  = 1920;
        if (cfg.TargetHeight == 0) cfg.TargetHeight = 1080;
        // Surum 1'de StartScene bir YOLDU, surum 2'de GUID. Tipe BAKARAK
        // ayiriyoruz: dogrudan value<uint64_t> cagirmak eski dosyalarda
        // istisna atiyor ve proje hic acilmiyordu.
        std::string legacyStartScene;

        if (j.contains("StartScene"))
        {
            const auto& node = j["StartScene"];
            if (node.is_string())
                legacyStartScene = node.get<std::string>();
            else if (node.is_number_unsigned())
                cfg.StartScene = AssetHandle{ node.get<std::uint64_t>() };
        }

        const int version = j.value("Version", 0);
        if (version > kProjectVersion)
            FX_CORE_WARN("Proje surumu %d (beklenen %d) - yine de aciliyor.",
                         version, kProjectVersion);

        SetActive(project);

        // Tarama SetActive'den SONRA: AssetManager yollari aktif projenin
        // kokune gore cozuyor.
        AssetManager::ScanProject();

        if (!legacyStartScene.empty())
        {
            cfg.StartScene = AssetManager::GetHandle(legacyStartScene);

            if (cfg.StartScene.IsValid())
            {
                FX_CORE_INFO("StartScene GUID'e cevrildi: %s", legacyStartScene.c_str());
                project->Save();
            }
            else
            {
                FX_CORE_WARN("StartScene bulunamadi, temizlendi: %s",
                             legacyStartScene.c_str());
            }
        }

        FX_CORE_INFO("Proje acildi: '%s'", cfg.Name.c_str());
        FX_CORE_INFO("  kok    : %s", project->m_Directory.c_str());
        FX_CORE_INFO("  varlik : %s", cfg.AssetDirectory.c_str());

        return project;
    }

    std::shared_ptr<Project> Project::Create(const std::string& directory,
                                             const std::string& name)
    {
        std::error_code ec;
        const auto root = std::filesystem::path(directory);

        std::filesystem::create_directories(root, ec);
        if (ec)
        {
            FX_CORE_ERROR("Proje klasoru olusturulamadi: %s (%s)",
                          directory.c_str(), ec.message().c_str());
            return nullptr;
        }

        auto project = std::shared_ptr<Project>(new Project());
        project->m_Directory     = WithTrailingSlash(root.generic_string());
        project->m_Config.Name   = name;
        project->m_FilePath      = project->m_Directory + name + kExtension;

        // Bos bir projede bile bu klasorler olsun: kullanici "nereye
        // koyacagim?" diye dusunmesin. Bos bir klasor agaci, bos bir
        // klasorden cok daha ogreticidir.
        for (const char* sub : { "assets", "assets/textures",
                                 "assets/scenes", "assets/prefabs" })
        {
            std::filesystem::create_directories(root / sub, ec);
        }

        if (!project->Save())
            return nullptr;

        SetActive(project);
        AssetManager::ScanProject();

        FX_CORE_INFO("Proje olusturuldu: '%s' -> %s",
                     name.c_str(), project->m_FilePath.c_str());

        return project;
    }

    bool Project::Save() const
    {
        json j;
        j["Version"]        = kProjectVersion;
        j["Name"]           = m_Config.Name;
        j["AssetDirectory"] = m_Config.AssetDirectory;
        j["StartScene"]     = static_cast<std::uint64_t>(m_Config.StartScene);
        j["TargetResolution"] = nlohmann::json::array({ m_Config.TargetWidth,
                                                        m_Config.TargetHeight });

        std::ofstream out(m_FilePath);
        if (!out)
        {
            FX_CORE_ERROR("Proje yazilamadi: %s", m_FilePath.c_str());
            return false;
        }

        out << std::setw(4) << j << std::endl;
        return true;
    }
}
