#include "FXEngine/Scene/Systems.h"
#include "FXEngine/Scene/Components.h"
#include "FXEngine/Renderer/Renderer2D.h"

namespace FX
{
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

            if (sprite.Texture)
            {
                if (rotated)
                    Renderer2D::DrawRotatedQuad(glm::vec2(transform.Translation),
                                                transform.Scale, transform.Rotation,
                                                sprite.Texture, sprite.TilingFactor, sprite.Color);
                else
                    Renderer2D::DrawQuad(transform.Translation, transform.Scale,
                                         sprite.Texture, sprite.TilingFactor, sprite.Color);
            }
            else
            {
                // Texture yok -> duz renk. Renderer2D iceride beyaz
                // texture kullaniyor, yani bu da AYNI batch'e giriyor.
                if (rotated)
                    Renderer2D::DrawRotatedQuad(glm::vec2(transform.Translation),
                                                transform.Scale, transform.Rotation, sprite.Color);
                else
                    Renderer2D::DrawQuad(transform.Translation, transform.Scale, sprite.Color);
            }
        }

        Renderer2D::EndScene();
    }
}
