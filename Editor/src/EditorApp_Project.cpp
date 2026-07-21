#include "EditorApp.h"
#include "Platform/FileDialogs.h"
#include "SampleScene.h"
#include "Game/GameProject.h"
#include "Game/GameLibrary.h"

#include <FXEngine/Core/Log.h>
#include <FXEngine/Core/FileSystem.h>
#include <FXEngine/Core/Project.h>
#include <FXEngine/Asset/AssetManager.h>
#include <FXEngine/Scene/Components.h>
#include <FXEngine/Scene/ScriptRegistry.h>
#include <FXEngine/Scene/SceneSerializer.h>
#include <FXEngine/Scene/PrefabSerializer.h>

#include <imgui.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>

namespace FXEd
{
    // Proje, sahne dosyasi, editor.json ve varlik ice aktarma islemleri.
    // (EditorApp'in bolunmus tanimlari - bkz. EditorApp.h)

    namespace
    {
        std::string ToLowerAscii(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return s;
        }
    }

    // Modal diyaloglar cerceve DISINDA acilmali (bkz. Faz 12): ImGui'nin
    // ic durumu diyalog boyunca donar ve arkasi siyah kalirdi. Hem
    // karsilama ekrani hem de editor cercevesi ayni istekleri isliyor.
    void EditorApp::ProcessProjectRequests()
    {
        if (m_NewProjectRequested)  { m_NewProjectRequested  = false; NewProject();  }
        if (m_OpenProjectRequested) { m_OpenProjectRequested = false; OpenProject(); }

        if (!m_PendingProjectPath.empty())
        {
            const std::string path = m_PendingProjectPath;
            m_PendingProjectPath.clear();
            OpenProject(path);
        }
    }

    // -----------------------------------------------------------------------
    // Script dosyasi olusturma (A4, B-6'da projeye tasindi)
    // -----------------------------------------------------------------------
    // Script'ler artik PROJENIN icine yaziliyor: <proje>/assets/scripts/.
    // B-6'ya kadar Editor/src/Scripts/ altindaydilar (editore derleniyordu);
    // artik oyun kodu Game.dll'e ait, editore degil. Namespace de bu yuzden
    // FXEd degil FXGame - kod editore degil oyuna ait.
    //
    // Dosya adi = sinif adi = sahne dosyasina yazilan ad. Uc kimligi
    // ayirmak, uctan birini degistirince digerlerini bozmak demekti.
    bool EditorApp::CreateScriptFile(const std::string& name, std::string& outPath)
    {
        // Icerik panelinden gelen hedef klasor varsa oraya, yoksa
        // varsayilan assets/scripts'e.
        const std::string scriptsDir =
            !m_NewScriptDir.empty() ? m_NewScriptDir : GameProject::ScriptsDir();
        if (scriptsDir.empty())
            return false;   // acik proje yok

        const std::filesystem::path dir{ scriptsDir };

        std::error_code ec;
        std::filesystem::create_directories(dir, ec);

        const std::filesystem::path file = dir / (name + ".h");
        outPath = file.string();

        // Var olani EZMIYORUZ: yazilmis bir script'i sessizce silmek
        // geri alinamaz bir kayip olurdu.
        if (std::filesystem::exists(file))
            return false;

        std::ofstream out(file);
        if (!out)
            return false;

        // Ham dize: sablonun kendisi C++ kodu, kacis dizileriyle
        // yazmak okunmaz hale getirirdi. %s yerine gecen tek sey ad.
        static constexpr const char* kTemplate = R"TPL(#pragma once

#include <FXEngine/Scene/ScriptableEntity.h>
#include <FXEngine/Scene/Components.h>
#include <FXEngine/Core/Input.h>
#include <FXEngine/Core/Log.h>

namespace FXGame
{
    // SINIF ADI DOSYA ADIYLA AYNI OLMALI: kayit CMake tarafindan buna
    // gore uretiliyor (bkz. .fxbuild/GameRegistrations.h.in).
    class %NAME% : public FX::ScriptableEntity
    {
    protected:
        // Play basladiginda bir kez.
        void OnCreate() override
        {
            FX_INFO("%NAME%: OnCreate (%s)", GetEntity().GetName().c_str());
        }

        // Her sabit adimda; dt her zaman ayni deger (1/60).
        void OnUpdate(float dt) override
        {
            auto& tf = GetComponent<FX::TransformComponent>();

            // Ornek: saniyede 1 birim saga.
            tf.Translation.x += 1.0f * dt;
        }

        // Stop'ta veya entity silindiginde bir kez.
        void OnDestroy() override
        {
        }
    };
}
)TPL";

