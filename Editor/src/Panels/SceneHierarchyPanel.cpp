#include "SceneHierarchyPanel.h"
#include "Commands/StructuralCommands.h"
#include "Panels/ComponentDrawer.h"

#include <FXEngine/Asset/AssetManager.h>
#include <FXEngine/Core/Log.h>
#include <FXEngine/Scene/ComponentMeta.h>
#include <FXEngine/Scene/Components.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <functional>
#include <string>

namespace FXEd
{
    namespace
    {
        // Bir prefab ornegi entity'sinden AYNI prefab'a ait en dista
        // atayi bulur. Bir cocuk seciliyken de islem tum ornege uygulansin
        // diye: koku koparip cocuklari bagli birakmak tutarsiz olurdu.
        // Ic ice prefab'ta (farkli handle) siniri asmiyor - C-5 konusu.
        FX::Entity InstanceRoot(FX::Entity e)
        {
            if (!e || !e.HasComponent<FX::PrefabInstanceComponent>())
                return e;

            const FX::AssetHandle handle =
                e.GetComponent<FX::PrefabInstanceComponent>().Prefab;

            FX::Entity root = e;
            while (FX::Entity parent = root.GetParent())
            {
                if (!parent.HasComponent<FX::PrefabInstanceComponent>())
                    break;
                if (parent.GetComponent<FX::PrefabInstanceComponent>().Prefab != handle)
                    break;
                root = parent;
            }
            return root;
        }
    }

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
        m_ToDuplicate.clear();
        m_ReorderEntity = {};
        m_ReorderDir    = 0;
        m_DropMoved     = {};
        m_DropTarget    = {};
        m_DropMode      = 0;
        m_VisibleOrder.clear();
        m_ClickTarget    = {};
        m_ReparentChild  = {};
        m_ReparentTarget = {};
        m_ReparentToRoot = false;

        // Sadece KOKLERI geziyoruz, KOK SIRASINDA (Scene tutuyor); cocuklar
        // DrawEntityNode icinde ozyineleme ile ciziliyor. Eskiden entt
        // view sirasindaydi ve kullanici kokleri siralayamiyordu.
        (void)registry;
        for (FX::Entity entity : m_Scene->GetRootEntities())
            DrawEntityNode(entity);

        // Bos alana birakma: koke tasi.
        if (ImGui::BeginDragDropTargetCustom(ImGui::GetCurrentWindow()->Rect(), ImGui::GetID("HierarchyRoot")))
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FX_ENTITY"))
            {
                const auto handle = *static_cast<const entt::entity*>(payload->Data);
                // Hedef gecersiz = kok listesinin sonuna.
                m_DropMoved  = FX::Entity{ handle, m_Scene };
                m_DropTarget = {};
                m_DropMode   = 2;
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
                Structural::CreateEntity("Yeni Entity");
            ImGui::EndPopup();
        }

        ImGui::End();

        // Tiklama agac tamamen cizildikten sonra isleniyor: Shift araligi
        // gorunen siraya, o da bu dongunun bitmesine bagli.
        ApplyPendingClick();

        // Yapiyi degistiren islemler dongu DISINDA: agac uzerinde
        // gezerken parent/child listelerini degistirmek yineleyicileri
        // gecersiz kilardi.
        if (m_CreateChildOf)
        {
            Structural::CreateEntity("Yeni Entity", m_CreateChildOf);
            m_CreateChildOf = {};
        }

        if (m_ReorderEntity && m_ReorderDir != 0)
        {
            Structural::MoveInParent(m_ReorderEntity, m_ReorderDir);
            m_ReorderEntity = {};
            m_ReorderDir    = 0;
        }

        if (m_DropMoved && m_DropMode != 0)
            ApplyDropRequest();

