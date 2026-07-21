#include <catch2/catch_test_macros.hpp>

#include "TestSupport.h"

#include <FXEngine/Scene/ComponentMeta.h>
#include <FXEngine/Scene/Components.h>
#include <FXEngine/Scene/Entity.h>
#include <FXEngine/Scene/Scene.h>
#include <FXEngine/Scene/SceneSerializer.h>

#include <nlohmann/json.hpp>

#include <fstream>

using FXTest::TempProject;
using json = nlohmann::json;

namespace
{
    constexpr FX::TextureLibrary* kNoTextures = nullptr;

    const FX::ComponentInfo& Info(const char* name)
    {
        FX::ComponentInfo* info = FX::ComponentRegistry::Find(name);
        REQUIRE(info != nullptr);
        return *info;
    }
}

TEST_CASE("Meta tablo motor component'lerini kendiliginden kaydediyor", "[meta]")
{
    // GetAll ilk erisimde RegisterBuiltins'i cagirmali; "cagirmayi
    // unuttun" diye sessiz bir hata sinifi olmamali.
    const auto& all = FX::ComponentRegistry::GetAll();
    CHECK(all.size() >= 9);

    for (const char* name : { "Transform", "SpriteRenderer", "Velocity",
                              "Camera", "Follow", "NativeScript",
                              "Tag", "Relationship", "WorldTransform" })
    {
        INFO(name);
        CHECK(FX::ComponentRegistry::Find(name) != nullptr);
    }
}

TEST_CASE("Alan erisimcisi dogru uyeyi okuyup yaziyor", "[meta]")
{
    FX::Scene scene;
    FX::Entity e = scene.CreateEntity("Test");
    e.AddComponent<FX::VelocityComponent>();

    const FX::ComponentInfo& info = Info("Velocity");
    REQUIRE(info.Fields.size() == 2);

    void* comp = info.GetPtr(e);
    REQUIRE(comp != nullptr);

    // Tablodan yaz, component'ten oku: erisimcinin dogru uyeye
    // bagli oldugunu bu kanitlar.
    for (const FX::FieldInfo& f : info.Fields)
    {
        if (std::string(f.Name) == "Linear")
        {
            REQUIRE(f.Type == FX::FieldType::Vec2);
            *static_cast<glm::vec2*>(f.Get(comp)) = { 3.0f, -4.0f };
        }
        else if (std::string(f.Name) == "Angular")
        {
            REQUIRE(f.Type == FX::FieldType::Float);
            *static_cast<float*>(f.Get(comp)) = 1.25f;
        }
    }

    const auto& vc = e.GetComponent<FX::VelocityComponent>();
    CHECK(vc.Linear.x == 3.0f);
    CHECK(vc.Linear.y == -4.0f);
    CHECK(vc.Angular  == 1.25f);
}

TEST_CASE("Alan tipleri dogru cikarilmis", "[meta]")
{
    const FX::ComponentInfo& cam = Info("Camera");
    CHECK(cam.Fields[0].Type == FX::FieldType::Float);   // OrthographicSize
    CHECK(cam.Fields[1].Type == FX::FieldType::Bool);    // Primary

    const FX::ComponentInfo& sprite = Info("SpriteRenderer");
    CHECK(sprite.Fields[0].Type == FX::FieldType::Color);  // Color
    CHECK(sprite.SaveExtra != nullptr);                    // doku slotu

    const FX::ComponentInfo& follow = Info("Follow");
    CHECK(follow.Fields[0].Type == FX::FieldType::EntityRef);

    const FX::ComponentInfo& script = Info("NativeScript");
    CHECK(script.Fields[0].Type == FX::FieldType::String);

    // Transform kaldirilamaz: her entity'nin konumu olmak zorunda.
    CHECK(Info("Transform").Removable == false);

    // Yapisal component'ler kendi nesneleri olarak yazilmaz.
    CHECK(Info("Tag").SerializedByTable == false);
    CHECK(Info("Relationship").SerializedByTable == false);
}

