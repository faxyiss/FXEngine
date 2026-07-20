#pragma once

// ===========================================================================
// FXEngine - Texture2D
//
// Texture, GPU bellegindeki bir goruntudur. Ama sadece "resim" degil:
// GPU icin texture, UV koordinatiyla ORNEKLENEBILEN (sample) bir veri
// tablosudur. Renk saklar ama isik haritasi, yukseklik verisi de saklayabilir.
//
// Bu sinif stb_image ile dosyadan yukler ve OpenGL texture nesnesine cevirir.
// ===========================================================================

#include <cstdint>
#include <string>

namespace FX
{
    // Buyutuldugunde/kucultuldugunde pikseller nasil yorumlansin?
    enum class TextureFilter
    {
        // En yakin pikseli aynen al -> keskin, blok blok gorunur.
        // Pixel-art oyunlar icin DOGRU secim budur.
        Nearest,

        // Komsu 4 pikselin agirlikli ortalamasi -> yumusak, bulanik.
        // Fotografik/yuksek cozunurluklu varliklar icin uygun.
        Linear
    };

    // UV koordinati 0..1 disina cikarsa ne olsun?
    enum class TextureWrap
    {
        Repeat,        // basa sar -> desen tekrarlanir (zemin, duvar)
        ClampToEdge,   // kenar pikselini uzat -> tek sprite icin dogru secim
        MirroredRepeat // aynalayarak tekrarla
    };

    struct TextureSpec
    {
        TextureFilter MinFilter = TextureFilter::Linear;
        TextureFilter MagFilter = TextureFilter::Linear;
        TextureWrap   WrapS     = TextureWrap::ClampToEdge;
        TextureWrap   WrapT     = TextureWrap::ClampToEdge;

        // Dikey cevirme. stb_image resmi ust satirdan okur, OpenGL ise
        // alt satirdan bekler -> cevirmezsek goruntu bas asagi cikar.
        // Ayrintili aciklama Texture.cpp'de.
        bool FlipVertically = true;

        // Mipmap: texture'in kucultulmus kopyalari. Uzaktaki/kucuk
        // nesnelerde titremeyi (aliasing) onler. 2D'de her zaman gerekmez
        // ama zoom-out yapilan bir editorde faydali.
        bool GenerateMipmaps = true;
    };

    class Texture2D
    {
    public:
        // Dosyadan yukle. Basarisizsa IsValid() false doner (istisna atmayiz).
        Texture2D(const std::string& path, const TextureSpec& spec = {});

        // Bellekteki ham pikselden olustur. Faz 4'te "beyaz texture" icin
        // gerekecek; simdilik hata durumunda yedek doku uretmek icin kullaniyoruz.
        Texture2D(std::uint32_t width, std::uint32_t height,
                  const void* data, std::uint32_t channels = 4,
                  const TextureSpec& spec = {});

        ~Texture2D();

        Texture2D(const Texture2D&)            = delete;
        Texture2D& operator=(const Texture2D&) = delete;

        // Texture'i bir "slot"a bagla.
        //
        // SLOT NEDIR? GPU ayni anda birden fazla texture kullanabilir
        // (orn. renk + normal haritasi). Her biri numarali bir yuvaya baglanir.
        // Shader'daki sampler2D uniform'una o YUVA NUMARASINI verirsin.
        // GL 3.3 en az 16 slot garanti eder. Faz 4'te bu ciddi ise yarayacak.
        void Bind(std::uint32_t slot = 0) const;
        void Unbind() const;

        std::uint32_t GetWidth()  const { return m_Width; }
        std::uint32_t GetHeight() const { return m_Height; }
        std::uint32_t GetRendererID() const { return m_RendererID; }
        const std::string& GetPath() const { return m_Path; }

        bool IsValid() const { return m_RendererID != 0; }

    private:
        // Ortak kurulum: GL nesnesi yarat, parametreleri uygula, veriyi yukle.
        void Create(const void* data, std::uint32_t channels, const TextureSpec& spec);

        std::uint32_t m_RendererID = 0;
        std::uint32_t m_Width      = 0;
        std::uint32_t m_Height     = 0;
        std::string   m_Path;
    };
}
