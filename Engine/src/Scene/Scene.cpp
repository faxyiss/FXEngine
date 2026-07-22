#include "FXEngine/Scene/Scene.h"
#include "FXEngine/Scene/Entity.h"
#include "FXEngine/Scene/ComponentMeta.h"
#include "FXEngine/Scene/Components.h"
#include "FXEngine/Scene/Systems.h"

#include "Scene/EntitySerialization.h"

#include <algorithm>

namespace FX
{
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

            // Kopyalanacak component listesi meta tablosundan geliyor
            // (A1). Ayri bir liste tutsaydik yeni bir component eklendiginde
            // "Play'e gecince kayboldu" hatasi dogardi.
            for (const ComponentInfo& info : ComponentRegistry::GetAll())
                info.CopyTo(dst, src);
        }

        // Script ORNEKLERI kopyalanmaz, yalnizca baglantilari.
        //
        // Instance ham bir isaretci: kopyalasaydik iki sahne ayni nesneyi
        // gosterir ve ikisi de silmeye calisirdi. Kopya sahne kendi
        // orneklerini OnRuntimeStart'ta yaratir.
        auto scripts = target->m_Registry.view<NativeScriptComponent>();
        for (auto handle : scripts)
            scripts.get<NativeScriptComponent>(handle).Instance = nullptr;

        // Kok sirasini aynen tasi: CopyTo, RelationshipComponent'i SetParent
        // KULLANMADAN kopyaladigi icin target->m_RootOrder olusturma
        // sirasinda birikti. UUID'ler korundugu icin dogrudan atayabiliriz.
        target->m_RootOrder = source.m_RootOrder;

        return target;
    }

    Entity Scene::DuplicateEntity(Entity source)
    {
        if (!source || !source.HasComponent<IDComponent>())
            return {};

        // Kaynak alt agacini KOK once topl (parent cocuktan once islensin).
        std::vector<Entity> subtree;
        {
            std::vector<Entity> stack{ source };
            while (!stack.empty())
            {
                Entity e = stack.back();
                stack.pop_back();
                subtree.push_back(e);
                for (Entity child : e.GetChildren())
                    stack.push_back(child);
            }
        }

        // 1. gecis: yeni kimliklerle olustur, component'leri KOPYALA.
        std::unordered_map<UUID, UUID> remap;   // eski UUID -> yeni UUID
        std::unordered_map<UUID, Entity> newByOld;

        for (Entity src : subtree)
        {
            Entity dst = CreateEntity();   // yeni UUID
            for (const ComponentInfo& info : ComponentRegistry::GetAll())
                info.CopyTo(dst, src);

            // CopyTo, Relationship'i de ESKI UUID'lerle kopyaladi; hiyerarsiyi
            // ikinci geciste SetParent ile yeniden kuracagiz, o yuzden bu
            // kopyayi temizliyoruz - yoksa parent'in cocuk listesinde hem
            // eski hem yeni kimlikler kalirdi.
            if (dst.HasComponent<RelationshipComponent>())
                dst.RemoveComponent<RelationshipComponent>();

            // Script ornegi kopyalanmaz, yalnizca veri (Scene::Copy gibi).
            if (dst.HasComponent<NativeScriptComponent>())
                dst.GetComponent<NativeScriptComponent>().Instance = nullptr;

            const UUID oldID = src.GetComponent<IDComponent>().ID;
            remap[oldID]     = dst.GetComponent<IDComponent>().ID;
            newByOld[oldID]  = dst;
        }

        // 2. gecis: hiyerarsiyi kur. Kok, kaynagin KARDESI olur.
        Entity newRoot = newByOld[source.GetComponent<IDComponent>().ID];

        for (Entity src : subtree)
        {
            Entity dst = newByOld[src.GetComponent<IDComponent>().ID];

            if (src == source)
            {
                // Kaynak bir cocuksa kopya da ayni parent'in cocugu olur.
                if (Entity parent = source.GetParent())
                    dst.SetParent(parent);
            }
            else if (Entity srcParent = src.GetParent())
            {
                // Ic parent: mutlaka remap'te (alt agacin tamami tarandi).
                dst.SetParent(newByOld[srcParent.GetComponent<IDComponent>().ID]);
            }
        }

        // 3. gecis: ic referanslari yeni kimliklere cevir (Follow, EntityRef,
        // script Entity alanlari). Disariya bakanlar oldugu gibi kalir.
        for (auto& [oldID, dst] : newByOld)
            Detail::RemapReferences(dst, remap);

        return newRoot;
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

        // Yeni entity kok olarak dogar (henuz parent'i yok). SetParent
        // sonra cikarabilir.
        m_RootOrder.push_back(uuid);

        return entity;
    }

    std::vector<Entity> Scene::GetRootEntities()
    {
        std::vector<Entity> roots;
        roots.reserve(m_RootOrder.size());
        for (UUID id : m_RootOrder)
        {
            Entity e = FindEntityByUUID(id);
            if (!e)
                continue;
            // Guvenlik: listede olsa bile gercekten kok mu?
            const bool isRoot = !e.HasComponent<RelationshipComponent>() ||
                                !e.GetComponent<RelationshipComponent>().Parent.IsValid();
            if (isRoot)
                roots.push_back(e);
        }
        return roots;
    }

    void Scene::AddRoot(UUID id)
    {
        if (!id.IsValid())
            return;
        if (std::find(m_RootOrder.begin(), m_RootOrder.end(), id) == m_RootOrder.end())
            m_RootOrder.push_back(id);
    }

    void Scene::RemoveRoot(UUID id)
    {
        m_RootOrder.erase(std::remove(m_RootOrder.begin(), m_RootOrder.end(), id),
                          m_RootOrder.end());
    }

    bool Scene::MoveRoot(UUID id, int direction)
    {
        if (direction == 0)
            return false;

        const auto it = std::find(m_RootOrder.begin(), m_RootOrder.end(), id);
        if (it == m_RootOrder.end())
            return false;

        const std::size_t idx = static_cast<std::size_t>(it - m_RootOrder.begin());
        if (direction < 0 ? (idx == 0) : (idx + 1 >= m_RootOrder.size()))
            return false;

        const std::size_t target = (direction < 0) ? idx - 1 : idx + 1;
        std::swap(m_RootOrder[idx], m_RootOrder[target]);
        return true;
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

        // Script'i olan bir entity oyun sirasinda silinebiliyor (A-1):
        // OnDestroy cagrilmadan ve ornek silinmeden gitmesi hem
        // "temizligimi yapamadim" hem de bellek sizintisi olurdu.
        ScriptSystem::DestroyInstance(entity);

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
        {
            const UUID id = entity.GetComponent<IDComponent>().ID;
            m_EntityMap.erase(id);
            RemoveRoot(id);   // kok degilse zaten yok, zararsiz
        }

        m_Registry.destroy(entity);
    }

    void Scene::DestroyEntityDeferred(Entity entity)
    {
        if (!entity || !entity.HasComponent<IDComponent>())
            return;

        const UUID id = entity.GetComponent<IDComponent>().ID;

        // Ayni entity iki kez istenebilir (iki script birbirini silmek
        // isteyebilir). Ikinci istek sessizce yok sayiliyor - hata degil.
        for (UUID pending : m_PendingDestroy)
            if (static_cast<std::uint64_t>(pending) == static_cast<std::uint64_t>(id))
                return;

        m_PendingDestroy.push_back(id);
    }

    void Scene::FlushDestroyQueue()
    {
        if (m_PendingDestroy.empty())
            return;

        // Kuyrugu TASIYORUZ: silinen bir entity'nin OnDestroy'u yeni
        // silme istegi yazabilir. Uzerinde gezerken diziye eklemek
        // yineleyicileri bozardi; yeni istekler sonraki kareye kalir.
        std::vector<UUID> batch;
        batch.swap(m_PendingDestroy);

        for (UUID id : batch)
        {
            // Arada baska bir silme bunu da goturmus olabilir (parent'i
            // silinen cocuk). FindEntityByUUID gecersiz doner, geciyoruz.
            if (Entity entity = FindEntityByUUID(id))
                DestroyEntity(entity);
        }
    }

    void Scene::Clear()
    {
        m_Registry.clear();
        m_EntityMap.clear();
        m_PendingDestroy.clear();
        m_RootOrder.clear();
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

        // Silme istekleri EN SONDA: hicbir sistem artik registry'yi
        // gezmiyor, dolayisiyla yapiyi degistirmek guvenli.
        FlushDestroyQueue();
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
