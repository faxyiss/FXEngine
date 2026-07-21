#include "FXEngine/Scene/Systems.h"
#include "FXEngine/Scene/Scene.h"
#include "FXEngine/Scene/Entity.h"
#include "FXEngine/Scene/Components.h"
#include "FXEngine/Scene/ScriptableEntity.h"
#include "FXEngine/Scene/ScriptRegistry.h"
#include "FXEngine/Renderer/Renderer2D.h"
#include "FXEngine/Core/Log.h"

#include <cmath>

namespace FX
{
    namespace
    {
        void UpdateWorldRecursive(Entity entity, const glm::mat4& parentWorld)
        {
            auto& tc = entity.GetComponent<TransformComponent>();
            const glm::mat4 world = parentWorld * tc.GetTransform();

            entity.AddOrReplaceIfMissing<WorldTransformComponent>().Matrix = world;

            for (Entity child : entity.GetChildren())
                UpdateWorldRecursive(child, world);
        }
    }

    void TransformSystem::Update(Scene& scene)
    {
        auto& registry = scene.GetRegistry();

        // Sadece KOKLERDEN basliyoruz; cocuklara ozyineleme ile iniyoruz.
        // Tum entity'ler uzerinde duz gezseydik bir cocugu parent'indan
        // once isleyebilirdik ve dunya matrisi bir kare geride kalirdi.
        auto view = registry.view<TransformComponent>();
        for (auto id : view)
        {
            Entity e{ id, &scene };

            const bool isRoot = !e.HasComponent<RelationshipComponent>() ||
                                !e.GetComponent<RelationshipComponent>().Parent.IsValid();

            if (isRoot)
                UpdateWorldRecursive(e, glm::mat4(1.0f));
        }
    }

    void FollowSystem::Update(Scene& scene, float /*dt*/)
    {
        auto& registry = scene.GetRegistry();

        auto view = registry.view<TransformComponent, FollowComponent, VelocityComponent>();

        for (auto entityID : view)
        {
            auto [transform, follow, velocity] =
                view.get<TransformComponent, FollowComponent, VelocityComponent>(entityID);

            if (!follow.Target.IsSet())
                continue;

            // UUID -> Entity cozumlemesi HER KAREDE yapiliyor.
            //
            // "Bir kez cozup isaretciyi saklasak daha hizli olmaz mi?"
            // Olurdu - ama hedef silindiginde elimizde olu bir tutamak
            // kalirdi ve bunu fark etmezdik. Gec cozumleme, gecersiz
            // referansi ANINDA gorunur kilar: arama basarisiz olur, sistem
            // sessizce atlar. O(1) harita aramasi bu guvenligin makul bedeli.
            //
            // Bu, "erken optimizasyon yapma" kuralinin dogru uygulamasi:
            // once dogru olani yap, profiling darbogaz gosterirse onbellekle.
            Entity target = scene.FindEntityByUUID(follow.Target.Target);

            if (!target || !target.HasComponent<TransformComponent>())
            {
                // Hedef yok (silinmis veya sahne dosyasi bozuk).
                // Cokmuyoruz, sadece durduruyoruz - kayip bir referans
                // programi oldurmemeli.
                velocity.Linear = { 0.0f, 0.0f };
                continue;
            }

            const auto& targetTf = target.GetComponent<TransformComponent>();

            const float dx = targetTf.Translation.x - transform.Translation.x;
            const float dy = targetTf.Translation.y - transform.Translation.y;
            const float dist = std::sqrt(dx * dx + dy * dy);

            if (dist <= follow.StopDistance || dist < 0.0001f)
            {
                // Yeterince yakin. dist kontrolu ayrica SIFIRA BOLMEYI de
                // onluyor: iki nesne tam ust uste gelirse normalize NaN
                // uretir ve nesne sonsuza gider.
                velocity.Linear = { 0.0f, 0.0f };
                continue;
            }

            // Hedefe dogru birim vektor x hiz.
            // Konumu DOGRUDAN degistirmiyoruz; Velocity'ye yaziyoruz ve
            // MovementSystem uyguluyor. Faz 5'teki oyuncu kontrolunun
            // ayni kalibi: sistemler birbirini bilmez, ayni veriye dokunur.
            velocity.Linear = { (dx / dist) * follow.Speed,
                                (dy / dist) * follow.Speed };
        }
    }

