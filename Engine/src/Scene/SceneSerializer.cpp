#include "FXEngine/Scene/SceneSerializer.h"
#include "FXEngine/Scene/Entity.h"
#include "FXEngine/Scene/Components.h"
#include "FXEngine/Core/Log.h"
#include "FXEngine/Core/FileSystem.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iomanip>

namespace FX
{
    using json = nlohmann::json;

    namespace
    {
        // -------------------------------------------------------------------
        // glm tiplerini JSON'a cevirme yardimcilari
        // -------------------------------------------------------------------
        // Dizi olarak yaziyoruz: [1.0, 2.0, 3.0]
        // Alternatifi {"x":1,"y":2,"z":3} idi; dizi hem daha kisa hem
        // okunakli. Sahne dosyasi elle acilip duzenlenebilir kalmali.
        json ToJson(const glm::vec2& v) { return json::array({ v.x, v.y }); }
        json ToJson(const glm::vec3& v) { return json::array({ v.x, v.y, v.z }); }
        json ToJson(const glm::vec4& v) { return json::array({ v.x, v.y, v.z, v.w }); }

        // Okurken SAVUNMACI davraniyoruz: alan yoksa veya boyutu yanlissa
        // varsayilani kullan, cokme.
        //
        // Neden? Cunku sahne dosyasi elle duzenlenebilir ve eski surumlerle
        // uyumluluk gerekir. Bir alanin eksik olmasi programin cokmesine
        // sebep olmamali - bu, serilestirmenin temel kurallarindandir.
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

        // Bir entity'yi JSON nesnesine cevirir.
        json SerializeEntity(Entity entity)
        {
            json e;

            // UUID EN BASA. Sadece duzen icin degil: dosyayi elle
            // incelerken hangi entity'ye baktigini ilk satirda gormek
            // istersin, cunku referanslar bu sayilarla kuruluyor.
            //
            // uint64_t olarak yaziyoruz. JSON sayilari cift duyarlikli
            // ondalik olarak saklar ve 2^53 ustu tam sayilar hassasiyet
            // kaybedebilir - ama nlohmann/json tam sayilari ayri tutar
            // (integer/unsigned ayrimi yapar), bu yuzden 64-bit degerler
            // guvenle gidip gelir.
            if (entity.HasComponent<IDComponent>())
                e["ID"] = static_cast<std::uint64_t>(entity.GetComponent<IDComponent>().ID);

            if (entity.HasComponent<TagComponent>())
                e["Tag"] = entity.GetComponent<TagComponent>().Tag;

            if (entity.HasComponent<TransformComponent>())
            {
                const auto& tc = entity.GetComponent<TransformComponent>();
                e["Transform"] = {
                    { "Translation", ToJson(tc.Translation) },
                    // RADYAN olarak yaziyoruz - ic temsille ayni.
                    // Arayuz derece gosteriyor ama dosya ic veriyi
                    // yansitmali; cevrim sadece arayuz sinirinda olmali.
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

                // ISARETCI YERINE YOL.
                // Bir shared_ptr'i diske yazamayiz - calisma zamani
                // adresidir, sonraki acilista anlamsizdir. Onun yerine
                // texture'in KIMLIGINI (dosya yolu) yaziyoruz.
                // Yuklerken TextureLibrary bunu tekrar isaretciye cevirecek.
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

                // ISTE FAZ 8'IN BUTUN MESELESI.
                // Burada bir isaretci veya entt kimligi degil, hedefin
                // KALICI UUID'sini yaziyoruz. Yukleme sirasinda hedef
                // entity ayni UUID ile yeniden olusturulacagi icin
                // referans kendiliginden dogru yere baglanir.
                e["Follow"] = {
                    { "Target",       static_cast<std::uint64_t>(fc.Target.Target) },
                    { "Speed",        fc.Speed },
                    { "StopDistance", fc.StopDistance }
                };
            }

            return e;
        }
    }

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

