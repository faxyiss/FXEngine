#include "EditorCamera.h"

#include <FXEngine/Core/Input.h>

#include <imgui.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace FXEd
{
    EditorCamera::EditorCamera()
        : m_Camera(-1.0f, 1.0f, -1.0f, 1.0f)
    {
    }

    void EditorCamera::SetViewport(const glm::vec2& size,
                                   const glm::vec2& boundsMin, const glm::vec2& boundsMax)
    {
        m_BoundsMin = boundsMin;
        m_BoundsMax = boundsMax;

        if (size.x > 0.0f && size.y > 0.0f && size != m_ViewportSize)
        {
            m_ViewportSize = size;
            RecalculateProjection();
        }
    }

    void EditorCamera::RecalculateProjection()
    {
        // Pencereye degil VIEWPORT PANELINE gore: paneller yer kapladigi
        // icin viewport her zaman pencereden kucuk, pencereyi kullansaydik
        // sahne yamuk gorunurdu.
        if (m_ViewportSize.y <= 0.0f)
            return;

        m_Camera.SetProjectionFromAspect(m_ViewportSize.x / m_ViewportSize.y, m_ZoomLevel);
    }

    void EditorCamera::SetZoom(float zoom)
    {
        m_ZoomLevel = std::clamp(zoom, kMinZoom, kMaxZoom);
        RecalculateProjection();
    }

    void EditorCamera::Reset()
    {
        m_Position  = { 0.0f, 0.0f, 0.0f };
        m_ZoomLevel = 8.0f;
        m_Camera.SetPosition(m_Position);
        RecalculateProjection();
    }

    void EditorCamera::OnUpdate(float dt)
    {
        // Surekli hareket SORGU tabanli girdi ister: "basildi mi" degil,
        // "su an basili mi". Faz 13a'nin FX::Input'u tam olarak bunun icin.
        float dx = 0.0f, dy = 0.0f;
        if (FX::Input::IsKeyPressed(FX::Key::A)) dx -= 1.0f;
        if (FX::Input::IsKeyPressed(FX::Key::D)) dx += 1.0f;
        if (FX::Input::IsKeyPressed(FX::Key::S)) dy -= 1.0f;
        if (FX::Input::IsKeyPressed(FX::Key::W)) dy += 1.0f;

        const float len = std::sqrt(dx * dx + dy * dy);
        if (len <= 0.0f)
            return;

        // Capraz giderken hizlanmamak icin normalize.
        const float move = m_MoveSpeed * dt / len;
        m_Position.x += dx * move;
        m_Position.y += dy * move;

        m_Camera.SetPosition(m_Position);
    }

    void EditorCamera::OnImGuiInteract(bool canStart, bool keyboardCaptured)
    {
        if (!m_Panning)
        {
            if (!canStart)
                return;

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                m_PanButton = ImGuiMouseButton_Right;
            else if (!keyboardCaptured &&
                     ImGui::IsKeyDown(ImGuiKey_Space) &&
                     ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                m_PanButton = ImGuiMouseButton_Left;
            else
                return;

            m_Panning = true;
        }

        if (!ImGui::IsMouseDown(m_PanButton))
        {
            m_Panning = false;
            return;
        }

        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        if (delta.x == 0.0f && delta.y == 0.0f)
            return;

        // Piksel deltasini elle olceklemek yerine AYNI KAREDE iki ekran
        // noktasini dunyaya cevirip farkini aliyoruz: zoom bedavaya dogru
        // cikiyor ve imlecin altindaki nokta parmaga yapisik kaliyor.
        const ImVec2 mouse = ImGui::GetMousePos();
        const glm::vec2 now  = ScreenToWorld(mouse.x, mouse.y);
        const glm::vec2 prev = ScreenToWorld(mouse.x - delta.x, mouse.y - delta.y);

        m_Position.x -= now.x - prev.x;
        m_Position.y -= now.y - prev.y;
        m_Camera.SetPosition(m_Position);
    }

    void EditorCamera::OnMouseScroll(float wheelY, float mouseX, float mouseY)
    {
        // Olc, zoomla, tekrar olc, farki geri it. Merkeze zoom yapmak
        // buyutmek istedigin seyi ekrandan kacirir.
        const glm::vec2 before = ScreenToWorld(mouseX, mouseY);

        m_ZoomLevel = std::clamp(m_ZoomLevel * ((wheelY > 0.0f) ? 0.9f : 1.1f),
                                 kMinZoom, kMaxZoom);
        RecalculateProjection();

        const glm::vec2 after = ScreenToWorld(mouseX, mouseY);

        m_Position.x += before.x - after.x;
        m_Position.y += before.y - after.y;
        m_Camera.SetPosition(m_Position);
    }

    glm::vec2 EditorCamera::ScreenToWorld(float screenX, float screenY) const
    {
        const float width  = m_BoundsMax.x - m_BoundsMin.x;
        const float height = m_BoundsMax.y - m_BoundsMin.y;

        if (width <= 0.0f || height <= 0.0f)
            return { 0.0f, 0.0f };

        // Panel-yerel piksel -> NDC (-1..1). Y ters: ImGui yukaridan
        // asagi sayar, NDC asagidan yukari.
        const float ndcX = 2.0f * (screenX - m_BoundsMin.x) / width - 1.0f;
        const float ndcY = 1.0f - 2.0f * (screenY - m_BoundsMin.y) / height;

        const glm::mat4 inverseVP =
            glm::inverse(m_Camera.GetProjectionMatrix() * m_Camera.GetViewMatrix());

        const glm::vec4 world = inverseVP * glm::vec4(ndcX, ndcY, 0.0f, 1.0f);
        return { world.x, world.y };
    }

    void EditorCamera::FocusOn(const glm::vec2& position, float extent)
    {
        m_Position = { position.x, position.y, 0.0f };

        // Pay birakiyoruz: tam sinirina zoom yapmak nesneyi ekranin
        // kenarina yapistirirdi.
        m_ZoomLevel = std::clamp(extent * 2.5f, kMinZoom, kMaxZoom);

        m_Camera.SetPosition(m_Position);
        RecalculateProjection();
    }
}
