#include <catch2/catch_test_macros.hpp>

#include <FXEngine/Scene/ComponentMeta.h>
#include <FXEngine/Scene/Components.h>
#include <FXEngine/Scene/Entity.h>
#include <FXEngine/Scene/EntitySnapshot.h>
#include <FXEngine/Scene/Scene.h>

namespace
{
    constexpr FX::TextureLibrary* kNoTextures = nullptr;
}

TEST_CASE("Snapshot silinen entity'yi ayni UUID ile geri getiriyor", "[snapshot]")
{
    FX::Scene scene;

    FX::Entity e = scene.CreateEntity("Kahraman");
    const FX::UUID id = e.GetUUID();
    e.GetComponent<FX::TransformComponent>().Translation = { 3.0f, 4.0f, 0.0f };
    e.AddComponent<FX::VelocityComponent>().Linear = { 1.5f, -2.0f };

    const FX::EntitySnapshot snap = FX::EntitySnapshot::Capture(e);
    REQUIRE_FALSE(snap.Empty());

    scene.DestroyEntity(e);
    REQUIRE_FALSE(scene.FindEntityByUUID(id));

    FX::Entity back = snap.Restore(scene, kNoTextures);
    REQUIRE(back);

    // Kimlik korunmali: "benzeri" degil AYNI entity.
    CHECK(static_cast<std::uint64_t>(back.GetUUID()) == static_cast<std::uint64_t>(id));
    CHECK(back.GetName() == "Kahraman");
    CHECK(back.GetComponent<FX::TransformComponent>().Translation.x == 3.0f);
    CHECK(back.GetComponent<FX::TransformComponent>().Translation.y == 4.0f);
    REQUIRE(back.HasComponent<FX::VelocityComponent>());
    CHECK(back.GetComponent<FX::VelocityComponent>().Linear.y == -2.0f);
}

TEST_CASE("Snapshot alt agaci ve parent bagini koruyor", "[snapshot]")
{
    FX::Scene scene;

    FX::Entity root  = scene.CreateEntity("Kok");
    FX::Entity body  = scene.CreateEntity("Govde");
    FX::Entity head  = scene.CreateEntity("Kafa");
    body.SetParent(root);
    head.SetParent(body);

    const FX::UUID bodyID = body.GetUUID();
    const FX::UUID headID = head.GetUUID();

    // Silinen dal: govde + kafa. Kok yasamaya devam ediyor.
    const FX::EntitySnapshot snap = FX::EntitySnapshot::Capture(body);
    scene.DestroyEntity(body);

    CHECK_FALSE(scene.FindEntityByUUID(bodyID));
    CHECK_FALSE(scene.FindEntityByUUID(headID));   // parent silinince cocuk da gider

    FX::Entity backBody = snap.Restore(scene, kNoTextures);
    REQUIRE(backBody);

    FX::Entity backHead = scene.FindEntityByUUID(headID);
    REQUIRE(backHead);

    // Hem ic bag hem de disaridaki (hala yasayan) parent'a bag geri gelmeli.
    CHECK(backBody.GetParent() == root);
    CHECK(backHead.GetParent() == backBody);
    CHECK(root.GetChildren().size() == 1);
}

TEST_CASE("Snapshot iki kez geri konursa ikinci kopya olusmuyor", "[snapshot]")
{
    FX::Scene scene;

    FX::Entity e = scene.CreateEntity("Tek");
    const FX::EntitySnapshot snap = FX::EntitySnapshot::Capture(e);
    scene.DestroyEntity(e);

    REQUIRE(snap.Restore(scene, kNoTextures));
    REQUIRE(snap.Restore(scene, kNoTextures));
    CHECK(scene.GetEntityCount() == 1);
}

TEST_CASE("Component snapshot'i verisiyle birlikte geri geliyor", "[snapshot]")
{
    FX::Scene scene;

    FX::Entity e = scene.CreateEntity("Nesne");
    e.AddComponent<FX::VelocityComponent>().Linear = { 7.0f, 8.0f };

    FX::ComponentInfo* info = FX::ComponentRegistry::Find("Velocity");
    REQUIRE(info != nullptr);

    const FX::ComponentSnapshot snap = FX::ComponentSnapshot::Capture(e, *info);
    REQUIRE_FALSE(snap.Empty());

    info->Remove(e);
    REQUIRE_FALSE(e.HasComponent<FX::VelocityComponent>());

    REQUIRE(snap.Restore(e, kNoTextures));
    REQUIRE(e.HasComponent<FX::VelocityComponent>());

    // Sadece "component geri geldi" degil, DEGERI de geri gelmeli -
    // varsayilanla eklemek sessiz bir veri kaybi olurdu.
    CHECK(e.GetComponent<FX::VelocityComponent>().Linear.x == 7.0f);
    CHECK(e.GetComponent<FX::VelocityComponent>().Linear.y == 8.0f);
}

TEST_CASE("Sahip olunmayan component'in snapshot'i bos", "[snapshot]")
{
    FX::Scene scene;
    FX::Entity e = scene.CreateEntity("Bos");

    FX::ComponentInfo* info = FX::ComponentRegistry::Find("Velocity");
    REQUIRE(info != nullptr);

    CHECK(FX::ComponentSnapshot::Capture(e, *info).Empty());
}
