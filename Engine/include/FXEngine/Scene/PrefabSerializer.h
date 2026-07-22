#pragma once

// ===========================================================================
// FXEngine - PrefabSerializer
//
// Prefab = bir entity AGACININ sablonu. Bir kez kurarsin, sahneye
// istedigin kadar ORNEK (instance) koyarsin.
//
// SAHNE ILE FARKI TEK BIR SEY: kimlik.
// Sahne yuklerken UUID'ler KORUNUR - amac dosyadaki dunyayi aynen geri
// getirmek. Prefab orneklerken UUID'ler YENIDEN URETILIR - ayni prefab'i
// iki kez koyabilmen gerekiyor ve iki ornek ayni kimlige sahip olamaz.
//
// Bunun bedeli: prefab ICINDEKI referanslar (parent, follow hedefi) eski
// kimlige isaret ediyor. Ornekleme sirasinda eski->yeni bir esleme tablosu
// tutup bu referanslari CEVIRIYORUZ. Buna "ID remapping" denir ve prefab
// sisteminin can alici noktasidir.
// ===========================================================================

#include "FXEngine/Scene/Scene.h"
#include "FXEngine/Renderer/TextureLibrary.h"

#include <glm/glm.hpp>

#include <string>

namespace FX
{
    class Entity;

    class PrefabSerializer
    {
    public:
        PrefabSerializer(Scene* scene, TextureLibrary* textureLibrary);

        // root ve TUM alt agaci dosyaya yazilir.
        bool Save(Entity root, const std::string& filepath);

        // Sahneye yeni kimliklerle ekler; kok entity doner (basarisizsa gecersiz).
        Entity Instantiate(const std::string& filepath, const glm::vec3& position);

        // Ornegi kaynak prefab'ina gore GERI ALIR (C-2 Revert).
        // SourceId ile eslesen entity'lerin component'lerini kaynaktan
        // yeniden yaziyor. KORUNAN: her entity'nin UUID'i, hiyerarsi,
        // prefab bagi ve KOK entity'nin konumu (Instantiate'in override
        // ettigi tek sey - ornek yerinde kalsin). Ornekte EKLENMIS
        // component'ler kaldirilir, ornekte SILINEN kaynak component'leri
        // geri gelir. Ic referanslar (Follow, script EntityRef) kaynak
        // kimliginden ornek kimligine yeniden esleniyor.
        //
        // Kapsam disi (C-5): ornekte eklenen/silinen COCUK entity'ler.
        // Doner: en az bir entity donduruldu mu.
        bool RevertInstance(Entity instanceRoot);

    private:
        Scene*          m_Scene   = nullptr;
        TextureLibrary* m_Library = nullptr;
    };
}
