#include "FXEngine/Scene/PrefabSerializer.h"
#include "FXEngine/Scene/Entity.h"
#include "FXEngine/Scene/Components.h"
#include "FXEngine/Core/Log.h"
#include "FXEngine/Core/FileSystem.h"

#include "Scene/EntitySerialization.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iomanip>
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

        for (const auto& e : doc["Entities"])
        {
            const std::string name = e.value("Tag", std::string("Entity"));

            // CreateEntity: YENI bir UUID uretir. Sahne yuklemenin tersi.
            Entity entity = m_Scene->CreateEntity(name);
            Detail::ApplyComponents(entity, e, m_Library);

            const UUID oldID{ e.value("ID", std::uint64_t{ 0 }) };
            if (oldID.IsValid())
                remap[oldID] = entity;

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
}
