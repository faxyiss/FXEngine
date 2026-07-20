#pragma once

// ===========================================================================
// FXEngine - Component'ler
//
// TEMEL KURAL: Component SAF VERIDIR. Metot yok, mantik yok, davranis yok.
//
// "Peki TransformComponent::GetTransform() ne oluyor?" - o bir DONUSUM,
// davranis degil: ayni veriyi baska bir bicimde sunuyor, hicbir sey
// degistirmiyor ve hicbir sisteme bagimli degil. Sinir cizgisi sudur:
// bir metot BASKA component'lere veya sahneye dokunuyorsa, o bir
// SYSTEM'e ait demektir.
//
// Neden bu kadar kati? Cunku veri ile davranisi ayirmak ECS'in tum
// faydasinin kaynagi: veri serilestirilebilir (Faz 7), sistemler
// bagimsiz test edilebilir, ayni veri farkli sistemlerce islenebilir.
// ===========================================================================

#include "FXEngine/Renderer/Texture.h"

#include <memory>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace FX
{
    // -----------------------------------------------------------------------
    // TagComponent - entity'nin okunabilir adi
    // -----------------------------------------------------------------------
    // Motorun calismasi icin GEREKLI DEGIL - entity zaten bir sayiyla
    // tanimli. Ama Faz 6'daki Hierarchy panelinde "Entity 4294967297"
    // yerine "Oyuncu" gormek isteriz. Editor'un varligi motorun veri
    // modelini sekillendiriyor; bu normaldir.
    struct TagComponent
    {
        std::string Tag;

        TagComponent() = default;
        TagComponent(const std::string& tag) : Tag(tag) {}
    };

    // -----------------------------------------------------------------------
    // TransformComponent - konum, donme, olcek
    // -----------------------------------------------------------------------
    struct TransformComponent
    {
        glm::vec3 Translation{ 0.0f, 0.0f, 0.0f };

        // RADYAN cinsinden. Neden derece degil?
        // Cunku matematik fonksiyonlari (sin, cos, glm::rotate) radyan
        // bekler. Ic veriyi radyan tutup SADECE arayuzde dereceye cevirmek,
        // her hesapta donusum yapmaktan hem hizli hem hatasizdir.
        // (Faz 6'da Inspector paneli derece gosterecek, cevrimi orada yapacagiz.)
        float Rotation = 0.0f;

        // 2D'de Z olcegi anlamsiz -> vec2 yeterli.
        // Kucuk bir karar ama her entity'de 4 bayt tasarruf ve
        // "bu 2D bir motor" mesajini kodun kendisi veriyor.
        glm::vec2 Scale{ 1.0f, 1.0f };

        TransformComponent() = default;
        TransformComponent(const glm::vec3& translation) : Translation(translation) {}
        TransformComponent(const glm::vec2& translation)
            : Translation(translation.x, translation.y, 0.0f) {}

        // Bu bir DONUSUM, davranis degil: ayni veriyi matris bicimine cevirir.
        // Hicbir sey degistirmez, hicbir seye bagimli degildir.
        glm::mat4 GetTransform() const
        {
            return glm::translate(glm::mat4(1.0f), Translation) *
                   glm::rotate(glm::mat4(1.0f), Rotation, { 0.0f, 0.0f, 1.0f }) *
                   glm::scale(glm::mat4(1.0f), { Scale.x, Scale.y, 1.0f });
        }
    };

    // -----------------------------------------------------------------------
    // SpriteRendererComponent - "bu entity ciziliyor"
    // -----------------------------------------------------------------------
    // Bu component'in VARLIGI, entity'nin cizilecegi anlamina gelir.
    // Bir entity'yi gizlemek icin bir "Visible" bayragi degil, component'i
    // KALDIRMAK da bir secenektir - ECS'te "yetenek" ile "durum" arasindaki
    // fark budur.
    struct SpriteRendererComponent
    {
        glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };

        // nullptr ise duz renk cizilir (Renderer2D beyaz texture kullanir).
        // shared_ptr cunku ayni texture'i yuzlerce entity paylasabilir;
        // her biri kendi kopyasini tutsaydi bellek israfi olurdu.
        std::shared_ptr<Texture2D> Texture;

        float TilingFactor = 1.0f;

        SpriteRendererComponent() = default;
        SpriteRendererComponent(const glm::vec4& color) : Color(color) {}
        SpriteRendererComponent(const std::shared_ptr<Texture2D>& texture,
                                const glm::vec4& tint = glm::vec4(1.0f))
            : Color(tint), Texture(texture) {}
    };

    // -----------------------------------------------------------------------
    // VelocityComponent - hareket verisi
    // -----------------------------------------------------------------------
    // MovementSystem bunu okur ve Transform'u gunceller.
    //
    // Neden Transform'un icine "velocity" alani koymadik?
    // Cunku her entity hareket etmez. Duvarlar, zemin, dekorlar sabittir.
    // Ayri component olunca MovementSystem SADECE hareket edenler uzerinde
    // gezer - 10.000 duvarin arasinda 5 hareketli nesne ararken zaman
    // kaybetmez. ECS'te "hangi component'ler bir arada" sorusu bir
    // performans kararidir.
    struct VelocityComponent
    {
        glm::vec2 Linear{ 0.0f, 0.0f };   // birim / saniye
        float     Angular = 0.0f;         // radyan / saniye

        VelocityComponent() = default;
        VelocityComponent(const glm::vec2& linear, float angular = 0.0f)
            : Linear(linear), Angular(angular) {}
    };
}
