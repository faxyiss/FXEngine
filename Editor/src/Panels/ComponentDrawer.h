#pragma once

// ===========================================================================
// Component meta tablosundan Inspector cizimi (A1)
//
// Elle yazilmis component bloklari yerine alan tablosu geziliyor. Tipe
// gore tek bir cizici var; ozel durumlar (doku slotu, kamera tekilligi,
// script listesi) tabloya BAGLANIYOR, ikinci bir listede yasamiyor.
// ===========================================================================

#include <FXEngine/Renderer/TextureLibrary.h>
#include <FXEngine/Scene/ComponentMeta.h>
#include <FXEngine/Scene/Entity.h>

namespace FXEd::ComponentDrawer
{
    // Editore ozgu cizicileri meta tabloya baglar. Editor acilista bir
    // kez cagirir; kutuphane isaretcisi doku slotunun ihtiyaci.
    void RegisterEditorUI(FX::TextureLibrary* library);

    // Bir component'in tum gorunur alanlarini ve ozel arayuzunu cizer.
    void DrawComponentBody(const FX::ComponentInfo& info, FX::Entity entity);
}
