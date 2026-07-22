#include <catch2/catch_test_macros.hpp>

#include "TestSupport.h"

#include <FXEngine/Scene/Scene.h>
#include <FXEngine/Scene/Entity.h>
#include <FXEngine/Scene/Components.h>
#include <FXEngine/Scene/PrefabSerializer.h>
#include <FXEngine/Scene/SceneSerializer.h>
#include <FXEngine/Asset/AssetManager.h>
#include <FXEngine/Core/FileSystem.h>

#include <nlohmann/json.hpp>

#include <fstream>

using FXTest::TempProject;

namespace
{
    // Doku yok: testler penceresiz. Prefab'in doku disindaki tum isi
    // (kimlik, hiyerarsi, bag damgasi) bu sekilde test edilebiliyor.
    constexpr FX::TextureLibrary* kNoTextures = nullptr;

    // parent + tek cocuk kurar, prefab olarak kaydeder, varlik tablosuna
    // sokar. Kaynak UUID'leri ve prefab handle'ini geri verir.
    struct PreparedPrefab
    {
        FX::UUID        SrcRoot;
        FX::UUID        SrcChild;
        FX::AssetHandle Handle;
        std::string     RelPath = "assets/prefabs/kutu.fxprefab";
    };

    PreparedPrefab MakePrefab(FX::Scene& scene)
    {
        PreparedPrefab p;

        FX::Entity root  = scene.CreateEntity("Kutu");
        FX::Entity child = scene.CreateEntity("Kapak");
        child.SetParent(root);

        p.SrcRoot  = root.GetUUID();
        p.SrcChild = child.GetUUID();

        FX::PrefabSerializer prefab(&scene, kNoTextures);
        REQUIRE(prefab.Save(root, p.RelPath));

        // GetHandle'in gecerli deger dondurmesi icin dosya tabloya girmeli.
        FX::AssetManager::ScanProject();
        p.Handle = FX::AssetManager::GetHandle(p.RelPath);

        return p;
    }
}

TEST_CASE("Prefab ornegi kaynagina baglaniyor", "[prefab]")
{
    TempProject project;
    FX::Scene scene;

    PreparedPrefab p = MakePrefab(scene);
    REQUIRE(p.Handle.IsValid());   // .fxprefab varlik olarak taniniyor

    FX::PrefabSerializer prefab(&scene, kNoTextures);
    FX::Entity instance = prefab.Instantiate(p.RelPath, { 3.0f, 4.0f, 0.0f });

    REQUIRE(instance);
    REQUIRE(instance.HasComponent<FX::PrefabInstanceComponent>());

    const auto& link = instance.GetComponent<FX::PrefabInstanceComponent>();
    CHECK(link.Prefab == p.Handle);
    CHECK(link.SourceId == p.SrcRoot);

    // Ornek YENI bir kimlik aldi; kaynagi degil.
    CHECK(instance.GetUUID() != p.SrcRoot);

    // Alt agac da damgalaniyor: cocuk kendi SourceId'siyle.
    auto children = instance.GetChildren();
    REQUIRE(children.size() == 1);

    FX::Entity childInstance = children[0];
    REQUIRE(childInstance.HasComponent<FX::PrefabInstanceComponent>());

    const auto& childLink = childInstance.GetComponent<FX::PrefabInstanceComponent>();
    CHECK(childLink.Prefab == p.Handle);
    CHECK(childLink.SourceId == p.SrcChild);
    CHECK(childInstance.GetUUID() != p.SrcChild);
}

TEST_CASE("Prefab bagi sahne round-trip'inde korunuyor", "[prefab]")
{
    TempProject project;

    FX::UUID        instanceId;
    FX::AssetHandle handle;
    FX::UUID        srcRoot;
    {
        FX::Scene scene;
        PreparedPrefab p = MakePrefab(scene);
        handle  = p.Handle;
        srcRoot = p.SrcRoot;

        FX::PrefabSerializer prefab(&scene, kNoTextures);
        FX::Entity instance = prefab.Instantiate(p.RelPath, { 0.0f, 0.0f, 0.0f });
        REQUIRE(instance);
        instanceId = instance.GetUUID();

        FX::SceneSerializer serializer(&scene, kNoTextures);
        REQUIRE(serializer.Serialize("assets/scenes/prefabtest.fxscene"));
    }

    FX::Scene loaded;
    FX::SceneSerializer serializer(&loaded, kNoTextures);
    REQUIRE(serializer.Deserialize("assets/scenes/prefabtest.fxscene"));

    FX::Entity instance = loaded.FindEntityByUUID(instanceId);
    REQUIRE(instance);
    REQUIRE(instance.HasComponent<FX::PrefabInstanceComponent>());

    const auto& link = instance.GetComponent<FX::PrefabInstanceComponent>();
    CHECK(link.Prefab == handle);
    CHECK(link.SourceId == srcRoot);
}

TEST_CASE("Prefab olarak kaydetmek bagi dosyaya yazmiyor", "[prefab]")
{
    // Bir prefab ornegini yeni bir prefab olarak kaydedersek, kaynak
    // dosya baska bir prefab'a bagli kalmamali - yoksa "kaynagin kaynagi"
    // gibi anlamsiz bir zincir olusurdu.
    TempProject project;
    FX::Scene scene;

    PreparedPrefab p = MakePrefab(scene);

    FX::PrefabSerializer prefab(&scene, kNoTextures);
    FX::Entity instance = prefab.Instantiate(p.RelPath, { 0.0f, 0.0f, 0.0f });
    REQUIRE(instance);
    REQUIRE(instance.HasComponent<FX::PrefabInstanceComponent>());

    const std::string copyRel = "assets/prefabs/kopya.fxprefab";
    REQUIRE(prefab.Save(instance, copyRel));

    std::ifstream in(FX::FileSystem::ResolveProjectAsset(copyRel));
    REQUIRE(in);
    nlohmann::json doc;
    in >> doc;

    REQUIRE(doc.contains("Entities"));
    for (const auto& e : doc["Entities"])
        CHECK_FALSE(e.contains("PrefabInstance"));
}
