#include "SceneHierarchyPanel.h"

#include <FXEngine/Scene/Components.h>
#include <FXEngine/Core/Log.h>

#include <imgui.h>

#include <glm/gtc/type_ptr.hpp>

#include <cstring>

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

    void SceneHierarchyPanel::SetAvailableTextures(
        const std::shared_ptr<FX::Texture2D>& checker,
        const std::shared_ptr<FX::Texture2D>& circle)
    {
        m_CheckerTexture = checker;
        m_CircleTexture  = circle;
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
        FX::Entity toDelete{};

        auto view = registry.view<FX::TagComponent>();
        for (auto entityID : view)
        {
            FX::Entity entity{ entityID, m_Scene };
            DrawEntityNode(entity);

            // Sag tik menusu, satirin hemen ustunde acilir.
            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Entity'yi Sil"))
                    toDelete = entity;
                ImGui::EndPopup();
            }
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

        // Silme islemi dongu disinda, guvenli noktada.
        if (toDelete)
        {
            if (m_Selection == toDelete)
                m_Selection = {};
            m_Scene->DestroyEntity(toDelete);
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

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth |
                                   ImGuiTreeNodeFlags_Leaf |          // alt entity yok (henuz)
                                   ImGuiTreeNodeFlags_NoTreePushOnOpen;

        if (m_Selection == entity)
            flags |= ImGuiTreeNodeFlags_Selected;

        // ImGui bir "ID yigini" tutar. Ayni isimde iki entity varsa
        // ikisi de ayni ID'yi alir ve birine tiklayinca digeri secilir.
        // Entity kimligini ID olarak vererek bunu onluyoruz.
        // (void* cast'i ImGui'nin ID API'sinin bir tuhafligi.)
        const auto id = static_cast<std::uint32_t>(entity.GetHandle());
        ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<std::uintptr_t>(id)),
                          flags, "%s", tag.c_str());

        if (ImGui::IsItemClicked())
            m_Selection = entity;
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

    void SceneHierarchyPanel::DrawComponents(FX::Entity entity)
    {
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

        ImGui::Separator();

        // --- Transform ---------------------------------------------------------
        if (entity.HasComponent<FX::TransformComponent>())
        {
            if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
            {
                auto& tc = entity.GetComponent<FX::TransformComponent>();

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

                // Texture secimi. Gercek bir editorde burada bir varlik
                // tarayicisi olurdu; MVP icin elimizdeki iki dokuyu
                // secmek yeterli.
                ImGui::Text("Texture");
                ImGui::SameLine(110.0f);

                const char* current = "Yok (duz renk)";
                if (sc.Texture == m_CheckerTexture) current = "Dama tahtasi";
                else if (sc.Texture == m_CircleTexture) current = "Daire";

                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::BeginCombo("##Tex", current))
                {
                    if (ImGui::Selectable("Yok (duz renk)", sc.Texture == nullptr))
                        sc.Texture = nullptr;
                    if (ImGui::Selectable("Dama tahtasi", sc.Texture == m_CheckerTexture))
                        sc.Texture = m_CheckerTexture;
                    if (ImGui::Selectable("Daire", sc.Texture == m_CircleTexture))
                        sc.Texture = m_CircleTexture;
                    ImGui::EndCombo();
                }
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

            // Hicbir sey eklenemiyorsa kullaniciya sebebini soyle.
            if (entity.HasComponent<FX::SpriteRendererComponent>() &&
                entity.HasComponent<FX::VelocityComponent>())
            {
                ImGui::TextDisabled("Eklenebilecek component yok");
            }

            ImGui::EndPopup();
        }
    }
}
