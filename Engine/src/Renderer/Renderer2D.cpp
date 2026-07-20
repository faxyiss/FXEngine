#include "FXEngine/Renderer/Renderer2D.h"

#include "FXEngine/Core/Log.h"
#include "FXEngine/Renderer/Shader.h"
#include "FXEngine/Renderer/VertexArray.h"
#include "FXEngine/Renderer/RenderCommand.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <vector>

namespace FX
{
    namespace
    {
        // -------------------------------------------------------------------
        // Bir kosenin tasidigi veri
        // -------------------------------------------------------------------
        // Faz 3'te kose sadece pozisyon + UV idi; renk ve transform uniform'du.
        // Batch'te uniform kullanamayiz (tek draw call, binlerce farkli quad),
        // bu yuzden HER SEY koseye tasindi.
        //
        // Bedeli: kose basina 44 bayt (Faz 3'te 20 idi). Kazanci: 1 draw call.
        // Bu takas neredeyse her zaman lehimize - bant genisligi bol,
        // draw call pahali.
        struct QuadVertex
        {
            glm::vec3 Position;       // DUNYA koordinatinda (CPU'da hesaplanmis)
            glm::vec4 Color;
            glm::vec2 TexCoord;
            float     TexIndex;       // hangi slot? float cunku attribute'lar float
            float     TilingFactor;
            int       EntityID;       // -1 = entity'ye ait degil
        };

        // -------------------------------------------------------------------
        // Batch'in sinirlari
        // -------------------------------------------------------------------
        // Bu sayilar keyfi degil, bir takasin sonucu:
        //   Cok kucuk -> sik sik Flush, batch'in anlami kalmaz
        //   Cok buyuk -> bosuna RAM ve GPU bellegi
        // 10.000 quad ~1.7 MB kose verisi tutar; makul bir baslangic.
        constexpr std::uint32_t MAX_QUADS    = 10000;
        constexpr std::uint32_t MAX_VERTICES = MAX_QUADS * 4;
        constexpr std::uint32_t MAX_INDICES  = MAX_QUADS * 6;

        // Shader'daki dizi boyutu ile ESLESMELI (Renderer2D.frag).
        constexpr std::uint32_t MAX_TEXTURE_SLOTS = 32;

        struct Renderer2DData
        {
            std::shared_ptr<VertexArray>  QuadVA;
            std::shared_ptr<VertexBuffer> QuadVB;
            std::unique_ptr<Shader>       TextureShader;
            std::shared_ptr<Texture2D>    WhiteTexture;

            std::uint32_t QuadIndexCount = 0;

            // CPU tarafindaki kose deposu.
            // Neden vector<QuadVertex> yerine ham dizi + isaretci?
            // Cunku push_back her cagrida boyut kontrolu yapar. Kare basina
            // 40.000 kez cagrilan bir yolda bu olculebilir. Isaretciyi
            // ilerletmek en ucuz yol. (Bu, "erken optimizasyon yapma"
            // kuralinin makul bir istisnasi: bu yol tanimi geregi sicak.)
            std::vector<QuadVertex> QuadVertexBufferBase;
            QuadVertex*             QuadVertexBufferPtr = nullptr;

            // Bu batch'te kullanilan texture'lar. 0. slot her zaman
            // beyaz texture (duz renkli quad'lar icin).
            std::array<std::shared_ptr<Texture2D>, MAX_TEXTURE_SLOTS> TextureSlots;
            std::uint32_t TextureSlotIndex = 1;   // 0 beyaz texture'a ayrildi

            std::uint32_t MaxTextureSlots = MAX_TEXTURE_SLOTS;

            // Quad'in birim kosesi (merkez 0,0 - kenar 1x1).
            // Her cizimde transform ile carpilarak dunya koordinatina cevrilir.
            // vec4 cunku matris carpimi icin w bileseni gerekiyor.
            glm::vec4 QuadVertexPositions[4];

            Renderer2D::Statistics Stats;
            bool SceneStarted = false;
        };

