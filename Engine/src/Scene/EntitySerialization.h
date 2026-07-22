#pragma once

// Sahne ve prefab serilestiricilerinin PAYLASTIGI entity <-> JSON cevrimi.
// include/ altinda degil src/ altinda: bu bir ic detay, motorun public
// API'si degil. Yarin format degisirse disariya hicbir sey yansimaz.

#include "FXEngine/Scene/Entity.h"
#include "FXEngine/Core/UUID.h"
#include "FXEngine/Renderer/TextureLibrary.h"

#include <nlohmann/json.hpp>

#include <unordered_map>

namespace FX
{
    struct ComponentInfo;
    struct FieldInfo;
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

    // TEK alani JSON'dan component'e okur (obj[f.Name] yoksa dokunmaz).
    // ApplyComponent'in altindaki adim; alan duzeyinde revert (C-3) bunu
    // dogrudan kullaniyor - ApplyComponent'ten fark, LoadExtra'yi
    // CALISTIRMAMASI (tek alan geri alinirken doku/script verisi
    // etkilenmemeli).
    void ReadField(const FieldInfo& f, void* component, const nlohmann::json& in);

    // entity'nin TUM entity referanslarini remap tablosundan gecirir:
    // component EntityRef alanlari (Follow.Target gibi) VE script Entity
    // alanlari (NativeScriptComponent.Fields). Tabloda olmayan UUID
    // DOKUNULMAZ - prefab/kopyanin DISINA bakan referans hala gecerlidir.
    //
    // Prefab ornekleme (kimlik yeniden uretir) ve entity cogaltma ortak
    // kullaniyor; iki ayri kopya olsaydi biri EntityRef script alanini
    // remap etmeyi unuturdu (prefab tarafinda tam olarak bu eksikti).
    void RemapReferences(Entity entity,
                         const std::unordered_map<UUID, UUID>& remap);
}
