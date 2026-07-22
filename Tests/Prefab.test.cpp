#include <catch2/catch_test_macros.hpp>

#include "TestSupport.h"

#include <FXEngine/Scene/Scene.h>
#include <FXEngine/Scene/Entity.h>
#include <FXEngine/Scene/Components.h>
#include <FXEngine/Scene/PrefabSerializer.h>
#include <FXEngine/Scene/PrefabOverrides.h>
#include <FXEngine/Scene/ComponentMeta.h>
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

TEST_CASE("Revert ornegi kaynagina donduruyor", "[prefab]")
{
    TempProject project;
    FX::Scene scene;

    PreparedPrefab p = MakePrefab(scene);

    FX::PrefabSerializer prefab(&scene, kNoTextures);
    FX::Entity root = prefab.Instantiate(p.RelPath, { 5.0f, 6.0f, 0.0f });
    REQUIRE(root);

    FX::Entity child = root.GetChildren()[0];

    // Ornekte override'lar: cocuk konumu, kok donmesi, eklenen component.
    child.GetComponent<FX::TransformComponent>().Translation = { 9.0f, 9.0f, 0.0f };
    root.GetComponent<FX::TransformComponent>().Rotation = 1.25f;
    root.AddComponent<FX::CameraComponent>();

    REQUIRE(prefab.RevertInstance(root));

    // Cocuk konumu kaynaga dondu (kaynakta 0).
    CHECK(child.GetComponent<FX::TransformComponent>().Translation.x == 0.0f);
    CHECK(child.GetComponent<FX::TransformComponent>().Translation.y == 0.0f);

    // Kok donmesi kaynaga dondu (kaynakta 0).
    CHECK(root.GetComponent<FX::TransformComponent>().Rotation == 0.0f);

    // Kok KONUMU korundu: ornek yerinde kalmali (Instantiate override'i).
    CHECK(root.GetComponent<FX::TransformComponent>().Translation.x == 5.0f);
    CHECK(root.GetComponent<FX::TransformComponent>().Translation.y == 6.0f);

    // Ornekte EKLENEN component revert'te kalkti.
    CHECK_FALSE(root.HasComponent<FX::CameraComponent>());

    // Bag revert'ten sag cikti.
    CHECK(root.HasComponent<FX::PrefabInstanceComponent>());
    CHECK(child.HasComponent<FX::PrefabInstanceComponent>());
}

TEST_CASE("Revert ic referansi ornek kimligine yeniden esliyor", "[prefab]")
{
    // Kaynakta cocuk, koku takip ediyor (ic referans). Ornekte bu referans
    // ornegin kokune bakmali - kaynagin degil. Revert bunu bozmamali.
    TempProject project;
    FX::Scene scene;

    FX::Entity srcRoot  = scene.CreateEntity("Kok");
    FX::Entity srcChild = scene.CreateEntity("Takipci");
    srcChild.SetParent(srcRoot);
    srcChild.AddComponent<FX::FollowComponent>(srcRoot.GetUUID());

    const std::string rel = "assets/prefabs/takip.fxprefab";
    FX::PrefabSerializer prefab(&scene, kNoTextures);
    REQUIRE(prefab.Save(srcRoot, rel));
    FX::AssetManager::ScanProject();

    FX::Entity root  = prefab.Instantiate(rel, { 0.0f, 0.0f, 0.0f });
    REQUIRE(root);
    FX::Entity child = root.GetChildren()[0];

    // Ornekleme sonrasi hedef ornegin kokune esitlenmis olmali.
    REQUIRE(child.HasComponent<FX::FollowComponent>());
    CHECK(child.GetComponent<FX::FollowComponent>().Target.Target == root.GetUUID());

    // Referansi boz, sonra revert et.
    child.GetComponent<FX::FollowComponent>().Target.Target = FX::UUID(0);
    REQUIRE(prefab.RevertInstance(root));

    // Revert sonrasi yine ornegin kokune bakmali (kaynagin kokune degil).
    CHECK(child.GetComponent<FX::FollowComponent>().Target.Target == root.GetUUID());
    CHECK(child.GetComponent<FX::FollowComponent>().Target.Target != srcRoot.GetUUID());
}

