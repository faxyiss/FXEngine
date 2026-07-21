#pragma once

// ===========================================================================
// Ornek sahne - Faz 5-8'in kanit sahnesi.
//
// Editorun kalici bir parcasi degil: karsilama ekranindaki "projesiz
// devam et" ve "Sahneyi Sifirla" icin duruyor. Bu yuzden EditorApp'in
// uyesi degil, serbest fonksiyon.
// ===========================================================================

#include <FXEngine/Core/UUID.h>
#include <FXEngine/Renderer/Texture.h>
#include <FXEngine/Scene/Scene.h>

#include <memory>

namespace FXEd::SampleScene
{
    using TexturePtr = std::shared_ptr<FX::Texture2D>;

    // Sahneyi doldurur ve "Oyuncu" entity'sinin UUID'sini dondurur -
    // takipciler ve kamera ona bagli, cagiran da onu secmek istiyor.
    FX::UUID Build(FX::Scene& scene, const TexturePtr& checkerboard,
                   const TexturePtr& circle);

    // Rastgele konumda, rastgele hizda tek bir gezgin ekler.
    void SpawnMover(FX::Scene& scene, const TexturePtr& circle);
}
