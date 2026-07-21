#include <catch2/catch_test_macros.hpp>

#include "TestSupport.h"

#include <FXEngine/Asset/AssetManager.h>

#include <filesystem>
#include <fstream>

using FXTest::TempProject;

namespace
{
    // Surum 1 formatinda bir .fxproject yazar: StartScene bir YOL.
    void WriteLegacyProject(const std::filesystem::path& file,
                            const std::string& startScenePath)
    {
        std::ofstream out(file);
        out << R"({
  "Version": 1,
  "Name": "Eski Proje",
  "AssetDirectory": "assets",
  "StartScene": ")" << startScenePath << R"("
})";
    }
}

TEST_CASE("Surum 1 projesinde StartScene yolu GUID'e cevriliyor", "[project]")
{
    // 0.6'nin asil vaadi: baslangic sahnesinin dosyasi tasinsa bile
    // proje onu bulabilsin. Once eski formatin sorunsuz aciliyor
    // oldugunu dogruluyoruz.
    const auto dir = std::filesystem::temp_directory_path() /
                     ("fxlegacy_" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(dir / "assets/scenes");

    {
        std::ofstream scene(dir / "assets/scenes/ana.fxscene");
        scene << R"({"Version":4,"Entities":[]})";
    }

    const auto projectFile = dir / "Eski.fxproject";
    WriteLegacyProject(projectFile, "assets/scenes/ana.fxscene");

    auto project = FX::Project::Load(projectFile.string());
    REQUIRE(project);

    const FX::AssetHandle start = project->GetConfig().StartScene;
    REQUIRE(start.IsValid());
    CHECK(FX::AssetManager::GetPath(start) == "assets/scenes/ana.fxscene");

    FX::Project::CloseActive();

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

TEST_CASE("StartScene GUID'i kaydedilip geri okunuyor", "[project]")
{
    TempProject temp;
    temp.WriteAsset("scenes/oyun.fxscene", R"({"Version":4,"Entities":[]})");

    FX::AssetManager::ScanProject();

    const FX::AssetHandle handle = FX::AssetManager::GetHandle("assets/scenes/oyun.fxscene");
    REQUIRE(handle.IsValid());

    auto project = FX::Project::GetActive();
    REQUIRE(project);

    project->GetConfig().StartScene = handle;
    REQUIRE(project->Save());

    const std::string file = project->GetFilePath();
    FX::Project::CloseActive();

    auto reopened = FX::Project::Load(file);
    REQUIRE(reopened);
    CHECK(static_cast<std::uint64_t>(reopened->GetConfig().StartScene) ==
          static_cast<std::uint64_t>(handle));
}

TEST_CASE("Baslangic sahnesi tasinsa da bulunuyor", "[project]")
{
    // Yol tabanliyken bu senaryo projeyi bozuyordu: .fxproject'i elle
    // duzeltmek gerekiyordu.
    TempProject temp;
    temp.WriteAsset("scenes/ilk.fxscene", R"({"Version":4,"Entities":[]})");

    FX::AssetManager::ScanProject();
    const FX::AssetHandle handle = FX::AssetManager::GetHandle("assets/scenes/ilk.fxscene");
    REQUIRE(handle.IsValid());

    auto project = FX::Project::GetActive();
    project->GetConfig().StartScene = handle;
    REQUIRE(project->Save());

    // Sahneyi baska bir klasore tasi (dosya izleyicinin yapacagi sey).
    std::filesystem::create_directories(temp.Assets() / "levels");
    std::error_code ec;
    std::filesystem::rename(temp.Assets() / "scenes/ilk.fxscene",
                            temp.Assets() / "levels/ilk.fxscene", ec);
    REQUIRE_FALSE(ec);

    FX::AssetManager::OnAssetMoved("assets/scenes/ilk.fxscene",
                                   "assets/levels/ilk.fxscene");

    // GUID degismedi, yol degisti - proje sahneyi yine buluyor.
    CHECK(static_cast<std::uint64_t>(project->GetConfig().StartScene) ==
          static_cast<std::uint64_t>(handle));
    CHECK(FX::AssetManager::GetPath(project->GetConfig().StartScene) ==
          "assets/levels/ilk.fxscene");
}

TEST_CASE("Hedef cozunurluk kaydedilip geri okunuyor", "[project]")
{
    TempProject temp("ResProje");

    auto active = FX::Project::GetActive();
    REQUIRE(active);

    // Yeni proje varsayilani 16:9.
    CHECK(active->GetConfig().TargetWidth  == 1920);
    CHECK(active->GetConfig().TargetHeight == 1080);

    active->GetConfig().TargetWidth  = 640;
    active->GetConfig().TargetHeight = 480;
    REQUIRE(active->Save());

    const std::string file = active->GetFilePath();
    FX::Project::CloseActive();

    auto reopened = FX::Project::Load(file);
    REQUIRE(reopened);
    CHECK(reopened->GetConfig().TargetWidth  == 640);
    CHECK(reopened->GetConfig().TargetHeight == 480);
    CHECK(reopened->GetConfig().TargetAspect() == 640.0f / 480.0f);
}

TEST_CASE("Surum 2 projesi acilinca varsayilan cozunurluk aliyor", "[project]")
{
    // Yeni bir alan eski dosyalari KIRMAMALI: surum 2 dosyalarinda
    // TargetResolution yok, varsayilan gecerli olmali.
    const auto dir = std::filesystem::temp_directory_path() /
                     ("fxv2_" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(dir);

    {
        std::ofstream out(dir / "V2.fxproject");
        out << R"({"Version":2,"Name":"V2","AssetDirectory":"assets","StartScene":0})";
    }

    auto project = FX::Project::Load((dir / "V2.fxproject").string());
    REQUIRE(project);
    CHECK(project->GetConfig().TargetWidth  == 1920);
    CHECK(project->GetConfig().TargetHeight == 1080);

    FX::Project::CloseActive();

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

TEST_CASE("Bozuk cozunurluk degeri varsayilana duser", "[project]")
{
    // Elle duzenlenmis dosyada 0 yazabilir; sifira bolme uretmemeli.
    const auto dir = std::filesystem::temp_directory_path() /
                     ("fxzero_" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(dir);

    {
        std::ofstream out(dir / "Z.fxproject");
        out << R"({"Version":3,"Name":"Z","AssetDirectory":"assets",
                   "StartScene":0,"TargetResolution":[0,0]})";
    }

    auto project = FX::Project::Load((dir / "Z.fxproject").string());
    REQUIRE(project);
    CHECK(project->GetConfig().TargetWidth  == 1920);
    CHECK(project->GetConfig().TargetHeight == 1080);
    CHECK(project->GetConfig().TargetAspect() > 0.0f);

    FX::Project::CloseActive();

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}
