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

namespace
{
    std::vector<std::string> RootNames(FX::Scene& scene)
    {
        std::vector<std::string> names;
        for (FX::Entity e : scene.GetRootEntities())
            names.push_back(e.GetName());
        return names;
    }
}

TEST_CASE("Kok sirasi MoveInParent ile degisiyor", "[order]")
{
    FX::Scene scene;
    FX::Entity a = scene.CreateEntity("A");
    FX::Entity b = scene.CreateEntity("B");
    FX::Entity cam = scene.CreateEntity("Kamera");

    REQUIRE(RootNames(scene) == std::vector<std::string>{ "A", "B", "Kamera" });

    // Kamerayi en uste: iki kez yukari.
    CHECK(cam.MoveInParent(-1));   // A, Kamera, B
    CHECK(cam.MoveInParent(-1));   // Kamera, A, B
    CHECK(RootNames(scene) == std::vector<std::string>{ "Kamera", "A", "B" });

    // En ustte bir daha yukari: is yok.
    CHECK_FALSE(cam.MoveInParent(-1));
}

TEST_CASE("Parent kazanan entity kok sirasindan cikiyor", "[order]")
{
    FX::Scene scene;
    FX::Entity a = scene.CreateEntity("A");
    FX::Entity b = scene.CreateEntity("B");

    CHECK(RootNames(scene) == std::vector<std::string>{ "A", "B" });

    b.SetParent(a);
    CHECK(RootNames(scene) == std::vector<std::string>{ "A" });

    // Koke geri don: sona eklenir.
    FX::Entity c = scene.CreateEntity("C");
    b.SetParent({});
    CHECK(RootNames(scene) == std::vector<std::string>{ "A", "C", "B" });
}

TEST_CASE("Kok sirasi kaydet/yukle sonrasi korunuyor", "[order][serializer]")
{
    FXTest::TempProject project;

    {
        FX::Scene scene;
        FX::Entity a = scene.CreateEntity("A");
        FX::Entity b = scene.CreateEntity("B");
        FX::Entity cam = scene.CreateEntity("Kamera");
        cam.MoveInParent(-1);
        cam.MoveInParent(-1);   // Kamera, A, B
        REQUIRE(RootNames(scene) == std::vector<std::string>{ "Kamera", "A", "B" });

        FX::SceneSerializer serializer(&scene, nullptr);
        REQUIRE(serializer.Serialize("assets/scenes/roots.fxscene"));
    }

    FX::Scene loaded;
    FX::SceneSerializer serializer(&loaded, nullptr);
    REQUIRE(serializer.Deserialize("assets/scenes/roots.fxscene"));

    CHECK(RootNames(loaded) == std::vector<std::string>{ "Kamera", "A", "B" });
}

TEST_CASE("PlaceEntity kardes listesinde konuma tasiyor", "[order][place]")
{
    FX::Scene scene;
    FX::Entity p = scene.CreateEntity("P");
    FX::Entity a = scene.CreateEntity("A");
    FX::Entity b = scene.CreateEntity("B");
    FX::Entity c = scene.CreateEntity("C");
    a.SetParent(p);
    b.SetParent(p);
    c.SetParent(p);   // P: A, B, C

    // C'yi 0. konuma (en basa).
    scene.PlaceEntity(c, p, 0);
    CHECK(ChildNames(p) == std::vector<std::string>{ "C", "A", "B" });

    // A'yi sona (index kirpilir).
    scene.PlaceEntity(a, p, 99);
    CHECK(ChildNames(p) == std::vector<std::string>{ "C", "B", "A" });
}

TEST_CASE("PlaceEntity kok ile cocuk arasinda tasiyor", "[order][place]")
{
    FX::Scene scene;
    FX::Entity p = scene.CreateEntity("P");
    FX::Entity r = scene.CreateEntity("R");   // ayri kok

    // R'yi P'nin cocugu yap (0. konum).
    scene.PlaceEntity(r, p, 0);
    CHECK(r.GetParent() == p);
    CHECK(ChildNames(p) == std::vector<std::string>{ "R" });
    CHECK(RootNames(scene) == std::vector<std::string>{ "P" });

    // R'yi tekrar koke al (kok listesinde 0. konuma).
    scene.PlaceEntity(r, {}, 0);
    CHECK_FALSE(r.GetParent());
    CHECK(RootNames(scene) == std::vector<std::string>{ "R", "P" });
}

TEST_CASE("PlaceEntity dongusel tasimayi reddediyor", "[order][place]")
{
    FX::Scene scene;
    FX::Entity p = scene.CreateEntity("P");
    FX::Entity c = scene.CreateEntity("C");
    c.SetParent(p);

    // P'yi kendi cocugunun altina tasima girisimi: reddedilmeli.
    scene.PlaceEntity(p, c, 0);
    CHECK_FALSE(p.GetParent());          // hala kok
    CHECK(c.GetParent() == p);           // c hala p'nin cocugu
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