// A1'in en kritik testi: tablodan uretilen JSON, elle yazilan
// serilestiricinin urettigiyle BIREBIR ayni olmali. Aksi halde
// sahne dosyasi formati sessizce degisir ve surum 5 gerekirdi.
TEST_CASE("Uretilen JSON bilinen sahne formatiyla ayni", "[meta][serializer]")
{
    TempProject project;

    FX::Scene scene;
    FX::Entity e = scene.CreateEntity("Zemin");

    auto& tf = e.GetComponent<FX::TransformComponent>();
    tf.Translation = { 1.0f, 2.0f, -0.5f };
    tf.Rotation    = 0.25f;            // RADYAN yazilmali, derece degil
    tf.Scale       = { 30.0f, 30.0f };

    auto& sc = e.AddComponent<FX::SpriteRendererComponent>();
    sc.Color        = { 0.5f, 0.6f, 0.7f, 1.0f };
    sc.TilingFactor = 15.0f;

    auto& vc = e.AddComponent<FX::VelocityComponent>();
    vc.Linear  = { 1.5f, -2.5f };
    vc.Angular = 0.5f;

    auto& cc = e.AddComponent<FX::CameraComponent>();
    cc.OrthographicSize = 6.0f;
    cc.Primary          = false;

    auto& fc = e.AddComponent<FX::FollowComponent>();
    fc.Target       = FX::UUID{ 12345 };
    fc.Speed        = 3.0f;
    fc.StopDistance = 2.0f;

    e.AddComponent<FX::NativeScriptComponent>(std::string("Spin"));

    FX::SceneSerializer serializer(&scene, kNoTextures);
    REQUIRE(serializer.Serialize("assets/scenes/format.fxscene"));

    std::ifstream in(project.Path("assets/scenes/format.fxscene"));
    REQUIRE(in.is_open());

    json doc;
    in >> doc;

    REQUIRE(doc["Entities"].size() == 1);
    const json& j = doc["Entities"][0];

    CHECK(j["Tag"] == "Zemin");
    CHECK(j.contains("ID"));

    CHECK(j["Transform"]["Translation"] == json::array({ 1.0f, 2.0f, -0.5f }));
    CHECK(j["Transform"]["Rotation"]    == 0.25f);
    CHECK(j["Transform"]["Scale"]       == json::array({ 30.0f, 30.0f }));

    CHECK(j["SpriteRenderer"]["Color"] == json::array({ 0.5f, 0.6f, 0.7f, 1.0f }));
    CHECK(j["SpriteRenderer"]["TilingFactor"] == 15.0f);
    CHECK(j["SpriteRenderer"]["TextureHandle"] == 0);   // doku yok

    CHECK(j["Velocity"]["Linear"]  == json::array({ 1.5f, -2.5f }));
    CHECK(j["Velocity"]["Angular"] == 0.5f);

    CHECK(j["Camera"]["OrthographicSize"] == 6.0f);
    CHECK(j["Camera"]["Primary"]          == false);

    CHECK(j["Follow"]["Target"]       == 12345);
    CHECK(j["Follow"]["Speed"]        == 3.0f);
    CHECK(j["Follow"]["StopDistance"] == 2.0f);

    CHECK(j["NativeScript"]["Name"] == "Spin");

    // Yapisal component'ler kendi nesneleri olarak YAZILMAMALI.
    CHECK_FALSE(j.contains("Relationship"));
    CHECK_FALSE(j.contains("WorldTransform"));
}

TEST_CASE("Dosyada olmayan alan component varsayilanini koruyor", "[meta][serializer]")
{
    TempProject project;

    // Elle yazilmis, eksik alanli bir sahne: eski surumlerden gelen
    // dosyalar boyle gorunuyor.
    project.WriteAsset("scenes/eksik.fxscene", R"({
        "Version": 4,
        "Entities": [
            { "ID": 42, "Tag": "Eksik",
              "Transform": { "Translation": [1.0, 2.0, 3.0] },
              "Camera": {} }
        ]
    })");

    FX::Scene scene;
    FX::SceneSerializer serializer(&scene, kNoTextures);
    REQUIRE(serializer.Deserialize("assets/scenes/eksik.fxscene"));

    FX::Entity e = scene.FindEntityByUUID(FX::UUID{ 42 });
    REQUIRE(e);

    const auto& tf = e.GetComponent<FX::TransformComponent>();
    CHECK(tf.Translation.x == 1.0f);
    CHECK(tf.Rotation == 0.0f);      // varsayilan
    CHECK(tf.Scale.x  == 1.0f);      // varsayilan, 0 DEGIL

    const auto& cc = e.GetComponent<FX::CameraComponent>();
    CHECK(cc.OrthographicSize == 8.0f);   // varsayilan
    CHECK(cc.Primary == true);            // varsayilan
}

