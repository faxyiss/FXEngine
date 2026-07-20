#pragma once

// ===========================================================================
// FXEngine - RenderCommand
//
// OpenGL cagrilarinin motor icindeki TEK kapisi.
//
// NEDEN? Faz 1'de hem Application hem EditorApp glClear cagiriyordu - iki
// yerde ayni is, hangisinin kazandigi belirsiz. Bu tipik bir "sizinti"dir:
// grafik API cagrilari kodun her yerine dagilirsa, ileride bir seyi
// degistirmek imkansizlasir.
//
// Bu sinif ince bir katman: mantik icermez, sadece gl* cagrilarini toparlar.
// Faydasi ilerideki fazlarda ortaya cikacak (durum onbellegi, istatistik
// toplama, farkli API destegi). Simdilik en buyuk faydasi: "OpenGL cagrisi
// nerede yapiliyor?" sorusunun tek bir cevabi olmasi.
// ===========================================================================

#include "FXEngine/Renderer/VertexArray.h"

#include <cstdint>
#include <memory>

#include <glm/glm.hpp>

namespace FX
{
    class RenderCommand
    {
    public:
        // Bir kez, context olustuktan sonra cagrilir. Baslangic GL durumunu kurar.
        static void Init();

        static void SetViewport(std::uint32_t x, std::uint32_t y,
                                std::uint32_t width, std::uint32_t height);

        static void SetClearColor(const glm::vec4& color);
        static void Clear();

        // Indexli cizim. indexCount 0 ise VAO'nun kendi index sayisini kullanir.
        static void DrawIndexed(const std::shared_ptr<VertexArray>& vertexArray,
                                std::uint32_t indexCount = 0);

        // Hata ayiklama: tel kafes (wireframe) modu. Ucgenlerin gercekten
        // nasil yerlestigini gormek icin paha bicilmez.
        static void SetWireframe(bool enabled);
    };
}
