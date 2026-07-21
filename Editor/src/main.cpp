// ===========================================================================
// FXEditor - giris noktasi
//
// Bu dosya BILEREK cok kisa. Isi sadece:
//   1) Log sistemini baslat
//   2) Uygulamayi olustur ve calistir
//
// main() fonksiyonunun kisa olmasi iyi bir isarettir: mantigi siniflar
// tasiyorsa, program akisi tek bakista okunur.
// ===========================================================================

// SDL3'te <SDL3/SDL.h>, SDL_main.h'i de ceker ve main()'imizi kendi giris
// noktasiyla degistirir. Biz kendi main'imizi yonetmek istiyoruz.
//
// DIKKAT: SDL_MAIN_HANDLED kullandigimiz icin SDL_SetMainReady() cagirmak
// gerekir - SDL'e "giris noktasi hazirligini ben yaptim" der. Bunu atlarsan
// bazi platformlarda SDL_Init hata verir.
//
// SDL2'den fark: SDL3'te <SDL3/SDL.h> artik SDL_main.h'i KENDILIGINDEN
// dahil etmiyor. SDL_SetMainReady() o header'da yasadigi icin acikca
// include etmemiz gerekiyor.
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <FXEngine/Core/Log.h>
#include <FXEngine/Core/Version.h>

#include "EditorApp.h"

int main(int argc, char** argv)
{
    // 1) Log EN BASTA. Bundan onceki hicbir log dogru renklenmez.
    FX::Log::Init();
    FX_CORE_INFO("%s", FX::GetEngineVersion());

    // 2) SDL'e giris noktasini bizim yonettigimizi bildir.
    SDL_SetMainReady();

    // 3) Uygulamayi kur ve calistir.
    //
    // Kapsam (scope) parantezleri onemli: app'in yikicisi ']' geldiginde
    // calisir ve pencereyi/context'i temizler. Bu SDL_Quit'ten ONCE olmali,
    // yoksa SDL kapandiktan sonra SDL fonksiyonu cagirmis oluruz.
    {
        FXEd::EditorApp app;
        if (argc > 1)
            app.SetStartupProject(argv[1]);
        app.Run();
    }

    // 4) SDL'i tamamen kapat.
    SDL_Quit();

    FX_CORE_INFO("Program sonlandi.");
    return 0;
}
