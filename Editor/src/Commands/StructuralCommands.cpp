#include "Commands/StructuralCommands.h"

#include "Commands/CommandStack.h"
#include "SelectionContext.h"

#include <FXEngine/Asset/AssetManager.h>
#include <FXEngine/Core/FileSystem.h>
#include <FXEngine/Scene/ComponentMeta.h>
#include <FXEngine/Scene/Components.h>
#include <FXEngine/Scene/EntitySnapshot.h>
#include <FXEngine/Scene/PrefabOverrides.h>
#include <FXEngine/Scene/PrefabSerializer.h>
#include <FXEngine/Scene/Scene.h>

#include <fstream>
#include <sstream>

namespace FXEd::Structural
{
    namespace
    {
        Context s_Ctx;

        bool Ready() { return s_Ctx.Scene && s_Ctx.Commands && s_Ctx.Selection; }

        FX::Entity Find(FX::UUID id)
        {
            return s_Ctx.Scene ? s_Ctx.Scene->FindEntityByUUID(id) : FX::Entity{};
        }

        std::vector<FX::UUID> IdsOf(const std::vector<FX::Entity>& entities)
        {
            std::vector<FX::UUID> ids;
            ids.reserve(entities.size());
            for (FX::Entity e : entities)
                if (e)
                    ids.push_back(e.GetUUID());
            return ids;
        }

        // Undo/Redo sonrasi secim, islemin SONUCUNU gostermeli: geri
        // alinan silme secili gelmeli, geri alinan olusturma secimden
        // dusmeli. Aksi halde "geri aldim ama ne oldu?" hissi olur.
        void SelectByIds(const std::vector<FX::UUID>& ids)
        {
            s_Ctx.Selection->Clear();
            for (FX::UUID id : ids)
                s_Ctx.Selection->Add(Find(id));
        }
    }

    void SetContext(const Context& context) { s_Ctx = context; }
    void SetScene(FX::Scene* scene)         { s_Ctx.Scene = scene; }

    FX::Entity CreateEntity(const std::string& name, FX::Entity parent)
    {
        if (!Ready())
            return {};

        FX::Entity entity = s_Ctx.Scene->CreateEntity(name);
        if (parent)
            entity.SetParent(parent);

        s_Ctx.Selection->Select(entity);
        PushCreated(entity, "Entity olustur: " + name);

        return entity;
    }

    void PushCreated(FX::Entity entity, const std::string& label)
    {
        if (!Ready() || !entity)
            return;

        // Redo, entity'yi yeniden YARATMAK yerine snapshot'i geri koyuyor:
        // boylece UUID de ayni kaliyor ve arada ona referans veren baska
        // bir komut (alan duzenlemesi) hedefini bulmaya devam ediyor.
        const FX::EntitySnapshot snap = FX::EntitySnapshot::Capture(entity);
        const FX::UUID id = entity.GetUUID();

        EditCommand cmd;
        cmd.Name = label;
        cmd.Undo = [id]()
        {
            if (FX::Entity e = Find(id))
            {
                s_Ctx.Selection->Remove(e);
                s_Ctx.Scene->DestroyEntity(e);
                s_Ctx.Selection->Prune();
            }
        };
        cmd.Redo = [snap, id]()
        {
            snap.Restore(*s_Ctx.Scene, s_Ctx.Library);
            SelectByIds({ id });
        };
        s_Ctx.Commands->Push(std::move(cmd));
    }

    int DuplicateEntities(const std::vector<FX::Entity>& sources,
                          const std::string& label)
    {
        if (!Ready())
            return 0;

        std::vector<FX::EntitySnapshot> snaps;
        std::vector<FX::UUID>           ids;

        for (FX::Entity src : sources)
        {
            FX::Entity dup = s_Ctx.Scene->DuplicateEntity(src);
            if (!dup)
                continue;

            // Redo, kopyayi yeniden URETMEK yerine snapshot'i geri koyuyor:
            // ayni UUID geri gelsin ki iki redo iki kopya yaratmasin ve
            // arada ona bakan komutlar hedefini bulsun (PushCreated ile ayni
            // gerekce).
            snaps.push_back(FX::EntitySnapshot::Capture(dup));
            ids.push_back(dup.GetUUID());
        }

        if (snaps.empty())
            return 0;

        SelectByIds(ids);

        const int count = static_cast<int>(snaps.size());

        EditCommand cmd;
        cmd.Name = (count > 1) ? (label + " (" + std::to_string(count) + ")") : label;
        cmd.Undo = [ids]()
        {
            for (FX::UUID id : ids)
                if (FX::Entity e = Find(id))
                {
                    s_Ctx.Selection->Remove(e);
                    s_Ctx.Scene->DestroyEntity(e);
                }
            s_Ctx.Selection->Prune();
        };
        cmd.Redo = [snaps, ids]()
        {
            for (const FX::EntitySnapshot& s : snaps)
                s.Restore(*s_Ctx.Scene, s_Ctx.Library);
            SelectByIds(ids);
        };
        s_Ctx.Commands->Push(std::move(cmd));

        return count;
    }

