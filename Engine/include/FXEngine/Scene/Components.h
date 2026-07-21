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
#include "FXEngine/Core/UUID.h"
#include "FXEngine/Scene/EntityRef.h"
#include "FXEngine/Scene/ScriptFields.h"

#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace FX
{
    // NativeScriptComponent yalnizca isaretci tasiyor; tam tanim
    // gerekmiyor ve Components.h'i hafif tutuyoruz.
    class ScriptableEntity;

    // -----------------------------------------------------------------------
    // IDComponent - entity'nin KALICI kimligi
    // -----------------------------------------------------------------------
    // Her entity'ye CreateEntity'de otomatik eklenir. TagComponent gibi
    // "istege bagli" degildir: kimliksiz bir entity serilestirilemez ve
    // referans verilemez.
    //
    // Neden ayri bir component, Entity sinifinin icinde bir alan degil?
    // Cunku Entity sadece bir TUTAMAK (handle) - veri tutmaz, registry'de
    // yasamaz. Kalici olmasi gereken her sey component olmali; ancak o
    // zaman serilestirilir, kopyalanir, sorgulanir.
    struct IDComponent
    {
        UUID ID;

        IDComponent() = default;
        IDComponent(UUID id) : ID(id) {}
    };

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
    // RelationshipComponent - hiyerarsi baglantilari
    // -----------------------------------------------------------------------
    // UUID tutuyoruz, entt::entity degil: yuklemeden sag cikmasi gerekiyor
    // (Faz 8'in sebebi). Cocuklari vector olarak saklamak bagli listeden
    // daha basit ve serilestirmesi dogrudan.
    struct RelationshipComponent
    {
        UUID              Parent{ 0 };
        std::vector<UUID> Children;
    };

    // Parent zinciri uygulanmis nihai matris. TransformSystem her karede
    // hesaplar; hicbir yerde elle yazilmaz.
    struct WorldTransformComponent
    {
        glm::mat4 Matrix{ 1.0f };
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

    // -----------------------------------------------------------------------
    // EntityRef artik kendi header'inda (FXEngine/Scene/EntityRef.h) -
    // ScriptFields.h de ona ihtiyac duydugu icin ortak alt basliga alindi.

    // -----------------------------------------------------------------------
    // FollowComponent - "su entity'yi takip et"
    // -----------------------------------------------------------------------
    // EntityRef'in ise yaradigini GOSTEREN ornek. Bir davranis degil,
    // sadece veri: hedef kim, ne kadar hizli, ne kadar yakina kadar.
    // Isi FollowSystem yapar.
    struct FollowComponent
    {
        EntityRef Target;
        float     Speed        = 2.0f;   // birim / saniye
        float     StopDistance = 1.0f;   // bu mesafeye gelince dur

        FollowComponent() = default;
        FollowComponent(UUID target, float speed = 2.0f)
            : Target(target), Speed(speed) {}
    };

    // -----------------------------------------------------------------------
    // CameraComponent - "bu entity bir kamera"
    // -----------------------------------------------------------------------
    // Play modunda sahneye BU kameradan bakilir. Konum ve donme
    // TransformComponent'ten gelir; burada sadece projeksiyon verisi var.
    //
    // Neden entity'ye bagli? Cunku kamera da sahnenin bir parcasi:
    // oyuncuyu takip etsin istiyorsan ona parent yaparsin, sarsinti
    // efekti istiyorsan transform'unu oynatirsin. Kamerayi sahnenin
    // disinda tutan motorlarda bunlarin hepsi ozel kod ister.
    struct CameraComponent
    {
        // Dikeyde kac birim gorunsun'un yarisi (OrthographicCamera ile ayni
        // anlam). En-boy orani viewport'tan gelir, burada saklanmaz:
        // ayni sahne farkli pencere boyutlarinda acilabilir.
        float OrthographicSize = 8.0f;

        // Birden fazla kamera olabilir; sahneyi hangisi cizecek?
        // Isaretlenmis ilki kazanir (Scene::GetPrimaryCameraEntity).
        bool Primary = true;

        CameraComponent() = default;
        CameraComponent(float size, bool primary = true)
            : OrthographicSize(size), Primary(primary) {}
    };

    // -----------------------------------------------------------------------
    // NativeScriptComponent - entity'ye C++ davranisi baglar (Faz 16a)
    // -----------------------------------------------------------------------
    // Component VERI kalmaya devam ediyor: icinde tek bir AD var.
    //
    // 16a'da burada fonksiyon isaretcileri duruyordu (Bind<T>). 16b'de
    // kaldirildi: sahne dosyasina fonksiyon isaretcisi yazilamaz, ad
    // yazilabilir. Adi sinifa cevirme isi ScriptRegistry'nin.
    //
    // Instance'in SAHIBI ScriptSystem: yaratmayi ve yok etmeyi o yapar,
    // cunku ne zaman yasayacaklari (yalnizca Play modunda) onun bilgisi.
    // Serilestirilmez - calisma zamani durumudur.
    struct NativeScriptComponent
    {
        std::string ScriptName;

        // Script alanlarinin editörde ayarlanan override degerleri
        // (ad -> deger). VERI: instance'ta degil burada durur, cunku DLL
        // yeniden yuklendiginde instance yok olup yeniden yaratiliyor.
        // Serilestirilir; Play'de OnCreate'ten once instance'a uygulanir.
        ScriptFieldMap Fields;

        ScriptableEntity* Instance = nullptr;

        NativeScriptComponent() = default;
        explicit NativeScriptComponent(std::string name)
            : ScriptName(std::move(name)) {}
    };

    // -----------------------------------------------------------------------
    // Component listesi burada DEGIL
    // -----------------------------------------------------------------------
    // A1'e kadar burada bir `AllComponents` tip listesi duruyordu; sahne
    // kopyalama onu kullaniyordu, serilestirme ve Inspector ise kendi
    // elle yazilmis listelerini. Uc liste er gec ayristi.
    //
    // Artik tek kaynak `ComponentMeta.h`'daki `ComponentRegistry`:
    // kopyalama, serilestirme ve Inspector uculu de oradan besleniyor.
    // Yeni bir component eklerken dokunulacak yer ComponentMeta.cpp'deki
    // RegisterBuiltins.
}
