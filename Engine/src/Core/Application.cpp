#include "FXEngine/Core/Application.h"
#include "FXEngine/Core/Log.h"

#include <glad/glad.h>
#include <SDL3/SDL.h>

namespace FX
{
    Application::Application(const WindowProps& props)
    {
        m_Window = std::make_unique<Window>(props);

        if (!m_Window->IsValid())
        {
            FX_CORE_ERROR("Pencere olusturulamadigi icin uygulama baslatilamiyor.");
            m_Running = false;
        }
    }

    // Yikici .cpp'de: unique_ptr<Window>'un yok edilebilmesi icin Window'un
    // TAM tanimi gerekir. Yikiciyi header'da "= default" birakirsak, header'i
    // include eden her dosyanin Window'un tam tanimini gormesi gerekirdi.
    Application::~Application() = default;

    void Application::Run()
    {
        if (!m_Running)
            return;

        OnInit();

        // -------------------------------------------------------------------
        // ZAMAN OLCUMU
        // -------------------------------------------------------------------
        // SDL_GetTicksNS() nanosaniye cozunurluklu, monoton artan bir sayac
        // dondurur. "Monoton" = kullanici sistem saatini degistirse bile
        // geri gitmez. Zaman olcumunde std::time / SDL_GetTicks (milisaniye)
        // yerine bunu kullanmak dogru tercihtir: 60 FPS'te bir kare 16.6 ms,
        // milisaniye cozunurluk bunun icin fazla kaba kalir.
        std::uint64_t previousTime = SDL_GetTicksNS();

        // Biriktirici (accumulator): "islenmeyi bekleyen zaman".
        float accumulator = 0.0f;

        // FPS sayaci icin
        std::uint64_t fpsTimer   = previousTime;
        int           frameCount = 0;

        FX_CORE_INFO("Ana dongu basliyor (sabit adim: %.4f sn = %.0f Hz)",
                     s_FixedDeltaTime, 1.0f / s_FixedDeltaTime);

        while (m_Running)
        {
            // --- 1) Gecen zamani olc -----------------------------------------
            const std::uint64_t currentTime = SDL_GetTicksNS();
            // ns -> saniye. 1e9f ile boluyoruz.
            float frameTime = static_cast<float>(currentTime - previousTime) / 1.0e9f;
            previousTime = currentTime;

            // Guvenlik siniri: bir kare 250 ms'den uzun surduyse (debugger'da
            // durdun, pencereyi surukledin, disk takildi) gercek sureyi degil
            // bu tavani kullan. Yoksa asagidaki while yuzlerce kez doner.
            if (frameTime > 0.25f)
                frameTime = 0.25f;

            accumulator += frameTime;

            // --- 2) Olaylari isle --------------------------------------------
            ProcessEvents();

            // --- 3) SABIT ADIMLI GUNCELLEME ----------------------------------
            // Bu dongunun kalbi burasi. Biriken zaman bir sabit adimi
            // asiyorsa, asmayana kadar guncelle.
            //
            // Onemli sonuc: OnUpdate bir karede 0 kez de cagrilabilir
            // (cok hizli makine), 3 kez de (yavas makine). Ama dt HER ZAMAN
            // ayni. Bu yuzden fizik/hareket her makinede AYNI davranir.
            int updateCount = 0;
            while (accumulator >= s_FixedDeltaTime && updateCount < s_MaxUpdatesPerFrame)
            {
                if (!m_Minimized)
                    OnUpdate(s_FixedDeltaTime);

                accumulator -= s_FixedDeltaTime;
                ++updateCount;
            }

            // Tavana carptiysak kalan zamani AT. Tutarsak bir sonraki karede
            // yine tavana carpar ve program bir daha asla yetisemez
            // ("spiral of death"). Zamani atmak, yavaslamis gorunmekten iyidir.
            if (updateCount >= s_MaxUpdatesPerFrame)
            {
                if (accumulator > s_FixedDeltaTime)
                    FX_CORE_WARN("Guncelleme yetismedi, %.1f ms atildi.", accumulator * 1000.0f);
                accumulator = 0.0f;
            }

            // --- 4) CIZIM -----------------------------------------------------
            if (!m_Minimized)
            {
                // alpha: iki mantik adimi arasinda neredeyiz? 0.0 - 1.0
                const float alpha = accumulator / s_FixedDeltaTime;

                // Faz 2 degisikligi: ekrani ARTIK BURADA TEMIZLEMIYORUZ.
                //
                // Faz 1'de hem burada hem EditorApp'te glClear vardi - ayni
                // isi iki yerde yapmak, hangisinin kazandigini belirsiz birakir.
                // Temizleme artik turetilen sinifin karari (RenderCommand::Clear).
                // Sebep: ileride birden fazla cizim hedefi (framebuffer)
                // olacak ve "her karede ekrani temizle" varsayimi yanlis olacak.
                OnRender(alpha);

                // Cizdigimiz gizli tamponu ekrandakiyle takas et.
                m_Window->SwapBuffers();
            }
            else
            {
                // Simge durumundayken CPU'yu bosuna yakma. Bu olmadan
                // program minimize haldeyken bile bir cekirdegi %100 kullanir.
                SDL_Delay(10);
            }

            // --- 5) FPS sayaci -------------------------------------------------
            ++frameCount;
            if (currentTime - fpsTimer >= 1'000'000'000ull)   // 1 saniye (ns)
            {
                FX_CORE_TRACE("FPS: %d", frameCount);
                frameCount = 0;
                fpsTimer   = currentTime;
            }
        }

        OnShutdown();
        FX_CORE_INFO("Ana dongu bitti.");
    }

    void Application::ProcessEvents()
    {
        SDL_Event event;

        // SDL_PollEvent kuyrukta olay VARSA true doner, yoksa false -
        // yani BLOKLAMAZ. (SDL_WaitEvent bloklar; oyunlarda istemeyiz cunku
        // kullanici hicbir sey yapmasa da her kareyi cizmemiz gerekir.)
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
                case SDL_EVENT_QUIT:
                    // Pencere X'i, Alt+F4, isletim sistemi kapatma istegi
                    FX_CORE_INFO("Cikis istegi alindi.");
                    m_Running = false;
                    break;

                case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                    m_Running = false;
                    break;

                case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                    // PIXEL_SIZE_CHANGED kullaniyoruz, RESIZED degil.
                    // Fark: yuksek DPI ekranlarda pencere "boyutu" ile
                    // gercek piksel sayisi ayrisir; OpenGL PIKSEL ile calisir.
                {
                    const auto w = static_cast<std::uint32_t>(event.window.data1);
                    const auto h = static_cast<std::uint32_t>(event.window.data2);

                    m_Window->OnResize(w, h);
                    FX_CORE_TRACE("Pencere yeniden boyutlandi: %ux%u piksel", w, h);

                    // Turetilen sinifa haber ver ki kamera projeksiyonunu
                    // yenileyebilsin. Bunu yapmazsak pencere boyutu degisince
                    // en-boy orani bozulur ve kareler dikdortgene doner.
                    OnWindowResize(w, h);
                    break;
                }

                case SDL_EVENT_WINDOW_MINIMIZED:
                    m_Minimized = true;
                    break;

                case SDL_EVENT_WINDOW_RESTORED:
                case SDL_EVENT_WINDOW_MAXIMIZED:
                    m_Minimized = false;
                    break;

                default:
                    break;
            }

            // Motor kendi isini bitirdi; simdi turetilen sinif da gorsun.
            // Sirasi onemli: motor kritik olaylari (cikis, boyut) her halukarda
            // isler, turetilen sinif bunlari engelleyemez.
            OnEvent(event);
        }
    }
}
