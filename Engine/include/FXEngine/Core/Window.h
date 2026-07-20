#pragma once

// ===========================================================================
// FXEngine - Window
//
// SORUMLULUK: Bir SDL3 penceresi ve ona bagli bir OpenGL 3.3 Core context'i
// olusturmak, yasatmak ve yok etmek. BASKA HICBIR SEY.
//
// Ozellikle: burada ana dongu YOK, olay isleme mantigi YOK, cizim YOK.
// O isler Application'a ait. "Kaynagi sahiplenen sinif" ile "akisi yoneten
// sinif" ayri olmali - motor mimarisinin en cok tekrar eden kalibi budur.
// ===========================================================================

#include <cstdint>

// ILERI BILDIRIM (forward declaration):
// SDL_Window'un ne oldugunu burada bilmemize gerek yok, sadece "bir tur var"
// demek yeterli. Boylece <SDL3/SDL.h>'i bu header'a sokmuyoruz ve bize
// include eden herkesin derleme suresini kurtariyoruz.
// SDL_Window bir struct, SDL_GLContext ise void* takma adi - bu yuzden
// ikisini farkli sekilde ele aliyoruz.
struct SDL_Window;

namespace FX
{
    // Pencere ayarlari tek bir yapida. Neden 5 ayri parametre degil?
    // Cunku ileride "vsync", "resizable", "fullscreen" eklenecek ve
    // fonksiyon imzasi her seferinde degisecek. Yapi kullanirsak
    // yeni alan eklemek mevcut cagrilari bozmaz.
    struct WindowProps
    {
        const char*  Title  = "FXEngine";
        std::uint32_t Width  = 1280;
        std::uint32_t Height = 720;
        bool          VSync  = true;
        bool          Resizable = true;
    };

    class Window
    {
    public:
        explicit Window(const WindowProps& props);
        ~Window();

        // --- Kopyalama ve tasima yasak ---------------------------------------
        // Window bir SISTEM KAYNAGI sahibi (pencere tutamaci + GL context).
        // Kopyalanirsa iki nesne ayni tutamaci tutar, ikisi de yok edilirken
        // SDL_DestroyWindow iki kez cagrilir -> cokme.
        // Bunu derleyiciye yasaklatiyoruz ki kaza olmasin. C++'ta buna
        // "Rule of Five" denir; en guvenli hali budur: hepsini sil.
        Window(const Window&)            = delete;
        Window& operator=(const Window&) = delete;
        Window(Window&&)                 = delete;
        Window& operator=(Window&&)      = delete;

        // Cizilen kareyi ekrana getirir (double buffering).
        void SwapBuffers();

        // Dikey senkronizasyon: ekranin yenileme hiziyla senkron cizim.
        void SetVSync(bool enabled);
        bool IsVSync() const { return m_VSync; }

        std::uint32_t GetWidth()  const { return m_Width; }
        std::uint32_t GetHeight() const { return m_Height; }

        // Pencere yeniden boyutlandiginda Application bunu cagirir.
        void OnResize(std::uint32_t width, std::uint32_t height);

        // Ham SDL tutamaci. ImGui gibi 3rd-party kodlar buna ihtiyac duyar
        // (Faz 6). Normalde ham tutamaci disari acmak kotu bir aliskanliktir,
        // ama tam da bu sebeple gerekli - kontrollu bir kacis kapisi.
        SDL_Window* GetNativeWindow() const { return m_Window; }

        // OpenGL context tutamaci. ImGui'nin SDL3+OpenGL backend'i
        // baslatilirken bunu ister (Faz 6). Tipi void* cunku SDL_GLContext
        // zaten void* takma adidir ve header'a SDL sokmak istemiyoruz.
        void* GetNativeContext() const { return m_GLContext; }

        // Olusturma basarili miydi? Yapici hata firlatmiyor (motorlarda
        // istisnalar genelde kapalidir), o yuzden durumu boyle sorguluyoruz.
        bool IsValid() const { return m_Window != nullptr && m_GLContext != nullptr; }

    private:
        SDL_Window*   m_Window    = nullptr;
        void*         m_GLContext = nullptr;   // SDL_GLContext == void* (SDL3)

        std::uint32_t m_Width  = 0;
        std::uint32_t m_Height = 0;
        bool          m_VSync  = true;
    };
}
