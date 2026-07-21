#include "SceneHierarchyPanel.h"
#include "Panels/ContentBrowserPanel.h"
#include "Scripts/SpinScript.h"

#include <FXEngine/Scene/Components.h>
#include <FXEngine/Asset/AssetManager.h>
#include <FXEngine/Core/Log.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
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
        if (m_Selection)
            m_Selection->Clear();
    }

    FX::Entity SceneHierarchyPanel::TakePrefabRequest()
    {
        FX::Entity request = m_PrefabRequest;
        m_PrefabRequest = {};
        return request;
    }

    void SceneHierarchyPanel::OnImGuiRender()
    {
        if (!m_Scene || !m_Selection)
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
        m_ToDelete.clear();
        m_VisibleOrder.clear();
        m_ClickTarget    = {};
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
        //
        // IsAnyItemHovered SART: IsWindowHovered bir satirin uzerindeyken
        // de true doner, yani bu kontrol satira yapilan tiklamada da
        // calisirdi. IsMouseDown yerine IsMouseClicked de sart - basili
        // tutulan fare her karede secimi silerdi ve tiklama isteginin
        // islendigi ILK kareden sonrakiler secimi geri aliyordu.
        //
        // Ctrl basiliysa hic dokunmuyoruz: coklu secim yaparken isabet
        // ettiremeyen bir tik butun secimi goturmemeli.
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered() &&
            !ImGui::IsAnyItemHovered() && !ImGui::GetIO().KeyCtrl)
        {
            m_Selection->Clear();
            m_RangeAnchor = {};
        }

        // Bos alana sag tik -> yeni entity olustur.
        if (ImGui::BeginPopupContextWindow("HierarchyContext",
                                           ImGuiPopupFlags_MouseButtonRight |
                                           ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::MenuItem("Bos Entity Olustur"))
                m_Selection->Select(m_Scene->CreateEntity("Yeni Entity"));
            ImGui::EndPopup();
        }

        ImGui::End();

        // Tiklama agac tamamen cizildikten sonra isleniyor: Shift araligi
        // gorunen siraya, o da bu dongunun bitmesine bagli.
        ApplyPendingClick();

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

        for (FX::Entity toDelete : m_ToDelete)
        {
            // Coklu silmede listedeki bir entity, daha once silinen
            // baska birinin cocugu olabilir - o zaten yok oldu.
            // Entity::operator bool registry'ye bakmiyor, bu yuzden
            // gecerliligi burada sormak zorundayiz.
            if (!toDelete || !m_Scene->GetRegistry().valid(toDelete.GetHandle()))
                continue;

            // Secili entity silinenin ALTINDA olabilir; o da yok olacak.
            // Silmeden ONCE ayikliyoruz: sonrasinda IsAncestorOf'a
            // gecersiz entity sormus olurduk.
            const std::vector<FX::Entity> selection = m_Selection->GetAll();
            for (FX::Entity selected : selection)
            {
                if (selected == toDelete || toDelete.IsAncestorOf(selected))
                    m_Selection->Remove(selected);
            }
            m_Scene->DestroyEntity(toDelete);
        }

        // ===================================================================
        // INSPECTOR PANELI
        // ===================================================================
        ImGui::Begin("Inspector");

        if (FX::Entity primary = m_Selection->GetPrimary())
        {
            // Coklu secimde SADECE birincil duzenleniyor. Cok-hedefli
            // alan duzenleme (Unity'nin "-" gosteren alanlari) ayri bir
            // is; yaniltici olmamasi icin durumu acikca yaziyoruz.
            if (m_Selection->Count() > 1)
            {
                ImGui::TextDisabled("%d entity secili - duzenlenen: birincil",
                                    static_cast<int>(m_Selection->Count()));
                ImGui::Separator();
            }
            DrawComponents(primary);
        }
        else
        {
            ImGui::TextDisabled("Hierarchy'den bir entity sec.");
        }

        ImGui::End();
    }

    void SceneHierarchyPanel::ApplyPendingClick()
    {
        if (!m_ClickTarget)
            return;

        FX::Entity target = m_ClickTarget;
        m_ClickTarget = {};

        if (m_ClickShift && m_RangeAnchor)
        {
            const auto begin = m_VisibleOrder.begin();
            const auto end   = m_VisibleOrder.end();

            const auto from = std::find(begin, end, m_RangeAnchor);
            const auto to   = std::find(begin, end, target);

            // Cipa artik gorunmuyorsa (dali kapanmis olabilir) aralik
            // hesaplanamaz; tek secime dusuyoruz.
            if (from == end || to == end)
            {
                m_Selection->Select(target);
                m_RangeAnchor = target;
                return;
            }

            m_Selection->Clear();
            for (auto it = std::min(from, to); it <= std::max(from, to); ++it)
                m_Selection->Add(*it);

            // Cipa DEGISMIYOR: Shift ile arka arkaya tiklayarak araligi
            // buyutup kucultmek ancak boyle mumkun.
            return;
        }

        if (m_ClickCtrl)
            m_Selection->Toggle(target);
        else
            m_Selection->Select(target);

        m_RangeAnchor = target;
    }

    void SceneHierarchyPanel::DrawEntityNode(FX::Entity entity)
    {
        m_VisibleOrder.push_back(entity);

        const auto& tag = entity.GetComponent<FX::TagComponent>().Tag;
        const bool hasChildren = entity.HasChildren();

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth |
                                   ImGuiTreeNodeFlags_OpenOnArrow;

        if (!hasChildren)
            flags |= ImGuiTreeNodeFlags_Leaf;
        if (m_Selection->IsSelected(entity))
            flags |= ImGuiTreeNodeFlags_Selected;

        // ImGui bir "ID yigini" tutar. Ayni isimde iki entity varsa
        // ikisi de ayni ID'yi alir ve birine tiklayinca digeri secilir.
        const auto handle = entity.GetHandle();
        const auto rawID  = static_cast<std::uint32_t>(handle);
        const bool opened = ImGui::TreeNodeEx(
            reinterpret_cast<void*>(static_cast<std::uintptr_t>(rawID)),
            flags, "%s", tag.c_str());

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            m_ClickTarget = entity;
            m_ClickCtrl   = ImGui::GetIO().KeyCtrl;
            m_ClickShift  = ImGui::GetIO().KeyShift;
        }

        // Sag tik secimin DISINDA bir ogeye yapildiysa secim ona gecer:
        // menudeki islem gorunmeyen bir secime uygulanmasin.
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right) &&
            !m_Selection->IsSelected(entity))
        {
            m_Selection->Select(entity);
            m_RangeAnchor = entity;
        }

        if (ImGui::BeginPopupContextItem())
        {
            const std::size_t selCount = m_Selection->Count();
            const bool        multi    = selCount > 1 && m_Selection->IsSelected(entity);

            if (multi)
                ImGui::TextDisabled("%d entity secili", static_cast<int>(selCount));

            if (ImGui::MenuItem("Alt Entity Ekle"))
            {
                FX::Entity child = m_Scene->CreateEntity("Yeni Entity");
                m_ReparentChild  = child;
                m_ReparentTarget = entity;
                m_Selection->Select(child);
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
            if (ImGui::MenuItem(multi ? "Secilenleri Sil" : "Entity'yi Sil"))
            {
                if (multi)
                    m_ToDelete = m_Selection->GetAll();
                else
                    m_ToDelete.push_back(entity);
            }
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

        DrawTextureSettings(sc);
    }

    void SceneHierarchyPanel::DrawTextureSettings(FX::SpriteRendererComponent& sc)
    {
        if (!sc.Texture || !m_Library)
            return;

        const std::string path = sc.Texture->GetPath();

        const FX::AssetHandle handle = FX::AssetManager::GetHandle(path);
        if (!handle.IsValid())
        {
            // Proje disindan gelen (motor varligi) veya taranmamis bir
            // doku olabilir; ayari yazacak bir .meta yok.
            ImGui::TextDisabled("Ayarlar: bu doku varlik tablosunda degil");
            return;
        }

        if (!ImGui::TreeNode("Doku Ayarlari"))
            return;

        FX::TextureImportSettings settings =
            FX::AssetManager::GetMetadata(handle).TextureSettings;

        bool changed = false;
        changed |= ImGui::Checkbox("Nearest (pixel-art)", &settings.Nearest);
        changed |= ImGui::Checkbox("Repeat (tekrarla)",   &settings.Repeat);
        changed |= ImGui::Checkbox("Mipmap uret",         &settings.GenerateMipmaps);

        ImGui::TextDisabled("Ayar dokunun .meta dosyasinda saklanir,");
        ImGui::TextDisabled("bu sprite'a degil DOSYAYA aittir.");

        if (changed)
        {
            FX::AssetManager::UpdateTextureSettings(handle, settings);
            ReplaceTextureInScene(path, m_Library->Reload(path));
        }

        ImGui::TreePop();
    }

    void SceneHierarchyPanel::ReplaceTextureInScene(
        const std::string& path, const std::shared_ptr<FX::Texture2D>& fresh)
    {
        if (!m_Scene || !fresh)
            return;

        auto view = m_Scene->GetRegistry().view<FX::SpriteRendererComponent>();
        for (auto entityID : view)
        {
            auto& sprite = view.get<FX::SpriteRendererComponent>(entityID);
            if (sprite.Texture && sprite.Texture->GetPath() == path)
                sprite.Texture = fresh;
        }
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

        if (entity.HasComponent<FX::NativeScriptComponent>())
        {
            bool keep = true;
            if (ImGui::CollapsingHeader("Native Script", &keep,
                                        ImGuiTreeNodeFlags_DefaultOpen))
            {
                const auto& nsc = entity.GetComponent<FX::NativeScriptComponent>();

                ImGui::Text("Script");
                ImGui::SameLine(110.0f);
                ImGui::TextUnformatted(nsc.ScriptName.empty() ? "(bagli degil)"
                                                              : nsc.ScriptName.c_str());

                // Ornek yalnizca Play'de var; bunu gostermek "script neden
                // calismiyor?" sorusunu bastan cevapliyor.
                ImGui::TextDisabled(nsc.Instance ? "Durum: calisiyor"
                                                 : "Durum: Play'de baslayacak");
            }

            if (!keep)
                entity.RemoveComponent<FX::NativeScriptComponent>();
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

            // GECICI (16a): script secimi elle listeleniyor. 16b'de bir
            // script kayit defteri (factory) gelince bu liste kendini
            // dolduracak ve secim serilestirilebilecek.
            if (!entity.HasComponent<FX::NativeScriptComponent>())
            {
                if (ImGui::BeginMenu("Native Script"))
                {
                    if (ImGui::MenuItem("Spin (kendi etrafinda doner)"))
                    {
                        entity.AddComponent<FX::NativeScriptComponent>()
                              .Bind<SpinScript>("Spin");
                        ImGui::CloseCurrentPopup();
                    }
                    if (ImGui::MenuItem("Move (ok tuslari)"))
                    {
                        entity.AddComponent<FX::NativeScriptComponent>()
                              .Bind<MoveScript>("Move");
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndMenu();
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
