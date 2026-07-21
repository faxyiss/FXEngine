#pragma once

// ===========================================================================
// FXEditor - GameProject (Asama B-2)
//
// Oyun kodu (script'ler) artik editore degil, PROJEYE ait: her proje
// kendi Game.dll'ini derliyor. Bu dosya o derlemenin iskelesini kurar.
//
// Iskele <proje>/.fxbuild/ altina URETILIR, elle yazilmaz: motorun
// include yollari ya da kayit mekanizmasi degistiginde eski projelerin
// kirilmamasi icin. Kullanicinin sahiplendigi tek yer assets/scripts/.
// ===========================================================================

#include <string>

namespace FXEd::GameProject
{
    // Hepsi aktif projeye gore; proje yoksa bos donerler.
    std::string ScriptsDir();     // <proje>/assets/scripts
    std::string BuildDir();       // <proje>/.fxbuild
    std::string CMakeBinaryDir(); // <proje>/.fxbuild/cmake
    std::string DllPath();        // <proje>/.fxbuild/out/Game.dll

    // .fxbuild/ iskelesini (CMakeLists.txt, GameMain.cpp, sablon) ve
    // assets/scripts/ klasorunu olusturur. Icerik aynysa dosyaya
    // DOKUNMAZ - her yazma CMake'i gereksiz yere yeniden configure
    // etmeye zorlardi.
    bool WriteBuildFiles(std::string* error = nullptr);

    // Game.dll'i derler (B-5). Gerekirse once configure eder, sonra
    // `cmake --build`. cmake'in tum ciktisi (stdout+stderr) `output`'a
    // yazilir - editor bunu bir panelde gosterir. Donus cmake'in cikis
    // kodu: 0 basari. SENKRON: derleme bitene kadar bloklar (~birkac sn).
    int Build(const std::string& config, std::string& output);
}
