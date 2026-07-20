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

#include <entt/entt.hpp>

#include <cstdint>
#include <string>

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

        Entity CreateEntity(const std::string& name = "Entity");
        void   DestroyEntity(Entity entity);

        // Mantik guncellemesi. SABIT adimli dt ile cagrilir (Application).
        void OnUpdate(float dt);

        // Cizim. Kamera disaridan geliyor cunku Faz 5'te kamera hala
        // editorun; Faz 6'da sahne icindeki bir CameraComponent'e tasinacak.
        void OnRender(const OrthographicCamera& camera);

        // Editor panelleri (Faz 6) icin gerekli olacak.
        // Motorun editoru bilmesi DEGIL bu - sadece veriye erisim aciyor.
        entt::registry& GetRegistry() { return m_Registry; }
        const entt::registry& GetRegistry() const { return m_Registry; }

        std::uint32_t GetEntityCount() const;

    private:
        entt::registry m_Registry;

        // Entity, registry'ye erismek zorunda -> arkadas sinif.
        // Alternatifi registry'yi public yapmakti; bu daha dar bir kapi.
        friend class Entity;
    };
}
