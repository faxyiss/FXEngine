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
// Kok her zaman <exe>/assets; disari cikilamaz.
// ===========================================================================

#include <FXEngine/Renderer/TextureLibrary.h>

#include <filesystem>
#include <string>
#include <vector>

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

        const std::filesystem::path& GetCurrentDirectory() const { return m_Current; }

        // Disaridan bir dosyayi mevcut klasore KOPYALAR (tasimaz - kaynak
        // dosya kullanicinin, ona dokunmuyoruz). Goreceli yolu doner,
        // basarisizsa bos string.
        std::string ImportFile(const std::string& absoluteSource);

        // Kullanici "Ice Aktar..." dediyse true doner ve istegi temizler.
        // Yerel dosya diyalogu modaldir; ImGui cercevesinin ortasinda
        // acilamaz, bu yuzden EditorApp'e devrediyoruz.
        bool TakeImportRequest();

        // Klasor icerigi degistiginde (disaridan kopyalama vb.) cagrilir.
        void Refresh() { m_NeedsRefresh = true; }

    private:
        void DrawToolbar();
        void DrawEntry(const std::filesystem::directory_entry& entry);
        void DrawModals();

        void RefreshIfNeeded();

        FX::TextureLibrary* m_Library = nullptr;

        std::filesystem::path m_Root;      // <exe>/assets
        std::filesystem::path m_Current;

        // Klasor icerigi her karede degil, sadece degistiginde okunuyor.
        // 60 Hz'de disk gezmek gereksiz ve buyuk klasorlerde fark edilir.
        std::vector<std::filesystem::directory_entry> m_Directories;
        std::vector<std::filesystem::directory_entry> m_Files;
        bool m_NeedsRefresh = true;

        // Bir ogeyi baska bir klasore TASIR (kopyalamaz). Kaynak proje
        // icindeki bir dosya/klasor; hedef bir klasor.
        void MoveItem(const std::filesystem::path& source,
                      const std::filesystem::path& targetDir);

        // Surukleme yukunu kabul edip tasimayi baslatan yardimci.
        // Klasor ogesi, "ust klasor" dugmesi ve kirinti yolu parcalari
        // ayni davranisi paylasiyor.
        void AcceptMoveTarget(const std::filesystem::path& targetDir);

        void SetMessage(const std::string& text);

        // Islemler dongu DISINDA uygulanir: uzerinde gezdigimiz listeyi
        // gezerken degistirmek yineleyicileri bozar.
        std::filesystem::path m_RenameTarget;
        std::filesystem::path m_DeleteTarget;

        // Tasima da dongu disinda: rename cagrisi dizin listesini
        // gecersiz kilar.
        std::filesystem::path m_MoveSource;
        std::filesystem::path m_MoveTarget;

        // Kullaniciya geri bildirim (tasima hatasi, referans uyarisi).
        std::string m_Message;
        float       m_MessageTimer = 0.0f;
        bool m_OpenNewFolderPopup = false;
        bool m_ImportRequest      = false;

        char m_NameBuffer[128]{};
        char m_SearchBuffer[64]{};

        float m_ThumbnailSize = 84.0f;
        float m_Padding       = 16.0f;
    };
}
