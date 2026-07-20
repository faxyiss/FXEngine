#pragma once

// ===========================================================================
// FXEngine - Log sistemi
//
// TASARIM NOTU: Bu header motorun neredeyse her .cpp'sine dahil edilecek.
// Bu yuzden BILEREK cok hafif tutuldu: <string>, <iostream>, <vector> gibi
// agir standart header'lar burada YOK. Onlar Log.cpp'de. Bir header ne kadar
// cok yerden include ediliyorsa, o kadar yalin olmali - yoksa tum projenin
// derleme suresi sisirir.
// ===========================================================================

namespace FX
{
    // Log seviyeleri. Siralama onemli: sayisal degeri buyudukce onem artar.
    // Boylece "su seviyenin altini gosterme" filtresi basit bir >= karsilastirmasi olur.
    enum class LogLevel
    {
        Trace = 0,  // cok ayrintili, normalde kapali
        Info,       // "sey oldu" - normal akis
        Warn,       // "bu tuhaf ama devam edebiliriz"
        Error,      // "bu is patladi"
        Off         // hicbir sey gosterme (filtre degeri olarak kullanilir)
    };

    // Log kaynagi. Motor mu, editor mu? Ciktida [ENGINE]/[EDITOR] olarak gorunur.
    enum class LogSource
    {
        Engine,
        Editor
    };

    class Log
    {
    public:
        // Konsolu hazirlar (Windows'ta ANSI renk desteklerini acar).
        // main()'in EN BASINDA cagirilmali.
        static void Init();

        // Bu seviyenin ALTINDAKI loglar bastirilir.
        // Ornek: SetLevel(LogLevel::Warn) -> Trace ve Info susar.
        static void SetLevel(LogLevel level);
        static LogLevel GetLevel();

        // Asil yazma fonksiyonu. printf tarzi format alir.
        // Makrolar bunu cagirir; sen dogrudan cagirmayacaksin.
        static void Print(LogSource source, LogLevel level, const char* format, ...);

        // Assert basarisiz oldugunda cagrilir: mesaji basar, sonra debugger'i durdurur.
        static void AssertFailed(const char* condition, const char* file, int line, const char* message);
    };
}

// ---------------------------------------------------------------------------
// Makrolar
// ---------------------------------------------------------------------------
// NEDEN MAKRO, fonksiyon degil?
//   1) __FILE__ / __LINE__ gibi cagiran yerin bilgisine sadece makro erisebilir
//      (assert icin sart).
//   2) Release derlemesinde makroyu tamamen bosa cevirip log cagrisini
//      koddan SILEBILIRIZ - fonksiyon olsaydi cagri masrafi kalirdi.
//
// (...) ve __VA_ARGS__ kullaniyoruz: boylece FX_INFO("selam") de,
// FX_INFO("%d", 5) de calisir. "format, ##__VA_ARGS__" yazsaydik derleyiciye
// ozel bir uzantiya bagimli olurduk; bu haliyle tasinabilir.

// --- Motor cekirdegi icin (Engine/ altindaki kod bunlari kullanir) ---
#define FX_CORE_TRACE(...) ::FX::Log::Print(::FX::LogSource::Engine, ::FX::LogLevel::Trace, __VA_ARGS__)
#define FX_CORE_INFO(...)  ::FX::Log::Print(::FX::LogSource::Engine, ::FX::LogLevel::Info,  __VA_ARGS__)
#define FX_CORE_WARN(...)  ::FX::Log::Print(::FX::LogSource::Engine, ::FX::LogLevel::Warn,  __VA_ARGS__)
#define FX_CORE_ERROR(...) ::FX::Log::Print(::FX::LogSource::Engine, ::FX::LogLevel::Error, __VA_ARGS__)

// --- Uygulama/editor icin (Editor/ altindaki kod bunlari kullanir) ---
#define FX_TRACE(...) ::FX::Log::Print(::FX::LogSource::Editor, ::FX::LogLevel::Trace, __VA_ARGS__)
#define FX_INFO(...)  ::FX::Log::Print(::FX::LogSource::Editor, ::FX::LogLevel::Info,  __VA_ARGS__)
#define FX_WARN(...)  ::FX::Log::Print(::FX::LogSource::Editor, ::FX::LogLevel::Warn,  __VA_ARGS__)
#define FX_ERROR(...) ::FX::Log::Print(::FX::LogSource::Editor, ::FX::LogLevel::Error, __VA_ARGS__)

// ---------------------------------------------------------------------------
// Assert
// ---------------------------------------------------------------------------
// FELSEFE: Assert, "bu durum ASLA olmamali" demektir - kullanici hatasi degil,
// PROGRAMCI hatasi icin. Dosya bulunamadi -> bu bir hata, FX_ERROR ile logla.
// Ama "renderer baslatilmadan cizim yapildi" -> bu bir assert; kod yanlis.
//
// Debug'da programi debugger'da durdurur (satirini gorursun).
// Release'de tamamen kaybolur -> sifir maliyet.

#ifdef FX_DEBUG
    #ifdef _MSC_VER
        #define FX_DEBUGBREAK() __debugbreak()
    #else
        #include <csignal>
        #define FX_DEBUGBREAK() std::raise(SIGTRAP)
    #endif

    // do/while(0) sarmali: makronun tek bir ifade gibi davranmasini saglar,
    // boylece `if (x) FX_ASSERT(...); else ...` bozulmaz. Klasik C hilesi.
    #define FX_ASSERT(condition, message)                                          \
        do {                                                                       \
            if (!(condition)) {                                                    \
                ::FX::Log::AssertFailed(#condition, __FILE__, __LINE__, message);  \
                FX_DEBUGBREAK();                                                   \
            }                                                                      \
        } while (0)
#else
    #define FX_DEBUGBREAK()
    // Release'de kosul DEGERLENDIRILMEZ bile. Dikkat: bu yuzden assert icine
    // yan etkisi olan kod yazma! FX_ASSERT(Init(), "...") YANLIS kullanimdir.
    #define FX_ASSERT(condition, message) do { (void)sizeof(condition); } while (0)
#endif
