#include "FXEngine/Scene/SceneSerializer.h"
#include "FXEngine/Scene/Entity.h"
#include "FXEngine/Scene/Components.h"
#include "FXEngine/Core/Log.h"
#include "FXEngine/Core/FileSystem.h"

#include "Scene/EntitySerialization.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <utility>
#include <vector>
#include <fstream>
#include <iomanip>

namespace FX
{
    using json = nlohmann::json;

    SceneSerializer::SceneSerializer(Scene* scene, TextureLibrary* textureLibrary)
        : m_Scene(scene), m_Library(textureLibrary)
    {
    }

    bool SceneSerializer::Serialize(const std::string& filepath)
    {
        if (!m_Scene)
        {
            FX_CORE_ERROR("SceneSerializer: sahne yok.");
            return false;
        }

        json root;

        // Surum 1: Faz 7. Surum 2: UUID + Follow. Surum 3: hiyerarsi.
        // Eski surumler hala aciliyor.
        root["Version"] = 3;
        root["Scene"]   = "Untitled";

        json entities = json::array();

        auto& registry = m_Scene->GetRegistry();
        auto view = registry.view<TagComponent>();

        // EnTT view'lari TERS SIRADA gezer (en son eklenen ilk gelir).
        // Sahnenin olusturma sirasini korumak icin ters ceviriyoruz -
        // boylece kaydet/yukle sonrasi Hierarchy paneli ayni sirada gorunur.
        std::vector<entt::entity> ordered;
        ordered.reserve(view.size());
        for (auto id : view)
            ordered.push_back(id);

        for (auto it = ordered.rbegin(); it != ordered.rend(); ++it)
            entities.push_back(Detail::SerializeEntity(Entity{ *it, m_Scene }));

        root["Entities"] = entities;

        const std::string fullPath = FileSystem::ResolveProjectAsset(filepath);

        // std::ofstream KLASOR OLUSTURMAZ; klasor yoksa kaydetme sessizce
        // basarisiz olur.
        {
            std::error_code ec;
            const auto parent = std::filesystem::path(fullPath).parent_path();
            if (!parent.empty())
                std::filesystem::create_directories(parent, ec);

            if (ec)
                FX_CORE_WARN("Klasor olusturulamadi: %s", ec.message().c_str());
        }

        std::ofstream out(fullPath);
        if (!out)
        {
            FX_CORE_ERROR("Sahne kaydedilemedi (dosya acilamadi): %s", fullPath.c_str());
            return false;
        }

        // dump(2): insan okuyabilir cikti. Sahne dosyasini metin editoruyle
        // acip incelemek ogrenme surecinde boyuttan daha degerli.
        out << std::setw(2) << root << std::endl;

        FX_CORE_INFO("Sahne kaydedildi: %s (%zu entity)",
                     fullPath.c_str(), entities.size());
        return true;
    }

    bool SceneSerializer::Deserialize(const std::string& filepath)
    {
        if (!m_Scene)
            return false;

        const std::string fullPath = FileSystem::ResolveProjectAsset(filepath);

        std::ifstream in(fullPath);
        if (!in)
        {
            FX_CORE_ERROR("Sahne yuklenemedi (dosya bulunamadi): %s", fullPath.c_str());
            return false;
        }

        json root;
        try
        {
            in >> root;
        }
        catch (const json::parse_error& err)
        {
            // nlohmann/json istisna atar. Motorun geri kalaninda istisna
            // kullanmiyoruz; kutuphane sinirinda yakalayip kendi hata
            // modelimize (bool donus) ceviriyoruz.
            FX_CORE_ERROR("Sahne dosyasi bozuk: %s", err.what());
            return false;
        }

        const int version = root.value("Version", 0);
        if (version == 1)
            FX_CORE_WARN("Sahne surumu 1 (Faz 7). UUID'ler yeniden uretilecek, "
                         "entity referanslari kaybolabilir.");
        else if (version != 2 && version != 3)
            FX_CORE_WARN("Sahne surumu %d, beklenen 3. Yine de denenecek.", version);

        if (!root.contains("Entities") || !root["Entities"].is_array())
        {
            FX_CORE_ERROR("Sahne dosyasinda 'Entities' dizisi yok.");
            return false;
        }

        // Yuklemek "uzerine eklemek" degil "degistirmek" demektir.
        // Scene::Clear(), registry.clear() DEGIL: UUID haritasi da temizlenmeli.
        m_Scene->Clear();

        std::size_t loaded = 0;

        // Parent baglantilari IKINCI GECISTE: dosyada bir cocuk parent'indan
        // once gelebilir.
        std::vector<std::pair<Entity, UUID>> pendingParents;

        for (const auto& e : root["Entities"])
        {
            const std::string name = e.value("Tag", std::string("Entity"));

            // UUID'yi DOSYADAN AL, yeniden uretme: referanslarin yuklemeden
            // sag cikmasinin tek yolu kimligi korumaktir. Faz 7 dosyalarinda
            // "ID" alani yok, o durumda yenisi uretilir.
            Entity entity;
            if (e.contains("ID"))
                entity = m_Scene->CreateEntityWithUUID(UUID(e["ID"].get<std::uint64_t>()), name);
            else
                entity = m_Scene->CreateEntity(name);

            Detail::ApplyComponents(entity, e, m_Library);

            if (e.contains("Parent"))
            {
                const UUID parentID{ e["Parent"].get<std::uint64_t>() };
                if (parentID.IsValid())
                    pendingParents.emplace_back(entity, parentID);
            }

            ++loaded;
        }

        for (auto& [child, parentID] : pendingParents)
        {
            Entity parent = m_Scene->FindEntityByUUID(parentID);
            if (parent)
                child.SetParent(parent);
            else
                FX_CORE_WARN("Entity '%s': parent bulunamadi (%llu), koke birakildi.",
                             child.GetName().c_str(),
                             static_cast<unsigned long long>(parentID));
        }

        FX_CORE_INFO("Sahne yuklendi: %s (%zu entity, %zu hiyerarsi bagi)",
                     fullPath.c_str(), loaded, pendingParents.size());
        return true;
    }
}
