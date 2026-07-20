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
        // "Bu dosyanin dokusunu ver, ayarlari umursamiyorum."
        // Onbellekte varsa onu doner, yoksa varsayilan ayarlarla yukler.
        // Icerik panelinin onizlemeleri gibi yerler icin.
        std::shared_ptr<Texture2D> Load(const std::string& path);

        // "Bu dosyayi BU ayarlarla istiyorum."
        // Dosya daha once BASKA ayarlarla yuklendiyse uyari basar ve
        // ilk yuklemeninkini doner (onbellek yola gore anahtarli).
        //
        // Iki ayri imza olmasinin sebebi: uyari ancak ISTEYEN birine
        // anlamli. Varsayilan argumanli tek bir fonksiyon olsaydi,
        // hicbir ayar talep etmeyen cagrilar da uyari alirdi - nitekim
        // aliyorlardi: icerik paneli bir klasore her girisinde
        // "farkli spec" uyarisi basiyordu.
        std::shared_ptr<Texture2D> Load(const std::string& path,
                                        const TextureSpec& spec);

        // Disaridan olusturulmus bir texture'i onbellege ekler.
        // Editor'un elle yukledigi dokularin da tabloda gorunmesi icin -
        // boylece kaydet/yukle sonrasinda ayni nesne kullanilir.
        void Add(const std::string& path, const std::shared_ptr<Texture2D>& texture);

        std::shared_ptr<Texture2D> Get(const std::string& path) const;
        bool Exists(const std::string& path) const;

        void Clear();

        std::size_t GetCount() const { return m_Textures.size(); }

    private:
        std::shared_ptr<Texture2D> LoadInternal(const std::string& path,
                                                const TextureSpec& spec,
                                                bool warnOnSpecMismatch);

        // shared_ptr: kutuphane texture'i tutar, entity'ler de tutar.
        // Kutuphane temizlense bile hala kullanilan bir texture yasamaya
        // devam eder - sahiplik paylasimi tam da bu durum icin var.
        std::unordered_map<std::string, std::shared_ptr<Texture2D>> m_Textures;

        // Hangi dosya hangi ayarlarla yuklendi? Sadece "farkli spec ile
        // tekrar istendi" uyarisini verebilmek icin tutuluyor.
        std::unordered_map<std::string, TextureSpec> m_Specs;
    };
}
