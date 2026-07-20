#include <catch2/catch_test_macros.hpp>

#include "TestSupport.h"

#include <FXEngine/Scene/Scene.h>
#include <FXEngine/Scene/Entity.h>
#include <FXEngine/Scene/Components.h>
#include <FXEngine/Scene/SceneSerializer.h>

#include <fstream>

using FXTest::TempProject;

namespace
{
    // Doku YOK: TextureLibrary'ye nullptr veriyoruz. Doku yuklemek OpenGL
    // baglami ister ve testler penceresiz calisiyor. Serilestiricinin
    // doku disindaki tum isi bu sekilde test edilebiliyor - zaten
    // "bir sinifin iki isi olmamali" tasariminin karsiligi bu.
    constexpr FX::TextureLibrary* kNoTextures = nullptr;
}

TEST_CASE("Sahne kaydedilip geri yuklenince ayni kaliyor", "[serializer]")
{
    TempProject project;

    FX::UUID idA, idB;
    {
        FX::Scene scene;

        FX::Entity a = scene.CreateEntity("Oyuncu");
        auto& tf = a.GetComponent<FX::TransformComponent>();
        tf.Translation = { 1.5f, -2.25f, 0.5f };
        tf.Rotation    = 0.75f;
        tf.Scale       = { 3.0f, 4.0f };

        FX::Entity b = scene.CreateEntity("Cocuk");
        b.SetParent(a);

        idA = a.GetComponent<FX::IDComponent>().ID;
        idB = b.GetComponent<FX::IDComponent>().ID;

        FX::SceneSerializer serializer(&scene, kNoTextures);
        REQUIRE(serializer.Serialize("assets/scenes/test.fxscene"));
    }

    FX::Scene loaded;
    FX::SceneSerializer serializer(&loaded, kNoTextures);
    REQUIRE(serializer.Deserialize("assets/scenes/test.fxscene"));

    CHECK(loaded.GetEntityCount() == 2);

    FX::Entity a = loaded.FindEntityByUUID(idA);
    FX::Entity b = loaded.FindEntityByUUID(idB);

    REQUIRE(a);
    REQUIRE(b);

    CHECK(a.GetComponent<FX::TagComponent>().Tag == "Oyuncu");

    const auto& tf = a.GetComponent<FX::TransformComponent>();
    CHECK(tf.Translation.x == 1.5f);
    CHECK(tf.Translation.y == -2.25f);
    CHECK(tf.Rotation == 0.75f);
    CHECK(tf.Scale.y == 4.0f);

    // Hiyerarsi iki gecisli yuklemeyle kuruluyor; kopmasi en olasi yer.
    CHECK(b.GetParent() == a);
}

TEST_CASE("Kamera component'i yuvarlanip geri geliyor", "[serializer]")
{
    TempProject project;

    FX::UUID id;
    {
        FX::Scene scene;
        FX::Entity cam = scene.CreateEntity("Kamera");
        auto& cc = cam.AddComponent<FX::CameraComponent>();
        cc.OrthographicSize = 12.5f;
        cc.Primary          = true;

        id = cam.GetComponent<FX::IDComponent>().ID;

        FX::SceneSerializer serializer(&scene, kNoTextures);
        REQUIRE(serializer.Serialize("assets/scenes/kamera.fxscene"));
    }

    FX::Scene loaded;
    FX::SceneSerializer serializer(&loaded, kNoTextures);
    REQUIRE(serializer.Deserialize("assets/scenes/kamera.fxscene"));

    FX::Entity cam = loaded.FindEntityByUUID(id);
    REQUIRE(cam);
    REQUIRE(cam.HasComponent<FX::CameraComponent>());
    CHECK(cam.GetComponent<FX::CameraComponent>().OrthographicSize == 12.5f);
    CHECK(cam.GetComponent<FX::CameraComponent>().Primary);
}

TEST_CASE("Olmayan dosyayi yuklemek false doner, cokmez", "[serializer]")
{
    TempProject project;

    FX::Scene scene;
    FX::SceneSerializer serializer(&scene, kNoTextures);

    CHECK_FALSE(serializer.Deserialize("assets/scenes/yok.fxscene"));
    CHECK(scene.GetEntityCount() == 0);
}

TEST_CASE("Bozuk JSON yuklemeyi cokertmiyor", "[serializer]")
{
    // Istisna atmiyoruz (motorlarda genelde kapali); bozuk girdi
    // false donmeli, program devam etmeli.
    TempProject project;
    project.WriteAsset("scenes/bozuk.fxscene", "{ bu gecerli json degil ]]");

    FX::Scene scene;
    FX::SceneSerializer serializer(&scene, kNoTextures);

    CHECK_FALSE(serializer.Deserialize("assets/scenes/bozuk.fxscene"));
}

TEST_CASE("Yukleme oncesi sahne temizleniyor", "[serializer]")
{
    // Ayni Scene nesnesine iki sahne yuklenirse eskiler kalmamali.
    TempProject project;

    {
        FX::Scene scene;
        scene.CreateEntity("Tek");
        FX::SceneSerializer serializer(&scene, kNoTextures);
        REQUIRE(serializer.Serialize("assets/scenes/tek.fxscene"));
    }

    FX::Scene scene;
    scene.CreateEntity("Onceden var olan");
    scene.CreateEntity("Bu da");

    FX::SceneSerializer serializer(&scene, kNoTextures);
    REQUIRE(serializer.Deserialize("assets/scenes/tek.fxscene"));

    CHECK(scene.GetEntityCount() == 1);
}
