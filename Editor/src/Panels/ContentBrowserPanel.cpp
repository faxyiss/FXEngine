#include "Panels/ContentBrowserPanel.h"
#include "Platform/FileDialogs.h"

#include <FXEngine/Core/FileSystem.h>
#include <FXEngine/Core/Project.h>
#include <FXEngine/Asset/AssetManager.h>
#include <FXEngine/Core/Log.h>

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

namespace FXEd
{
    namespace
    {
        std::string ToLower(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return s;
        }

        bool IsImage(const std::filesystem::path& p)
        {
            const std::string ext = ToLower(p.extension().string());
            return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp";
        }

        // Uzantiya gore vurgu rengi. Ikonun kendisi tek bicim; rengi ve
        // uzerindeki yazi dosya turunu ayirt ettiriyor.
        ImU32 AccentColor(const std::filesystem::path& p)
        {
            const std::string ext = ToLower(p.extension().string());
            if (ext == ".fxscene")  return IM_COL32( 70, 125, 190, 255);
            if (ext == ".fxprefab") return IM_COL32(150,  95, 180, 255);
            if (ext == ".vert" || ext == ".frag") return IM_COL32( 80, 150,  95, 255);
            if (ext == ".json")     return IM_COL32(200, 150,  60, 255);
            return IM_COL32(110, 115, 130, 255);
        }

        // -------------------------------------------------------------------
        // Ikonlar KOD ILE ciziliyor, dosyadan yuklenmiyor.
        //
        // Neden? Bir ikon seti eklemek assets/ icine karisan, surumlenmesi
        // gereken ve olceklendiginde bulaniklasan ikili dosyalar demek.
        // ImGui'nin cizim listesi zaten elimizde ve bu sekiller birkac
        // dikdortgen. Panel boyutu degisince de keskin kaliyorlar.
        // -------------------------------------------------------------------
        void DrawFolderIcon(ImDrawList* dl, ImVec2 min, ImVec2 max)
        {
            const float w = max.x - min.x;
            const float h = max.y - min.y;

            const ImVec2 p{ min.x + w * 0.14f, min.y + h * 0.24f };
            const ImVec2 q{ max.x - w * 0.14f, max.y - h * 0.20f };

            // Arka parca ve sekme
            dl->AddRectFilled(p, ImVec2(p.x + w * 0.36f, p.y + h * 0.16f),
                              IM_COL32(196, 152, 60, 255), 3.0f);
            dl->AddRectFilled(ImVec2(p.x, p.y + h * 0.06f), q,
                              IM_COL32(236, 190, 88, 255), 4.0f);
        }

        void DrawFileIcon(ImDrawList* dl, ImVec2 min, ImVec2 max,
                          ImU32 accent, const std::string& label)
        {
            const float w = max.x - min.x;
            const float h = max.y - min.y;

            const ImVec2 p{ min.x + w * 0.24f, min.y + h * 0.14f };
            const ImVec2 q{ max.x - w * 0.24f, max.y - h * 0.14f };
            const float  fold = w * 0.18f;

            dl->AddRectFilled(p, q, IM_COL32(232, 235, 242, 255), 3.0f);

            // Kivrilmis kose
            dl->AddTriangleFilled(ImVec2(q.x - fold, p.y),
                                  ImVec2(q.x, p.y + fold),
                                  ImVec2(q.x - fold, p.y + fold),
                                  IM_COL32(176, 182, 196, 255));

            // Uzanti seridi
            const ImVec2 b0{ p.x, q.y - h * 0.30f };
            const ImVec2 b1{ q.x, q.y - h * 0.08f };
            dl->AddRectFilled(b0, b1, accent, 2.0f);

            if (!label.empty())
            {
                const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
                const ImVec2 textPos{ (b0.x + b1.x - textSize.x) * 0.5f,
                                      (b0.y + b1.y - textSize.y) * 0.5f };

                // Serit dar kalirsa yaziyi kirp - tasarsa komsu hucrenin
                // uzerine yazardi.
                dl->PushClipRect(b0, b1, true);
                dl->AddText(textPos, IM_COL32(255, 255, 255, 235), label.c_str());
                dl->PopClipRect();
            }
        }

        // Uzun dosya adlarini ortadan kisalt: "cok_uzun_bir_ad.png" ->
        // "cok_uz....png". Sondan kesmek uzantiyi yok ederdi ve dosya
        // turunu ayirt edemezdin.
        std::string ShortenName(const std::string& name, float maxWidth)
        {
            if (ImGui::CalcTextSize(name.c_str()).x <= maxWidth)
                return name;

            const std::size_t dot = name.rfind('.');
            const std::string ext = (dot == std::string::npos) ? "" : name.substr(dot);

            std::string stem = (dot == std::string::npos) ? name : name.substr(0, dot);
            while (!stem.empty() &&
                   ImGui::CalcTextSize((stem + "..." + ext).c_str()).x > maxWidth)
            {
                stem.pop_back();
            }

            return stem + "..." + ext;
        }
    }

    void ContentBrowserPanel::SetContext(FX::TextureLibrary* library)
    {
        m_Library = library;

        // Klasor adi PROJEDEN geliyor, sabit "assets" degil. AssetManager
        // zaten config'e bakiyordu; panel bakmayinca ikisi farkli klasoru
        // dogru sanabilirdi.
        m_AssetDir = FX::Project::HasActive()
                         ? FX::Project::GetActive()->GetConfig().AssetDirectory
                         : std::string("assets");

        m_Root    = std::filesystem::path(FX::FileSystem::ResolveProjectAsset(m_AssetDir));
        m_Current = m_Root;

        std::error_code ec;
        if (!std::filesystem::exists(m_Root, ec))
        {
            std::filesystem::create_directories(m_Root, ec);
            FX_WARN("Content browser: assets klasoru yoktu, olusturuldu (%s)",
                    m_Root.string().c_str());
        }

        m_NeedsRefresh = true;

        // Izleyici KOKU izliyor, gezilen klasoru degil: baska bir klasorde
        // yapilan degisiklik de varlik tablosunu bozar, gorunmuyor olmasi
        // onemli degil.
        m_Watcher.Start(m_Root.string());
    }

