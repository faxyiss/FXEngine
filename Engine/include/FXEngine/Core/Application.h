#pragma once

// ===========================================================================
// FXEngine - Application
//
// SORUMLULUK: Ana donguyu yonetmek. Pencereyi yaratir, olaylari toplar,
// sabit adimli (fixed timestep) guncelleme yapar, cizim cagrisini tetikler.
//
// Bu sinif SOYUTTUR (abstract). Motor donguyu KURAR ama icinde ne olacagini
// BILMEZ - onu turetilen sinif (Editor) doldurur. Boylece Engine, Editor'u
// hicbir zaman tanimak zorunda kalmaz; bagimlilik yalnizca yukaridan asagi akar.
// Bu kaliba "Template Method" denir.
// ===========================================================================

#include "FXEngine/Core/Window.h"
#include "FXEngine/Events/Event.h"

#include <memory>

// SDL_Event bir union; ileri bildirimi mumkun ama tur eksik olurdu.
// Bu yuzden takma ad kullaniyoruz: turetilen sinif isterse <SDL3/SDL.h>'i
// kendisi include eder. Header'imiz hafif kalir.
union SDL_Event;

namespace FX
{
    class Application
    {
    public:
        explicit Application(const WindowProps& props = {});
        virtual ~Application();

        Application(const Application&)            = delete;
        Application& operator=(const Application&) = delete;

        // Ana donguyu calistirir. Program bittiginde geri doner.
        void Run();

        // Donguyu disaridan durdurmak icin (orn. menuden "Cikis").
        void Close() { m_Running = false; }

        Window& GetWindow() { return *m_Window; }

    protected:
        // --- Turetilen sinifin dolduracagi kancalar (hooks) ------------------
        // Hepsi bos varsayilana sahip: Editor sadece ihtiyac duyduklarini yazar.

        // Dongu baslamadan bir kez. Kaynak yukleme burada yapilir.
        virtual void OnInit() {}

        // Dongu bittikten sonra bir kez. Temizlik burada.
        virtual void OnShutdown() {}

        // SABIT adimli mantik guncellemesi.
        // dt HER ZAMAN ayni deger (varsayilan 1/60 sn). Fizik, hareket,
        // oyun mantigi buraya yazilir -> makineden bagimsiz, deterministik.
        // Bir karede 0, 1 veya birden fazla kez cagrilabilir!
        virtual void OnUpdate(float dt) { (void)dt; }

        // Cizim. Kare basina TAM BIR KEZ cagrilir.
        //
        // alpha: son OnUpdate'ten bu yana biriken zamanin oranidir (0.0 - 1.0).
        // "Iki mantik adimi arasinda tam olarak neredeyiz?" sorusunun cevabi.
        // Bununla eski ve yeni konum arasinda ara deger hesaplayip (interpolasyon)
        // 60 Hz mantikla 144 Hz akici goruntu elde edebilirsin.
        // Faz 1'de kullanmiyoruz ama imzaya simdiden koyuyoruz ki sonra
        // her yeri degistirmek zorunda kalmayalim.
        virtual void OnRender(float alpha) { (void)alpha; }

        // HAM SDL olaylari. Yalnizca SDL_Event bekleyen tuketiciler
        // (ImGui backend'i, dosya birakma) icin. Yeni kod bunun yerine
        // OnEvent(Event&) kullanmali.
        virtual void OnRawEvent(const SDL_Event& event) { (void)event; }

        // MOTOR olaylari (Faz 13b). SDL'den bagimsiz; oyun ve editor
        // mantigi buraya yazilir.
        virtual void OnEvent(Event& event) { (void)event; }

        // Ham islemede olay tuketildiyse (orn. ImGui klavyeyi istiyor)
        // turetilen sinif bunu cagirir; motor olayi Handled gelir ve
        // ayni girdi iki kez islenmez.
        void MarkRawEventHandled() { m_RawEventHandled = true; }

        // Pencere piksel boyutu degisti. Motor viewport'u zaten guncelledi;
        // burasi turetilen sinifin kamera projeksiyonunu yenilemesi icin.
        //
        // Neden ayri bir kanca? OnEvent icinde de yakalanabilirdi ama o zaman
        // her turetilen sinif ayni SDL olay kodunu tekrar yazardi. Sik
        // ihtiyac duyulan bir olayi acik bir kancaya cikarmak, API'yi
        // kullanilabilir kilar.
        virtual void OnWindowResize(std::uint32_t width, std::uint32_t height)
        { (void)width; (void)height; }

    private:
        void ProcessEvents();

        std::unique_ptr<Window> m_Window;
        bool m_Running    = true;
        bool m_Minimized  = false;

        // Tek bir olay icin gecerli; her olaydan sonra sifirlanir.
        bool m_RawEventHandled = false;

        // Sabit adim: saniyede 60 mantik guncellemesi.
        // constexpr -> derleme zamaninda hesaplanir, calisma zamani maliyeti yok.
        static constexpr float  s_FixedDeltaTime = 1.0f / 60.0f;

        // Spiral of death korumasi: bir kare cok uzun surerse (debugger'da
        // durdun, pencere suruklendi), biriktirici sisip yuzlerce guncelleme
        // tetikler; bu daha da yavaslatir ve program kilitlenir. Bir karede
        // en fazla bu kadar guncelleme yapariz, gerisini ATARIZ.
        static constexpr int    s_MaxUpdatesPerFrame = 5;
    };
}