        if (!m_ToDuplicate.empty())
        {
            // Yalnizca kokleri cogalt: secimde hem parent hem cocugu varsa
            // cocuk zaten parent'in alt agacinda kopyalanir.
            std::vector<FX::Entity> roots;
            for (FX::Entity e : m_ToDuplicate)
            {
                bool ancestorSelected = false;
                for (FX::Entity other : m_ToDuplicate)
                    if (other != e && other.IsAncestorOf(e)) { ancestorSelected = true; break; }
                if (!ancestorSelected)
                    roots.push_back(e);
            }
            Structural::DuplicateEntities(roots, "Cogalt");
        }

        if (m_ReparentChild)
        {
            if (m_ReparentToRoot)
                m_ReparentChild.SetParent({});
            else if (m_ReparentTarget)
                m_ReparentChild.SetParent(m_ReparentTarget);
        }

        if (!m_ToDelete.empty())
        {
            // Secili entity silinenin ALTINDA olabilir; o da yok olacak.
            // Silmeden ONCE ayikliyoruz: sonrasinda IsAncestorOf'a
            // gecersiz entity sormus olurduk.
            for (FX::Entity toDelete : m_ToDelete)
            {
                if (!toDelete || !m_Scene->GetRegistry().valid(toDelete.GetHandle()))
                    continue;

                const std::vector<FX::Entity> selection = m_Selection->GetAll();
                for (FX::Entity selected : selection)
                    if (selected == toDelete || toDelete.IsAncestorOf(selected))
                        m_Selection->Remove(selected);
            }

            Structural::DestroyEntities(m_ToDelete);
        }

        // ===================================================================
        // INSPECTOR PANELI
        // ===================================================================
        ImGui::Begin("Inspector");

