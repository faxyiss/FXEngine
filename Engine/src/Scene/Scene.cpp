#include "FXEngine/Scene/Scene.h"
#include "FXEngine/Scene/Entity.h"
#include "FXEngine/Scene/Components.h"
#include "FXEngine/Scene/Systems.h"

namespace FX
{
    Entity Scene::CreateEntity(const std::string& name)
    {
        // Varsayilan UUID yapicisi rastgele uretir.
        return CreateEntityWithUUID(UUID(), name);
    }

    Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name)
    {
        Entity entity{ m_Registry.create(), this };

        // HER entity uc sey alir: kimlik, ad, konum.
        //
        // IDComponent Faz 8'de eklendi ve digerlerinden farkli bir
        // statude: Tag ve Transform "pratik varsayimlar"di, ID ise
        // ZORUNLU - kimliksiz entity serilestirilemez ve referans
        // verilemez.
        entity.AddComponent<IDComponent>(uuid);
        entity.AddComponent<TagComponent>(name);
        entity.AddComponent<TransformComponent>();

        m_EntityMap[uuid] = entity.GetHandle();

        return entity;
    }

    void Scene::DestroyEntity(Entity entity)
    {
        if (!entity)
            return;

        // ONCE haritadan cikar, SONRA registry'den sil.
        // Ters yapsaydik, silinmis entity'nin IDComponent'ini okumaya
        // calisirdik - tanimsiz davranis.
        if (entity.HasComponent<IDComponent>())
            m_EntityMap.erase(entity.GetComponent<IDComponent>().ID);

        m_Registry.destroy(entity);
    }

    void Scene::Clear()
    {
        m_Registry.clear();
        m_EntityMap.clear();
    }

    Entity Scene::FindEntityByUUID(UUID uuid)
    {
        const auto it = m_EntityMap.find(uuid);
        if (it == m_EntityMap.end())
            return {};   // gecersiz Entity - cagiran `if (e)` ile kontrol eder

        // Harita ile registry'nin ayrisma ihtimaline karsi dogrula.
        // Normalde olmamali ama registry'ye disaridan erisim acik
        // oldugu surece bu kontrol ucuz bir sigortadir.
        if (!m_Registry.valid(it->second))
            return {};

        return Entity{ it->second, this };
    }

    Entity Scene::FindEntityByName(const std::string& name)
    {
        auto view = m_Registry.view<TagComponent>();
        for (auto id : view)
        {
            if (view.get<TagComponent>(id).Tag == name)
                return Entity{ id, this };
        }
        return {};
    }

    std::uint32_t Scene::GetEntityCount() const
    {
        // NOT: entt 3.13'te registry.storage<T>() bir ISARETCI dondurur
        // (referans degil), ve canli entity sayisi free_list() ile alinir.
        // (in_use() ayni isi yapiyordu ama bu surumde kullanimdan kalkti.)
        //
        // Neden size() degil? Cunku size(), silinmis ama kimligi geri
        // donusturulmek uzere saklanan entity'leri de sayar. EnTT
        // kimlikleri yeniden kullanir; free_list() gercekten CANLI olanlari verir.
        const auto* pool = m_Registry.storage<entt::entity>();
        return pool ? static_cast<std::uint32_t>(pool->free_list()) : 0u;
    }

    void Scene::OnUpdate(float dt)
    {
        // SISTEM SIRASI BURADA TANIMLIDIR ve bu, Scene sinifinin
        // varlik sebebidir.
        //
        //   Follow -> Movement
        // Sira onemli: FollowSystem hedefe dogru bir HIZ yaziyor,
        // MovementSystem o hizi konuma uyguluyor. Ters olsaydi takip
        // her zaman bir kare gecikmeli calisirdi.
        FollowSystem::Update(*this, dt);
        MovementSystem::Update(m_Registry, dt);
    }

    void Scene::OnRender(const OrthographicCamera& camera)
    {
        SpriteRenderSystem::Render(m_Registry, camera);
    }
}
