#include "FXEngine/Core/FileWatcher.h"
#include "FXEngine/Core/Log.h"

#include <chrono>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

namespace FX
{
    namespace
    {
        double NowSeconds()
        {
            using namespace std::chrono;
            return duration<double>(steady_clock::now().time_since_epoch()).count();
        }
    }

    FileWatcher::~FileWatcher()
    {
        Stop();
    }

    std::vector<FileChange> FileWatcher::Poll(float quietSeconds)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);

        if (m_Pending.empty())
            return {};

        if (NowSeconds() - m_LastEventTime < quietSeconds)
            return {};

        return std::move(m_Pending);
    }

#ifdef _WIN32

    bool FileWatcher::Start(const std::string& absoluteRoot)
    {
        Stop();

        HANDLE dir = CreateFileA(
            absoluteRoot.c_str(),
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            nullptr);

        if (dir == INVALID_HANDLE_VALUE)
        {
            FX_WARN("FileWatcher: klasor acilamadi (%s)", absoluteRoot.c_str());
            return false;
        }

        m_DirHandle = dir;
        m_StopEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        m_Root      = absoluteRoot;

        m_Running.store(true);
        m_Thread = std::thread(&FileWatcher::ThreadMain, this);

        FX_INFO("FileWatcher: izleniyor (%s)", absoluteRoot.c_str());
        return true;
    }

    void FileWatcher::Stop()
    {
        if (!m_Running.load())
            return;

        m_Running.store(false);

        // Thread ReadDirectoryChangesW icinde bloke; olayi kurup
        // bekleyisini kiriyoruz. CancelIoEx bekleyen okumayi da atiyor.
        if (m_StopEvent)
            SetEvent(static_cast<HANDLE>(m_StopEvent));
        if (m_DirHandle)
            CancelIoEx(static_cast<HANDLE>(m_DirHandle), nullptr);

        if (m_Thread.joinable())
            m_Thread.join();

        if (m_DirHandle)
        {
            CloseHandle(static_cast<HANDLE>(m_DirHandle));
            m_DirHandle = nullptr;
        }
        if (m_StopEvent)
        {
            CloseHandle(static_cast<HANDLE>(m_StopEvent));
            m_StopEvent = nullptr;
        }

        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Pending.clear();
    }

    void FileWatcher::ThreadMain()
    {
        // 64 KB: ayri bir okumaya sigmayan degisiklik yigini nadir.
        // Tasarsa Windows bize sifir uzunlukta bildirim veriyor; o durumda
        // "ne degistigini bilmiyoruz" diyip tam tarama isteyecegiz.
        std::vector<char> buffer(64 * 1024);

        OVERLAPPED overlapped{};
        overlapped.hEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);

        const DWORD filter = FILE_NOTIFY_CHANGE_FILE_NAME |
                             FILE_NOTIFY_CHANGE_DIR_NAME |
                             FILE_NOTIFY_CHANGE_LAST_WRITE;

        std::string pendingOldName;   // RENAMED_OLD_NAME ile NEW_NAME arasi

        while (m_Running.load())
        {
            ResetEvent(overlapped.hEvent);

            DWORD bytes = 0;
            if (!ReadDirectoryChangesW(static_cast<HANDLE>(m_DirHandle),
                                       buffer.data(), static_cast<DWORD>(buffer.size()),
                                       TRUE, filter, &bytes, &overlapped, nullptr))
            {
                break;
            }

            HANDLE waits[2] = { overlapped.hEvent, static_cast<HANDLE>(m_StopEvent) };
            const DWORD which = WaitForMultipleObjects(2, waits, FALSE, INFINITE);

            if (which != WAIT_OBJECT_0 || !m_Running.load())
                break;

            if (!GetOverlappedResult(static_cast<HANDLE>(m_DirHandle),
                                     &overlapped, &bytes, FALSE))
            {
                break;
            }

            std::vector<FileChange> batch;

            if (bytes == 0)
            {
                // Tampon tasti: hangi dosyalarin degistigini bilmiyoruz.
                // Yolu bos birakmak cagirana "tam tarama gerek" demek.
                batch.push_back({ FileAction::Modified, {}, {} });
            }
            else
            {
                const char* cursor = buffer.data();
                for (;;)
                {
                    const auto* info =
                        reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(cursor);

                    const int wideLength =
                        static_cast<int>(info->FileNameLength / sizeof(WCHAR));

                    std::string path;
                    if (wideLength > 0)
                    {
                        const int needed = WideCharToMultiByte(
                            CP_UTF8, 0, info->FileName, wideLength,
                            nullptr, 0, nullptr, nullptr);

                        path.resize(static_cast<std::size_t>(needed));
                        WideCharToMultiByte(CP_UTF8, 0, info->FileName, wideLength,
                                            path.data(), needed, nullptr, nullptr);

                        for (char& c : path)
                            if (c == '\\') c = '/';
                    }

                    switch (info->Action)
                    {
                        case FILE_ACTION_ADDED:
                            batch.push_back({ FileAction::Added, path, {} });
                            break;
                        case FILE_ACTION_REMOVED:
                            batch.push_back({ FileAction::Removed, path, {} });
                            break;
                        case FILE_ACTION_MODIFIED:
                            batch.push_back({ FileAction::Modified, path, {} });
                            break;
                        case FILE_ACTION_RENAMED_OLD_NAME:
                            pendingOldName = path;
                            break;
                        case FILE_ACTION_RENAMED_NEW_NAME:
                            // Eski ad gelmediyse (tampon sinirinda bolunmus
                            // olabilir) bunu yeni dosya sayiyoruz: yanlis
                            // eslesme yapmaktansa yeni GUID vermek iyidir.
                            if (pendingOldName.empty())
                                batch.push_back({ FileAction::Added, path, {} });
                            else
                                batch.push_back({ FileAction::Renamed, path, pendingOldName });
                            pendingOldName.clear();
                            break;
                        default:
                            break;
                    }

                    if (info->NextEntryOffset == 0)
                        break;
                    cursor += info->NextEntryOffset;
                }
            }

            if (!batch.empty())
            {
                std::lock_guard<std::mutex> lock(m_Mutex);
                m_Pending.insert(m_Pending.end(), batch.begin(), batch.end());
                m_LastEventTime = NowSeconds();
            }
        }

        CloseHandle(overlapped.hEvent);
    }

#else
    // Linux/macOS: inotify / FSEvents ayri bir is. Izleyici olmadan da
    // editor calisir - sadece "Yenile" gerekir.
    bool FileWatcher::Start(const std::string&) { return false; }
    void FileWatcher::Stop() {}
    void FileWatcher::ThreadMain() {}
#endif
}
