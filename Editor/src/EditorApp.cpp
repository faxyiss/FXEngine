#include "EditorApp.h"

#include <FXEngine/Core/Log.h>
#include <FXEngine/Core/FileSystem.h>
#include <FXEngine/Renderer/RenderCommand.h>
#include <FXEngine/Renderer/Renderer2D.h>
#include <FXEngine/Scene/Components.h>

#include <SDL3/SDL.h>

#include <cmath>
#include <random>

namespace FXEd
{
    namespace
    {
        // Tekrarlanabilir rastgelelik. Sabit tohum (seed) kullaniyoruz ki
        // her calistirmada AYNI sahne olussun - hata ayiklarken "bu sefer
        // farkli cikti" derdinden kurtulmak icin.
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
              props.Title     = "FXEditor - Faz 5 (ECS)";
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
        FX_INFO("  FXEditor - Faz 5: EnTT / ECS");
        FX_INFO("=====================================");

        FX::RenderCommand::Init();
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

        BuildScene();

        FX_INFO("");
        FX_INFO("Kontroller:");
        FX_INFO("  W/A/S/D  -> OYUNCUYU hareket ettir (C ile kameraya gecer)");
        FX_INFO("  C        -> kontrol: oyuncu <-> kamera");
        FX_INFO("  Q/E      -> kamerayi dondur");
        FX_INFO("  Tekerlek -> zoom");
        FX_INFO("  R        -> kamerayi sifirla");
        FX_INFO("  N        -> yeni hareketli entity ekle (10 tane)");
        FX_INFO("  H        -> oyuncunun SpriteRenderer'ini ekle/kaldir  <-- ECS testi");
        FX_INFO("  M        -> oyuncunun Velocity'sini ekle/kaldir       <-- ECS testi");
        FX_INFO("  T        -> tel kafes");
        FX_INFO("  V        -> VSync");
        FX_INFO("  TAB      -> istatistikler");
        FX_INFO("  ESC      -> cikis");
    }

    void EditorApp::BuildScene()
    {
        m_Scene = std::make_unique<FX::Scene>();

        // ===================================================================
        // DIKKAT: Burada hicbir cizim cagrisi YOK.
        // Sadece VERI kuruyoruz. Ne zaman ve nasil cizilecegi
        // SpriteRenderSystem'in isi; nasil hareket edecegi MovementSystem'in.
        // "Veri component'te, davranis system'de" kurali tam olarak bu.
        // ===================================================================

        // --- Zemin (hareketsiz) ---------------------------------------------
        // Velocity component'i YOK -> MovementSystem bu entity'ye
        // hic dokunmaz, uzerinden gecmez bile.
        {
            auto floor = m_Scene->CreateEntity("Zemin");

            auto& tf = floor.GetComponent<FX::TransformComponent>();
            tf.Translation = { 0.0f, 0.0f, -0.5f };   // arkada
            tf.Scale       = { 30.0f, 30.0f };

            auto& sprite = floor.AddComponent<FX::SpriteRendererComponent>(m_Checkerboard);
            sprite.TilingFactor = 15.0f;
            sprite.Color = { 0.55f, 0.58f, 0.70f, 1.0f };
        }

        // --- Oyuncu ----------------------------------------------------------
        {
            m_PlayerEntity = m_Scene->CreateEntity("Oyuncu");

            auto& tf = m_PlayerEntity.GetComponent<FX::TransformComponent>();
            tf.Translation = { 0.0f, 0.0f, 0.1f };
            tf.Scale       = { 1.0f, 1.0f };

            m_PlayerEntity.AddComponent<FX::SpriteRendererComponent>(
                m_Circle, glm::vec4{ 1.0f, 0.85f, 0.3f, 1.0f });

            // Oyuncuya Velocity ekliyoruz ama sifir - klavye bunu dolduracak.
            // Boylece MovementSystem oyuncuyu da ISLER; oyuncu ozel bir
            // durum degil, sadece velocity'si disaridan yazilan bir entity.
            m_PlayerEntity.AddComponent<FX::VelocityComponent>();
        }

        // --- Yorungede donen uydular -----------------------------------------
        // Hepsi ayni component setine sahip; farklari sadece VERI.
        // OOP olsaydi bunlar icin bir "Satellite" sinifi yazardik.
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
                glm::vec4{ 0.4f + RandFloat(0.0f, 0.6f),
                           0.4f + RandFloat(0.0f, 0.6f),
                           0.9f, 1.0f });