        // Tek bir global durum. Renderer2D static bir API oldugu icin
        // (Renderer2D::DrawQuad diye cagriliyor) durumu da burada tutuyoruz.
        //
        // Bu bilincli bir tercih: 2D renderer'in ayni anda birden fazla
        // ornegine ihtiyac duyulmaz ve static API cagri yerlerini cok
        // sadelestirir. Ilerideki fazlarda birden fazla renderer gerekirse
        // burasi bir sinifa donusur.
        Renderer2DData s_Data;
    }

    void Renderer2D::Init()
    {
        // --- Bu makinede kac slot var? ---------------------------------------
        // GL 3.3 en az 16 GARANTI eder, cogu modern GPU 32 verir.
        // Shader 32'ye gore yazildi ama makine daha az veriyorsa ona uyariz.
        GLint maxSlots = 0;
        glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxSlots);
        s_Data.MaxTextureSlots = std::min(static_cast<std::uint32_t>(maxSlots), MAX_TEXTURE_SLOTS);

        // --- Kose deposu -------------------------------------------------------
        s_Data.QuadVertexBufferBase.resize(MAX_VERTICES);

        // --- VAO + dinamik VBO -------------------------------------------------
        s_Data.QuadVA = std::make_shared<VertexArray>();
        s_Data.QuadVA->Bind();

        s_Data.QuadVB = std::make_shared<VertexBuffer>(
            static_cast<std::uint32_t>(MAX_VERTICES * sizeof(QuadVertex)));

        // Layout, QuadVertex yapisinin ALAN SIRASIYLA birebir ayni olmali.
        // Bir alani unutursan ya da sirayi degistirirsen GPU veriyi yanlis
        // yorumlar ve goruntu anlamsiz sekilde bozulur - derleyici bunu
        // yakalayamaz cunku ikisi ayri "diller"de tanimli.
        s_Data.QuadVB->SetLayout({
            { ShaderDataType::Float3, "a_Position"     },
            { ShaderDataType::Float4, "a_Color"        },
            { ShaderDataType::Float2, "a_TexCoord"     },
            { ShaderDataType::Float,  "a_TexIndex"     },
            { ShaderDataType::Float,  "a_TilingFactor" },
            { ShaderDataType::Int,    "a_EntityID"     }
        });
        s_Data.QuadVA->AddVertexBuffer(s_Data.QuadVB);

        // --- Index buffer: BIR KEZ uretilir ------------------------------------
        // Buyuk fikir: quad'larin indeks DESENI her zaman aynidir.
        //   quad 0 -> 0,1,2, 2,3,0
        //   quad 1 -> 4,5,6, 6,7,4
        //   quad 2 -> 8,9,10, 10,11,8
        // Yani sadece 4'er 4'er kayar. Bu yuzden 10.000 quad'lik indeks
        // dizisini basta bir kez uretip GPU'ya yukluyoruz ve BIR DAHA
        // HIC DOKUNMUYORUZ. Her karede sadece kose verisi degisiyor.
        {
            std::vector<std::uint32_t> indices(MAX_INDICES);
            std::uint32_t offset = 0;
            for (std::uint32_t i = 0; i < MAX_INDICES; i += 6)
            {
                indices[i + 0] = offset + 0;
                indices[i + 1] = offset + 1;
                indices[i + 2] = offset + 2;

                indices[i + 3] = offset + 2;
                indices[i + 4] = offset + 3;
                indices[i + 5] = offset + 0;

                offset += 4;
            }

            auto ebo = std::make_shared<IndexBuffer>(indices.data(), MAX_INDICES);
            s_Data.QuadVA->SetIndexBuffer(ebo);
            // indices vector'u burada yok oluyor - veri artik GPU'da.
        }

        s_Data.QuadVA->Unbind();

        // --- Beyaz texture -----------------------------------------------------
        // Duz renkli quad'lar icin. Neden gerekli? Cunku shader tek yol
        // izlesin istiyoruz: her quad bir texture'i renkle CARPAR.
        // Beyaz (1,1,1,1) ile carpmak rengi degistirmez -> duz renk elde ederiz.
        //
        // Alternatif, shader'da "texture var mi?" diye dallanmakti; ama
        // GPU'da dallanma pahalidir ve bu yontem hem daha basit hem daha hizli.
        {
            std::uint32_t whitePixel = 0xffffffff;   // RGBA, hepsi 255
            TextureSpec spec;
            spec.GenerateMipmaps = false;            // 1x1 icin anlamsiz
            spec.MinFilter = TextureFilter::Nearest;
            spec.MagFilter = TextureFilter::Nearest;
            s_Data.WhiteTexture = std::make_shared<Texture2D>(1, 1, &whitePixel, 4, spec);
        }

