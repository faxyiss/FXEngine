#pragma once

// ===========================================================================
// FXEngine - Framebuffer
//
// Normalde OpenGL cizimi dogrudan PENCEREYE gider. Framebuffer bunu
// degistirir: "cizdiklerini ekrana degil, su TEXTURE'a yaz" der.
//
// Sonucta elimizde sahnenin resmi bir texture olarak kalir. Onu istedigimiz
// yerde kullanabiliriz: bir ImGui panelinde gostermek (Faz 6'nin amaci),
// aynada yansitmak, minimap yapmak, uzerinde post-process efekti calistirmak.
//
// NEDEN ENGINE'DE, EDITOR'DE DEGIL?
// Cunku "kendi ciktina texture olarak ciz" bir RENDER yetenegidir, editor
// ozelligi degil. Oyun tarafinda da gerekir. ImGui ise Editor'e ait -
// sevk edilen bir oyunda editor arayuzu olmaz.
//
// Bir framebuffer'in iki tur ekine (attachment) ihtiyaci var:
//   RENK  -> texture olarak, cunku sonra okuyacagiz
//   DERINLIK -> renderbuffer olarak, cunku sadece test icin lazim,
//               okumayacagiz. Renderbuffer texture'dan daha ucuzdur.
// ===========================================================================

#include <cstdint>

namespace FX
{
    struct FramebufferSpec
    {
        std::uint32_t Width  = 1280;
        std::uint32_t Height = 720;
    };

    class Framebuffer
    {
    public:
        explicit Framebuffer(const FramebufferSpec& spec);
        ~Framebuffer();

        Framebuffer(const Framebuffer&)            = delete;
        Framebuffer& operator=(const Framebuffer&) = delete;

        // Bind'den sonra yapilan TUM cizimler bu framebuffer'a gider.
        // Unbind, varsayilan framebuffer'a (ekrana) geri doner.
        void Bind();
        void Unbind();

        // Boyut degistiginde tum ekleri YENIDEN OLUSTURUR.
        // OpenGL'de bir texture'in boyutunu "degistirmek" diye bir sey yok;
        // yeni bir tane yaratip eskisini silmek gerekir.
        void Resize(std::uint32_t width, std::uint32_t height);

        // ImGui'ye verecegimiz sey: renk ekinin texture kimligi.
        std::uint32_t GetColorAttachmentID() const { return m_ColorAttachment; }

        const FramebufferSpec& GetSpec() const { return m_Spec; }

        bool IsValid() const { return m_RendererID != 0; }

    private:
        // Tum GL nesnelerini (yeniden) olusturur.
        void Invalidate();

        std::uint32_t m_RendererID      = 0;
        std::uint32_t m_ColorAttachment = 0;   // texture
        std::uint32_t m_DepthAttachment = 0;   // renderbuffer

        FramebufferSpec m_Spec;
    };
}
