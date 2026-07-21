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
#include <string>

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
        // Yeni ImGui cercevesi. Dockspace AYRI (bkz. BeginDockspace):
        // ust arac cubugu ikisinin ARASINDA cizilmeli, yoksa dockspace
        // ona yer ayirmaz ve paneller altina girer.
        void Begin();

        // Kenetlenme alanini acar; bundan sonra cizilen paneller
        // yapisabilir. Karsilama ekrani bunu hic cagirmiyor.
        void BeginDockspace();

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

        // Panelleri varsayilan duzene dondurur (bir sonraki Begin'de uygulanir).
        void ResetLayout() { m_BuildDefaultLayout = true; }

    private:
        void BuildDefaultLayout(unsigned int dockspaceID);

        // Kaydedilmis duzende bu pencere var mi? Yeni bir panel
        // eklendiginde eski duzeni yenilemek icin.
        bool IniHasWindow(const char* name) const;

        bool m_Initialized = false;

        // ImGui, IniFilename isaretcisini SAKLAR ve sonra okur - gecici bir
        // string veremeyiz, uye olarak yasatmak zorundayiz.
        std::string m_IniPath;

        bool m_BuildDefaultLayout = false;
    };
}
