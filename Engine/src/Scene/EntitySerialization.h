#pragma once

// Sahne ve prefab serilestiricilerinin PAYLASTIGI entity <-> JSON cevrimi.
// include/ altinda degil src/ altinda: bu bir ic detay, motorun public
// API'si degil. Yarin format degisirse disariya hicbir sey yansimaz.

#include "FXEngine/Scene/Entity.h"
#include "FXEngine/Renderer/TextureLibrary.h"

#include <nlohmann/json.hpp>

namespace FX
{
    struct ComponentInfo;
}

namespace FX::Detail
{
    nlohmann::json SerializeEntity(Entity entity);

    // Parent bagini KURMAZ - onu cagiran ikinci geciste yapar, cunku
    // dosyada bir cocuk parent'indan once gelebilir.
    void ApplyComponents(Entity entity, const nlohmann::json& j, TextureLibrary* library);

    // TEK component'in verisi. Entity duzeyindeki ikilinin altinda yatan
    // adim; ayrica "component'i sil, geri al" komutunun ihtiyaci olan
    // sey tam olarak bu (EntitySnapshot).
    nlohmann::json SerializeComponent(const ComponentInfo& info, Entity entity);
    void ApplyComponent(const ComponentInfo& info, Entity entity,
                        const nlohmann::json& obj, TextureLibrary* library);
}
