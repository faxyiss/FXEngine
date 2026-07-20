#pragma once

// ===========================================================================
// FXEditor - EditorApp
// ===========================================================================

#include <FXEngine/Core/Application.h>
#include <FXEngine/Renderer/Texture.h>
#include <FXEngine/Renderer/OrthographicCamera.h>

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
        void UpdateCameraMovement(float dt);
        void UpdateCameraProjection();
        void LogStats();

        // --- Kaynaklar ---------------------------------------------------------
        // shared_ptr cunku Renderer2D::DrawQuad shared_ptr aliyor
        // (batch, texture'i slot dizisinde tutmak zorunda - o yuzden
        // sahiplik paylasilmali).
        std::shared_ptr<FX::Texture2D> m_Checkerboard;
        std::shared_ptr<FX::Texture2D> m_Circle;

        // --- Kamera ------------------------------------------------------------
        std::unique_ptr<FX::OrthographicCamera> m_Camera;

        glm::vec3 m_CameraPosition{ 0.0f, 0.0f, 0.0f };
        float     m_CameraRotation = 0.0f;
        float     m_ZoomLevel      = 12.0f;

        float m_CameraMoveSpeed   = 8.0f;
        float m_CameraRotateSpeed = 90.0f;

        // --- Test sahnesi ------------------------------------------------------
        // Izgara boyutu. 50x50 = 2500 quad. 1/2 tuslariyla degistirilebilir
        // ki batch'in sinirlarini GOZLE gorebilelim.
        int  m_GridSize = 50;
        bool m_UseTextures = false;   // texture'li mi, duz renkli mi

        // --- Durum -------------------------------------------------------------
        float m_Time = 0.0f;

        std::uint64_t m_FrameCount = 0;

        // FPS olcumu: batch'in etkisini SAYIYLA gormek icin.
        float m_FpsTimer      = 0.0f;
        int   m_FpsFrames     = 0;
        float m_CurrentFps    = 0.0f;

        bool m_Wireframe = false;
        bool m_Animate   = true;
    };
}
