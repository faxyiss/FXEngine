#include "FXEngine/Scene/PrefabOverrides.h"
#include "FXEngine/Scene/Components.h"
#include "FXEngine/Scene/ComponentMeta.h"
#include "FXEngine/Scene/Scene.h"
#include "FXEngine/Asset/AssetManager.h"
#include "FXEngine/Core/FileSystem.h"

#include "Scene/EntitySerialization.h"

#include <nlohmann/json.hpp>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <unordered_map>

namespace FX::PrefabOverrides
{
    using json = nlohmann::json;

    namespace
    {
        // Kaynak dosya onbellegi: Inspector her kare soruyor, her kare
        // disk okumak olmaz. Yazma zamani degisince kendiliginden tazelenir
        // (Apply/dis duzenleme sonrasi ayri bir "cache'i bosalt" cagrisi
        // gerekmez).
        struct CachedPrefab
        {
            std::filesystem::file_time_type WriteTime{};
            std::unordered_map<UUID, json>  ById;   // SourceId -> entity json
            bool Valid = false;
        };

        std::unordered_map<UUID, CachedPrefab> s_Cache;   // handle -> icerik

        const CachedPrefab* GetSource(AssetHandle handle)
        {
            const std::string relPath = AssetManager::GetPath(handle);
            if (relPath.empty())
                return nullptr;

            const std::string fullPath = FileSystem::ResolveProjectAsset(relPath);

            std::error_code ec;
            const auto writeTime = std::filesystem::last_write_time(fullPath, ec);
            if (ec)
                return nullptr;

            CachedPrefab& entry = s_Cache[handle];
            if (entry.Valid && entry.WriteTime == writeTime)
                return &entry;

            entry = {};
            entry.WriteTime = writeTime;

            std::ifstream in(fullPath);
            if (!in)
                return nullptr;

            json doc;
            try
            {
                in >> doc;
            }
            catch (const json::parse_error&)
            {
                return nullptr;
            }

            if (!doc.contains("Entities") || !doc["Entities"].is_array())
                return nullptr;

            for (auto& e : doc["Entities"])
            {
                const UUID id{ e.value("ID", std::uint64_t{ 0 }) };
                if (id.IsValid())
                    entry.ById[id] = std::move(e);
            }

            entry.Valid = true;
            return &entry;
        }

        // instance'in kaynak entity json'u ya da nullptr.
        const json* SourceEntityOf(Entity instance)
        {
            if (!instance || !instance.HasComponent<PrefabInstanceComponent>())
                return nullptr;

            const auto& link = instance.GetComponent<PrefabInstanceComponent>();
            const CachedPrefab* cached = GetSource(link.Prefab);
            if (!cached)
                return nullptr;

            const auto it = cached->ById.find(link.SourceId);
            return it != cached->ById.end() ? &it->second : nullptr;
        }

        // Ornegin ic hedefini kaynak uzayina cevirir: hedef AYNI prefab'in
        // damgali bir uyesiyse SourceId'si, degilse (dis referans) oldugu
        // gibi kalir.
        std::uint64_t ToSourceSpace(Entity instance, UUID target)
        {
            if (!target.IsValid())
                return 0;

            Scene* scene = instance.GetScene();
            if (!scene)
                return static_cast<std::uint64_t>(target);

            Entity t = scene->FindEntityByUUID(target);
            if (t && t.HasComponent<PrefabInstanceComponent>())
            {
                const auto& tl = t.GetComponent<PrefabInstanceComponent>();
                const auto& il = instance.GetComponent<PrefabInstanceComponent>();
                if (tl.Prefab == il.Prefab && tl.SourceId.IsValid())
                    return static_cast<std::uint64_t>(tl.SourceId);
            }
            return static_cast<std::uint64_t>(target);
        }

        // Ayni prefab'a ait en distaki ata. Ayni prefab'in birden fazla
        // ornegi olabilir; kaynak->ornek cozumu SAHNEDE degil bu ornegin
        // kendi alt agacinda yapilmali.
        Entity InstanceRootOf(Entity e)
        {
            const AssetHandle handle =
                e.GetComponent<PrefabInstanceComponent>().Prefab;

            Entity root = e;
            while (Entity parent = root.GetParent())
            {
                if (!parent.HasComponent<PrefabInstanceComponent>())
                    break;
                if (parent.GetComponent<PrefabInstanceComponent>().Prefab != handle)
                    break;
                root = parent;
            }
            return root;
        }

