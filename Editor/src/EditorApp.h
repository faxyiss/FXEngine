#pragma once

// ===========================================================================
// FXEditor - EditorApp
// ===========================================================================

#include <FXEngine/Core/Application.h>
#include <FXEngine/Renderer/Texture.h>
#include <FXEngine/Renderer/TextureLibrary.h>
#include <FXEngine/Renderer/OrthographicCamera.h>
#include <FXEngine/Renderer/Framebuffer.h>
#include <FXEngine/Scene/Scene.h>
#include <FXEngine/Scene/Entity.h>
#include <FXEngine/Core/UUID.h>

#include "ImGuiLayer.h"
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/ContentBrowserPanel.h"

#include <deque>
#include <memory>
#include <string>

namespace FXEd
{
    class EditorApp : public FX::Application
    {
    public:
        EditorApp();

    protected:
        void OnInit()               override;
        void OnShutdown()           override;
        void OnUpdate(float dt)     override;
        void OnRender(float alpha)  override;
        void OnEvent(const SDL_Event& event) override;
        void OnWindowResize(std::uint32_t width, std::uint32_t height) override;

    private:
        void BuildScene();
        void SpawnMover();

        // --- Sahne dosyasi islemleri (Faz 12) ---------------------------------
        void NewScene();
        void SaveScene();                              // yol yoksa SaveSceneAs
        void SaveSceneAs();
        void OpenScene();                              // dosya diyalogu acar
        void OpenScene(const std::string& path);       // dogrudan yukler

        // Son acilan sahneler exe'nin yanindaki editor.json'da tutulur.
        void LoadEditorConfig();
        void SaveEditorConfig();
        void PushRecentScene(const std::string& path);

        // Icerik panelinden viewport'a birakilan dosyayi ele alir.
        void HandleContentDrop(const std::string& relativePath, float screenX, float screenY);

        // Isletim sisteminden (Explorer'dan) surukleyip birakilan dosya.
        void HandleExternalDrop(const std::string& absolutePath, float screenX, float screenY);

        // "Ice Aktar..." -> dosya diyalogu -> Icerik paneline kopyala.
        void ImportAssets();

        // Fare ekran konumu -> sahne dunya konumu.
        glm::vec2 ScreenToWorld(float screenX, float screenY) const;

        void SetStatus(const std::string& message);

        void UpdateCameraMovement(float dt);
        void UpdateCameraProjection();

        // Orta tus (veya Space + sol tus) ile suruklerken kamerayi kaydirir.
        void UpdateCameraPan();

        // Tekerlek zoom'u: imlecin altindaki dunya noktasi sabit kalir.
        void ZoomAtCursor(float wheelY, float mouseX, float mouseY);

        // ImGui panellerini cizer.
        void DrawMenuBar();
        void DrawViewportPanel();
        void DrawViewportToolbar();
        void DrawStatsPanel();
        void DrawGizmo();

        // Fare altindaki entity'yi framebuffer'in ID ekinden okur.
        void PickEntity();

        // --- Sahne -------------------------------------------------------------
        std::unique_ptr<FX::Scene> m_Scene;

        // FAZ 8 DEGISIKLIGI: Artik Entity tutamagi degil UUID sakliyoruz.
        //
        // Faz 7'de m_PlayerEntity bir tutamakti ve sahne yuklendiginde
        // gecersiz kaliyordu - elle temizlemek zorundaydik. UUID kalici
        // oldugu icin yukleme sonrasi da GECERLI: sadece FindEntityByUUID
        // ile cozmemiz yeterli.
        //
        // Bu, Faz 8'in getirdigi degisimin en somut ornegi.
        FX::UUID m_PlayerUUID{ 0 };

        // Her karede degil, ihtiyac aninda cozulur.
        FX::Entity GetPlayer();

        // --- Editor arayuzu ----------------------------------------------------
        ImGuiLayer          m_ImGuiLayer;
        SceneHierarchyPanel m_HierarchyPanel;
        ContentBrowserPanel m_ContentBrowser;

        // Sahne artik dogrudan ekrana degil, BUNA ciziliyor.
        // Sonra texture'i ImGui panelinde gosteriyoruz.
        std::unique_ptr<FX::Framebuffer> m_Framebuffer;

