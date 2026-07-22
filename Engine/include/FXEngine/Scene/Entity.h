#pragma once

// ===========================================================================
// FXEngine - Entity
//
// Bu sinif VERI TUTMAZ. Sadece iki alan var: bir kimlik ve sahnenin adresi.
// Yani bir "tutamak" (handle), bir nesne degil. Kopyalamak ucuzdur,
// deger olarak gecirilir - isaretci gibi dusun.
//
// NEDEN VAR? entt'nin ham API'si sudur:
//     registry.emplace<TransformComponent>(entityId, ...);
//     registry.get<TransformComponent>(entityId);
// Her cagrida hem registry'yi hem id'yi tasimak zorundasin. Bu sarmalayici
// onu su hale getirir:
//     entity.AddComponent<TransformComponent>(...);
//     entity.GetComponent<TransformComponent>();
//
// Islevsel bir sey eklemiyor; SADECE cagri yerlerini okunur kiliyor.
// Motor mimarisinde bu tur "ince sarmalayicilar" yaygin ve degerlidir.
// ===========================================================================

#include "FXEngine/Scene/Scene.h"
#include "FXEngine/Core/Log.h"

#include <entt/entt.hpp>

#include <string>
#include <utility>
#include <vector>

namespace FX
{
    class Entity
    {
    public:
        Entity() = default;
        Entity(entt::entity handle, Scene* scene)
            : m_EntityHandle(handle), m_Scene(scene) {}

        // --- Component islemleri ------------------------------------------
        // Sablonlar header'da tanimlanmali: derleyici cagri yerinde
        // hangi tiple kullanildigini gormeli.

        template<typename T, typename... Args>
        T& AddComponent(Args&&... args)
        {
            FX_ASSERT(!HasComponent<T>(), "Entity bu component'e zaten sahip!");

            // std::forward: "mukemmel yonlendirme". Cagirandan gelen
            // argumanlari kopyalamadan, tam olarak geldikleri degerlik
            // kategorisiyle (lvalue/rvalue) iletir. Bu olmadan gecici
            // nesneler gereksiz yere kopyalanirdi.
            return m_Scene->m_Registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
        }

        // Varsa degistirir, yoksa ekler. "Bu deger su olsun" demek istedigin
        // ama entity'nin durumunu bilmedigin yerler icin.
        template<typename T, typename... Args>
        T& AddOrReplaceComponent(Args&&... args)
        {
            return m_Scene->m_Registry.emplace_or_replace<T>(m_EntityHandle,
                                                             std::forward<Args>(args)...);
        }

        template<typename T>
        T& GetComponent()
        {
            FX_ASSERT(HasComponent<T>(), "Entity'de bu component yok!");
            return m_Scene->m_Registry.get<T>(m_EntityHandle);
        }

        template<typename T>
        bool HasComponent() const
        {
            // entt 3.13: all_of<T...>() - "hepsine sahip mi?"
            return m_Scene->m_Registry.all_of<T>(m_EntityHandle);
        }

        // Yoksa ekler, varsa mevcuduna referans doner.
        template<typename T, typename... Args>
        T& AddOrReplaceIfMissing(Args&&... args)
        {
            if (HasComponent<T>())
                return GetComponent<T>();
            return AddComponent<T>(std::forward<Args>(args)...);
        }

        template<typename T>
        void RemoveComponent()
        {
            FX_ASSERT(HasComponent<T>(), "Kaldirilacak component yok!");
            m_Scene->m_Registry.remove<T>(m_EntityHandle);
        }

        // --- Hiyerarsi ------------------------------------------------------
        // Tanimlar Entity.cpp'de: Scene'in tam tanimina ve component
        // basliklarina ihtiyac duyuyorlar.

        UUID GetUUID() const;
        const std::string& GetName() const;

        Entity GetParent() const;

        // Gecersiz bir Entity vererek koke tasinir.
        // Dongusel baglanti (kendi torununun cocugu olmak) reddedilir.
        void SetParent(Entity parent);

        std::vector<Entity> GetChildren() const;
        bool HasChildren() const;

        // Bu entity'yi bir sira YUKARI (-1) ya da ASAGI (+1) tasir. Parent'i
        // varsa parent'in cocuk listesinde, yoksa Scene'in KOK listesinde
        // (B-1b). Sira Hierarchy'de gorunen sirayi ve (hiyerarsi sirali
        // serilestirme sayesinde) dosyaya yazilan sirayi belirler.
        // Doner: gercekten tasindi mi (uctaysa false).
        bool MoveInParent(int direction);

        // this, other'in ustunde bir yerde mi? Dongu kontrolu icin.
        bool IsAncestorOf(Entity other) const;

        // --- Gecerlilik ----------------------------------------------------
        // explicit: kazara bool'a donusmesini engeller.
        //   if (entity) -> calisir (istedigimiz)
        //   int x = entity; -> derlenmez (istemedigimiz)
        explicit operator bool() const { return m_EntityHandle != entt::null && m_Scene != nullptr; }

        // Ham entt kimligine erisim (entt API'siyle dogrudan calisan yerler icin).
        operator entt::entity() const { return m_EntityHandle; }
        entt::entity GetHandle() const { return m_EntityHandle; }

        // Sahneye erisim: script'ler baska entity'leri bulmak icin
        // (FindEntityByUUID) buna ihtiyac duyuyor.
        Scene* GetScene() const { return m_Scene; }

        bool operator==(const Entity& other) const
        {
            return m_EntityHandle == other.m_EntityHandle && m_Scene == other.m_Scene;
        }
        bool operator!=(const Entity& other) const { return !(*this == other); }

    private:
        entt::entity m_EntityHandle = entt::null;

        // Ham isaretci, akilli isaretci DEGIL. Entity sahneye SAHIP OLMAZ,
        // ona referans verir. shared_ptr kullansaydik dairesel sahiplik
        // olusur ve sahne hicbir zaman yok edilemezdi.
        Scene* m_Scene = nullptr;
    };
}
