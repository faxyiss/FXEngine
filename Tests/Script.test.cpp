#include <catch2/catch_test_macros.hpp>

#include <FXEngine/Scene/Scene.h>
#include <FXEngine/Scene/Entity.h>
#include <FXEngine/Scene/Components.h>
#include <FXEngine/Scene/ScriptableEntity.h>

namespace
{
    // Yasam dongusunu sayarak izleyen test script'i. Sayaclar statik
    // cunku ornegi motor yaratiyor; disaridan tutamak alamiyoruz.
    struct Counters
    {
        int Created   = 0;
        int Updated   = 0;
        int Destroyed = 0;
    };

    Counters g_Counters;

    class CountingScript : public FX::ScriptableEntity
    {
    protected:
        void OnCreate() override  { ++g_Counters.Created; }
        void OnUpdate(float) override { ++g_Counters.Updated; }
        void OnDestroy() override { ++g_Counters.Destroyed; }
    };

    // Script'in kendi entity'sine gercekten ulasabildigini dogrular.
    class MoveRightScript : public FX::ScriptableEntity
    {
    protected:
        void OnUpdate(float dt) override
        {
            GetComponent<FX::TransformComponent>().Translation.x += dt;
        }
    };
}

TEST_CASE("Script yalnizca Play ile Stop arasinda yasar", "[script]")
{
    g_Counters = {};

    FX::Scene scene;
    FX::Entity e = scene.CreateEntity("Scripted");
    e.AddComponent<FX::NativeScriptComponent>().Bind<CountingScript>("Counting");

    // Edit modu: OnUpdate cagrilsa bile script calismamali.
    scene.OnUpdate(0.016f);
    CHECK(g_Counters.Created == 0);
    CHECK(g_Counters.Updated == 0);

    scene.OnRuntimeStart();
    CHECK(g_Counters.Created == 1);

    scene.OnUpdate(0.016f);
    scene.OnUpdate(0.016f);
    CHECK(g_Counters.Updated == 2);

    scene.OnRuntimeStop();
    CHECK(g_Counters.Destroyed == 1);

    // Stop'tan sonra guncelleme script'e ulasmamali.
    scene.OnUpdate(0.016f);
    CHECK(g_Counters.Updated == 2);
}

TEST_CASE("Script kendi entity'sinin component'ini degistirebiliyor", "[script]")
{
    FX::Scene scene;
    FX::Entity e = scene.CreateEntity();
    e.AddComponent<FX::NativeScriptComponent>().Bind<MoveRightScript>("MoveRight");

    scene.OnRuntimeStart();
    scene.OnUpdate(0.5f);
    scene.OnRuntimeStop();

    CHECK(e.GetComponent<FX::TransformComponent>().Translation.x == 0.5f);
}

TEST_CASE("Bind edilmemis script cokmuyor", "[script]")
{
    // Inspector'dan component eklenip script secilmeden birakilabilir;
    // bu bir hata degil, henuz doldurulmamis bir alan.
    FX::Scene scene;
    FX::Entity e = scene.CreateEntity();
    e.AddComponent<FX::NativeScriptComponent>();

    scene.OnRuntimeStart();
    scene.OnUpdate(0.016f);
    scene.OnRuntimeStop();

    CHECK(e.GetComponent<FX::NativeScriptComponent>().Instance == nullptr);
}

TEST_CASE("Scene::Copy script ORNEGINI kopyalamiyor", "[script]")
{
    // Ham isaretci kopyalansaydi iki sahne ayni nesneyi gosterir ve
    // ikisi de silmeye calisirdi.
    g_Counters = {};

    FX::Scene source;
    FX::Entity e = source.CreateEntity();
    e.AddComponent<FX::NativeScriptComponent>().Bind<CountingScript>("Counting");

    source.OnRuntimeStart();
    REQUIRE(g_Counters.Created == 1);

    auto copy = FX::Scene::Copy(source);
    REQUIRE(copy);

    auto view = copy->GetRegistry().view<FX::NativeScriptComponent>();
    REQUIRE(view.size() == 1);

    for (auto handle : view)
    {
        // Baglanti kopyalandi, ornek kopyalanmadi.
        CHECK(view.get<FX::NativeScriptComponent>(handle).Instance == nullptr);
        CHECK(view.get<FX::NativeScriptComponent>(handle).ScriptName == "Counting");
    }

    // Kopya kendi ornegini yaratir.
    copy->OnRuntimeStart();
    CHECK(g_Counters.Created == 2);

    copy->OnRuntimeStop();
    source.OnRuntimeStop();
    CHECK(g_Counters.Destroyed == 2);
}

TEST_CASE("Sahne yok edilince script'ler temizleniyor", "[script]")
{
    g_Counters = {};

    {
        FX::Scene scene;
        FX::Entity e = scene.CreateEntity();
        e.AddComponent<FX::NativeScriptComponent>().Bind<CountingScript>("Counting");

        scene.OnRuntimeStart();
        REQUIRE(g_Counters.Created == 1);

        // OnRuntimeStop cagrilmadan yikiliyor: yikici halletmeli,
        // yoksa bellek sizardi.
    }

    CHECK(g_Counters.Destroyed == 1);
}
