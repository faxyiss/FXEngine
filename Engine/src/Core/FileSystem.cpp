#include "FXEngine/Core/FileSystem.h"
#include "FXEngine/Core/Log.h"

#include <SDL3/SDL.h>

#include <filesystem>
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

    std::string FileSystem::MakeRelativeToBase(const std::string& absolutePath)
    {
        if (absolutePath.empty())
            return absolutePath;

        std::error_code ec;

        // weakly_canonical: sembolik baglari ve "..\" parcalarini duzler.
        // Duzlemezsek "C:/x/build/bin/../bin/assets" ile "C:/x/build/bin/assets"
        // farkli iki yol gibi gorunur ve goreceli hesap tutmaz.
        const auto base = std::filesystem::weakly_canonical(
            std::filesystem::path(GetBaseDirectory()), ec);
        const auto full = std::filesystem::weakly_canonical(
            std::filesystem::path(absolutePath), ec);

        if (ec)
            return absolutePath;

        const auto rel = std::filesystem::relative(full, base, ec);

        // Bos sonuc veya ".." ile baslamasi: dosya base'in disinda.
        if (ec || rel.empty() || rel.native().rfind(
                std::filesystem::path("..").native(), 0) == 0)
        {
            FX_CORE_WARN("Varlik exe klasorunun disinda, mutlak yol saklanacak: %s",
                         absolutePath.c_str());
            return absolutePath;
        }

        return rel.generic_string();
    }

    bool FileSystem::Exists(const std::string& path)
    {
        // <filesystem> yerine ifstream kullaniyoruz: tek ihtiyacimiz
        // "acilabiliyor mu" sorusu ve bu her derleyicide sorunsuz calisir.
        std::ifstream f(path);
        return f.good();
    }
}