            // Sadece donerler, yer degistirmezler.
            e.AddComponent<FX::VelocityComponent>(glm::vec2{ 0.0f, 0.0f }, 1.2f);
        }

        // --- Serbest dolasan entity'ler ---------------------------------------
        for (int i = 0; i < 30; ++i)
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

        // Rastgele: yarisi texture'li, yarisi duz renk.
        // Ikisi de AYNI batch'e girer (Renderer2D beyaz texture kullaniyor).
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

        float dx = 0.0f, dy = 0.0f;
        if (keys[SDL_SCANCODE_A]) dx -= 1.0f;
        if (keys[SDL_SCANCODE_D]) dx += 1.0f;
        if (keys[SDL_SCANCODE_S]) dy -= 1.0f;
        if (keys[SDL_SCANCODE_W]) dy += 1.0f;

        const float len = std::sqrt(dx * dx + dy * dy);
        if (len > 0.0f) { dx /= len; dy /= len; }

        if (m_PlayerControl)
        {
            // ===============================================================
            // ONEMLI: Oyuncunun konumunu DOGRUDAN degistirmiyoruz.
            // Sadece Velocity component'ine yaziyoruz; hareketi
            // MovementSystem yapiyor - tipki diger 38 entity gibi.
            //
            // Bunun anlami: oyuncu ozel bir sinif DEGIL. Girdi sistemi
            // veriyi yaziyor, hareket sistemi onu isliyor. Sistemler
            // birbirini bilmiyor, sadece ayni veriye dokunuyorlar.
            // ECS'te sistemler arasi iletisim BOYLE olur.
            // ===============================================================
            if (m_PlayerEntity && m_PlayerEntity.HasComponent<FX::VelocityComponent>())
            {
                auto& vel = m_PlayerEntity.GetComponent<FX::VelocityComponent>();
                vel.Linear = { dx * 5.0f, dy * 5.0f };
            }
        }
        else
        {
            const float move = m_CameraMoveSpeed * dt;
            const float rad  = glm::radians(m_CameraRotation);
            const float cs = std::cos(rad), sn = std::sin(rad);

            m_CameraPosition.x += (dx * cs - dy * sn) * move;
            m_CameraPosition.y += (dx * sn + dy * cs) * move;
        }

        const float rotate = m_CameraRotateSpeed * dt;
        if (keys[SDL_SCANCODE_Q]) m_CameraRotation += rotate;
        if (keys[SDL_SCANCODE_E]) m_CameraRotation -= rotate;

        m_Camera->SetPosition(m_CameraPosition);
        m_Camera->SetRotation(m_CameraRotation);
    }

    void EditorApp::OnUpdate(float dt)
    {
        m_Time += dt;

        UpdateCameraMovement(dt);

        // Tum oyun mantigi tek satir. Icinde MovementSystem calisiyor;
        // ileride Physics, Collision, Script sistemleri de buraya girecek
        // ve bu satir DEGISMEYECEK - sadece Scene::OnUpdate genisleyecek.
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

        FX::RenderCommand::SetClearColor({ 0.07f, 0.08f, 0.11f, 1.0f });
        FX::RenderCommand::Clear();

        FX::Renderer2D::ResetStats();

        // Cizim de tek satir. Faz 4'te burada 40 satirlik dongu vardi.
        m_Scene->OnRender(*m_Camera);
    }

    void EditorApp::LogStats()
    {
        const auto stats = FX::Renderer2D::GetStats();

        FX_INFO("--- Istatistikler ---");
        FX_INFO("  Entity sayisi  : %u", m_Scene->GetEntityCount());
        FX_INFO("  Cizilen quad   : %u", stats.QuadCount);
        FX_INFO("  Draw call      : %u", stats.DrawCalls);
        FX_INFO("  FPS            : %.0f%s", m_CurrentFps,
                GetWindow().IsVSync() ? "  (VSync acik)" : "");
        FX_INFO("  Kontrol        : %s", m_PlayerControl ? "OYUNCU" : "KAMERA");

        if (m_PlayerEntity)
        {
            const auto& tf = m_PlayerEntity.GetComponent<FX::TransformComponent>();
            FX_INFO("  Oyuncu konum   : (%.2f, %.2f)", tf.Translation.x, tf.Translation.y);
            FX_INFO("  Oyuncu sprite  : %s",
                    m_PlayerEntity.HasComponent<FX::SpriteRendererComponent>() ? "VAR" : "YOK");
            FX_INFO("  Oyuncu velocity: %s",
                    m_PlayerEntity.HasComponent<FX::VelocityComponent>() ? "VAR" : "YOK");
        }
    }

    void EditorApp::OnEvent(const SDL_Event& event)
    {
        if (event.type == SDL_EVENT_MOUSE_WHEEL)
        {
            m_ZoomLevel *= (event.wheel.y > 0) ? 0.9f : 1.1f;
            m_ZoomLevel = glm::clamp(m_ZoomLevel, 1.0f, 40.0f);
            UpdateCameraProjection();
            return;
        }

        if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat)
            return;

        switch (event.key.key)
        {
            case SDLK_ESCAPE:
                Close();
                break;

            case SDLK_C:
                m_PlayerControl = !m_PlayerControl;
                // Kameraya gecerken oyuncuyu durdur, yoksa son hizla
                // sonsuza kadar kayar.
                if (!m_PlayerControl && m_PlayerEntity &&
                    m_PlayerEntity.HasComponent<FX::VelocityComponent>())
                {
                    m_PlayerEntity.GetComponent<FX::VelocityComponent>().Linear = { 0.0f, 0.0f };
                }
                FX_INFO("Kontrol -> %s", m_PlayerControl ? "OYUNCU" : "KAMERA");
                break;

            case SDLK_N:
                // 10'ar ekliyoruz ki entity sayisini hizlica buyutup
                // batch renderer'in davranisini gozleyebilelim.
                for (int i = 0; i < 10; ++i)
                    SpawnMover();
                FX_INFO("10 entity eklendi. Toplam: %u", m_Scene->GetEntityCount());
                break;

            case SDLK_H:
            {
                // ===========================================================
                // ECS'IN KALBI BU TEST:
                // Bir component'i CALISMA ZAMANINDA ekleyip kaldiriyoruz.
                // Sprite kaldirilinca entity GORUNMEZ olur ama VAR OLMAYA
                // devam eder - hareket etmeyi surdurur (Velocity duruyor).
                //
                // OOP'de bunun karsiligi, nesnenin SINIFINI calisirken
                // degistirmek olurdu ki mumkun degildir.
                // ===========================================================
                if (!m_PlayerEntity) break;

                if (m_PlayerEntity.HasComponent<FX::SpriteRendererComponent>())
                {
                    m_PlayerEntity.RemoveComponent<FX::SpriteRendererComponent>();
                    FX_INFO("Oyuncunun SpriteRenderer'i KALDIRILDI -> gorunmez oldu");
                    FX_INFO("  ama hala var ve hareket ediyor (TAB ile konumuna bak)");
                }
                else
                {
                    m_PlayerEntity.AddComponent<FX::SpriteRendererComponent>(
                        m_Circle, glm::vec4{ 1.0f, 0.85f, 0.3f, 1.0f });
                    FX_INFO("Oyuncunun SpriteRenderer'i EKLENDI -> tekrar gorunur");
                }
                break;
            }

            case SDLK_M:
            {
                if (!m_PlayerEntity) break;

                if (m_PlayerEntity.HasComponent<FX::VelocityComponent>())
                {
                    m_PlayerEntity.RemoveComponent<FX::VelocityComponent>();
                    FX_INFO("Oyuncunun Velocity'si KALDIRILDI -> MovementSystem");
                    FX_INFO("  artik bu entity'yi hic gormuyor, WASD etkisiz");
                }
                else
                {
                    m_PlayerEntity.AddComponent<FX::VelocityComponent>();
                    FX_INFO("Velocity EKLENDI -> WASD tekrar calisiyor");
                }
                break;
            }

            case SDLK_R:
                m_CameraPosition = { 0.0f, 0.0f, 0.0f };
                m_CameraRotation = 0.0f;
                m_ZoomLevel      = 8.0f;
                UpdateCameraProjection();
                break;

            case SDLK_T:
                m_Wireframe = !m_Wireframe;
                FX::RenderCommand::SetWireframe(m_Wireframe);
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

    void EditorApp::OnShutdown()
    {
        // Sahneyi renderer'dan ONCE yikiyoruz: entity'ler texture
        // shared_ptr'lari tutuyor, once onlar birakilsin.
        m_Scene.reset();

        FX::Renderer2D::Shutdown();
        FX_INFO("Editor kapaniyor.");
    }
}