        Entity FindBySourceId(Entity subtreeRoot, AssetHandle handle, UUID sourceId)
        {
            if (subtreeRoot.HasComponent<PrefabInstanceComponent>())
            {
                const auto& link = subtreeRoot.GetComponent<PrefabInstanceComponent>();
                if (link.Prefab == handle && link.SourceId == sourceId)
                    return subtreeRoot;
            }

            for (Entity child : subtreeRoot.GetChildren())
                if (Entity found = FindBySourceId(child, handle, sourceId))
                    return found;

            return {};
        }

        std::uint64_t AsU64(const json& v)
        {
            return v.is_number_unsigned() ? v.get<std::uint64_t>() : 0;
        }
    }

    std::vector<const char*> OverriddenFields(Entity instance,
                                              const ComponentInfo& info)
    {
        std::vector<const char*> result;

        const json* src = SourceEntityOf(instance);
        if (!src || !info.Has(instance))
            return result;

        if (!src->contains(info.Name))
        {
            // Component kaynakta hic yok: ornekte eklenmis, tamami sapma.
            for (const FieldInfo& f : info.Fields)
                result.push_back(f.Name);
            return result;
        }

        const json& srcComp = (*src)[info.Name];
        const json  curComp = Detail::SerializeComponent(info, instance);

        for (const FieldInfo& f : info.Fields)
        {
            // Kaynak dosyada alan yoksa (eski dosya, yeni alan) kiyas
            // yapilamaz; yanlis isaret yerine "fark yok" sayilir.
            if (!curComp.contains(f.Name) || !srcComp.contains(f.Name))
                continue;

            if (f.Type == FieldType::EntityRef)
            {
                const std::uint64_t cur    = AsU64(curComp[f.Name]);
                const std::uint64_t srcVal = AsU64(srcComp[f.Name]);
                if (ToSourceSpace(instance, UUID(cur)) != srcVal)
                    result.push_back(f.Name);
                continue;
            }

            if (curComp[f.Name] != srcComp[f.Name])
                result.push_back(f.Name);
        }

        return result;
    }

    bool RevertField(Entity instance, const ComponentInfo& info,
                     const char* fieldName)
    {
        const json* src = SourceEntityOf(instance);
        if (!src || !src->contains(info.Name) || !info.Has(instance))
            return false;

        const FieldInfo* field = nullptr;
        for (const FieldInfo& f : info.Fields)
            if (std::strcmp(f.Name, fieldName) == 0) { field = &f; break; }
        if (!field)
            return false;

        const json& srcComp = (*src)[info.Name];
        if (!srcComp.contains(field->Name))
            return false;

        void* comp = info.GetPtr(instance);
        if (!comp)
            return false;

        if (field->Type == FieldType::EntityRef)
        {
            // Kaynak deger kaynak uzayinda; bu ornegin alt agacindaki
            // karsiligina cevrilir. Bulunamazsa dis referanstir, oldugu
            // gibi kullanilir.
            const std::uint64_t srcVal = AsU64(srcComp[field->Name]);

            std::uint64_t newVal = srcVal;
            if (srcVal != 0)
            {
                const AssetHandle handle =
                    instance.GetComponent<PrefabInstanceComponent>().Prefab;
                if (Entity match = FindBySourceId(InstanceRootOf(instance),
                                                  handle, UUID(srcVal)))
                    newVal = static_cast<std::uint64_t>(match.GetUUID());
            }

            auto* ref = static_cast<EntityRef*>(field->Get(comp));
            if (static_cast<std::uint64_t>(ref->Target) == newVal)
                return false;

            ref->Target = UUID(newVal);
            return true;
        }

        // ReadField LoadExtra CALISTIRMAZ: tek alan geri alinirken doku /
        // script verisi etkilenmemeli (ApplyComponent'ten farki bu).
        const json before = Detail::SerializeComponent(info, instance);
        Detail::ReadField(*field, comp, srcComp);
        return Detail::SerializeComponent(info, instance) != before;
    }
}
