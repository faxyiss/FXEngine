#include "Scene/EntitySerialization.h"

#include "FXEngine/Scene/Components.h"
#include "FXEngine/Core/Log.h"

namespace FX::Detail
{
    using json = nlohmann::json;

    namespace
    {
        json ToJson(const glm::vec2& v) { return json::array({ v.x, v.y }); }
        json ToJson(const glm::vec3& v) { return json::array({ v.x, v.y, v.z }); }
        json ToJson(const glm::vec4& v) { return json::array({ v.x, v.y, v.z, v.w }); }

        // Okurken savunmaci: alan yoksa veya boyutu yanlissa varsayilani
        // kullan, cokme. Sahne dosyasi elle duzenlenebilir olmali.
        glm::vec2 ToVec2(const json& j, const glm::vec2& fallback = glm::vec2(0.0f))
        {
            if (!j.is_array() || j.size() < 2) return fallback;
            return { j[0].get<float>(), j[1].get<float>() };
        }

        glm::vec3 ToVec3(const json& j, const glm::vec3& fallback = glm::vec3(0.0f))
        {
            if (!j.is_array() || j.size() < 3) return fallback;
            return { j[0].get<float>(), j[1].get<float>(), j[2].get<float>() };
        }

        glm::vec4 ToVec4(const json& j, const glm::vec4& fallback = glm::vec4(1.0f))
        {
            if (!j.is_array() || j.size() < 4) return fallback;
            return { j[0].get<float>(), j[1].get<float>(),
                     j[2].get<float>(), j[3].get<float>() };
        }
    }

    json SerializeEntity(Entity entity)
    {
        json e;

        if (entity.HasComponent<IDComponent>())
            e["ID"] = static_cast<std::uint64_t>(entity.GetComponent<IDComponent>().ID);

        if (entity.HasComponent<TagComponent>())
            e["Tag"] = entity.GetComponent<TagComponent>().Tag;

        // Sadece PARENT yaziliyor, cocuk listesi degil: cocuklar bundan
        // yeniden kurulabilir. Iki yonu de yazmak, ikisinin tutarsiz
        // kalabilecegi bir dosya formati demektir.
        if (entity.HasComponent<RelationshipComponent>())
        {
            const auto& rc = entity.GetComponent<RelationshipComponent>();
            if (rc.Parent.IsValid())
                e["Parent"] = static_cast<std::uint64_t>(rc.Parent);
        }

        if (entity.HasComponent<TransformComponent>())
        {
            const auto& tc = entity.GetComponent<TransformComponent>();
            e["Transform"] = {
                // RADYAN: ic temsille ayni. Derece cevrimi sadece arayuz
                // sinirinda yapilir.
                { "Translation", ToJson(tc.Translation) },
                { "Rotation",    tc.Rotation },
                { "Scale",       ToJson(tc.Scale) }
            };
        }

        if (entity.HasComponent<SpriteRendererComponent>())
        {
            const auto& sc = entity.GetComponent<SpriteRendererComponent>();

            json sprite;
            sprite["Color"]        = ToJson(sc.Color);
            sprite["TilingFactor"] = sc.TilingFactor;

            // Isaretci yerine YOL: shared_ptr bir calisma zamani adresi,
            // sonraki acilista anlamsiz. Texture'in kimligi dosya yolu.
            sprite["Texture"] = sc.Texture ? sc.Texture->GetPath() : std::string();

            e["SpriteRenderer"] = sprite;
        }

        if (entity.HasComponent<VelocityComponent>())
        {
            const auto& vc = entity.GetComponent<VelocityComponent>();
            e["Velocity"] = {
                { "Linear",  ToJson(vc.Linear) },
                { "Angular", vc.Angular }
            };
        }

        if (entity.HasComponent<FollowComponent>())
        {
            const auto& fc = entity.GetComponent<FollowComponent>();
            e["Follow"] = {
                { "Target",       static_cast<std::uint64_t>(fc.Target.Target) },
                { "Speed",        fc.Speed },
                { "StopDistance", fc.StopDistance }
            };
        }

        if (entity.HasComponent<CameraComponent>())
        {
            const auto& cc = entity.GetComponent<CameraComponent>();
            e["Camera"] = {
                { "OrthographicSize", cc.OrthographicSize },
                { "Primary",          cc.Primary }
            };
        }

        return e;
    }

    void ApplyComponents(Entity entity, const json& j, TextureLibrary* library)
    {
        if (j.contains("Transform"))
        {
            const auto& t = j["Transform"];
            auto& tc = entity.GetComponent<TransformComponent>();

            tc.Translation = ToVec3(t.value("Translation", json::array()));
            tc.Rotation    = t.value("Rotation", 0.0f);
            tc.Scale       = ToVec2(t.value("Scale", json::array()), glm::vec2(1.0f));
        }

        if (j.contains("SpriteRenderer"))
        {
            const auto& s = j["SpriteRenderer"];

            auto& sc = entity.AddOrReplaceComponent<SpriteRendererComponent>();
            sc.Color        = ToVec4(s.value("Color", json::array()));
            sc.TilingFactor = s.value("TilingFactor", 1.0f);

            const std::string texPath = s.value("Texture", std::string());
            if (!texPath.empty() && library)
            {
                sc.Texture = library->Load(texPath);

                if (!sc.Texture)
                    FX_CORE_WARN("Entity '%s': texture bulunamadi (%s), duz renk kullanilacak.",
                                 entity.GetName().c_str(), texPath.c_str());
            }
        }

        if (j.contains("Velocity"))
        {
            const auto& v = j["Velocity"];
            auto& vc = entity.AddOrReplaceComponent<VelocityComponent>();
            vc.Linear  = ToVec2(v.value("Linear", json::array()));
            vc.Angular = v.value("Angular", 0.0f);
        }

        if (j.contains("Follow"))
        {
            const auto& f = j["Follow"];
            auto& fc = entity.AddOrReplaceComponent<FollowComponent>();

            // Hedef entity henuz olusturulmamis olabilir; sorun degil,
            // FollowSystem cozumlemeyi her karede tekrarliyor.
            fc.Target       = UUID(f.value("Target", std::uint64_t{ 0 }));
            fc.Speed        = f.value("Speed", 2.0f);
            fc.StopDistance = f.value("StopDistance", 1.0f);
        }

        if (j.contains("Camera"))
        {
            const auto& c = j["Camera"];
            auto& cc = entity.AddOrReplaceComponent<CameraComponent>();
            cc.OrthographicSize = c.value("OrthographicSize", 8.0f);
            cc.Primary          = c.value("Primary", true);
        }
    }
}
