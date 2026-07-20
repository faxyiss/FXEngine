#include "FXEngine/Scene/Systems.h"
#include "FXEngine/Scene/Scene.h"
#include "FXEngine/Scene/Entity.h"
#include "FXEngine/Scene/Components.h"
#include "FXEngine/Renderer/Renderer2D.h"

#include <cmath>

namespace FX
{
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

    void SpriteRenderSystem::Render(entt::registry& registry, const OrthographicCamera& camera)
    {
        Renderer2D::BeginScene(camera);

        auto view = registry.view<TransformComponent, SpriteRendererComponent>();

        for (auto entity : view)
        {
            auto [transform, sprite] = view.get<TransformComponent, SpriteRendererComponent>(entity);

            // Donme yoksa hizli yolu sec: sin/cos ve fazladan matris
            // carpimi atlanir. Cogu sprite dondurulmez, bu ayrim
            // binlerce entity'de olculebilir fark yaratir.
            //
            // Not: "== 0.0f" ile float karsilastirmasi genelde kotu bir
            // aliskanliktir, ama burada dogru: donmenin TAM OLARAK sifir
            // olup olmadigini soruyoruz, bir hesabin sonucunu degil.
            // Kucuk bir aci (0.0001) hizli yola girmezse zarari da yok.
            const bool rotated = (transform.Rotation != 0.0f);
            const int  id      = static_cast<int>(entity);

            if (sprite.Texture)
            {
                if (rotated)
                    Renderer2D::DrawRotatedQuad(glm::vec2(transform.Translation),
                                                transform.Scale, transform.Rotation,
                                                sprite.Texture, sprite.TilingFactor,
                                                sprite.Color, id);
                else
                    Renderer2D::DrawQuad(transform.Translation, transform.Scale,
                                         sprite.Texture, sprite.TilingFactor,
                                         sprite.Color, id);
            }
            else
            {
                // Texture yok -> duz renk. Renderer2D iceride beyaz
                // texture kullaniyor, yani bu da AYNI batch'e giriyor.
                if (rotated)
                    Renderer2D::DrawRotatedQuad(glm::vec2(transform.Translation),
                                                transform.Scale, transform.Rotation,
                                                sprite.Color, id);
                else
                    Renderer2D::DrawQuad(transform.Translation, transform.Scale,
                                         sprite.Color, id);
            }
        }

        Renderer2D::EndScene();
    }
}
