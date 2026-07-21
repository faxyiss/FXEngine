#include <catch2/catch_test_macros.hpp>

#include <FXEngine/Scene/Components.h>
#include <FXEngine/Scene/Entity.h>
#include <FXEngine/Scene/Scene.h>
#include <FXEngine/Scene/SceneSerializer.h>

#include "TestSupport.h"

// B-1: cocuk sirasi. MoveInParent siralamayi degistirmeli ve bu sira
// kaydet/yukle sonrasi korunmali (hiyerarsi sirali serilestirme).

namespace
{
    std::vector<std::string> ChildNames(FX::Entity parent)
    {
        std::vector<std::string> names;
        for (FX::Entity c : parent.GetChildren())
            names.push_back(c.GetName());
        return names;
    }
}

TEST_CASE("MoveInParent cocuk sirasini degistiriyor", "[order]")
{
    FX::Scene scene;
    FX::Entity parent = scene.CreateEntity("Parent");
    FX::Entity a = scene.CreateEntity("A");
    FX::Entity b = scene.CreateEntity("B");
    FX::Entity c = scene.CreateEntity("C");
    a.SetParent(parent);
    b.SetParent(parent);
    c.SetParent(parent);

    REQUIRE(ChildNames(parent) == std::vector<std::string>{ "A", "B", "C" });

    // C'yi bir yukari: A, C, B
    CHECK(c.MoveInParent(-1));
    CHECK(ChildNames(parent) == std::vector<std::string>{ "A", "C", "B" });

    // A'yi bir asagi: C, A, B
    CHECK(a.MoveInParent(+1));
    CHECK(ChildNames(parent) == std::vector<std::string>{ "C", "A", "B" });
}

TEST_CASE("MoveInParent uclarda is yapmiyor", "[order]")
{
    FX::Scene scene;
    FX::Entity parent = scene.CreateEntity("Parent");
    FX::Entity a = scene.CreateEntity("A");
    FX::Entity b = scene.CreateEntity("B");
    a.SetParent(parent);
    b.SetParent(parent);

    CHECK_FALSE(a.MoveInParent(-1));   // zaten en ustte
    CHECK_FALSE(b.MoveInParent(+1));   // zaten en altta
    CHECK(ChildNames(parent) == std::vector<std::string>{ "A", "B" });

    // Kokun parent'i yok: is yapmamali, cokmemeli.
    CHECK_FALSE(parent.MoveInParent(-1));
}

TEST_CASE("Cocuk sirasi kaydet/yukle sonrasi korunuyor", "[order][serializer]")
{
    FXTest::TempProject project;

    FX::UUID parentID;
    {
        FX::Scene scene;
        FX::Entity parent = scene.CreateEntity("Parent");
        FX::Entity a = scene.CreateEntity("A");
        FX::Entity b = scene.CreateEntity("B");
        FX::Entity c = scene.CreateEntity("C");
        a.SetParent(parent);
        b.SetParent(parent);
        c.SetParent(parent);
        parentID = parent.GetUUID();

        // Sirayi ters cevir: C, B, A
        c.MoveInParent(-1);   // A, C, B
        c.MoveInParent(-1);   // C, A, B
        b.MoveInParent(-1);   // C, B, A
        REQUIRE(ChildNames(parent) == std::vector<std::string>{ "C", "B", "A" });

        FX::SceneSerializer serializer(&scene, nullptr);
        REQUIRE(serializer.Serialize("assets/scenes/order.fxscene"));
    }

    FX::Scene loaded;
    FX::SceneSerializer serializer(&loaded, nullptr);
    REQUIRE(serializer.Deserialize("assets/scenes/order.fxscene"));

    FX::Entity parent = loaded.FindEntityByUUID(parentID);
    REQUIRE(parent);

    // Yeniden siralama kaydedildi mi?
    CHECK(ChildNames(parent) == std::vector<std::string>{ "C", "B", "A" });
}
