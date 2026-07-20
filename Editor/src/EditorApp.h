#pragma once

// ===========================================================================
// FXEditor - EditorApp
//
// FX::Application'dan tureyip motorun birakigi bosluklari doldurur.
// Motor "ne zaman" cagrilacagini bilir; bu sinif "ne yapilacagini" bilir.
// ===========================================================================

#include <FXEngine/Core/Application.h>
#include <FXEngine/Renderer/Shader.h>
#include <FXEngine/Renderer/VertexArray.h>
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
        // Klavye durumuna gore kamerayi hareket ettirir.
        // OnEvent yerine OnUpdate'te yapiliyor - sebebi .cpp'de acikli.
        void UpdateCameraMovement(float dt);

        // En-boy oranina gore kamera projeksiyonunu yeniden kurar.
        void UpdateCameraProjection();

        // --- Cizim kaynaklari -------------------------------------------------
        std::unique_ptr<FX::Shader>      m_TextureShader;
        std::shared_ptr<FX::VertexArray> m_QuadVA;

        std::unique_ptr<FX::Texture2D>   m_Checkerboard;
        std::unique_ptr<FX::Texture2D>   m_Circle;

        // --- Kamera ------------------------------------------------------------
        // unique_ptr cunku yapicisi parametre istiyor ve gercek degerleri
        // ancak OnInit'te (pencere hazir olunca) biliyoruz.
        std::unique_ptr<FX::OrthographicCamera> m_Camera;

        glm::vec3 m_CameraPosition{ 0.0f, 0.0f, 0.0f };
        float     m_CameraRotation = 0.0f;    // derece
        float     m_ZoomLevel      = 5.0f;    // dikeyde gorunen birimin yarisi

        float m_CameraMoveSpeed   = 5.0f;     // birim / saniye
        float m_CameraRotateSpeed = 90.0f;    // derece / saniye

        // --- Durum -------------------------------------------------------------
        float m_Time = 0.0f;

        std::uint64_t m_UpdateCount = 0;
        std::uint64_t m_FrameCount  = 0;

        bool m_Wireframe = false;
        bool m_Animate   = true;
    };
}
