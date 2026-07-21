#include <catch2/catch_test_macros.hpp>

#include <FXEngine/Scene/Components.h>
#include <FXEngine/Scene/Entity.h>
#include <FXEngine/Scene/Scene.h>

// B-2: entity cogaltma. Kopya YENI kimliklerle, kaynagin kardesi olarak,
// ic referanslari kendi icine bakacak sekilde uretilmeli.

TEST_CASE("Cogaltma yeni UUID veriyor ve veriyi kopyaliyor", "[duplicate]")
{
    FX::Scene scene;

    FX::Entity src = scene.CreateEntity("Kaynak");
    src.GetComponent<FX::TransformComponent>().Translation = { 2.0f, 3.0f, 0.0f };
    src.AddComponent<FX::VelocityComponent>().Linear = { 1.0f, 0.0f };

    FX::Entity dup = scene.DuplicateEntity(src);
    REQUIRE(dup);

    // Yeni kimlik: iki ornek ayni UUID'yi tasiyamaz.
    CHECK(static_cast<std::uint64_t>(dup.GetUUID()) !=
          static_cast<std::uint64_t>(src.GetUUID()));

    // Veri kopyalandi.
    CHECK(dup.GetComponent<FX::TransformComponent>().Translation.x == 2.0f);
    REQUIRE(dup.HasComponent<FX::VelocityComponent>());
    CHECK(dup.GetComponent<FX::VelocityComponent>().Linear.x == 1.0f);

    CHECK(scene.GetEntityCount() == 2);
}

TEST_CASE("Cogaltma kaynagin kardesi oluyor", "[duplicate]")
{
    FX::Scene scene;

    FX::Entity parent = scene.CreateEntity("Parent");
    FX::Entity child  = scene.CreateEntity("Child");
    child.SetParent(parent);

    FX::Entity dup = scene.DuplicateEntity(child);
    REQUIRE(dup);

    CHECK(dup.GetParent() == parent);
    CHECK(parent.GetChildren().size() == 2);
}

TEST_CASE("Cogaltma alt agaci koru ve ic referanslari ice cevirir", "[duplicate]")
{
    FX::Scene scene;

    // Gorevli -> Hedef, ikisi de ayni alt agacin altinda.
    FX::Entity root   = scene.CreateEntity("Kok");
    FX::Entity hunter = scene.CreateEntity("Avci");
    FX::Entity target = scene.CreateEntity("Hedef");
    hunter.SetParent(root);
    target.SetParent(root);

    // Avci, Hedef'i takip ediyor (ic referans).
    hunter.AddComponent<FX::FollowComponent>().Target = target.GetUUID();

    FX::Entity dupRoot = scene.DuplicateEntity(root);
    REQUIRE(dupRoot);

    // Kopya alt agaci: iki cocuk.
    REQUIRE(dupRoot.GetChildren().size() == 2);

    // Kopyadaki Avci'nin hedefi, ORIJINAL Hedef degil KOPYA Hedef olmali.
    FX::Entity dupHunter, dupTarget;
    for (FX::Entity c : dupRoot.GetChildren())
    {
        if (c.HasComponent<FX::FollowComponent>()) dupHunter = c;
        else                                       dupTarget = c;
    }
    REQUIRE(dupHunter);
    REQUIRE(dupTarget);

    const FX::UUID followed = dupHunter.GetComponent<FX::FollowComponent>().Target.Target;
    CHECK(static_cast<std::uint64_t>(followed) ==
          static_cast<std::uint64_t>(dupTarget.GetUUID()));
    // Orijinal hedefe DEGIL.
    CHECK(static_cast<std::uint64_t>(followed) !=
          static_cast<std::uint64_t>(target.GetUUID()));
}

TEST_CASE("Cogaltma disariya bakan referansa dokunmuyor", "[duplicate]")
{
    FX::Scene scene;

    FX::Entity outside = scene.CreateEntity("Disarida");
    FX::Entity hunter  = scene.CreateEntity("Avci");
    hunter.AddComponent<FX::FollowComponent>().Target = outside.GetUUID();

    // Avci tek basina cogaltiliyor; hedefi kopyanin DISINDA.
    FX::Entity dup = scene.DuplicateEntity(hunter);
    REQUIRE(dup);

    // Disariya bakan referans korunmali: hala ayni entity'yi gostermeli.
    const FX::UUID followed = dup.GetComponent<FX::FollowComponent>().Target.Target;
    CHECK(static_cast<std::uint64_t>(followed) ==
          static_cast<std::uint64_t>(outside.GetUUID()));
}

TEST_CASE("Script Entity alani cogaltmada ice cevriliyor", "[duplicate]")
{
    FX::Scene scene;

    FX::Entity root   = scene.CreateEntity("Kok");
    FX::Entity a      = scene.CreateEntity("A");
    FX::Entity b      = scene.CreateEntity("B");
    a.SetParent(root);
    b.SetParent(root);

    // A'nin script'inde B'ye bakan bir Entity alani var.
    auto& nsc = a.AddComponent<FX::NativeScriptComponent>("Bakan");
    FX::ScriptFieldValue val;
    val.Kind = FX::ScriptFieldValue::Type::Entity;
    val.E    = b.GetUUID();
    nsc.Fields["Hedef"] = val;

    FX::Entity dupRoot = scene.DuplicateEntity(root);
    REQUIRE(dupRoot);

    FX::Entity dupA, dupB;
    for (FX::Entity c : dupRoot.GetChildren())
    {
        if (c.HasComponent<FX::NativeScriptComponent>()) dupA = c;
        else                                             dupB = c;
    }
    REQUIRE(dupA);
    REQUIRE(dupB);

    const FX::UUID ref = dupA.GetComponent<FX::NativeScriptComponent>().Fields.at("Hedef").E;
    CHECK(static_cast<std::uint64_t>(ref) == static_cast<std::uint64_t>(dupB.GetUUID()));
}