    bool ContentBrowserPanel::TakeImportRequest()
    {
        const bool request = m_ImportRequest;
        m_ImportRequest = false;
        return request;
    }

    std::string ContentBrowserPanel::TakeOpenRequest()
    {
        if (m_OpenRequest.empty())
            return {};

        const std::string relative =
            FX::FileSystem::MakeRelativeToProject(m_OpenRequest.string());
        m_OpenRequest.clear();
        return relative;
    }

    void ContentBrowserPanel::ProcessFileChanges()
    {
        std::vector<FX::FileChange> changes = m_Watcher.Poll();
        if (changes.empty())
            return;

        // Tasima, ayni klasor icinde ad degisikligiyse Windows bize
        // Renamed veriyor; klasorler arasi tasimada bazen Removed + Added
        // ciftine bolunuyor. Ciftleri EN AZINDAN ayni yiginda esleyip
        // tasima sayiyoruz - yoksa dosya yeni bir GUID alir ve sahnedeki
        // referans kopardi. Izleyicinin var olma sebebi tam olarak bu.
        auto fileName = [](const std::string& p) {
            const auto slash = p.find_last_of('/');
            return slash == std::string::npos ? p : p.substr(slash + 1);
        };

        for (auto& removed : changes)
        {
            if (removed.Action != FX::FileAction::Removed)
                continue;

            for (auto& added : changes)
            {
                if (added.Action != FX::FileAction::Added ||
                    fileName(added.Path) != fileName(removed.Path))
                    continue;

                added.Action  = FX::FileAction::Renamed;
                added.OldPath = removed.Path;
                removed.Path.clear();     // isleme alinmasin
                removed.Action = FX::FileAction::Modified;
                break;
            }
        }

        bool tableChanged = false;

        for (const FX::FileChange& change : changes)
        {
            // Bos yol = izleyicinin tamponu tasti, ne degistigini bilmiyor.
            if (change.Path.empty() && change.OldPath.empty())
            {
                FX::AssetManager::ScanProject();
                tableChanged = true;
                continue;
            }

            // .meta dosyalarinin kendi olaylari ilgilendirmiyor: onlari
            // zaten varligin yaninda biz yonetiyoruz. Islersek varligi
            // silinmis sanip GUID'i atardik.
            if (FX::AssetManager::IsMetaFile(change.Path) ||
                FX::AssetManager::IsMetaFile(change.OldPath))
                continue;

            const std::string relative = change.Path.empty()
                                             ? std::string{}
                                             : m_AssetDir + "/" + change.Path;

            switch (change.Action)
            {
                case FX::FileAction::Added:
                    FX::AssetManager::Register(relative);
                    tableChanged = true;
                    break;

                case FX::FileAction::Removed:
                    FX::AssetManager::OnAssetDeleted(relative);
                    tableChanged = true;
                    break;

                case FX::FileAction::Renamed:
                    FX::AssetManager::OnAssetMoved(m_AssetDir + "/" + change.OldPath, relative);
                    tableChanged = true;
                    break;

                case FX::FileAction::Modified:
                    break;
            }
        }

        if (tableChanged)
            SetMessage("Klasor disaridan degisti, varlik tablosu guncellendi");

        Refresh();
    }

    void ContentBrowserPanel::RefreshIfNeeded()
    {
        if (!m_NeedsRefresh)
            return;

        m_NeedsRefresh = false;
        m_Directories.clear();
        m_Files.clear();

        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(m_Current, ec))
        {
            if (entry.is_directory(ec))
            {
                m_Directories.push_back(entry);
                continue;
            }

            // .meta dosyalari kullaniciya GOSTERILMIYOR: onlar varligin
            // kendisi degil, hakkindaki bilgi. Unity de gizler.
            // Yine de diskte duruyorlar ve surum kontrolune girmeliler.
            if (FX::AssetManager::IsMetaFile(entry.path().string()))
                continue;

            m_Files.push_back(entry);
        }

