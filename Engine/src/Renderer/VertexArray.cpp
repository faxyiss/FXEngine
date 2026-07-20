#include "FXEngine/Renderer/VertexArray.h"
#include "FXEngine/Core/Log.h"

#include <glad/glad.h>

namespace FX
{
    namespace
    {
        // Bizim ShaderDataType'imizi OpenGL'in kendi enum'una cevirir.
        // Neden kendi enum'umuz var? Cunku motor kodu GL_FLOAT gibi
        // API'ye ozel sabitlerle dolmasin. Ileride baska bir grafik API
        // eklenirse sadece bu fonksiyon degisir.
        GLenum ShaderDataTypeToGLBaseType(ShaderDataType type)
        {
            switch (type)
            {
                case ShaderDataType::Float:
                case ShaderDataType::Float2:
                case ShaderDataType::Float3:
                case ShaderDataType::Float4:
                case ShaderDataType::Mat3:
                case ShaderDataType::Mat4:   return GL_FLOAT;

                case ShaderDataType::Int:
                case ShaderDataType::Int2:
                case ShaderDataType::Int3:
                case ShaderDataType::Int4:   return GL_INT;

                case ShaderDataType::Bool:   return GL_BOOL;
                default: break;
            }
            FX_CORE_ERROR("ShaderDataTypeToGLBaseType: bilinmeyen tur!");
            return 0;
        }
    }

    VertexArray::VertexArray()
    {
        glGenVertexArrays(1, &m_RendererID);
    }

    VertexArray::~VertexArray()
    {
        glDeleteVertexArrays(1, &m_RendererID);
    }

    void VertexArray::Bind() const
    {
        glBindVertexArray(m_RendererID);
    }

    void VertexArray::Unbind() const
    {
        glBindVertexArray(0);
    }

    void VertexArray::AddVertexBuffer(const std::shared_ptr<VertexBuffer>& vertexBuffer)
    {
        if (vertexBuffer->GetLayout().GetElements().empty())
        {
            // Layout'suz bir VBO eklemek anlamsizdir: GPU verinin ne oldugunu
            // bilemez. Bu bir PROGRAMCI hatasidir, kullanici hatasi degil -
            // bu yuzden assert.
            FX_ASSERT(false, "VertexBuffer'in layout'u yok! SetLayout cagirdin mi?");
            return;
        }

        // SIRA KRITIK: once VAO'yu bagla, sonra VBO'yu.
        // glVertexAttribPointer, o an bagli olan GL_ARRAY_BUFFER'i AKTIF
        // VAO'ya kaydeder. VAO bagli degilse ayar hicbir yere yazilmaz.
        glBindVertexArray(m_RendererID);
        vertexBuffer->Bind();

        const auto& layout = vertexBuffer->GetLayout();
        for (const auto& element : layout)
        {
            // Attribute'u etkinlestir. Varsayilan olarak KAPALIDIR;
            // acmayi unutmak "ekranda hicbir sey yok" hatasinin klasik sebebidir.
            glEnableVertexAttribArray(m_VertexBufferIndex);

            // Tarifin kendisi:
            //   index      : shader'daki "layout(location = N)" ile eslesir
            //   size       : kac bilesen (vec3 -> 3)
            //   type       : GL_FLOAT
            //   normalized : tamsayilari 0..1'e olceklensin mi
            //   stride     : bir koseden digerine kac bayt
            //   pointer    : kose baslangicindan itibaren bu alanin ofseti
            //
            // Son parametre tarihsel bir tuhaflik: tipi const void* ama
            // aslinda bir OFSET sayisi. Eski OpenGL'de gercekten isaretciydi,
            // simdi tampon icindeki bayt kaydirmasi anlamina geliyor.
            glVertexAttribPointer(
                m_VertexBufferIndex,
                static_cast<GLint>(element.GetComponentCount()),
                ShaderDataTypeToGLBaseType(element.Type),
                element.Normalized ? GL_TRUE : GL_FALSE,
                static_cast<GLsizei>(layout.GetStride()),
                reinterpret_cast<const void*>(element.Offset));

            ++m_VertexBufferIndex;
        }

        m_VertexBuffers.push_back(vertexBuffer);
    }

    void VertexArray::SetIndexBuffer(const std::shared_ptr<IndexBuffer>& indexBuffer)
    {
        // GL_ELEMENT_ARRAY_BUFFER baglantisi VAO DURUMUNUN PARCASIDIR.
        // Bu yuzden once VAO'yu baglayip sonra index buffer'i baglamak,
        // "bu VAO bu index buffer'i kullanir" demektir.
        //
        // Tersini yaparsan (VAO bagli degilken EBO bind) baglanti hicbir
        // VAO'ya kaydedilmez ve cizimde "index yok" hatasi alirsin.
        glBindVertexArray(m_RendererID);
        indexBuffer->Bind();

        m_IndexBuffer = indexBuffer;
    }
}
