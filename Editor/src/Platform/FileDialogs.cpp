#include "Platform/FileDialogs.h"

#include <FXEngine/Core/Window.h>
#include <FXEngine/Core/FileSystem.h>

#include <SDL3/SDL.h>

#include <cstring>
#include <filesystem>

#ifdef _WIN32
    // NOMINMAX: <windows.h> min/max makrolari tanimlar ve glm'in
    // std::min/max cagrilarini bozar. WIN32_LEAN_AND_MEAN derlemeyi hizlandirir.
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <windows.h>
    #include <commdlg.h>
#endif

namespace FXEd
{
    const char* FileDialogs::SceneFilter()
    {
        return "FXEngine Sahne (*.fxscene)\0*.fxscene\0Tum Dosyalar (*.*)\0*.*\0";
    }

    const char* FileDialogs::PrefabFilter()
    {
        return "FXEngine Prefab (*.fxprefab)\0*.fxprefab\0Tum Dosyalar (*.*)\0*.*\0";
    }

#ifdef _WIN32
    namespace
    {
        HWND NativeHandle(const FX::Window& window)
        {
            SDL_Window* sdlWindow = window.GetNativeWindow();
            if (!sdlWindow)
                return nullptr;

            // SDL3'te platforma ozel tutamaklar "property" sistemi uzerinden
            // aliniyor; SDL2'deki SDL_SysWMinfo kaldirildi.
            SDL_PropertiesID props = SDL_GetWindowProperties(sdlWindow);
            return static_cast<HWND>(SDL_GetPointerProperty(
                props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
        }

        // Dosya diyaloglari CALISMA DIZININI degistirebilir. Bu bizim icin
        // olumcul: assets yollari FileSystem uzerinden cozuluyor ama baska
        // her sey (nispi fstream acmalari) bozulurdu. OFN_NOCHANGEDIR ile
        // engelliyoruz - Win32'nin en cok unutulan bayraklarindan biri.
        constexpr DWORD kCommonFlags =
            OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;

        OPENFILENAMEA MakeOfn(const FX::Window& window, const char* filter,
                              char* buffer, DWORD bufferSize,
                              const std::string& initialDir)
        {
            OPENFILENAMEA ofn{};
            ofn.lStructSize     = sizeof(OPENFILENAMEA);
            ofn.hwndOwner       = NativeHandle(window);
            ofn.lpstrFile       = buffer;
            ofn.nMaxFile        = bufferSize;
            ofn.lpstrFilter     = filter;
            ofn.nFilterIndex    = 1;
            ofn.lpstrInitialDir = initialDir.c_str();
            ofn.Flags           = kCommonFlags;
            return ofn;
        }
    }

    std::string FileDialogs::OpenFile(const FX::Window& window, const char* filter)
    {
        char buffer[MAX_PATH]{};
        const std::string initial = FX::FileSystem::ResolveAsset("assets");

        OPENFILENAMEA ofn = MakeOfn(window, filter, buffer, sizeof(buffer), initial);
        ofn.Flags |= OFN_FILEMUSTEXIST;

        if (GetOpenFileNameA(&ofn) == TRUE)
            return buffer;

        return {};
    }

    std::string FileDialogs::SaveFile(const FX::Window& window, const char* filter)
    {
        char buffer[MAX_PATH]{};
        const std::string initial = FX::FileSystem::ResolveAsset("assets");

        OPENFILENAMEA ofn = MakeOfn(window, filter, buffer, sizeof(buffer), initial);
        ofn.Flags |= OFN_OVERWRITEPROMPT;

        // Kullanici uzanti yazmazsa filtreden tamamla. lpstrDefExt olmadan
        // "Sahnem" yazan kullanici uzantisiz bir dosya alirdi.
        const char* ext = filter + std::char_traits<char>::length(filter) + 1;
        std::string defExt;
        if (const char* dot = std::strrchr(ext, '.'))
            defExt = dot + 1;
        ofn.lpstrDefExt = defExt.empty() ? nullptr : defExt.c_str();

        if (GetSaveFileNameA(&ofn) == TRUE)
            return buffer;

        return {};
    }

#else
    // Linux/macOS: portali (zenity, nfd) sonraki bir fazda. Simdilik
    // sessizce iptal donuyoruz ki kod en azindan DERLENSIN.
    std::string FileDialogs::OpenFile(const FX::Window&, const char*) { return {}; }
    std::string FileDialogs::SaveFile(const FX::Window&, const char*) { return {}; }
#endif
}
