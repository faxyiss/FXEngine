#include "FXEngine/Core/Log.h"

#include <cstdarg>   // va_list, va_start - degisken sayida argüman
#include <cstdio>    // vsnprintf, fprintf
#include <ctime>     // zaman damgasi

#ifdef _WIN32
    // Windows.h devasa bir header. WIN32_LEAN_AND_MEAN cogu alt sistemi
    // (winsock, RPC, OLE...) disarida birakir -> derleme cok daha hizli.
    // NOMINMAX ise Windows'un min/max MAKROLARINI engeller; yoksa
    // std::min / glm::max gibi cagrilar bozulur. Bu ikisi Windows'ta
    // neredeyse her zaman gerekli.
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <Windows.h>
#endif

namespace FX
{
    namespace
    {
        // Anonim namespace: bu degiskenler/fonksiyonlar SADECE bu .cpp'ye ait.
        // C'deki "static" anahtar kelimesinin modern C++ karsiligi.
        // Baska bir dosya bunlara erisemez -> ic detay gercekten ic kalir.

        LogLevel s_MinLevel = LogLevel::Trace;

        // ANSI escape kodlari. Terminal bunlari renk komutu olarak yorumlar.
        // "\033[" ile baslar, "m" ile biter. 0 = tum bicimlendirmeyi sifirla.
        constexpr const char* COLOR_RESET  = "\033[0m";
        constexpr const char* COLOR_GRAY   = "\033[90m";
        constexpr const char* COLOR_WHITE  = "\033[37m";
        constexpr const char* COLOR_YELLOW = "\033[33m";
        constexpr const char* COLOR_RED    = "\033[1;31m";  // 1 = kalin
        constexpr const char* COLOR_CYAN   = "\033[36m";
        constexpr const char* COLOR_GREEN  = "\033[32m";

        const char* LevelColor(LogLevel level)
        {
            switch (level)
            {
                case LogLevel::Trace: return COLOR_GRAY;
                case LogLevel::Info:  return COLOR_WHITE;
                case LogLevel::Warn:  return COLOR_YELLOW;
                case LogLevel::Error: return COLOR_RED;
                default:              return COLOR_RESET;
            }
        }

        // Sabit genislikte etiket ("INFO " gibi) -> loglar sutun sutun hizali durur.
        // Kucuk detay ama 200 satirlik ciktida okunabilirligi ciddi artirir.
        const char* LevelName(LogLevel level)
        {
            switch (level)
            {
                case LogLevel::Trace: return "TRACE";
                case LogLevel::Info:  return "INFO ";
                case LogLevel::Warn:  return "WARN ";
                case LogLevel::Error: return "ERROR";
                default:              return "?????";
            }
        }

        const char* SourceName(LogSource source)
        {
            return (source == LogSource::Engine) ? "ENGINE" : "EDITOR";
        }

        const char* SourceColor(LogSource source)
        {
            // Motor ve editor farkli renkte -> goz akista ayirt eder.
            return (source == LogSource::Engine) ? COLOR_CYAN : COLOR_GREEN;
        }
    }

    void Log::Init()
    {
#ifdef _WIN32
        // Windows konsolu ANSI renk kodlarini VARSAYILAN OLARAK anlamaz;
        // "\033[33m" yazarsan ekrana cop karakter basar. Asagidaki bayrak
        // (Windows 10 1511+) VT100 yorumlamasini acar.
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE)
        {
            DWORD mode = 0;
            if (GetConsoleMode(hOut, &mode))
                SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }

        // Konsol ciktisini UTF-8 yap: Turkce karakterler bozulmasin.
        SetConsoleOutputCP(CP_UTF8);
#endif
        FX_CORE_INFO("Log sistemi hazir.");
    }

    void Log::SetLevel(LogLevel level) { s_MinLevel = level; }
    LogLevel Log::GetLevel()           { return s_MinLevel; }

    void Log::Print(LogSource source, LogLevel level, const char* format, ...)
    {
        // Filtre: seviye yeterince onemli degilse hic ugrasma.
        // Bu kontrolu EN BASTA yapiyoruz ki formatlama masrafi bile olusmasin.
        if (level < s_MinLevel || s_MinLevel == LogLevel::Off)
            return;

        // --- 1) Zaman damgasi -------------------------------------------------
        // Sadece saat:dakika:saniye. Tarih gereksiz; oturum zaten kisa.
        std::time_t now = std::time(nullptr);
        std::tm tmBuf{};
#ifdef _WIN32
        localtime_s(&tmBuf, &now);          // Windows'un guvenli surumu
#else
        localtime_r(&now, &tmBuf);          // POSIX'in guvenli surumu
#endif
        char timeStr[16];
        std::strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &tmBuf);

        // --- 2) Kullanicinin mesajini formatla --------------------------------
        // Sabit tamponla calisiyoruz: heap tahsisi yok, hizli ve basit.
        // 1024 karakter bir log satiri icin fazlasiyla yeterli; tasarsa
        // vsnprintf guvenle keser (buffer overflow olmaz).
        char message[1024];
        va_list args;
        va_start(args, format);
        std::vsnprintf(message, sizeof(message), format, args);
        va_end(args);

        // --- 3) Tek seferde bas ----------------------------------------------
        // Tek fprintf cagrisi kullaniyoruz cunku birden fazla cagri yapsaydik
        // ileride coklu thread'de satirlar birbirine karisirdi.
        // stderr degil stdout: bunlar hata degil, normal program ciktisi.
        std::fprintf(stdout, "%s[%s]%s %s%s%s %s[%s]%s %s%s\n",
                     COLOR_GRAY,          timeStr,               COLOR_RESET,
                     LevelColor(level),   LevelName(level),      COLOR_RESET,
                     SourceColor(source), SourceName(source),    COLOR_RESET,
                     LevelColor(level),   message);

        // Hatalarda tamponu hemen bosalt. Program crash ederse son log
        // satirinin kaybolmasini istemeyiz - genelde en kritik olan odur.
        if (level >= LogLevel::Error)
            std::fflush(stdout);
    }

    void Log::AssertFailed(const char* condition, const char* file, int line, const char* message)
    {
        std::fprintf(stdout,
                     "%s[ASSERT BASARISIZ]%s\n"
                     "  Kosul : %s\n"
                     "  Mesaj : %s\n"
                     "  Yer   : %s:%d\n",
                     COLOR_RED, COLOR_RESET,
                     condition, message, file, line);
        std::fflush(stdout);
    }
}