        // Viewport panelinin PIKSEL boyutu. Pencere boyutundan farklidir:
        // paneller yer kapladigi icin viewport her zaman daha kucuktur.
        // Kamera en-boy orani BUNA gore hesaplanmali, pencereye gore degil.
        glm::vec2 m_ViewportSize{ 0.0f, 0.0f };

        // Viewport panelinin ekran uzerindeki sinirlari. Fare konumunu
        // panel-yerel koordinata cevirmek icin gerekli.
        glm::vec2 m_ViewportBoundsMin{ 0.0f, 0.0f };
        glm::vec2 m_ViewportBoundsMax{ 0.0f, 0.0f };

        // Fare viewport panelinin uzerinde mi? Kamera kisayollarini
        // sadece o zaman calistiracagiz.
        bool m_ViewportHovered = false;
        bool m_ViewportFocused = false;

        // ImGuizmo islem tipi (ImGuizmo::OPERATION). -1 = gizmo kapali.
        int m_GizmoOperation = -1;

        // ImGuizmo::MODE - tutamaklar nesnenin kendi ekseninde mi (LOCAL)
        // yoksa dunya ekseninde mi (WORLD) duracak. ImGuizmo.h burada
        // dahil edilmedigi icin int; degerler .cpp'de atanir.
        int m_GizmoMode = 0;   // = ImGuizmo::LOCAL

        // Kademeli hareket. Ctrl basiliyken de gecici olarak acilir.
        bool  m_SnapEnabled   = false;
        float m_SnapTranslate = 0.5f;
        float m_SnapRotate    = 15.0f;
        float m_SnapScale     = 0.25f;

        // Fare ile kaydirma suruyor mu, hangi tusla baslatildi?
        // Tusu basili tutarken viewport'tan cikilabildigi icin durum
        // saklanmak zorunda: baslatma viewport'a bagli, surdurme degil.
        bool m_Panning   = false;
        int  m_PanButton = 0;

        // Menuden gelen ice aktarma istegi. Yerel diyalog modaldir ve
        // ImGui cercevesinin ortasinda acilamaz; cerceve bittikten sonra
        // isleniyor.
        bool m_ImportRequested = false;

        // --- Kaynaklar ---------------------------------------------------------
        // Kutuphane, yol -> texture eslemesini tutar. Sahne yuklerken
        // JSON'daki yollari texture'a cevirmek icin gerekli, ve ayni
        // dosyanin tekrar tekrar yuklenmesini onler.
        FX::TextureLibrary m_TextureLibrary;

        std::shared_ptr<FX::Texture2D> m_Checkerboard;
        std::shared_ptr<FX::Texture2D> m_Circle;

        // Acik sahnenin yolu. BOS = "henuz kaydedilmedi"; Ctrl+S bu durumda
        // Farkli Kaydet diyalogunu acar. Faz 7'deki sabit yol kalkti.
        std::string m_ScenePath;

        // En yenisi basta. deque cunku hem basa ekleyip hem sondan
        // atiyoruz; vector'de bas ekleme her seferinde tum diziyi kaydirirdi.
        std::deque<std::string> m_RecentScenes;
        static constexpr std::size_t kMaxRecentScenes = 8;

        // Son islemin sonucu - menude kullaniciya geri bildirim icin.
        std::string m_StatusMessage;
        float       m_StatusTimer = 0.0f;

        // --- Kamera ------------------------------------------------------------
        std::unique_ptr<FX::OrthographicCamera> m_Camera;

        glm::vec3 m_CameraPosition{ 0.0f, 0.0f, 0.0f };
        float     m_CameraRotation = 0.0f;
        float     m_ZoomLevel      = 8.0f;
        float     m_CameraMoveSpeed = 8.0f;

        // --- Durum -------------------------------------------------------------
        float m_Time = 0.0f;
        bool  m_ScenePaused = false;

        float m_FpsTimer   = 0.0f;
        int   m_FpsFrames  = 0;
        float m_CurrentFps = 0.0f;

        bool m_ShowDemoWindow = false;
    };
}