    int DestroyEntities(const std::vector<FX::Entity>& entities)
    {
        if (!Ready())
            return 0;

        std::vector<FX::EntitySnapshot> snaps;
        std::vector<FX::UUID>           ids;

        for (FX::Entity e : entities)
        {
            // Listedeki bir entity daha once silinen baskasinin cocugu
            // olabilir - o zaten yok oldu.
            if (!e || !s_Ctx.Scene->GetRegistry().valid(e.GetHandle()))
                continue;

            snaps.push_back(FX::EntitySnapshot::Capture(e));
            ids.push_back(e.GetUUID());

            s_Ctx.Selection->Remove(e);
            s_Ctx.Scene->DestroyEntity(e);
        }

        if (snaps.empty())
            return 0;

        // Alt agaci silinen bir entity secimde kalmis olabilir.
        s_Ctx.Selection->Prune();

        const int count = static_cast<int>(snaps.size());

        EditCommand cmd;
        cmd.Name = (count > 1) ? (std::to_string(count) + " entity sil")
                               : std::string{ "Entity sil" };
        cmd.Undo = [snaps, ids]()
        {
            for (const FX::EntitySnapshot& s : snaps)
                s.Restore(*s_Ctx.Scene, s_Ctx.Library);
            SelectByIds(ids);
        };
        cmd.Redo = [ids]()
        {
            for (FX::UUID id : ids)
                if (FX::Entity e = Find(id))
                    s_Ctx.Scene->DestroyEntity(e);
            s_Ctx.Selection->Prune();
        };
        s_Ctx.Commands->Push(std::move(cmd));

        return count;
    }

    namespace
    {
        // entity'nin kardes listesindeki (cocuk ya da kok) 0-tabanli
        // konumu. Bulunamazsa listenin sonu.
        int SiblingIndex(FX::Entity entity)
        {
            if (!entity)
                return 0;

            std::vector<FX::Entity> siblings;
            if (FX::Entity parent = entity.GetParent())
                siblings = parent.GetChildren();
            else
                siblings = s_Ctx.Scene->GetRootEntities();

            for (int i = 0; i < static_cast<int>(siblings.size()); ++i)
                if (siblings[i] == entity)
                    return i;
            return static_cast<int>(siblings.size());
        }
    }

    void ReorderTo(FX::Entity moved, FX::Entity newParent, int index)
    {
        if (!Ready() || !moved)
            return;

        // Eski konumu yakala (geri alma icin). Parent'i UUID olarak
        // tutuyoruz: tutamak degil.
        FX::Entity oldParent = moved.GetParent();
        const FX::UUID oldParentID = oldParent ? oldParent.GetUUID() : FX::UUID(0);
        const int      oldIndex    = SiblingIndex(moved);

        const FX::UUID movedID     = moved.GetUUID();
        const FX::UUID newParentID = newParent ? newParent.GetUUID() : FX::UUID(0);

        // Ayni yere birakma: is yok (ve gereksiz undo adimi yaratma).
        if (newParentID == oldParentID && (index == oldIndex || index == oldIndex + 1))
            return;

        s_Ctx.Scene->PlaceEntity(moved, newParent, index);

        EditCommand cmd;
        cmd.Name = "Hiyerarside tasi";
        cmd.Undo = [movedID, oldParentID, oldIndex]()
        {
            FX::Entity m = Find(movedID);
            if (!m) return;
            FX::Entity p = oldParentID.IsValid() ? Find(oldParentID) : FX::Entity{};
            s_Ctx.Scene->PlaceEntity(m, p, oldIndex);
        };
        cmd.Redo = [movedID, newParentID, index]()
        {
            FX::Entity m = Find(movedID);
            if (!m) return;
            FX::Entity p = newParentID.IsValid() ? Find(newParentID) : FX::Entity{};
            s_Ctx.Scene->PlaceEntity(m, p, index);
        };
        s_Ctx.Commands->Push(std::move(cmd));
    }