        if (FX::Entity primary = m_Selection->GetPrimary())
        {
            // Coklu secim: birincilde degistirilen bir alan, ayni
            // component'e sahip diger secili entity'lere de uygulaniyor
            // (yalnizca degisen alan; her entity'nin kendi konumu korunur).
            if (m_Selection->Count() > 1)
            {
                ImGui::TextDisabled("%d entity secili - degisiklikler hepsine uygulanir",
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

            // Olusturma da dongu DISINDA: burasi registry view'i uzerinde
            // gezilirken calisiyor ve yeni entity + parent bagi
            // yineleyicileri bozardi.
            if (ImGui::MenuItem("Alt Entity Ekle"))
                m_CreateChildOf = entity;
            if (ImGui::MenuItem("Koke Tasi", nullptr, false, static_cast<bool>(entity.GetParent())))
            {
                m_ReparentChild  = entity;
                m_ReparentToRoot = true;
            }
            if (ImGui::MenuItem("Cogalt"))
            {
                if (multi) m_ToDuplicate = m_Selection->GetAll();
                else       m_ToDuplicate.push_back(entity);
            }

            // Siralama hem cocuklarda (RelationshipComponent.Children) hem
            // de koklerde (Scene kok listesi) calisiyor.
            if (ImGui::MenuItem("Yukari Tasi"))  { m_ReorderEntity = entity; m_ReorderDir = -1; }
            if (ImGui::MenuItem("Asagi Tasi"))   { m_ReorderEntity = entity; m_ReorderDir = +1; }
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

        // Birak: fare satirin UST ucundaysa hedefin ONUNE, ALT ucundaysa
        // ARKASINA (kardes olarak), ORTASINDAYSA UZERINE (cocuk) - Unity'nin
        // hiyerarsi davranisi. Ince bir cizgi/cerceve nereye dusecegini
        // gosteriyor.
        if (ImGui::BeginDragDropTarget())
        {
            const ImVec2 rmin = ImGui::GetItemRectMin();
            const ImVec2 rmax = ImGui::GetItemRectMax();
            const float  h    = rmax.y - rmin.y;
            const float  my   = ImGui::GetMousePos().y;

            int mode;
            if      (my < rmin.y + h * 0.30f) mode = 1;   // one
            else if (my > rmax.y - h * 0.30f) mode = 3;   // arkasina
            else                              mode = 2;   // uzerine

            ImDrawList* dl = ImGui::GetForegroundDrawList();
            const ImU32 col = IM_COL32(255, 200, 60, 255);
            if      (mode == 1) dl->AddLine({ rmin.x, rmin.y }, { rmax.x, rmin.y }, col, 2.0f);
            else if (mode == 3) dl->AddLine({ rmin.x, rmax.y }, { rmax.x, rmax.y }, col, 2.0f);
            else                dl->AddRect(rmin, rmax, col, 0.0f, 0, 2.0f);

            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FX_ENTITY"))
            {
                const auto dropped = *static_cast<const entt::entity*>(payload->Data);
                m_DropMoved  = FX::Entity{ dropped, m_Scene };
                m_DropTarget = entity;
                m_DropMode   = mode;
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

    void SceneHierarchyPanel::ApplyDropRequest()
    {
        FX::Entity moved  = m_DropMoved;
        FX::Entity target = m_DropTarget;
        const int  mode   = m_DropMode;

        m_DropMoved  = {};
        m_DropTarget = {};
        m_DropMode   = 0;

        if (!moved)
            return;

        // Kendine ya da kendi alt agacina birakma anlamsiz / dongusel.
        if (moved == target || (target && moved.IsAncestorOf(target)))
            return;

        FX::Entity newParent;
        int        index;

        if (!target)
        {
            // Bos alan: kok listesinin sonuna.
            newParent = {};
            index     = static_cast<int>(m_Scene->GetRootEntities().size());
        }
        else if (mode == 2)
        {
            // Uzerine: hedefin cocugu, sona.
            newParent = target;
            index     = static_cast<int>(target.GetChildren().size());
        }
        else
        {
            // One / arkasina: hedefin kardesi. Index, moved CIKARILMIS
            // listeye gore (PlaceEntity once detach ediyor).
            newParent = target.GetParent();
            std::vector<FX::Entity> siblings =
                newParent ? newParent.GetChildren() : m_Scene->GetRootEntities();

            int pos = static_cast<int>(siblings.size());
            int i = 0;
            for (FX::Entity s : siblings)
            {
                if (s == moved)
                    continue;   // detach sonrasi listede olmayacak
                if (s == target) { pos = i; break; }
                ++i;
            }
            index = (mode == 1) ? pos : pos + 1;
        }

        Structural::ReorderTo(moved, newParent, index);
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

        // --- Prefab bagi (C-1) -------------------------------------------------
        // Kimlik bloguna ait: bu bir duzenlenebilir component degil,
        // entity'nin NEREDEN geldigi bilgisi. Component tablosunda
        // NotInInspector, o yuzden genel dongude cizilmiyor; burada elle.
        if (entity.HasComponent<FX::PrefabInstanceComponent>())
        {
            const auto& link = entity.GetComponent<FX::PrefabInstanceComponent>();
            const std::string path = FX::AssetManager::GetPath(link.Prefab);

            if (path.empty())
            {
                // GUID var ama karsiligi yok: prefab silinmis veya proje
                // disindan gelmis. Sessizce gizlemek yerine gorunur uyari.
                ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.35f, 1.0f),
                                   "Prefab: <kayip kaynak>");
            }
            else
            {
                const std::string name =
                    std::filesystem::path(path).stem().string();
                ImGui::TextColored(ImVec4(0.55f, 0.75f, 1.0f, 1.0f),
                                   "Prefab: %s", name.c_str());
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", path.c_str());
            }

            // Revert yalnizca kaynak varken anlamli (kayip kaynaga
            // donduremeyiz). Bagi Kir her durumda mumkun.
            if (!path.empty())
            {
                if (ImGui::SmallButton("Revert"))
                    m_RevertPrefab = entity;
                ImGui::SameLine();
            }
            if (ImGui::SmallButton("Bagi Kir"))
                m_UnlinkPrefab = entity;
        }

        ImGui::Separator();

        // Coklu secim: birincil DISINDAKI secili entity'ler. Bir alan
        // degisince ComponentDrawer bunlara da yaziyor. Tek secimde bos.
        std::vector<FX::Entity> alsoApplyTo;
        if (m_Selection && m_Selection->Count() > 1)
        {
            for (const FX::Entity sel : m_Selection->GetAll())
                if (sel != entity)
                    alsoApplyTo.push_back(sel);
        }

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

                ComponentDrawer::DrawComponentBody(info, entity, alsoApplyTo);
            }

            // Yapiyi degistiren islem DONGU ICINDE guvenli degil:
            // istek biriktirilip dongu bittikten sonra isleniyor.
            if (!keep)
                m_ComponentToRemove = info.Name;
        }

        if (!m_ComponentToRemove.empty())
        {
            if (FX::ComponentInfo* info = FX::ComponentRegistry::Find(m_ComponentToRemove))
            {
                // Coklu secimde silme de hepsine uygulanir (ekleme gibi).
                std::vector<FX::Entity> targets{ entity };
                targets.insert(targets.end(), alsoApplyTo.begin(), alsoApplyTo.end());
                Structural::RemoveComponent(*info, targets);
            }

            m_ComponentToRemove.clear();
        }

        // "Bagi Kir": ornegi kaynagindan kopar. Tek entity degil TUM alt
        // agac - koku koparip cocuklari bagli birakmak tutarsiz olurdu.
        // Once ornegin KOKUne cikiyoruz: bir cocuk seciliyken de tum ornek
        // koparilir (Unity davranisi).
        if (m_UnlinkPrefab)
        {
            if (FX::ComponentInfo* info = FX::ComponentRegistry::Find("PrefabInstance"))
            {
                std::vector<FX::Entity> targets;
                std::function<void(FX::Entity)> gather = [&](FX::Entity e)
                {
                    if (e.HasComponent<FX::PrefabInstanceComponent>())
                        targets.push_back(e);
                    for (FX::Entity child : e.GetChildren())
                        gather(child);
                };
                gather(InstanceRoot(m_UnlinkPrefab));

                if (!targets.empty())
                    Structural::RemoveComponent(*info, targets);
            }
            m_UnlinkPrefab = {};
        }

        // "Revert": ornegi kaynagina gore geri al. Kokten calisir.
        if (m_RevertPrefab)
        {
            Structural::RevertPrefabInstance(InstanceRoot(m_RevertPrefab));
            m_RevertPrefab = {};
        }

        ImGui::Separator();
        DrawAddComponentMenu(entity);
    }

    void SceneHierarchyPanel::DrawAddComponentMenu(FX::Entity entity)
    {
        // Coklu secimde component TUM secili entity'lere ekleniyor.
        // Hedef listesi: cok secim varsa hepsi, yoksa yalniz birincil.
        std::vector<FX::Entity> targets;
        if (m_Selection && m_Selection->Count() > 1)
            targets = m_Selection->GetAll();
        else
            targets.push_back(entity);

        // Butonu ortala (kozmetik ama panel derli toplu gorunur).
        const float width = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + width * 0.15f);

        if (ImGui::Button("Component Ekle", ImVec2(width * 0.7f, 0.0f)))
            ImGui::OpenPopup("AddComponent");

        if (ImGui::BeginPopup("AddComponent"))
        {
            int shown = 0;

            for (const FX::ComponentInfo& info : FX::ComponentRegistry::GetAll())
            {
                if (!info.AddableFromMenu)
                    continue;

                // En az bir hedefte YOKSA goster: o hedeflere eklenebilir.
                // Hepsinde varsa gizle - gecersiz eylemi sunmayalim.
                bool anyMissing = false;
                for (FX::Entity t : targets)
                    if (!info.Has(t)) { anyMissing = true; break; }

                if (!anyMissing)
                    continue;

                ++shown;

                if (ImGui::MenuItem(info.Label))
                {
                    // Zaten sahip olana eklemeyi (AddComponent assert eder)
                    // komut kendi eliyor.
                    Structural::AddComponent(info, targets);
                    ImGui::CloseCurrentPopup();
                }
            }

            if (shown == 0)
                ImGui::TextDisabled("Eklenebilecek component yok");

            ImGui::EndPopup();
        }
    }
}
