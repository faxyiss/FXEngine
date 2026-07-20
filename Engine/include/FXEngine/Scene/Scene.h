#pragma once

// ===========================================================================
// FXEngine - Scene
//
// Sahne, entt::registry'yi SAHIPLENIR ve sistemleri dogru sirada calistirir.
//
// Registry'yi neden dogrudan disari acmiyoruz (bir getter var ama)?
// Cunku Scene, sistemlerin CALISMA SIRASINI garanti eden yer. Hareket,
// cizimden once olmali; carpisma, hareketten sonra. Bu sira bir yerde
// tanimli olmali - o yer burasi.
// ===========================================================================

#include "FXEngine/Renderer/OrthographicCamera.h"
#include "FXEngine/Core/UUID.h"

#include <entt/entt.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace FX
{
    class Entity;

    class Scene
    {
    public:
        Scene() = default;
        ~Scene() = default;

        // Kopyalamayi yasakliyoruz: registry kopyalamak binlerce entity'yi
        // cogaltmak demek ve neredeyse her zaman kaza eseri olur.
        // Sahne kopyalama (duplicate) gerekirse acik bir Copy() fonksiyonu
        // yazilir - sessizce olmaz.
        Scene(const Scene&)            = delete;
        Scene& operator=(const Scene&) = delete;

        // Derin kopya. Kaynak sahne DEGISMEZ.
        //
        // UUID'ler KORUNUR (prefab orneklemenin tersi, Faz 12): kopya
        // ayni sahnenin baska bir zamandaki hali, yeni nesneler degil.
        // Korunmasaydi FollowComponent gibi entity referanslari kopyada
        // hedeflerini bulamazdi.
        //
        // Play moduna gecerken kullaniliyor: oyun KOPYADA calisir,
        // duzenlenen sahneye dokunulmaz.
        static std::unique_ptr<Scene> Copy(Scene& source);

        // Yeni bir UUID uretir.
        Entity CreateEntity(const std::string& name = "Entity");

        // BELIRLI bir UUID ile olusturur - sahne YUKLERKEN kullanilir.
        // Dosyadaki kimligi korumak sart: korumazsak entity'ler arasi
        // referanslar (EntityRef) yuklemeden sonra bozulur.
        Entity CreateEntityWithUUID(UUID uuid, const std::string& name = "Entity");

        void DestroyEntity(Entity entity);

        // Tum entity'leri siler. Sahne yuklerken registry.clear() yerine
        // bunu kullanmak zorundayiz: UUID haritasi da temizlenmeli, yoksa
        // olu entity'lere isaret eden kayitlar kalir.
        void Clear();

        // UUID -> Entity. Bulunamazsa gecersiz (bos) Entity doner;
        // cagiran `if (entity)` ile kontrol etmeli.
        //
        // O(1): dogrusal arama yapmiyoruz. Binlerce entity'nin oldugu bir
        // sahnede her karede takip hedefi aramak dogrusal olsaydi
        // kare suresini yerdi.
        Entity FindEntityByUUID(UUID uuid);

        // Ada gore arama. UUID'nin aksine isimler BENZERSIZ DEGILDIR -
        // ilk eslesen doner. Editor kolayligi icin var, oyun mantiginda
        // isim yerine UUID kullanilmali.
        Entity FindEntityByName(const std::string& name);

        // Mantik guncellemesi. SABIT adimli dt ile cagrilir (Application).
        void OnUpdate(float dt);

        // Cizim. Kamera disaridan geliyor cunku Faz 5'te kamera hala
        // editorun; Faz 6'da sahne icindeki bir CameraComponent'e tasinacak.
        void OnRender(const OrthographicCamera& camera);

        // Primary isaretli ilk CameraComponent'e sahip entity.
        // Yoksa gecersiz Entity doner - cagiran `if (e)` ile bakmali.
        Entity GetPrimaryCameraEntity();

        // Editor panelleri (Faz 6) icin gerekli olacak.
        // Motorun editoru bilmesi DEGIL bu - sadece veriye erisim aciyor.
        entt::registry& GetRegistry() { return m_Registry; }
        const entt::registry& GetRegistry() const { return m_Registry; }

        std::uint32_t GetEntityCount() const;

    private:
        entt::registry m_Registry;

        // UUID -> entt::entity haritasi.
        //
        // Neden ayri bir harita? Registry'yi gezip IDComponent'leri
        // karsilastirmak da ise yarardi ama O(n) olurdu. Bu harita
        // aramayi O(1) yapiyor; bedeli entity basina birkac bayt ve
        // olusturma/silmede haritayi guncel tutma sorumlulugu.
        //
        // DIKKAT: Bu harita ile registry'nin SENKRON kalmasi sart.
        // Bu yuzden entity olusturma/silme SADECE Scene uzerinden
        // yapilmali - registry'ye dogrudan create/destroy cagirmak
        // haritayi bozar. GetRegistry()'nin acik olmasi bu riski
        // tasiyor; ileride daha dar bir erisim gerekebilir.
        std::unordered_map<UUID, entt::entity> m_EntityMap;

        // Entity, registry'ye erismek zorunda -> arkadas sinif.
        // Alternatifi registry'yi public yapmakti; bu daha dar bir kapi.
        friend class Entity;
    };
}
