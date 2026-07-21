#pragma once

// ===========================================================================
// FXEngine - Project
//
// Bir editorun "KURULU OLDUGU YER" ile "UZERINDE CALISTIGI YER" farkli
// seylerdir. Word'un kendisi Program Files'tadir, belgen Masaustunde.
//
// Faz 21'e kadar bu ayrim yoktu: editor <exe>/assets'i kok aliyordu ve
// ice aktarilan her varlik build/ altinda kaliyordu. build/ klasorunu
// silmek kullanicinin emegini de silerdi - ve build/ silmek gunluk bir
// islemdir.
//
// .fxproject dosyasi projenin KOKUNDE durur; onun bulundugu klasor
// projenin kokudur. Yani proje "kendini tarif eden" bir klasordur -
// tasinabilir, surum kontrolune girebilir, baska makinede acilabilir.
// ===========================================================================

#include "FXEngine/Asset/Asset.h"

#include <cstdint>
#include <memory>
#include <string>

namespace FX
{
    struct ProjectConfig
    {
        std::string Name = "Isimsiz Proje";

        // Proje kokune GORECELI. Varlik tarayicisi burayi gosterir ve
        // varlik yollari buna gore saklanir.
        std::string AssetDirectory = "assets";

        // Editor acilista bunu yukler. Gecersiz = bos sahne ile basla.
        //
        // GUID, yol DEGIL (0.6): sahne dosyasini tasimak ya da adini
        // degistirmek projeyi bozmasin. Yol tabanliyken .fxproject'i elle
        // duzeltmek gerekiyordu.
        AssetHandle StartScene{ 0 };

        // Oyunun tasarlandigi cozunurluk (surum 3). Game View bunun
        // EN-BOY ORANINI kullanir; piksel sayisi degil oran onemli -
        // gercek pencere her boyutta olabilir.
        //
        // Projeye ait cunku "bu oyun 16:9 tasarlandi" projenin bir
        // karari; kullanicinin makinesine gore degismez.
        std::uint32_t TargetWidth  = 1920;
        std::uint32_t TargetHeight = 1080;

        float TargetAspect() const
        {
            return (TargetHeight > 0)
                 ? static_cast<float>(TargetWidth) / static_cast<float>(TargetHeight)
                 : 1.0f;
        }
    };

    class Project
    {
    public:
        // Dosyadan yukler ve AKTIF proje yapar (FileSystem'in proje
        // kokunu da ayarlar). Basarisizsa nullptr.
        //
        // filepath: .fxproject dosyasinin MUTLAK yolu.
        static std::shared_ptr<Project> Load(const std::string& filepath);

        // Yeni proje olusturur: klasor yapisini kurar, .fxproject yazar
        // ve aktif proje yapar.
        //
        // directory: projenin kokü olacak MUTLAK yol (yoksa olusturulur).
        static std::shared_ptr<Project> Create(const std::string& directory,
                                               const std::string& name);

        // Aktif projeyi diske yazar.
        bool Save() const;

        // Govdeleri .cpp'de: s_Active bir VERI sembolu ve DLL sinirinda
        // disa aktarilmiyor. Inline kalsalarsa Editor s_Active'i dogrudan
        // okumaya calisir ve link kirilir.
        static const std::shared_ptr<Project>& GetActive();
        static bool HasActive();

        // Aktif projeyi kapatir ve FileSystem'i exe klasorune dondurur.
        static void CloseActive();

        const ProjectConfig& GetConfig() const { return m_Config; }
        ProjectConfig&       GetConfig()       { return m_Config; }

        // Projenin kok klasoru (sonunda ayirici ile).
        const std::string& GetDirectory() const { return m_Directory; }

        // .fxproject dosyasinin tam yolu.
        const std::string& GetFilePath() const { return m_FilePath; }

        static constexpr const char* kExtension = ".fxproject";

    private:
        static void SetActive(const std::shared_ptr<Project>& project);

        ProjectConfig m_Config;
        std::string   m_Directory;
        std::string   m_FilePath;

        // Ayni anda tek proje acik. Birden fazla proje destegi, her
        // yerde "hangi proje?" sorusunu tasimak demek - editorler
        // bunu yapmaz, ikinci proje icin ikinci pencere acilir.
        static std::shared_ptr<Project> s_Active;
    };
}
