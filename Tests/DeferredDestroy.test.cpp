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

    int g_SpawnedCreated = 0;

    // Spawn edilen ornegin OnCreate'inin gercekten calistigini sayar.
    class Bullet : public FX::ScriptableEntity
    {
    protected:
        void OnCreate() override { ++g_SpawnedCreated; }
    };

    // Ilk OnUpdate'te bir kez "Prototip"i spawn eder.
    class Spawner : public FX::ScriptableEntity
    {
    public:
        bool m_Done = false;
    protected:
        void OnUpdate(float) override
        {
            if (m_Done)
                return;
            m_Done = true;

            FX::Entity proto = GetScene()->FindEntityByName("Prototip");
            FX::Entity spawned = Instantiate(proto);

            // Aninda gecerli bir handle donmeli ve hemen ayarlanabilmeli.
            if (spawned)
                spawned.GetComponent<FX::TransformComponent>().Translation.x = 7.0f;
        }
    };

    struct RegisterOnce
    {
        RegisterOnce()
        {
            FX::ScriptRegistry::Register<SelfDestruct>("__SelfDestruct");
            FX::ScriptRegistry::Register<Killer>("__Killer");
            FX::ScriptRegistry::Register<Bullet>("__Bullet");
            FX::ScriptRegistry::Register<Spawner>("__Spawner");
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

TEST_CASE("Script prototip'ten runtime spawn edebiliyor", "[deferred][spawn]")
{
    EnsureRegistered();
    g_SpawnedCreated = 0;

    FX::Scene scene;

    FX::Entity proto = scene.CreateEntity("Prototip");
    proto.AddComponent<FX::NativeScriptComponent>().ScriptName = "__Bullet";

    FX::Entity spawner = scene.CreateEntity("Spawner");
    spawner.AddComponent<FX::NativeScriptComponent>().ScriptName = "__Spawner";

    scene.OnRuntimeStart();
    REQUIRE(scene.GetEntityCount() == 2);

    // Prototip'in KENDI Bullet script'i de Play'de calisir (sahnedeki bir
    // entity); OnCreate'i baslangicta bir kez cagrilir. Spawn sayimini
    // yalitmak icin sayaci simdi sifirliyoruz.
    CHECK(g_SpawnedCreated == 1);
    g_SpawnedCreated = 0;

    // Kare 1: spawner prototipi kopyalar. Spawn edilen entity ayni karede
    // var olmali; OnCreate karenin SONUNDA (StartPending) calismali.
    scene.OnUpdate(1.0f / 60.0f);

    CHECK(scene.GetEntityCount() == 3);
    CHECK(g_SpawnedCreated == 1);

    // Spawn edilen kopya prototip gibi __Bullet script'i tasimali ve
    // spawner'in ayarladigi konumu korumali.
    FX::Entity bullet;
    auto view = scene.GetRegistry().view<FX::TagComponent>();
    for (auto id : view)
    {
        FX::Entity e{ id, &scene };
        if (e.GetName() == "Prototip" && e != proto) { bullet = e; break; }
    }
    REQUIRE(bullet);
    CHECK(bullet.GetComponent<FX::TransformComponent>().Translation.x == 7.0f);

    // Kare 2: bir daha spawn YOK (spawner m_Done). OnCreate tekrar cagrilmaz.
    scene.OnUpdate(1.0f / 60.0f);
    CHECK(scene.GetEntityCount() == 3);
    CHECK(g_SpawnedCreated == 1);

    scene.OnRuntimeStop();
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
