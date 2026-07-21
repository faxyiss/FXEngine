#include "SceneHierarchyPanel.h"
#include "Panels/ComponentDrawer.h"

#include <FXEngine/Core/Log.h>
#include <FXEngine/Scene/ComponentMeta.h>
#include <FXEngine/Scene/Components.h>

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

        // --- Component'ler: tamami meta tablosundan (A1) --------------------
        // Eskiden burada component basina elle yazilmis bir blok vardi ve
        // yeni bir component eklerken burayi unutmak onu GORUNMEZ yapiyordu.
        for (const FX::ComponentInfo& info : FX::ComponentRegistry::GetAll())
        {
            if (!info.ShowInInspector || !info.Has(entity))
                continue;

            bool keep = true;

            // Kaldirilamayan component'lerde 'X' dugmesi hic cizilmiyor:
            // arayuz gecersiz eylemi mumkun kilmamali.
            const bool open = info.Removable
                ? ImGui::CollapsingHeader(info.Label, &keep, ImGuiTreeNodeFlags_DefaultOpen)
                : ImGui::CollapsingHeader(info.Label, ImGuiTreeNodeFlags_DefaultOpen);

            if (open)
            {
                if (info.Name == std::string("Transform") && entity.GetParent())
                    ImGui::TextDisabled("(parent'a gore yerel)");

                ComponentDrawer::DrawComponentBody(info, entity);
            }

            // Yapiyi degistiren islem DONGU ICINDE guvenli degil:
            // istek biriktirilip dongu bittikten sonra isleniyor.
            if (!keep)
                m_ComponentToRemove = info.Name;
        }

        if (!m_ComponentToRemove.empty())
        {
            if (FX::ComponentInfo* info = FX::ComponentRegistry::Find(m_ComponentToRemove))
                info->Remove(entity);

            m_ComponentToRemove.clear();
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
            // ZATEN VAR OLANLARI GOSTERMIYORUZ. Gosterseydik kullanici
            // tiklar, AddComponent'teki assert tetiklenirdi. Arayuz
            // gecersiz eylemi mumkun kilmamali.
            int shown = 0;

            for (const FX::ComponentInfo& info : FX::ComponentRegistry::GetAll())
            {
                if (!info.AddableFromMenu || info.Has(entity))
                    continue;

                ++shown;

                if (ImGui::MenuItem(info.Label))
                {
                    info.Add(entity);

                    // Bagimliliklari olan component'ler eksigini kendisi
                    // tamamliyor (Follow -> Velocity).
                    if (info.OnAdded)
                        info.OnAdded(entity);

                    ImGui::CloseCurrentPopup();
                }
            }

            if (shown == 0)
                ImGui::TextDisabled("Eklenebilecek component yok");

            ImGui::EndPopup();
        }
    }
}
