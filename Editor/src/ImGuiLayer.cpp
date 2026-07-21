#include "ImGuiLayer.h"

#include <FXEngine/Core/Window.h>
#include <FXEngine/Core/Log.h>
#include <FXEngine/Core/FileSystem.h>

#include <imgui.h>
#include <imgui_internal.h>   // DockBuilder API'si burada
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include <ImGuizmo.h>

#include <SDL3/SDL.h>

#include <fstream>
#include <string>

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

        // Ini dosyasini EXE'NIN YANINA yaziyoruz, calisma dizinine degil.
        // Aksi halde programi farkli klasorlerden baslattiginda duzen
        // kayboluyor - Faz 2'de shader yollarinda yasadigimiz sorunun aynisi.
        m_IniPath = FX::FileSystem::GetBaseDirectory() + "imgui.ini";
        io.IniFilename = m_IniPath.c_str();

        // Ini yoksa ilk acilista varsayilan duzeni kuracagiz. Yoksa tum
        // paneller ust uste, icerige gore kucucuk boyutlarda aciliyor.
        //
        // A2'de "Viewport" paneli "Scene" + "Game" olarak ikiye ayrildi.
        // Eski bir ini'de "Game" yok; duzeni yeniden kurmazsak yeni
        // paneller yerlesmemis halde ortada yuzuyor.
        m_BuildDefaultLayout = !FX::FileSystem::Exists(m_IniPath) ||
                               !IniHasWindow("Game");

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
        ImGuizmo::BeginFrame();
    }

    void ImGuiLayer::BeginDockspace()
    {
        // Tum calisma alanini kaplayan gorunmez bir "kenetlenme alani".
        // PassthruCentralNode: ortadaki bos alan seffaf kalir.
        //
        // Menu cubugu ve ust arac cubugu BUNDAN ONCE cizilmis olmali;
        // ikisi de calisma alanini kucultuyor ve dockspace kalani aliyor.
        const ImGuiID dockspaceID =
            ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(),
                                         ImGuiDockNodeFlags_PassthruCentralNode);

        if (m_BuildDefaultLayout)
        {
            m_BuildDefaultLayout = false;
            BuildDefaultLayout(dockspaceID);
        }
    }

    bool ImGuiLayer::IniHasWindow(const char* name) const
    {
        std::ifstream in(m_IniPath);
        if (!in.is_open())
            return false;

        const std::string needle = std::string("[Window][") + name + "]";

        std::string line;
        while (std::getline(in, line))
        {
            if (line.rfind(needle, 0) == 0)
                return true;
        }
        return false;
    }

    void ImGuiLayer::BuildDefaultLayout(unsigned int dockspaceID)
    {
        ImGui::DockBuilderRemoveNode(dockspaceID);
        ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceID, ImGui::GetMainViewport()->Size);

        ImGuiID center = dockspaceID;
        const ImGuiID left  = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left,  0.18f, nullptr, &center);
        const ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.24f, nullptr, &center);
        const ImGuiID leftBottom = ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, 0.45f, nullptr, nullptr);
        const ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.28f, nullptr, &center);

        ImGui::DockBuilderDockWindow("Hierarchy",      left);
        ImGui::DockBuilderDockWindow("Istatistikler",  leftBottom);
        ImGui::DockBuilderDockWindow("Inspector",      right);
        ImGui::DockBuilderDockWindow("Icerik",         bottom);
        // Scene ve Game AYNI dugume: sekme olarak yan yana gelirler,
        // Play'e basinca Game one cikar (Unity duzeni).
        ImGui::DockBuilderDockWindow("Scene",          center);
        ImGui::DockBuilderDockWindow("Game",           center);

        ImGui::DockBuilderFinish(dockspaceID);
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