        // --- Shader ------------------------------------------------------------
        s_Data.TextureShader.reset(Shader::FromFiles("Renderer2D",
                                                     "assets/shaders/Renderer2D.vert",
                                                     "assets/shaders/Renderer2D.frag"));
        if (!s_Data.TextureShader || !s_Data.TextureShader->IsValid())
        {
            FX_CORE_ERROR("Renderer2D shader'i yuklenemedi!");
            return;
        }

        // sampler2D u_Textures[32] uniform'una [0,1,2,...,31] yaziyoruz.
        // Yani "0. eleman 0. slotu okusun, 1. eleman 1. slotu..." demek.
        {
            std::array<int, MAX_TEXTURE_SLOTS> samplers{};
            for (std::uint32_t i = 0; i < MAX_TEXTURE_SLOTS; ++i)
                samplers[i] = static_cast<int>(i);

            s_Data.TextureShader->Bind();
            s_Data.TextureShader->SetIntArray("u_Textures", samplers.data(), MAX_TEXTURE_SLOTS);
        }

        // --- Birim quad kose konumlari -----------------------------------------
        // Merkezi (0,0), boyutu 1x1. Transform ile carpilinca istenen
        // konum/boyut/aciya gelir.
        s_Data.QuadVertexPositions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
        s_Data.QuadVertexPositions[1] = { -0.5f,  0.5f, 0.0f, 1.0f };
        s_Data.QuadVertexPositions[2] = {  0.5f,  0.5f, 0.0f, 1.0f };
        s_Data.QuadVertexPositions[3] = {  0.5f, -0.5f, 0.0f, 1.0f };

        s_Data.TextureSlots[0] = s_Data.WhiteTexture;

