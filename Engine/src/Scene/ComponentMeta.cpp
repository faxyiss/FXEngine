#include "FXEngine/Scene/ComponentMeta.h"

#include "FXEngine/Asset/AssetManager.h"
#include "FXEngine/Core/Log.h"
#include "FXEngine/Renderer/TextureLibrary.h"
#include "FXEngine/Scene/Components.h"

#include <nlohmann/json.hpp>

namespace FX
{
    using json = nlohmann::json;

    std::deque<ComponentInfo>& ComponentRegistry::Entries()
    {
        static std::deque<ComponentInfo> s_Entries;
        return s_Entries;
    }

    namespace { bool s_BuiltinsRegistered = false; }

    const std::deque<ComponentInfo>& ComponentRegistry::GetAll()
    {
        RegisterBuiltins();
        return Entries();
    }

    ComponentInfo* ComponentRegistry::Find(std::string_view name)
    {
        RegisterBuiltins();

        for (ComponentInfo& info : Entries())
        {
            if (name == info.Name)
                return &info;
        }
        return nullptr;
    }

    void ComponentRegistry::Reset()
    {
        Entries().clear();
        s_BuiltinsRegistered = false;
    }

    namespace
    {
        // SpriteRenderer'in dokusu alan tablosuyla ifade edilemiyor:
        // component'te bir shared_ptr duruyor, dosyada bir GUID.
        // Bu cevrimi tablo yapamaz, bu yuzden ozel kanca.
        void SaveSpriteTexture(const void* component, json& out)
        {
            const auto& sc = *static_cast<const SpriteRendererComponent*>(component);

            // Isaretci yerine KIMLIK: shared_ptr bir calisma zamani adresi,
            // sonraki acilista anlamsiz. Yol da yaziliyor ama SADECE insan
            // okusun diye - yukleme onu kullanmiyor.
            if (sc.Texture)
            {
                const std::string path = sc.Texture->GetPath();
                out["TextureHandle"] = static_cast<std::uint64_t>(AssetManager::GetHandle(path));
                out["TexturePath"]   = path;
            }
            else
            {
                out["TextureHandle"] = std::uint64_t{ 0 };
            }
        }

        void LoadSpriteTexture(void* component, const json& in, TextureLibrary* library)
        {
            auto& sc = *static_cast<SpriteRendererComponent*>(component);

            // Once GUID (surum 4+), yoksa eski "Texture" yolu (surum <=3).
            // Eski sahneleri kirmiyoruz; kaydedildiginde yeni bicime gecerler.
            std::string texPath;

            if (in.contains("TextureHandle"))
            {
                const auto raw = in.value("TextureHandle", std::uint64_t{ 0 });
                if (raw != 0)
                {
                    texPath = AssetManager::GetPath(AssetHandle(raw));

                    if (texPath.empty())
                    {
                        // GUID var ama karsiligi yok: varlik silinmis veya
                        // proje disindan gelmis olabilir. Dosyadaki yol
                        // ipucusuna dusuyoruz - sessizce duz renk
                        // gostermektense elimizdeki en iyi bilgiyi kullan.
                        texPath = in.value("TexturePath", std::string());

                        if (!texPath.empty())
                            FX_CORE_WARN("Varlik kimligi %llu bulunamadi, dosyadaki "
                                         "yola donuluyor (%s).",
                                         static_cast<unsigned long long>(raw),
                                         texPath.c_str());
                    }
                }
            }
            else
            {
                texPath = in.value("Texture", std::string());   // surum <= 3
            }

            if (!texPath.empty() && library)
            {
                sc.Texture = library->Load(texPath);

                if (!sc.Texture)
                    FX_CORE_WARN("Texture bulunamadi (%s), duz renk kullanilacak.",
                                 texPath.c_str());
            }
        }
    }

    void ComponentRegistry::RegisterBuiltins()
    {
        if (s_BuiltinsRegistered)
            return;
        s_BuiltinsRegistered = true;

        // --- Yapisal: yalnizca kopyalamaya katilirlar ------------------------
        ComponentRegistry::Register<TagComponent>("Tag", "Tag").Structural();
        ComponentRegistry::Register<RelationshipComponent>("Relationship", "Hiyerarsi")
            .Structural();
        ComponentRegistry::Register<WorldTransformComponent>("WorldTransform", "Dunya Transform")
            .Structural();

        // --- Duzenlenebilir component'ler -----------------------------------
        // Kayit sirasi Inspector'daki sira.
        //
        // Alan adlari ("Translation", "Rotation"...) DOSYA FORMATIDIR;
        // mevcut .fxscene dosyalariyla birebir ayni olmalari sart.

        ComponentRegistry::Register<TransformComponent>("Transform", "Transform")
            .Field<&TransformComponent::Translation>("Translation", "Konum")
            .Field<&TransformComponent::Rotation>("Rotation",    "Donme (der)")
                .Degrees().Speed(1.0f)
            .Field<&TransformComponent::Scale>("Scale",       "Olcek")
            .NotRemovable().HiddenInAddMenu();

        ComponentRegistry::Register<SpriteRendererComponent>("SpriteRenderer", "Sprite Renderer")
            .Field<&SpriteRendererComponent::Color>("Color",        "Renk").AsColor()
            .Field<&SpriteRendererComponent::TilingFactor>("TilingFactor", "Tiling").Speed(0.1f)
            .Extra(SaveSpriteTexture, LoadSpriteTexture);

        ComponentRegistry::Register<VelocityComponent>("Velocity", "Velocity")
            .Field<&VelocityComponent::Linear>("Linear",  "Dogrusal").Speed(0.1f)
            .Field<&VelocityComponent::Angular>("Angular", "Acisal (der/s)")
                .Degrees().Speed(5.0f);

        ComponentRegistry::Register<CameraComponent>("Camera", "Camera")
            .Field<&CameraComponent::OrthographicSize>("OrthographicSize", "Boyut")
                .Speed(0.1f).Range(0.1f, 1000.0f)
            // Tekillik mantigi ozel arayuzde: birincil isaretlenince
            // digerlerinin isareti kalkmali.
            .Field<&CameraComponent::Primary>("Primary", "Birincil").CustomUI();

        ComponentRegistry::Register<FollowComponent>("Follow", "Follow")
            .Field<&FollowComponent::Target>("Target",       "Hedef")
            .Field<&FollowComponent::Speed>("Speed",        "Hiz").Speed(0.1f)
            .Field<&FollowComponent::StopDistance>("StopDistance", "Durma mesafesi").Speed(0.1f)
            // FollowSystem uc component istiyor; Velocity yoksa takip
            // SESSIZCE calismaz.
            .OnAdded([](Entity e) { e.AddOrReplaceIfMissing<VelocityComponent>(); });

        ComponentRegistry::Register<NativeScriptComponent>("NativeScript", "Native Script")
            // Ad, kayitli script'ler listesinden secilir; serbest metin
            // kutusu yanlis ad yazmayi mumkun kilardi.
            .Field<&NativeScriptComponent::ScriptName>("Name", "Script").CustomUI();

        FX_CORE_INFO("ComponentRegistry: %zu component kayitli.", Entries().size());
    }
}
