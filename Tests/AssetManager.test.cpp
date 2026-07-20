#include <catch2/catch_test_macros.hpp>

#include "TestSupport.h"

#include <FXEngine/Asset/AssetManager.h>

#include <filesystem>

using FXTest::TempProject;

TEST_CASE("Tarama varliklara GUID atar ve .meta yazar", "[asset]")
{
    TempProject project;
    project.WriteAsset("textures/kahraman.png");

    FX::AssetManager::Clear();
    FX::AssetManager::ScanProject();

    const FX::AssetHandle handle = FX::AssetManager::GetHandle("assets/textures/kahraman.png");

    CHECK(handle.IsValid());
    CHECK(FX::AssetManager::Exists(handle));
    CHECK(std::filesystem::exists(project.Assets() / "textures/kahraman.png.meta"));
}

TEST_CASE("Ikinci tarama AYNI GUID'i veriyor", "[asset]")
{
    // Butun sistemin dayandigi ozellik: .meta diskte durdugu surece
    // kimlik degismez. Degisseydi her acilista sahne referanslari kopardi.
    TempProject project;
    project.WriteAsset("textures/a.png");

    FX::AssetManager::Clear();
    FX::AssetManager::ScanProject();
    const FX::AssetHandle first = FX::AssetManager::GetHandle("assets/textures/a.png");

    FX::AssetManager::Clear();
    FX::AssetManager::ScanProject();
    const FX::AssetHandle second = FX::AssetManager::GetHandle("assets/textures/a.png");

    CHECK(static_cast<std::uint64_t>(first) == static_cast<std::uint64_t>(second));
}

TEST_CASE("Tasima kimligi korur", "[asset]")
{
    TempProject project;
    project.WriteAsset("textures/eski.png");

    FX::AssetManager::Clear();
    FX::AssetManager::ScanProject();

    const FX::AssetHandle before = FX::AssetManager::GetHandle("assets/textures/eski.png");
    REQUIRE(before.IsValid());

    std::error_code ec;
    std::filesystem::rename(project.Assets() / "textures/eski.png",
                            project.Assets() / "textures/yeni.png", ec);
    REQUIRE_FALSE(ec);

    FX::AssetManager::OnAssetMoved("assets/textures/eski.png", "assets/textures/yeni.png");

    CHECK(FX::AssetManager::GetPath(before) == "assets/textures/yeni.png");
    CHECK_FALSE(FX::AssetManager::GetHandle("assets/textures/eski.png").IsValid());
    CHECK(std::filesystem::exists(project.Assets() / "textures/yeni.png.meta"));
    CHECK_FALSE(std::filesystem::exists(project.Assets() / "textures/eski.png.meta"));
}

TEST_CASE("Silme kaydi ve .meta'yi temizler", "[asset]")
{
    TempProject project;
    project.WriteAsset("textures/gidecek.png");

    FX::AssetManager::Clear();
    FX::AssetManager::ScanProject();

    const FX::AssetHandle handle = FX::AssetManager::GetHandle("assets/textures/gidecek.png");
    REQUIRE(handle.IsValid());

    FX::AssetManager::OnAssetDeleted("assets/textures/gidecek.png");

    CHECK_FALSE(FX::AssetManager::Exists(handle));
    CHECK_FALSE(std::filesystem::exists(project.Assets() / "textures/gidecek.png.meta"));
}

TEST_CASE("Register ayni dosyaya ikinci GUID vermiyor", "[asset]")
{
    TempProject project;
    project.WriteAsset("textures/b.png");

    FX::AssetManager::Clear();

    const FX::AssetHandle first  = FX::AssetManager::Register("assets/textures/b.png");
    const FX::AssetHandle second = FX::AssetManager::Register("assets/textures/b.png");

    CHECK(first.IsValid());
    CHECK(static_cast<std::uint64_t>(first) == static_cast<std::uint64_t>(second));
    CHECK(FX::AssetManager::GetCount() == 1);
}

TEST_CASE("Taninmayan uzanti varlik sayilmiyor", "[asset]")
{
    TempProject project;
    project.WriteAsset("notlar.txt");

    FX::AssetManager::Clear();
    FX::AssetManager::ScanProject();

    CHECK_FALSE(FX::AssetManager::GetHandle("assets/notlar.txt").IsValid());
    CHECK_FALSE(std::filesystem::exists(project.Assets() / "notlar.txt.meta"));
}

TEST_CASE("Bilinmeyen GUID bos yol donduruyor", "[asset]")
{
    TempProject project;

    FX::AssetManager::Clear();

    const FX::AssetHandle unknown{ 123456789 };
    CHECK_FALSE(FX::AssetManager::Exists(unknown));
    CHECK(FX::AssetManager::GetPath(unknown).empty());
}

TEST_CASE("MetaPathFor ve IsMetaFile tutarli", "[asset]")
{
    CHECK(FX::AssetManager::MetaPathFor("assets/x.png") == "assets/x.png.meta");
    CHECK(FX::AssetManager::IsMetaFile("assets/x.png.meta"));
    CHECK_FALSE(FX::AssetManager::IsMetaFile("assets/x.png"));
}
