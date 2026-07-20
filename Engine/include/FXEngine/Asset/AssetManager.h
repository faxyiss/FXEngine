#pragma once

// ===========================================================================
// FXEngine - AssetManager
//
// Varlik veritabani: GUID <-> yol eslemesi.
//
// NEDEN PROJEYE BAGLI? Cunku tarayacagi bir kok gerekiyor. Faz 21'de proje
// kavrami gelene kadar bu sinif yazilamazdi - yol haritasinda "AssetManager
// Faz 21'e bagli" diye not dusulmesinin sebebi buydu.
//
// TARAMA MODELI: proje acilirken tum varlik klasoru bir kez geziliyor.
// Alternatifi tembel yukleme (istendiginde ara) olurdu ama o zaman
// "GUID 123 hangi dosya?" sorusuna cevap vermek icin yine tum agaci
// taramak gerekirdi. Bir kez tara, tabloda tut.
//
// Dosya sistemi DISARIDAN degisirse (Explorer'da tasima) tablo bayatlar.
// Icerik paneli kendi islemlerini bildiriyor; disaridan degisiklikler
// icin "Yenile" var. Dogrusu bir dosya izleyici - teknik borc listesinde.
// ===========================================================================

#include "FXEngine/Asset/Asset.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace FX
{
    class AssetManager
    {
    public:
        // Aktif projenin varlik klasorunu tarar. .meta'si olmayan her
        // varliga GUID atayip .meta yazar.
        //
        // Proje acilirken cagrilir; proje yoksa hicbir sey yapmaz.
        static void ScanProject();

        static void Clear();

        // Yol -> GUID. Varlik tabloda yoksa gecersiz handle doner.
        static AssetHandle GetHandle(const std::string& relativePath);

        // GUID -> yol. Bulunamazsa BOS string.
        //
        // Referans doner: cagiran yerlerde her cagride string kopyalamak
        // istemiyoruz (sahne yuklerken entity basina cagriliyor).
        static const std::string& GetPath(AssetHandle handle);

        static bool Exists(AssetHandle handle);

        static const AssetMetadata& GetMetadata(AssetHandle handle);

        // --- Dosya sistemi olaylari -----------------------------------------
        // Icerik paneli bunlari cagiriyor. Tablo ile diskin senkron
        // kalmasi bu cagrilara bagli; unutulan bir cagri "GUID var ama
        // dosya baska yerde" durumuna yol acar.

        // Yeni bir varlik eklendi (ice aktarma). GUID uretir, .meta yazar.
        static AssetHandle Register(const std::string& relativePath);

        // Varlik tasindi veya adi degisti. GUID KORUNUR - butun mesele bu.
        static void OnAssetMoved(const std::string& oldRelative,
                                 const std::string& newRelative);

        static void OnAssetDeleted(const std::string& relativePath);

        // Bir varligin .meta dosyasinin yolu ("x.png" -> "x.png.meta").
        static std::string MetaPathFor(const std::string& relativePath);

        // .meta dosyasi mi? Icerik paneli bunlari gizliyor.
        static bool IsMetaFile(const std::string& path);

        static std::size_t GetCount();

        // Tum kayitlar (icerik paneli / hata ayiklama icin).
        static std::vector<AssetMetadata> GetAll();

    private:
        // Diskteki .meta'yi okur; yoksa veya bozuksa yeni GUID uretip yazar.
        static AssetMetadata LoadOrCreateMeta(const std::string& relativePath,
                                              AssetType type);

        static bool WriteMeta(const AssetMetadata& meta);
    };
}