        // directory_iterator'in sirasi ISLETIM SISTEMINE BAGLIDIR ve NTFS
        // ile ext4'te farkli cikar; kendimiz siralamazsak panel makineden
        // makineye baska gorunur.
        auto byName = [](const auto& a, const auto& b) {
            return ToLower(a.path().filename().string()) <
                   ToLower(b.path().filename().string());
        };
        std::sort(m_Directories.begin(), m_Directories.end(), byName);
        std::sort(m_Files.begin(), m_Files.end(), byName);
    }

    std::string ContentBrowserPanel::ImportFile(const std::string& absoluteSource)
    {
        std::error_code ec;
        const std::filesystem::path source(absoluteSource);

        if (!std::filesystem::exists(source, ec) || std::filesystem::is_directory(source, ec))
        {
            FX_WARN("Ice aktarilamadi (dosya degil): %s", absoluteSource.c_str());
            return {};
        }

        std::filesystem::path target = m_Current / source.filename();

        // Ayni isim varsa UZERINE YAZMA, numaralandir. Kullanicinin
        // mevcut bir varligini sessizce yok etmek kabul edilemez;
        // bir sahne o dosyaya isaret ediyor olabilir.
        if (std::filesystem::exists(target, ec))
        {
            const std::string stem = source.stem().string();
            const std::string ext  = source.extension().string();

            for (int i = 1; i < 1000; ++i)
            {
                target = m_Current / (stem + " (" + std::to_string(i) + ")" + ext);
                if (!std::filesystem::exists(target, ec))
                    break;
            }
        }

        // KOPYALIYORUZ, tasimiyoruz: kaynak dosya kullanicinin, ona
        // dokunmak bizim isimiz degil.
        std::filesystem::copy_file(source, target, ec);
        if (ec)
        {
            FX_ERROR("Ice aktarilamadi: %s (%s)", absoluteSource.c_str(), ec.message().c_str());
            return {};
        }

        Refresh();

        const std::string relative = FX::FileSystem::MakeRelativeToProject(target.string());

        // Yeni varliga HEMEN kimlik ver: sahneye surukleyip birakildiginda
        // GUID'i hazir olsun, bir sonraki proje taramasini beklemesin.
        FX::AssetManager::Register(relative);

        FX_INFO("Ice aktarildi: %s", relative.c_str());
        return relative;
    }

    void ContentBrowserPanel::OnImGuiRender()
    {
        // Panel gizli olsa bile isleniyor: varlik tablosunun diskle
        // tutarliligi panelin gorunur olmasina bagli olamaz.
        ProcessFileChanges();

        // Begin'in donusunu kontrol ediyoruz: panel katlanmis/gizliyse
        // ImGui widget'lari zaten atlar ama BIZIM klasor okuma ve doku
        // yukleme isimiz yine de calisirdi.
        if (!ImGui::Begin("Icerik"))
        {
            ImGui::End();
            return;
        }

        if (m_Root.empty())
        {
            ImGui::TextDisabled("Baglam ayarlanmadi.");
            ImGui::End();
            return;
        }

        DrawToolbar();
        ImGui::Separator();

        RefreshIfNeeded();

        // Kac sutun sigar? Panel daraltilinca tek sutuna dusmeli;
        // sabit sutun sayisi dar panellerde tasar.
        const float cellSize = m_ThumbnailSize + m_Padding;
        const float width    = ImGui::GetContentRegionAvail().x;
        const int   columns  = std::max(1, static_cast<int>(width / cellSize));

        // Geri bildirim satiri IZGARADAN ONCE: izgara cocuk penceresi
        // kalan tum alani kapliyor, altina yazilan hicbir sey gorunmez.
        if (m_MessageTimer > 0.0f)
        {
            m_MessageTimer -= ImGui::GetIO().DeltaTime;
            ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.35f, 1.0f), "%s", m_Message.c_str());
            ImGui::Separator();
        }

        ImGui::BeginChild("IcerikIzgara", ImVec2(0.0f, 0.0f), false);

        const std::string search = ToLower(m_SearchBuffer);
        auto matches = [&search](const std::filesystem::directory_entry& e) {
            return search.empty() ||
                   ToLower(e.path().filename().string()).find(search) != std::string::npos;
        };

        // KLASORLER HER ZAMAN ONCE. Alfabetik siralamaya karistirsaydik
        // klasorler dosyalarin arasina dagilirdi; dosya yoneticilerinin
        // tamaminin klasorleri ayri gruplamasinin sebebi bu.
        if (m_ViewMode == ViewMode::Grid)
        {
            ImGui::Columns(columns, nullptr, false);

            int index = 0;
            for (const auto& e : m_Directories) { if (matches(e)) DrawEntry(e, index); ++index; }
            for (const auto& e : m_Files)       { if (matches(e)) DrawEntry(e, index); ++index; }

            ImGui::Columns(1);
        }
        else if (ImGui::BeginTable("IcerikListe", 3,
                                   ImGuiTableFlags_RowBg |
                                   ImGuiTableFlags_BordersInnerV |
                                   ImGuiTableFlags_Resizable |
                                   ImGuiTableFlags_ScrollY))
        {
            // Ad esneyip kalan alani aliyor; tur ve boyut sabit.
            // Tersi olsaydi uzun bir dosya adi kesilir, boyut sutunu
            // gereksiz yere genisleyerek bos dururdu.
            ImGui::TableSetupColumn("Ad",    ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Tur",   ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableSetupColumn("Boyut", ImGuiTableColumnFlags_WidthFixed, 80.0f);

            // Baslik satiri kaydirirken sabit kalsin.
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();

            int index = 0;
            for (const auto& e : m_Directories) { if (matches(e)) DrawEntry(e, index); ++index; }
            for (const auto& e : m_Files)       { if (matches(e)) DrawEntry(e, index); ++index; }

            ImGui::EndTable();
        }

        // Bos alana SOL tik secimi temizler - Explorer'da oldugu gibi.
        // IsWindowHovered + !IsAnyItemHovered: bir ogenin uzerinde
        // degilsek bos alandayiz demektir.
        if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            ClearSelection();
        }

        // Bos alana sag tik: klasor islemleri.
        if (ImGui::BeginPopupContextWindow("IcerikBosAlan",
                                           ImGuiPopupFlags_MouseButtonRight |
                                           ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::MenuItem("Yeni Klasor..."))
            {
                m_NameBuffer[0] = '\0';
                m_OpenNewFolderPopup = true;
            }
            if (ImGui::MenuItem("Dosya Ice Aktar..."))
                m_ImportRequest = true;
            ImGui::Separator();
            if (ImGui::MenuItem("Yenile"))
                Refresh();
            if (ImGui::MenuItem("Explorer'da Ac"))
                FileDialogs::RevealInFileManager(m_Current.string());
            ImGui::EndPopup();
        }

        ImGui::EndChild();

        // Tasima cerceve SONUNDA: rename cagrisi dizin listesini
        // gecersiz kilar, listeyi gezerken cagrilamaz.
        if (!m_MoveSources.empty() && !m_MoveTarget.empty())
        {
            const auto sources = m_MoveSources;
            const auto dst     = m_MoveTarget;
            m_MoveSources.clear();
            m_MoveTarget.clear();

            for (const auto& src : sources)
                MoveItem(src, dst);

            // MoveItem her oge icin kendi mesajini basiyor; cokluda
            // sadece sonuncusu kalirdi. Ozet daha faydali.
            if (sources.size() > 1)
                SetMessage(std::to_string(sources.size()) + " oge tasindi -> " +
                           dst.filename().string());

            ClearSelection();
        }

        DrawModals();

        ImGui::End();
    }

    void ContentBrowserPanel::DrawToolbar()
    {
        const bool atRoot = (m_Current == m_Root);

        ImGui::BeginDisabled(atRoot);
        if (ImGui::Button("<"))
        {
            m_Current = m_Current.parent_path();
            ClearSelection();
            Refresh();
        }
        ImGui::EndDisabled();

        // Ust klasore tasima: kokte degilken "<" bir birakma hedefi.
        // Bu olmadan bir dosyayi yukari cikarmanin yolu olmazdi -
        // yukarisi ekranda gorunmuyor.
        if (!atRoot)
            AcceptMoveTarget(m_Current.parent_path());

        if (ImGui::IsItemHovered() && !atRoot)
            ImGui::SetTooltip("Ust klasor (buraya birakarak tasiyabilirsin)");

        ImGui::SameLine();
        if (ImGui::Button("Yenile"))
            Refresh();

        ImGui::SameLine();
        if (ImGui::Button("Yeni Klasor"))
        {
            m_NameBuffer[0] = '\0';
            m_OpenNewFolderPopup = true;
        }

        ImGui::SameLine();
        if (ImGui::Button("Ice Aktar..."))
            m_ImportRequest = true;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Bilgisayardan dosya kopyala.\n"
                              "Dosyayi pencereye surukleyip birakmak da olur.");

        // Kirinti yolu (breadcrumb): her parca tiklanabilir.
        ImGui::SameLine();
        ImGui::TextDisabled("|");

        std::filesystem::path walk;
        std::filesystem::path jumpTo;

        for (const auto& part : std::filesystem::relative(m_Current, m_Root.parent_path()))
        {
            walk /= part;

            ImGui::SameLine();
            ImGui::PushID(walk.string().c_str());
            if (ImGui::SmallButton(part.string().c_str()))
                jumpTo = m_Root.parent_path() / walk;

            // Kirinti yolunun her parcasi da birakma hedefi: birkac
            // seviye yukari tasimak icin tek tek geri gitmek gerekmiyor.
            AcceptMoveTarget(m_Root.parent_path() / walk);

            ImGui::PopID();
        }

        // Dongu bittikten sonra: gezerken m_Current'i degistirmek
        // kalan parcalari yanlis hesaplatirdi.
        if (!jumpTo.empty() && jumpTo != m_Current)
        {
            m_Current = jumpTo;
            ClearSelection();
            Refresh();
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputTextWithHint("##Ara", "ara...", m_SearchBuffer, sizeof(m_SearchBuffer));

        // --- Gorunum secici -------------------------------------------------
        ImGui::SameLine();
        ImGui::TextDisabled("|");

        const bool isGrid = (m_ViewMode == ViewMode::Grid);

        ImGui::SameLine();
        if (isGrid)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button("Izgara"))
            m_ViewMode = ViewMode::Grid;
        if (isGrid)
            ImGui::PopStyleColor();
        ImGui::SetItemTooltip("Buyuk kucuk resimler");

        ImGui::SameLine();
        if (!isGrid)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button("Liste"))
            m_ViewMode = ViewMode::List;
        if (!isGrid)
            ImGui::PopStyleColor();
        ImGui::SetItemTooltip("Tek sutun: ad, tur, boyut");

        // Kucuk resim boyutu sadece izgarada anlamli; listede satir
        // yuksekligi metne gore sabit.
        if (isGrid)
        {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120.0f);
            ImGui::SliderFloat("##Boyut", &m_ThumbnailSize, 48.0f, 160.0f, "boyut %.0f");
        }
    }

    void ContentBrowserPanel::DrawEntry(const std::filesystem::directory_entry& entry,
                                        int index)
    {
        if (m_ViewMode == ViewMode::Grid) DrawEntryGrid(entry, index);
        else                              DrawEntryList(entry, index);
    }

    // -----------------------------------------------------------------------
    // Coklu secim
    // -----------------------------------------------------------------------
    bool ContentBrowserPanel::IsSelected(const std::filesystem::path& path) const
    {
        return std::find(m_Selected.begin(), m_Selected.end(), path) != m_Selected.end();
    }

    void ContentBrowserPanel::ClearSelection()
    {
        m_Selected.clear();
        m_LastClickedIndex = -1;
    }

    std::vector<std::filesystem::path> ContentBrowserPanel::VisibleOrder() const
    {
        // Cizim sirasiyla AYNI olmali: Shift araligi kullanicinin
        // ekranda gordugu siraya gore hesaplanmali, disk sirasina gore
        // degil.
        std::vector<std::filesystem::path> order;
        order.reserve(m_Directories.size() + m_Files.size());

        for (const auto& e : m_Directories) order.push_back(e.path());
        for (const auto& e : m_Files)       order.push_back(e.path());
        return order;
    }

    void ContentBrowserPanel::HandleSelectionClick(const std::filesystem::path& path,
                                                   int index)
    {
        const ImGuiIO& io = ImGui::GetIO();

        if (io.KeyShift && m_LastClickedIndex >= 0)
        {
            // Aralik secimi: son tiklanan ile bu ogenin arasindaki her sey.
            const auto order = VisibleOrder();

            int a = m_LastClickedIndex;
            int b = index;
            if (a > b) std::swap(a, b);

            m_Selected.clear();
            for (int i = a; i <= b && i < static_cast<int>(order.size()); ++i)
                m_Selected.push_back(order[static_cast<std::size_t>(i)]);

            // m_LastClickedIndex'i GUNCELLEMIYORUZ: Shift ile arka arkaya
            // tiklayarak araligi buyutup kucultmek isteniyor; capa
            // sabit kalmali.
            return;
        }

        if (io.KeyCtrl)
        {
            const auto it = std::find(m_Selected.begin(), m_Selected.end(), path);
            if (it != m_Selected.end()) m_Selected.erase(it);
            else                        m_Selected.push_back(path);

            m_LastClickedIndex = index;
            return;
        }

        // Duz tik: sadece bu oge.
        m_Selected.assign(1, path);
        m_LastClickedIndex = index;
    }

    std::vector<std::filesystem::path> ContentBrowserPanel::ItemsToDrag(
        const std::filesystem::path& dragged) const
    {
        // Suruklenen oge secimin PARCASIYSA tum secimi tasi; degilse
        // sadece onu. Explorer'in davranisi bu ve dogrusu da bu:
        // secimin disindaki bir seyi surukleyince kullanici o tek
        // ogeyi kastediyordur.
        if (IsSelected(dragged) && m_Selected.size() > 1)
            return m_Selected;

        return { dragged };
    }

    void ContentBrowserPanel::DrawEntryIcon(const std::filesystem::path& path,
                                            bool isDirectory,
                                            const ImVec2& min, const ImVec2& max)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        if (isDirectory)
        {
            DrawFolderIcon(drawList, min, max);
            return;
        }

        if (IsImage(path) && m_Library)
        {
            // Kucuk resim = dosyanin KENDISI. Ayri bir ikon setine gerek yok
            // ve tarayicida gordugun her resim zaten onbellege giriyor.
            const std::string relative = FX::FileSystem::MakeRelativeToProject(path.string());
            auto texture = m_Library->Load(relative);

            if (texture)
            {
                // Resmi kendi en-boy oraninda, hucreye ORTALAYARAK ciz.
                // Hucreyi doldursaydik dikdortgen resimler ezik gorunurdu.
                const float cell   = std::min(max.x - min.x, max.y - min.y);
                const float aspect = static_cast<float>(texture->GetWidth()) /
                                     static_cast<float>(texture->GetHeight());
                const float box    = cell * 0.84f;

                const float drawW = (aspect >= 1.0f) ? box : box * aspect;
                const float drawH = (aspect >= 1.0f) ? box / aspect : box;

                const ImVec2 center{ (min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f };

                drawList->AddImage(static_cast<ImTextureID>(texture->GetRendererID()),
                                   ImVec2(center.x - drawW * 0.5f, center.y - drawH * 0.5f),
                                   ImVec2(center.x + drawW * 0.5f, center.y + drawH * 0.5f),
                                   ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
                return;
            }

            DrawFileIcon(drawList, min, max, IM_COL32(180, 60, 60, 255), "?");
            return;
        }

        std::string ext = path.extension().string();
        if (!ext.empty())
            ext.erase(ext.begin());   // bastaki noktayi at

        DrawFileIcon(drawList, min, max, AccentColor(path), ToLower(ext));
    }

    void ContentBrowserPanel::DrawEntryInteractions(const std::filesystem::path& path,
                                                    const std::string& filename,
                                                    bool isDirectory, bool clicked,
                                                    int index)
    {
        // Cift tik iki gorunumde iki farkli sinyalle geliyor:
        //
        // Izgaradaki Button bastirmayi BIRAKMA karesinde bildiriyor,
        // IsMouseDoubleClicked ise sadece ikinci BASMA karesinde true;
        // ikisi ayni karede bulusmadigi icin izgarada cift tik hic
        // algilanmiyordu - orada IsItemHovered'a bakmak gerekiyor.
        // Listedeki Selectable ise basma karesinde donuyor ama tablo
        // satirinda IsItemHovered guvenilmez, dolayisiyla orada donus
        // degeri dogru sinyal. Ikisini de kabul ediyoruz.
        const bool doubleClicked =
            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
            (clicked || ImGui::IsItemHovered());

        // Surukleme kaynagi: KLASORLER DAHIL her sey.
        //
        // Klasorler de suruklenebiliyor cunku tasima ozelligi onlari da
        // kapsiyor. Viewport'a birakilan bir klasor zaten hicbir sey
        // yapmaz - HandleContentDrop uzantiya bakiyor ve klasorun
        // uzantisi yok.
        if (ImGui::BeginDragDropSource())
        {
            const std::string relative = FX::FileSystem::MakeRelativeToProject(path.string());

            // Yuk hala TEK yol tasiyor: viewport'a birakma (Faz 12) bu
            // bicimi bekliyor ve coklu sprite olusturma ayri bir is.
            // Coklu tasimada hangi ogelerin gidecegini ItemsToDrag
            // belirliyor - yuke bakmiyoruz, secime bakiyoruz.
            //
            // Sonlandirici DAHIL gonderiyoruz: alan tarafi payload'i
            // dogrudan const char* olarak okuyor, olmazsa tasar.
            ImGui::SetDragDropPayload(kContentPayload, relative.c_str(), relative.size() + 1);

            const auto items = ItemsToDrag(path);
            if (items.size() > 1)
                ImGui::Text("%d oge", static_cast<int>(items.size()));
            else
                ImGui::Text("%s%s", isDirectory ? "[klasor] " : "", filename.c_str());

            ImGui::EndDragDropSource();
        }

        // Birakma hedefi: sadece klasorler. Bir dosyanin uzerine birakmak
        // anlamsiz - "icine" koyacak bir yeri yok.
        if (isDirectory)
            AcceptMoveTarget(path);

        // Sag tik secimin DISINDA bir ogeye yapildiysa secimi ona
        // ayarliyoruz: menudeki islem gorunmeyen bir secime uygulanmasin.
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right) &&
            !IsSelected(path))
        {
            m_Selected.assign(1, path);
            m_LastClickedIndex = index;
        }

        if (ImGui::BeginPopupContextItem("OgeMenu"))
        {
            const int selCount = static_cast<int>(m_Selected.size());

            if (selCount > 1)
                ImGui::TextDisabled("%d oge secili", selCount);

            if (selCount == 1 && ImGui::MenuItem("Yeniden Adlandir..."))
            {
                std::strncpy(m_NameBuffer, filename.c_str(), sizeof(m_NameBuffer) - 1);
                m_NameBuffer[sizeof(m_NameBuffer) - 1] = 0;
                m_RenameTarget = path;
            }
            if (ImGui::MenuItem(selCount > 1 ? "Secilenleri Sil..." : "Sil..."))
                m_DeleteTarget = path;
            ImGui::Separator();
            if (ImGui::MenuItem("Explorer'da Goster"))
                FileDialogs::RevealInFileManager(path.string());
            if (ImGui::MenuItem("Yolu Kopyala"))
                ImGui::SetClipboardText(
                    FX::FileSystem::MakeRelativeToProject(path.string()).c_str());
            ImGui::EndPopup();
        }

        // CIFT tik klasore girer / dosyayi acar, TEK tik secer.
        //
        // Tek tikla girmeye devam etseydik secim ile gezinme ayni
        // eylemi paylasirdi: bir klasoru secmek imkansiz olurdu ve
        // coklu secim klasorleri disarida birakirdi.
        if (doubleClicked)
        {
            if (isDirectory)
            {
                // Klasore girme dongunun SONUNDA degil burada guvenli:
                // m_Current degisse bile bu karede listeyi bir daha
                // gezmiyoruz, Refresh bayragi bir sonraki karede okunuyor.
                m_Current = path;
                ClearSelection();
                Refresh();
            }
            else
            {
                // Acma isini EditorApp yapiyor: sahne yuklemek, prefab
                // orneklemek ve isletim sistemine devretmek panelin
                // bilmesi gereken seyler degil.
                m_OpenRequest = path;
            }
            return;
        }

        if (clicked)
            HandleSelectionClick(path, index);
    }

    void ContentBrowserPanel::DrawEntryGrid(const std::filesystem::directory_entry& entry,
                                            int index)
    {
        const auto& path = entry.path();
        const std::string filename = path.filename().string();
        const bool isDirectory = entry.is_directory();

        // Ayni isimli dosyalar farkli klasorlerde olabilir; ImGui ID
        // yiginina tam yolu koymak cakismayi tumden onler.
        ImGui::PushID(path.string().c_str());

        const ImVec2 size(m_ThumbnailSize, m_ThumbnailSize);

        // Dugmeyi SEFFAF cizip uzerine kendi ikonumuzu koyuyoruz.
        // Boylece vurgu/tiklama gorselleri ImGui'den bedava geliyor
        // ama gorunum bize ait.
        const bool selected = IsSelected(path);

        ImGui::PushStyleColor(ImGuiCol_Button,
                              selected ? ImVec4(0.26f, 0.45f, 0.72f, 0.55f)
                                       : ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.10f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1.0f, 1.0f, 1.0f, 0.18f));
        const bool clicked = ImGui::Button("##oge", size);
        ImGui::PopStyleColor(3);

        const ImVec2 cellMin = ImGui::GetItemRectMin();
        const ImVec2 cellMax = ImGui::GetItemRectMax();

        // Secili ogeye cerceve: dolgu rengi kucuk resmin arkasinda
        // kaliyor, kenarlik ise her zaman gorunur.
        if (selected)
        {
            ImGui::GetWindowDrawList()->AddRect(cellMin, cellMax,
                                                IM_COL32(120, 170, 255, 220), 3.0f, 0, 2.0f);
        }

        DrawEntryIcon(path, isDirectory, cellMin, cellMax);

        DrawEntryInteractions(path, filename, isDirectory, clicked, index);

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", filename.c_str());

        // Isim, hucre genisligini asmayacak sekilde kisaltiliyor.
        ImGui::TextUnformatted(ShortenName(filename, m_ThumbnailSize).c_str());

        ImGui::NextColumn();
        ImGui::PopID();
    }

    void ContentBrowserPanel::DrawEntryList(const std::filesystem::directory_entry& entry,
                                            int index)
    {
        const auto& path = entry.path();
        const std::string filename = path.filename().string();
        const bool isDirectory = entry.is_directory();

        ImGui::PushID(path.string().c_str());
        ImGui::TableNextRow();

        // --- Ad sutunu ------------------------------------------------------
        ImGui::TableNextColumn();

        const float rowHeight = ImGui::GetTextLineHeight() + 6.0f;

        // Selectable TUM SATIRI kapliyor: tiklama alani genis olsun,
        // kullanici ince bir metin seridini avlamak zorunda kalmasin.
        const bool clicked = ImGui::Selectable("##satir", IsSelected(path),
                                               ImGuiSelectableFlags_SpanAllColumns |
                                               ImGuiSelectableFlags_AllowDoubleClick,
                                               ImVec2(0.0f, rowHeight));

        const ImVec2 rowMin = ImGui::GetItemRectMin();

        // Ikon satirin solunda, kare bir alanda.
        const ImVec2 iconMin{ rowMin.x + 2.0f, rowMin.y + 1.0f };
        const ImVec2 iconMax{ iconMin.x + rowHeight - 2.0f, rowMin.y + rowHeight - 1.0f };
        DrawEntryIcon(path, isDirectory, iconMin, iconMax);

        // Ad, ikonun sagina. Listede KISALTMIYORUZ: liste gorunumunun
        // varlik sebebi zaten uzun adlari tam gorebilmek.
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(iconMax.x + 8.0f, rowMin.y + 3.0f),
            ImGui::GetColorU32(ImGuiCol_Text), filename.c_str());

        DrawEntryInteractions(path, filename, isDirectory, clicked, index);

        // --- Tur sutunu -----------------------------------------------------
        ImGui::TableNextColumn();
        if (isDirectory)
        {
            ImGui::TextDisabled("klasor");
        }
        else
        {
            std::string ext = path.extension().string();
            if (!ext.empty())
                ext.erase(ext.begin());
            ImGui::TextUnformatted(ext.empty() ? "-" : ToLower(ext).c_str());
        }

        // --- Boyut sutunu ---------------------------------------------------
        ImGui::TableNextColumn();
        if (isDirectory)
        {
            ImGui::TextDisabled("-");
        }
        else
        {
            std::error_code ec;
            const auto bytes = std::filesystem::file_size(path, ec);
            if (ec)
                ImGui::TextDisabled("?");
            else if (bytes < 1024)
                ImGui::Text("%llu B", static_cast<unsigned long long>(bytes));
            else if (bytes < 1024 * 1024)
                ImGui::Text("%.1f KB", static_cast<double>(bytes) / 1024.0);
            else
                ImGui::Text("%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
        }

        ImGui::PopID();
    }

    void ContentBrowserPanel::MoveMetaAlongside(const std::filesystem::path& oldPath,
                                                const std::filesystem::path& newPath)
    {
        std::error_code ec;

        // Klasorlerin .meta'si yok; iclerindeki dosyalarinki zaten
        // klasorle birlikte tasindi.
        if (std::filesystem::is_directory(newPath, ec))
            return;

        const std::filesystem::path oldMeta = oldPath.string() + ".meta";
        if (!std::filesystem::exists(oldMeta, ec))
            return;

        const std::filesystem::path newMeta = newPath.string() + ".meta";
        std::filesystem::rename(oldMeta, newMeta, ec);

        if (ec)
            FX_WARN("Meta tasinamadi: %s (%s)",
                    oldMeta.string().c_str(), ec.message().c_str());
    }

    void ContentBrowserPanel::SetMessage(const std::string& text)
    {
        m_Message      = text;
        m_MessageTimer = 6.0f;
    }

    void ContentBrowserPanel::AcceptMoveTarget(const std::filesystem::path& targetDir)
    {
        if (!ImGui::BeginDragDropTarget())
            return;

        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kContentPayload))
        {
            // Yuk GORECELI yol tasiyor (viewport'a birakma da ayni yuku
            // kullaniyor); tasima icin mutlak yola cevirmemiz gerekiyor.
            const std::string relative(static_cast<const char*>(payload->Data));

            // Tasimayi BURADA yapmiyoruz: dizin listesini gezerken
            // rename cagirmak yineleyicileri bozar. Istegi biriktirip
            // cerceve sonunda isliyoruz - Faz 9'daki kuralin aynisi.
            const std::filesystem::path dragged(
                FX::FileSystem::ResolveProjectAsset(relative));

            m_MoveSources = ItemsToDrag(dragged);
            m_MoveTarget  = targetDir;
        }

        ImGui::EndDragDropTarget();
    }

    void ContentBrowserPanel::MoveItem(const std::filesystem::path& source,
                                       const std::filesystem::path& targetDir)
    {
        std::error_code ec;

        // weakly_canonical: "..\" parcalarini duzler. Duzlemezsek
        // "a/b/../b" ile "a/b" farkli gorunur ve asagidaki
        // karsilastirmalar yanlis sonuc verir.
        const auto src = std::filesystem::weakly_canonical(source, ec);
        const auto dst = std::filesystem::weakly_canonical(targetDir, ec);

        if (ec || src.empty() || dst.empty())
        {
            SetMessage("Yol cozulemedi.");
            return;
        }

        // Zaten o klasorde: sessizce vazgec. Kullanici bir seyi ayni
        // yere birakti, bu bir hata degil.
        if (src.parent_path() == dst)
            return;

        // Bir klasoru kendi icine (veya kendi alt agacina) tasimak
        // dosya sistemini bozar. Isletim sistemi bunu genelde
        // reddeder ama davranis platforma gore degisir; kendimiz
        // engelliyoruz.
        if (std::filesystem::is_directory(src, ec))
        {
            for (auto p = dst; !p.empty(); p = p.parent_path())
            {
                if (p == src)
                {
                    SetMessage("Bir klasor kendi icine tasinamaz.");
                    return;
                }
                if (p == p.parent_path())
                    break;   // koke ulasildi
            }
        }

        const auto target = dst / src.filename();

        // UZERINE YAZMIYORUZ. Ice aktarmada (Faz 12) ayni durumda
        // numaralandiriyoruz cunku orada dosya disaridan geliyor ve adi
        // kullanici icin onemli degil. Tasimada ise ad onemli: sessizce
        // "x (1).png" uretmek kafa karistirir. Reddedip soyluyoruz.
        if (std::filesystem::exists(target, ec))
        {
            SetMessage("Hedefte ayni isimde bir oge zaten var: " +
                       src.filename().string());
            return;
        }

        // Goreceli yollari rename'den ONCE hesapliyoruz: sonrasinda
        // kaynak dosya artik yerinde olmaz.
        const std::string oldRel = FX::FileSystem::MakeRelativeToProject(src.string());
        const std::string newRel = FX::FileSystem::MakeRelativeToProject(target.string());

        std::filesystem::rename(src, target, ec);
        if (ec)
        {
            SetMessage("Tasinamadi: " + ec.message());
            FX_ERROR("Tasima basarisiz: %s -> %s (%s)",
                     src.string().c_str(), target.string().c_str(), ec.message().c_str());
            return;
        }

        // .meta dosyasi varligin YANINDA yasar ve onunla birlikte gider.
        // Gitmeseydi bir sonraki taramada varliga yeni bir GUID
        // atanirdi ve tum referanslar kopardi - .meta'nin varlik
        // sebebini yok ederdik.
        MoveMetaAlongside(src, target);

        // Kimlik korunuyor, sadece yol degisiyor. Sahne dosyalari GUID
        // sakladigi icin referanslar SAGLAM KALIYOR.
        FX::AssetManager::OnAssetMoved(oldRel, newRel);

        SetMessage(src.filename().string() + " tasindi -> " + dst.filename().string());

        FX_INFO("Tasindi: %s -> %s", src.string().c_str(), target.string().c_str());
        Refresh();
    }

    void ContentBrowserPanel::DrawModals()
    {
        // ImGui modallari, acilma istegiyle AYNI ID kapsaminda
        // OpenPopup edilmek zorunda. Bu yuzden istekleri bayrak olarak
        // biriktirip hepsini burada, tek yerde aciyoruz.
        if (m_OpenNewFolderPopup)
        {
            m_OpenNewFolderPopup = false;
            ImGui::OpenPopup("Yeni Klasor");
        }
        if (!m_RenameTarget.empty() && !ImGui::IsPopupOpen("Yeniden Adlandir"))
            ImGui::OpenPopup("Yeniden Adlandir");
        if (!m_DeleteTarget.empty() && !ImGui::IsPopupOpen("Sil"))
            ImGui::OpenPopup("Sil");

        if (ImGui::BeginPopupModal("Yeni Klasor", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Klasor adi:");
            ImGui::SetNextItemWidth(280.0f);
            const bool submitted = ImGui::InputText("##ad", m_NameBuffer, sizeof(m_NameBuffer),
                                                    ImGuiInputTextFlags_EnterReturnsTrue);

            if (ImGui::Button("Olustur", ImVec2(120.0f, 0.0f)) || submitted)
            {
                if (m_NameBuffer[0] != '\0')
                {
                    std::error_code ec;
                    std::filesystem::create_directory(m_Current / m_NameBuffer, ec);
                    if (ec)
                        FX_ERROR("Klasor olusturulamadi: %s", ec.message().c_str());
                    Refresh();
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Iptal", ImVec2(120.0f, 0.0f)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("Yeniden Adlandir", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextDisabled("%s", m_RenameTarget.filename().string().c_str());

            // ESKIDEN buraya "bu dosyayi kullanan sahneler onu bulamayacak"
            // uyarisi basiyorduk: kimlik dosya yoluydu ve adi degistirmek
            // tum referanslari koparirdi.
            //
            // AssetManager ile kimlik .meta'daki GUID'e tasindi; ad ve
            // konum artik sadece birer etiket. Uyari gecersiz kaldigi
            // icin KALDIRILDI - dogru olmayan bir uyari, uyari olmamasindan
            // daha kotudur.
            ImGui::TextDisabled("Referanslar korunur (kimlik .meta'da).");

            ImGui::SetNextItemWidth(280.0f);
            const bool submitted = ImGui::InputText("##yeniad", m_NameBuffer, sizeof(m_NameBuffer),
                                                    ImGuiInputTextFlags_EnterReturnsTrue);

            if (ImGui::Button("Degistir", ImVec2(120.0f, 0.0f)) || submitted)
            {
                if (m_NameBuffer[0] != '\0')
                {
                    std::error_code ec;
                    const std::filesystem::path renamed =
                        m_RenameTarget.parent_path() / m_NameBuffer;

                    const std::string oldRel =
                        FX::FileSystem::MakeRelativeToProject(m_RenameTarget.string());
                    const std::string newRel =
                        FX::FileSystem::MakeRelativeToProject(renamed.string());

                    std::filesystem::rename(m_RenameTarget, renamed, ec);

                    if (!ec)
                    {
                        MoveMetaAlongside(m_RenameTarget, renamed);
                        FX::AssetManager::OnAssetMoved(oldRel, newRel);
                    }
                    if (ec)
                        FX_ERROR("Yeniden adlandirilamadi: %s", ec.message().c_str());
                    Refresh();
                }
                m_RenameTarget.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Iptal", ImVec2(120.0f, 0.0f)))
            {
                m_RenameTarget.clear();
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("Sil", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            // Secim varsa TUM secim silinir; yoksa sag tiklanan oge.
            // Menude "Secilenleri Sil..." yazip tek dosya silmek
            // kullaniciyi yaniltirdi.
            std::vector<std::filesystem::path> targets =
                IsSelected(m_DeleteTarget) && m_Selected.size() > 1
                    ? m_Selected
                    : std::vector<std::filesystem::path>{ m_DeleteTarget };

            bool anyDirectory = false;
            for (const auto& t : targets)
            {
                std::error_code dirEc;
                if (std::filesystem::is_directory(t, dirEc))
                    anyDirectory = true;
            }

            if (targets.size() > 1)
            {
                ImGui::Text("%d oge", static_cast<int>(targets.size()));

                // Ilk birkacini SAYMAKLA kalmiyor, gosteriyoruz: "12 oge
                // silinecek" diyen bir onay kutusu neyin gittigini
                // gizler.
                ImGui::Indent();
                for (std::size_t i = 0; i < targets.size() && i < 6; ++i)
                    ImGui::TextDisabled("%s", targets[i].filename().string().c_str());
                if (targets.size() > 6)
                    ImGui::TextDisabled("... ve %d tane daha",
                                        static_cast<int>(targets.size() - 6));
                ImGui::Unindent();
            }
            else
            {
                ImGui::Text("%s", m_DeleteTarget.filename().string().c_str());
            }

            ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.45f, 1.0f),
                               anyDirectory ? "Klasorler ve TUM icerikleri silinecek."
                                            : "Kalici olarak silinecek.");
            ImGui::TextDisabled("Geri alinamaz.");

            if (ImGui::Button("Sil", ImVec2(120.0f, 0.0f)))
            {
                int removed = 0;
                for (const auto& t : targets)
                {
                    std::error_code ec;
                    const bool isDir = std::filesystem::is_directory(t, ec);
                    const std::string rel = FX::FileSystem::MakeRelativeToProject(t.string());

                    if (isDir)
                    {
                        std::filesystem::remove_all(t, ec);
                    }
                    else
                    {
                        std::filesystem::remove(t, ec);

                        // Varligin .meta'si de gitmeli, yoksa sahipsiz
                        // bir .meta kalir ve bir sonraki taramada
                        // "dosyasi olmayan kayit" olusurdu.
                        std::error_code metaEc;
                        std::filesystem::remove(t.string() + ".meta", metaEc);

                        if (!ec)
                            FX::AssetManager::OnAssetDeleted(rel);
                    }

                    if (ec)
                        FX_ERROR("Silinemedi (%s): %s",
                                 t.filename().string().c_str(), ec.message().c_str());
                    else
                        ++removed;
                }

                FX_INFO("Silindi: %d oge", removed);
                if (targets.size() > 1)
                    SetMessage(std::to_string(removed) + " oge silindi.");

                m_DeleteTarget.clear();
                ClearSelection();
                Refresh();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Iptal", ImVec2(120.0f, 0.0f)))
            {
                m_DeleteTarget.clear();
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
}
