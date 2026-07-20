#include <catch2/catch_test_macros.hpp>

#include <FXEngine/Scene/Scene.h>
#include <FXEngine/Scene/Entity.h>
#include <FXEngine/Scene/Components.h>

TEST_CASE("CreateEntity kimlik, ad ve transform verir", "[scene]")
{
    FX::Scene scene;
    FX::Entity e = scene.CreateEntity("Oyuncu");

    CHECK(e.HasComponent<FX::IDComponent>());
    CHECK(e.HasComponent<FX::TagComponent>());
    CHECK(e.HasComponent<FX::TransformComponent>());
    CHECK(e.GetComponent<FX::TagComponent>().Tag == "Oyuncu");
    CHECK(scene.GetEntityCount() == 1);
}

TEST_CASE("FindEntityByUUID olusturulan entity'yi bulur", "[scene]")
{
    FX::Scene scene;
    FX::Entity e = scene.CreateEntity();

    const FX::UUID uuid = e.GetComponent<FX::IDComponent>().ID;

    CHECK(scene.FindEntityByUUID(uuid) == e);
    CHECK_FALSE(scene.FindEntityByUUID(FX::UUID{ 999 }));
}

TEST_CASE("CreateEntityWithUUID verilen kimligi korur", "[scene]")
{
    // Sahne yuklemenin dayandigi ozellik: dosyadaki kimlik aynen geri gelir.
    FX::Scene scene;
    const FX::UUID wanted{ 4242 };

    FX::Entity e = scene.CreateEntityWithUUID(wanted, "Yuklenen");

    CHECK(static_cast<std::uint64_t>(e.GetComponent<FX::IDComponent>().ID) == 4242);
    CHECK(scene.FindEntityByUUID(wanted) == e);
}

TEST_CASE("DestroyEntity haritayi da temizler", "[scene]")
{
    FX::Scene scene;
    FX::Entity e = scene.CreateEntity();
    const FX::UUID uuid = e.GetComponent<FX::IDComponent>().ID;

    scene.DestroyEntity(e);

    CHECK(scene.GetEntityCount() == 0);
    CHECK_FALSE(scene.FindEntityByUUID(uuid));
}

TEST_CASE("SetParent iki yonlu bag kurar", "[scene][hiyerarsi]")
{
    FX::Scene scene;
    FX::Entity parent = scene.CreateEntity("Parent");
    FX::Entity child  = scene.CreateEntity("Child");

    child.SetParent(parent);

    CHECK(child.GetParent() == parent);
    CHECK(parent.HasChildren());
    REQUIRE(parent.GetChildren().size() == 1);
    CHECK(parent.GetChildren()[0] == child);
    CHECK(parent.IsAncestorOf(child));
    CHECK_FALSE(child.IsAncestorOf(parent));
}

TEST_CASE("Torun de ata sayilir", "[scene][hiyerarsi]")
{
    FX::Scene scene;
    FX::Entity a = scene.CreateEntity("A");
    FX::Entity b = scene.CreateEntity("B");
    FX::Entity c = scene.CreateEntity("C");

    b.SetParent(a);
    c.SetParent(b);

    CHECK(a.IsAncestorOf(c));
}

TEST_CASE("Parent silinince cocuklari da gider", "[scene][hiyerarsi]")
{
    // Unity/Godot davranisi. Cocuk sahnede oksuz kalsaydi kullanici
    // silinmis sandigi nesneleri bulamazdi.
    FX::Scene scene;
    FX::Entity parent = scene.CreateEntity("Parent");
    FX::Entity child  = scene.CreateEntity("Child");
    child.SetParent(parent);

    const FX::UUID childID = child.GetComponent<FX::IDComponent>().ID;

    scene.DestroyEntity(parent);

    CHECK(scene.GetEntityCount() == 0);
    CHECK_FALSE(scene.FindEntityByUUID(childID));
}

TEST_CASE("Scene::Copy UUID'leri korur", "[scene][play]")
{
    // Play modunun temeli: kopya sahnedeki entity'ler AYNI kimligi tasir,
    // yoksa referanslar (EntityRef) kopardi.
    FX::Scene source;
    FX::Entity a = source.CreateEntity("A");
    FX::Entity b = source.CreateEntity("B");
    b.SetParent(a);

    const FX::UUID idA = a.GetComponent<FX::IDComponent>().ID;
    const FX::UUID idB = b.GetComponent<FX::IDComponent>().ID;

    auto copy = FX::Scene::Copy(source);
    REQUIRE(copy);

    CHECK(copy->GetEntityCount() == 2);

    FX::Entity copyA = copy->FindEntityByUUID(idA);
    FX::Entity copyB = copy->FindEntityByUUID(idB);

    REQUIRE(copyA);
    REQUIRE(copyB);
    CHECK(copyA.GetComponent<FX::TagComponent>().Tag == "A");
    CHECK(copyB.GetParent() == copyA);
}
