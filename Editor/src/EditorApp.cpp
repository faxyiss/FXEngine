#include "EditorApp.h"

#include <FXEngine/Core/Log.h>
#include <FXEngine/Core/FileSystem.h>
#include <FXEngine/Renderer/RenderCommand.h>
#include <FXEngine/Renderer/Renderer2D.h>

#include <SDL3/SDL.h>

#include <cmath>

namespace FXEd
{
    EditorApp::EditorApp()
        : FX::Application([]() {
              FX::WindowProps props;
              props.Title     = "FXEditor - Faz 4 (Batch Renderer)";
              props.Width     = 1280;
              props.Height    = 720;
              props.VSync     = true;
              props.Resizable = true;
              return props;
          }())
    {
    }

    void EditorApp::OnInit()
    {
        FX_INFO("=====================================");
        FX_INFO("  FXEditor - Faz 4: Batch Renderer");
        FX_INFO("=====================================");

        FX::RenderCommand::Init();

        // Renderer2D kendi shader'ini, buffer'larini ve beyaz texture'ini
        // kendi kurar. Editor'un artik VAO/VBO/shader bilmesine gerek yok -
        // Faz 3'te bu dosyada 60 satir GPU kurulumu vardi, simdi tek satir.
        // Iyi bir soyutlamanin belirtisi budur.
        FX::Renderer2D::Init();

        FX::TextureSpec sharpSpec;
        sharpSpec.MinFilter = FX::TextureFilter::Nearest;
        sharpSpec.MagFilter = FX::TextureFilter::Nearest;
        sharpSpec.WrapS     = FX::TextureWrap::Repeat;
        sharpSpec.WrapT     = FX::TextureWrap::Repeat;

        m_Checkerboard = std::make_shared<FX::Texture2D>(
            "assets/textures/checkerboard.png", sharpSpec);
        m_Circle = std::make_shared<FX::Texture2D>("assets/textures/circle.png");

        if (!m_Checkerboard->IsValid() || !m_Circle->IsValid())
        {
            FX_ERROR("Texture'lar yuklenemedi. Aranan klasor: %s",
                     FX::FileSystem::GetBaseDirectory().c_str());
            Close();
            return;
        }

        m_Camera = std::make_unique<FX::OrthographicCamera>(-1.0f, 1.0f, -1.0f, 1.0f);
        UpdateCameraProjection();

        FX_INFO("Texture slotu (bu GPU): %u", FX::Renderer2D::GetMaxTextureSlots());
        FX_INFO("");
        FX_INFO("Kontroller:");
        FX_INFO("  W/A/S/D  -> kamera hareket");
        FX_INFO("  Q/E      -> kamera dondur");
        FX_INFO("  Tekerlek -> zoom");
        FX_INFO("  R        -> kamerayi sifirla");
        FX_INFO("  1 / 2    -> izgarayi kucult / buyut  <-- ASIL TEST");
        FX_INFO("  X        -> duz renk <-> texture");
        FX_INFO("  T        -> tel kafes");
        FX_INFO("  SPACE    -> animasyon dur/devam");
        FX_INFO("  V        -> VSync (kapatinca gercek FPS gorunur)");
        FX_INFO("  TAB      -> istatistikler");
        FX_INFO("  ESC      -> cikis");
        FX_INFO("");
        FX_INFO("Izgara: %dx%d = %d quad", m_GridSize, m_GridSize, m_GridSize * m_GridSize);
    }

    void EditorApp::UpdateCameraProjection()
    {
        const float w = static_cast<float>(GetWindow().GetWidth());
        const float h = static_cast<float>(GetWindow().GetHeight());
        if (h <= 0.0f)
            return;
        m_Camera->SetProjectionFromAspect(w / h, m_ZoomLevel);
    }

    void EditorApp::OnWindowResize(std::uint32_t, std::uint32_t)
    {
        UpdateCameraProjection();
    }

    void EditorApp::UpdateCameraMovement(float dt)
    {
        const bool* keys = SDL_GetKeyboardState(nullptr);

        const float move   = m_CameraMoveSpeed   * dt;
        const float rotate = m_CameraRotateSpeed * dt;

        const float rad = glm::radians(m_CameraRotation);
        const float cs  = std::cos(rad);
        const float sn  = std::sin(rad);

        float dx = 0.0f, dy = 0.0f;
        if (keys[SDL_SCANCODE_A]) dx -= 1.0f;
        if (keys[SDL_SCANCODE_D]) dx += 1.0f;
        if (keys[SDL_SCANCODE_S]) dy -= 1.0f;
        if (keys[SDL_SCANCODE_W]) dy += 1.0f;

        const float len = std::sqrt(dx * dx + dy * dy);
        if (len > 0.0f)
        {
            dx /= len;
            dy /= len;
            m_CameraPosition.x += (dx * cs - dy * sn) * move;
            m_CameraPosition.y += (dx * sn + dy * cs) * move;
        }

        if (keys[SDL_SCANCODE_Q]) m_CameraRotation += rotate;
        if (keys[SDL_SCANCODE_E]) m_CameraRotation -= rotate;

        m_Camera->SetPosition(m_CameraPosition);
        m_Camera->SetRotation(m_CameraRotation);
    }

