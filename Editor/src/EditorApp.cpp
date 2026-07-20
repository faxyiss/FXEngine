#include "EditorApp.h"

#include <FXEngine/Core/Log.h>
#include <FXEngine/Core/FileSystem.h>
#include <FXEngine/Renderer/RenderCommand.h>
#include <FXEngine/Renderer/Renderer2D.h>
#include <FXEngine/Scene/Components.h>

#include <imgui.h>

#include <SDL3/SDL.h>

#include <cmath>
#include <random>

namespace FXEd
{
    namespace
    {
        std::mt19937 s_Rng{ 12345 };

        float RandFloat(float min, float max)
        {
            std::uniform_real_distribution<float> dist(min, max);
            return dist(s_Rng);
        }
    }

    EditorApp::EditorApp()
        : FX::Application([]() {
              FX::WindowProps props;
              props.Title     = "FXEditor - Faz 6";
              props.Width     = 1600;
              props.Height    = 900;
              props.VSync     = true;
              props.Resizable = true;
              return props;
          }())
    {
    }

    void EditorApp::OnInit()
    {
        FX_INFO("=====================================");
        FX_INFO("  FXEditor - Faz 6: ImGui Editor");
        FX_INFO("=====================================");

        FX::RenderCommand::Init();
        FX::Renderer2D::Init();

        m_ImGuiLayer.Init(GetWindow());

        // Framebuffer: sahne buraya cizilecek, ekrana degil.
        FX::FramebufferSpec fbSpec;
        fbSpec.Width  = GetWindow().GetWidth();
        fbSpec.Height = GetWindow().GetHeight();
        m_Framebuffer = std::make_unique<FX::Framebuffer>(fbSpec);

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

        BuildScene();

        m_HierarchyPanel.SetContext(m_Scene.get());
        m_HierarchyPanel.SetAvailableTextures(m_Checkerboard, m_Circle);
        m_HierarchyPanel.SetSelectedEntity(m_PlayerEntity);

        FX_INFO("");
        FX_INFO("Editor hazir. Panelleri surukleyerek yeniden duzenleyebilirsin;");
        FX_INFO("duzen imgui.ini'ye kaydedilir.");
        FX_INFO("Viewport uzerindeyken: W/A/S/D kamera, tekerlek zoom.");
    }

    void EditorApp::BuildScene()
    {
        m_Scene = std::make_unique<FX::Scene>();

        {
            auto floor = m_Scene->CreateEntity("Zemin");
            auto& tf = floor.GetComponent<FX::TransformComponent>();
            tf.Translation = { 0.0f, 0.0f, -0.5f };
            tf.Scale       = { 30.0f, 30.0f };

            auto& sprite = floor.AddComponent<FX::SpriteRendererComponent>(m_Checkerboard);
            sprite.TilingFactor = 15.0f;
            sprite.Color = { 0.55f, 0.58f, 0.70f, 1.0f };
        }

        {
            m_PlayerEntity = m_Scene->CreateEntity("Oyuncu");
            auto& tf = m_PlayerEntity.GetComponent<FX::TransformComponent>();
            tf.Translation = { 0.0f, 0.0f, 0.1f };

            m_PlayerEntity.AddComponent<FX::SpriteRendererComponent>(
                m_Circle, glm::vec4{ 1.0f, 0.85f, 0.3f, 1.0f });
            m_PlayerEntity.AddComponent<FX::VelocityComponent>();
        }

        for (int i = 0; i < 8; ++i)
        {
            auto e = m_Scene->CreateEntity("Uydu " + std::to_string(i));

            const float angle = static_cast<float>(i) / 8.0f * 6.2831853f;
            auto& tf = e.GetComponent<FX::TransformComponent>();
            tf.Translation = { std::cos(angle) * 4.0f, std::sin(angle) * 4.0f, 0.0f };
            tf.Scale       = { 0.6f, 0.6f };
            tf.Rotation    = angle;

            e.AddComponent<FX::SpriteRendererComponent>(
                m_Checkerboard,
                glm::vec4{ 0.4f + RandFloat(0.0f, 0.6f), 0.4f + RandFloat(0.0f, 0.6f), 0.9f, 1.0f });
            e.AddComponent<FX::VelocityComponent>(glm::vec2{ 0.0f, 0.0f }, 1.2f);
        }

        for (int i = 0; i < 15; ++i)
            SpawnMover();

        FX_INFO("Sahne kuruldu: %u entity", m_Scene->GetEntityCount());
    }

