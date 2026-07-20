#include "FXEngine/Renderer/Buffer.h"
#include "FXEngine/Core/Log.h"

#include <glad/glad.h>

namespace FX
{
    std::uint32_t ShaderDataTypeSize(ShaderDataType type)
    {
        switch (type)
        {
            case ShaderDataType::Float:  return 4;
            case ShaderDataType::Float2: return 4 * 2;
            case ShaderDataType::Float3: return 4 * 3;
            case ShaderDataType::Float4: return 4 * 4;
            case ShaderDataType::Int:    return 4;
            case ShaderDataType::Int2:   return 4 * 2;
            case ShaderDataType::Int3:   return 4 * 3;
            case ShaderDataType::Int4:   return 4 * 4;
            case ShaderDataType::Mat3:   return 4 * 3 * 3;
            case ShaderDataType::Mat4:   return 4 * 4 * 4;
            case ShaderDataType::Bool:   return 1;
            default: break;
        }
        FX_CORE_ERROR("ShaderDataTypeSize: bilinmeyen tur!");
        return 0;
    }

    // -----------------------------------------------------------------------
    // BufferElement
    // -----------------------------------------------------------------------
    BufferElement::BufferElement(ShaderDataType type, const std::string& name, bool normalized)
        : Name(name), Type(type), Size(ShaderDataTypeSize(type)), Offset(0), Normalized(normalized)
    {
        // Offset burada 0 birakiliyor; BufferLayout onu toplu hesaplayacak.
        // Sebep: bir elemanin ofseti KENDISINE degil, kendinden oncekilere baglidir.
    }

    std::uint32_t BufferElement::GetComponentCount() const
    {
        switch (Type)
        {
            case ShaderDataType::Float:  return 1;
            case ShaderDataType::Float2: return 2;
            case ShaderDataType::Float3: return 3;
            case ShaderDataType::Float4: return 4;
            case ShaderDataType::Int:    return 1;
            case ShaderDataType::Int2:   return 2;
            case ShaderDataType::Int3:   return 3;
            case ShaderDataType::Int4:   return 4;
            case ShaderDataType::Mat3:   return 3;   // 3 adet vec3 olarak gonderilir
            case ShaderDataType::Mat4:   return 4;   // 4 adet vec4 olarak gonderilir
            case ShaderDataType::Bool:   return 1;
            default: break;
        }
        FX_CORE_ERROR("GetComponentCount: bilinmeyen tur!");
        return 0;
    }

    // -----------------------------------------------------------------------
    // BufferLayout
    // -----------------------------------------------------------------------
    BufferLayout::BufferLayout(std::initializer_list<BufferElement> elements)
        : m_Elements(elements)
    {
        CalculateOffsetsAndStride();
    }

    void BufferLayout::CalculateOffsetsAndStride()
    {
        // Bu kucuk fonksiyon, elle yazildiginda en cok hata edilen yerdir.
        // Bir elemanin ofseti = kendinden oncekilerin boyutlari toplami.
        // Stride ise hepsinin toplami.
        //
        // Ornek: { Float3 "pos", Float2 "uv" }
        //   pos -> offset 0,  size 12
        //   uv  -> offset 12, size 8
        //   stride = 20
        std::size_t offset = 0;
        m_Stride = 0;

        for (auto& element : m_Elements)
        {
            element.Offset = offset;
            offset   += element.Size;
            m_Stride += element.Size;
        }
    }

    // -----------------------------------------------------------------------
    // VertexBuffer
    // -----------------------------------------------------------------------
    VertexBuffer::VertexBuffer(const void* data, std::uint32_t size)
    {
        // 1) GPU'da bir tampon nesnesi yarat (henuz bellegi yok, sadece kimlik).
        glGenBuffers(1, &m_RendererID);

        // 2) Bind: "bundan sonraki GL_ARRAY_BUFFER islemleri bunu hedef alsin".
        //    OpenGL bir DURUM MAKINESIDIR: nesneyi fonksiyona parametre olarak
        //    vermezsin, once "aktif" yaparsin sonra uzerinde islem yaparsin.
        //    Modern API'lerde (Vulkan) bu tasarim terk edildi ama GL 3.3'te boyle.
        glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);

        // 3) Veriyi CPU'dan GPU'ya kopyala.
        //    GL_STATIC_DRAW = "bir kez yazacagim, cok kez okuyacagim".
        //    Bu bir ZORUNLULUK degil, surucuye VERILEN IPUCUDUR - surucu buna
        //    gore bellegi nereye koyacagina karar verir. Faz 4'te her karede
        //    degisen veri icin GL_DYNAMIC_DRAW kullanacagiz.
        glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
    }

    VertexBuffer::VertexBuffer(std::uint32_t size)
    {
        glGenBuffers(1, &m_RendererID);
        glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);

        // data = nullptr -> "bu kadar yer ayir ama icini doldurma".
        //
        // GL_DYNAMIC_DRAW ipucu: "cok kez yazacagim, cok kez okunacak".
        // Surucu bunu gorunce tamponu CPU'nun hizlica yazabilecegi bir
        // bellege koyar. GL_STATIC_DRAW deseydik surucu onu GPU'nun uzak
        // bellegine koyar, her karede yazmak yavas olurdu.
        glBufferData(GL_ARRAY_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
    }

    VertexBuffer::~VertexBuffer()
    {
        glDeleteBuffers(1, &m_RendererID);
    }

    void VertexBuffer::SetData(const void* data, std::uint32_t size)
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);

        // glBufferSubData: mevcut tamponun BIR KISMINI gunceller.
        // glBufferData kullansaydik her karede tamponu yeniden TAHSIS ederdik -
        // surucu eski bellegi bosaltip yenisini ayirir, bu pahalidir.
        // SubData ise ayni bellege yazar.
        glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
    }

    void VertexBuffer::Bind() const
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
    }

    void VertexBuffer::Unbind() const
    {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    // -----------------------------------------------------------------------
    // IndexBuffer
    // -----------------------------------------------------------------------
    IndexBuffer::IndexBuffer(const std::uint32_t* indices, std::uint32_t count)
        : m_Count(count)
    {
        glGenBuffers(1, &m_RendererID);

        // DIKKAT: GL_ELEMENT_ARRAY_BUFFER baglantisi VAO'nun ICINDE saklanir.
        // Yani bir VAO bagliyken index buffer baglarsan, o VAO artik o index
        // buffer'i "hatirlar". Bu, VAO'nun az bilinen ama cok onemli bir
        // ozelligidir - VertexArray sinifinda buna guveniyoruz.
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(count) * sizeof(std::uint32_t),
                     indices, GL_STATIC_DRAW);
    }

    IndexBuffer::~IndexBuffer()
    {
        glDeleteBuffers(1, &m_RendererID);
    }

    void IndexBuffer::Bind() const
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
    }

    void IndexBuffer::Unbind() const
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
}
