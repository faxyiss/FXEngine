#pragma once

// ===========================================================================
// FXEditor - EditorCamera
//
// Duzenleme viewport'unun kamerasi. EditorApp'e dagilmis olan konum,
// zoom, kaydirma ve ekran<->dunya cevrimi burada toplandi.
//
// NEDEN AYRI BIR SINIF (Faz 10)?
// Play modunda sahnenin KENDI kamerasi (CameraComponent) devreye girecek.
// O an iki kamera birlikte var olacak: biri oyunun, biri editorun.
// Ikisi ayni degiskenler uzerinden yonetilemez.
//
// Motorda degil editorde: bu kameranin fare surukleme ve zoom davranisi
// bir DUZENLEME araci. Sevk edilen bir oyunda karsiligi yok.
// ===========================================================================

#include <FXEngine/Renderer/OrthographicCamera.h>

#include <glm/glm.hpp>

namespace FXEd
{
    class EditorCamera
    {
    public:
        EditorCamera();

        // Viewport panelinin piksel boyutu ve ekran uzerindeki sinirlari.
        // Sinirlar ScreenToWorld icin gerekli; boyut en-boy orani icin.
        void SetViewport(const glm::vec2& size,
                         const glm::vec2& boundsMin, const glm::vec2& boundsMax);

        // Klavye (W/A/S/D). ImGui klavyeyi istiyorsa cagirilmamali.
        void OnUpdate(float dt);

        // Fare ile kaydirma. ImGui cercevesi ICINDE cagrilir - fare
        // deltasini ImGui'den okuyor.
        //
        // canStart: kaydirma BASLATILABILIR mi (fare viewport'ta, gizmo
        // kullanilmiyor). Baslamis bir kaydirma bu bayraktan bagimsiz
        // surer: tusu basili tutarken panelden cikilabilmeli.
        void OnImGuiInteract(bool canStart, bool keyboardCaptured);

        // Tekerlek. Imlecin altindaki dunya noktasi sabit kalir.
        void OnMouseScroll(float wheelY, float mouseX, float mouseY);

        // Ekran (ImGui) koordinati -> sahne dunya koordinati.
        glm::vec2 ScreenToWorld(float screenX, float screenY) const;

        // Verilen noktayi merkeze alip `extent` birimlik nesneyi
        // ekrana sigdirir.
        void FocusOn(const glm::vec2& position, float extent);

        void Reset();

        const FX::OrthographicCamera& GetCamera() const { return m_Camera; }

        const glm::vec3& GetPosition() const { return m_Position; }

        float GetZoom() const { return m_ZoomLevel; }
        void  SetZoom(float zoom);

        // Kaydirma su an suruyor mu? EditorApp secim yaparken buna bakiyor.
        bool IsPanning() const { return m_Panning; }

        static constexpr float kMinZoom = 1.0f;
        static constexpr float kMaxZoom = 40.0f;

    private:
        void RecalculateProjection();

        FX::OrthographicCamera m_Camera;

        // Kamera DONMUYOR: bu bir sahne duzenleme viewport'u, egik kamera
        // duzenlemeyi zorlastirmaktan baska ise yaramiyordu. Ekran ekseni
        // = dunya ekseni oldugu icin kaydirma da sadelesiyor.
        glm::vec3 m_Position{ 0.0f, 0.0f, 0.0f };
        float     m_ZoomLevel = 8.0f;
        float     m_MoveSpeed = 8.0f;

        glm::vec2 m_ViewportSize{ 0.0f, 0.0f };
        glm::vec2 m_BoundsMin{ 0.0f, 0.0f };
        glm::vec2 m_BoundsMax{ 0.0f, 0.0f };

        bool m_Panning   = false;
        int  m_PanButton = 0;
    };
}
