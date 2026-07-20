#include "SceneHierarchyPanel.h"
#include "Panels/ContentBrowserPanel.h"

#include <FXEngine/Scene/Components.h>
#include <FXEngine/Core/Log.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <glm/gtc/type_ptr.hpp>

#include <cstring>
#include <string>

namespace FXEd
{
    void SceneHierarchyPanel::SetContext(FX::Scene* scene)
    {
        m_Scene = scene;

        // Sahne degisince secimi TEMIZLE. Yoksa eski sahnenin entity
        // kimligi yeni sahnede baska bir entity'ye denk gelir ve
        // Inspector alakasiz veri gosterir. Sinsi bir hata turudur.
        m_Selection = {};
    }

    FX::Entity SceneHierarchyPanel::TakePrefabRequest()
    {
        FX::Entity request = m_PrefabRequest;
        m_PrefabRequest = {};
        return request;
    }

    void SceneHierarchyPanel::OnImGuiRender()
    {
        if (!m_Scene)
            return;

        // ===================================================================
        // HIERARCHY PANELI
        // ===================================================================
        ImGui::Begin("Hierarchy");

        ImGui::Text("Entity sayisi: %u", m_Scene->GetEntityCount());
        ImGui::Separator();

        auto& registry = m_Scene->GetRegistry();

        // TagComponent'e sahip TUM entity'ler uzerinde gez.
        // Scene::CreateEntity her entity'ye Tag ekliyor, yani bu
        // pratikte "tum entity'ler" demek.
        //
        // NOT: view uzerinde gezerken entity SILMEK tehlikelidir -
        // dizi altimizdan degisir. Bu yuzden silme istegini bir
        // degiskende biriktirip dongu BITTIKTEN SONRA uyguluyoruz.
        m_ToDelete = {};
        m_ReparentChild  = {};
        m_ReparentTarget = {};
        m_ReparentToRoot = false;

        // Sadece KOKLERI geziyoruz; cocuklar DrawEntityNode icinde
        // ozyineleme ile ciziliyor.
        auto view = registry.view<FX::TagComponent>();
        for (auto entityID : view)
        {
            FX::Entity entity{ entityID, m_Scene };

            const bool isRoot = !entity.HasComponent<FX::RelationshipComponent>() ||
                                !entity.GetComponent<FX::RelationshipComponent>().Parent.IsValid();
            if (isRoot)
                DrawEntityNode(entity);
        }

        // Bos alana birakma: koke tasi.
        if (ImGui::BeginDragDropTargetCustom(ImGui::GetCurrentWindow()->Rect(), ImGui::GetID("HierarchyRoot")))
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FX_ENTITY"))
            {
                const auto handle = *static_cast<const entt::entity*>(payload->Data);
                m_ReparentChild  = FX::Entity{ handle, m_Scene };
                m_ReparentToRoot = true;
            }
            ImGui::EndDragDropTarget();
        }

