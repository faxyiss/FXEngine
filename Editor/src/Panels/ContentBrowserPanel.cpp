#include "Panels/ContentBrowserPanel.h"
#include "Platform/FileDialogs.h"

#include <FXEngine/Core/FileSystem.h>
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

        m_Root    = std::filesystem::path(FX::FileSystem::ResolveProjectAsset("assets"));
        m_Current = m_Root;

        std::error_code ec;
        if (!std::filesystem::exists(m_Root, ec))
        {
            std::filesystem::create_directories(m_Root, ec);
            FX_WARN("Content browser: assets klasoru yoktu, olusturuldu (%s)",
                    m_Root.string().c_str());
        }

        m_NeedsRefresh = true;
    }

    bool ContentBrowserPanel::TakeImportRequest()
    {
        const bool request = m_ImportRequest;
        m_ImportRequest = false;
        return request;
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
            if (entry.is_directory(ec)) m_Directories.push_back(entry);
            else                        m_Files.push_back(entry);
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
        FX_INFO("Ice aktarildi: %s", relative.c_str());
        return relative;
    }

    void ContentBrowserPanel::OnImGuiRender()
    {
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

        ImGui::Columns(columns, nullptr, false);

        const std::string search = ToLower(m_SearchBuffer);
        auto matches = [&search](const std::filesystem::directory_entry& e) {
            return search.empty() ||
                   ToLower(e.path().filename().string()).find(search) != std::string::npos;
        };

        for (const auto& e : m_Directories) if (matches(e)) DrawEntry(e);
        for (const auto& e : m_Files)       if (matches(e)) DrawEntry(e);

        ImGui::Columns(1);

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
        if (!m_MoveSource.empty() && !m_MoveTarget.empty())
        {
            const auto src = m_MoveSource;
            const auto dst = m_MoveTarget;
            m_MoveSource.clear();
            m_MoveTarget.clear();
            MoveItem(src, dst);
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
            Refresh();
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputTextWithHint("##Ara", "ara...", m_SearchBuffer, sizeof(m_SearchBuffer));

        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::SliderFloat("##Boyut", &m_ThumbnailSize, 48.0f, 160.0f, "boyut %.0f");
    }

    void ContentBrowserPanel::DrawEntry(const std::filesystem::directory_entry& entry)
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
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.10f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1.0f, 1.0f, 1.0f, 0.18f));
        const bool clicked = ImGui::Button("##oge", size);
        ImGui::PopStyleColor(3);

        const ImVec2 itemMin = ImGui::GetItemRectMin();
        const ImVec2 itemMax = ImGui::GetItemRectMax();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        if (isDirectory)
        {
            DrawFolderIcon(drawList, itemMin, itemMax);
        }
        else if (IsImage(path) && m_Library)
        {
            // Kucuk resim = dosyanin KENDISI. Ayri bir ikon setine gerek yok
            // ve tarayicida gordugun her resim zaten onbellege giriyor.
            const std::string relative = FX::FileSystem::MakeRelativeToProject(path.string());
            auto texture = m_Library->Load(relative);

            if (texture)
            {
                // Resmi kendi en-boy oraninda, hucreye ORTALAYARAK ciz.
                // Hucreyi doldursaydik dikdortgen resimler ezik gorunurdu.
                const float aspect = static_cast<float>(texture->GetWidth()) /
                                     static_cast<float>(texture->GetHeight());
                const float inset  = m_ThumbnailSize * 0.08f;
                const float box    = m_ThumbnailSize - inset * 2.0f;

                const float drawW = (aspect >= 1.0f) ? box : box * aspect;
                const float drawH = (aspect >= 1.0f) ? box / aspect : box;

                const ImVec2 center{ (itemMin.x + itemMax.x) * 0.5f,
                                     (itemMin.y + itemMax.y) * 0.5f };

                drawList->AddImage(static_cast<ImTextureID>(texture->GetRendererID()),
                                   ImVec2(center.x - drawW * 0.5f, center.y - drawH * 0.5f),
                                   ImVec2(center.x + drawW * 0.5f, center.y + drawH * 0.5f),
                                   ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
            }
            else
            {
                DrawFileIcon(drawList, itemMin, itemMax, IM_COL32(180, 60, 60, 255), "?");
            }
        }
        else
        {
            std::string ext = path.extension().string();
            if (!ext.empty())
                ext.erase(ext.begin());   // bastaki noktayi at

            DrawFileIcon(drawList, itemMin, itemMax, AccentColor(path), ToLower(ext));
        }

        // Surukleme kaynagi: KLASORLER DAHIL her sey.
        //
        // Klasorler de suruklenebiliyor cunku tasima ozelligi (asagida)
        // onlari da kapsiyor. Viewport'a birakilan bir klasor zaten
        // hicbir sey yapmaz - HandleContentDrop uzantiya bakiyor ve
        // klasorun uzantisi yok.
        if (ImGui::BeginDragDropSource())
        {
            const std::string relative = FX::FileSystem::MakeRelativeToProject(path.string());

            // '\0' DAHIL gonderiyoruz: alan tarafi payload'i dogrudan
            // const char* olarak okuyor, sonlandirici olmazsa tasar.
            ImGui::SetDragDropPayload(kContentPayload, relative.c_str(), relative.size() + 1);
            ImGui::Text("%s%s", isDirectory ? "[klasor] " : "", filename.c_str());
            ImGui::EndDragDropSource();
        }

        // Birakma hedefi: sadece klasorler. Bir dosyanin uzerine birakmak
        // anlamsiz - "icine" koyacak bir yeri yok.
        if (isDirectory)
            AcceptMoveTarget(path);

        if (ImGui::BeginPopupContextItem("OgeMenu"))
        {
            if (ImGui::MenuItem("Yeniden Adlandir..."))
            {
                std::strncpy(m_NameBuffer, filename.c_str(), sizeof(m_NameBuffer) - 1);
                m_NameBuffer[sizeof(m_NameBuffer) - 1] = '\0';
                m_RenameTarget = path;
            }
            if (ImGui::MenuItem("Sil..."))
                m_DeleteTarget = path;
            ImGui::Separator();
            if (ImGui::MenuItem("Explorer'da Goster"))
                FileDialogs::RevealInFileManager(path.string());
            if (ImGui::MenuItem("Yolu Kopyala"))
                ImGui::SetClipboardText(
                    FX::FileSystem::MakeRelativeToProject(path.string()).c_str());
            ImGui::EndPopup();
        }

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", filename.c_str());

        // Klasore girme dongunun SONUNDA degil burada guvenli: m_Current
        // degisse bile bu karede listeyi bir daha gezmiyoruz, Refresh
        // bayragi bir sonraki karede okunuyor.
        if (clicked && isDirectory)
        {
            m_Current = path;
            Refresh();
        }

        // Isim, hucre genisligini asmayacak sekilde kisaltiliyor.
        ImGui::TextUnformatted(ShortenName(filename, m_ThumbnailSize).c_str());

        ImGui::NextColumn();
        ImGui::PopID();
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
            m_MoveSource = std::filesystem::path(
                FX::FileSystem::ResolveProjectAsset(relative));
            m_MoveTarget = targetDir;
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

        std::filesystem::rename(src, target, ec);
        if (ec)
        {
            SetMessage("Tasinamadi: " + ec.message());
            FX_ERROR("Tasima basarisiz: %s -> %s (%s)",
                     src.string().c_str(), target.string().c_str(), ec.message().c_str());
            return;
        }

        // Varlik kimligi hala DOSYA YOLU (bkz. teknik borc): tasinan
        // dosyaya isaret eden sahneler onu bulamayacak. Yeniden
        // adlandirmada bunu modal ile soyluyoruz; burada surukleme
        // akisini modal ile kesmek yerine mesaj olarak veriyoruz.
        SetMessage(src.filename().string() + " tasindi -> " +
                   dst.filename().string() +
                   "  (bu ogeyi kullanan sahneler onu bulamayacak)");

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

            // Yeniden adlandirmak, yolu kimlik olarak kullandigimiz icin
            // o dosyaya isaret eden TUM referanslari koparir. Sessiz
            // kalmak yerine uyariyoruz.
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.35f, 1.0f),
                               "Bu dosyayi kullanan sahneler onu bulamayacak.");

            ImGui::SetNextItemWidth(280.0f);
            const bool submitted = ImGui::InputText("##yeniad", m_NameBuffer, sizeof(m_NameBuffer),
                                                    ImGuiInputTextFlags_EnterReturnsTrue);

            if (ImGui::Button("Degistir", ImVec2(120.0f, 0.0f)) || submitted)
            {
                if (m_NameBuffer[0] != '\0')
                {
                    std::error_code ec;
                    std::filesystem::rename(m_RenameTarget,
                                            m_RenameTarget.parent_path() / m_NameBuffer, ec);
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
            const bool isDirectory = std::filesystem::is_directory(m_DeleteTarget);

            ImGui::Text("%s", m_DeleteTarget.filename().string().c_str());
            ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.45f, 1.0f),
                               isDirectory ? "Klasor ve TUM icerigi silinecek."
                                           : "Dosya kalici olarak silinecek.");
            ImGui::TextDisabled("Geri alinamaz.");

            if (ImGui::Button("Sil", ImVec2(120.0f, 0.0f)))
            {
                std::error_code ec;
                if (isDirectory) std::filesystem::remove_all(m_DeleteTarget, ec);
                else             std::filesystem::remove(m_DeleteTarget, ec);

                if (ec)
                    FX_ERROR("Silinemedi: %s", ec.message().c_str());
                else
                    FX_INFO("Silindi: %s", m_DeleteTarget.filename().string().c_str());

                m_DeleteTarget.clear();
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
