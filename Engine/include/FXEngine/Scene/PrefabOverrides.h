#pragma once

// ===========================================================================
// FXEngine - Prefab override tespiti (C-3)
//
// SORU: ornekteki hangi alan kaynaktan sapmis?
//
// CEVABIN YONTEMI: acik takip DEGIL, DIFF. Override bilgisini ikinci bir
// yerde saklamiyoruz - kaynak + ornek zaten var, fark her soruldugunda
// hesaplaniyor ("ayni bilgi iki yerde durmasin"). Kaynak dosya bir
// onbellekte tutulur ve yazma zamani degisince kendiliginden tazelenir;
// karsilastirmanin kare basi maliyeti kucuk JSON kiyaslamalari.
//
// EntityRef alanlari ozel: ornegin ic referansi ornek kimligi tasir,
// kaynak dosya kaynak kimligi. Dogrudan kiyas hep "farkli" derdi; kiyastan
// once ornek hedefi kaynak uzayina cevrilir (SourceId).
// ===========================================================================

#include "FXEngine/Scene/Entity.h"
#include "FXEngine/Asset/Asset.h"

#include <vector>

namespace FX
{
    class Scene;
    class TextureLibrary;
    struct ComponentInfo;

    namespace PrefabOverrides
    {
        // instance'in info component'inde KAYNAKTAN farkli alanlarin
        // adlari (FieldInfo::Name isaretcileri - tablo yasadigi surece
        // gecerli). Kaynak yoksa bos; component kaynakta hic yoksa
        // (ornekte eklenmis) TUM alanlar.
        std::vector<const char*> OverriddenFields(Entity instance,
                                                  const ComponentInfo& info);

        // Tek alani kaynaktaki degerine dondurur. EntityRef degerleri
        // ornek uzayina cevrilir (ic referans ornegin kendi entity'sine
        // baglanir). Doner: deger gercekten degisti mi.
        bool RevertField(Entity instance, const ComponentInfo& info,
                         const char* fieldName);

        // Ayni prefab'a ait en distaki ata (ornegin koku). Prefab ornegi
        // degilse entity'nin kendisi. Panel/komut katmani islemleri hep
        // kokten baslatsin diye tek yerde.
        Entity InstanceRoot(Entity entity);

        // Sahnedeki bu prefab'a bagli TUM ornek kokleri (tekillestirilmis).
        std::vector<Entity> InstanceRootsOf(Scene& scene, AssetHandle prefab);

        // C-4 Apply: ornekteki degisiklikleri KAYNAK dosyaya yazar ve
        // sahnedeki DIGER orneklerin override edilmemis alanlarini yeni
        // kaynaga gunceller (override edilenler korunur - Unity davranisi).
        //
        // Dosyada kimlikler KAYNAK uzayinda kalir (SourceId'ler): diger
        // orneklerin bagi kopmaz. Kok konumu kaynaktaki degerinde birakilir
        // (ornege ozgu yerlestirme kaynaga sizmali degil - Revert'in
        // simetrigi). Ornekte EKLENEN/SILINEN entity'ler dosyaya
        // islenmez (yapisal apply C-5).
        //
        // Doner: dosya yazildi mi.
        bool ApplyInstance(Entity instanceRoot, TextureLibrary* library);
    }
}