    void MoveInParent(FX::Entity entity, int direction)
    {
        if (!Ready() || !entity)
            return;

        const FX::UUID id = entity.GetUUID();
        if (!entity.MoveInParent(direction))
            return;   // uctaydi, komut yazma

        // Geri alma ters yon: swap kendi tersidir, bir daha ayni yonun
        // tersine tasimak eski sirayi geri getirir.
        EditCommand cmd;
        cmd.Name = (direction < 0) ? "Yukari tasi" : "Asagi tasi";
        cmd.Undo = [id, direction]()
        {
            if (FX::Entity e = Find(id)) e.MoveInParent(-direction);
        };
        cmd.Redo = [id, direction]()
        {
            if (FX::Entity e = Find(id)) e.MoveInParent(direction);
        };
        s_Ctx.Commands->Push(std::move(cmd));
    }

    void AddComponent(const FX::ComponentInfo& info,
                      const std::vector<FX::Entity>& targets)
    {
        if (!Ready())
            return;

        // Yalnizca EKSIK olanlar: zaten sahip olana eklemek hem assert
        // eder hem de geri alinca kullanicinin kendi component'ini
        // silmis olurduk.
        std::vector<FX::Entity> affected;
        for (FX::Entity t : targets)
            if (t && !info.Has(t))
                affected.push_back(t);

        if (affected.empty())
            return;

        const std::vector<FX::UUID> ids  = IdsOf(affected);
        const std::string           name = info.Name;

        auto apply = [ids, name]()
        {
            FX::ComponentInfo* info = FX::ComponentRegistry::Find(name);
            if (!info)
                return;

            for (FX::UUID id : ids)
            {
                FX::Entity e = Find(id);
                if (!e || info->Has(e))
                    continue;

                info->Add(e);
                // Bagimliliklari olan component'ler eksigini kendisi
                // tamamliyor (Follow -> Velocity).
                if (info->OnAdded)
                    info->OnAdded(e);
            }
        };

        apply();

        EditCommand cmd;
        cmd.Name = std::string{ "Component ekle: " } + info.Label;
        cmd.Undo = [ids, name]()
        {
            FX::ComponentInfo* info = FX::ComponentRegistry::Find(name);
            if (!info)
                return;
            for (FX::UUID id : ids)
                if (FX::Entity e = Find(id))
                    info->Remove(e);
        };
        cmd.Redo = apply;
        s_Ctx.Commands->Push(std::move(cmd));
    }

    void RemoveComponent(const FX::ComponentInfo& info,
                         const std::vector<FX::Entity>& targets)
    {
        if (!Ready() || !info.Removable)
            return;

        // Silmeden ONCE veriyi yakala: geri alma "component'i geri koy"
        // degil "eski haliyle geri koy" olmali. Varsayilanla eklemek
        // sessiz bir veri kaybi olurdu.
        std::vector<FX::UUID>              ids;
        std::vector<FX::ComponentSnapshot> snaps;

        for (FX::Entity t : targets)
        {
            if (!t || !info.Has(t))
                continue;

            snaps.push_back(FX::ComponentSnapshot::Capture(t, info));
            ids.push_back(t.GetUUID());
        }

        if (ids.empty())
            return;

        const std::string name = info.Name;

        auto apply = [ids, name]()
        {
            FX::ComponentInfo* info = FX::ComponentRegistry::Find(name);
            if (!info)
                return;
            for (FX::UUID id : ids)
                if (FX::Entity e = Find(id))
                    info->Remove(e);
        };

        apply();

        EditCommand cmd;
        cmd.Name = std::string{ "Component sil: " } + info.Label;
        cmd.Undo = [ids, snaps]()
        {
            for (std::size_t i = 0; i < ids.size(); ++i)
                if (FX::Entity e = Find(ids[i]))
                    snaps[i].Restore(e, s_Ctx.Library);
        };
        cmd.Redo = apply;
        s_Ctx.Commands->Push(std::move(cmd));
    }

