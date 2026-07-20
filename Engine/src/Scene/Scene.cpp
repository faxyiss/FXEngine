#include "FXEngine/Scene/Scene.h"
#include "FXEngine/Scene/Entity.h"
#include "FXEngine/Scene/Components.h"
#include "FXEngine/Scene/Systems.h"

namespace FX
{
    Entity Scene::CreateEntity(const std::string& name)
    {
        Entity entity{ m_Registry.create(), this };

        // HER entity Tag ve Transform alir.
        //
        // Bu bir tasarim karari: teknik olarak Transform'suz bir entity
        // mumkun (orn. saf bir "oyun ayarlari" nesnesi). Ama editorde
        // her seyin konumu ve adi olmasi hayati kolaylastirir - Hierarchy
        // paneli isim, Inspector konum gosterecek (Faz 6).
        //
        // Unity, Godot ve Hazel de ayni tercihi yapar. Kural degil,
        // yaygin ve pratik bir varsayim.
        entity.AddComponent<TagComponent>(name);
        entity.AddComponent<TransformComponent>();

        return entity;
    }

    void Scene::DestroyEntity(Entity entity)
    {
        m_Registry.destroy(entity);
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
        // Su an tek sistem var ama sira zaten onemli olacak:
        //   Input -> Movement -> Physics -> Collision -> Script
        // Carpisma, hareketten SONRA calismali; yoksa bir kare gecikmeli
        // carpisma tespiti yaparsin ve nesneler duvarlara girer.
        MovementSystem::Update(m_Registry, dt);
    }

    void Scene::OnRender(const OrthographicCamera& camera)
    {
        SpriteRenderSystem::Render(m_Registry, camera);
    }
}