TEST_CASE("Override tespiti kaynaktan sapan alani buluyor", "[prefab]")
{
    TempProject project;
    FX::Scene scene;

    PreparedPrefab p = MakePrefab(scene);

    FX::PrefabSerializer prefab(&scene, kNoTextures);
    FX::Entity root  = prefab.Instantiate(p.RelPath, { 5.0f, 6.0f, 0.0f });
    REQUIRE(root);
    FX::Entity child = root.GetChildren()[0];

    FX::ComponentInfo* tf = FX::ComponentRegistry::Find("Transform");
    REQUIRE(tf);

    // Cocuk el degmemis: sapma yok.
    CHECK(FX::PrefabOverrides::OverriddenFields(child, *tf).empty());

    // Kok konumu Instantiate'in override'i: yalniz Translation sapmis.
    auto rootOv = FX::PrefabOverrides::OverriddenFields(root, *tf);
    REQUIRE(rootOv.size() == 1);
    CHECK(std::string(rootOv[0]) == "Translation");

    // Cocugun rotasyonu degisince o da tespit ediliyor.
    child.GetComponent<FX::TransformComponent>().Rotation = 0.5f;
    auto childOv = FX::PrefabOverrides::OverriddenFields(child, *tf);
    REQUIRE(childOv.size() == 1);
    CHECK(std::string(childOv[0]) == "Rotation");
}

TEST_CASE("RevertField tek alani donduruyor, digerlerine dokunmuyor", "[prefab]")
{
    TempProject project;
    FX::Scene scene;

    PreparedPrefab p = MakePrefab(scene);

    FX::PrefabSerializer prefab(&scene, kNoTextures);
    FX::Entity root  = prefab.Instantiate(p.RelPath, { 0.0f, 0.0f, 0.0f });
    REQUIRE(root);
    FX::Entity child = root.GetChildren()[0];

    auto& tf = child.GetComponent<FX::TransformComponent>();
    tf.Rotation = 0.5f;
    tf.Scale    = { 3.0f, 3.0f };

    FX::ComponentInfo* info = FX::ComponentRegistry::Find("Transform");
    REQUIRE(info);

    CHECK(FX::PrefabOverrides::RevertField(child, *info, "Rotation"));

    CHECK(tf.Rotation == 0.0f);        // geri dondu
    CHECK(tf.Scale.x == 3.0f);         // diger override korundu

    // Zaten kaynakta: ikinci cagri "degismedi" demeli.
    CHECK_FALSE(FX::PrefabOverrides::RevertField(child, *info, "Rotation"));
}

TEST_CASE("EntityRef override'i ornek uzayinda kiyaslaniyor", "[prefab]")
{
    // Ic referans ornekte ornek kimligi tasir, kaynakta kaynak kimligi.
    // Dogru kiyas icin uzay cevrimi sart - yoksa el degmemis Follow bile
    // "sapmis" gorunurdu.
    TempProject project;
    FX::Scene scene;

    FX::Entity srcRoot  = scene.CreateEntity("Kok");
    FX::Entity srcChild = scene.CreateEntity("Takipci");
    srcChild.SetParent(srcRoot);
    srcChild.AddComponent<FX::FollowComponent>(srcRoot.GetUUID());

    const std::string rel = "assets/prefabs/takip2.fxprefab";
    FX::PrefabSerializer prefab(&scene, kNoTextures);
    REQUIRE(prefab.Save(srcRoot, rel));
    FX::AssetManager::ScanProject();

    FX::Entity root  = prefab.Instantiate(rel, { 0.0f, 0.0f, 0.0f });
    REQUIRE(root);
    FX::Entity child = root.GetChildren()[0];

    FX::ComponentInfo* info = FX::ComponentRegistry::Find("Follow");
    REQUIRE(info);

    // El degmemis: ic referans sapma DEGIL.
    CHECK(FX::PrefabOverrides::OverriddenFields(child, *info).empty());

    // Hedef bozulunca tespit ediliyor...
    child.GetComponent<FX::FollowComponent>().Target.Target = FX::UUID(0);
    auto ov = FX::PrefabOverrides::OverriddenFields(child, *info);
    REQUIRE(ov.size() == 1);
    CHECK(std::string(ov[0]) == "Target");

    // ...ve RevertField hedefi ORNEGIN kokune donduruyor (kaynagin degil).
    CHECK(FX::PrefabOverrides::RevertField(child, *info, "Target"));
    CHECK(child.GetComponent<FX::FollowComponent>().Target.Target == root.GetUUID());
}
