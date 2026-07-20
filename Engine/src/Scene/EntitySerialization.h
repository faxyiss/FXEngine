#pragma once

// Sahne ve prefab serilestiricilerinin PAYLASTIGI entity <-> JSON cevrimi.
// include/ altinda degil src/ altinda: bu bir ic detay, motorun public
// API'si degil. Yarin format degisirse disariya hicbir sey yansimaz.

#include "FXEngine/Scene/Entity.h"
#include "FXEngine/Renderer/TextureLibrary.h"

#include <nlohmann/json.hpp>

namespace FX::Detail
{
    nlohmann::json SerializeEntity(Entity entity);

    // Parent bagini KURMAZ - onu cagiran ikinci geciste yapar, cunku
    // dosyada bir cocuk parent'indan once gelebilir.
    void ApplyComponents(Entity entity, const nlohmann::json& j, TextureLibrary* library);
}
