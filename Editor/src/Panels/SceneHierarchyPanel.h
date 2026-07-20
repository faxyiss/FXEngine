#pragma once

// ===========================================================================
// FXEditor - SceneHierarchyPanel
//
// Iki panel tek sinifta:
//   HIERARCHY -> sahnedeki entity'lerin listesi, secim
//   INSPECTOR -> secili entity'nin component'lerini goster/duzenle
//
// Neden ayri iki sinif degil? Cunku ikisi de AYNI durumu okuyor:
// "hangi entity secili". Ayirsaydik ikisine de ayni SelectionContext'i
// gecirmemiz gerekirdi; birlikte tutmak simdilik daha basit.
//
// SECIMIN SAHIBI BU PANEL DEGIL (0.2). Panel de viewport, gizmo ve
// prefab gibi sadece bir TUKETICI; secim EditorApp'in tuttugu
// SelectionContext'te duruyor.
// ===========================================================================

#include "SelectionContext.h"

#include <FXEngine/Scene/Scene.h>
#include <FXEngine/Scene/Entity.h>
#include <FXEngine/Scene/Components.h>
#include <FXEngine/Renderer/Texture.h>
#include <FXEngine/Renderer/TextureLibrary.h>

#include <memory>
#include <vector>

namespace FXEd
{
    class SceneHierarchyPanel
    {
    public:
        void SetContext(FX::Scene* scene);

        // Faz 12: sabit iki doku yerine kutuphanenin tamami. Texture artik
        // Icerik panelinden surukle-birak ile veriliyor.
        void SetTextureLibrary(FX::TextureLibrary* library) { m_Library = library; }

        // Her karede cagrilir; iki ImGui penceresi cizer.
        void OnImGuiRender();

        // Secim disaridan veriliyor; panel sahibi degil.
        void SetSelection(SelectionContext* selection) { m_Selection = selection; }

        // Kullanici "Prefab olarak kaydet" dediyse hedef entity'yi dondurur
        // ve istegi temizler. Dosya diyalogu acmak panelin isi degil -
        // pencereye erisimi olan EditorApp'in isi.
        FX::Entity TakePrefabRequest();

    private:
        // Hierarchy'de tek bir satir.
        void DrawEntityNode(FX::Entity entity);

        // Explorer/Unity davranisi: tek tik secer, Ctrl ekler/cikarir,
        // Shift son tiklanandan buraya kadar olan araligi secer.
        //
        // Tiklama ANINDA uygulanmiyor: Shift araligi "gorunen sira"ya
        // ihtiyac duyuyor ve o sira ancak agac tamamen cizilince belli
        // oluyor. Istek biriktirilip cerceve sonunda isleniyor - agac
        // uzerinde gezerken secim degistirmemenin de faydasi var.
        void ApplyPendingClick();

        // Inspector'da secili entity'nin tum component'leri.
        void DrawComponents(FX::Entity entity);

        void DrawTextureSlot(FX::SpriteRendererComponent& sprite);

        // "Component Ekle" acilir menusu.
        void DrawAddComponentMenu(FX::Entity entity);

        FX::Scene* m_Scene = nullptr;   // sahibi degiliz, sadece referans

        SelectionContext* m_Selection = nullptr;   // sahibi degiliz

        // Agac uzerinde gezerken yapiyi degistirmek yineleyicileri bozar;
        // istekleri biriktirip dongu bittikten sonra uyguluyoruz.
        std::vector<FX::Entity> m_ToDelete;
        FX::Entity m_ReparentChild;
        FX::Entity m_ReparentTarget;
        bool       m_ReparentToRoot = false;

        FX::Entity m_PrefabRequest;

        // Bu karede cizilen entity'ler, GORUNEN sirada (kapali dallarin
        // cocuklari yok). Shift araligi bunun uzerinden hesaplaniyor.
        std::vector<FX::Entity> m_VisibleOrder;

        FX::Entity m_ClickTarget;
        bool       m_ClickCtrl  = false;
        bool       m_ClickShift = false;

        // Shift araliginin baslangici: son kez Shift'siz tiklanan entity.
        FX::Entity m_RangeAnchor;

        FX::TextureLibrary* m_Library = nullptr;
    };
}
