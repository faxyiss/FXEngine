#pragma once

// ===========================================================================
// FXEditor - ContentBrowserPanel
//
// assets/ klasorunu gezen dosya tarayicisi.
//
// Panel dosya SISTEMINI okur, bir varlik veritabanini degil. Ikisi arasindaki
// fark Faz 12 notlarinda: gercek motorlar her dosyaya bir UUID atayip
// yaninda .meta dosyasi tutar; boylece dosyayi TASIYINCA referanslar
// kopmaz. Biz simdilik yolu kimlik olarak kullaniyoruz.
//
// Dizinden disari cikma (assets/ disina) engelli: kok her zaman assets/.
// ===========================================================================

#include <FXEngine/Renderer/TextureLibrary.h>

#include <filesystem>
#include <string>

namespace FXEd
{
    // Surukle-birak yuk tipi. Tasidigi sey: exe klasorune GORECELI yol
    // ('\0' dahil, C dizisi olarak). Entity payload'i gibi tutamak degil,
    // cunku hedefler farkli kareler icinde de gecerli olmasini bekliyor.
    inline constexpr const char* kContentPayload = "FX_CONTENT_ITEM";

    class ContentBrowserPanel
    {
    public:
        // Dokular kucuk resim (thumbnail) icin gerekli; ayni kutuphaneyi
        // sahne de kullaniyor, yani bir resmi tarayicida gormek onu
        // sahne icin de onbellege almis oluyor.
        void SetContext(FX::TextureLibrary* library);

        void OnImGuiRender();

    private:
        void DrawBreadcrumbs();
        void DrawEntry(const std::filesystem::directory_entry& entry);

        FX::TextureLibrary* m_Library = nullptr;

        std::filesystem::path m_Root;      // <exe>/assets
        std::filesystem::path m_Current;

        float m_ThumbnailSize = 84.0f;
        float m_Padding       = 12.0f;
    };
}
