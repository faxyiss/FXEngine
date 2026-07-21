#pragma once

// ===========================================================================
// FXEngine - EntitySnapshot / ComponentSnapshot
//
// Bir entity'yi (ve alt agacini) ya da tek bir component'i DONDURUP geri
// koymanin yolu. Editorun yapisal Undo'su bunun ustune kuruluyor: "sil"
// komutunun geri alinmasi, silinen seyin ne oldugunu bir yerde tutmayi
// gerektiriyor.
//
// NEDEN JSON METNI? Cunku kaydetme yolu zaten var ve DOGRULANMIS: sahne
// dosyasi da aynen boyle uretiliyor. Ikinci bir "entity kopyalama" yolu
// yazsaydik, yeni bir component eklendiginde biri guncellenip digeri
// unutulurdu - A1'in cozdugu hatanin aynisi. Bedeli birkac kilobayt metin
// ve bir ayristirma; Undo adimi basina hicbir sey.
//
// UUID'ler KORUNUR: geri konan entity ayni entity'dir, benzeri degil.
// Baska entity'lerin ona tutan referanslari (EntityRef) bu sayede geri
// alma sonrasinda da hedefini bulur.
// ===========================================================================

#include "FXEngine/Core/UUID.h"
#include "FXEngine/Scene/Entity.h"

#include <string>

namespace FX
{
    class Scene;
    class TextureLibrary;
    struct ComponentInfo;

    class EntitySnapshot
    {
    public:
        // entity ve TUM torunlari. Gecersiz entity -> bos snapshot.
        static EntitySnapshot Capture(Entity root);

        // Sahneye geri koyar; kok entity'yi doner (basarisizsa gecersiz).
        // Kokun parent'i hala yasiyorsa bag da yeniden kurulur.
        //
        // Ayni UUID'ye sahip bir entity zaten varsa DOKUNULMAZ: iki kez
        // Restore cagirmak ikinci bir kopya uretmemeli.
        Entity Restore(Scene& scene, TextureLibrary* library) const;

        bool Empty() const { return m_Data.empty(); }
        UUID RootID() const { return m_Root; }

    private:
        std::string m_Data;
        UUID        m_Root{ 0 };
    };

    class ComponentSnapshot
    {
    public:
        // owner'da o component yoksa bos snapshot doner.
        static ComponentSnapshot Capture(Entity owner, const ComponentInfo& info);

        // Component'i verisiyle geri koyar. Doner: uygulandi mi?
        bool Restore(Entity owner, TextureLibrary* library) const;

        bool Empty() const { return m_Data.empty(); }
        const std::string& ComponentName() const { return m_Name; }

    private:
        std::string m_Name;
        std::string m_Data;
    };
}
