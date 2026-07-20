#pragma once

#include <cstdint>
#include <initializer_list>
#include <vector>

namespace FX
{
    enum class FramebufferTextureFormat
    {
        None = 0,
        RGBA8,
        RED_INTEGER,        // entity ID gibi tamsayi verisi icin
        DEPTH24STENCIL8
    };

    struct FramebufferTextureSpec
    {
        FramebufferTextureFormat Format = FramebufferTextureFormat::None;

        FramebufferTextureSpec() = default;
        FramebufferTextureSpec(FramebufferTextureFormat format) : Format(format) {}
    };

    struct FramebufferSpec
    {
        std::uint32_t Width  = 1280;
        std::uint32_t Height = 720;
        std::vector<FramebufferTextureSpec> Attachments;
    };

    class Framebuffer
    {
    public:
        explicit Framebuffer(const FramebufferSpec& spec);
        ~Framebuffer();

        Framebuffer(const Framebuffer&)            = delete;
        Framebuffer& operator=(const Framebuffer&) = delete;

        void Bind();
        void Unbind();
        void Resize(std::uint32_t width, std::uint32_t height);

        // Tek bir pikseli CPU'ya okur. Fare altindaki entity'yi bulmak icin.
        // glReadPixels GPU'nun bitmesini bekler; kare basina birkac cagriyi
        // gecmemeli.
        int ReadPixel(std::uint32_t attachmentIndex, int x, int y);

        // Tamsayi ekini belirli bir degere doldurur. Entity ID tamponunu
        // -1'e cekmek icin: glClear onu 0 yapardi ve 0 gecerli bir
        // entt::entity degeri.
        void ClearAttachment(std::uint32_t attachmentIndex, int value);

        std::uint32_t GetColorAttachmentID(std::uint32_t index = 0) const;

        const FramebufferSpec& GetSpec() const { return m_Spec; }
        bool IsValid() const { return m_RendererID != 0; }

    private:
        void Invalidate();

        std::uint32_t m_RendererID = 0;

        std::vector<FramebufferTextureSpec> m_ColorSpecs;
        FramebufferTextureSpec              m_DepthSpec;

        std::vector<std::uint32_t> m_ColorAttachments;
        std::uint32_t              m_DepthAttachment = 0;

        FramebufferSpec m_Spec;
    };
}
