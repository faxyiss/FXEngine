#include "EditorApp.h"

#include <FXEngine/Core/Log.h>
#include <FXEngine/Core/FileSystem.h>
#include <FXEngine/Renderer/RenderCommand.h>

#include <glm/gtc/matrix_transform.hpp>

#include <SDL3/SDL.h>

#include <cmath>

namespace FXEd
{
    EditorApp::EditorApp()
        : FX::Application([]() {
              FX::WindowProps props;
              props.Title     = "FXEditor - Faz 3";
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
        FX_INFO("  FXEditor - Faz 3");
        FX_INFO("=====================================");

        FX::RenderCommand::Init();

        // ===================================================================
        // 1) KOSE VERISI - artik UV koordinatlari da var
        // ===================================================================
        // Her kose 5 float: 3 pozisyon + 2 UV.
        //
        // Koordinatlar artik NDC degil, DUNYA BIRIMI. Kamera 5.0 zoom ile
        // dikeyde -5..+5 gosterecek, yani 1x1'lik bu quad ekranin onda biri
        // kadar yer kaplayacak. Artik "0.5" gibi soyut sayilarla degil,
        // anlamli birimlerle dusunebiliyoruz.
        //
        // UV yerlesimi:
        //   (0,1) ---- (1,1)      sol alt kose  = (0,0)
        //     |          |        sag ust kose  = (1,1)
        //   (0,0) ---- (1,0)
        const float vertices[] = {
            // x      y     z      u     v
            -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,   // 0: sol alt
            -0.5f,  0.5f, 0.0f,  0.0f, 1.0f,   // 1: sol ust
             0.5f,  0.5f, 0.0f,  1.0f, 1.0f,   // 2: sag ust
             0.5f, -0.5f, 0.0f,  1.0f, 0.0f    // 3: sag alt
        };

        const std::uint32_t indices[] = { 0, 1, 2,  2, 3, 0 };

        // ===================================================================
        // 2) GPU NESNELERI
        // ===================================================================
        m_QuadVA = std::make_shared<FX::VertexArray>();
        m_QuadVA->Bind();

        auto vbo = std::make_shared<FX::VertexBuffer>(vertices, sizeof(vertices));

        // LAYOUT ARTIK IKI ELEMANLI.
        // BufferLayout offset/stride'i kendisi hesapliyor:
        //   a_Position -> offset 0,  boyut 12
        //   a_TexCoord -> offset 12, boyut 8
        //   stride = 20
        // Bunu elle yazsaydik "12" sayisini bir yerde unutup goruntuyu
        // bozmak cok kolay olurdu. Faz 2'de bu soyutlamayi bu yuzden kurmustuk.
        //
        // Ekleme SIRASI, shader'daki layout(location=N) ile eslesir:
        // ilk eleman -> location 0, ikinci -> location 1.
        vbo->SetLayout({
            { FX::ShaderDataType::Float3, "a_Position" },
            { FX::ShaderDataType::Float2, "a_TexCoord" }
        });

        m_QuadVA->AddVertexBuffer(vbo);

        auto ebo = std::make_shared<FX::IndexBuffer>(
            indices, static_cast<std::uint32_t>(sizeof(indices) / sizeof(std::uint32_t)));
        m_QuadVA->SetIndexBuffer(ebo);
        m_QuadVA->Unbind();

        // ===================================================================
        // 3) SHADER
        // ===================================================================
        m_TextureShader.reset(FX::Shader::FromFiles("Texture",
                                                    "assets/shaders/Texture.vert",
                                                    "assets/shaders/Texture.frag"));
        if (!m_TextureShader || !m_TextureShader->IsValid())
        {
            FX_ERROR("Shader yuklenemedi. Aranan klasor: %s",
                     FX::FileSystem::GetBaseDirectory().c_str());
            Close();
            return;
        }

        // sampler2D'ye "0 numarali slotu oku" diyoruz.
        // Bunu bir kez yapmak yeterli; uniform degeri programda saklanir.
        m_TextureShader->Bind();
        m_TextureShader->SetInt("u_Texture", 0);

        // ===================================================================
        // 4) TEXTURE'LAR
        // ===================================================================
        // Dama tahtasi: Nearest filtre ile yukluyoruz ki kareler KESKIN cikssin.
        // Linear olsaydi zoom yapinca kenarlar bulanik olurdu - pixel-art
        // tarzi icin istemedigimiz sey. Farki gormek icin F tusuyla
        // degistirebiliyoruz.
        FX::TextureSpec sharpSpec;
        sharpSpec.MinFilter = FX::TextureFilter::Nearest;
        sharpSpec.MagFilter = FX::TextureFilter::Nearest;
        sharpSpec.WrapS     = FX::TextureWrap::Repeat;   // tiling testi icin
        sharpSpec.WrapT     = FX::TextureWrap::Repeat;

        m_Checkerboard = std::make_unique<FX::Texture2D>(
            "assets/textures/checkerboard.png", sharpSpec);

        // Daire: alfa kanalli, saydamlik testi icin. Linear filtre uygun
        // cunku yuvarlak kenarlarin yumusak olmasini istiyoruz.
        m_Circle = std::make_unique<FX::Texture2D>("assets/textures/circle.png");

        if (!m_Checkerboard->IsValid() || !m_Circle->IsValid())
        {
            FX_ERROR("Texture'lar yuklenemedi. assets/textures/ klasorunu kontrol et.");
            Close();
            return;
        }

        // ===================================================================
        // 5) KAMERA
        // ===================================================================
        // Baslangicta 1:1 ile kuruyoruz, hemen ardindan gercek pencere
        // oranina gore duzeltiyoruz.
        m_Camera = std::make_unique<FX::OrthographicCamera>(-1.0f, 1.0f, -1.0f, 1.0f);
        UpdateCameraProjection();

        FX_INFO("Kontroller:");
        FX_INFO("  W/A/S/D  -> kamerayi hareket ettir");
        FX_INFO("  Q/E      -> kamerayi dondur");
        FX_INFO("  Tekerlek -> yakinlas/uzaklas");
        FX_INFO("  R        -> kamerayi sifirla");
        FX_INFO("  T        -> tel kafes ac/kapa");
        FX_INFO("  SPACE    -> animasyon dur/devam");
        FX_INFO("  V        -> VSync");
        FX_INFO("  TAB      -> istatistikler");
        FX_INFO("  ESC      -> cikis");
    }

    void EditorApp::UpdateCameraProjection()
    {
        const float w = static_cast<float>(GetWindow().GetWidth());
        const float h = static_cast<float>(GetWindow().GetHeight());

        // Sifira bolme korumasi: pencere simge durumuna kucultuldugunde
        // yukseklik 0 gelebilir. Sonuc NaN olur ve TUM matris bozulur -
        // ekran tamamen kararir. Bulunmasi zor bir hatadir.
        if (h <= 0.0f)
            return;

        m_Camera->SetProjectionFromAspect(w / h, m_ZoomLevel);
    }

    void EditorApp::OnWindowResize(std::uint32_t /*width*/, std::uint32_t /*height*/)
    {
        // Pencere boyutu degisti -> en-boy orani degisti -> projeksiyon yenilenmeli.
        // Bunu yapmazsak kareler dikdortgene doner (Faz 2'deki sorun).
        UpdateCameraProjection();
    }

    void EditorApp::UpdateCameraMovement(float dt)
    {
        // KLAVYE DURUMU vs KLAVYE OLAYI - onemli bir ayrim:
        //
        // OnEvent (olay) "tusa BASILDI" anini yakalar - bir kez tetiklenir.
        //   Uygun: menu acma, zipla, ates et.
        //
        // SDL_GetKeyboardState (durum) "tus SU AN basili mi" der.
        //   Uygun: surekli hareket. Cunku hareketin her karede, dt ile
        //   olceklenerek uygulanmasi gerekir.
        //
        // Hareketi OnEvent'e yazsaydik, isletim sisteminin tus tekrar
        // hizina bagimli ve tutuk bir hareket elde ederdik.
        const bool* keys = SDL_GetKeyboardState(nullptr);

        // dt ile carpiyoruz: "saniyede 5 birim" -> kare hizindan bagimsiz.
        const float move   = m_CameraMoveSpeed   * dt;
        const float rotate = m_CameraRotateSpeed * dt;

        // Kamera dondurulmusse, "yukari" tusu ekranda yukari gitmeli -
        // dunyanin Y ekseninde degil. Bu yuzden hareket yonunu kameranin
        // acisiyla donduruyoruz. Dondurmeseydik kamera egikken kontroller
        // ters gelirdi.
        const float rad = glm::radians(m_CameraRotation);
        const float cs  = std::cos(rad);
        const float sn  = std::sin(rad);

        float dx = 0.0f, dy = 0.0f;
        if (keys[SDL_SCANCODE_A]) dx -= 1.0f;
        if (keys[SDL_SCANCODE_D]) dx += 1.0f;
        if (keys[SDL_SCANCODE_S]) dy -= 1.0f;
        if (keys[SDL_SCANCODE_W]) dy += 1.0f;

        // Capraz giderken hizlanmayi onlemek icin normalize.
        // (1,1) vektorunun uzunlugu 1.41'dir; duzeltmezsek capraz hareket
        // duz hareketten %41 hizli olur. Klasik bir oyun hatasi.
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
        ++m_UpdateCount;
    }

    void EditorApp::OnRender(float /*alpha*/)
    {
        ++m_FrameCount;

        FX::RenderCommand::SetClearColor({ 0.08f, 0.09f, 0.12f, 1.0f });
        FX::RenderCommand::Clear();

        m_TextureShader->Bind();

        // Kameranin matrisi TUM nesneler icin ayni -> kare basina bir kez set.
        m_TextureShader->SetMat4("u_ViewProjection", m_Camera->GetViewProjectionMatrix());

        // -------------------------------------------------------------------
        // 1) Zemin: buyuk, tiled dama tahtasi
        // -------------------------------------------------------------------
        // TilingFactor 10 -> UV'ler 10 ile carpilir -> desen 10x10 tekrarlanir.
        // Bu, texture'in WrapS/T = Repeat olmasi sayesinde calisir.
        // ClampToEdge olsaydi kenar pikseli uzardi ve cirkin cizgiler olurdu.
        {
            m_Checkerboard->Bind(0);
            m_TextureShader->SetFloat("u_TilingFactor", 10.0f);
            m_TextureShader->SetFloat4("u_Color", { 1.0f, 1.0f, 1.0f, 1.0f });

            glm::mat4 transform =
                glm::translate(glm::mat4(1.0f), { 0.0f, 0.0f, -0.1f }) *   // hafif arkada
                glm::scale(glm::mat4(1.0f), { 20.0f, 20.0f, 1.0f });

            m_TextureShader->SetMat4("u_Transform", transform);
            FX::RenderCommand::DrawIndexed(m_QuadVA);
        }

        // -------------------------------------------------------------------
        // 2) Donen dama tahtasi karesi - UV yonelimini gormek icin
        // -------------------------------------------------------------------
        {
            m_Checkerboard->Bind(0);
            m_TextureShader->SetFloat("u_TilingFactor", 1.0f);
            m_TextureShader->SetFloat4("u_Color", { 1.0f, 1.0f, 1.0f, 1.0f });

            glm::mat4 transform =
                glm::translate(glm::mat4(1.0f), { -2.0f, 0.0f, 0.0f }) *
                glm::rotate(glm::mat4(1.0f), m_Time * 0.5f, { 0.0f, 0.0f, 1.0f }) *
                glm::scale(glm::mat4(1.0f), { 2.0f, 2.0f, 1.0f });

            m_TextureShader->SetMat4("u_Transform", transform);
            FX::RenderCommand::DrawIndexed(m_QuadVA);
        }

        // -------------------------------------------------------------------
        // 3) Saydam daireler - blending ve tint testi
        // -------------------------------------------------------------------
        // Ayni texture, farkli u_Color ile 3 kez ciziliyor. Tek bir gri
        // sprite'i istedigin renkte kullanabilmenin yolu budur -
        // 3 ayri PNG tutmak yerine 1 tane tutup renklendiriyorsun.
        {
            m_Circle->Bind(0);
            m_TextureShader->SetFloat("u_TilingFactor", 1.0f);

            const glm::vec4 colors[3] = {
                { 1.0f, 0.4f, 0.4f, 0.85f },
                { 0.4f, 1.0f, 0.5f, 0.85f },
                { 0.5f, 0.6f, 1.0f, 0.85f }
            };

            for (int i = 0; i < 3; ++i)
            {
                // Her daire farkli fazda salinsin.
                const float offset = static_cast<float>(i) * 2.0f;
                const float y = std::sin(m_Time * 1.5f + offset) * 1.2f;

                m_TextureShader->SetFloat4("u_Color", colors[i]);

                glm::mat4 transform =
                    glm::translate(glm::mat4(1.0f), { 2.0f + static_cast<float>(i) * 1.3f, y, 0.0f }) *
                    glm::scale(glm::mat4(1.0f), { 1.5f, 1.5f, 1.0f });

                m_TextureShader->SetMat4("u_Transform", transform);
                FX::RenderCommand::DrawIndexed(m_QuadVA);
            }
        }

        // NOT: Burada 5 ayri cizim cagrisi (draw call) yaptik. Her biri
        // CPU-GPU arasinda ayri bir iletisim demek. 5 icin sorun degil,
        // ama 5000 sprite icin felaket olur. Faz 4'te batch renderer ile
        // bunlarin hepsini TEK cagriya indirecegiz - iste o zaman bu
        // dosyadaki tekrar da ortadan kalkacak.
    }

    void EditorApp::OnEvent(const SDL_Event& event)
    {
        // Fare tekerlegi ile zoom.
        if (event.type == SDL_EVENT_MOUSE_WHEEL)
        {
            // Tekerlek CARPARAK olcekleniyor, cikararak degil.
            // Sebep: zoom logaritmik algilanir. Sabit cikarma yapsaydik
            // yakinken cok hizli, uzaktayken cok yavas hissedilirdi.
            m_ZoomLevel *= (event.wheel.y > 0) ? 0.9f : 1.1f;

            // Sinirlar: 0'a ulasirsa projeksiyon cokerdi (sifir genislik),
            // cok buyurse hicbir sey gorunmezdi.
            m_ZoomLevel = glm::clamp(m_ZoomLevel, 0.5f, 30.0f);

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

                case SDLK_R:
                    m_CameraPosition = { 0.0f, 0.0f, 0.0f };
                    m_CameraRotation = 0.0f;
                    m_ZoomLevel      = 5.0f;
                    UpdateCameraProjection();
                    FX_INFO("Kamera sifirlandi.");
                    break;

                case SDLK_T:
                    m_Wireframe = !m_Wireframe;
                    FX::RenderCommand::SetWireframe(m_Wireframe);
                    FX_INFO("Tel kafes -> %s", m_Wireframe ? "ACIK" : "KAPALI");
                    break;

                case SDLK_SPACE:
                    m_Animate = !m_Animate;
                    FX_INFO("Animasyon -> %s", m_Animate ? "ACIK" : "DURDU");
                    break;

                case SDLK_V:
                {
                    const bool s = !GetWindow().IsVSync();
                    GetWindow().SetVSync(s);
                    FX_INFO("VSync -> %s", s ? "ACIK" : "KAPALI");
                    break;
                }

                case SDLK_TAB:
                    FX_INFO("--- Istatistikler ---");
                    FX_INFO("  Kamera konum  : (%.2f, %.2f)", m_CameraPosition.x, m_CameraPosition.y);
                    FX_INFO("  Kamera aci    : %.1f derece", m_CameraRotation);
                    FX_INFO("  Zoom          : %.2f (dikeyde %.1f birim gorunuyor)",
                            m_ZoomLevel, m_ZoomLevel * 2.0f);
                    FX_INFO("  Pencere       : %ux%u piksel (oran %.3f)",
                            GetWindow().GetWidth(), GetWindow().GetHeight(),
                            static_cast<float>(GetWindow().GetWidth()) /
                            static_cast<float>(GetWindow().GetHeight()));
                    FX_INFO("  Guncelleme    : %llu", static_cast<unsigned long long>(m_UpdateCount));
                    FX_INFO("  Kare          : %llu", static_cast<unsigned long long>(m_FrameCount));
                    FX_INFO("  Cizim cagrisi : 5 / kare  (Faz 4'te 1 olacak)");
                    break;

                default:
                    break;
            }
        }
    }

    void EditorApp::OnShutdown()
    {
        FX_INFO("Editor kapaniyor. Toplam %llu guncelleme, %llu kare.",
                static_cast<unsigned long long>(m_UpdateCount),
                static_cast<unsigned long long>(m_FrameCount));
    }
}
