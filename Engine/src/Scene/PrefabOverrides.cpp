#include "FXEngine/Scene/PrefabOverrides.h"
#include "FXEngine/Scene/Components.h"
#include "FXEngine/Scene/ComponentMeta.h"
#include "FXEngine/Scene/Scene.h"
#include "FXEngine/Asset/AssetManager.h"
#include "FXEngine/Core/FileSystem.h"
#include "FXEngine/Core/Log.h"

#include "Scene/EntitySerialization.h"

#include <nlohmann/json.hpp>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

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

        void CollectSubtree(Entity entity, std::vector<Entity>& out)
        {
            out.push_back(entity);
            for (Entity child : entity.GetChildren())
                CollectSubtree(child, out);
        }

        // Bir entity json'undaki TUM entity referanslarini (Parent, EntityRef
        // alanlari, script "e" alanlari) tablodan gecirir. Tabloda olmayan
        // deger dokunulmaz (disariya bakan referans). RemapReferences'in
        // json duzeyindeki karsiligi: Apply canli entity yerine dosyaya
        // yazilacak metin uzerinde calisiyor.
        void RemapJsonRefs(json& entityJson,
                           const std::unordered_map<UUID, UUID>& remap)
        {
            auto mapVal = [&remap](std::uint64_t v) -> std::uint64_t
            {
                const auto it = remap.find(UUID(v));
                return it != remap.end() ? static_cast<std::uint64_t>(it->second) : v;
            };

            if (entityJson.contains("Parent"))
                entityJson["Parent"] = mapVal(AsU64(entityJson["Parent"]));

            for (const ComponentInfo& info : ComponentRegistry::GetAll())
            {
                if (!entityJson.contains(info.Name))
                    continue;

                json& comp = entityJson[info.Name];
                for (const FieldInfo& f : info.Fields)
                    if (f.Type == FieldType::EntityRef && comp.contains(f.Name))
                        comp[f.Name] = mapVal(AsU64(comp[f.Name]));
            }

            // Script Entity alanlari - RemapReferences'taki bilginin json
            // izdusumu (format ComponentMeta'daki NativeScript Extra'sindan).
            if (entityJson.contains("NativeScript") &&
                entityJson["NativeScript"].contains("Fields"))
            {
                for (auto& [name, fj] : entityJson["NativeScript"]["Fields"].items())
                    if (fj.value("t", std::string()) == "e" && fj.contains("v"))
                        fj["v"] = mapVal(AsU64(fj["v"]));
            }
        }

        // Apply sonrasi BASKA bir ornegin tek entity'sini tazeler: eski
        // kaynaga esit (override edilmemis) alanlar yeni kaynaga cekilir,
        // override edilenler korunur (Unity davranisi).
        void RefreshInstanceEntity(Entity e, const json& oldSrc,
                                   const json& newSrc, TextureLibrary* library)
        {
            for (const ComponentInfo& info : ComponentRegistry::GetAll())
            {
                if (!info.SerializedByTable)
                    continue;
                if (std::string_view(info.Name) == "PrefabInstance")
                    continue;
                // Component iki kaynakta da yoksa/teksa yapisal degisim
                // demektir - C-5'e birakildi.
                if (!oldSrc.contains(info.Name) || !newSrc.contains(info.Name))
                    continue;
                if (!info.Has(e))
                    continue;

                const json& oldC = oldSrc[info.Name];
                const json& newC = newSrc[info.Name];
                if (oldC == newC)
                    continue;   // kaynak bu component'te degismemis

                void* comp = info.GetPtr(e);
                const json cur = Detail::SerializeComponent(info, e);

                for (const FieldInfo& f : info.Fields)
                {
                    if (!oldC.contains(f.Name) || !newC.contains(f.Name))
                        continue;
                    if (oldC[f.Name] == newC[f.Name])
                        continue;   // bu alan degismemis

                    if (f.Type == FieldType::EntityRef)
                    {
                        const std::uint64_t curV = AsU64(cur[f.Name]);
                        if (ToSourceSpace(e, UUID(curV)) != AsU64(oldC[f.Name]))
                            continue;   // override edilmis: dokunma

                        std::uint64_t newV = AsU64(newC[f.Name]);
                        if (newV != 0)
                        {
                            const AssetHandle handle =
                                e.GetComponent<PrefabInstanceComponent>().Prefab;
                            if (Entity m = FindBySourceId(InstanceRootOf(e),
                                                          handle, UUID(newV)))
                                newV = static_cast<std::uint64_t>(m.GetUUID());
                        }
                        static_cast<EntityRef*>(f.Get(comp))->Target = UUID(newV);
                    }
                    else
                    {
                        if (cur.contains(f.Name) && cur[f.Name] != oldC[f.Name])
                            continue;   // override edilmis: dokunma
                        Detail::ReadField(f, comp, newC);
                    }
                }

                // Alan tablosu disindaki veri (doku GUID'i, script alanlari):
                // tek blob olarak kiyaslanir. Ornek eski kaynakla aynida
                // kaldiysa yeni kaynagin blobu yuklenir.
                if (info.LoadExtra)
                {
                    auto extraOf = [&info](const json& c)
                    {
                        json ex = c;
                        for (const FieldInfo& f : info.Fields)
                            ex.erase(f.Name);
                        return ex;
                    };

                    const json oldEx = extraOf(oldC);
                    if (oldEx != extraOf(newC) && extraOf(cur) == oldEx)
                        info.LoadExtra(comp, newC, library);
                }
            }
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

    Entity InstanceRoot(Entity entity)
    {
        if (!entity || !entity.HasComponent<PrefabInstanceComponent>())
            return entity;
        return InstanceRootOf(entity);
    }

    std::vector<Entity> InstanceRootsOf(Scene& scene, AssetHandle prefab)
    {
        std::vector<Entity> roots;
        std::unordered_set<std::uint64_t> seen;

        auto view = scene.GetRegistry().view<PrefabInstanceComponent>();
        for (auto handle : view)
        {
            if (view.get<PrefabInstanceComponent>(handle).Prefab != prefab)
                continue;

            Entity root = InstanceRootOf(Entity{ handle, &scene });
            if (seen.insert(static_cast<std::uint64_t>(root.GetUUID())).second)
                roots.push_back(root);
        }
        return roots;
    }

    bool ApplyInstance(Entity instanceRoot, TextureLibrary* library)
    {
        if (!instanceRoot || !instanceRoot.HasComponent<PrefabInstanceComponent>())
            return false;

        instanceRoot = InstanceRootOf(instanceRoot);
        Scene* scene = instanceRoot.GetScene();
        if (!scene)
            return false;

        const AssetHandle handle =
            instanceRoot.GetComponent<PrefabInstanceComponent>().Prefab;
        const std::string relPath = AssetManager::GetPath(handle);
        if (relPath.empty())
        {
            FX_CORE_ERROR("Apply: prefab kaynagi bulunamadi (kayip GUID).");
            return false;
        }

        const std::string fullPath = FileSystem::ResolveProjectAsset(relPath);
        json doc;
        {
            std::ifstream in(fullPath);
            if (!in)
            {
                FX_CORE_ERROR("Apply: prefab dosyasi acilamadi: %s", fullPath.c_str());
                return false;
            }
            try
            {
                in >> doc;
            }
            catch (const json::parse_error& err)
            {
                FX_CORE_ERROR("Apply: prefab dosyasi bozuk: %s", err.what());
                return false;
            }
        }

        if (!doc.contains("Entities") || !doc["Entities"].is_array())
            return false;

        // Ornek alt agaci + iki yonlu esleme.
        std::vector<Entity> subtree;
        CollectSubtree(instanceRoot, subtree);

        std::unordered_map<UUID, UUID>   instToSrc;   // ornek UUID -> SourceId
        std::unordered_map<UUID, Entity> srcToInst;   // SourceId -> ornek entity
        for (Entity e : subtree)
        {
            if (!e.HasComponent<PrefabInstanceComponent>())
                continue;
            const auto& link = e.GetComponent<PrefabInstanceComponent>();
            if (link.Prefab != handle || !link.SourceId.IsValid())
                continue;
            instToSrc[e.GetUUID()] = link.SourceId;
            srcToInst[link.SourceId] = e;
        }

        // Yeni entity listesi ESKI dosyanin sirasiyla kurulur: eslesen
        // kayit ornekten yeniden yazilir, eslesmeyen (ornekte silinmis)
        // oldugu gibi korunur. Ornekte EKLENEN entity'ler dosyaya girmez
        // (yapisal apply C-5).
        json newEntities = json::array();
        int  applied     = 0;

        for (const auto& oldE : doc["Entities"])
        {
            const UUID srcId{ oldE.value("ID", std::uint64_t{ 0 }) };
            const auto it = srcToInst.find(srcId);
            if (it == srcToInst.end())
            {
                newEntities.push_back(oldE);
                continue;
            }

            Entity inst = it->second;

            json j = Detail::SerializeEntity(inst);
            j["ID"] = static_cast<std::uint64_t>(srcId);
            j.erase("PrefabInstance");

            // Parent + ic referanslar kaynak uzayina.
            RemapJsonRefs(j, instToSrc);

            if (inst == instanceRoot)
            {
                j.erase("Parent");

                // Kok konumu KAYNAKTAKI degerinde kalir: ornege ozgu
                // yerlestirme kaynaga sizmamali (Revert'in simetrigi).
                if (j.contains("Transform") && oldE.contains("Transform") &&
                    oldE["Transform"].contains("Translation"))
                    j["Transform"]["Translation"] = oldE["Transform"]["Translation"];
            }

            newEntities.push_back(std::move(j));
            ++applied;
        }

        if (applied == 0)
        {
            FX_CORE_WARN("Apply: ornekle kaynak arasinda eslesen entity yok.");
            return false;
        }

        // Diger ornekleri tazele - dosya yazilmadan, bellekteki eski/yeni
        // kopyalarla (override kiyasi ESKI kaynaga karsi yapilmali).
        std::unordered_map<UUID, const json*> oldById, newById;
        for (const auto& e : doc["Entities"])
            if (const UUID id{ e.value("ID", std::uint64_t{ 0 }) }; id.IsValid())
                oldById[id] = &e;
        for (const auto& e : newEntities)
            if (const UUID id{ e.value("ID", std::uint64_t{ 0 }) }; id.IsValid())
                newById[id] = &e;

        for (Entity otherRoot : InstanceRootsOf(*scene, handle))
        {
            if (otherRoot == instanceRoot)
                continue;

            std::vector<Entity> others;
            CollectSubtree(otherRoot, others);

            for (Entity e : others)
            {
                if (!e.HasComponent<PrefabInstanceComponent>())
                    continue;
                const auto& link = e.GetComponent<PrefabInstanceComponent>();
                if (link.Prefab != handle)
                    continue;

                const auto oldIt = oldById.find(link.SourceId);
                const auto newIt = newById.find(link.SourceId);
                if (oldIt != oldById.end() && newIt != newById.end())
                    RefreshInstanceEntity(e, *oldIt->second, *newIt->second, library);
            }
        }

        // Dosyaya yaz. Root kimligi ve surum korunur.
        json out;
        out["Version"]  = doc.value("Version", 1);
        out["Type"]     = "FXPrefab";
        out["Root"]     = doc.value("Root", std::uint64_t{ 0 });
        out["Entities"] = std::move(newEntities);

        std::ofstream file(fullPath);
        if (!file)
        {
            FX_CORE_ERROR("Apply: prefab dosyasina yazilamadi: %s", fullPath.c_str());
            return false;
        }
        file << std::setw(2) << out << std::endl;

        FX_CORE_INFO("Prefab'a uygulandi: %s (%d entity).", relPath.c_str(), applied);
        return true;
    }
}
