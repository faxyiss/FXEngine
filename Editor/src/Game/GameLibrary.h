#pragma once

// ===========================================================================
// FXEditor - GameLibrary (Asama B-3)
//
// Oyun kodunu tasiyan Game.dll'i yukler, script kayitlarini alir ve
// birakir. Motor (FXEngine.dll) surec genelinde tek oldugu icin DLL'in
// kaydettigi script'leri editor de gorur - B-1'in tum amaci buydu.
//
// Bu adimda DLL DOGRUDAN yukleniyor. Windows dosyayi kilitler; uzerine
// yeniden derleme icin golge kopya gerekir - o B-4.
// ===========================================================================

#include <string>

namespace FXEd
{
    class GameLibrary
    {
    public:
        GameLibrary() = default;
        ~GameLibrary();

        GameLibrary(const GameLibrary&)            = delete;
        GameLibrary& operator=(const GameLibrary&) = delete;

        // Onceki DLL'i birakir, ScriptRegistry'yi temizler, yeni DLL'i
        // yukler ve script'lerini kaydeder. Yol yoksa ya da yukleme
        // basarisizsa false; ScriptRegistry yine de temiz birakilir.
        bool Load(const std::string& dllPath, std::string* error = nullptr);

        // DLL'i birakir ve ScriptRegistry'yi temizler.
        void Unload();

        bool IsLoaded() const { return m_Handle != nullptr; }

        // EnTT tip kimligi + veri erisimi DLL sinirinda calisiyor mu?
        // Load'dan sonra bir kez cagrilir. DLL'de sembol yoksa false.
        bool RunSelfTest(std::string* detail = nullptr);

    private:
        void* m_Handle = nullptr;  // HMODULE (windows.h'i header'a sizdirmamak icin void*)
    };
}
