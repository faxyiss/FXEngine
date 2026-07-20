#pragma once

// ===========================================================================
// FXEngine - Varlik kimligi
//
// PROBLEM (Faz 12'den beri acik):
// Bir varligin kimligi DOSYA YOLUYDU. "assets/textures/player.png" yazan
// bir sahne, dosya baska bir klasore tasinir tasinmaz onu bulamaz hale
// geliyordu. Icerik panelinde tasima ozelligi eklendiginde bu somut bir
// tuzaga donustu: kullaniciya bir arac verip "kullanirsan bir seyler
// bozulur" demek zorunda kaldik.
//
// COZUM: kimlik ile konumu AYIRMAK.
//   - Her varliga bir GUID atanir ve yanindaki .meta dosyasinda saklanir.
//   - Sahne dosyasi GUID saklar, yol saklamaz.
//   - Dosya tasindiginda .meta de onunla gider; GUID degismez.
//
// Bu, Faz 8'de entity icin ogrendigimiz ilkenin varliga uygulanmis hali:
// KIMLIK ILE KONUM AYRI SEYLERDIR. (Faz 12 notlari, bolum 3.)
//
// Unity'nin .meta dosyalari tam olarak budur; Unreal ayni bilgiyi
// .uasset basliginda tutar.
// ===========================================================================

#include "FXEngine/Core/UUID.h"

#include <string>

namespace FX
{
    // Varlik kimligi. Entity UUID'siyle AYNI tip ama farkli bir uzayda
    // yasiyor: bir entity ile bir texture ayni sayiya sahip olabilir ve
    // bu bir sorun degildir - hicbir yerde ayni tabloda aranmiyorlar.
    using AssetHandle = UUID;

    enum class AssetType
    {
        None = 0,
        Texture,
        Scene,
        Prefab
    };

    const char* AssetTypeToString(AssetType type);
    AssetType   AssetTypeFromString(const std::string& text);

    // Uzantidan tur cikarimi. Tanimadigimiz uzantilar None doner ve
    // varlik veritabanina ALINMAZ - her dosyaya GUID atamak .meta
    // cöplugu yaratirdi.
    AssetType AssetTypeFromExtension(const std::string& extension);

    // Bir varlik hakkinda bildiklerimiz. Yol PROJE KOKUNE goreli ve
    // '/' ayirici kullanir; boylece .meta dosyalari platformlar arasi
    // tasinabilir.
    struct AssetMetadata
    {
        AssetHandle Handle{ 0 };
        std::string RelativePath;
        AssetType   Type = AssetType::None;

        bool IsValid() const { return Handle.IsValid() && Type != AssetType::None; }
    };
}