        // Surum numarasi: ileride format degisirse eski dosyalari
        // taniyip donusturebilmek icin. Bugun kullanmiyoruz ama
        // SONRADAN EKLEMEK cok zordur - dosyalar coktan yazilmis olur.
        // Surum 2: entity UUID'leri ve FollowComponent eklendi.
        // Surum 1 dosyalari hala aciliyor (ID alani yoksa yeni uretiliyor) -
        // Faz 7'de bu alani "bugun kullanmiyoruz ama sonradan eklemek zor"
        // diye koymustuk; ilk faydasini simdi goruyoruz.
        root["Version"] = 2;
        root["Scene"]   = "Untitled";

        json entities = json::array();

        auto& registry = m_Scene->GetRegistry();
        auto view = registry.view<TagComponent>();

        // NOT: EnTT view'lari TERS SIRADA gezer (en son eklenen ilk gelir).
        // Bu, cizim sirasini etkilemedigi icin Faz 5'te onemsizdi ama
        // burada dosyadaki sirayi belirliyor. Sahnenin olusturma sirasini
        // korumak icin listeyi ters ceviriyoruz - boylece kaydet/yukle
        // sonrasi Hierarchy paneli ayni sirada gorunur.
        std::vector<entt::entity> ordered;
        ordered.reserve(view.size());
        for (auto id : view)
            ordered.push_back(id);

        for (auto it = ordered.rbegin(); it != ordered.rend(); ++it)
            entities.push_back(SerializeEntity(Entity{ *it, m_Scene }));

        root["Entities"] = entities;

        // --- Dosyaya yaz ---------------------------------------------------
        const std::string fullPath = FileSystem::ResolveAsset(filepath);

        // Ust klasoru olustur. std::ofstream KLASOR OLUSTURMAZ - klasor
        // yoksa dosya acilamaz ve kaydetme sessizce basarisiz olur.
        // Kullanicinin "assets/scenes/" klasorunu elle acmasini beklemek
        // kotu bir tasarim olurdu.
        //
        // std::filesystem C++17 ile geldi; ec (error_code) surumunu
        // kullaniyoruz cunku motorun geri kalaninda istisna kullanmiyoruz.
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

        // dump(2) = 2 bosluk girintili, INSAN OKUYABILIR cikti.
        // dump() tek satirda yazardi; daha kucuk olurdu ama sahne
        // dosyasini metin editoruyle acip incelemek/duzenlemek
        // ogrenme sureci icin cok degerli. Boyut burada oncelik degil.
        out << std::setw(2) << root << std::endl;

        FX_CORE_INFO("Sahne kaydedildi: %s (%zu entity)",
                     fullPath.c_str(), entities.size());
        return true;
    }

    bool SceneSerializer::Deserialize(const std::string& filepath)
    {
        if (!m_Scene)
            return false;

        const std::string fullPath = FileSystem::ResolveAsset(filepath);

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
            // nlohmann/json ISTISNA ATAR. Motorun geri kalaninda istisna
            // kullanmiyoruz ama burada YAKALAMAK zorundayiz: bozuk bir
            // JSON dosyasi programi cokertmemeli. Istisnayi kutuphanenin
            // sinirinda yakalayip kendi hata modelimize (bool donus)
            // ceviriyoruz. Bu, 3rd-party kutuphanelerle calisirken
            // standart bir yaklasimdir.
            FX_CORE_ERROR("Sahne dosyasi bozuk: %s", err.what());
            return false;
        }

        const int version = root.value("Version", 0);
        if (version == 1)
            FX_CORE_WARN("Sahne surumu 1 (Faz 7). UUID'ler yeniden uretilecek, "
                         "entity referanslari kaybolabilir.");
        else if (version != 2)
            FX_CORE_WARN("Sahne surumu %d, beklenen 2. Yine de denenecek.", version);

        if (!root.contains("Entities") || !root["Entities"].is_array())
        {
            FX_CORE_ERROR("Sahne dosyasinda 'Entities' dizisi yok.");
            return false;
        }