        std::string content = kTemplate;

        for (std::size_t pos = content.find("%NAME%");
             pos != std::string::npos;
             pos = content.find("%NAME%", pos + name.size()))
        {
            content.replace(pos, 6, name);
        }

        out << content;
        return out.good();
    }

    // -----------------------------------------------------------------------
    // Karsilama ekrani (Faz 21)
    // -----------------------------------------------------------------------
    void EditorApp::DrawLauncher()
    {
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(40.0f, 32.0f));

        ImGui::Begin("##Launcher", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);

        // Genis ekranda icerik sola yapisik kalmasin: sabit genislikte
        // bir sutun acip ortaliyoruz. Tam genislige yayilan bir liste
        // okunmaz - goz satirin basi ile sonu arasinda kaybolur.
        const float contentWidth = 760.0f;
        const float avail  = ImGui::GetContentRegionAvail().x;
        const float indent = (avail - contentWidth) * 0.5f;
        if (indent > 0.0f)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);

        // Grup DEGIL cocuk pencere: Separator ve benzeri ogeler icinde
        // bulunduklari pencerenin genisligini kullanir. Grup icinde
        // olsaydilar ekranin tamamina yayilir, ortalanmis icerikle
        // hizasiz gorunurlerdi.
        ImGui::BeginChild("##LauncherContent",
                          ImVec2(std::min(contentWidth, avail), 0.0f));

        // Logo + baslik yan yana. Logo yoksa (dosya bulunamadi) baslik
        // tek basina ciziliyor - kozmetik bir eksik programi durdurmamali.
        if (m_Logo)
        {
            const float size = 64.0f;
            // UV'ler ters: doku FlipVertically ile yuklendi, ImGui
            // sol-ust bekliyor.
            ImGui::Image(static_cast<ImTextureID>(m_Logo->GetRendererID()),
                         ImVec2(size, size), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
            ImGui::SameLine(0.0f, 16.0f);
        }

        ImGui::BeginGroup();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.80f, 0.35f, 1.0f));
        ImGui::SetWindowFontScale(1.8f);
        ImGui::TextUnformatted("FXEngine");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::TextDisabled("Bir proje ac veya yeni bir tane olustur.");
        ImGui::EndGroup();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // --- Eylemler -------------------------------------------------------
        if (ImGui::Button("Yeni Proje...", ImVec2(150.0f, 34.0f)))
            m_NewProjectRequested = true;

        ImGui::SameLine();
        if (ImGui::Button("Proje Ac...", ImVec2(150.0f, 34.0f)))
            m_OpenProjectRequested = true;

        ImGui::Spacing();
        ImGui::Spacing();

        // --- Son projeler ---------------------------------------------------
        ImGui::Text("Son Projeler");
        ImGui::Separator();

        if (m_RecentProjects.empty())
        {
            // Bos olsa da ayni yuksekligi kapliyor: liste dolunca
            // dugmelerin yeri degisirse kullanici her acilista farkli
            // bir duzenle karsilasir.
            ImGui::BeginChild("##RecentEmpty", ImVec2(0.0f, 240.0f));
            ImGui::Spacing();
            ImGui::TextDisabled("Henuz proje yok.");
            ImGui::TextDisabled("\"Yeni Proje...\" ile basla veya var olan bir");
            ImGui::TextDisabled(".fxproject dosyasini \"Proje Ac...\" ile sec.");
            ImGui::EndChild();
        }
        else
        {
            // Liste cizilirken degistirilemez (silme/acma yineleyiciyi
            // bozar); istekleri biriktirip dongu bittikten sonra
            // isliyoruz. Faz 9'daki "yapiyi degistiren islemler dongu
            // disinda" kuralinin bir baska gorunumu.
            std::string toOpen;
            std::string toForget;

            ImGui::BeginChild("##RecentList", ImVec2(0.0f, 240.0f));

            for (const auto& path : m_RecentProjects)
            {
                const bool exists = FX::FileSystem::Exists(path);
                const std::string name =
                    std::filesystem::path(path).stem().string();

                ImGui::PushID(path.c_str());

                // Kayip projeyi listeden gizlemek yerine GOSTERIP
                // isaretliyoruz: kullanici projesinin nerede oldugunu
                // hatirlamak isteyebilir, sessizce kaybolmasi kafa
                // karistirir.
                if (!exists)
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.4f, 0.4f, 1.0f));

                // TEK TIK aciyor. Karsilama ekraninda "secmek" diye bir
                // eylem yok - tikladigin projeyi acmak istiyorsundur.
                // Cift tik istemek, hicbir karsiligi olmayan bir ara
                // durum (secili ama acilmamis) uretirdi.
                if (ImGui::Selectable(name.c_str(), false, 0, ImVec2(0.0f, 38.0f)) && exists)
                    toOpen = path;

                if (!exists)
                    ImGui::PopStyleColor();

                // Yol, adin hemen altinda kucuk ve soluk: ayni isimde
                // iki proje olabilir, ayirt etmenin tek yolu yol.
                const ImVec2 itemMin = ImGui::GetItemRectMin();
                ImGui::GetWindowDrawList()->AddText(
                    ImVec2(itemMin.x + 4.0f, itemMin.y + 19.0f),
                    ImGui::GetColorU32(ImGuiCol_TextDisabled),
                    exists ? path.c_str() : (path + "   (bulunamadi)").c_str());

                if (ImGui::BeginPopupContextItem("##ctx"))
                {
                    if (exists && ImGui::MenuItem("Ac"))
                        toOpen = path;
                    if (ImGui::MenuItem("Listeden Kaldir"))
                        toForget = path;
                    ImGui::EndPopup();
                }

                ImGui::PopID();
            }

            ImGui::EndChild();

            ImGui::TextDisabled("Acmak icin tikla. Sag tik: listeden kaldir.");

            if (!toForget.empty())
            {
                m_RecentProjects.erase(
                    std::remove(m_RecentProjects.begin(), m_RecentProjects.end(), toForget),
                    m_RecentProjects.end());
                SaveEditorConfig();
            }

            if (!toOpen.empty())
                m_PendingProjectPath = toOpen;
        }

        // --- Projesiz devam ---------------------------------------------------
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Projesiz Devam Et (ornek sahne)", ImVec2(280.0f, 0.0f)))
            StartWithoutProject();

        ImGui::SameLine();
        ImGui::TextDisabled("Varliklar exe klasorunde kalir, build/ silinince kaybolur.");

        ImGui::EndChild();

        ImGui::End();
        ImGui::PopStyleVar(2);
    }

    void EditorApp::StartWithoutProject()
    {
        // Varlik taramasi doku YUKLEMEDEN once: ayarlar .meta'dan
        // geliyor ve tablo bos olsaydi varsayilanlara duserdik.
        // (Projeli modda bunu Project::Load yapiyor.)
        FX::AssetManager::ScanProject();

        // Sarma ayari artik BURADA verilmiyor: checkerboard.png'nin
        // kendi .meta dosyasinda "Repeat: true" yaziyor. Zemin dosemesi
        // TilingFactor ile tekrarlanabilsin diye gerekli olan ayar,
        // artik dokunun kalici bir ozelligi.
        m_Checkerboard = m_TextureLibrary.Load("assets/textures/checkerboard.png");
        m_Circle       = m_TextureLibrary.Load("assets/textures/circle.png");

        if (!m_Checkerboard || !m_Circle)
        {
            SetStatus("Ornek dokular bulunamadi.");
            FX_ERROR("Ornek dokular yuklenemedi. Aranan klasor: %s",
                     FX::FileSystem::GetProjectDirectory().c_str());
            return;
        }

        m_EditorScene = std::make_unique<FX::Scene>();
        m_Scene       = m_EditorScene.get();

        const FX::UUID player = SampleScene::Build(*m_Scene, m_Checkerboard, m_Circle);

        m_HierarchyPanel.SetContext(m_Scene);
        m_Selection.Select(m_Scene->FindEntityByUUID(player));

        m_ShowLauncher = false;
        FX_WARN("Projesiz mod: varliklar exe klasorunde tutulacak.");
    }

    // -----------------------------------------------------------------------
    // Proje islemleri (Faz 21)
    // -----------------------------------------------------------------------
    void EditorApp::NewProject()
    {
        // KLASOR diyalogu yerine "farkli kaydet" kullaniyoruz: kullanici
        // .fxproject dosyasinin adini ve yerini seciyor, projenin koku de
        // o dosyanin klasoru oluyor. Win32'nin klasor secme diyalogu ayri
        // bir API (SHBrowseForFolder/IFileDialog) ve daha kotu bir
        // kullanici deneyimi sunuyor - burada ikinci bir API'ye gerek yok.
        const std::string path = FileDialogs::SaveFile(GetWindow(),
                                                       FileDialogs::ProjectFilter());
        if (path.empty())
            return;

        const auto file = std::filesystem::path(path);
        const std::string dir  = file.parent_path().string();
        const std::string name = file.stem().string();

        if (auto project = FX::Project::Create(dir, name))
        {
            PushRecentProject(project->GetFilePath());
            PrepareGameBuild();
            LoadGameLibrary();

            NewScene();
            m_ContentBrowser.SetContext(&m_TextureLibrary);   // kok degisti
            m_ShowLauncher = false;
            SetStatus("Proje olusturuldu: " + name);
        }
        else
        {
            SetStatus("PROJE OLUSTURULAMADI");
        }
    }

    void EditorApp::PrepareGameBuild()
    {
        std::string error;
        if (!GameProject::WriteBuildFiles(&error))
            FX_WARN("Oyun derleme iskelesi kurulamadi: %s", error.c_str());
    }

    void EditorApp::LoadGameLibrary()
    {
        const std::string dll = GameProject::DllPath();

        std::string error;
        if (!m_GameLibrary.Load(dll, &error))
        {
            // Henuz derlenmemis bir proje icin bu NORMAL: Derle'ye
            // basilana kadar (B-5) Game.dll yok. Sessiz gecmiyoruz ama
            // hata da degil.
            FX_INFO("Oyun kutuphanesi yuklenmedi: %s", error.c_str());
            return;
        }

        std::string detail;
        const bool ok = m_GameLibrary.RunSelfTest(&detail);
        if (ok)
            FX_INFO("Game.dll: %s", detail.c_str());
        else
            FX_ERROR("Game.dll self-test BASARISIZ: %s", detail.c_str());

        const auto& names = FX::ScriptRegistry::GetNames();
        FX_INFO("Game.dll yuklendi, %zu script kayitli.", names.size());
    }

    void EditorApp::BuildGame()
    {
        if (!FX::Project::HasActive())
        {
            SetStatus("Derlemek icin once bir proje ac.");
            return;
        }

        // Play sirasinda yeniden yukleme YASAK (vtable'lar gecersizlesir):
        // once Stop. Kullaniciya soyluyoruz, sessizce durdurmuyoruz.
        if (IsPlaying())
        {
            OnSceneStop();
            SetStatus("Derleme icin Play durduruldu.");
        }

        SetStatus("Derleniyor...");

        // Golge kopya sayesinde ORIJINAL Game.dll kilitli degil (yuklu
        // olan out/loaded/Game.dll); dolayisiyla once bosaltmaya gerek
        // yok, dogrudan derleyebiliyoruz.
        const int rc = GameProject::Build(FX_BUILD_CONFIG, m_BuildLog);
        m_LastBuildOk      = (rc == 0);
        m_ShowBuildConsole = true;
        m_ScrollBuildLog   = true;

        // Cikti icindeki hata satirlarini ayikla: konsolun en ustunde
        // ozet olarak gosterilecek (uzun cmake ciktisinda dosya+satiri
        // aramak zor). MSVC hatalari "dosya(satir): error Cxxxx" formatinda.
        m_BuildErrors.clear();
        if (!m_LastBuildOk)
        {
            std::istringstream stream(m_BuildLog);
            std::string line;
            while (std::getline(stream, line))
            {
                std::string low = line;
                std::transform(low.begin(), low.end(), low.begin(),
                               [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
                // "error C", "error LNK", "fatal error", "error MSB"...
                if (low.find("error") != std::string::npos &&
                    low.find("0 error") == std::string::npos)
                {
                    // Satir sonundaki \r'yi at (Windows cikti).
                    if (!line.empty() && line.back() == '\r')
                        line.pop_back();
                    m_BuildErrors.push_back(line);
                }
            }
        }

        if (!m_LastBuildOk)
        {
            SetStatus("Derleme BASARISIZ - konsola bak.");
            FX_ERROR("Oyun derlemesi basarisiz (kod %d).", rc);
            return;
        }

        // Basariliysa yeni DLL'i yeniden yukle (Load once eskisini birakir).
        LoadGameLibrary();
        SetStatus("Derlendi ve yeniden yuklendi.");
    }

    void EditorApp::OpenProject()
    {
        const std::string path = FileDialogs::OpenFile(GetWindow(),
                                                       FileDialogs::ProjectFilter());
        if (!path.empty())
            OpenProject(path);
    }

    bool EditorApp::OpenProject(const std::string& filepath)
    {
        auto project = FX::Project::Load(filepath);
        if (!project)
        {
            SetStatus("PROJE ACILAMADI: " + filepath);
            return false;
        }

        PushRecentProject(project->GetFilePath());
        PrepareGameBuild();

        // Script'ler sahneden ONCE kayitli olmali: sahne yuklemesi
        // (StartScene) script adlarini cozmeye calisacak.
        LoadGameLibrary();

        // Doku onbellegi ESKI projenin yollarini tutuyor. Temizlemezsek
        // yeni projedeki "assets/textures/x.png" eski projenin ayni
        // isimli dosyasini dondururdu - izi surulmesi cok zor bir hata.
        m_TextureLibrary.Clear();
        m_Checkerboard.reset();
        m_Circle.reset();

        // Sahne listesi de eski projeye aitti.
        m_RecentScenes.clear();

        m_ContentBrowser.SetContext(&m_TextureLibrary);

        // Baslangic sahnesi GUID ile tutuluyor (0.6): dosyanin yeri
        // degismis olabilir, tabloya soruyoruz.
        const FX::AssetHandle startScene = project->GetConfig().StartScene;
        const std::string startPath      = FX::AssetManager::GetPath(startScene);

        if (!startPath.empty() &&
            FX::FileSystem::Exists(FX::FileSystem::ResolveProjectAsset(startPath)))
        {
            OpenScene(startPath);
        }
        else
        {
            if (startScene.IsValid())
                FX_WARN("Baslangic sahnesi bulunamadi (GUID %llu)",
                        static_cast<unsigned long long>(startScene));
            NewScene();
        }

        m_ShowLauncher = false;
        SetStatus("Proje acildi: " + project->GetConfig().Name);
        return true;
    }

    void EditorApp::PushRecentProject(const std::string& path)
    {
        if (path.empty())
            return;

        m_RecentProjects.erase(
            std::remove(m_RecentProjects.begin(), m_RecentProjects.end(), path),
            m_RecentProjects.end());

        m_RecentProjects.push_front(path);

        while (m_RecentProjects.size() > kMaxRecentProjects)
            m_RecentProjects.pop_back();

        SaveEditorConfig();
    }

    void EditorApp::NewScene()
    {
        m_EditorScene = std::make_unique<FX::Scene>();
        m_Scene       = m_EditorScene.get();
        m_ScenePath.clear();

        // Undo gecmisi eski sahnenin entity'lerine referans tutuyor;
        // yeni sahnede anlamsiz ve tehlikeli.
        m_Commands.Clear();

        m_HierarchyPanel.SetContext(m_Scene);
        SetStatus("Yeni bos sahne");
    }

    void EditorApp::SaveScene()
    {
        // Yol yoksa "kaydet" ile "farkli kaydet" ayni seydir. Sessizce
        // varsayilan bir yola yazmak, kullanicinin dosyasini nereye
        // koydugumuzu bilmemesi demek olurdu.
        if (m_ScenePath.empty())
        {
            SaveSceneAs();
            return;
        }

        FX::SceneSerializer serializer(m_Scene, &m_TextureLibrary);

        if (serializer.Serialize(m_ScenePath))
        {
            SetStatus("Kaydedildi: " + m_ScenePath);
            PushRecentScene(m_ScenePath);
        }
        else
        {
            SetStatus("KAYDEDILEMEDI: " + m_ScenePath);
        }
    }

    void EditorApp::SaveSceneAs()
    {
        const std::string absolute =
            FileDialogs::SaveFile(GetWindow(), FileDialogs::SceneFilter());

        if (absolute.empty())
            return;   // kullanici iptal etti, hata degil

        // Mutlak degil GORECELI yol sakliyoruz: proje baska bir makineye
        // kopyalandiginda "C:/Users/..." hicbir sey ifade etmez.
        m_ScenePath = FX::FileSystem::MakeRelativeToProject(absolute);
        SaveScene();
    }

    void EditorApp::OpenScene()
    {
        const std::string absolute =
            FileDialogs::OpenFile(GetWindow(), FileDialogs::SceneFilter());

        if (absolute.empty())
            return;

        OpenScene(FX::FileSystem::MakeRelativeToProject(absolute));
    }

    void EditorApp::OpenScene(const std::string& path)
    {
        FX::SceneSerializer serializer(m_Scene, &m_TextureLibrary);

        if (!serializer.Deserialize(path))
        {
            SetStatus("YUKLENEMEDI: " + path);
            return;
        }

        m_ScenePath = path;
        SetStatus("Yuklendi: " + path);
        PushRecentScene(path);

        // Undo gecmisi eski sahneye aitti.
        m_Commands.Clear();

        // Panel secimini temizliyoruz: secili entity KULLANICI tercihiydi,
        // yeni sahnede karsiligi olmayabilir.
        m_HierarchyPanel.SetContext(m_Scene);
    }

    // -----------------------------------------------------------------------
    // Editor tercihleri: exe'nin yaninda, imgui.ini ile ayni yerde.
    // Sahne dosyasina yazamayiz - bunlar sahnenin degil KULLANICININ verisi
    // ve baska bir sahne acildiginda da gecerli kalmali.
    // -----------------------------------------------------------------------
    void EditorApp::LoadEditorConfig()
    {
        const std::string path = FX::FileSystem::GetBaseDirectory() + "editor.json";

        std::ifstream in(path);
        if (!in)
            return;   // ilk calistirma, dosya henuz yok

        nlohmann::json doc;
        try
        {
            in >> doc;
        }
        catch (const nlohmann::json::parse_error& err)
        {
            FX_WARN("editor.json bozuk, yok sayiliyor: %s", err.what());
            return;
        }

        if (doc.contains("RecentScenes") && doc["RecentScenes"].is_array())
        {
            for (const auto& item : doc["RecentScenes"])
            {
                if (!item.is_string())
                    continue;

                // Silinmis dosyalari listeye alma: tiklandiginda hata
                // verecek bir menu ogesi gostermenin anlami yok.
                const std::string scene = item.get<std::string>();
                if (FX::FileSystem::Exists(FX::FileSystem::ResolveProjectAsset(scene)))
                    m_RecentScenes.push_back(scene);
            }
        }

        // Projeler MUTLAK yol tasiyor: proje kokunun kendisi soz konusu
        // oldugunda "neye goreceli?" sorusunun cevabi yok.
        if (doc.contains("RecentProjects") && doc["RecentProjects"].is_array())
        {
            for (const auto& item : doc["RecentProjects"])
            {
                if (item.is_string())
                    m_RecentProjects.push_back(item.get<std::string>());
            }
        }

        m_ContentBrowser.SetListView(doc.value("ContentListView", false));

        // Viewport tercihleri (A3). A3'e kadar bunlar koda gomuluydu ve
        // her acilista sifirlaniyordu: kademe degerini ayarliyordun,
        // editoru kapatip acinca varsayilana donuyordu.
        m_ShowGrid          = doc.value("ShowGrid", true);
        m_ShowCameraGizmos  = doc.value("ShowCameraGizmos", true);
        m_GizmoMode         = doc.value("GizmoMode", 0);
        m_SnapEnabled       = doc.value("SnapEnabled", false);
        m_SnapTranslate     = doc.value("SnapTranslate", 0.5f);
        m_SnapRotate        = doc.value("SnapRotate", 15.0f);
        m_SnapScale         = doc.value("SnapScale", 0.25f);
        m_GameViewLocked    = doc.value("GameViewLockAspect", false);

        m_EditorCamera.SetMoveSpeed(doc.value("CameraMoveSpeed", 8.0f));

        FX_INFO("editor.json okundu (%zu son sahne, %zu son proje).",
                m_RecentScenes.size(), m_RecentProjects.size());
    }

    void EditorApp::SaveEditorConfig()
    {
        nlohmann::json doc;
        doc["RecentScenes"]   = m_RecentScenes;
        doc["RecentProjects"] = m_RecentProjects;

        // Gorunum tercihi de kullanicinin verisi: her proje acilisinda
        // Izgara'ya donmesi kucuk ama surekli bir rahatsizlikti.
        doc["ContentListView"] = m_ContentBrowser.IsListView();

        doc["ShowGrid"]           = m_ShowGrid;
        doc["ShowCameraGizmos"]   = m_ShowCameraGizmos;
        doc["GizmoMode"]          = m_GizmoMode;
        doc["SnapEnabled"]        = m_SnapEnabled;
        doc["SnapTranslate"]      = m_SnapTranslate;
        doc["SnapRotate"]         = m_SnapRotate;
        doc["SnapScale"]          = m_SnapScale;
        doc["GameViewLockAspect"] = m_GameViewLocked;
        doc["CameraMoveSpeed"]    = m_EditorCamera.GetMoveSpeed();

        std::ofstream out(FX::FileSystem::GetBaseDirectory() + "editor.json");
        if (out)
            out << std::setw(2) << doc << std::endl;
    }

    void EditorApp::PushRecentScene(const std::string& path)
    {
        // Zaten varsa cikar: tekrar eklemek listenin BASINA tasimak demek.
        m_RecentScenes.erase(
            std::remove(m_RecentScenes.begin(), m_RecentScenes.end(), path),
            m_RecentScenes.end());

        m_RecentScenes.push_front(path);

        while (m_RecentScenes.size() > kMaxRecentScenes)
            m_RecentScenes.pop_back();

        SaveEditorConfig();
    }

    void EditorApp::ImportAssets()
    {
        const auto files = FileDialogs::OpenFiles(GetWindow(), FileDialogs::AssetFilter());
        if (files.empty())
            return;

        std::size_t imported = 0;
        for (const auto& file : files)
        {
            if (!m_ContentBrowser.ImportFile(file).empty())
                ++imported;
        }

        SetStatus(std::to_string(imported) + " dosya ice aktarildi");
    }

    void EditorApp::HandleExternalDrop(const std::string& absolutePath,
                                       float screenX, float screenY)
    {
        // Once assets/ icine KOPYALA. Kopyalamadan dogrudan kullanmak,
        // sahne dosyasina kullanicinin masaustunu gosteren bir yol
        // yazmak demek olurdu - proje tasinir tasinmaz kirilir.
        const std::string relative = m_ContentBrowser.ImportFile(absolutePath);
        if (relative.empty())
        {
            SetStatus("Ice aktarilamadi: " + absolutePath);
            return;
        }

        SetStatus("Ice aktarildi: " + relative);

        // Viewport uzerine birakildiysa ayrica sahneye de koy.
        const bool overViewport =
            screenX >= m_ViewportBoundsMin.x && screenX <= m_ViewportBoundsMax.x &&
            screenY >= m_ViewportBoundsMin.y && screenY <= m_ViewportBoundsMax.y;

        if (overViewport)
            HandleContentDrop(relative, screenX, screenY);
    }

    void EditorApp::SetAsStartScene()
    {
        auto project = FX::Project::GetActive();
        if (!project || m_ScenePath.empty())
            return;

        const FX::AssetHandle handle = FX::AssetManager::GetHandle(m_ScenePath);
        if (!handle.IsValid())
        {
            // Sahne yeni kaydedildiyse izleyici henuz yakalamamis
            // olabilir; kaydettirmek beklemekten iyi.
            const FX::AssetHandle fresh = FX::AssetManager::Register(m_ScenePath);
            if (!fresh.IsValid())
            {
                SetStatus("Sahne varlik tablosuna eklenemedi: " + m_ScenePath);
                return;
            }
            project->GetConfig().StartScene = fresh;
        }
        else
        {
            project->GetConfig().StartScene = handle;
        }

        if (project->Save())
            SetStatus("Baslangic sahnesi: " + m_ScenePath);
        else
            SetStatus("Proje kaydedilemedi");
    }

    void EditorApp::OpenAsset(const std::string& relativePath)
    {
        const std::string ext =
            ToLowerAscii(std::filesystem::path(relativePath).extension().string());

        if (ext == ".fxscene")
        {
            OpenScene(relativePath);
            return;
        }

        if (ext == ".fxprefab")
        {
            // Suruklemenin aksine hedef nokta yok; viewport'un ortasi
            // en az sasirtan yer.
            HandleContentDrop(relativePath,
                              (m_ViewportBoundsMin.x + m_ViewportBoundsMax.x) * 0.5f,
                              (m_ViewportBoundsMin.y + m_ViewportBoundsMax.y) * 0.5f);
            return;
        }

        // Editorun anlamadigi turler isletim sistemine gidiyor: bir png'ye
        // cift tiklayinca sahneye sprite dusurmek beklenmeyen bir sey olurdu.
        FileDialogs::OpenExternally(FX::FileSystem::ResolveProjectAsset(relativePath));
    }

    void EditorApp::HandleContentDrop(const std::string& relativePath,
                                      float screenX, float screenY)
    {
        const std::string ext = std::filesystem::path(relativePath).extension().string();

        if (ext == ".fxscene")
        {
            OpenScene(relativePath);
            return;
        }

        const glm::vec2 world = m_EditorCamera.ScreenToWorld(screenX, screenY);

        if (ext == ".fxprefab")
        {
            FX::PrefabSerializer prefab(m_Scene, &m_TextureLibrary);

            FX::Entity instance = prefab.Instantiate(relativePath, { world.x, world.y, 0.2f });
            if (instance)
            {
                m_Selection.Select(instance);
                SetStatus("Prefab eklendi: " + instance.GetName());
            }
            else
            {
                SetStatus("Prefab orneklenemedi: " + relativePath);
            }
            return;
        }

        auto texture = m_TextureLibrary.Load(relativePath);
        if (!texture)
        {
            SetStatus("Bu dosya viewport'a birakilamaz: " + relativePath);
            return;
        }

        FX::Entity entity =
            m_Scene->CreateEntity(std::filesystem::path(relativePath).stem().string());

        auto& tf = entity.GetComponent<FX::TransformComponent>();
        tf.Translation = { world.x, world.y, 0.2f };

        // Sprite'i resmin en-boy oranina gore olcekliyoruz. Etmezsek
        // dikdortgen bir resim kare kutuya sikisip ezik gorunur.
        const float aspect = static_cast<float>(texture->GetWidth()) /
                             static_cast<float>(texture->GetHeight());
        tf.Scale = (aspect >= 1.0f) ? glm::vec2{ 1.0f, 1.0f / aspect }
                                    : glm::vec2{ aspect, 1.0f };

        entity.AddComponent<FX::SpriteRendererComponent>(texture);

        m_Selection.Select(entity);
        SetStatus("Sprite olusturuldu: " + entity.GetName());
    }

}
