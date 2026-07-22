#include "FXEngine/Scene/PrefabSerializer.h"
#include "FXEngine/Scene/Entity.h"
#include "FXEngine/Scene/Components.h"
#include "FXEngine/Scene/ComponentMeta.h"
#include "FXEngine/Asset/AssetManager.h"
#include "FXEngine/Core/Log.h"
#include "FXEngine/Core/FileSystem.h"

#include "Scene/EntitySerialization.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace FX
{
    using json = nlohmann::json;

    namespace
    {
        void CollectSubtree(Entity entity, std::vector<Entity>& out)
        {
            out.push_back(entity);
            for (Entity child : entity.GetChildren())
                CollectSubtree(child, out);
        }
    }

    PrefabSerializer::PrefabSerializer(Scene* scene, TextureLibrary* textureLibrary)
        : m_Scene(scene), m_Library(textureLibrary)
    {
    }

    bool PrefabSerializer::Save(Entity root, const std::string& filepath)
    {
        if (!m_Scene || !root)
        {
            FX_CORE_ERROR("PrefabSerializer: gecersiz kok entity.");
            return false;
        }

        std::vector<Entity> subtree;
        CollectSubtree(root, subtree);

        json entities = json::array();
        for (Entity e : subtree)
        {
            json j = Detail::SerializeEntity(e);

            // Kokun parent'i prefab'in DISINDA kalir; onu yazarsak
            // ornekleme sirasinda cozulemeyen bir referans olur.
            if (e == root)
                j.erase("Parent");

            // Bir prefab ornegini yeni bir prefab olarak kaydediyor
            // olabiliriz; kaynak dosya baska bir prefab'a bagli olmamali.
            j.erase("PrefabInstance");

            entities.push_back(std::move(j));
        }

        json doc;
        doc["Version"]  = 1;
        doc["Type"]     = "FXPrefab";
        doc["Root"]     = static_cast<std::uint64_t>(root.GetUUID());
        doc["Entities"] = entities;

        const std::string fullPath = FileSystem::ResolveProjectAsset(filepath);

        {
            std::error_code ec;
            const auto parent = std::filesystem::path(fullPath).parent_path();
            if (!parent.empty())
                std::filesystem::create_directories(parent, ec);
        }

        std::ofstream out(fullPath);
        if (!out)
        {
            FX_CORE_ERROR("Prefab kaydedilemedi: %s", fullPath.c_str());
            return false;
        }

        out << std::setw(2) << doc << std::endl;

        FX_CORE_INFO("Prefab kaydedildi: %s (%zu entity)", fullPath.c_str(), subtree.size());
        return true;
    }

    Entity PrefabSerializer::Instantiate(const std::string& filepath, const glm::vec3& position)
    {
        if (!m_Scene)
            return {};

        const std::string fullPath = FileSystem::ResolveProjectAsset(filepath);

        std::ifstream in(fullPath);
        if (!in)
        {
            FX_CORE_ERROR("Prefab bulunamadi: %s", fullPath.c_str());
            return {};
        }

        json doc;
        try
        {
            in >> doc;
        }
        catch (const json::parse_error& err)
        {
            FX_CORE_ERROR("Prefab dosyasi bozuk: %s", err.what());
            return {};
        }

        if (doc.value("Type", std::string()) != "FXPrefab")
        {
            FX_CORE_ERROR("Bu bir prefab dosyasi degil: %s", fullPath.c_str());
            return {};
        }

        if (!doc.contains("Entities") || !doc["Entities"].is_array())
        {
            FX_CORE_ERROR("Prefab dosyasinda 'Entities' dizisi yok.");
            return {};
        }

        // Eski kimlik -> yeni entity. Prefab'in butun mekanizmasi bu tabloda.
        std::unordered_map<UUID, Entity> remap;
        std::vector<std::pair<Entity, UUID>> pendingParents;

        const UUID oldRootID{ doc.value("Root", std::uint64_t{ 0 }) };
        Entity newRoot;

        // Ornegin kaynagina baglanmasi icin (C-1). Yol degil GUID: dosya
        // tasininca bag kopmasin. Handle gecersizse (proje disi prefab)
        // bag yine kurulur ama Inspector "kayip" gosterir.
        const AssetHandle prefabHandle = AssetManager::GetHandle(filepath);

        for (const auto& e : doc["Entities"])
        {
            const std::string name = e.value("Tag", std::string("Entity"));

            // CreateEntity: YENI bir UUID uretir. Sahne yuklemenin tersi.
            Entity entity = m_Scene->CreateEntity(name);
            Detail::ApplyComponents(entity, e, m_Library);

            const UUID oldID{ e.value("ID", std::uint64_t{ 0 }) };
            if (oldID.IsValid())
                remap[oldID] = entity;

            // Her ornek entity kendi kaynak entity'sini bilir: alt agactaki
            // her dugum kendi SourceId'siyle damgalanir (C-2 Revert bunu
            // kaynak<->ornek eslemesi olarak kullanacak).
            auto& link    = entity.AddComponent<PrefabInstanceComponent>();
            link.Prefab   = prefabHandle;
            link.SourceId = oldID;

            if (oldID == oldRootID)
                newRoot = entity;

            if (e.contains("Parent"))
            {
                const UUID oldParent{ e["Parent"].get<std::uint64_t>() };
                if (oldParent.IsValid())
                    pendingParents.emplace_back(entity, oldParent);
            }
        }

        if (!newRoot)
        {
            FX_CORE_ERROR("Prefab kok entity'si bulunamadi: %s", fullPath.c_str());
            return {};
        }

        for (auto& [child, oldParent] : pendingParents)
        {
            auto it = remap.find(oldParent);
            if (it != remap.end())
                child.SetParent(it->second);
        }

        // Prefab ICINE bakan referanslari yeni kimliklere cevir. Disariya
        // bakan referanslara dokunulmuyor (RemapReferences tabloda olmayani
        // birakiyor). Ortak yardimci hem component EntityRef alanlarini hem
        // de script Entity alanlarini kapsiyor - eskiden burasi yalnizca
        // FollowComponent'i ceviriyordu ve script referanslarini iskaliyordu.
        std::unordered_map<UUID, UUID> uuidRemap;
        uuidRemap.reserve(remap.size());
        for (auto& [oldID, newEntity] : remap)
            uuidRemap[oldID] = newEntity.GetUUID();

        for (auto& [oldID, newEntity] : remap)
            Detail::RemapReferences(newEntity, uuidRemap);

        newRoot.GetComponent<TransformComponent>().Translation = position;

        FX_CORE_INFO("Prefab orneklendi: %s (%zu entity)", fullPath.c_str(), remap.size());
        return newRoot;
    }

    bool PrefabSerializer::RevertInstance(Entity instanceRoot)
    {
        if (!m_Scene || !instanceRoot)
            return false;

        if (!instanceRoot.HasComponent<PrefabInstanceComponent>())
        {
            FX_CORE_ERROR("Revert: entity bir prefab ornegi degil.");
            return false;
        }

        const AssetHandle handle =
            instanceRoot.GetComponent<PrefabInstanceComponent>().Prefab;
        const std::string relPath = AssetManager::GetPath(handle);
        if (relPath.empty())
        {
            FX_CORE_ERROR("Revert: prefab kaynagi bulunamadi (kayip GUID).");
            return false;
        }

        const std::string fullPath = FileSystem::ResolveProjectAsset(relPath);
        std::ifstream in(fullPath);
        if (!in)
        {
            FX_CORE_ERROR("Revert: prefab dosyasi acilamadi: %s", fullPath.c_str());
            return false;
        }

        json doc;
        try
        {
            in >> doc;
        }
        catch (const json::parse_error& err)
        {
            FX_CORE_ERROR("Revert: prefab dosyasi bozuk: %s", err.what());
            return false;
        }

        if (!doc.contains("Entities") || !doc["Entities"].is_array())
            return false;

        // Kaynak UUID -> json entity nesnesi. doc yasadigi surece gecerli.
        std::unordered_map<UUID, const json*> sourceById;
        for (const auto& e : doc["Entities"])
        {
            const UUID id{ e.value("ID", std::uint64_t{ 0 }) };
            if (id.IsValid())
                sourceById[id] = &e;
        }

        std::vector<Entity> subtree;
        CollectSubtree(instanceRoot, subtree);

        // Ic referanslari cevirmek icin kaynak->ornek UUID eslemesi.
        // Ornekleme sirasindaki remap'in tersi bilgisi: her damgali entity
        // hangi kaynaktan geldigini SourceId'de tutuyor.
        std::unordered_map<UUID, UUID> refRemap;
        for (Entity e : subtree)
        {
            if (!e.HasComponent<PrefabInstanceComponent>())
                continue;
            const UUID srcId = e.GetComponent<PrefabInstanceComponent>().SourceId;
            if (srcId.IsValid())
                refRemap[srcId] = e.GetUUID();
        }

        int reverted = 0;
        for (Entity e : subtree)
        {
            if (!e.HasComponent<PrefabInstanceComponent>())
                continue;

            const UUID srcId = e.GetComponent<PrefabInstanceComponent>().SourceId;
            const auto it = sourceById.find(srcId);
            if (it == sourceById.end())
                continue;   // kaynakta karsiligi yok (ornekte eklendi) - dokunma

            const json& src = *it->second;

            // Kok konumu korunuyor: Instantiate'in override ettigi tek sey
            // Translation'di; Revert de onu koruyor ki ornek yerinde kalsin.
            const bool isRoot = (e == instanceRoot);
            glm::vec3 keepTranslation{ 0.0f };
            if (isRoot && e.HasComponent<TransformComponent>())
                keepTranslation = e.GetComponent<TransformComponent>().Translation;

            // Ornekte EKLENMIS (kaynakta olmayan) component'leri kaldir.
            // PrefabInstance haric: bag revert'ten sag cikmali.
            for (const ComponentInfo& info : ComponentRegistry::GetAll())
            {
                if (!info.SerializedByTable)
                    continue;
                if (std::string_view(info.Name) == "PrefabInstance")
                    continue;
                if (info.Has(e) && !src.contains(info.Name))
                    info.Remove(e);
            }

            // Kaynak component'lerini uzerine yaz: eksigi ekler (ornekte
            // silinmisti), var olani kaynak degeriyle degistirir.
            Detail::ApplyComponents(e, src, m_Library);

            if (isRoot && e.HasComponent<TransformComponent>())
                e.GetComponent<TransformComponent>().Translation = keepTranslation;

            ++reverted;
        }

        // Ic referanslar kaynak kimligini gosteriyor (dosyadan oyle geldi);
        // ornek kimligine cevir. Disariya bakan referanslar dokunulmuyor.
        for (Entity e : subtree)
            Detail::RemapReferences(e, refRemap);

        FX_CORE_INFO("Prefab ornegi kaynagina donduruldu: %s (%d entity).",
                     relPath.c_str(), reverted);
        return reverted > 0;
    }
}
