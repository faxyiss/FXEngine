#pragma once

// ===========================================================================
// FXEngine - Vertex / Index Buffer + Layout
//
// VBO (Vertex Buffer Object)  : GPU bellegindeki HAM kose verisi. Anlami yok,
//                               sadece bir sayi yigini.
// EBO (Element/Index Buffer)  : "Su koseleri su sirayla kullan" listesi.
// BufferLayout                : O sayi yiginin ANLAMI. "Ilk 3 float pozisyon,
//                               sonraki 2 float doku koordinati" gibi.
//
// NEDEN BufferLayout diye ayri bir soyutlama?
// Cunku alternatifi, her yeni kose formatinda glVertexAttribPointer
// cagrilarini elle yazmaktir - ofset ve stride'i elle hesaplayarak.
// Bu hesaplar sessizce yanlis yapilir (bir float kaydirirsin, model
// bozulur, sebebini bulamazsin). Layout, o hesabi tek yerde ve otomatik yapar.
// ===========================================================================

#include <cstdint>
#include <string>
#include <vector>

namespace FX
{
    // Shader'a gidebilecek veri turleri. Faz 2'de sadece Float3 kullanacagiz
    // ama turleri simdiden tanimlamak, her yeni turde enum'u genisletip
    // 3 ayri switch'i guncellemekten iyidir.
    enum class ShaderDataType
    {
        None = 0,
        Float, Float2, Float3, Float4,
        Int,   Int2,   Int3,   Int4,
        Mat3,  Mat4,
        Bool
    };

    // Bir turun kac bayt tuttugu.
    std::uint32_t ShaderDataTypeSize(ShaderDataType type);

    // Kose yapisindaki TEK BIR alan.
    struct BufferElement
    {
        std::string    Name;                 // sadece hata ayiklama icin
        ShaderDataType Type       = ShaderDataType::None;
        std::uint32_t  Size       = 0;       // bayt cinsinden
        std::size_t    Offset     = 0;       // kose baslangicindan kacinci bayt
        bool           Normalized = false;

        BufferElement() = default;
        BufferElement(ShaderDataType type, const std::string& name, bool normalized = false);

        // Bu tur kac BILESEN icerir? Float3 -> 3, Mat4 -> 4 (4 adet vec4).
        // glVertexAttribPointer'in "size" parametresi bunu ister.
        std::uint32_t GetComponentCount() const;
    };

    class BufferLayout
    {
    public:
        BufferLayout() = default;

        // initializer_list ile su sekilde yazilabilsin diye:
        //   BufferLayout{ { ShaderDataType::Float3, "a_Position" } }
        BufferLayout(std::initializer_list<BufferElement> elements);

        // Stride: bir koseden digerine kac bayt atlanacagi.
        // Ornek: pozisyon(12 bayt) + uv(8 bayt) -> stride 20.
        std::uint32_t GetStride() const { return m_Stride; }
        const std::vector<BufferElement>& GetElements() const { return m_Elements; }

        // Aralik tabanli for dongusunde kullanilabilsin diye.
        std::vector<BufferElement>::const_iterator begin() const { return m_Elements.begin(); }
        std::vector<BufferElement>::const_iterator end()   const { return m_Elements.end(); }

    private:
        // Her elemanin ofsetini ve toplam stride'i hesaplar.
        // Elle yapilinca en cok hata edilen kisim tam olarak budur.
        void CalculateOffsetsAndStride();

        std::vector<BufferElement> m_Elements;
        std::uint32_t              m_Stride = 0;
    };

    // -----------------------------------------------------------------------
    // VertexBuffer
    // -----------------------------------------------------------------------
    class VertexBuffer
    {
    public:
        // Statik veri: bir kez yuklenir, degismez (GL_STATIC_DRAW).
        VertexBuffer(const void* data, std::uint32_t size);
        ~VertexBuffer();

        VertexBuffer(const VertexBuffer&)            = delete;
        VertexBuffer& operator=(const VertexBuffer&) = delete;

        void Bind() const;
        void Unbind() const;

        const BufferLayout& GetLayout() const { return m_Layout; }
        void SetLayout(const BufferLayout& layout) { m_Layout = layout; }

    private:
        std::uint32_t m_RendererID = 0;
        BufferLayout  m_Layout;
    };

    // -----------------------------------------------------------------------
    // IndexBuffer
    // -----------------------------------------------------------------------
    class IndexBuffer
    {
    public:
        // count = INDEKS SAYISI (bayt degil). Bir kare icin 6.
        IndexBuffer(const std::uint32_t* indices, std::uint32_t count);
        ~IndexBuffer();

        IndexBuffer(const IndexBuffer&)            = delete;
        IndexBuffer& operator=(const IndexBuffer&) = delete;

        void Bind() const;
        void Unbind() const;

        std::uint32_t GetCount() const { return m_Count; }

    private:
        std::uint32_t m_RendererID = 0;
        std::uint32_t m_Count      = 0;
    };
}