    void EditorApp::SpawnMover()
    {
        auto e = m_Scene->CreateEntity("Gezgin");

        auto& tf = e.GetComponent<FX::TransformComponent>();
        tf.Translation = { RandFloat(-8.0f, 8.0f), RandFloat(-8.0f, 8.0f), 0.0f };
        const float s = RandFloat(0.2f, 0.6f);
        tf.Scale = { s, s };

        if (RandFloat(0.0f, 1.0f) > 0.5f)
            e.AddComponent<FX::SpriteRendererComponent>(
                m_Circle, glm::vec4{ RandFloat(0.3f, 1.0f), RandFloat(0.3f, 1.0f),
                                     RandFloat(0.3f, 1.0f), 0.9f });
        else
            e.AddComponent<FX::SpriteRendererComponent>(
                glm::vec4{ RandFloat(0.3f, 1.0f), RandFloat(0.3f, 1.0f),
                           RandFloat(0.3f, 1.0f), 0.9f });

        e.AddComponent<FX::VelocityComponent>(
            glm::vec2{ RandFloat(-1.5f, 1.5f), RandFloat(-1.5f, 1.5f) },
            RandFloat(-2.0f, 2.0f));
    }

    void EditorApp::UpdateCameraProjection()
    {
        // ARTIK PENCEREYE DEGIL, VIEWPORT PANELINE gore hesapliyoruz.
        // Paneller yer kapladigi icin viewport pencereden kucuktur;
        // pencereyi kullansaydik sahne yamuk gorunurdu.
        if (m_ViewportSize.y <= 0.0f)
            return;
        m_Camera->SetProjectionFromAspect(m_ViewportSize.x / m_ViewportSize.y, m_ZoomLevel);
    }

    void EditorApp::OnWindowResize(std::uint32_t, std::uint32_t)
    {
        // Framebuffer ve kamera artik viewport paneline bagli.
        // Pencere boyutu degisince panel de degisecek ve
        // DrawViewportPanel bunu yakalayacak - burada is yok.
    }

    void EditorApp::UpdateCameraMovement(float dt)
    {
        // ---------------------------------------------------------------
        // IKI KADEMELI KONTROL:
        //   1) ImGui klavyeyi istiyorsa (metin kutusuna yaziliyor) -> dur
        //   2) Fare viewport uzerinde degilse -> dur
        //
        // Birincisi olmadan, entity adini "Wall" yazmaya calisirken
        // 'W' kamerayi oynatir. ImGui kullanan uygulamalarda en sik
        // yapilan hata budur.
        // ---------------------------------------------------------------
        if (m_ImGuiLayer.WantsKeyboard())
            return;
        if (!m_ViewportHovered && !m_ViewportFocused)
            return;

        const bool* keys = SDL_GetKeyboardState(nullptr);

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

            const float move = m_CameraMoveSpeed * dt;
            const float rad  = glm::radians(m_CameraRotation);
            const float cs = std::cos(rad), sn = std::sin(rad);

            m_CameraPosition.x += (dx * cs - dy * sn) * move;
            m_CameraPosition.y += (dx * sn + dy * cs) * move;
        }

        if (keys[SDL_SCANCODE_Q]) m_CameraRotation += 90.0f * dt;
        if (keys[SDL_SCANCODE_E]) m_CameraRotation -= 90.0f * dt;