    void RevertPrefabInstance(FX::Entity instanceRoot)
    {
        if (!Ready() || !instanceRoot ||
            !instanceRoot.HasComponent<FX::PrefabInstanceComponent>())
            return;

        // Revert component KUMESINI degistirebiliyor (eklenen kalkar,
        // silinen geri gelir), o yuzden alan-degeri degil TAM snapshot ile
        // geri aliyoruz - silme/cogaltma undo'suyla ayni destroy+restore
        // kalibi. UUID'ler korundugu icin arada ona bakan komutlar hedefini
        // bulmaya devam ediyor.
        const FX::EntitySnapshot before = FX::EntitySnapshot::Capture(instanceRoot);
        const FX::UUID           rootId = instanceRoot.GetUUID();

        FX::PrefabSerializer prefab(s_Ctx.Scene, s_Ctx.Library);
        if (!prefab.RevertInstance(instanceRoot))
            return;   // kaynak kayip veya is olmadi: gereksiz undo adimi yok

        const FX::EntitySnapshot after = FX::EntitySnapshot::Capture(Find(rootId));

        EditCommand cmd;
        cmd.Name = "Prefab'i geri al (Revert)";
        cmd.Undo = [before, rootId]()
        {
            if (FX::Entity e = Find(rootId))
                s_Ctx.Scene->DestroyEntity(e);
            before.Restore(*s_Ctx.Scene, s_Ctx.Library);
            SelectByIds({ rootId });
        };
        cmd.Redo = [after, rootId]()
        {
            if (FX::Entity e = Find(rootId))
                s_Ctx.Scene->DestroyEntity(e);
            after.Restore(*s_Ctx.Scene, s_Ctx.Library);
            SelectByIds({ rootId });
        };
        s_Ctx.Commands->Push(std::move(cmd));

        SelectByIds({ rootId });
    }

    namespace
    {
        std::string ReadAllText(const std::string& path)
        {
            std::ifstream in(path, std::ios::binary);
            if (!in)
                return {};
            std::ostringstream ss;
            ss << in.rdbuf();
            return ss.str();
        }

        void WriteAllText(const std::string& path, const std::string& text)
        {
            std::ofstream out(path, std::ios::binary);
            out << text;
        }
    }

    void ApplyPrefabInstance(FX::Entity instanceRoot)
    {
        if (!Ready() || !instanceRoot ||
            !instanceRoot.HasComponent<FX::PrefabInstanceComponent>())
            return;

        instanceRoot = FX::PrefabOverrides::InstanceRoot(instanceRoot);

        const FX::AssetHandle handle =
            instanceRoot.GetComponent<FX::PrefabInstanceComponent>().Prefab;
        const std::string relPath = FX::AssetManager::GetPath(handle);
        if (relPath.empty())
            return;

        const std::string fullPath = FX::FileSystem::ResolveProjectAsset(relPath);

        // Undo dosyanin ESKI icerigini geri yazacak; simdiden oku.
        const std::string oldBytes = ReadAllText(fullPath);
        if (oldBytes.empty())
            return;

        // Apply diger ornekleri de degistirir; onlarin oncesini yakala.
        // Uygulanan ornegin kendisi Apply'da DEGISMEZ, snapshot gerekmez.
        std::vector<FX::EntitySnapshot> before;
        std::vector<FX::UUID>           ids;
        for (FX::Entity r : FX::PrefabOverrides::InstanceRootsOf(*s_Ctx.Scene, handle))
        {
            if (r == instanceRoot)
                continue;
            before.push_back(FX::EntitySnapshot::Capture(r));
            ids.push_back(r.GetUUID());
        }

        if (!FX::PrefabOverrides::ApplyInstance(instanceRoot, s_Ctx.Library))
            return;

        const std::string newBytes = ReadAllText(fullPath);

        std::vector<FX::EntitySnapshot> after;
        for (FX::UUID id : ids)
            after.push_back(FX::EntitySnapshot::Capture(Find(id)));

        EditCommand cmd;
        cmd.Name = "Prefab'a uygula (Apply)";
        cmd.Undo = [fullPath, oldBytes, before, ids]()
        {
            WriteAllText(fullPath, oldBytes);
            for (std::size_t i = 0; i < ids.size(); ++i)
            {
                if (FX::Entity e = Find(ids[i]))
                    s_Ctx.Scene->DestroyEntity(e);
                before[i].Restore(*s_Ctx.Scene, s_Ctx.Library);
            }
        };
        cmd.Redo = [fullPath, newBytes, after, ids]()
        {
            WriteAllText(fullPath, newBytes);
            for (std::size_t i = 0; i < ids.size(); ++i)
            {
                if (FX::Entity e = Find(ids[i]))
                    s_Ctx.Scene->DestroyEntity(e);
                after[i].Restore(*s_Ctx.Scene, s_Ctx.Library);
            }
        };
        s_Ctx.Commands->Push(std::move(cmd));
    }
}
