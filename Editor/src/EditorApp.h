#pragma once

// ===========================================================================
// FXEditor - EditorApp
// ===========================================================================

#include <FXEngine/Core/Application.h>
#include <FXEngine/Renderer/Texture.h>
#include <FXEngine/Renderer/OrthographicCamera.h>
#include <FXEngine/Scene/Scene.h>
#include <FXEngine/Scene/Entity.h>

#include <memory>
#include <vector>

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

        // Test sahnesini kurar. Entity'leri OLUSTURUR ama nasil
        // cizileceklerini/hareket edeceklerini BILMEZ - o sistemlerin isi.
        void BuildScene();

        // Rastgele bir yerde hareketli bir entity uretir.
        void SpawnMover();

        // --- Sahne -------------------------------------------------------------
        std::unique_ptr<FX::Scene> m_Scene;

        // Ozel bir entity'ye referans tutmak: tutamak kopyalamak ucuz,
        // isaretci tutmaya gerek yok. Bu, Entity'nin "handle" olmasinin
        // pratik faydasi.
        FX::Entity m_PlayerEntity;

        // --- Kaynaklar ---------------------------------------------------------
        std::shared_ptr<FX::Texture2D> m_Checkerboard;
        std::shared_ptr<FX::Texture2D> m_Circle;

        // --- Kamera ------------------------------------------------------------
        std::unique_ptr<FX::OrthographicCamera> m_Camera;

        glm::vec3 m_CameraPosition{ 0.0f, 0.0f, 0.0f };
        float     m_CameraRotation = 0.0f;
        float     m_ZoomLevel      = 8.0f;

        float m_CameraMoveSpeed   = 8.0f;
        float m_CameraRotateSpeed = 90.0f;

        // --- Durum -------------------------------------------------------------
        float m_Time = 0.0f;

        float m_FpsTimer   = 0.0f;
        int   m_FpsFrames  = 0;
        float m_CurrentFps = 0.0f;

        bool m_Wireframe = false;
        bool m_PlayerControl = true;   // WASD kamerayi mi oyuncuyu mu surer
    };
}
