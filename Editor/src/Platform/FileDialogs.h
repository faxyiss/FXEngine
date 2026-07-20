#pragma once

// ===========================================================================
// FXEditor - yerel dosya diyaloglari
//
// NEDEN MOTORDA DEGIL EDITORDE?
// "Dosya ac penceresi" bir ISLETIM SISTEMI arayuz ozelligi. Motorun
// calisma zamaninda boyle bir seye ihtiyaci yok - oyun kullanicidan
// dosya secmez, sahnesini yolundan yukler. Bu, editorun isi.
//
// Fonksiyonlar bos string donerse kullanici IPTAL etti demektir.
// Bunu "hata" saymiyoruz; cagiran sessizce vazgecmeli.
// ===========================================================================

#include <string>
#include <vector>

namespace FX { class Window; }

namespace FXEd
{
    class FileDialogs
    {
    public:
        // filter: "Sahne (*.fxscene)\0*.fxscene\0" formatinda, CIFT '\0' ile biter.
        // Win32'nin bekledigi bicim bu; yardimcilar asagida hazir.
        static std::string OpenFile(const FX::Window& window, const char* filter);
        static std::string SaveFile(const FX::Window& window, const char* filter);

        // Coklu secim. Bos vektor = iptal.
        static std::vector<std::string> OpenFiles(const FX::Window& window, const char* filter);

        // Dosyayi isletim sisteminin dosya yoneticisinde secili gosterir.
        // Modal degil, ImGui cercevesi icinden cagrilabilir.
        static void RevealInFileManager(const std::string& absolutePath);

        static const char* SceneFilter();
        static const char* PrefabFilter();
        static const char* AssetFilter();
    };
}
