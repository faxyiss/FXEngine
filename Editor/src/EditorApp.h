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

#include "ImGuiLayer.h"
#include "Panels/SceneHierarchyPanel.h"

#include <memory>

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

        // Faz 7: sahne kaydet/yukle
        void SaveScene();
        void LoadScene();

        void UpdateCameraMovement(float dt);
        void UpdateCameraProjection();

        // ImGui panellerini cizer.
        void DrawMenuBar();
        void DrawViewportPanel();
        void DrawStatsPanel();

        // --- Sahne -------------------------------------------------------------
        std::unique_ptr<FX::Scene> m_Scene;
        FX::Entity m_PlayerEntity;

        // --- Editor arayuzu ----------------------------------------------------
        ImGuiLayer          m_ImGuiLayer;
        SceneHierarchyPanel m_HierarchyPanel;

        // Sahne artik dogrudan ekrana degil, BUNA ciziliyor.
        // Sonra texture'i ImGui panelinde gosteriyoruz.
        std::unique_ptr<FX::Framebuffer> m_Framebuffer;

        // Viewport panelinin PIKSEL boyutu. Pencere boyutundan farklidir:
        // paneller yer kapladigi icin viewport her zaman daha kucuktur.
        // Kamera en-boy orani BUNA gore hesaplanmali, pencereye gore degil.
        glm::vec2 m_ViewportSize{ 0.0f, 0.0f };

        // Fare viewport panelinin uzerinde mi? Kamera kisayollarini
        // sadece o zaman calistiracagiz.
        bool m_ViewportHovered = false;
        bool m_ViewportFocused = false;

        // --- Kaynaklar ---------------------------------------------------------
        // Kutuphane, yol -> texture eslemesini tutar. Sahne yuklerken
        // JSON'daki yollari texture'a cevirmek icin gerekli, ve ayni
        // dosyanin tekrar tekrar yuklenmesini onler.
        FX::TextureLibrary m_TextureLibrary;

        std::shared_ptr<FX::Texture2D> m_Checkerboard;
        std::shared_ptr<FX::Texture2D> m_Circle;

        // Sahne dosyasinin yolu. Gercek bir editorde dosya secme
        // penceresi olurdu; MVP icin sabit yol yeterli.
        std::string m_ScenePath = "assets/scenes/Sahne.fxscene";

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
