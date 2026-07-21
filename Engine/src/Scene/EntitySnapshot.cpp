#include "FXEngine/Scene/EntitySnapshot.h"

#include "FXEngine/Scene/ComponentMeta.h"
#include "FXEngine/Scene/Components.h"
#include "FXEngine/Scene/Scene.h"

#include "Scene/EntitySerialization.h"

#include <nlohmann/json.hpp>

#include <vector>

namespace FX
{
    using json = nlohmann::json;

    namespace
    {
        // Kokten yapraga: geri koyarken parent'in cocuktan ONCE var olmasi
        // sart degil (bag ikinci geciste kuruluyor) ama sira korunmasi
        // cocuklarin hiyerarsideki gorunen dizilisini de koruyor.
        void CollectTree(Entity entity, std::vector<Entity>& out)
        {
            if (!entity)
                return;

            out.push_back(entity);
            for (Entity child : entity.GetChildren())
                CollectTree(child, out);
        }
    }

    EntitySnapshot EntitySnapshot::Capture(Entity root)
    {
        EntitySnapshot snap;
        if (!root)
            return snap;

        std::vector<Entity> tree;
        CollectTree(root, tree);

        json doc;
        doc["Root"] = static_cast<std::uint64_t>(root.GetUUID());

        json entities = json::array();
        for (Entity e : tree)
            entities.push_back(Detail::SerializeEntity(e));
        doc["Entities"] = std::move(entities);

        snap.m_Root = root.GetUUID();
        snap.m_Data = doc.dump();
        return snap;
    }

    Entity EntitySnapshot::Restore(Scene& scene, TextureLibrary* library) const
    {
        if (m_Data.empty())
            return {};

        const json doc = json::parse(m_Data, nullptr, false);
        if (doc.is_discarded() || !doc.contains("Entities"))
            return {};

        // Iki gecis: once herkes var olsun, sonra baglar kurulsun.
        // Tek geciste kursaydik bir cocugun parent'i henuz olusmamis
        // olabilirdi - sahne yuklemenin ogrendigi ders.
        for (const json& e : doc["Entities"])
        {
            if (!e.contains("ID"))
                continue;

            const UUID id{ e["ID"].get<std::uint64_t>() };
            if (scene.FindEntityByUUID(id))
                continue;

            Entity entity = scene.CreateEntityWithUUID(
                id, e.value("Tag", std::string{ "Entity" }));
            Detail::ApplyComponents(entity, e, library);
        }

        for (const json& e : doc["Entities"])
        {
            if (!e.contains("ID") || !e.contains("Parent"))
                continue;

            Entity child = scene.FindEntityByUUID(UUID{ e["ID"].get<std::uint64_t>() });
            Entity parent = scene.FindEntityByUUID(UUID{ e["Parent"].get<std::uint64_t>() });

            // Parent artik yasamiyor olabilir (kullanici once parent'i
            // silmis olabilir): o zaman entity kokte kalir, cokmez.
            if (child && parent)
                child.SetParent(parent);
        }

        return scene.FindEntityByUUID(m_Root);
    }

    ComponentSnapshot ComponentSnapshot::Capture(Entity owner, const ComponentInfo& info)
    {
        ComponentSnapshot snap;
        if (!owner || !info.Has(owner))
            return snap;

        snap.m_Name = info.Name;
        snap.m_Data = Detail::SerializeComponent(info, owner).dump();
        return snap;
    }

    bool ComponentSnapshot::Restore(Entity owner, TextureLibrary* library) const
    {
        if (!owner || m_Data.empty())
            return false;

        ComponentInfo* info = ComponentRegistry::Find(m_Name);
        if (!info)
            return false;

        const json obj = json::parse(m_Data, nullptr, false);
        if (obj.is_discarded())
            return false;

        Detail::ApplyComponent(*info, owner, obj, library);
        return true;
    }
}
