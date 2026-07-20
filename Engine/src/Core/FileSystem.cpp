#include "FXEngine/Core/FileSystem.h"
#include "FXEngine/Core/Log.h"

#include <SDL3/SDL.h>

#include <fstream>

namespace FX
{
    const std::string& FileSystem::GetBaseDirectory()
    {
        // "Fonksiyon icinde static" kalibi: degisken ILK cagrida olusturulur
        // ve program boyunca yasar. C++11'den beri bu islem THREAD-GUVENLIDIR
        // (derleyici gerekli kilidi kendisi koyar).
        //
        // Neden global degisken degil? Cunku global'lerin olusma sirasi
        // dosyalar arasinda garanti degildir ("static initialization order
        // fiasco"). Bu kalip o sorunu tamamen ortadan kaldirir: ilk
        // KULLANIMDA olusur, ne once ne sonra.
        static std::string s_BaseDir = []() -> std::string
        {
            // SDL_GetBasePath() exe'nin klasorunu dondurur ve bunu
            // isletim sisteminden ogrenir (Windows'ta GetModuleFileName,
            // Linux'ta /proc/self/exe...). Yani tasinabilir sekilde
            // "ben neredeyim?" sorusunu cevaplar.
            const char* base = SDL_GetBasePath();
            if (!base)
            {
                FX_CORE_WARN("SDL_GetBasePath() basarisiz: %s", SDL_GetError());
                FX_CORE_WARN("Yollar calisma dizinine gore cozulecek.");
                return "./";
            }

            // NOT: SDL3'te SDL_GetBasePath'in dondurdugu isaretci SDL'e ait,
            // SDL_free ETMEMELIYIZ (SDL2'de tam tersiydi - orada free gerekiyordu).
            // Bu, SDL2 -> SDL3 gecisinde kolayca gozden kacan bir degisiklik.
            return std::string(base);
        }();

        return s_BaseDir;
    }

    std::string FileSystem::ResolveAsset(const std::string& relativePath)
    {
        // Zaten mutlak yol verilmisse dokunma.
        // Windows'ta "C:\..." veya "\\sunucu\...", POSIX'te "/..."
        if (!relativePath.empty())
        {
            if (relativePath[0] == '/' || relativePath[0] == '\\')
                return relativePath;
            if (relativePath.size() > 1 && relativePath[1] == ':')
                return relativePath;
        }

        return GetBaseDirectory() + relativePath;
    }

    bool FileSystem::Exists(const std::string& path)
    {
        // <filesystem> yerine ifstream kullaniyoruz: tek ihtiyacimiz
        // "acilabiliyor mu" sorusu ve bu her derleyicide sorunsuz calisir.
        std::ifstream f(path);
        return f.good();
    }
}