    void EditorApp::OnUpdate(float dt)
    {
        if (m_Animate)
            m_Time += dt;

        UpdateCameraMovement(dt);
    }

    void EditorApp::OnRender(float /*alpha*/)
    {
        ++m_FrameCount;

        // --- FPS olcumu --------------------------------------------------------
        // Gercek zamani olcuyoruz (mantik zamanini degil), cunku batch'in
        // etkisi CIZIM tarafinda. VSync kapaliyken bu sayi anlamli olur.
        {
            static std::uint64_t s_LastTime = SDL_GetTicksNS();
            const std::uint64_t now = SDL_GetTicksNS();
            m_FpsTimer += static_cast<float>(now - s_LastTime) / 1.0e9f;
            s_LastTime = now;

            ++m_FpsFrames;
            if (m_FpsTimer >= 0.5f)
            {
                m_CurrentFps = static_cast<float>(m_FpsFrames) / m_FpsTimer;
                m_FpsFrames  = 0;
                m_FpsTimer   = 0.0f;
            }
        }

        FX::RenderCommand::SetClearColor({ 0.08f, 0.09f, 0.12f, 1.0f });
        FX::RenderCommand::Clear();

        // Istatistikleri her karede sifirla ki "bu karede kac draw call"
        // sorusunu cevaplayabilelim.
        FX::Renderer2D::ResetStats();

        // ===================================================================
        // BATCH: BeginScene ile EndScene arasindaki HER SEY tek pakete girer
        // ===================================================================
        FX::Renderer2D::BeginScene(*m_Camera);

        // --- Izgara ------------------------------------------------------------
        // m_GridSize^2 kadar quad. 50x50 = 2500.
        // Faz 3'teki yaklasimla bu 2500 draw call olurdu; simdi 1 (veya
        // texture kullanirsak yine 1, cunku sadece 2 texture var).
        const float half = static_cast<float>(m_GridSize) * 0.5f;

        for (int y = 0; y < m_GridSize; ++y)
        {
            for (int x = 0; x < m_GridSize; ++x)
            {
                const float fx = (static_cast<float>(x) - half) * 0.11f;
                const float fy = (static_cast<float>(y) - half) * 0.11f;

                // Renk konuma gore degissin - her quad'in AYRI rengi
                // olabildigini gostermek icin. Faz 3'te renk uniform'du,
                // yani her farkli renk ayri bir draw call demekti.
                const glm::vec4 color = {
                    (static_cast<float>(x) / static_cast<float>(m_GridSize)) * 0.8f + 0.2f,
                    0.35f,
                    (static_cast<float>(y) / static_cast<float>(m_GridSize)) * 0.8f + 0.2f,
                    0.85f
                };

                if (m_UseTextures)
                {
                    // Iki texture'i donusumlu kullaniyoruz. Ikisi de ayni
                    // batch'te kaliyor cunku 32 slot var, 2 tanesini
                    // kullaniyoruz -> hala TEK draw call.
                    const auto& tex = ((x + y) % 2 == 0) ? m_Checkerboard : m_Circle;
                    FX::Renderer2D::DrawQuad({ fx, fy }, { 0.09f, 0.09f }, tex, 1.0f, color);
                }
                else
                {
                    FX::Renderer2D::DrawQuad({ fx, fy }, { 0.09f, 0.09f }, color);
                }
            }
        }

        // --- Donen buyuk sprite'lar --------------------------------------------
        // Izgaranin ustunde, dondurulmus quad'lar. Bunlar da AYNI batch'e
        // giriyor - farkli texture, farkli aci, farkli renk olmasi
        // hicbir sey degistirmiyor.
        for (int i = 0; i < 4; ++i)
        {
            const float angle  = m_Time * (0.5f + static_cast<float>(i) * 0.2f);
            const float radius = 2.5f;
            const float px = std::cos(m_Time * 0.4f + static_cast<float>(i) * 1.57f) * radius;
            const float py = std::sin(m_Time * 0.4f + static_cast<float>(i) * 1.57f) * radius;

            FX::Renderer2D::DrawRotatedQuad(
                { px, py }, { 1.2f, 1.2f }, angle,
                (i % 2 == 0) ? m_Checkerboard : m_Circle,
                1.0f,
                { 1.0f, 1.0f, 1.0f, 0.95f });
        }

        FX::Renderer2D::EndScene();
    }

