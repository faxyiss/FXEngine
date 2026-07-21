#pragma once

// ===========================================================================
// FXEngine - EntityRef
//
// Baska bir entity'ye KALICI referans. BU, FAZ 8'IN ASIL SEBEBIDIR.
//
// Neden dogrudan Entity (veya entt::entity) saklamiyoruz?
//   1) entt::entity yuklemede degisir -> dosyaya yazilamaz
//   2) Hedef silinirse tutamak GECERSIZ olur ama GECERLI GORUNUR
//      (EnTT kimlikleri geri donusturur) -> sessizce yanlis entity'ye
//      baglanirsin. Faz 7'de bu tuzagi bizzat yasadik.
//
// UUID saklayip GEC COZUMLEME (lazy resolve) yapmak ikisini de cozer:
// hedef yoksa arama basarisiz olur ve bunu ANLARIZ.
//
// Bilincli olarak Scene'i BILMIYOR - sadece bir UUID tasiyor. Cozumleme
// Scene::FindEntityByUUID ile cagiran tarafta yapilir.
//
// KENDI HEADER'INDA (Components.h'tan cikarildi): hem Components.h hem de
// ScriptFields.h buna ihtiyac duyuyor ve Components.h zaten ScriptFields.h'i
// include ediyor - dairesel bagimliligi kirmak icin ortak bir alt basliga
// alindi. Yalnizca UUID'ye bagli.
// ===========================================================================

#include "FXEngine/Core/UUID.h"

namespace FX
{
    struct EntityRef
    {
        UUID Target{ 0 };   // 0 = hedef yok

        EntityRef() = default;
        EntityRef(UUID target) : Target(target) {}

        bool IsSet() const { return Target.IsValid(); }
        void Clear() { Target = UUID(0); }
    };
}
