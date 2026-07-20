#include "FXEngine/Core/Project.h"
#include "FXEngine/Core/FileSystem.h"
#include "FXEngine/Core/Log.h"

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
        constexpr int kProjectVersion = 1;

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
        cfg.StartScene     = j.value("StartScene", std::string());

        const int version = j.value("Version", 0);
        if (version != kProjectVersion)
            FX_CORE_WARN("Proje surumu %d (beklenen %d) - yine de aciliyor.",
                         version, kProjectVersion);

        SetActive(project);

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
        j["StartScene"]     = m_Config.StartScene;

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