        // --- Mevcut sahneyi temizle ------------------------------------------
        // Yuklemek "uzerine eklemek" degil "degistirmek" demektir.
        //
        // Scene::Clear() kullaniyoruz, registry.clear() DEGIL: UUID
        // haritasinin da temizlenmesi sart. Faz 8 oncesi registry'yi
        // dogrudan temizliyorduk; artik Scene iki veri yapisi tutuyor
        // ve ikisi senkron kalmali.
        m_Scene->Clear();

        std::size_t loaded = 0;

        for (const auto& e : root["Entities"])
        {
            const std::string name = e.value("Tag", std::string("Entity"));

            // =============================================================
            // UUID'YI DOSYADAN AL, YENIDEN URETME.
            //
            // CreateEntity() cagirsaydik her entity yeni bir kimlik alirdi
            // ve FollowComponent'lerdeki hedef UUID'leri hicbir seye
            // isaret etmezdi. Referanslarin yuklemeden sag cikmasinin
            // tek yolu kimligi korumaktir.
            //
            // Eski (Faz 7) dosyalarda "ID" alani yok; o durumda yeni
            // bir UUID uretiyoruz. Boylece eski sahneler hala aciliyor -
            // Version alanini bosuna koymamistik.
            // =============================================================
            Entity entity;
            if (e.contains("ID"))
                entity = m_Scene->CreateEntityWithUUID(UUID(e["ID"].get<std::uint64_t>()), name);
            else
                entity = m_Scene->CreateEntity(name);

            if (e.contains("Transform"))
            {
                const auto& t = e["Transform"];
                auto& tc = entity.GetComponent<TransformComponent>();

                tc.Translation = ToVec3(t.value("Translation", json::array()));
                tc.Rotation    = t.value("Rotation", 0.0f);
                tc.Scale       = ToVec2(t.value("Scale", json::array()), glm::vec2(1.0f));
            }

            if (e.contains("SpriteRenderer"))
            {
                const auto& s = e["SpriteRenderer"];

                auto& sc = entity.AddComponent<SpriteRendererComponent>();
                sc.Color        = ToVec4(s.value("Color", json::array()));
                sc.TilingFactor = s.value("TilingFactor", 1.0f);

                // YOL -> TEXTURE.
                // Kutuphane ayni yolu ikinci kez isteyince diske gitmiyor;
                // 100 entity ayni dokuyu paylasiyorsa tek yukleme yapiliyor.
                const std::string texPath = s.value("Texture", std::string());
                if (!texPath.empty() && m_Library)
                {
                    sc.Texture = m_Library->Load(texPath);

                    if (!sc.Texture)
                        FX_CORE_WARN("Entity '%s': texture bulunamadi (%s), duz renk kullanilacak.",
                                     name.c_str(), texPath.c_str());
                }
            }

            if (e.contains("Velocity"))
            {
                const auto& v = e["Velocity"];
                auto& vc = entity.AddComponent<VelocityComponent>();
                vc.Linear  = ToVec2(v.value("Linear", json::array()));
                vc.Angular = v.value("Angular", 0.0f);
            }

            if (e.contains("Follow"))
            {
                const auto& f = e["Follow"];
                auto& fc = entity.AddComponent<FollowComponent>();

                // Hedef UUID'sini oldugu gibi aliyoruz.
                //
                // DIKKAT: Hedef entity HENUZ OLUSTURULMAMIS olabilir -
                // dosyada bizden sonra geliyorsa. Bu sorun DEGIL, cunku
                // cozumlemeyi burada yapmiyoruz; FollowSystem her karede
                // aramayi tekrarliyor. "Gec cozumleme"nin ikinci faydasi
                // bu: yukleme sirasi onemsizlesiyor.
                //
                // Isaretci cozseydik, iki gecisli bir yukleyici yazmak
                // zorunda kalirdik (once tum entity'ler, sonra referanslar).
                fc.Target       = UUID(f.value("Target", std::uint64_t{ 0 }));
                fc.Speed        = f.value("Speed", 2.0f);
                fc.StopDistance = f.value("StopDistance", 1.0f);
            }

            ++loaded;
        }

        FX_CORE_INFO("Sahne yuklendi: %s (%zu entity)", fullPath.c_str(), loaded);
        return true;
    }
}
