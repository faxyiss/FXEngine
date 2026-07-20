#include "EditorApp.h"

#include <FXEngine/Core/Log.h>
#include <FXEngine/Core/FileSystem.h>
#include <FXEngine/Renderer/RenderCommand.h>
#include <FXEngine/Renderer/Renderer2D.h>
#include <FXEngine/Scene/Components.h>
#include <FXEngine/Scene/SceneSerializer.h>

#include <imgui.h>
#include <ImGuizmo.h>

#include <glm/gtc/type_ptr.hpp>

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

        // Iki renk eki: gorunen goruntu + entity ID (secim icin).
        FX::FramebufferSpec fbSpec;
        fbSpec.Width  = GetWindow().GetWidth();
        fbSpec.Height = GetWindow().GetHeight();
        fbSpec.Attachments = {
            FX::FramebufferTextureFormat::RGBA8,
            FX::FramebufferTextureFormat::RED_INTEGER,
            FX::FramebufferTextureFormat::DEPTH24STENCIL8
        };
        m_Framebuffer = std::make_unique<FX::Framebuffer>(fbSpec);

        FX::TextureSpec sharpSpec;
        sharpSpec.MinFilter = FX::TextureFilter::Nearest;
        sharpSpec.MagFilter = FX::TextureFilter::Nearest;
        sharpSpec.WrapS     = FX::TextureWrap::Repeat;
        sharpSpec.WrapT     = FX::TextureWrap::Repeat;

        // ARTIK KUTUPHANE UZERINDEN yukluyoruz.
        // Onemli: sahne yuklerken serializer de ayni kutuphaneyi kullanacak.
        // Boylece "assets/textures/circle.png" yolu her iki yolda da AYNI
        // texture nesnesine cozulur - Inspector'daki texture secimi
        // isaretci karsilastirmasiyla calistigi icin bu sart.
        m_Checkerboard = m_TextureLibrary.Load("assets/textures/checkerboard.png", sharpSpec);
        m_Circle       = m_TextureLibrary.Load("assets/textures/circle.png");

        if (!m_Checkerboard || !m_Circle)
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
        m_HierarchyPanel.SetSelectedEntity(GetPlayer());

        FX_INFO("");
        FX_INFO("Editor hazir. Panelleri surukleyerek yeniden duzenleyebilirsin;");
        FX_INFO("duzen imgui.ini'ye kaydedilir.");
        FX_INFO("Viewport uzerindeyken: W/A/S/D kamera, tekerlek zoom.");
        FX_INFO("Ctrl+S kaydet, Ctrl+O yukle. Sahne dosyasi: %s", m_ScenePath.c_str());
        FX_INFO("Viewport'ta sol tik ile entity sec.");
        FX_INFO("Gizmo: Z kapali, X tasi, C dondur, B olcekle (Ctrl = kademeli)");
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
            auto player = m_Scene->CreateEntity("Oyuncu");
            auto& tf = player.GetComponent<FX::TransformComponent>();
            tf.Translation = { 0.0f, 0.0f, 0.1f };

            player.AddComponent<FX::SpriteRendererComponent>(
                m_Circle, glm::vec4{ 1.0f, 0.85f, 0.3f, 1.0f });
            player.AddComponent<FX::VelocityComponent>();

            // Tutamak degil KIMLIK sakliyoruz - yuklemeden sag cikar.
            m_PlayerUUID = player.GetComponent<FX::IDComponent>().ID;
        }

        // --- Takipciler --------------------------------------------------------
        // Faz 8'in kanit sahnesi: bu uc entity oyuncuyu UUID uzerinden
        // takip ediyor. Sahneyi kaydedip yukledikten sonra da takip
        // ETMEYE DEVAM etmeliler - iste kalici kimligin butun mesele.
        for (int i = 0; i < 3; ++i)
        {
            auto e = m_Scene->CreateEntity("Takipci " + std::to_string(i));

            auto& tf = e.GetComponent<FX::TransformComponent>();
            tf.Translation = { -6.0f + static_cast<float>(i) * 1.5f, -6.0f, 0.05f };
            tf.Scale = { 0.7f, 0.7f };

            e.AddComponent<FX::SpriteRendererComponent>(
                m_Circle, glm::vec4{ 1.0f, 0.35f, 0.35f, 1.0f });
            e.AddComponent<FX::VelocityComponent>();

            auto& fc = e.AddComponent<FX::FollowComponent>();
            fc.Target       = m_PlayerUUID;
            fc.Speed        = 1.5f + static_cast<float>(i) * 0.5f;
            fc.StopDistance = 1.0f + static_cast<float>(i) * 0.6f;
        }

        // --- Hiyerarsi ornegi ---------------------------------------------------
        // Govde doner; kule ve namlu onun cocugu oldugu icin birlikte doner.
        // Kule kendi de doner -> namlu iki donmeyi birden alir.
        {
            auto body = m_Scene->CreateEntity("Tank Govde");
            auto& btf = body.GetComponent<FX::TransformComponent>();
            btf.Translation = { -6.0f, 4.0f, 0.2f };
            btf.Scale       = { 2.0f, 2.0f };
            body.AddComponent<FX::SpriteRendererComponent>(
                m_Checkerboard, glm::vec4{ 0.45f, 0.75f, 0.45f, 1.0f });
            body.AddComponent<FX::VelocityComponent>(glm::vec2{ 0.0f, 0.0f }, 0.4f);

            auto turret = m_Scene->CreateEntity("Tank Kule");
            auto& ttf = turret.GetComponent<FX::TransformComponent>();
            ttf.Translation = { 0.0f, 0.0f, 0.05f };
            ttf.Scale       = { 0.55f, 0.55f };
            turret.AddComponent<FX::SpriteRendererComponent>(
                m_Circle, glm::vec4{ 1.0f, 0.9f, 0.4f, 1.0f });
            turret.AddComponent<FX::VelocityComponent>(glm::vec2{ 0.0f, 0.0f }, 1.5f);
            turret.SetParent(body);

            auto barrel = m_Scene->CreateEntity("Tank Namlu");
            auto& natf = barrel.GetComponent<FX::TransformComponent>();
            natf.Translation = { 0.9f, 0.0f, 0.05f };
            natf.Scale       = { 1.4f, 0.35f };
            barrel.AddComponent<FX::SpriteRendererComponent>(
                glm::vec4{ 0.9f, 0.35f, 0.25f, 1.0f });
            barrel.SetParent(turret);
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

    FX::Entity EditorApp::GetPlayer()
    {
        if (!m_Scene || !m_PlayerUUID.IsValid())
            return {};

        // O(1) harita aramasi. Her karede cagirmak sorun degil ve
        // tutamak onbelleklemenin aksine GUVENLI: oyuncu silinmisse
        // gecersiz Entity doner, biz de fark ederiz.
        return m_Scene->FindEntityByUUID(m_PlayerUUID);
    }

    void EditorApp::SaveScene()
    {
        FX::SceneSerializer serializer(m_Scene.get(), &m_TextureLibrary);

        if (serializer.Serialize(m_ScenePath))
            m_StatusMessage = "Sahne kaydedildi: " + m_ScenePath;
        else
            m_StatusMessage = "KAYDEDILEMEDI! (assets/scenes/ klasoru var mi?)";

        m_StatusTimer = 4.0f;
    }

    void EditorApp::LoadScene()
    {
        FX::SceneSerializer serializer(m_Scene.get(), &m_TextureLibrary);

        if (serializer.Deserialize(m_ScenePath))
        {
            m_StatusMessage = "Sahne yuklendi: " + m_ScenePath;

            // ===============================================================
            // FAZ 8 FARKI:
            //
            // Faz 7'de burada "m_PlayerEntity = {}" yazmak ZORUNDAYDIK.
            // Tutamaklar yuklemeden sonra gecersizdi ama GECERLI GORUNUYORDU
            // (EnTT kimlikleri geri donusturur), dolayisiyla korlemesine
            // temizlemek tek guvenli secenekti - ve oyuncuya erisimi
            // kaybediyorduk.
            //
            // Artik m_PlayerUUID sakliyoruz. Kimlik dosyada yaziyor, ayni
            // degerle geri geliyor; hicbir sey temizlemeye gerek yok.
            // Oyuncu, sahne yuklendikten sonra da bulunabilir durumda.
            //
            // Panel secimini yine de temizliyoruz: secili entity KULLANICI
            // tercihiydi, yeni sahnede karsiligi olmayabilir. Bu, teknik
            // bir zorunluluk degil, davranissal bir tercih.
            // ===============================================================
            m_HierarchyPanel.SetContext(m_Scene.get());

            if (auto player = GetPlayer())
                FX_INFO("Oyuncu yuklemeden sonra UUID ile bulundu: %llu",
                        static_cast<unsigned long long>(m_PlayerUUID));
            else
                FX_WARN("Oyuncu bulunamadi - sahne dosyasinda yok olabilir.");
        }
        else
        {
            m_StatusMessage = "YUKLENEMEDI! Once Ctrl+S ile kaydet.";
        }

        m_StatusTimer = 4.0f;
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

        if (m_StatusTimer > 0.0f)
            m_StatusTimer -= dt;

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
        // Viewport paneli buyumus/kucultulmusse framebuffer'i BURADA,
        // ImGui cercevesi acilmadan once yeniden olustur.
        if (m_ViewportSize.x > 0.0f && m_ViewportSize.y > 0.0f)
        {
            const auto w = static_cast<std::uint32_t>(m_ViewportSize.x);
            const auto h = static_cast<std::uint32_t>(m_ViewportSize.y);
            const auto& spec = m_Framebuffer->GetSpec();

            if (spec.Width != w || spec.Height != h)
            {
                m_Framebuffer->Resize(w, h);
                UpdateCameraProjection();
            }
        }

        m_Framebuffer->Bind();

        FX::RenderCommand::SetClearColor({ 0.07f, 0.08f, 0.11f, 1.0f });
        FX::RenderCommand::Clear();

        // ID ekini -1'e doldur. glClear onu 0 yapardi ve 0 gecerli bir
        // entity kimligi - bos alana tiklayinca ilk entity secilirdi.
        m_Framebuffer->ClearAttachment(1, -1);

        FX::Renderer2D::ResetStats();

        m_Scene->OnRender(*m_Camera);

        PickEntity();

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
                if (ImGui::MenuItem("Sahneyi Kaydet", "Ctrl+S"))
                    SaveScene();
                if (ImGui::MenuItem("Sahneyi Yukle", "Ctrl+O"))
                    LoadScene();
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

            if (ImGui::BeginMenu("Gizmo"))
            {
                if (ImGui::MenuItem("Kapali",  "Z", m_GizmoOperation == -1))
                    m_GizmoOperation = -1;
                if (ImGui::MenuItem("Tasi",    "X", m_GizmoOperation == ImGuizmo::TRANSLATE))
                    m_GizmoOperation = ImGuizmo::TRANSLATE;
                if (ImGui::MenuItem("Dondur",  "C", m_GizmoOperation == ImGuizmo::ROTATE))
                    m_GizmoOperation = ImGuizmo::ROTATE;
                if (ImGui::MenuItem("Olcekle", "B", m_GizmoOperation == ImGuizmo::SCALE))
                    m_GizmoOperation = ImGuizmo::SCALE;
                ImGui::Separator();
                ImGui::TextDisabled("Ctrl basili = kademeli");
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
                if (ImGui::MenuItem("Panel Duzenini Sifirla"))
                    m_ImGuiLayer.ResetLayout();
                ImGui::MenuItem("ImGui Demo", nullptr, &m_ShowDemoWindow);
                ImGui::EndMenu();
            }

            // Kaydet/yukle geri bildirimi - birkac saniye gorunur kalir.
            // Sessiz basari, kullaniciya "oldu mu?" diye sorduran kotu
            // bir tasarimdir.
            if (m_StatusTimer > 0.0f)
            {
                ImGui::SameLine(ImGui::GetWindowWidth() - 620.0f);
                ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.6f, 1.0f), "%s",
                                   m_StatusMessage.c_str());
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

        // Panelin ekran uzerindeki sinirlari. GetCursorScreenPos, icerik
        // alaninin sol UST kosesini verir - baslik cubugu haric.
        const ImVec2 contentMin = ImGui::GetCursorScreenPos();
        const ImVec2 panelSize  = ImGui::GetContentRegionAvail();

        m_ViewportBoundsMin = { contentMin.x, contentMin.y };
        m_ViewportBoundsMax = { contentMin.x + panelSize.x, contentMin.y + panelSize.y };

        // Sadece KAYDEDIYORUZ. Framebuffer'i burada yeniden olusturmak,
        // ImGui cercevesinin ortasinda texture silmek demek: ImGui o
        // kare icinde eski kimlige hala atifta bulunabiliyor ve
        // "Texture name does not refer to a texture object" hatasi aliniyor.
        // Yeniden boyutlandirma OnRender'in basinda, cerceve acilmadan once.
        if (panelSize.x > 0.0f && panelSize.y > 0.0f)
            m_ViewportSize = { panelSize.x, panelSize.y };

        // ===============================================================
        // Framebuffer texture'ini panelde goster.
        //
        // UV'LER TERS: (0,1) -> (1,0)
        // Sebep: OpenGL texture'lari SOL ALT kokenlidir, ImGui ise
        // SOL UST bekler. Duzeltmezsek sahne BAS ASAGI gorunur.
        // Faz 3'te stb_image icin yaptigimiz cevirmenin ayni mantigi,
        // bu sefer diger yonde.
        // ===============================================================
        const auto texID = static_cast<ImTextureID>(m_Framebuffer->GetColorAttachmentID(0));
        ImGui::Image(texID, panelSize, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

        DrawGizmo();

        ImGui::End();
        ImGui::PopStyleVar();
    }

    void EditorApp::PickEntity()
    {
        // Framebuffer BAGLI durumdayken cagriliyor (OnRender icinde).
        if (!m_ViewportHovered || ImGuizmo::IsOver() || ImGuizmo::IsUsing())
            return;

        if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            return;

        const ImVec2 mouse = ImGui::GetMousePos();

        float mx = mouse.x - m_ViewportBoundsMin.x;
        float my = mouse.y - m_ViewportBoundsMin.y;

        // OpenGL'in Y ekseni asagidan yukari; ImGui'nin yukaridan asagi.
        const float height = m_ViewportBoundsMax.y - m_ViewportBoundsMin.y;
        my = height - my;

        const int pixel = m_Framebuffer->ReadPixel(1, static_cast<int>(mx), static_cast<int>(my));

        if (pixel < 0)
        {
            m_HierarchyPanel.SetSelectedEntity({});
            return;
        }

        const auto handle = static_cast<entt::entity>(pixel);
        if (m_Scene->GetRegistry().valid(handle))
        {
            FX::Entity picked{ handle, m_Scene.get() };
            m_HierarchyPanel.SetSelectedEntity(picked);
            FX_INFO("Secildi: %s", picked.GetComponent<FX::TagComponent>().Tag.c_str());
        }
    }

    void EditorApp::DrawGizmo()
    {
        FX::Entity selected = m_HierarchyPanel.GetSelectedEntity();
        if (!selected || m_GizmoOperation < 0)
            return;
        if (!selected.HasComponent<FX::TransformComponent>())
            return;

        ImGuizmo::SetOrthographic(true);
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(m_ViewportBoundsMin.x, m_ViewportBoundsMin.y,
                          m_ViewportBoundsMax.x - m_ViewportBoundsMin.x,
                          m_ViewportBoundsMax.y - m_ViewportBoundsMin.y);

        const glm::mat4& view = m_Camera->GetViewMatrix();
        const glm::mat4& proj = m_Camera->GetProjectionMatrix();

        auto& tc = selected.GetComponent<FX::TransformComponent>();

        // Gizmo DUNYA uzayinda calisir. Parent'i olan bir entity'nin
        // yerel matrisini verirsek tutamaklar yanlis yerde cikar.
        glm::mat4 parentWorld{ 1.0f };
        if (FX::Entity parent = selected.GetParent())
        {
            if (parent.HasComponent<FX::WorldTransformComponent>())
                parentWorld = parent.GetComponent<FX::WorldTransformComponent>().Matrix;
        }

        glm::mat4 transform = parentWorld * tc.GetTransform();

        // Ctrl basiliyken kademeli hareket (snap).
        const bool* keys = SDL_GetKeyboardState(nullptr);
        const bool snap = keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL];

        const float snapValue =
            (m_GizmoOperation == ImGuizmo::ROTATE) ? 15.0f : 0.5f;
        const float snapValues[3] = { snapValue, snapValue, snapValue };

        ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
                             static_cast<ImGuizmo::OPERATION>(m_GizmoOperation),
                             ImGuizmo::LOCAL,
                             glm::value_ptr(transform),
                             nullptr,
                             snap ? snapValues : nullptr);

        if (ImGuizmo::IsUsing())
        {
            // Dunya -> yerel: parent'in tersiyle carp.
            const glm::mat4 local = glm::inverse(parentWorld) * transform;

            float translation[3], rotation[3], scale[3];
            ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(local),
                                                  translation, rotation, scale);

            tc.Translation = { translation[0], translation[1], translation[2] };
            tc.Rotation    = glm::radians(rotation[2]);
            tc.Scale       = { scale[0], scale[1] };
        }
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

        // Ctrl basili mi? SDL3'te modifier durumu event.key.mod'da gelir.
        // KMOD_CTRL, sol ve sag Ctrl'un ikisini birden kapsar.
        const bool ctrl = (event.key.mod & SDL_KMOD_CTRL) != 0;

        switch (event.key.key)
        {
            case SDLK_ESCAPE:
                Close();
                break;

            case SDLK_SPACE:
                m_ScenePaused = !m_ScenePaused;
                break;

            case SDLK_S:
                if (ctrl) SaveScene();
                break;

            case SDLK_O:
                if (ctrl) LoadScene();
                break;

            // Gizmo kisayollari - sadece viewport odaktayken, cunku
            // W/E/R ayni zamanda kamera tuslari.
            case SDLK_Z:
                if (m_ViewportHovered) m_GizmoOperation = -1;
                break;
            case SDLK_X:
                if (m_ViewportHovered) m_GizmoOperation = ImGuizmo::TRANSLATE;
                break;
            case SDLK_C:
                if (m_ViewportHovered) m_GizmoOperation = ImGuizmo::ROTATE;
                break;
            case SDLK_B:
                if (m_ViewportHovered) m_GizmoOperation = ImGuizmo::SCALE;
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
