#include "FXEngine/Renderer/Texture.h"
#include "FXEngine/Core/Log.h"
#include "FXEngine/Core/FileSystem.h"

#include <glad/glad.h>

// stb_image TEK BASLIKLI (header-only) bir kutuphanedir ve alisilmadik bir
// yontem kullanir: normalde sadece bildirimleri verir; STB_IMAGE_IMPLEMENTATION
// tanimlanan TEK BIR .cpp dosyasinda ise tum govdeyi de uretir.
//
// Bu makroyu iki farkli .cpp'de tanimlarsan "duplicate symbol" linker hatasi
// alirsin. Bu yuzden kural: tek bir dosyada, ve o dosya bu.
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace FX
{
    namespace
    {
        GLenum ToGLFilter(TextureFilter filter, bool useMipmap)
        {
            if (filter == TextureFilter::Nearest)
                // Mipmap varsa hem mipmap seviyeleri hem pikseller arasi
                // secim yapilir. GL_NEAREST_MIPMAP_LINEAR: seviyeler arasi
                // yumusak gecis ama piksel keskin - pixel-art icin ideal.
                return useMipmap ? GL_NEAREST_MIPMAP_LINEAR : GL_NEAREST;

            return useMipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;
        }

        GLenum ToGLWrap(TextureWrap wrap)
        {
            switch (wrap)
            {
                case TextureWrap::Repeat:         return GL_REPEAT;
                case TextureWrap::ClampToEdge:    return GL_CLAMP_TO_EDGE;
                case TextureWrap::MirroredRepeat: return GL_MIRRORED_REPEAT;
            }
            return GL_CLAMP_TO_EDGE;
        }
    }

    Texture2D::Texture2D(const std::string& path, const TextureSpec& spec)
        : m_Path(path)
    {
        const std::string fullPath = FileSystem::ResolveAsset(path);

        // ---------------------------------------------------------------
        // stb_image'in dikey cevirme ayari
        // ---------------------------------------------------------------
        // PNG/JPG dosyalari goruntuyu UST satirdan basa dogru saklar
        // (ekran koordinati mantigi: y asagi artar).
        //
        // OpenGL ise texture'i ALT satirdan baslar sayar: UV (0,0) SOL ALT
        // kosedir.
        //
        // Cevirmezsek her sprite bas asagi cikar. Bu, 2D grafik yazan
        // herkesin en az bir kez dustugu tuzaktir - ve belirtisi cok net:
        // "her sey ters".
        //
        // NOT: Bu ayar GLOBAL bir durumdur, texture basina degil. Bu yuzden
        // her yuklemeden once set ediyoruz.
        stbi_set_flip_vertically_on_load(spec.FlipVertically ? 1 : 0);

        int width = 0, height = 0, channels = 0;

        // Son parametre 0 = "dosyada kac kanal varsa onu ver".
        // 4 yazsaydik her seyi RGBA'ya zorlardik; bazen istenir ama
        // gri tonlamali bir maskeyi 4 kanala sismek bellek israfidir.
        stbi_uc* data = stbi_load(fullPath.c_str(), &width, &height, &channels, 0);

        if (!data)
        {
            FX_CORE_ERROR("Texture yuklenemedi: %s", fullPath.c_str());
            FX_CORE_ERROR("  stb_image: %s", stbi_failure_reason());
            return;
        }

        m_Width  = static_cast<std::uint32_t>(width);
        m_Height = static_cast<std::uint32_t>(height);

        Create(data, static_cast<std::uint32_t>(channels), spec);

        // stb kendi malloc'unu kullanir -> kendi free'siyle birakilmali.
        // delete[] veya free() KULLANMA; stb ileride ayirici degistirebilir.
        stbi_image_free(data);

        FX_CORE_INFO("Texture yuklendi: %s (%ux%u, %d kanal, id=%u)",
                     path.c_str(), m_Width, m_Height, channels, m_RendererID);
    }

    Texture2D::Texture2D(std::uint32_t width, std::uint32_t height,
                         const void* data, std::uint32_t channels,
                         const TextureSpec& spec)
        : m_Width(width), m_Height(height), m_Path("<bellekten>")
    {
        Create(data, channels, spec);
    }

    void Texture2D::Create(const void* data, std::uint32_t channels, const TextureSpec& spec)
    {
        // internalFormat: GPU'nun veriyi NASIL SAKLAYACAGI
        // dataFormat    : BIZIM verdigimiz verinin duzeni
        // Ikisi genelde ayni ama ayri parametreler; farkli olabilecekleri
        // durumlar var (orn. sRGB saklama, RGB veri).
        GLenum internalFormat = 0;
        GLenum dataFormat     = 0;

        switch (channels)
        {
            case 4: internalFormat = GL_RGBA8; dataFormat = GL_RGBA; break;
            case 3: internalFormat = GL_RGB8;  dataFormat = GL_RGB;  break;
            case 1: internalFormat = GL_R8;    dataFormat = GL_RED;  break;
            default:
                FX_CORE_ERROR("Desteklenmeyen kanal sayisi: %u", channels);
                return;
        }

        glGenTextures(1, &m_RendererID);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);

        // --- Hizalama tuzagi -------------------------------------------------
        // OpenGL varsayilan olarak her satirin 4 baytin kati oldugunu varsayar.
        // 3 kanalli (RGB) ve genisligi 4'un kati OLMAYAN bir goruntude
        // (orn. 254x254 RGB) bu varsayim tutmaz ve goruntu KAYARAK bozulur -
        // egik cizgiler halinde. Cok kafa karistirici bir hatadir.
        // 1 = "satirlari hizalama, bayt bayt oku".
        if (dataFormat == GL_RGB || dataFormat == GL_RED)
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        // Veriyi GPU'ya yukle.
        glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internalFormat),
                     static_cast<GLsizei>(m_Width), static_cast<GLsizei>(m_Height),
                     0, dataFormat, GL_UNSIGNED_BYTE, data);

        // Mipmap'ler glTexImage2D'DEN SONRA uretilmeli - taban seviye
        // yuklenmeden kucultulmus kopyalari uretilemez.
        if (spec.GenerateMipmaps)
            glGenerateMipmap(GL_TEXTURE_2D);

        // --- Filtreleme ve sarma ---------------------------------------------
        // MIN: texture ekranda KUCULTULDUGUNDE (bir piksel birden fazla
        //      texel kapsar) -> mipmap burada devreye girer
        // MAG: texture BUYUTULDUGUNDE (bir texel birden fazla piksel kaplar)
        //      -> mipmap anlamsizdir, o yuzden useMipmap=false
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        static_cast<GLint>(ToGLFilter(spec.MinFilter, spec.GenerateMipmaps)));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                        static_cast<GLint>(ToGLFilter(spec.MagFilter, false)));

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<GLint>(ToGLWrap(spec.WrapS)));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<GLint>(ToGLWrap(spec.WrapT)));

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    Texture2D::~Texture2D()
    {
        if (m_RendererID)
            glDeleteTextures(1, &m_RendererID);
    }

    void Texture2D::Bind(std::uint32_t slot) const
    {
        // IKI ADIM, sirasi onemli:
        //   1) Hangi slot aktif olsun?
        //   2) O slota hangi texture baglansin?
        // glActiveTexture'i unutursan her texture 0 numarali slota gider
        // ve sadece sonuncusu gorunur.
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);
    }

    void Texture2D::Unbind() const
    {
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}
