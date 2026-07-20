#include "Panels/ContentBrowserPanel.h"

#include <FXEngine/Core/FileSystem.h>
#include <FXEngine/Core/Log.h>

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

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

        ImVec4 ColorForExtension(const std::filesystem::path& p)
        {
            const std::string ext = ToLower(p.extension().string());
            if (ext == ".fxscene")  return { 0.25f, 0.45f, 0.70f, 1.0f };
            if (ext == ".fxprefab") return { 0.55f, 0.35f, 0.65f, 1.0f };
            if (ext == ".vert" || ext == ".frag") return { 0.30f, 0.50f, 0.35f, 1.0f };
            return { 0.28f, 0.28f, 0.32f, 1.0f };
        }
    }

    void ContentBrowserPanel::SetContext(FX::TextureLibrary* library)
    {
        m_Library = library;

        m_Root    = std::filesystem::path(FX::FileSystem::ResolveAsset("assets"));
        m_Current = m_Root;

        std::error_code ec;
        if (!std::filesystem::exists(m_Root, ec))
            FX_WARN("Content browser: assets klasoru bulunamadi (%s)",
                    m_Root.string().c_str());
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

        DrawBreadcrumbs();
        ImGui::Separator();

        // Klasorleri once, sonra dosyalari; her grup alfabetik.
        // directory_iterator'in sirasi isletim sistemine BAGLIDIR ve
        // NTFS ile ext4'te farkli cikar; kendimiz siralamazsak panel
        // makineden makineye baska gorunur.
        std::vector<std::filesystem::directory_entry> dirs, files;
        {
            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator(m_Current, ec))
            {
                if (entry.is_directory(ec)) dirs.push_back(entry);
                else                        files.push_back(entry);
            }
            if (ec)
                ImGui::TextDisabled("Klasor okunamadi.");
        }

        auto byName = [](const auto& a, const auto& b) {
            return ToLower(a.path().filename().string()) < ToLower(b.path().filename().string());
        };
        std::sort(dirs.begin(), dirs.end(), byName);
        std::sort(files.begin(), files.end(), byName);

        // Kac sutun sigar? Panel daraltilinca tek sutuna dusmeli;
        // sabit sutun sayisi dar panellerde tasar.
        const float cellSize = m_ThumbnailSize + m_Padding;
        const float width    = ImGui::GetContentRegionAvail().x;
        const int   columns  = std::max(1, static_cast<int>(width / cellSize));

        ImGui::Columns(columns, nullptr, false);

        for (const auto& e : dirs)  DrawEntry(e);
        for (const auto& e : files) DrawEntry(e);

        ImGui::Columns(1);

        ImGui::Separator();
        ImGui::SetNextItemWidth(160.0f);
        ImGui::SliderFloat("Boyut", &m_ThumbnailSize, 48.0f, 160.0f, "%.0f");

        ImGui::End();
    }

    void ContentBrowserPanel::DrawBreadcrumbs()
    {
        const bool atRoot = (m_Current == m_Root);

        ImGui::BeginDisabled(atRoot);
        if (ImGui::Button("< Geri"))
            m_Current = m_Current.parent_path();
        ImGui::EndDisabled();

        ImGui::SameLine();

        // Kok her zaman "assets"; kullaniciya exe'nin tam yolunu
        // gostermenin bir faydasi yok.
        std::error_code ec;
        const auto rel = std::filesystem::relative(m_Current, m_Root.parent_path(), ec);
        ImGui::TextDisabled("%s", ec ? "assets" : rel.generic_string().c_str());
    }

    void ContentBrowserPanel::DrawEntry(const std::filesystem::directory_entry& entry)
    {
        const auto& path = entry.path();
        const std::string filename = path.filename().string();

        // Ayni isimli dosyalar farkli klasorlerde olabilir; ImGui ID
        // yigininda tam yolu kullanmak cakismayi tumden onler.
        ImGui::PushID(path.string().c_str());

        const ImVec2 size(m_ThumbnailSize, m_ThumbnailSize);
        bool clicked = false;

        if (entry.is_directory())
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.32f, 0.18f, 1.0f));
            clicked = ImGui::Button("[ ]", size);
            ImGui::PopStyleColor();
        }
        else if (IsImage(path) && m_Library)
        {
            // Kucuk resim = dosyanin KENDISI. Ayri bir ikon setine gerek yok
            // ve tarayicida gordugun her resim zaten onbellege giriyor.
            const std::string rel = FX::FileSystem::MakeRelativeToBase(path.string());
            auto texture = m_Library->Load(rel);

            if (texture)
            {
                clicked = ImGui::ImageButton("##thumb",
                    static_cast<ImTextureID>(texture->GetRendererID()),
                    size, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
                clicked = ImGui::Button("!", size);
                ImGui::PopStyleColor();
            }
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ColorForExtension(path));
            clicked = ImGui::Button(path.extension().string().c_str(), size);
            ImGui::PopStyleColor();
        }

        // Surukleme kaynagi: klasorler haric her sey.
        if (!entry.is_directory() && ImGui::BeginDragDropSource())
        {
            const std::string rel = FX::FileSystem::MakeRelativeToBase(path.string());

            // '\0' DAHIL gonderiyoruz: alan tarafi payload'i dogrudan
            // const char* olarak okuyor, sonlandirici olmazsa tasar.
            ImGui::SetDragDropPayload(kContentPayload, rel.c_str(), rel.size() + 1);
            ImGui::Text("%s", filename.c_str());
            ImGui::EndDragDropSource();
        }

        if (clicked && entry.is_directory())
            m_Current = path;

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", filename.c_str());

        ImGui::TextWrapped("%s", filename.c_str());

        ImGui::NextColumn();
        ImGui::PopID();
    }
}
