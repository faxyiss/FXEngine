#pragma once

// ===========================================================================
// FXEngine - FileWatcher
//
// Bir klasoru (alt klasorleriyle) izler ve degisiklikleri biriktirir.
//
// NEDEN GEREKLI? Faz 22'den beri varligin kimligi GUID; GUID -> yol tablosu
// AssetManager'da duruyor. Explorer'dan yapilan bir tasima bu tabloyu
// bayatlatiyor ve tutarlilik tamamen icerik panelinin dogru cagri yapmasina
// bagli kaliyordu. Izleyici bu bagi kopariyor: disk ne derse tablo o.
//
// MODEL: arka planda bir thread bloke olarak bekler, olaylari kuyruga
// yazar. Ana thread Poll() ile alir. Olaylari dogrudan thread icinde
// islemiyoruz - AssetManager ve paneller thread-safe degil ve oyle
// olmalarini istemiyoruz.
//
// SESSIZLIK BEKLEME (debounce): tek bir dosya kopyalamasi bile birden
// fazla olay uretir (created + modified + modified...). Poll, son olaydan
// sonra kisa bir sessizlik olusana kadar hicbir sey dondurmez; boylece
// yarim yazilmis dosyayi taramaya calismayiz.
// ===========================================================================

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace FX
{
    enum class FileAction
    {
        Added,
        Removed,
        Modified,
        Renamed
    };

    struct FileChange
    {
        FileAction  Action = FileAction::Modified;
        std::string Path;      // izlenen koke goreceli, '/' ayracli
        std::string OldPath;   // yalnizca Renamed icin dolu
    };

    class FileWatcher
    {
    public:
        FileWatcher() = default;
        ~FileWatcher();

        FileWatcher(const FileWatcher&)            = delete;
        FileWatcher& operator=(const FileWatcher&) = delete;

        // Zaten calisiyorsa once durdurur. Klasor yoksa false doner.
        bool Start(const std::string& absoluteRoot);
        void Stop();

        bool IsRunning() const { return m_Running.load(); }

        // Biriken degisiklikleri alir ve kuyrugu bosaltir. Son olayin
        // uzerinden quietSeconds gecmediyse BOS doner - degisiklikler
        // kuyrukta kalir, bir sonraki cagrida gelir.
        std::vector<FileChange> Poll(float quietSeconds = 0.25f);

    private:
        void ThreadMain();

        std::thread       m_Thread;
        std::atomic<bool> m_Running{ false };

        std::string m_Root;

        std::mutex              m_Mutex;
        std::vector<FileChange> m_Pending;
        double                  m_LastEventTime = 0.0;   // steady_clock, saniye

        // Windows HANDLE'lari. void* tutuluyor ki <windows.h> bu header'i
        // include eden herkese bulasmasin.
        void* m_DirHandle  = nullptr;
        void* m_StopEvent  = nullptr;
    };
}
