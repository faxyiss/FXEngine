#include "FXEngine/Renderer/Framebuffer.h"
#include "FXEngine/Core/Log.h"

#include <glad/glad.h>

namespace FX
{
    namespace
    {
        constexpr std::uint32_t MAX_FRAMEBUFFER_SIZE = 8192;

        bool IsDepthFormat(FramebufferTextureFormat format)
        {
            return format == FramebufferTextureFormat::DEPTH24STENCIL8;
        }

        void AttachColorTexture(std::uint32_t id, GLenum internalFormat, GLenum format,
                                GLenum type, std::uint32_t width, std::uint32_t height,
                                std::uint32_t index)
        {
            glBindTexture(GL_TEXTURE_2D, id);
            glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internalFormat),
                         static_cast<GLsizei>(width), static_cast<GLsizei>(height),
                         0, format, type, nullptr);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index,
                                   GL_TEXTURE_2D, id, 0);
        }
    }

    Framebuffer::Framebuffer(const FramebufferSpec& spec)
        : m_Spec(spec)
    {
        for (const auto& attachment : spec.Attachments)
        {
            if (IsDepthFormat(attachment.Format))
                m_DepthSpec = attachment;
            else
                m_ColorSpecs.push_back(attachment);
        }

        Invalidate();
    }

    Framebuffer::~Framebuffer()
    {
        glDeleteFramebuffers(1, &m_RendererID);
        glDeleteTextures(static_cast<GLsizei>(m_ColorAttachments.size()),
                         m_ColorAttachments.data());
        glDeleteTextures(1, &m_DepthAttachment);
    }

    void Framebuffer::Invalidate()
    {
        if (m_RendererID)
        {
            glDeleteFramebuffers(1, &m_RendererID);
            glDeleteTextures(static_cast<GLsizei>(m_ColorAttachments.size()),
                             m_ColorAttachments.data());
            glDeleteTextures(1, &m_DepthAttachment);

            m_ColorAttachments.clear();
            m_DepthAttachment = 0;
        }

        glGenFramebuffers(1, &m_RendererID);
        glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);

        if (!m_ColorSpecs.empty())
        {
            m_ColorAttachments.resize(m_ColorSpecs.size());
            glGenTextures(static_cast<GLsizei>(m_ColorAttachments.size()),
                          m_ColorAttachments.data());

            for (std::size_t i = 0; i < m_ColorSpecs.size(); ++i)
            {
                switch (m_ColorSpecs[i].Format)
                {
                    case FramebufferTextureFormat::RGBA8:
                        AttachColorTexture(m_ColorAttachments[i], GL_RGBA8, GL_RGBA,
                                           GL_UNSIGNED_BYTE, m_Spec.Width, m_Spec.Height,
                                           static_cast<std::uint32_t>(i));
                        break;

                    case FramebufferTextureFormat::RED_INTEGER:
                        AttachColorTexture(m_ColorAttachments[i], GL_R32I, GL_RED_INTEGER,
                                           GL_INT, m_Spec.Width, m_Spec.Height,
                                           static_cast<std::uint32_t>(i));
                        break;

                    default:
                        break;
                }
            }
        }

        if (m_DepthSpec.Format != FramebufferTextureFormat::None)
        {
            glGenRenderbuffers(1, &m_DepthAttachment);
            glBindRenderbuffer(GL_RENDERBUFFER, m_DepthAttachment);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
                                  static_cast<GLsizei>(m_Spec.Width),
                                  static_cast<GLsizei>(m_Spec.Height));
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                      GL_RENDERBUFFER, m_DepthAttachment);
        }

        // Birden fazla renk eki varsa OpenGL'e hangilerine yazacagimizi
        // acikca sylemek zorundayiz; varsayilan sadece ilkidir.
        if (m_ColorAttachments.size() > 1)
        {
            GLenum buffers[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
                                  GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
            glDrawBuffers(static_cast<GLsizei>(m_ColorAttachments.size()), buffers);
        }
        else if (m_ColorAttachments.empty())
        {
            glDrawBuffer(GL_NONE);
        }

        const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
            FX_CORE_ERROR("Framebuffer eksik! Durum kodu: 0x%X (%ux%u)",
                          status, m_Spec.Width, m_Spec.Height);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void Framebuffer::Bind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);
        glViewport(0, 0, static_cast<GLsizei>(m_Spec.Width), static_cast<GLsizei>(m_Spec.Height));

        // Tamsayi ekleri blending desteklemez. Bunu Bind'de HER KARE
        // yapmak zorundayiz: ImGui'nin OpenGL backend'i her karede
        // glEnable(GL_BLEND) cagirip durumu geri yukluyor ve tek seferlik
        // ayarimizi eziyor.
        for (std::size_t i = 0; i < m_ColorSpecs.size(); ++i)
        {
            if (m_ColorSpecs[i].Format == FramebufferTextureFormat::RED_INTEGER)
                glDisablei(GL_BLEND, static_cast<GLuint>(i));
        }
    }

    void Framebuffer::Unbind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void Framebuffer::Resize(std::uint32_t width, std::uint32_t height)
    {
        if (width == 0 || height == 0 ||
            width > MAX_FRAMEBUFFER_SIZE || height > MAX_FRAMEBUFFER_SIZE)
        {
            FX_CORE_WARN("Framebuffer yeniden boyutlandirma yok sayildi: %ux%u", width, height);
            return;
        }

        if (width == m_Spec.Width && height == m_Spec.Height)
            return;

        m_Spec.Width  = width;
        m_Spec.Height = height;
        Invalidate();
    }

    int Framebuffer::ReadPixel(std::uint32_t attachmentIndex, int x, int y)
    {
        if (attachmentIndex >= m_ColorAttachments.size())
            return -1;

        if (x < 0 || y < 0 ||
            x >= static_cast<int>(m_Spec.Width) || y >= static_cast<int>(m_Spec.Height))
            return -1;

        glReadBuffer(GL_COLOR_ATTACHMENT0 + attachmentIndex);

        int pixel = -1;
        glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_INT, &pixel);
        return pixel;
    }

    void Framebuffer::ClearAttachment(std::uint32_t attachmentIndex, int value)
    {
        if (attachmentIndex >= m_ColorAttachments.size())
            return;

        glClearBufferiv(GL_COLOR, static_cast<GLint>(attachmentIndex), &value);
    }

    std::uint32_t Framebuffer::GetColorAttachmentID(std::uint32_t index) const
    {
        if (index >= m_ColorAttachments.size())
            return 0;
        return m_ColorAttachments[index];
    }
}