namespace
{
    // A1'in butun iddiasi: yeni bir component TEK YERE yazilinca
    // serilestirme, geri yukleme ve kopyalama kendiliginden calismali.
    struct HealthComponent
    {
        float Current = 100.0f;
        int   Lives   = 3;
        bool  Invincible = false;
    };
}

TEST_CASE("Tek yere kaydedilen component uc yolda da calisiyor", "[meta]")
{
    TempProject project;

    // TEK dokunulan yer burasi. Serilestirici, Inspector ve Scene::Copy
    // icin ayrica hicbir sey yazilmadi.
    if (!FX::ComponentRegistry::Find("Health"))
    {
        FX::ComponentRegistry::Register<HealthComponent>("Health", "Health")
            .Field<&HealthComponent::Current>("Current", "Can")
            .Field<&HealthComponent::Lives>("Lives", "Hak")
            .Field<&HealthComponent::Invincible>("Invincible", "Olumsuz");
    }

    FX::UUID id;
    {
        FX::Scene scene;
        FX::Entity e = scene.CreateEntity("Kahraman");
        id = e.GetComponent<FX::IDComponent>().ID;

        auto& h = e.AddComponent<HealthComponent>();
        h.Current    = 42.5f;
        h.Lives      = 7;
        h.Invincible = true;

        // 1) Kopyalama
        auto copy = FX::Scene::Copy(scene);
        FX::Entity c = copy->FindEntityByUUID(id);
        REQUIRE(c);
        REQUIRE(c.HasComponent<HealthComponent>());
        CHECK(c.GetComponent<HealthComponent>().Lives == 7);

        // 2) Serilestirme
        FX::SceneSerializer serializer(&scene, kNoTextures);
        REQUIRE(serializer.Serialize("assets/scenes/health.fxscene"));
    }

    // 3) Geri yukleme
    FX::Scene loaded;
    FX::SceneSerializer serializer(&loaded, kNoTextures);
    REQUIRE(serializer.Deserialize("assets/scenes/health.fxscene"));

    FX::Entity e = loaded.FindEntityByUUID(id);
    REQUIRE(e);
    REQUIRE(e.HasComponent<HealthComponent>());

    const auto& h = e.GetComponent<HealthComponent>();
    CHECK(h.Current    == 42.5f);
    CHECK(h.Lives      == 7);
    CHECK(h.Invincible == true);
}

TEST_CASE("Scene::Copy tablodaki her component'i tasiyor", "[meta][scene]")
{
    FX::Scene scene;
    FX::Entity a = scene.CreateEntity("Kaynak");

    a.AddComponent<FX::VelocityComponent>(glm::vec2{ 5.0f, 6.0f }, 0.5f);
    a.AddComponent<FX::CameraComponent>(4.0f, true);
    a.AddComponent<FX::NativeScriptComponent>(std::string("Spin"));

    const FX::UUID id = a.GetComponent<FX::IDComponent>().ID;

    auto copy = FX::Scene::Copy(scene);
    FX::Entity b = copy->FindEntityByUUID(id);
    REQUIRE(b);

    CHECK(b.GetComponent<FX::TagComponent>().Tag == "Kaynak");
    CHECK(b.GetComponent<FX::VelocityComponent>().Linear.x == 5.0f);
    CHECK(b.GetComponent<FX::CameraComponent>().OrthographicSize == 4.0f);
    CHECK(b.GetComponent<FX::NativeScriptComponent>().ScriptName == "Spin");

    // Script ORNEGI kopyalanmaz, yalnizca baglantisi.
    CHECK(b.GetComponent<FX::NativeScriptComponent>().Instance == nullptr);
}
