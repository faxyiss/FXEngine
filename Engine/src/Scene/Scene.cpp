#include "FXEngine/Scene/Scene.h"
#include "FXEngine/Scene/Entity.h"
#include "FXEngine/Scene/Components.h"
#include "FXEngine/Scene/Systems.h"

#include <algorithm>

namespace FX
{
    namespace
    {
        // Tek bir component tipini kaynaktan hedefe tasir.
        // Hedefte yoksa hic dokunmaz - "her entity her component'e sahip
        // degil" durumu normal.
        template<typename C>
        void CopyComponentIfExists(Entity dst, Entity src)
        {
            if (src.HasComponent<C>())
                dst.AddOrReplaceComponent<C>(src.GetComponent<C>());
        }

        // Paket acilimi: AllComponents listesindeki her tip icin
        // yukaridakini cagirir. Listeye bir tip eklemek burada da
        // otomatik olarak gecerli olur.
        template<typename... C>
        void CopyComponents(ComponentGroup<C...>, Entity dst, Entity src)
        {
            (CopyComponentIfExists<C>(dst, src), ...);
        }
    }

    std::unique_ptr<Scene> Scene::Copy(Scene& source)
    {
        auto target = std::make_unique<Scene>();

        // Tek gecis yeterli: hiyerarsi ve referanslar UUID uzerinden
        // calisiyor ve UUID'ler korunuyor. Faz 12'deki prefab
        // orneklemesi iki gecis gerektiriyordu cunku orada kimlikler
        // YENIDEN URETILIYOR ve eski->yeni tablosu kurulmasi gerekiyordu.
        // Burada boyle bir tablo yok: kimlik zaten ayni.
        auto view = source.m_Registry.view<IDComponent>();
        for (auto handle : view)
        {
            const UUID uuid = view.get<IDComponent>(handle).ID;
            Entity src{ handle, &source };

            // CreateEntityWithUUID Tag ve Transform'u zaten ekliyor;
            // asagidaki kopyalama onlarin uzerine yaziyor.
            Entity dst = target->CreateEntityWithUUID(uuid);

            CopyComponents(AllComponents{}, dst, src);
        }

        // Script ORNEKLERI kopyalanmaz, yalnizca baglantilari.
        //
        // Instance ham bir isaretci: kopyalasaydik iki sahne ayni nesneyi
        // gosterir ve ikisi de silmeye calisirdi. Kopya sahne kendi
        // orneklerini OnRuntimeStart'ta yaratir.
        auto scripts = target->m_Registry.view<NativeScriptComponent>();
        for (auto handle : scripts)
            scripts.get<NativeScriptComponent>(handle).Instance = nullptr;

        return target;
    }

    Entity Scene::GetPrimaryCameraEntity()
    {
        auto view = m_Registry.view<CameraComponent>();
        for (auto handle : view)
        {
            if (view.get<CameraComponent>(handle).Primary)
                return Entity{ handle, this };
        }
        return {};
    }

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

        // Cocuklar da silinir. Alternatifi onlari koke tasimakti; birlikte
        // silmek daha sezgisel (bir tankin namlusu tank olunce kalmamali)
        // ve Unity/Godot da boyle yapar.
        //
        // Listeyi KOPYALIYORUZ: ozyineleme sirasinda parent'in Children
        // dizisi degisiyor, uzerinde gezerken degistirmek gecersiz
        // yineleyiciye yol acardi.
        const auto children = entity.GetChildren();
        for (Entity child : children)
            DestroyEntity(child);

        // Parent'in cocuk listesinden kendimizi cikar.
        if (Entity parent = entity.GetParent())
        {
            auto& prc = parent.GetComponent<RelationshipComponent>();
            const UUID myID = entity.GetComponent<IDComponent>().ID;
            prc.Children.erase(std::remove(prc.Children.begin(), prc.Children.end(), myID),
                               prc.Children.end());
        }

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

    Scene::~Scene()
    {
        OnRuntimeStop();
    }

    void Scene::OnRuntimeStart()
    {
        m_Running = true;
        ScriptSystem::OnRuntimeStart(*this);
    }

    void Scene::OnRuntimeStop()
    {
        ScriptSystem::OnRuntimeStop(*this);
        m_Running = false;
    }

    void Scene::OnUpdate(float dt)
    {
        // SCRIPT'LER EN ONDE: oyun mantigi hizi ve hedefi bu karede
        // belirlesin, sonraki sistemler onu uygulasin. Sonda calissaydi
        // yazdiklari bir kare gecikmeyle gorunurdu.
        //
        // Yalnizca Play modunda: Edit modunda script calismasi,
        // duzenlerken nesnelerin kacmasi demek (Faz 10'un dersi).
        if (m_Running)
            ScriptSystem::Update(*this, dt);

        // SISTEM SIRASI BURADA TANIMLIDIR ve bu, Scene sinifinin
        // varlik sebebidir.
        //
        //   Follow -> Movement -> Transform
        // FollowSystem hedefe dogru bir HIZ yaziyor, MovementSystem o hizi
        // konuma uyguluyor, TransformSystem de nihai dunya matrislerini
        // hesapliyor. TransformSystem EN SONDA olmali: ondan once calisan
        // her sey yerel konumu degistirebilir.
        FollowSystem::Update(*this, dt);
        MovementSystem::Update(m_Registry, dt);
        TransformSystem::Update(*this);
    }

    void Scene::OnRender(const OrthographicCamera& camera)
    {
        // Editorde sahne duraklatilmis olabilir; o zaman OnUpdate hic
        // calismaz ama Inspector'dan yapilan degisikliklerin gorunmesi
        // gerekir. Bu yuzden dunya matrislerini burada da tazeliyoruz.
        TransformSystem::Update(*this);

        SpriteRenderSystem::Render(m_Registry, camera);
    }
}
