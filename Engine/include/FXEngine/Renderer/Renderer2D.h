#pragma once

// ===========================================================================
// FXEngine - Renderer2D (Batch Renderer)
//
// PROBLEM: Her sprite icin ayri bir glDrawElements cagrisi yapmak.
// Modern bir GPU saniyede milyonlarca ucgen cizebilir ama ancak birkac bin
// draw call kaldirabilir. 10.000 sprite'i tek tek cizersen darbogaz GPU
// degil, CPU'nun SURUCUYLE KONUSMASI olur - GPU bosta beklerken CPU tikanir.
//
// COZUM: CPU tarafinda buyuk bir kose dizisi doldur, kare sonunda hepsini
// bir kerede GPU'ya gonder ve TEK draw call yap.
//
// BUNUN UC SONUCU VAR (ve ucu de ogretici):
//   1) Transform artik uniform OLAMAZ -> donusum CPU'da hesaplanir,
//      kose pozisyonlari dogrudan dunya koordinatinda yazilir.
//   2) Renk artik uniform OLAMAZ -> her kosenin verisine girer.
//   3) Texture artik uniform OLAMAZ -> TEXTURE SLOT'lari devreye girer:
//      32 texture 32 ayri slota baglanir, her kose hangi slotu
//      kullanacagini bir float olarak tasir.
//
// KULLANIM:
//   Renderer2D::BeginScene(camera);
//     Renderer2D::DrawQuad(...);   // istedigin kadar
//   Renderer2D::EndScene();        // asil gonderim burada
// ===========================================================================

#include "FXEngine/Renderer/OrthographicCamera.h"
#include "FXEngine/Renderer/Texture.h"

#include <cstdint>
#include <memory>

#include <glm/glm.hpp>

namespace FX
{
    class Renderer2D
    {
    public:
        // Bir kez cagrilir: shader, buffer'lar, beyaz texture hazirlanir.
        static void Init();
        static void Shutdown();

        // Kare basina: kameranin matrisini alir, batch'i sifirlar.
        static void BeginScene(const OrthographicCamera& camera);

        // Biriken her seyi gonderir. BU CAGRILMAZSA HICBIR SEY CIZILMEZ -
        // cunku DrawQuad sadece CPU tarafindaki diziye yazar.
        static void EndScene();

        // --- Cizim ------------------------------------------------------------
        // Not: position quad'in MERKEZIDIR, sol alt kosesi degil.
        // Merkez tabanli olmak dondurme ve olcekleme islemlerini
        // dogal kilar (donme noktasi merkez olur).

        // Duz renkli
        static void DrawQuad(const glm::vec2& position, const glm::vec2& size,
                             const glm::vec4& color);
        static void DrawQuad(const glm::vec3& position, const glm::vec2& size,
                             const glm::vec4& color);

        // Texture'li
        static void DrawQuad(const glm::vec2& position, const glm::vec2& size,
                             const std::shared_ptr<Texture2D>& texture,
                             float tilingFactor = 1.0f,
                             const glm::vec4& tint = glm::vec4(1.0f));
        static void DrawQuad(const glm::vec3& position, const glm::vec2& size,
                             const std::shared_ptr<Texture2D>& texture,
                             float tilingFactor = 1.0f,
                             const glm::vec4& tint = glm::vec4(1.0f));

        // Dondurulmus surumler. Ayri fonksiyon olmasinin sebebi: donme
        // gerektirmeyen quad'lar icin sin/cos hesabini ve matris carpimini
        // tamamen atlayabilmek. Cogu sprite dondurulmez; bu ayrim
        // binlerce quad'da olculebilir fark yaratir.
        static void DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size,
                                    float rotationRadians, const glm::vec4& color);
        static void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size,
                                    float rotationRadians, const glm::vec4& color);
        static void DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size,
                                    float rotationRadians,
                                    const std::shared_ptr<Texture2D>& texture,
                                    float tilingFactor = 1.0f,
                                    const glm::vec4& tint = glm::vec4(1.0f));

        // --- Istatistik --------------------------------------------------------
        // Batch renderer'in ISE YARADIGINI OLCEBILMEK icin. Sayilari
        // gormeden "optimize ettim" demek anlamsizdir.
        struct Statistics
        {
            std::uint32_t DrawCalls = 0;
            std::uint32_t QuadCount = 0;

            std::uint32_t GetVertexCount() const { return QuadCount * 4; }
            std::uint32_t GetIndexCount()  const { return QuadCount * 6; }
        };

        static Statistics GetStats();
        static void ResetStats();

        // Bu makinede kac texture slotu var? (GL 3.3 en az 16 garanti eder)
        static std::uint32_t GetMaxTextureSlots();

    private:
        // Mevcut batch'i gonderir ve yenisini baslatir.
        // Iki durumda cagrilir: (a) dizi doldu, (b) texture slotlari doldu.
        static void Flush();
        static void StartNewBatch();
    };
}