        FX_CORE_INFO("Renderer2D hazir (max %u quad/batch, %u texture slotu).",
                     MAX_QUADS, s_Data.MaxTextureSlots);
    }

    void Renderer2D::Shutdown()
    {
        // shared_ptr/unique_ptr'lar kendiliginden temizlenir; burada sadece
        // GL nesnelerinin context olmeden once silinmesini garanti ediyoruz.
        s_Data.QuadVA.reset();
        s_Data.QuadVB.reset();
        s_Data.TextureShader.reset();
        s_Data.WhiteTexture.reset();
        for (auto& t : s_Data.TextureSlots)
            t.reset();
        s_Data.QuadVertexBufferBase.clear();
        s_Data.QuadVertexBufferBase.shrink_to_fit();
    }

    std::uint32_t Renderer2D::GetMaxTextureSlots()
    {
        return s_Data.MaxTextureSlots;
    }

    void Renderer2D::BeginScene(const OrthographicCamera& camera)
    {
        FX_ASSERT(!s_Data.SceneStarted, "BeginScene, EndScene cagrilmadan tekrar cagrildi!");
        s_Data.SceneStarted = true;

        // Kamera matrisi TUM batch icin ayni -> tek uniform, sahne basinda set.
        s_Data.TextureShader->Bind();
        s_Data.TextureShader->SetMat4("u_ViewProjection", camera.GetViewProjectionMatrix());

        StartNewBatch();
    }

    void Renderer2D::StartNewBatch()
    {
        s_Data.QuadIndexCount = 0;

        // Yazma isaretcisini dizinin basina al. Veriyi SILMIYORUZ -
        // uzerine yazacagiz. Temizlemek gereksiz is olurdu.
        s_Data.QuadVertexBufferPtr = s_Data.QuadVertexBufferBase.data();

        // 0. slot beyaz texture'a ayrilmis durumda, digerlerini serbest birak.
        s_Data.TextureSlotIndex = 1;
    }

    void Renderer2D::EndScene()
    {
        FX_ASSERT(s_Data.SceneStarted, "EndScene, BeginScene cagrilmadan cagrildi!");
        Flush();
        s_Data.SceneStarted = false;
    }

    void Renderer2D::Flush()
    {
        if (s_Data.QuadIndexCount == 0)
            return;   // cizecek bir sey yok

        // --- 1) CPU'daki veriyi GPU'ya gonder ---------------------------------
        // SADECE DOLU KISMI gonderiyoruz. Tampon 10.000 quad'lik ama
        // 50 quad cizdiysek 50 quad'lik veri gonderiyoruz. Tamamini
        // gondermek her karede 1.7 MB bosuna trafik demek olurdu.
        const auto dataSize = static_cast<std::uint32_t>(
            reinterpret_cast<std::uint8_t*>(s_Data.QuadVertexBufferPtr) -
            reinterpret_cast<std::uint8_t*>(s_Data.QuadVertexBufferBase.data()));

        s_Data.QuadVB->SetData(s_Data.QuadVertexBufferBase.data(), dataSize);

        // --- 2) Texture'lari slotlara bagla ------------------------------------
        for (std::uint32_t i = 0; i < s_Data.TextureSlotIndex; ++i)
            s_Data.TextureSlots[i]->Bind(i);

        // --- 3) TEK draw call --------------------------------------------------
        s_Data.TextureShader->Bind();
        RenderCommand::DrawIndexed(s_Data.QuadVA, s_Data.QuadIndexCount);

        ++s_Data.Stats.DrawCalls;
    }

    // -----------------------------------------------------------------------
    // Ortak cizim yolu
    // -----------------------------------------------------------------------
    namespace
    {
        // Standart UV'ler. Tum quad'lar icin ayni.
        constexpr glm::vec2 DEFAULT_TEX_COORDS[4] = {
            { 0.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f }
        };

        // Bir texture'i batch'e ekler ve slot numarasini dondurur.
        // Zaten varsa mevcut slotu dondurur (ayni texture'i iki slota koymak israf).
        float ResolveTextureSlot(const std::shared_ptr<Texture2D>& texture)
        {
            // Dogrusal arama. 32 eleman icin hash tablosundan HIZLIDIR
            // (onbellek dostu, dallanma az). "Erken optimizasyon yapma"
            // kuralinin dogru uygulamasi: basit olan zaten hizli.
            for (std::uint32_t i = 0; i < s_Data.TextureSlotIndex; ++i)
            {
                if (s_Data.TextureSlots[i].get() == texture.get())
                    return static_cast<float>(i);
            }

            // Slotlar dolduysa mevcut batch'i gonder, temiz sayfa ac.
            // Bu, batch'in ikinci kirilma noktasi (birincisi: dizi doldu).
            if (s_Data.TextureSlotIndex >= s_Data.MaxTextureSlots)
            {
                Renderer2D::EndScene();   // Flush + kapat
                // EndScene sahneyi kapatti; yeniden acmak yerine dogrudan
                // batch'i yeniliyoruz. (BeginScene kamera uniform'unu tekrar
                // set ederdi; gerek yok, degismedi.)
                s_Data.SceneStarted = true;
                s_Data.QuadIndexCount = 0;
                s_Data.QuadVertexBufferPtr = s_Data.QuadVertexBufferBase.data();
                s_Data.TextureSlotIndex = 1;
            }

            const float slot = static_cast<float>(s_Data.TextureSlotIndex);
            s_Data.TextureSlots[s_Data.TextureSlotIndex] = texture;
            ++s_Data.TextureSlotIndex;
            return slot;
        }

        // Dizi doldu mu? Dolduysa gonder ve yeniden basla.
        void EnsureRoom()
        {
            if (s_Data.QuadIndexCount >= MAX_INDICES)
            {
                Renderer2D::EndScene();
                s_Data.SceneStarted = true;
                s_Data.QuadIndexCount = 0;
                s_Data.QuadVertexBufferPtr = s_Data.QuadVertexBufferBase.data();
                s_Data.TextureSlotIndex = 1;
            }
        }

        // Butun DrawQuad varyantlari buraya iner.
        // Tek bir yerde toplamak, bir hata duzeltmesinin 6 fonksiyona
        // tek tek uygulanmasi gerekmemesini saglar.
        void SubmitQuad(const glm::mat4& transform, const glm::vec4& color,
                        float texIndex, float tilingFactor, int entityID)
        {
            EnsureRoom();

            for (int i = 0; i < 4; ++i)
            {
                // ISTE BATCH'IN KALBI:
                // Transform'u UNIFORM olarak gondermek yerine BURADA,
                // CPU'da uyguluyoruz. Sonuc dogrudan dunya koordinatinda
                // bir pozisyon - GPU'nun ek bir sey yapmasina gerek yok.
                //
                // Bedeli: quad basina 4 matris-vektor carpimi (CPU'da).
                // Kazanci: binlerce quad tek draw call'da.
                s_Data.QuadVertexBufferPtr->Position =
                    glm::vec3(transform * s_Data.QuadVertexPositions[i]);

                s_Data.QuadVertexBufferPtr->Color        = color;
                s_Data.QuadVertexBufferPtr->TexCoord     = DEFAULT_TEX_COORDS[i];
                s_Data.QuadVertexBufferPtr->TexIndex     = texIndex;
                s_Data.QuadVertexBufferPtr->TilingFactor = tilingFactor;
                s_Data.QuadVertexBufferPtr->EntityID     = entityID;

                ++s_Data.QuadVertexBufferPtr;
            }

            s_Data.QuadIndexCount += 6;
            ++s_Data.Stats.QuadCount;
        }

        // Donmesiz quad icin hizli yol: sin/cos ve rotasyon matrisi yok.
        glm::mat4 MakeTransform(const glm::vec3& position, const glm::vec2& size)
        {
            return glm::translate(glm::mat4(1.0f), position) *
                   glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
        }

        glm::mat4 MakeRotatedTransform(const glm::vec3& position, const glm::vec2& size,
                                       float rotationRadians)
        {
            return glm::translate(glm::mat4(1.0f), position) *
                   glm::rotate(glm::mat4(1.0f), rotationRadians, { 0.0f, 0.0f, 1.0f }) *
                   glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
        }
    }

    // -----------------------------------------------------------------------
    // Genel API
    // -----------------------------------------------------------------------
    void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size,
                              const glm::vec4& color, int entityID)
    {
        DrawQuad(glm::vec3(position, 0.0f), size, color, entityID);
    }

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size,
                              const glm::vec4& color, int entityID)
    {
        // texIndex 0 = beyaz texture -> renk oldugu gibi cikar.
        SubmitQuad(MakeTransform(position, size), color, 0.0f, 1.0f, entityID);
    }

    void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size,
                              const std::shared_ptr<Texture2D>& texture,
                              float tilingFactor, const glm::vec4& tint, int entityID)
    {
        DrawQuad(glm::vec3(position, 0.0f), size, texture, tilingFactor, tint, entityID);
    }

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size,
                              const std::shared_ptr<Texture2D>& texture,
                              float tilingFactor, const glm::vec4& tint, int entityID)
    {
        const float texIndex = ResolveTextureSlot(texture);
        SubmitQuad(MakeTransform(position, size), tint, texIndex, tilingFactor, entityID);
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size,
                                     float rotationRadians, const glm::vec4& color, int entityID)
    {
        DrawRotatedQuad(glm::vec3(position, 0.0f), size, rotationRadians, color, entityID);
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size,
                                     float rotationRadians, const glm::vec4& color, int entityID)
    {
        SubmitQuad(MakeRotatedTransform(position, size, rotationRadians), color, 0.0f, 1.0f, entityID);
    }

    void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size,
                                     float rotationRadians,
                                     const std::shared_ptr<Texture2D>& texture,
                                     float tilingFactor, const glm::vec4& tint, int entityID)
    {
        const float texIndex = ResolveTextureSlot(texture);
        SubmitQuad(MakeRotatedTransform(glm::vec3(position, 0.0f), size, rotationRadians),
                   tint, texIndex, tilingFactor, entityID);
    }

    Renderer2D::Statistics Renderer2D::GetStats() { return s_Data.Stats; }
    void Renderer2D::ResetStats()                 { s_Data.Stats = Statistics{}; }
}
