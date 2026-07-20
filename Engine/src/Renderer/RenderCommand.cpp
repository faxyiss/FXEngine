#include "FXEngine/Renderer/RenderCommand.h"
#include "FXEngine/Core/Log.h"

#include <glad/glad.h>

namespace FX
{
    void RenderCommand::Init()
    {
        // --- Saydamlik (blending) --------------------------------------------
        // 2D motorda sart: sprite'larin kenarlari saydam olacak.
        // Formul: sonuc = kaynak * kaynakAlfa + hedef * (1 - kaynakAlfa)
        // Yani alfa 1 ise tamamen ustune yazar, 0 ise hic gorunmez.
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Dithering tamsayi eklerinde desteklenmiyor ve modern renk
        // derinliginde bir isimize yaramiyor.
        // (Blending'in tamsayi ekleri icin kapatilmasi Framebuffer::Bind'de:
        // ImGui her karede blend durumunu geri yukledigi icin tek seferlik
        // ayar orada tutmuyor.)
        glDisable(GL_DITHER);

        // --- Derinlik testi ---------------------------------------------------
        // 2D'de derinligi Z ile siralamak isteyebiliriz (katmanlar).
        // Simdilik aciyoruz; Faz 4'te sprite siralamasi konusunu ele alacagiz.
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        FX_CORE_INFO("RenderCommand hazir (blend acik, derinlik testi acik).");
    }

    void RenderCommand::SetViewport(std::uint32_t x, std::uint32_t y,
                                    std::uint32_t width, std::uint32_t height)
    {
        glViewport(static_cast<GLint>(x), static_cast<GLint>(y),
                   static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    }

    void RenderCommand::SetClearColor(const glm::vec4& color)
    {
        glClearColor(color.r, color.g, color.b, color.a);
    }

    void RenderCommand::Clear()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void RenderCommand::DrawIndexed(const std::shared_ptr<VertexArray>& vertexArray,
                                    std::uint32_t indexCount)
    {
        vertexArray->Bind();

        const std::uint32_t count =
            (indexCount == 0) ? vertexArray->GetIndexBuffer()->GetCount() : indexCount;

        // glDrawElements: "bagli VAO'nun index buffer'indaki ilk `count`
        // indeksi kullanarak ucgen ciz".
        //   GL_TRIANGLES  : her 3 indeks bir ucgen
        //   GL_UNSIGNED_INT : indekslerimiz uint32
        //   nullptr       : index buffer'in basindan basla
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(count), GL_UNSIGNED_INT, nullptr);
    }

    void RenderCommand::DrawLines(const std::shared_ptr<VertexArray>& vertexArray,
                                  std::uint32_t vertexCount)
    {
        vertexArray->Bind();
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertexCount));
    }

    void RenderCommand::SetLineWidth(float width)
    {
        glLineWidth(width);
    }

    void RenderCommand::SetDepthTest(bool enabled)
    {
        if (enabled) glEnable(GL_DEPTH_TEST);
        else         glDisable(GL_DEPTH_TEST);
    }

    void RenderCommand::SetWireframe(bool enabled)
    {
        glPolygonMode(GL_FRONT_AND_BACK, enabled ? GL_LINE : GL_FILL);
    }
}