        m_Camera->SetPosition(m_CameraPosition);
        m_Camera->SetRotation(m_CameraRotation);
    }

    void EditorApp::OnUpdate(float dt)
    {
        m_Time += dt;

        UpdateCameraMovement(dt);

        // Duraklatma: editorde nesneleri incelemek icin sahneyi
        // dondurabilmek sart. Inspector'da bir degeri surukleyip
        // etkisini gormek isterken nesnenin kacmasi istenmez.
        if (!m_ScenePaused)
            m_Scene->OnUpdate(dt);
    }

    void EditorApp::OnRender(float /*alpha*/)
    {
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

        // ===================================================================
        // 1) SAHNEYI FRAMEBUFFER'A CIZ (ekrana degil)
        // ===================================================================
        m_Framebuffer->Bind();

        FX::RenderCommand::SetClearColor({ 0.07f, 0.08f, 0.11f, 1.0f });
        FX::RenderCommand::Clear();
        FX::Renderer2D::ResetStats();

        m_Scene->OnRender(*m_Camera);

        m_Framebuffer->Unbind();

        // ===================================================================
        // 2) EKRANI TEMIZLE, ARAYUZU CIZ
        // ===================================================================
        // Viewport'u pencere boyutuna geri al: framebuffer::Bind onu
        // kendi boyutuna ayarlamisti, ImGui tum pencereye cizecek.
        FX::RenderCommand::SetViewport(0, 0, GetWindow().GetWidth(), GetWindow().GetHeight());
        FX::RenderCommand::SetClearColor({ 0.05f, 0.05f, 0.06f, 1.0f });
        FX::RenderCommand::Clear();

        m_ImGuiLayer.Begin();

        DrawMenuBar();
        DrawViewportPanel();
        DrawStatsPanel();
        m_HierarchyPanel.OnImGuiRender();

        if (m_ShowDemoWindow)
            ImGui::ShowDemoWindow(&m_ShowDemoWindow);

        m_ImGuiLayer.End(GetWindow().GetWidth(), GetWindow().GetHeight());
    }

    void EditorApp::DrawMenuBar()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Dosya"))
            {
                // Faz 7'de burasi gercek kaydet/yukle olacak.
                ImGui::MenuItem("Sahneyi Kaydet", nullptr, false, false);
                ImGui::MenuItem("Sahneyi Yukle",  nullptr, false, false);
                ImGui::Separator();
                if (ImGui::MenuItem("Cikis"))
                    Close();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Sahne"))
            {
                if (ImGui::MenuItem(m_ScenePaused ? "Devam Et" : "Duraklat"))
                    m_ScenePaused = !m_ScenePaused;

                if (ImGui::MenuItem("Entity Ekle (10)"))
                {
                    for (int i = 0; i < 10; ++i)
                        SpawnMover();
                }

                if (ImGui::MenuItem("Sahneyi Sifirla"))
                {
                    BuildScene();
                    m_HierarchyPanel.SetContext(m_Scene.get());
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Gorunum"))
            {
                if (ImGui::MenuItem("Kamerayi Sifirla"))
                {
                    m_CameraPosition = { 0.0f, 0.0f, 0.0f };
                    m_CameraRotation = 0.0f;
                    m_ZoomLevel      = 8.0f;
                    UpdateCameraProjection();
                }
                ImGui::MenuItem("ImGui Demo", nullptr, &m_ShowDemoWindow);
                ImGui::EndMenu();
            }

            // Sagda durum bilgisi
            ImGui::SameLine(ImGui::GetWindowWidth() - 220.0f);
            ImGui::TextDisabled("%s | %.0f FPS",
                                m_ScenePaused ? "DURAKLATILDI" : "calisiyor",
                                m_CurrentFps);

            ImGui::EndMainMenuBar();
        }
    }

    void EditorApp::DrawViewportPanel()
    {
        // Viewport panelinde kenar boslugu ISTEMIYORUZ - goruntu
        // panelin tamamini kaplasin.
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Viewport");

        m_ViewportFocused = ImGui::IsWindowFocused();
        m_ViewportHovered = ImGui::IsWindowHovered();

        const ImVec2 panelSize = ImGui::GetContentRegionAvail();

        // Panel boyutu degistiyse framebuffer'i ve kamerayi guncelle.
        // Framebuffer::Resize kendi icinde "ayni boyutsa hicbir sey yapma"
        // kontrolu yapiyor, o yuzden her karede cagirmak guvenli.
        if (panelSize.x > 0.0f && panelSize.y > 0.0f &&
            (m_ViewportSize.x != panelSize.x || m_ViewportSize.y != panelSize.y))
        {
            m_ViewportSize = { panelSize.x, panelSize.y };
            m_Framebuffer->Resize(static_cast<std::uint32_t>(panelSize.x),
                                  static_cast<std::uint32_t>(panelSize.y));
            UpdateCameraProjection();
        }

        // ===============================================================
        // Framebuffer texture'ini panelde goster.
        //
        // UV'LER TERS: (0,1) -> (1,0)
        // Sebep: OpenGL texture'lari SOL ALT kokenlidir, ImGui ise
        // SOL UST bekler. Duzeltmezsek sahne BAS ASAGI gorunur.
        // Faz 3'te stb_image icin yaptigimiz cevirmenin ayni mantigi,
        // bu sefer diger yonde.
        // ===============================================================
        const auto texID = static_cast<ImTextureID>(m_Framebuffer->GetColorAttachmentID());
        ImGui::Image(texID, panelSize, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

        ImGui::End();
        ImGui::PopStyleVar();
    }

    void EditorApp::DrawStatsPanel()
    {
        ImGui::Begin("Istatistikler");

        const auto stats = FX::Renderer2D::GetStats();

        ImGui::Text("Render");
        ImGui::Separator();
        ImGui::Text("Draw call   : %u", stats.DrawCalls);
        ImGui::Text("Quad        : %u", stats.QuadCount);
        ImGui::Text("Kose        : %u", stats.GetVertexCount());
        ImGui::Text("FPS         : %.0f", m_CurrentFps);

        ImGui::Spacing();
        ImGui::Text("Sahne");
        ImGui::Separator();
        ImGui::Text("Entity      : %u", m_Scene->GetEntityCount());
        ImGui::Text("Durum       : %s", m_ScenePaused ? "duraklatildi" : "calisiyor");

        ImGui::Spacing();
        ImGui::Text("Viewport");
        ImGui::Separator();
        ImGui::Text("Boyut       : %.0f x %.0f", m_ViewportSize.x, m_ViewportSize.y);
        ImGui::Text("Pencere     : %u x %u", GetWindow().GetWidth(), GetWindow().GetHeight());
        ImGui::Text("Odakta      : %s", m_ViewportFocused ? "evet" : "hayir");

        ImGui::Spacing();
        ImGui::Text("Kamera");
        ImGui::Separator();
        ImGui::Text("Konum       : (%.2f, %.2f)", m_CameraPosition.x, m_CameraPosition.y);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::SliderFloat("##Zoom", &m_ZoomLevel, 1.0f, 40.0f, "Zoom %.1f"))
            UpdateCameraProjection();

        ImGui::Spacing();
        bool vsync = GetWindow().IsVSync();
        if (ImGui::Checkbox("VSync", &vsync))
            GetWindow().SetVSync(vsync);

        ImGui::End();
    }

    void EditorApp::OnEvent(const SDL_Event& event)
    {
        // ImGui once gorsun. true donerse olayi tuketmis demektir.
        if (m_ImGuiLayer.OnEvent(event))
            return;

        if (event.type == SDL_EVENT_MOUSE_WHEEL)
        {
            // Zoom sadece viewport uzerindeyken. Panellerde tekerlek
            // kaydirma icin kullanilmali.
            if (m_ViewportHovered)
            {
                m_ZoomLevel *= (event.wheel.y > 0) ? 0.9f : 1.1f;
                m_ZoomLevel = glm::clamp(m_ZoomLevel, 1.0f, 40.0f);
                UpdateCameraProjection();
            }
            return;
        }

        if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat)
            return;

        switch (event.key.key)
        {
            case SDLK_ESCAPE:
                Close();
                break;

            case SDLK_SPACE:
                m_ScenePaused = !m_ScenePaused;
                break;

            default:
                break;
        }
    }

    void EditorApp::OnShutdown()
    {
        // SIRA ONEMLI: once ImGui (GL kaynaklarini birakir),
        // sonra framebuffer ve sahne, en son renderer.
        m_ImGuiLayer.Shutdown();
        m_Framebuffer.reset();
        m_Scene.reset();
        FX::Renderer2D::Shutdown();

        FX_INFO("Editor kapaniyor.");
    }
}
