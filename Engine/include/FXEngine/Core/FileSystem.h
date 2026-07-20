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

        // ===================================================================
        // IKI AYRI KOK (Faz 21)
        // ===================================================================
        // Bir editorun "KURULU OLDUGU YER" ile "UZERINDE CALISTIGI YER"
        // farkli seylerdir. Bu ayrimi yapmayan araclar tasinabilir olmaz:
        //
        //   MOTOR VARLIKLARI  (shader'lar)  -> exe'nin yani. Motorla
        //                                      birlikte gelir, kullanici
        //                                      bunlari duzenlemez.
        //   PROJE VARLIKLARI  (texture,     -> acik projenin klasoru.
        //                      sahne, prefab)  Kullanicinin isi burada.
        //
        // Faz 12'ye kadar ikisi ayni yerdeydi (<exe>/assets) ve ice
        // aktarilan her varlik build/ altinda kaliyordu: build'i silince
        // kullanicinin emegi de silinirdi.
        // ===================================================================

        // Acik projenin kok klasoru (sonunda ayirici ile).
        // Proje acilmadiysa base dizine esittir - eski davranis.
        static const std::string& GetProjectDirectory();

        // Proje acilirken cagrilir. Bos verilirse base dizine geri doner.
        static void SetProjectDirectory(const std::string& path);

        static bool HasProject();

        // MOTOR varligi: her zaman exe klasorune gore cozulur.
        //   ResolveEngineAsset("assets/shaders/X.vert")
        //     -> "C:/FXEngine/build/bin/assets/shaders/X.vert"
        static std::string ResolveEngineAsset(const std::string& relativePath);

        // PROJE varligi: acik projenin koküne gore cozulur.
        //   ResolveProjectAsset("assets/textures/x.png")
        //     -> "C:/Projelerim/OyunumX/assets/textures/x.png"
        static std::string ResolveProjectAsset(const std::string& relativePath);

        // ResolveProjectAsset'in TERSI. Dosya diyaloglari mutlak yol
        // dondurur ama sahne dosyasina mutlak yol yazmak felakettir:
        // proje baska bir makineye kopyalandiginda hicbir texture
        // bulunamaz. Varlik yollari her zaman PROJE KOKUNE goreceli
        // saklanmali.
        //
        // Yol proje kokunun disindaysa oldugu gibi doner (baska care yok).
        // Ayirici olarak '/' kullanir - dosyalarin platformlar arasi
        // tasinabilir olmasi icin.
        static std::string MakeRelativeToProject(const std::string& absolutePath);

        static bool Exists(const std::string& path);
    };
}