        // Bos alana sol tik -> secimi kaldir.
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsWindowHovered())
            m_Selection = {};

        // Bos alana sag tik -> yeni entity olustur.
        if (ImGui::BeginPopupContextWindow("HierarchyContext",
                                           ImGuiPopupFlags_MouseButtonRight |
                                           ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::MenuItem("Bos Entity Olustur"))
                m_Selection = m_Scene->CreateEntity("Yeni Entity");
            ImGui::EndPopup();
        }

        ImGui::End();

        // Yapiyi degistiren islemler dongu DISINDA: agac uzerinde
        // gezerken parent/child listelerini degistirmek yineleyicileri
        // gecersiz kilardi.
        if (m_ReparentChild)
        {
            if (m_ReparentToRoot)
                m_ReparentChild.SetParent({});
            else if (m_ReparentTarget)
                m_ReparentChild.SetParent(m_ReparentTarget);
        }

        if (m_ToDelete)
        {
            // Secili entity silinenin ALTINDA olabilir; o da yok olacak.
            if (m_Selection && (m_Selection == m_ToDelete || m_ToDelete.IsAncestorOf(m_Selection)))
                m_Selection = {};
            m_Scene->DestroyEntity(m_ToDelete);
        }

        // ===================================================================
        // INSPECTOR PANELI
        // ===================================================================
        ImGui::Begin("Inspector");

        if (m_Selection)
            DrawComponents(m_Selection);
        else
            ImGui::TextDisabled("Hierarchy'den bir entity sec.");

        ImGui::End();
    }

    void SceneHierarchyPanel::DrawEntityNode(FX::Entity entity)
    {
        const auto& tag = entity.GetComponent<FX::TagComponent>().Tag;
        const bool hasChildren = entity.HasChildren();

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth |
                                   ImGuiTreeNodeFlags_OpenOnArrow;

        if (!hasChildren)
            flags |= ImGuiTreeNodeFlags_Leaf;
        if (m_Selection == entity)
            flags |= ImGuiTreeNodeFlags_Selected;

        // ImGui bir "ID yigini" tutar. Ayni isimde iki entity varsa
        // ikisi de ayni ID'yi alir ve birine tiklayinca digeri secilir.
        const auto handle = entity.GetHandle();
        const auto rawID  = static_cast<std::uint32_t>(handle);
        const bool opened = ImGui::TreeNodeEx(
            reinterpret_cast<void*>(static_cast<std::uintptr_t>(rawID)),
            flags, "%s", tag.c_str());

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            m_Selection = entity;

        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Alt Entity Ekle"))
            {
                FX::Entity child = m_Scene->CreateEntity("Yeni Entity");
                m_ReparentChild  = child;
                m_ReparentTarget = entity;
                m_Selection      = child;
            }
            if (ImGui::MenuItem("Koke Tasi", nullptr, false, static_cast<bool>(entity.GetParent())))
            {
                m_ReparentChild  = entity;
                m_ReparentToRoot = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Prefab Olarak Kaydet..."))
                m_PrefabRequest = entity;
            ImGui::Separator();
            if (ImGui::MenuItem("Entity'yi Sil"))
                m_ToDelete = entity;
            ImGui::EndPopup();
        }

        // Surukle: entt kimligini tasiyoruz (ayni kare icinde gecerli).
        if (ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload("FX_ENTITY", &handle, sizeof(handle));
            ImGui::Text("%s", tag.c_str());
            ImGui::EndDragDropSource();
        }

        // Birak: suruklenen entity bunun cocugu olsun.
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FX_ENTITY"))
            {
                const auto dropped = *static_cast<const entt::entity*>(payload->Data);
                m_ReparentChild  = FX::Entity{ dropped, m_Scene };
                m_ReparentTarget = entity;
                m_ReparentToRoot = false;
            }
            ImGui::EndDragDropTarget();
        }

        if (opened)
        {
            for (FX::Entity child : entity.GetChildren())
                DrawEntityNode(child);
            ImGui::TreePop();
        }
    }

    namespace
    {
        // Etiketli, surukle-degistir bir vec2 alani.
        // ImGui'de tekrar eden kaliplari kucuk yardimcilara ayirmak,
        // panel kodunu okunur tutar.
        void DrawVec2Control(const char* label, glm::vec2& values, float speed = 0.05f)
        {
            ImGui::PushID(label);
            ImGui::Text("%s", label);
            ImGui::SameLine(110.0f);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::DragFloat2("##v", glm::value_ptr(values), speed);
            ImGui::PopID();
        }

        void DrawVec3Control(const char* label, glm::vec3& values, float speed = 0.05f)
        {
            ImGui::PushID(label);
            ImGui::Text("%s", label);
            ImGui::SameLine(110.0f);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::DragFloat3("##v", glm::value_ptr(values), speed);
            ImGui::PopID();
        }

        void DrawFloatControl(const char* label, float& value, float speed = 0.05f)
        {
            ImGui::PushID(label);
            ImGui::Text("%s", label);
            ImGui::SameLine(110.0f);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::DragFloat("##f", &value, speed);
            ImGui::PopID();
        }
    }

    void SceneHierarchyPanel::DrawTextureSlot(FX::SpriteRendererComponent& sc)
    {
        ImGui::Text("Texture");
        ImGui::SameLine(110.0f);

        const ImVec2 slotSize(64.0f, 64.0f);

        if (sc.Texture)
        {
            // UV'ler ters: OpenGL sol-alt kokenli, ImGui sol-ust bekler.
            ImGui::Image(static_cast<ImTextureID>(sc.Texture->GetRendererID()),
                         slotSize, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
        }
        else
        {
            ImGui::Button("Doku\nyok", slotSize);
        }

        // BIRAKMA HEDEFI. Icerik panelinden gelen yolu kutuphaneye veriyoruz;
        // kutuphane ayni yolu ikinci kez gorurse diske gitmiyor.
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kContentPayload))
            {
                const char* path = static_cast<const char*>(payload->Data);

                if (m_Library)
                {
                    if (auto texture = m_Library->Load(path))
                        sc.Texture = texture;
                    else
                        FX_WARN("Doku yuklenemedi: %s", path);
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::SameLine();
        ImGui::BeginGroup();
        if (sc.Texture)
        {
            ImGui::TextDisabled("%s", sc.Texture->GetPath().c_str());
            if (ImGui::SmallButton("Dokuyu Kaldir"))
                sc.Texture = nullptr;
        }
        else
        {
            ImGui::TextDisabled("Icerik panelinden\nbir resim surukle");
        }
        ImGui::EndGroup();
    }

    void SceneHierarchyPanel::DrawComponents(FX::Entity entity)
    {
        // --- Kimlik ------------------------------------------------------------
        // UUID salt okunur: kullanicinin degistirmesi anlamsiz ve tehlikeli
        // olurdu (referanslar kopar). Ama GORUNMESI gerekiyor - bir
        // FollowComponent hedefini ayarlarken bu sayiyi okuyacaksin.
        if (entity.HasComponent<FX::IDComponent>())
        {
            const auto id = static_cast<std::uint64_t>(
                entity.GetComponent<FX::IDComponent>().ID);

            ImGui::TextDisabled("UUID");
            ImGui::SameLine(110.0f);
            ImGui::TextDisabled("%llu", static_cast<unsigned long long>(id));

            // Kopyalama kolayligi: uzun sayilari elle yazmak istemezsin.
            if (ImGui::IsItemClicked())
            {
                ImGui::SetClipboardText(std::to_string(id).c_str());
                FX_INFO("UUID panoya kopyalandi: %llu",
                        static_cast<unsigned long long>(id));
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Kopyalamak icin tikla");
        }

        // --- Tag ---------------------------------------------------------------
        if (entity.HasComponent<FX::TagComponent>())
        {
            auto& tag = entity.GetComponent<FX::TagComponent>().Tag;

            // ImGui C API'siyle calisir: std::string degil char tamponu ister.
            // Kullanicinin yazdigini tampona alip sonra string'e geri yaziyoruz.
            char buffer[128];
            std::memset(buffer, 0, sizeof(buffer));
            std::strncpy(buffer, tag.c_str(), sizeof(buffer) - 1);

            ImGui::Text("Isim");
            ImGui::SameLine(110.0f);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
                tag = std::string(buffer);
        }

        if (FX::Entity parent = entity.GetParent())
        {
            ImGui::TextDisabled("Parent");
            ImGui::SameLine(110.0f);
            ImGui::TextDisabled("%s", parent.GetName().c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Ayir"))
                m_ReparentChild = entity, m_ReparentToRoot = true;
        }

        ImGui::Separator();

        // --- Transform ---------------------------------------------------------
        if (entity.HasComponent<FX::TransformComponent>())
        {
            if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
            {
                auto& tc = entity.GetComponent<FX::TransformComponent>();

                if (entity.GetParent())
                    ImGui::TextDisabled("(parent'a gore yerel)");

                DrawVec3Control("Konum", tc.Translation);

                // ARAYUZDE DERECE, VERIDE RADYAN.
                // Component radyan tutuyor (matematik fonksiyonlari oyle
                // istiyor) ama insan derece ile dusunur. Cevrimi tam
                // burada, sinirda yapiyoruz - Faz 5 notlarinda planladigimiz gibi.
                float degrees = glm::degrees(tc.Rotation);
                DrawFloatControl("Donme (der)", degrees, 1.0f);
                tc.Rotation = glm::radians(degrees);

                DrawVec2Control("Olcek", tc.Scale);
            }
        }

        // --- SpriteRenderer -----------------------------------------------------
        if (entity.HasComponent<FX::SpriteRendererComponent>())
        {
            bool keep = true;
            if (ImGui::CollapsingHeader("Sprite Renderer", &keep,
                                        ImGuiTreeNodeFlags_DefaultOpen))
            {
                auto& sc = entity.GetComponent<FX::SpriteRendererComponent>();

                ImGui::Text("Renk");
                ImGui::SameLine(110.0f);
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::ColorEdit4("##Color", glm::value_ptr(sc.Color));

                DrawFloatControl("Tiling", sc.TilingFactor, 0.1f);

                DrawTextureSlot(sc);
            }

            // CollapsingHeader'in 'X' dugmesi -> component'i kaldir.
            // Faz 5'te H tusuyla yaptigimiz seyin arayuzlu hali.
            if (!keep)
                entity.RemoveComponent<FX::SpriteRendererComponent>();
        }

        // --- Velocity ------------------------------------------------------------
        if (entity.HasComponent<FX::VelocityComponent>())
        {
            bool keep = true;
            if (ImGui::CollapsingHeader("Velocity", &keep, ImGuiTreeNodeFlags_DefaultOpen))
            {
                auto& vc = entity.GetComponent<FX::VelocityComponent>();

                DrawVec2Control("Dogrusal", vc.Linear, 0.1f);

                float angDeg = glm::degrees(vc.Angular);
                DrawFloatControl("Acisal (der/s)", angDeg, 5.0f);
                vc.Angular = glm::radians(angDeg);
            }

            if (!keep)
                entity.RemoveComponent<FX::VelocityComponent>();
        }

        // --- Camera ---------------------------------------------------------------
        if (entity.HasComponent<FX::CameraComponent>())
        {
            bool keep = true;
            if (ImGui::CollapsingHeader("Camera", &keep, ImGuiTreeNodeFlags_DefaultOpen))
            {
                auto& cc = entity.GetComponent<FX::CameraComponent>();

                DrawFloatControl("Boyut", cc.OrthographicSize, 0.1f);
                if (cc.OrthographicSize < 0.1f)
                    cc.OrthographicSize = 0.1f;

                ImGui::Text("Birincil");
                ImGui::SameLine(110.0f);
                if (ImGui::Checkbox("##Primary", &cc.Primary) && cc.Primary)
                {
                    // Tek birincil kamera olmali. Isaretlenen kamerayi
                    // birincil yaparken digerlerinin isaretini kaldiriyoruz;
                    // aksi halde "hangisi kazanir" sorusunun cevabi
                    // registry'deki siraya kalirdi - kullanicinin
                    // goremedigi bir sey.
                    auto view = m_Scene->GetRegistry().view<FX::CameraComponent>();
                    for (auto other : view)
                    {
                        if (other != entity.GetHandle())
                            view.get<FX::CameraComponent>(other).Primary = false;
                    }
                }

                ImGui::TextDisabled("Konum ve donme Transform'dan gelir.");
            }

            if (!keep)
                entity.RemoveComponent<FX::CameraComponent>();
        }

        // --- Follow ---------------------------------------------------------------
        if (entity.HasComponent<FX::FollowComponent>())
        {
            bool keep = true;
            if (ImGui::CollapsingHeader("Follow", &keep, ImGuiTreeNodeFlags_DefaultOpen))
            {
                auto& fc = entity.GetComponent<FX::FollowComponent>();

                // HEDEF SECICI.
                // Entity'yi ADIYLA gosteriyoruz ama sakladigimiz sey UUID.
                // Kullanici arayuzu okunabilir olmali, veri modeli kalici -
                // ikisi ayni sey olmak zorunda degil.
                FX::Entity target = m_Scene->FindEntityByUUID(fc.Target.Target);

                std::string preview = "Yok";
                if (fc.Target.IsSet())
                {
                    preview = target
                        ? target.GetComponent<FX::TagComponent>().Tag
                        // Hedef UUID dolu ama entity bulunamiyor: silinmis
                        // olabilir. Bunu SESSIZCE gizlemek yerine acikca
                        // gosteriyoruz, yoksa kullanici neden calismadigini
                        // anlayamaz.
                        : std::string("<kayip: ") +
                          std::to_string(static_cast<std::uint64_t>(fc.Target.Target)) + ">";
                }

                ImGui::Text("Hedef");
                ImGui::SameLine(110.0f);
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::BeginCombo("##Target", preview.c_str()))
                {
                    if (ImGui::Selectable("Yok", !fc.Target.IsSet()))
                        fc.Target.Clear();

                    // Sahnedeki tum entity'leri listele.
                    auto view = m_Scene->GetRegistry().view<FX::TagComponent, FX::IDComponent>();
                    for (auto id : view)
                    {
                        FX::Entity candidate{ id, m_Scene };

                        // Kendini takip etmek anlamsiz - secenegi hic sunma.
                        // (Faz 6'daki ilke: arayuz gecersiz eylemi mumkun kilmamali.)
                        if (candidate == entity)
                            continue;

                        const auto& tag = candidate.GetComponent<FX::TagComponent>().Tag;
                        const auto  uuid = candidate.GetComponent<FX::IDComponent>().ID;

                        // Ayni isimde birden fazla entity olabilir; ImGui'nin
                        // ID yiginina UUID'yi itiyoruz ki secimler karismasin.
                        ImGui::PushID(static_cast<int>(static_cast<std::uint64_t>(uuid)));
                        if (ImGui::Selectable(tag.c_str(), uuid == fc.Target.Target))
                            fc.Target.Target = uuid;
                        ImGui::PopID();
                    }
                    ImGui::EndCombo();
                }

                DrawFloatControl("Hiz", fc.Speed, 0.1f);
                DrawFloatControl("Durma mesafesi", fc.StopDistance, 0.1f);

                // FollowSystem, Velocity'ye YAZIYOR. Velocity yoksa
                // sistem bu entity'yi hic gormez (view uc component istiyor).
                // Kullaniciyi bu sessiz basarisizliktan haberdar et.
                if (!entity.HasComponent<FX::VelocityComponent>())
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f),
                                       "Velocity component'i gerekli!");
                    if (ImGui::Button("Velocity Ekle"))
                        entity.AddComponent<FX::VelocityComponent>();
                }
            }

            if (!keep)
                entity.RemoveComponent<FX::FollowComponent>();
        }

        ImGui::Separator();
        DrawAddComponentMenu(entity);
    }

    void SceneHierarchyPanel::DrawAddComponentMenu(FX::Entity entity)
    {
        // Butonu ortala (kozmetik ama panel derli toplu gorunur).
        const float width = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + width * 0.15f);

        if (ImGui::Button("Component Ekle", ImVec2(width * 0.7f, 0.0f)))
            ImGui::OpenPopup("AddComponent");

        if (ImGui::BeginPopup("AddComponent"))
        {
            // ZATEN VAR OLANLARI GOSTERMIYORUZ.
            // Gosterseydik kullanici tiklar, AddComponent'teki assert
            // tetiklenir ve program debug'da durur. Arayuz, gecersiz
            // eylemi mumkun kilmamali - hata mesaji gostermekten iyidir.
            if (!entity.HasComponent<FX::SpriteRendererComponent>())
            {
                if (ImGui::MenuItem("Sprite Renderer"))
                {
                    entity.AddComponent<FX::SpriteRendererComponent>();
                    ImGui::CloseCurrentPopup();
                }
            }

            if (!entity.HasComponent<FX::VelocityComponent>())
            {
                if (ImGui::MenuItem("Velocity"))
                {
                    entity.AddComponent<FX::VelocityComponent>();
                    ImGui::CloseCurrentPopup();
                }
            }

            if (!entity.HasComponent<FX::CameraComponent>())
            {
                if (ImGui::MenuItem("Camera"))
                {
                    entity.AddComponent<FX::CameraComponent>();
                    ImGui::CloseCurrentPopup();
                }
            }

            if (!entity.HasComponent<FX::FollowComponent>())
            {
                if (ImGui::MenuItem("Follow"))
                {
                    entity.AddComponent<FX::FollowComponent>();

                    // FollowSystem uc component istiyor; Velocity yoksa
                    // takip sessizce calismaz. Kullaniciyi bu tuzaga
                    // dusurmek yerine eksigi kendimiz tamamliyoruz.
                    if (!entity.HasComponent<FX::VelocityComponent>())
                        entity.AddComponent<FX::VelocityComponent>();

                    ImGui::CloseCurrentPopup();
                }
            }

            // Hicbir sey eklenemiyorsa kullaniciya sebebini soyle.
            if (entity.HasComponent<FX::SpriteRendererComponent>() &&
                entity.HasComponent<FX::VelocityComponent>() &&
                entity.HasComponent<FX::CameraComponent>() &&
                entity.HasComponent<FX::FollowComponent>())
            {
                ImGui::TextDisabled("Eklenebilecek component yok");
            }

            ImGui::EndPopup();
        }
    }
}
