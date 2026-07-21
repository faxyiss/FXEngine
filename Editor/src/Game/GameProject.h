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
}
