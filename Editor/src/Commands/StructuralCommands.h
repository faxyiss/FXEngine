#pragma once

// ===========================================================================
// FXEditor - Yapisal Undo komutlari
//
// A5 yalnizca ALAN duzenlemesini ve gizmo donusumunu geri alabiliyordu;
// entity olusturma/silme ve component ekleme/silme kapsam disindaydi.
// Yani kullanicinin en cok korktugu islem (yanlislikla silmek) geri
// alinamiyordu.
//
// NEDEN AYRI BIR DOSYA? Ayni islem uc yerden tetikleniyor: hierarchy
// paneli sag tik menusu, Inspector'daki 'X', Delete tusu (EditorApp).
// Uc yere ayri ayri komut yazmak, birini unutmanin cezasini SESSIZ
// yapardi - "bazen geri aliniyor, bazen alinmiyor".
//
// Komutlar entity'yi UUID ile tutuyor, entt tutamagi ile degil: silinen
// bir entity geri konunca entt kimligi degisir, UUID degismez (Faz 8'in
// tasiyici fikri).
// ===========================================================================

#include <FXEngine/Scene/Entity.h>

#include <string>
#include <vector>

namespace FX
{
    class Scene;
    class TextureLibrary;
    struct ComponentInfo;
}

namespace FXEd
{
    class CommandStack;
    class SelectionContext;

    namespace Structural
    {
        // ComponentDrawer::RegisterEditorUI ile ayni kalip: editor
        // acilista bagliyor, paneller yalnizca cagiriyor.
        struct Context
        {
            FX::Scene*          Scene     = nullptr;
            CommandStack*       Commands  = nullptr;
            SelectionContext*   Selection = nullptr;
            FX::TextureLibrary* Library   = nullptr;
        };

        void SetContext(const Context& context);

        // Sahne isaretcisi Play/Stop ve sahne yuklemede degisiyor.
        void SetScene(FX::Scene* scene);

        // Olusturur, secer ve komutu yigina yazar. Gecersiz parent = kok.
        FX::Entity CreateEntity(const std::string& name, FX::Entity parent = {});

        // ZATEN olusturulmus ve kurulmus bir entity'yi geri alinabilir
        // yapar (prefab ornekleme, viewport'a doku birakma). Cagri
        // entity'nin son halinden SONRA yapilmali - snapshot o an
        // aliniyor, redo onu geri koyuyor.
        void PushCreated(FX::Entity entity, const std::string& label);

        // Verilenleri (ve alt agaclarini) cogaltir: her biri kaynaginin
        // kardesi, yeni kimliklerle. Kopyalar secilir; tamami TEK undo
        // adimi. Doner: yeni koklerin sayisi.
        int DuplicateEntities(const std::vector<FX::Entity>& sources,
                              const std::string& label = "Cogalt");

        // Verilenleri (ve alt agaclarini) siler. Zaten silinmis olanlar
        // atlanir. Doner: gercekten silinen sayisi.
        int DestroyEntities(const std::vector<FX::Entity>& entities);

        // entity'yi parent'inin cocuk listesinde bir sira tasir (-1 yukari,
        // +1 asagi). Geri alinabilir. Uctaysa is yapmaz.
        void MoveInParent(FX::Entity entity, int direction);

        // Surukle-birak: moved'i newParent'in altina (gecersiz = kok), hedef
        // kardesin ONUNE/ARKASINA (ya da newParent'a birakildiysa sonuna)
        // tasir. Konumu Scene::PlaceEntity index'ine cevirir ve geri
        // alinabilir yapar (eski parent + eski index yakalanir).
        void ReorderTo(FX::Entity moved, FX::Entity newParent, int index);

        // Component'i eksik olan hedeflere ekler / sahip olanlardan siler.
        void AddComponent(const FX::ComponentInfo& info,
                          const std::vector<FX::Entity>& targets);
        void RemoveComponent(const FX::ComponentInfo& info,
                             const std::vector<FX::Entity>& targets);

        // Prefab ornegini kaynagina gore geri alir (C-2). instanceRoot
        // ornegin KOKU olmali. Kaynak kayipsa / is olmadiysa komut yazmaz.
        // Undo/Redo, alt agaci snapshot'tan geri koyar (destroy + restore).
        void RevertPrefabInstance(FX::Entity instanceRoot);

        // Ornekteki degisiklikleri kaynak dosyaya uygular (C-4) ve diger
        // ornekleri tazeler. Undo dosyanin eski icerigini geri yazar ve
        // diger ornek koklerini snapshot'tan geri koyar (uygulanan ornek
        // Apply'da degismez, snapshot'a girmez).
        void ApplyPrefabInstance(FX::Entity instanceRoot);
    }
}
