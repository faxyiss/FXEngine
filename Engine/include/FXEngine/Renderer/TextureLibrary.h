#pragma once

// ===========================================================================
// FXEngine - TextureLibrary
//
// PROBLEM: Sahneyi yuklerken her SpriteRenderer bir texture YOLU tasiyor.
// 100 entity ayni "circle.png"i kullaniyorsa, naif bir yukleyici dosyayi
// 100 kez diskten okur, 100 ayri GPU texture'i olusturur. Bellek ve
// yukleme suresi bosuna 100 katina cikar.
//
// COZUM: Yol -> texture esleme tablosu. Ayni yol ikinci kez istendiginde
// diske gitmez, mevcut texture'i dondurur.
//
// Bu, motor dunyasinda "asset cache" veya "resource manager" denen seyin
// en yalin halidir. Gercek motorlarda burada referans sayimi, tembel
// yukleme, sicak yeniden yukleme (hot reload) da olur - simdilik
// ihtiyacimiz olan tek sey tekrarli yuklemeyi onlemek.
// ===========================================================================

#include "FXEngine/Renderer/Texture.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace FX
{
    class TextureLibrary
    {
    public:
        // Dosyanin dokusunu verir. Ayarlar VARLIGIN KENDI .meta
        // dosyasindan gelir (AssetManager), cagirandan degil.
        //
        // ESKIDEN iki ayri imza vardi: biri "ne varsa ver", digeri
        // "bu ayarlarla istiyorum". Ikincisi bir catisma uretiyordu -
        // ayni dosyayi farkli ayarlarla isteyen ikinci cagiran sessizce
        // ilkininkini aliyordu ve kutuphane bunu uyari basarak haber
        // vermek zorunda kaliyordu.
        //
        // Ayar dosyaya baglaninca "kim once istedi" sorusu tumden
        // ortadan kalkti: bir dosyanin ayari her zaman kendi .meta'sinda
        // yazili olandir. Uyariya da gerek kalmadi.
        std::shared_ptr<Texture2D> Load(const std::string& path);

        // Bir dokuyu diskten YENIDEN yukler (.meta ayarlari degistiginde).
        // Onbellekteki nesne AYNI kalir - sahnedeki shared_ptr'lar
        // gecerliligini korur, sadece icerigi guncellenir.
        std::shared_ptr<Texture2D> Reload(const std::string& path);

        // Disaridan olusturulmus bir texture'i onbellege ekler.
        // Editor'un elle yukledigi dokularin da tabloda gorunmesi icin -
        // boylece kaydet/yukle sonrasinda ayni nesne kullanilir.
        void Add(const std::string& path, const std::shared_ptr<Texture2D>& texture);

        std::shared_ptr<Texture2D> Get(const std::string& path) const;
        bool Exists(const std::string& path) const;

        void Clear();

        std::size_t GetCount() const { return m_Textures.size(); }

        // Yuklenemeyen bir dokunun yerine gecen mor/siyah damali "eksik
        // doku". Sessizce gorunmez kalmak yerine EKRANDA belli olsun -
        // 2D grafik yazan herkesin tanidigi "bir sey eksik" isareti.
        // Ilk istendiginde uretilir (OpenGL baglami gerektirir).
        std::shared_ptr<Texture2D> MissingTexture();

    private:
        // Varligin .meta'sindaki ice aktarma ayarlarini TextureSpec'e
        // cevirir. Varlik kayitli degilse (proje disi, .meta yok)
        // varsayilanlar kullanilir.
        static TextureSpec SpecForAsset(const std::string& path);

        // Tembel uretilen "eksik doku" (yukaridaki MissingTexture).
        std::shared_ptr<Texture2D> m_Missing;

        // shared_ptr: kutuphane texture'i tutar, entity'ler de tutar.
        // Kutuphane temizlense bile hala kullanilan bir texture yasamaya
        // devam eder - sahiplik paylasimi tam da bu durum icin var.
        std::unordered_map<std::string, std::shared_ptr<Texture2D>> m_Textures;

    };
}
