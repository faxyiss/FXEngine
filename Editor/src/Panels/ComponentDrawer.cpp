#include "Panels/ComponentDrawer.h"
#include "Panels/ContentBrowserPanel.h"   // kContentPayload
#include "Commands/CommandStack.h"

#include <FXEngine/Asset/AssetManager.h>
#include <FXEngine/Core/Log.h>
#include <FXEngine/Scene/Components.h>
#include <FXEngine/Scene/Scene.h>
#include <FXEngine/Scene/ScriptRegistry.h>

#include <imgui.h>

#include <glm/gtc/type_ptr.hpp>

#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace FXEd::ComponentDrawer
{
    namespace
    {
        constexpr float kLabelWidth = 110.0f;

        void Label(const char* text)
        {
            ImGui::Text("%s", text);
            ImGui::SameLine(kLabelWidth);
            ImGui::SetNextItemWidth(-1.0f);
        }

        void ClampToRange(const FX::FieldInfo& f, float& value)
        {
            if (f.Min == f.Max)
                return;

            if (value < f.Min) value = f.Min;
            if (value > f.Max) value = f.Max;
        }

        // --- Hedef secici (EntityRef) ---------------------------------------
        // Entity'yi ADIYLA gosteriyoruz ama sakladigimiz sey UUID.
        // Arayuz okunabilir olmali, veri modeli kalici - ikisi ayni sey
        // olmak zorunda degil.
        bool DrawEntityRef(const FX::FieldInfo& f, FX::EntityRef& ref, FX::Entity owner)
        {
            FX::Scene* scene = owner.GetScene();
            if (!scene)
                return false;

            bool changed = false;

            FX::Entity target = scene->FindEntityByUUID(ref.Target);

            std::string preview = "Yok";
            if (ref.IsSet())
            {
                // Hedef dolu ama entity bulunamiyor: silinmis olabilir.
                // Sessizce gizlemek yerine acikca gosteriyoruz.
                preview = target
                    ? target.GetName()
                    : std::string("<kayip: ") +
                      std::to_string(static_cast<std::uint64_t>(ref.Target)) + ">";
            }

            Label(f.Label);

            if (!ImGui::BeginCombo("##ref", preview.c_str()))
                return false;

            if (ImGui::Selectable("Yok", !ref.IsSet()))
            {
                ref.Clear();
                changed = true;
            }

            auto view = scene->GetRegistry().view<FX::TagComponent, FX::IDComponent>();
            for (auto id : view)
            {
                FX::Entity candidate{ id, scene };

                // Kendini takip etmek anlamsiz - secenegi hic sunma.
                if (candidate == owner)
                    continue;

                const auto uuid = candidate.GetComponent<FX::IDComponent>().ID;

                // Ayni isimde birden fazla entity olabilir; ImGui'nin ID
                // yiginina UUID'yi itiyoruz ki secimler karismasin.
                ImGui::PushID(static_cast<int>(static_cast<std::uint64_t>(uuid)));
                if (ImGui::Selectable(candidate.GetName().c_str(), uuid == ref.Target))
                {
                    ref.Target = uuid;
                    changed = true;
                }
                ImGui::PopID();
            }

            ImGui::EndCombo();
            return changed;
        }

        // Doner: bu alan bu karede degistiyse true (coklu secim yaymasi icin).
        bool DrawField(const FX::FieldInfo& f, void* component, FX::Entity owner)
        {
            void* p = f.Get(component);

            ImGui::PushID(f.Name);
            bool changed = false;

            switch (f.Type)
            {
            case FX::FieldType::Bool:
                ImGui::Text("%s", f.Label);
                ImGui::SameLine(kLabelWidth);
                changed = ImGui::Checkbox("##b", static_cast<bool*>(p));
                break;

            case FX::FieldType::Int:
                Label(f.Label);
                changed = ImGui::DragInt("##i", static_cast<int*>(p), f.DragSpeed);
                break;

            case FX::FieldType::Float:
            {
                float& value = *static_cast<float*>(p);

                // ARAYUZDE DERECE, VERIDE RADYAN. Cevrim tam burada,
                // sinirda yapiliyor.
                float shown = f.DegreesInUI ? glm::degrees(value) : value;

                Label(f.Label);
                changed = ImGui::DragFloat("##f", &shown, f.DragSpeed);

                value = f.DegreesInUI ? glm::radians(shown) : shown;
                ClampToRange(f, value);
                break;
            }

            case FX::FieldType::Vec2:
                Label(f.Label);
                changed = ImGui::DragFloat2("##v2", glm::value_ptr(*static_cast<glm::vec2*>(p)),
                                            f.DragSpeed);
                break;

            case FX::FieldType::Vec3:
                Label(f.Label);
                changed = ImGui::DragFloat3("##v3", glm::value_ptr(*static_cast<glm::vec3*>(p)),
                                            f.DragSpeed);
                break;

            case FX::FieldType::Vec4:
                Label(f.Label);
                changed = ImGui::DragFloat4("##v4", glm::value_ptr(*static_cast<glm::vec4*>(p)),
                                            f.DragSpeed);
                break;

            case FX::FieldType::Color:
                Label(f.Label);
                changed = ImGui::ColorEdit4("##color", glm::value_ptr(*static_cast<glm::vec4*>(p)));
                break;

            case FX::FieldType::String:
            {
                // ImGui C API'siyle calisir: std::string degil char tamponu ister.
                auto& str = *static_cast<std::string*>(p);

                char buffer[256];
                std::memset(buffer, 0, sizeof(buffer));
                std::strncpy(buffer, str.c_str(), sizeof(buffer) - 1);

                Label(f.Label);
                if (ImGui::InputText("##s", buffer, sizeof(buffer)))
                {
                    str = buffer;
                    changed = true;
                }
                break;
            }

            case FX::FieldType::EntityRef:
                changed = DrawEntityRef(f, *static_cast<FX::EntityRef*>(p), owner);
                break;
            }

            ImGui::PopID();
            return changed;
        }

        // Coklu secimde: birincilde degisen alani ayni tipteki diger
        // entity'lere yazar. Yalnizca DEGISEN alan kopyalanir - digerleri
        // (orn. her entity'nin kendi konumu) korunur.
        void CopyFieldValue(const FX::FieldInfo& f, void* dst, const void* src)
        {
            switch (f.Type)
            {
            case FX::FieldType::Bool:   *static_cast<bool*>(dst)  = *static_cast<const bool*>(src);  break;
            case FX::FieldType::Int:    *static_cast<int*>(dst)   = *static_cast<const int*>(src);   break;
            case FX::FieldType::Float:  *static_cast<float*>(dst) = *static_cast<const float*>(src); break;
            case FX::FieldType::Vec2:   *static_cast<glm::vec2*>(dst) = *static_cast<const glm::vec2*>(src); break;
            case FX::FieldType::Vec3:   *static_cast<glm::vec3*>(dst) = *static_cast<const glm::vec3*>(src); break;
            case FX::FieldType::Vec4:
            case FX::FieldType::Color:  *static_cast<glm::vec4*>(dst) = *static_cast<const glm::vec4*>(src); break;
            case FX::FieldType::String: *static_cast<std::string*>(dst) = *static_cast<const std::string*>(src); break;
            case FX::FieldType::EntityRef: *static_cast<FX::EntityRef*>(dst) = *static_cast<const FX::EntityRef*>(src); break;
            }
        }

        // --- Undo/Redo: alan degeri yakalama --------------------------------
        // Bir alanin degerini tipten bagimsiz tutan kap. Undo, degistirilen
        // alanin ESKI ve YENI degerini saklamak zorunda; tek tip yeterli
        // olmadigi icin hepsini bir arada tutuyoruz (kucuk israf, basit kod).
        struct FieldValue
        {
            FX::FieldType Type{};
            bool          B{};
            int           I{};
            float         F{};
            glm::vec2     V2{};
            glm::vec3     V3{};
            glm::vec4     V4{};
            std::string   S;
            FX::EntityRef Ref;
        };

        FieldValue ReadFieldValue(const FX::FieldInfo& f, void* comp)
        {
            FieldValue v;
            v.Type = f.Type;
            const void* p = f.Get(comp);
            switch (f.Type)
            {
            case FX::FieldType::Bool:   v.B = *static_cast<const bool*>(p); break;
            case FX::FieldType::Int:    v.I = *static_cast<const int*>(p); break;
            case FX::FieldType::Float:  v.F = *static_cast<const float*>(p); break;
            case FX::FieldType::Vec2:   v.V2 = *static_cast<const glm::vec2*>(p); break;
            case FX::FieldType::Vec3:   v.V3 = *static_cast<const glm::vec3*>(p); break;
            case FX::FieldType::Vec4:
            case FX::FieldType::Color:  v.V4 = *static_cast<const glm::vec4*>(p); break;
            case FX::FieldType::String: v.S = *static_cast<const std::string*>(p); break;
            case FX::FieldType::EntityRef: v.Ref = *static_cast<const FX::EntityRef*>(p); break;
            }
            return v;
        }

        bool FieldValuesEqual(const FieldValue& a, const FieldValue& b)
        {
            if (a.Type != b.Type)
                return false;
            switch (a.Type)
            {
            case FX::FieldType::Bool:   return a.B == b.B;
            case FX::FieldType::Int:    return a.I == b.I;
            case FX::FieldType::Float:  return a.F == b.F;
            case FX::FieldType::Vec2:   return a.V2 == b.V2;
            case FX::FieldType::Vec3:   return a.V3 == b.V3;
            case FX::FieldType::Vec4:
            case FX::FieldType::Color:  return a.V4 == b.V4;
            case FX::FieldType::String: return a.S == b.S;
            case FX::FieldType::EntityRef: return a.Ref.Target == b.Ref.Target;
            }
            return true;
        }

        // Entity + component + alan ADIYLA bir degeri yazar. Ad tabanli:
        // komut closure'i ham isaretci degil kimlik tutmali (entity silinmis
        // olabilir; o zaman sessizce atlanir).
        void ApplyFieldByName(FX::Entity e, const std::string& compName,
                              const char* fieldName, const FieldValue& v)
        {
            if (!e)
                return;
            FX::ComponentInfo* info = FX::ComponentRegistry::Find(compName);
            if (!info || !info->Has(e))
                return;
            void* comp = info->GetPtr(e);
            if (!comp)
                return;
            for (const FX::FieldInfo& f : info->Fields)
            {
                if (std::strcmp(f.Name, fieldName) != 0)
                    continue;
                switch (f.Type)
                {
                case FX::FieldType::Bool:   *static_cast<bool*>(f.Get(comp)) = v.B; break;
                case FX::FieldType::Int:    *static_cast<int*>(f.Get(comp)) = v.I; break;
                case FX::FieldType::Float:  *static_cast<float*>(f.Get(comp)) = v.F; break;
                case FX::FieldType::Vec2:   *static_cast<glm::vec2*>(f.Get(comp)) = v.V2; break;
                case FX::FieldType::Vec3:   *static_cast<glm::vec3*>(f.Get(comp)) = v.V3; break;
                case FX::FieldType::Vec4:
                case FX::FieldType::Color:  *static_cast<glm::vec4*>(f.Get(comp)) = v.V4; break;
                case FX::FieldType::String: *static_cast<std::string*>(f.Get(comp)) = v.S; break;
                case FX::FieldType::EntityRef: *static_cast<FX::EntityRef*>(f.Get(comp)) = v.Ref; break;
                }
                return;
            }
        }

        struct FieldEdit
        {
            FX::Entity  Entity;
            FieldValue  Old;
        };

        // Suren surukleme boyunca ESKI degerler burada bekliyor; birakilinca
        // (IsItemDeactivatedAfterEdit) komut olusturuluyor. Ayni anda tek
        // widget aktif oldugu icin tek bir bekleyen edit yeterli.
        struct PendingFieldEdit
        {
            bool                    Active = false;
            std::string             ComponentName;
            const char*             FieldName = nullptr;
            const char*             FieldLabel = nullptr;
            std::vector<FieldEdit>  Targets;   // birincil + coklu secim
        };

        PendingFieldEdit s_Pending;
        CommandStack*    s_Commands = nullptr;   // FXEd::CommandStack

        // Bir alan degisikligini (birincil + hedefler icin eski->yeni) komut
        // olarak yigina yazar. old ve new degerleri closure'a kopyalaniyor.
        void PushFieldCommand(const std::string& compName, const char* fieldName,
                              const char* fieldLabel,
                              const std::vector<FieldEdit>& edits)
        {
            if (!s_Commands || edits.empty())
                return;

            // Yeni degerleri simdi (canli) oku.
            struct Entry { FX::Entity e; FieldValue oldV, newV; };
            std::vector<Entry> entries;
            bool anyChanged = false;

            for (const FieldEdit& fe : edits)
            {
                if (!fe.Entity)
                    continue;
                FX::ComponentInfo* info = FX::ComponentRegistry::Find(compName);
                if (!info || !info->Has(fe.Entity))
                    continue;
                void* comp = info->GetPtr(fe.Entity);
                if (!comp)
                    continue;

                const FX::FieldInfo* field = nullptr;
                for (const FX::FieldInfo& f : info->Fields)
                    if (std::strcmp(f.Name, fieldName) == 0) { field = &f; break; }
                if (!field)
                    continue;

                Entry en{ fe.Entity, fe.Old, ReadFieldValue(*field, comp) };
                if (!FieldValuesEqual(en.oldV, en.newV))
                    anyChanged = true;
                entries.push_back(std::move(en));
            }

            if (!anyChanged)
                return;   // deger gercekten degismedi: gereksiz komut yok

            const std::string comp = compName;
            const std::string fname = fieldName;

            FXEd::EditCommand cmd;
            cmd.Name = std::string("Duzenle: ") + (fieldLabel ? fieldLabel : fieldName);
            cmd.Undo = [comp, fname, entries]() {
                for (const Entry& e : entries)
                    ApplyFieldByName(e.e, comp, fname.c_str(), e.oldV);
            };
            cmd.Redo = [comp, fname, entries]() {
                for (const Entry& e : entries)
                    ApplyFieldByName(e.e, comp, fname.c_str(), e.newV);
            };
            s_Commands->Push(std::move(cmd));
        }

        // --- Ozel arayuzler --------------------------------------------------

        FX::TextureLibrary* s_Library = nullptr;

        // Bir dokunun ayarlari .meta'da yasiyor ve DOSYAYA aittir; degisince
        // o dokuyu kullanan tum sprite'lar yenilenmeli.
        void ReplaceTextureInScene(FX::Scene* scene, const std::string& path,
                                   const std::shared_ptr<FX::Texture2D>& fresh)
        {
            if (!scene || !fresh)
                return;

            auto view = scene->GetRegistry().view<FX::SpriteRendererComponent>();
            for (auto id : view)
            {
                auto& sprite = view.get<FX::SpriteRendererComponent>(id);
                if (sprite.Texture && sprite.Texture->GetPath() == path)
                    sprite.Texture = fresh;
            }
        }

        void DrawTextureSettings(FX::SpriteRendererComponent& sc, FX::Entity owner)
        {
            if (!sc.Texture || !s_Library)
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
                ReplaceTextureInScene(owner.GetScene(), path, s_Library->Reload(path));
            }

            ImGui::TreePop();
        }

        void DrawTextureSlot(void* component, FX::Entity owner)
        {
            auto& sc = *static_cast<FX::SpriteRendererComponent*>(component);

            ImGui::Text("Texture");
            ImGui::SameLine(kLabelWidth);

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

            // BIRAKMA HEDEFI. Icerik panelinden gelen yolu kutuphaneye
            // veriyoruz; ayni yolu ikinci kez gorurse diske gitmiyor.
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload =
                        ImGui::AcceptDragDropPayload(kContentPayload))
                {
                    const char* path = static_cast<const char*>(payload->Data);

                    if (s_Library)
                    {
                        if (auto texture = s_Library->Load(path))
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

            DrawTextureSettings(sc, owner);
        }

        void DrawCameraExtras(void* component, FX::Entity owner)
        {
            auto& cc = *static_cast<FX::CameraComponent*>(component);

            ImGui::Text("Birincil");
            ImGui::SameLine(kLabelWidth);

            if (ImGui::Checkbox("##Primary", &cc.Primary) && cc.Primary)
            {
                // Tek birincil kamera olmali. Aksi halde "hangisi kazanir"
                // sorusunun cevabi registry'deki siraya kalirdi -
                // kullanicinin goremedigi bir sey.
                if (FX::Scene* scene = owner.GetScene())
                {
                    auto view = scene->GetRegistry().view<FX::CameraComponent>();
                    for (auto other : view)
                    {
                        if (other != owner.GetHandle())
                            view.get<FX::CameraComponent>(other).Primary = false;
                    }
                }
            }

            ImGui::TextDisabled("Konum ve donme Transform'dan gelir.");
        }

        void DrawScriptExtras(void* component, FX::Entity owner)
        {
            auto& nsc = *static_cast<FX::NativeScriptComponent*>(component);

            const char* preview = nsc.ScriptName.empty() ? "(secilmedi)"
                                                         : nsc.ScriptName.c_str();

            Label("Script");

            // Liste ScriptRegistry'den geliyor: yeni bir script
            // kaydedildiginde burada kendiliginden gorunur.
            if (ImGui::BeginCombo("##script", preview))
            {
                if (ImGui::Selectable("(secilmedi)", nsc.ScriptName.empty()))
                    nsc.ScriptName.clear();

                for (const std::string& name : FX::ScriptRegistry::GetNames())
                {
                    if (ImGui::Selectable(name.c_str(), nsc.ScriptName == name))
                        nsc.ScriptName = name;
                }
                ImGui::EndCombo();
            }

            // Sahne baska bir derlemeden gelmis olabilir: dosyadaki ad bu
            // derlemede kayitli olmayabilir. Sessiz kalmak "script neden
            // calismiyor?" sorusunu doguruyordu.
            if (!nsc.ScriptName.empty() && !FX::ScriptRegistry::Contains(nsc.ScriptName))
            {
                ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f),
                                   "'%s' bu derlemede kayitli degil!",
                                   nsc.ScriptName.c_str());
            }

            // Ornek yalnizca Play'de var; bunu gostermek "script neden
            // calismiyor?" sorusunu bastan cevapliyor.
            ImGui::TextDisabled(nsc.Instance ? "Durum: calisiyor"
                                             : "Durum: Play'de baslayacak");
            (void)owner;
        }

        void DrawFollowExtras(void*, FX::Entity owner)
        {
            // FollowSystem Velocity'ye YAZIYOR; yoksa entity'yi hic gormez.
            // Component eklenirken OnAdded bunu tamamliyor ama sonradan
            // elle silinmis olabilir.
            if (owner.HasComponent<FX::VelocityComponent>())
                return;

            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f),
                               "Velocity component'i gerekli!");
            if (ImGui::Button("Velocity Ekle"))
                owner.AddComponent<FX::VelocityComponent>();
        }
    }

    void RegisterEditorUI(FX::TextureLibrary* library, CommandStack* commands)
    {
        s_Library  = library;
        s_Commands = commands;

        auto bind = [](const char* name,
                       std::function<void(void*, FX::Entity)> fn)
        {
            if (FX::ComponentInfo* info = FX::ComponentRegistry::Find(name))
                info->DrawExtraUI = std::move(fn);
            else
                FX_WARN("ComponentDrawer: '%s' tabloda yok.", name);
        };

        bind("SpriteRenderer", DrawTextureSlot);
        bind("Camera",         DrawCameraExtras);
        bind("NativeScript",   DrawScriptExtras);
        bind("Follow",         DrawFollowExtras);
    }

    void DrawComponentBody(const FX::ComponentInfo& info, FX::Entity entity,
                           const std::vector<FX::Entity>& alsoApplyTo)
    {
        void* component = info.GetPtr(entity);
        if (!component)
            return;

        for (const FX::FieldInfo& f : info.Fields)
        {
            if (f.HiddenInInspector)
                continue;

            // Undo icin ESKI degerleri widget'tan ONCE yakala (birincil +
            // hedefler). Bu karede henuz degismedikleri icin gercek eski.
            const FieldValue primaryOld = ReadFieldValue(f, component);
            std::vector<FieldEdit> edits;
            edits.push_back({ entity, primaryOld });
            for (FX::Entity other : alsoApplyTo)
                if (info.Has(other))
                    if (void* oc = info.GetPtr(other))
                        edits.push_back({ other, ReadFieldValue(f, oc) });

            const bool changed = DrawField(f, component, entity);

            // Bu alanin widget'i suren surukleme desteklermi? Suruklenebilir
            // olanlar (sayilar, renk, metin, onay kutusu) baslat/birak ile
            // TEK adim; EntityRef combo'su aninda kesinlesir.
            const bool activated   = ImGui::IsItemActivated();
            const bool deactivated = ImGui::IsItemDeactivatedAfterEdit();

            // Coklu secim: bu alan degistiyse ayni component'e sahip diger
            // entity'lere de yaz. Sadece degisen alan - digerleri korunur.
            if (changed && !alsoApplyTo.empty())
            {
                const void* srcField = f.Get(component);
                for (FX::Entity other : alsoApplyTo)
                {
                    if (!info.Has(other))
                        continue;
                    if (void* otherComp = info.GetPtr(other))
                        CopyFieldValue(f, f.Get(otherComp), srcField);
                }
            }

            // --- Undo/Redo yakalama ---
            if (f.Type == FX::FieldType::EntityRef)
            {
                // Combo secimi aninda kesinlesir: degistiyse hemen komut.
                if (changed)
                    PushFieldCommand(info.Name, f.Name, f.Label, edits);
            }
            else
            {
                if (activated)
                {
                    // Surukleme/duzenleme basladi: eski degerleri sakla.
                    s_Pending.Active        = true;
                    s_Pending.ComponentName = info.Name;
                    s_Pending.FieldName     = f.Name;
                    s_Pending.FieldLabel    = f.Label;
                    s_Pending.Targets       = edits;
                }
                if (deactivated && s_Pending.Active &&
                    s_Pending.FieldName == f.Name &&
                    s_Pending.ComponentName == info.Name)
                {
                    // Birakildi: eski (saklanan) -> yeni (canli) komutu.
                    PushFieldCommand(s_Pending.ComponentName, s_Pending.FieldName,
                                     s_Pending.FieldLabel, s_Pending.Targets);
                    s_Pending.Active = false;
                }
            }
        }

        if (info.DrawExtraUI)
            info.DrawExtraUI(component, entity);
    }
}
