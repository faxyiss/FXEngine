#pragma once

// ===========================================================================
// FXEditor - ImGuiLayer
//
// NEDEN EDITOR'DE, ENGINE'DE DEGIL?
// Engine'in gorevi oyun calistirmak. Sevk edilen bir oyunda editor arayuzu
// yoktur; ImGui'yi Engine'e koysaydik her oyun binary'si onu tasirdi.
// CMake'te de imgui zaten sadece FXEditor'e link edilmis durumda.
//
// Bu sinif ImGui'nin yasam dongusunu sarmalar:
//   Init      -> context, stil, backend'ler
//   Begin/End -> her karede cerceve acma/kapama
//   OnEvent   -> SDL olaylarini ImGui'ye ilet
// ===========================================================================

#include <cstdint>

namespace FX { class Window; }
union SDL_Event;

namespace FXEd
{
    class ImGuiLayer
    {
    public:
        void Init(FX::Window& window);
        void Shutdown();

        // Yeni bir ImGui cercevesi baslatir. Tum ImGui:: cagrilari
        // Begin ile End arasinda olmali.
        void Begin();

        // Cerceveyi kapatir ve ImGui'nin cizim verisini GPU'ya gonderir.
        void End(std::uint32_t displayWidth, std::uint32_t displayHeight);

        // SDL olayini ImGui'ye iletir.
        // Donus degeri: ImGui bu olayi "tuketti" mi?
        // true ise editor kendi kisayollarini CALISTIRMAMALI - kullanici
        // bir metin kutusuna yaziyor olabilir ve 'W' tusu kamerayi
        // oynatmamali. Bu kontrolu atlamak ImGui kullanan uygulamalarda
        // en sik yapilan hatadir.
        bool OnEvent(const SDL_Event& event);

        // ImGui su an klavyeyi/fareyi istiyor mu?
        bool WantsKeyboard() const;
        bool WantsMouse() const;

    private:
        bool m_Initialized = false;
    };
}
