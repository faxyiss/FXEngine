#include "ImGuiLayer.h"

#include <FXEngine/Core/Window.h>
#include <FXEngine/Core/Log.h>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>

#include <SDL3/SDL.h>

namespace FXEd
{
    void ImGuiLayer::Init(FX::Window& window)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();

        // --- Docking ----------------------------------------------------------
        // Panellerin birbirine kenetlenebilmesi, ayrilabilmesi, sekme
        // haline gelebilmesi. Bu ozellik ImGui'nin "docking" dalinda var,
        // ana dalda yok - Faz 0'da bilerek o dali cektik.
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        // Viewport (pencere disina surukleme) BILEREK KAPALI.
        // Acarsak paneller isletim sistemi penceresi olarak disari
        // cikabilir - etkileyici ama SDL3 + OpenGL ile ek context
        // yonetimi gerektirir ve hata kaynagi olur. MVP'de gereksiz.
        // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        // Panel duzenini imgui.ini'ye kaydeder. Programi kapatip acinca
        // panelleri tekrar duzenlemek zorunda kalmazsin.
        io.IniFilename = "imgui.ini";

        ImGui::StyleColorsDark();

        // Kenarlari biraz yuvarlat - varsayilan cok koseli duruyor.
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding    = 4.0f;
        style.FrameRounding     = 3.0f;
        style.TabRounding       = 3.0f;
        style.WindowBorderSize  = 1.0f;

        // --- Backend'ler -------------------------------------------------------
        // ImGui iki parcadan olusur:
        //   PLATFORM backend  -> girdi, pencere, pano (SDL3)
        //   RENDERER backend  -> cizim verisini GPU'ya gonderme (OpenGL3)
        // Bu ayrim sayesinde ImGui herhangi bir pencere/grafik kombinasyonuyla
        // calisabilir. Biz ikisini de kendimiz baglıyoruz.
        ImGui_ImplSDL3_InitForOpenGL(window.GetNativeWindow(),
                                     window.GetNativeContext());

        // GLSL surumu shader'larimizla ayni olmali: GL 3.3 -> GLSL 330.
        ImGui_ImplOpenGL3_Init("#version 330 core");

        m_Initialized = true;
        FX_INFO("ImGui hazir (surum %s, docking acik).", IMGUI_VERSION);
    }

    void ImGuiLayer::Shutdown()
    {
        if (!m_Initialized)
            return;

        // TERS SIRADA kapatiyoruz: once renderer, sonra platform,
        // en son context. Ters yaparsan zaten yok edilmis context'e
        // erisen kod calisir.
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();

        m_Initialized = false;
    }

    void ImGuiLayer::Begin()
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // --- Dockspace ---------------------------------------------------------
        // Tum ekrani kaplayan gorunmez bir "kenetlenme alani" olusturur.
        // Panellerin nereye yapisabilecegini bu belirler.
        //
        // PassthruCentralNode: ortadaki bos alan SEFFAF kalir. Bunu
        // istiyoruz cunku viewport paneli oraya yerlesecek.
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(),
                                     ImGuiDockNodeFlags_PassthruCentralNode);
    }

    void ImGuiLayer::End(std::uint32_t displayWidth, std::uint32_t displayHeight)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(static_cast<float>(displayWidth),
                                static_cast<float>(displayHeight));

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    bool ImGuiLayer::OnEvent(const SDL_Event& event)
    {
        // ImGui olayi kendi ic durumuna isler (fare konumu, tus durumu...).
        ImGui_ImplSDL3_ProcessEvent(&event);

        // Olayin "tuketilip tuketilmedigini" WantCapture bayraklariyla
        // anliyoruz. ProcessEvent'in donus degeri bu amac icin guvenilir degil.
        const ImGuiIO& io = ImGui::GetIO();

        switch (event.type)
        {
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
            case SDL_EVENT_TEXT_INPUT:
                return io.WantCaptureKeyboard;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
            case SDL_EVENT_MOUSE_MOTION:
            case SDL_EVENT_MOUSE_WHEEL:
                return io.WantCaptureMouse;

            default:
                return false;
        }
    }

    bool ImGuiLayer::WantsKeyboard() const { return ImGui::GetIO().WantCaptureKeyboard; }
    bool ImGuiLayer::WantsMouse()    const { return ImGui::GetIO().WantCaptureMouse; }
}
