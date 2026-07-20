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
              props.Title     = "FXEditor - Faz 2";
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
        FX_INFO("  FXEditor - Faz 2");
        FX_INFO("=====================================");

        // Grafik durumunu kur (blend, derinlik testi).
        FX::RenderCommand::Init();

        // ===================================================================
        // 1) KOSE VERISI (VBO'ya gidecek)
        // ===================================================================
        // Su an bir kamera YOK (Faz 3'un konusu). O yuzden koordinatlari
        // dogrudan OpenGL'in kendi "Normalized Device Coordinates" (NDC)
        // sisteminde veriyoruz:
        //
        //        (0, +1)          Ekranin ortasi (0,0)
        //           |             Sol kenar     x = -1
        //  (-1,0) --+-- (+1, 0)   Sag kenar     x = +1
        //           |             Alt kenar     y = -1
        //        (0, -1)          Ust kenar     y = +1
        //
        // DIKKAT: Bu sistemde Y YUKARI dogru artar (ekran koordinatlarinin
        // tersine). Ayrica en-boy orani hesaba katilmaz - bu yuzden 1280x720
        // pencerede "kare" degil dikdortgen gorunecek. Bunu Faz 3'te
        // orthographic kamera ile duzeltecegiz. Simdilik bilerek boyle.
        //
        // Kose sirasi (saat yonunun TERSI - OpenGL'in varsayilan "on yuz" yonu):
        //   1 ----- 2
        //   |       |
        //   |       |
        //   0 ----- 3
        const float vertices[] = {
            // x      y      z
            -0.5f, -0.5f, 0.0f,   // 0: sol alt
            -0.5f,  0.5f, 0.0f,   // 1: sol ust
             0.5f,  0.5f, 0.0f,   // 2: sag ust
             0.5f, -0.5f, 0.0f    // 3: sag alt
        };

        // ===================================================================
        // 2) INDEKSLER (EBO'ya gidecek)
        // ===================================================================
        // Kare = 2 ucgen. Kose 0 ve 2 IKI ucgende de kullaniliyor -
        // index buffer'in varlik sebebi tam olarak bu tekrarı onlemek.
        //
        //   1 ----- 2      Ucgen A: 0 -> 1 -> 2
        //   | \     |
        //   |   \   |      Ucgen B: 2 -> 3 -> 0
        //   |     \ |
        //   0 ----- 3
        //
        // 4 kose verisi + 6 indeks < 6 kose verisi. Kose ne kadar buyurse
        // (pozisyon+renk+uv+normal...) tasarruf o kadar artar.
        const std::uint32_t indices[] = {
            0, 1, 2,   // ucgen A
            2, 3, 0    // ucgen B
        };

        // ===================================================================
        // 3) GPU NESNELERINI KUR
        // ===================================================================
        m_QuadVA = std::make_shared<FX::VertexArray>();
        m_QuadVA->Bind();

        auto vbo = std::make_shared<FX::VertexBuffer>(vertices, sizeof(vertices));

        // LAYOUT: "bu tampondaki her kose 3 float'tan olusan bir pozisyondur".
        // Bu satir olmadan GPU o sayilarin ne anlama geldigini bilemez.
        // Isim ("a_Position") sadece hata ayiklama icin; eslesme
        // shader'daki layout(location=0) ile INDEX uzerinden olur.
        vbo->SetLayout({
            { FX::ShaderDataType::Float3, "a_Position" }
        });

        m_QuadVA->AddVertexBuffer(vbo);

        auto ebo = std::make_shared<FX::IndexBuffer>(
            indices, static_cast<std::uint32_t>(sizeof(indices) / sizeof(std::uint32_t)));
        m_QuadVA->SetIndexBuffer(ebo);

        m_QuadVA->Unbind();

        // ===================================================================
        // 4) SHADER
        // ===================================================================
        // Yollar calisma dizinine gore. CMake bu dosyalari exe'nin yanindaki
        // assets/ klasorune kopyaliyor (Editor/CMakeLists.txt'e bak).
        m_Shader.reset(FX::Shader::FromFiles("FlatColor",
                                             "assets/shaders/FlatColor.vert",
                                             "assets/shaders/FlatColor.frag"));

        if (!m_Shader || !m_Shader->IsValid())
        {
            FX_ERROR("Shader yuklenemedi.");
            FX_ERROR("Aranan klasor: %s", FX::FileSystem::GetBaseDirectory().c_str());
            FX_ERROR("Orada assets/shaders/ var mi? Yoksa projeyi yeniden derle");
            FX_ERROR("(CMake, assets/ klasorunu exe'nin yanina kopyalar).");
            Close();
            return;
        }

        FX_INFO("Kontroller:");
        FX_INFO("  ESC     -> cikis");
        FX_INFO("  W       -> tel kafes (wireframe) ac/kapa");
        FX_INFO("  A       -> animasyonu durdur/baslat");
        FX_INFO("  V       -> VSync ac/kapa");
        FX_INFO("  BOSLUK  -> istatistikler");
    }

    void EditorApp::OnShutdown()
    {
        FX_INFO("Editor kapaniyor. Toplam %llu guncelleme, %llu kare.",
                static_cast<unsigned long long>(m_UpdateCount),
                static_cast<unsigned long long>(m_FrameCount));
    }

    void EditorApp::OnUpdate(float dt)
    {
        if (m_Animate)
            m_Time += dt;
        ++m_UpdateCount;
    }

    void EditorApp::OnRender(float /*alpha*/)
    {
        ++m_FrameCount;

        // --- Ekrani temizle ---------------------------------------------------
        // Artik dogrudan gl* cagirmiyoruz; her sey RenderCommand uzerinden.
        // Faz 1'deki cift glClear sorunu boylece cozuldu.
        FX::RenderCommand::SetClearColor({ 0.10f, 0.11f, 0.14f, 1.0f });
        FX::RenderCommand::Clear();

        // --- Quad'i ciz -------------------------------------------------------
        m_Shader->Bind();

        // Renk zamanla degissin ki uniform'un gercekten calistigini gorelim.
        // sin() -1..+1 -> *0.5+0.5 ile 0..1'e cekiyoruz (renk araligi).
        const float r = std::sin(m_Time * 1.3f) * 0.5f + 0.5f;
        const float g = std::sin(m_Time * 0.9f + 2.0f) * 0.5f + 0.5f;
        const float b = std::sin(m_Time * 0.6f + 4.0f) * 0.5f + 0.5f;
        m_Shader->SetFloat4("u_Color", { r, g, b, 1.0f });

        // Donen bir transform matrisi.
        // mat4(1.0f) = birim matris ("hicbir sey yapma"). Donusumler hep
        // ondan baslar. glm::rotate ONE carpim yapar, yani islemler
        // SAGDAN SOLA uygulanir - matris matematiginin klasik kafa karistiricisi.
        glm::mat4 transform = glm::rotate(glm::mat4(1.0f),
                                          m_Time * 0.8f,               // radyan
                                          glm::vec3(0.0f, 0.0f, 1.0f)); // Z ekseni
        m_Shader->SetMat4("u_Transform", transform);

        // Tek cizim cagrisi: VAO'yu bagla, 6 indeksi ucgen olarak ciz.
        FX::RenderCommand::DrawIndexed(m_QuadVA);
    }

    void EditorApp::OnEvent(const SDL_Event& event)
    {
        if (event.type == SDL_EVENT_KEY_DOWN)
        {
            if (event.key.repeat)
                return;

            switch (event.key.key)
            {
                case SDLK_ESCAPE:
                    Close();
                    break;

                case SDLK_W:
                    m_Wireframe = !m_Wireframe;
                    FX::RenderCommand::SetWireframe(m_Wireframe);
                    // Tel kafes modunda IKI UCGENI de ayri ayri gorursun.
                    // Karenin aslinda 2 ucgenden olustugunu gozle dogrulamak
                    // icin en iyi arac budur.
                    FX_INFO("Tel kafes -> %s (kosegen cizgiyi gor: kare = 2 ucgen)",
                            m_Wireframe ? "ACIK" : "KAPALI");
                    break;

                case SDLK_A:
                    m_Animate = !m_Animate;
                    FX_INFO("Animasyon -> %s", m_Animate ? "ACIK" : "DURDU");
                    break;

                case SDLK_V:
                {
                    const bool newState = !GetWindow().IsVSync();
                    GetWindow().SetVSync(newState);
                    FX_INFO("VSync -> %s", newState ? "ACIK" : "KAPALI");
                    break;
                }

                case SDLK_SPACE:
                    FX_INFO("--- Istatistikler ---");
                    FX_INFO("  Gecen sure (mantik) : %.2f sn", m_Time);
                    FX_INFO("  Guncelleme sayisi   : %llu",
                            static_cast<unsigned long long>(m_UpdateCount));
                    FX_INFO("  Kare sayisi         : %llu",
                            static_cast<unsigned long long>(m_FrameCount));
                    if (m_FrameCount > 0)
                        FX_INFO("  Guncelleme / Kare   : %.2f",
                                static_cast<double>(m_UpdateCount) / static_cast<double>(m_FrameCount));
                    FX_INFO("  Pencere             : %ux%u piksel",
                            GetWindow().GetWidth(), GetWindow().GetHeight());
                    break;

                default:
                    break;
            }
        }
    }
}
