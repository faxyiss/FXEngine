#pragma once

// ===========================================================================
// FXEngine - FileSystem
//
// PROBLEM: "assets/shaders/X.vert" gibi GORECELI yollar, programin CALISMA
// DIZININE (current working directory) gore cozulur. Ama calisma dizini
// programi NASIL baslattigina gore degisir:
//
//   .\build\bin\FXEditor.exe        -> calisma dizini: C:\FXEngine     ✗
//   cd build\bin; .\FXEditor.exe    -> calisma dizini: C:\FXEngine\build\bin ✓
//   Visual Studio'dan F5            -> calisma dizini: proje klasoru   ✗
//   Exe'ye cift tiklama             -> calisma dizini: exe'nin klasoru ✓
//
// Yani ayni program, ayni dosyalar, ama bazen calisiyor bazen calismiyor.
// Bu, oyun geliztirmede klasik bir tuzaktir.
//
// COZUM: Yollari calisma dizinine degil, EXE'NIN BULUNDUGU KLASORE gore coz.
// Exe nerede duruyorsa assets/ de onun yanindadir - bu her zaman dogrudur.
// ===========================================================================

#include <string>

namespace FX
{
    class FileSystem
    {
    public:
        // Exe'nin bulundugu klasor (sonunda ayirici ile).
        // Ilk cagrida hesaplanir, sonra onbellekten doner.
        static const std::string& GetBaseDirectory();

        // Goreceli bir varlik yolunu exe klasorune gore tam yola cevirir.
        //   ResolveAsset("assets/shaders/X.vert")
        //     -> "C:/FXEngine/build/bin/assets/shaders/X.vert"
        static std::string ResolveAsset(const std::string& relativePath);

        // ResolveAsset'in TERSI. Dosya diyaloglari mutlak yol dondurur ama
        // sahne dosyasina mutlak yol yazmak felakettir: proje baska bir
        // makineye kopyalandiginda hicbir texture bulunamaz. Varlik yollari
        // her zaman exe klasorune GORECELI saklanmali.
        //
        // Yol base dizinin disindaysa oldugu gibi doner (baska care yok).
        // Ayirici olarak '/' kullanir - dosyalarin platformlar arasi
        // tasinabilir olmasi icin.
        static std::string MakeRelativeToBase(const std::string& absolutePath);

        static bool Exists(const std::string& path);
    };
}