    void MovementSystem::Update(entt::registry& registry, float dt)
    {
        // VIEW: "su component'lerin HEPSINE sahip entity'leri ver".
        //
        // Bu, ECS'in en onemli araci. Iki sey birden yapar:
        //   1) Filtreleme  - sadece ilgili entity'ler
        //   2) Onbellek dostu erisim - EnTT bu component'leri bitisik
        //      dizilerde tutar, uzerlerinde gezmek RAM'de duz bir yuruyustur
        //
        // 10.000 duvarin arasindaki 5 hareketli nesneyi ararken hicbir
        // duvara DOKUNMAYIZ bile - duvarlarda VelocityComponent yok.
        auto view = registry.view<TransformComponent, VelocityComponent>();

        for (auto entity : view)
        {
            // get<A, B> tek cagrida ikisini birden dondurur.
            // Yapisal baglama (structured binding) ile ayiriyoruz.
            auto [transform, velocity] = view.get<TransformComponent, VelocityComponent>(entity);

            // dt ile carpim: "saniyede 3 birim" -> kare hizindan bagimsiz.
            transform.Translation.x += velocity.Linear.x * dt;
            transform.Translation.y += velocity.Linear.y * dt;
            transform.Rotation      += velocity.Angular  * dt;
        }
    }

    void ScriptSystem::OnRuntimeStart(Scene& scene)
    {
        auto view = scene.GetRegistry().view<NativeScriptComponent>();
        for (auto entityID : view)
        {
            auto& nsc = view.get<NativeScriptComponent>(entityID);

            // Script secilmemis olabilir; eksik script bir hata degil,
            // henuz doldurulmamis bir alan.
            if (nsc.ScriptName.empty() || nsc.Instance)
                continue;

            nsc.Instance = ScriptRegistry::Create(nsc.ScriptName);
            if (!nsc.Instance)
            {
                // Sahne dosyasindaki script bu derlemede kayitli degil.
                // SESSIZ GECMEK en kotusu olurdu: kullanici "script neden
                // calismiyor?" diye saatlerce arar.
                FX_CORE_WARN("ScriptSystem: '%s' kayitli degil, atlandi.",
                             nsc.ScriptName.c_str());
                continue;
            }

            nsc.Instance->m_Entity = Entity{ entityID, &scene };
            nsc.Instance->OnCreate();
        }
    }

    void ScriptSystem::Update(Scene& scene, float dt)
    {
        auto view = scene.GetRegistry().view<NativeScriptComponent>();
        for (auto entityID : view)
        {
            auto& nsc = view.get<NativeScriptComponent>(entityID);
            if (nsc.Instance)
                nsc.Instance->OnUpdate(dt);
        }
    }

    void ScriptSystem::OnRuntimeStop(Scene& scene)
    {
        auto view = scene.GetRegistry().view<NativeScriptComponent>();
        for (auto entityID : view)
        {
            auto& nsc = view.get<NativeScriptComponent>(entityID);
            if (!nsc.Instance)
                continue;

            nsc.Instance->OnDestroy();
            delete nsc.Instance;
            nsc.Instance = nullptr;
        }
    }

    void SpriteRenderSystem::Render(entt::registry& registry, const OrthographicCamera& camera)
    {
        Renderer2D::BeginScene(camera);

        // WorldTransformComponent'i TransformSystem dolduruyor; burada
        // yerel transform'a hic bakmiyoruz.
        auto view = registry.view<WorldTransformComponent, SpriteRendererComponent>();

        for (auto entity : view)
        {
            auto [world, sprite] =
                view.get<WorldTransformComponent, SpriteRendererComponent>(entity);

            const int id = static_cast<int>(entity);

            if (sprite.Texture)
                Renderer2D::DrawQuad(world.Matrix, sprite.Texture,
                                     sprite.TilingFactor, sprite.Color, id);
            else
                Renderer2D::DrawQuad(world.Matrix, sprite.Color, id);
        }

        Renderer2D::EndScene();
    }
}
