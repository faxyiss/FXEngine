#pragma once

// ===========================================================================
// FXEngine - System'ler
//
// DAVRANIS BURADA YASAR. Component'ler veri, system'ler mantik.
//
// Her sistem su kaliba uyar:
//   1) Registry'den bir "view" al: belirli component'lere sahip entity'ler
//   2) Uzerlerinde gez
//   3) Veriyi oku / yaz
//
// Sistemlerin DURUMU YOKTUR (static fonksiyonlar). Ayni girdi -> ayni cikti.
// Bu, onlari tek baslarina test edilebilir kilar: sahte bir registry
// olusturup sistemi calistirmak yeterli, motorun geri kalanina gerek yok.
// ===========================================================================

#include "FXEngine/Renderer/OrthographicCamera.h"

#include <entt/entt.hpp>

namespace FX
{
    class Scene;

    // -----------------------------------------------------------------------
    // TransformSystem - hiyerarsiyi gezip dunya matrislerini hesaplar
    // -----------------------------------------------------------------------
    // Kokten yapraga (top-down) gider: bir cocugun dunya matrisi
    // parent'inki hesaplanmadan bulunamaz.
    class TransformSystem
    {
    public:
        static void Update(Scene& scene);
    };

    // -----------------------------------------------------------------------
    // FollowSystem - FollowComponent'in hedefine dogru hiz yazar
    // -----------------------------------------------------------------------
    // Faz 8'in kanit sistemi: UUID ile kurulmus referanslarin gercekten
    // calistigini gosterir.
    //
    // DIKKAT: Bu sistem digerlerinden farkli olarak registry degil SCENE
    // aliyor. Sebep: UUID cozumlemesi icin Scene::FindEntityByUUID gerekiyor
    // ve o harita registry'de degil, Scene'de yasiyor.
    //
    // Bu bir tasarim gerilimi: sistemler ideal olarak sadece registry'ye
    // bagimli olmali (test etmesi kolay olsun diye). Ama referans
    // cozumlemesi sahne capinda bir islem. Kabul edip acikca yaziyoruz.
    class FollowSystem
    {
    public:
        static void Update(Scene& scene, float dt);
    };

    // -----------------------------------------------------------------------
    // MovementSystem - Velocity'yi Transform'a uygular
    // -----------------------------------------------------------------------
    class MovementSystem
    {
    public:
        // dt SABIT adimla gelir (Application'daki fixed timestep).
        // Bu yuzden hareket her makinede birebir aynidir.
        static void Update(entt::registry& registry, float dt);
    };

    // -----------------------------------------------------------------------
    // ScriptSystem - NativeScriptComponent'lerin yasam dongusu (Faz 16a)
    // -----------------------------------------------------------------------
    // Digerlerinden farkli olarak DURUM olusturur: script ornekleri
    // heap'te yasar ve Start ile Stop arasinda var olurlar. Bu yuzden
    // uc ayri giris noktasi var - "her karede cagir" yetmiyor.
    //
    // Yalnizca Play modunda calisir. Edit modunda script'in calismasi
    // demek, duzenlerken nesnelerin kacmasi demektir (Faz 10'un dersi).
    class ScriptSystem
    {
    public:
        // Play basladiginda: ornekleri yaratir ve OnCreate cagirir.
        static void OnRuntimeStart(Scene& scene);

        // Her sabit adimda: OnUpdate.
        static void Update(Scene& scene, float dt);

        // Stop'ta: OnDestroy cagirir ve ornekleri siler.
        // Cagrilmazsa bellek sizar - Scene yikicisi da bunu cagirir.
        static void OnRuntimeStop(Scene& scene);
    };

    // -----------------------------------------------------------------------
    // SpriteRenderSystem - cizilecekleri Renderer2D'ye gonderir
    // -----------------------------------------------------------------------
    class SpriteRenderSystem
    {
    public:
        // BeginScene/EndScene'i BU SISTEM cagirir. Cagiran koda birakmak
        // "EndScene'i unutmak" hatasina davetiye cikarirdi; ayrica
        // ileride birden fazla render sistemi olursa (ISIK, PARTIKUL)
        // batch'in kim tarafindan acilip kapandigi belirsizlesirdi.
        static void Render(entt::registry& registry, const OrthographicCamera& camera);
    };
}
