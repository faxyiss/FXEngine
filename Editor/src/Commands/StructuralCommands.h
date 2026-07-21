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

        // Verilenleri (ve alt agaclarini) siler. Zaten silinmis olanlar
        // atlanir. Doner: gercekten silinen sayisi.
        int DestroyEntities(const std::vector<FX::Entity>& entities);

        // Component'i eksik olan hedeflere ekler / sahip olanlardan siler.
        void AddComponent(const FX::ComponentInfo& info,
                          const std::vector<FX::Entity>& targets);
        void RemoveComponent(const FX::ComponentInfo& info,
                             const std::vector<FX::Entity>& targets);
    }
}