    void EditorApp::LogStats()
    {
        const auto stats = FX::Renderer2D::GetStats();

        FX_INFO("--- Istatistikler ---");
        FX_INFO("  Izgara         : %dx%d", m_GridSize, m_GridSize);
        FX_INFO("  Quad sayisi    : %u", stats.QuadCount);
        FX_INFO("  Kose sayisi    : %u", stats.GetVertexCount());
        FX_INFO("  DRAW CALL      : %u   <-- asil sayi bu", stats.DrawCalls);

        if (stats.DrawCalls > 0)
            FX_INFO("  Quad/draw call : %.0f", static_cast<float>(stats.QuadCount) /
                                               static_cast<float>(stats.DrawCalls));

        FX_INFO("  Batch'siz olsa : %u draw call olurdu", stats.QuadCount);
        FX_INFO("  FPS            : %.0f%s", m_CurrentFps,
                GetWindow().IsVSync() ? "  (VSync ACIK - V ile kapat, gercek FPS'i gor)" : "");
        FX_INFO("  Mod            : %s", m_UseTextures ? "TEXTURE'LI" : "DUZ RENK");
        FX_INFO("  Kamera         : (%.1f, %.1f) zoom %.1f",
                m_CameraPosition.x, m_CameraPosition.y, m_ZoomLevel);
    }

    void EditorApp::OnEvent(const SDL_Event& event)
    {
        if (event.type == SDL_EVENT_MOUSE_WHEEL)
        {
            m_ZoomLevel *= (event.wheel.y > 0) ? 0.9f : 1.1f;
            m_ZoomLevel = glm::clamp(m_ZoomLevel, 0.5f, 60.0f);
            UpdateCameraProjection();
            return;
        }

        if (event.type == SDL_EVENT_KEY_DOWN)
        {
            if (event.key.repeat)
                return;

            switch (event.key.key)
            {
                case SDLK_ESCAPE:
                    Close();
                    break;

                case SDLK_1:
                    m_GridSize = std::max(10, m_GridSize - 20);
                    FX_INFO("Izgara: %dx%d = %d quad", m_GridSize, m_GridSize,
                            m_GridSize * m_GridSize);
                    break;

                case SDLK_2:
                    // 150x150 = 22.500 quad -> MAX_QUADS (10.000) asiliyor
                    // ve renderer OTOMATIK olarak birden fazla batch'e boluyor.
                    // TAB ile draw call sayisinin 1'den 3'e ciktigini gorursun.
                    // Batch'in sinirini gozle gormek icin bu test onemli.
                    m_GridSize = std::min(200, m_GridSize + 20);
                    FX_INFO("Izgara: %dx%d = %d quad%s", m_GridSize, m_GridSize,
                            m_GridSize * m_GridSize,
                            (m_GridSize * m_GridSize > 10000)
                                ? "  (10.000 siniri asildi - TAB'a bas, draw call artmis olmali)"
                                : "");
                    break;

                case SDLK_X:
                    m_UseTextures = !m_UseTextures;
                    FX_INFO("Mod -> %s", m_UseTextures ? "TEXTURE'LI" : "DUZ RENK");
                    break;

                case SDLK_R:
                    m_CameraPosition = { 0.0f, 0.0f, 0.0f };
                    m_CameraRotation = 0.0f;
                    m_ZoomLevel      = 12.0f;
                    UpdateCameraProjection();
                    break;

                case SDLK_T:
                    m_Wireframe = !m_Wireframe;
                    FX::RenderCommand::SetWireframe(m_Wireframe);
                    break;

                case SDLK_SPACE:
                    m_Animate = !m_Animate;
                    break;

                case SDLK_V:
                {
                    const bool s = !GetWindow().IsVSync();
                    GetWindow().SetVSync(s);
                    FX_INFO("VSync -> %s", s ? "ACIK" : "KAPALI");
                    break;
                }

                case SDLK_TAB:
                    LogStats();
                    break;

                default:
                    break;
            }
        }
    }

    void EditorApp::OnShutdown()
    {
        // Renderer2D'yi GL context olmeden once kapat.
        FX::Renderer2D::Shutdown();

        FX_INFO("Editor kapaniyor. Toplam %llu kare.",
                static_cast<unsigned long long>(m_FrameCount));
    }
}
