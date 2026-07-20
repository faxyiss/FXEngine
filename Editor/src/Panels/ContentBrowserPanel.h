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

#include <imgui.h>

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
        // Izgara: buyuk kucuk resimler, gorsel varliklar icin.
        // Liste: tek sutun, ad + tur + boyut. Cok dosya varken ve
        //        adlar uzunken izgaradan cok daha okunur.
        enum class ViewMode { Grid, List };

        void DrawToolbar();
        void DrawModals();

        void DrawEntry(const std::filesystem::directory_entry& entry, int index);
        void DrawEntryGrid(const std::filesystem::directory_entry& entry, int index);
        void DrawEntryList(const std::filesystem::directory_entry& entry, int index);

        // --- Coklu secim ------------------------------------------------------
        // Explorer davranisi: tek tik secer, cift tik klasore girer,
        // Ctrl ekler/cikarir, Shift aralik secer.
        //
        // Tek tikla klasore girmeye devam etseydik secim ile gezinme ayni
        // eylemi paylasirdi ve klasorler hicbir zaman secilemezdi.
        void HandleSelectionClick(const std::filesystem::path& path, int index);

        bool IsSelected(const std::filesystem::path& path) const;

        // Suruklenen oge secimde degilse secim ONA ayarlanir; secimdeyse
        // tum secim tasinir. (Explorer da boyle yapar.)
        std::vector<std::filesystem::path> ItemsToDrag(
            const std::filesystem::path& dragged) const;

        // Gorunen siradaki (klasorler once, sonra dosyalar) tum ogeler.
        // Shift araligi bu sira uzerinden hesaplaniyor.
        std::vector<std::filesystem::path> VisibleOrder() const;

        void ClearSelection();

        // Iki gorunumun PAYLASTIGI davranis: surukleme kaynagi, birakma
        // hedefi, sag tik menusu, klasore girme.
        //
        // Ayri ayri yazsaydik bir duzeltmeyi iki yere uygulamak
        // gerekirdi ve biri er gec unutulurdu - Faz 12'de serilestirme
        // icin ogrendigimiz dersin aynisi.
        void DrawEntryInteractions(const std::filesystem::path& path,
                                   const std::string& filename,
                                   bool isDirectory, bool clicked, int index);

        // Ikonu verilen dikdortgene cizer (izgarada buyuk, listede kucuk).
        void DrawEntryIcon(const std::filesystem::path& path, bool isDirectory,
                           const ImVec2& min, const ImVec2& max);

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
        // gecersiz kilar. Coklu secimde birden fazla kaynak olabiliyor.
        std::vector<std::filesystem::path> m_MoveSources;
        std::filesystem::path              m_MoveTarget;

        // Secili ogeler. Klasor degisince temizleniyor: baska bir
        // klasordeki ogeleri secili tutmak, gorunmeyen dosyalar uzerinde
        // islem yapilmasina yol acardi.
        std::vector<std::filesystem::path> m_Selected;

        // Shift araligi icin son tiklanan ogenin gorunen sirasi.
        int m_LastClickedIndex = -1;

        // Kullaniciya geri bildirim (tasima hatasi, referans uyarisi).
        std::string m_Message;
        float       m_MessageTimer = 0.0f;
        bool m_OpenNewFolderPopup = false;
        bool m_ImportRequest      = false;

        char m_NameBuffer[128]{};
        char m_SearchBuffer[64]{};

        ViewMode m_ViewMode = ViewMode::Grid;

        float m_ThumbnailSize = 84.0f;
        float m_Padding       = 16.0f;
    };
}
