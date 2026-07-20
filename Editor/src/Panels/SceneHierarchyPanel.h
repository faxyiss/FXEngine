#pragma once

// ===========================================================================
// FXEditor - SceneHierarchyPanel
//
// Iki panel tek sinifta:
//   HIERARCHY -> sahnedeki entity'lerin listesi, secim
//   INSPECTOR -> secili entity'nin component'lerini goster/duzenle
//
// Neden ayri iki sinif degil? Cunku ikisi de AYNI durumu paylasiyor:
// "hangi entity secili". Ayirsaydik bu durumu bir yerde tutup ikisine
// de gecirmemiz gerekirdi. Birlikte tutmak simdilik daha basit ve
// daha az hata kaynagi.
// ===========================================================================

#include <FXEngine/Scene/Scene.h>
#include <FXEngine/Scene/Entity.h>
#include <FXEngine/Scene/Components.h>
#include <FXEngine/Renderer/Texture.h>
#include <FXEngine/Renderer/TextureLibrary.h>

#include <memory>

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

        FX::Entity GetSelectedEntity() const { return m_Selection; }
        void SetSelectedEntity(FX::Entity entity) { m_Selection = entity; }

        // Kullanici "Prefab olarak kaydet" dediyse hedef entity'yi dondurur
        // ve istegi temizler. Dosya diyalogu acmak panelin isi degil -
        // pencereye erisimi olan EditorApp'in isi.
        FX::Entity TakePrefabRequest();

    private:
        // Hierarchy'de tek bir satir.
        void DrawEntityNode(FX::Entity entity);

        // Inspector'da secili entity'nin tum component'leri.
        void DrawComponents(FX::Entity entity);

        void DrawTextureSlot(FX::SpriteRendererComponent& sprite);

        // "Component Ekle" acilir menusu.
        void DrawAddComponentMenu(FX::Entity entity);

        FX::Scene* m_Scene = nullptr;   // sahibi degiliz, sadece referans

        FX::Entity m_Selection;

        // Agac uzerinde gezerken yapiyi degistirmek yineleyicileri bozar;
        // istekleri biriktirip dongu bittikten sonra uyguluyoruz.
        FX::Entity m_ToDelete;
        FX::Entity m_ReparentChild;
        FX::Entity m_ReparentTarget;
        bool       m_ReparentToRoot = false;

        FX::Entity m_PrefabRequest;

        FX::TextureLibrary* m_Library = nullptr;
    };
}
