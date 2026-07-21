#pragma once

// ===========================================================================
// Testler icin ortak yardimcilar.
//
// Cogu testin bir AKTIF PROJEYE ihtiyaci var: AssetManager ve
// SceneSerializer yollari FileSystem::ResolveProjectAsset ile cozuyor ve
// o da aktif projeye bakiyor. Gercek dosya sistemi kullaniyoruz, sahte
// (mock) bir katman yazmiyoruz - test edilen sey zaten "diske dogru
// yaziyor muyuz".
// ===========================================================================

#include <FXEngine/Core/Project.h>

#include <filesystem>
#include <random>
#include <string>

namespace FXTest
{
    // Gecici klasorde bir proje kurar, yikilirken siler.
    class TempProject
    {
    public:
        explicit TempProject(const std::string& name = "TestProject")
        {
            std::random_device rd;
            m_Dir = std::filesystem::temp_directory_path() /
                    ("fxtest_" + std::to_string(rd()));

            FX::Project::Create(m_Dir.string(), name);
        }

        ~TempProject()
        {
            FX::Project::CloseActive();

            std::error_code ec;
            std::filesystem::remove_all(m_Dir, ec);
        }

        TempProject(const TempProject&)            = delete;
        TempProject& operator=(const TempProject&) = delete;

        const std::filesystem::path& Dir() const { return m_Dir; }

        std::filesystem::path Assets() const { return m_Dir / "assets"; }

        // Proje kokune goreceli yolun tam karsiligi ("assets/scenes/a.fxscene").
        std::filesystem::path Path(const std::string& relativeToProject) const
        {
            return m_Dir / relativeToProject;
        }

        // Verilen goreceli yola (assets/ altina) iceriksiz bir dosya yazar.
        // Varlik olarak taninmasi icin uzantisi anlamli olmali.
        void WriteAsset(const std::string& relativeToAssets,
                        const std::string& content = "x") const
        {
            const auto full = Assets() / relativeToAssets;
            std::filesystem::create_directories(full.parent_path());

            FILE* f = std::fopen(full.string().c_str(), "wb");
            if (f)
            {
                std::fwrite(content.data(), 1, content.size(), f);
                std::fclose(f);
            }
        }

    private:
        std::filesystem::path m_Dir;
    };
}
