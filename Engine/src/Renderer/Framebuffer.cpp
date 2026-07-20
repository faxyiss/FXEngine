#include "FXEngine/Renderer/Framebuffer.h"
#include "FXEngine/Core/Log.h"

#include <glad/glad.h>

namespace FX
{
    namespace
    {
        // GPU'lar cok buyuk texture'lari reddeder. Ayrica kullanici pencereyi
        // asiri kucultunce 0 boyut gelebilir ve bu GL hatasi uretir.
        constexpr std::uint32_t MAX_FRAMEBUFFER_SIZE = 8192;
    }

    Framebuffer::Framebuffer(const FramebufferSpec& spec)
        : m_Spec(spec)
    {
        Invalidate();
    }

    Framebuffer::~Framebuffer()
    {
        glDeleteFramebuffers(1, &m_RendererID);
        glDeleteTextures(1, &m_ColorAttachment);
        glDeleteRenderbuffers(1, &m_DepthAttachment);
    }

    void Framebuffer::Invalidate()
    {
        // Zaten varsa once eskisini temizle. Resize her cagrildiginda
        // buraya giriyoruz; temizlemezsek her boyut degisiminde GPU'da
        // bir framebuffer daha birikir (klasik kaynak sizintisi).
        if (m_RendererID)
        {
            glDeleteFramebuffers(1, &m_RendererID);
            glDeleteTextures(1, &m_ColorAttachment);
            glDeleteRenderbuffers(1, &m_DepthAttachment);

            m_RendererID      = 0;
            m_ColorAttachment = 0;
            m_DepthAttachment = 0;
        }

        glGenFramebuffers(1, &m_RendererID);
        glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);

        // --- RENK EKI: texture ------------------------------------------------
        // Texture kullaniyoruz cunku sonra bunu OKUYACAGIZ (ImGui'de gosterecegiz).
        glGenTextures(1, &m_ColorAttachment);
        glBindTexture(GL_TEXTURE_2D, m_ColorAttachment);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                     static_cast<GLsizei>(m_Spec.Width), static_cast<GLsizei>(m_Spec.Height),
                     0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);   // nullptr = sadece yer ayir

        // GL_LINEAR: viewport paneli tam piksel boyutunda olmayabilir,
        // yumusak olceklensin.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // CLAMP_TO_EDGE: kenarlarda karsi taraftan sizinti olmasin.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, m_ColorAttachment, 0);

        // --- DERINLIK EKI: renderbuffer ---------------------------------------
        // Renderbuffer, "yazilabilir ama okunamaz" bir tampondur.
        // Derinlik degerlerini hicbir zaman okumayacagiz (sadece testte
        // kullaniliyor), o yuzden texture'a gerek yok - renderbuffer daha ucuz
        // ve surucu onu daha iyi optimize edebilir.
        //
        // GL_DEPTH24_STENCIL8: 24 bit derinlik + 8 bit stencil tek tamponda.
        // Stencil'i su an kullanmiyoruz ama bu format standart ve her yerde
        // desteklenir; ayirmak ekstra bellek maliyeti getirmez.
        glGenRenderbuffers(1, &m_DepthAttachment);
        glBindRenderbuffer(GL_RENDERBUFFER, m_DepthAttachment);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
                              static_cast<GLsizei>(m_Spec.Width),
                              static_cast<GLsizei>(m_Spec.Height));
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                  GL_RENDERBUFFER, m_DepthAttachment);

        // --- Dogrulama --------------------------------------------------------
        // BU KONTROLU ATLAMA. Eksik/uyumsuz bir framebuffer'a cizim yapmak
        // sessizce basarisiz olur: hata almazsin, sadece ekran bos kalir.
        const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            FX_CORE_ERROR("Framebuffer eksik! Durum kodu: 0x%X (%ux%u)",
                          status, m_Spec.Width, m_Spec.Height);
        }

        // Varsayilan framebuffer'a geri don, yoksa sonraki cizimler
        // yanlislikla buraya gider.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void Framebuffer::Bind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);

        // VIEWPORT'U DA AYARLAMAK SART.
        // Framebuffer 800x600 ama viewport hala 1280x720 kalirsa,
        // cizim texture'in disina tasar ve goruntunun bir kismi kaybolur.
        // Bu, framebuffer kullanirken en sik yapilan hatadir.
        glViewport(0, 0, static_cast<GLsizei>(m_Spec.Width), static_cast<GLsizei>(m_Spec.Height));
    }

    void Framebuffer::Unbind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void Framebuffer::Resize(std::uint32_t width, std::uint32_t height)
    {
        // Gecersiz boyut korumasi. Panel kapatildiginda veya pencere
        // simge durumuna kucultuldugunde 0 gelebilir; 0 boyutlu texture
        // olusturmak GL hatasi uretir ve framebuffer bozulur.
        if (width == 0 || height == 0 ||
            width > MAX_FRAMEBUFFER_SIZE || height > MAX_FRAMEBUFFER_SIZE)
        {
            FX_CORE_WARN("Framebuffer yeniden boyutlandirma yok sayildi: %ux%u", width, height);
            return;
        }

        // Ayni boyutsa hicbir sey yapma. Bu kontrol onemli: ImGui her
        // karede panel boyutunu bildirir; kontrol olmadan her karede
        // texture yeniden olusturulur ve performans coker.
        if (width == m_Spec.Width && height == m_Spec.Height)
            return;

        m_Spec.Width  = width;
        m_Spec.Height = height;
        Invalidate();
    }
}
