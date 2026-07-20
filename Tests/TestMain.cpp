// Catch2 main'i kutuphaneden geliyor (Catch2WithMain); burasi sadece
// testlerin ortak kurulumu icin.

#include <FXEngine/Core/Log.h>

namespace
{
    // Motor her sahne kaydinda ve her proje acilisinda INFO basiyor;
    // 26 testte bu, basarisizlik ciktisini goremeyecek kadar gurultu
    // demek. Uyari ve hatalar duruyor - onlari GORMEK istiyoruz.
    struct QuietLog
    {
        QuietLog() { FX::Log::SetLevel(FX::LogLevel::Warn); }
    };

    const QuietLog s_QuietLog;
}
