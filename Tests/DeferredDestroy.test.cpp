#include <catch2/catch_test_macros.hpp>

#include <FXEngine/Scene/Components.h>
#include <FXEngine/Scene/Entity.h>
#include <FXEngine/Scene/Scene.h>
#include <FXEngine/Scene/ScriptRegistry.h>
#include <FXEngine/Scene/ScriptableEntity.h>

// A-1: script'ten entity silme. Silme ANINDA degil kare sonunda olmali;
// aksi halde ScriptSystem'in gezdigi view'in altindan dizi degisir.

namespace
{
    int g_OnDestroyCount = 0;

    // Ilk OnUpdate'te kendini siler.
    class SelfDestruct : public FX::ScriptableEntity
    {
    protected:
        void OnUpdate(float) override
        {
            Destroy();

            // Silme istegi verildikten SONRA da bu kare yasiyoruz:
            // component'e erisim gecerli olmali.
            GetComponent<FX::TransformComponent>().Translation.x = 5.0f;
        }

        void OnDestroy() override { ++g_OnDestroyCount; }
    };

    // Her karede "Kurban" adli entity'yi silmeye calisir.
    class Killer : public FX::ScriptableEntity
    {
    protected:
        void OnUpdate(float) override
        {
            if (FX::Entity victim = GetScene()->FindEntityByName("Kurban"))
                Destroy(victim);
        }
    };

    struct RegisterOnce
    {
        RegisterOnce()
        {
            FX::ScriptRegistry::Register<SelfDestruct>("__SelfDestruct");
            FX::ScriptRegistry::Register<Killer>("__Killer");
        }
    };

    void EnsureRegistered()
    {
        static RegisterOnce once;
        (void)once;
    }
}

TEST_CASE("Script kendini silebiliyor, silme kare sonunda oluyor", "[deferred]")
{
    EnsureRegistered();
    g_OnDestroyCount = 0;

    FX::Scene scene;
    FX::Entity e = scene.CreateEntity("Kendini Silen");
    const FX::UUID id = e.GetUUID();
    e.AddComponent<FX::NativeScriptComponent>().ScriptName = "__SelfDestruct";

    scene.OnRuntimeStart();
    scene.OnUpdate(1.0f / 60.0f);

    // Silme kare SONUNDA islendi: entity gitti ama script kendi
    // karesini tamamlayabildi (Translation.x yazilabildi).
    CHECK_FALSE(scene.FindEntityByUUID(id));
    CHECK(scene.GetEntityCount() == 0);

    // Oyun sirasinda silinen entity'nin OnDestroy'u da cagrilmali.
    CHECK(g_OnDestroyCount == 1);

    scene.OnRuntimeStop();
}

TEST_CASE("Script baska bir entity'yi silebiliyor", "[deferred]")
{
    EnsureRegistered();

    FX::Scene scene;

    FX::Entity killer = scene.CreateEntity("Katil");
    killer.AddComponent<FX::NativeScriptComponent>().ScriptName = "__Killer";

    FX::Entity victim = scene.CreateEntity("Kurban");
    const FX::UUID victimID = victim.GetUUID();

    scene.OnRuntimeStart();
    scene.OnUpdate(1.0f / 60.0f);

    CHECK_FALSE(scene.FindEntityByUUID(victimID));
    CHECK(scene.GetEntityCount() == 1);

    // Ikinci kare: hedef yok, cokmeden gecmeli.
    scene.OnUpdate(1.0f / 60.0f);
    CHECK(scene.GetEntityCount() == 1);

    scene.OnRuntimeStop();
}

TEST_CASE("Ayni entity iki kez silinmek istenirse sorun cikmiyor", "[deferred]")
{
    FX::Scene scene;
    FX::Entity e = scene.CreateEntity("Tek");
    const FX::UUID id = e.GetUUID();

    scene.DestroyEntityDeferred(e);
    scene.DestroyEntityDeferred(e);
    CHECK(scene.HasPendingDestroys());

    scene.FlushDestroyQueue();

    CHECK_FALSE(scene.FindEntityByUUID(id));
    CHECK(scene.GetEntityCount() == 0);
    CHECK_FALSE(scene.HasPendingDestroys());
}

TEST_CASE("Silinen parent'in cocugu kuyrukta kalsa bile cokmuyor", "[deferred]")
{
    FX::Scene scene;

    FX::Entity parent = scene.CreateEntity("Parent");
    FX::Entity child  = scene.CreateEntity("Child");
    child.SetParent(parent);

    // Cocuk parent'tan SONRA islenecek; parent silinince o da gitmis olacak.
    scene.DestroyEntityDeferred(parent);
    scene.DestroyEntityDeferred(child);

    scene.FlushDestroyQueue();

    CHECK(scene.GetEntityCount() == 0);
}

TEST_CASE("Silme istegi Edit modunda da calisiyor", "[deferred]")
{
    FX::Scene scene;
    FX::Entity e = scene.CreateEntity("Duran");

    scene.DestroyEntityDeferred(e);
    CHECK(scene.GetEntityCount() == 1);   // henuz islenmedi

    scene.OnUpdate(1.0f / 60.0f);         // OnUpdate kuyrugu bosaltir
    CHECK(scene.GetEntityCount() == 0);
}
