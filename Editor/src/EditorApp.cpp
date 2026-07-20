#include "EditorApp.h"
#include "Platform/FileDialogs.h"

#include <FXEngine/Core/Log.h>
#include <FXEngine/Core/FileSystem.h>
#include <FXEngine/Core/Project.h>
#include <FXEngine/Asset/AssetManager.h>
#include <FXEngine/Renderer/RenderCommand.h>
#include <FXEngine/Renderer/Renderer2D.h>
#include <FXEngine/Scene/Components.h>
#include <FXEngine/Scene/SceneSerializer.h>
#include <FXEngine/Scene/PrefabSerializer.h>

#include <imgui.h>
#include <ImGuizmo.h>

#include <nlohmann/json.hpp>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>

namespace FXEd
{
    namespace
    {
        std::mt19937 s_Rng{ 12345 };

        float RandFloat(float min, float max)
        {
            std::uniform_real_distribution<float> dist(min, max);
            return dist(s_Rng);
        }
    }

    EditorApp::EditorApp()
        : FX::Application([]() {
              FX::WindowProps props;
              props.Title     = "FXEditor - Faz 12";
              props.Width     = 1600;
              props.Height    = 900;
              props.VSync     = true;
              props.Resizable = true;
              return props;
          }())
    {
    }

    void EditorApp::OnInit()
    {
        FX_INFO("=====================================");
        FX_INFO("  FXEditor - Faz 12: Varlik yonetimi");
        FX_INFO("=====================================");

        FX::RenderCommand::Init();
        FX::Renderer2D::Init();

        m_ImGuiLayer.Init(GetWindow());

        // Iki renk eki: gorunen goruntu + entity ID (secim icin).
        FX::FramebufferSpec fbSpec;
        fbSpec.Width  = GetWindow().GetWidth();
        fbSpec.Height = GetWindow().GetHeight();
        fbSpec.Attachments = {
            FX::FramebufferTextureFormat::RGBA8,
            FX::FramebufferTextureFormat::RED_INTEGER,
            FX::FramebufferTextureFormat::DEPTH24STENCIL8
        };
        m_Framebuffer = std::make_unique<FX::Framebuffer>(fbSpec);

        m_HierarchyPanel.SetTextureLibrary(&m_TextureLibrary);
        m_ContentBrowser.SetContext(&m_TextureLibrary);

        LoadEditorConfig();

        // Editor BOS bir sahne ile aciliyor ve karsilama ekranini
        // gosteriyor. Sahnenin var olmasi sart: panellerin ve render
        // yolunun her yerinde null kontrolu yapmaktansa bos bir sahne
        // tutmak cok daha basit.
        NewScene();

        // Proje SECILMEDEN varlik yuklemiyoruz: varlik yollari ve doku
        // onbellegi projeye goreli cozuluyor. Once yukleyip sonra proje
        // acsaydik eski kokten gelen dokular onbellekte kalirdi.
        m_ShowLauncher = true;

        FX_INFO("");
        FX_INFO("Editor hazir. Panelleri surukleyerek yeniden duzenleyebilirsin;");
        FX_INFO("duzen imgui.ini'ye kaydedilir.");
        FX_INFO("Viewport kamerasi: sag tus (veya Space+sol tus) ile kaydir,");
        FX_INFO("W/A/S/D ile de kaydirabilirsin, tekerlek imlece dogru zoom yapar.");
        FX_INFO("Ctrl+N yeni, Ctrl+O ac, Ctrl+S kaydet, Ctrl+Shift+S farkli kaydet.");
        FX_INFO("Icerik panelinden viewport'a resim/prefab/sahne surukleyebilirsin.");
        FX_INFO("Disaridan dosya: pencereye surukle-birak veya Ctrl+I.");
        FX_INFO("Viewport'ta sol tik ile entity sec, F ile odaklan, G izgara.");
        FX_INFO("Gizmo: Z kapali, X tasi, C dondur, B olcekle (Ctrl = kademeli)");
    }

    void EditorApp::BuildScene()
    {
        m_EditorScene = std::make_unique<FX::Scene>();
        m_Scene       = m_EditorScene.get();

        {
            auto floor = m_Scene->CreateEntity("Zemin");
            auto& tf = floor.GetComponent<FX::TransformComponent>();
            tf.Translation = { 0.0f, 0.0f, -0.5f };
            tf.Scale       = { 30.0f, 30.0f };

            auto& sprite = floor.AddComponent<FX::SpriteRendererComponent>(m_Checkerboard);
            sprite.TilingFactor = 15.0f;
            sprite.Color = { 0.55f, 0.58f, 0.70f, 1.0f };
        }

        {
            auto player = m_Scene->CreateEntity("Oyuncu");
            auto& tf = player.GetComponent<FX::TransformComponent>();
            tf.Translation = { 0.0f, 0.0f, 0.1f };

            player.AddComponent<FX::SpriteRendererComponent>(
                m_Circle, glm::vec4{ 1.0f, 0.85f, 0.3f, 1.0f });
            player.AddComponent<FX::VelocityComponent>();

            // Tutamak degil KIMLIK sakliyoruz - yuklemeden sag cikar.
            m_PlayerUUID = player.GetComponent<FX::IDComponent>().ID;
        }

        // --- Takipciler --------------------------------------------------------
        // Faz 8'in kanit sahnesi: bu uc entity oyuncuyu UUID uzerinden
        // takip ediyor. Sahneyi kaydedip yukledikten sonra da takip
        // ETMEYE DEVAM etmeliler - iste kalici kimligin butun mesele.
        for (int i = 0; i < 3; ++i)
        {
            auto e = m_Scene->CreateEntity("Takipci " + std::to_string(i));

            auto& tf = e.GetComponent<FX::TransformComponent>();
            tf.Translation = { -6.0f + static_cast<float>(i) * 1.5f, -6.0f, 0.05f };
            tf.Scale = { 0.7f, 0.7f };

            e.AddComponent<FX::SpriteRendererComponent>(
                m_Circle, glm::vec4{ 1.0f, 0.35f, 0.35f, 1.0f });
            e.AddComponent<FX::VelocityComponent>();

            auto& fc = e.AddComponent<FX::FollowComponent>();
            fc.Target       = m_PlayerUUID;
            fc.Speed        = 1.5f + static_cast<float>(i) * 0.5f;
            fc.StopDistance = 1.0f + static_cast<float>(i) * 0.6f;
        }

        // --- Hiyerarsi ornegi ---------------------------------------------------
        // Govde doner; kule ve namlu onun cocugu oldugu icin birlikte doner.
        // Kule kendi de doner -> namlu iki donmeyi birden alir.
        {
            auto body = m_Scene->CreateEntity("Tank Govde");
            auto& btf = body.GetComponent<FX::TransformComponent>();
            btf.Translation = { -6.0f, 4.0f, 0.2f };
            btf.Scale       = { 2.0f, 2.0f };
            body.AddComponent<FX::SpriteRendererComponent>(
                m_Checkerboard, glm::vec4{ 0.45f, 0.75f, 0.45f, 1.0f });
            body.AddComponent<FX::VelocityComponent>(glm::vec2{ 0.0f, 0.0f }, 0.4f);

            auto turret = m_Scene->CreateEntity("Tank Kule");
            auto& ttf = turret.GetComponent<FX::TransformComponent>();
            ttf.Translation = { 0.0f, 0.0f, 0.05f };
            ttf.Scale       = { 0.55f, 0.55f };
            turret.AddComponent<FX::SpriteRendererComponent>(
                m_Circle, glm::vec4{ 1.0f, 0.9f, 0.4f, 1.0f });
            turret.AddComponent<FX::VelocityComponent>(glm::vec2{ 0.0f, 0.0f }, 1.5f);
            turret.SetParent(body);

            auto barrel = m_Scene->CreateEntity("Tank Namlu");
            auto& natf = barrel.GetComponent<FX::TransformComponent>();
            natf.Translation = { 0.9f, 0.0f, 0.05f };
            natf.Scale       = { 1.4f, 0.35f };
            barrel.AddComponent<FX::SpriteRendererComponent>(
                glm::vec4{ 0.9f, 0.35f, 0.25f, 1.0f });
            barrel.SetParent(turret);
        }

        for (int i = 0; i < 8; ++i)
        {
            auto e = m_Scene->CreateEntity("Uydu " + std::to_string(i));

            const float angle = static_cast<float>(i) / 8.0f * 6.2831853f;
            auto& tf = e.GetComponent<FX::TransformComponent>();
            tf.Translation = { std::cos(angle) * 4.0f, std::sin(angle) * 4.0f, 0.0f };
            tf.Scale       = { 0.6f, 0.6f };
            tf.Rotation    = angle;

            e.AddComponent<FX::SpriteRendererComponent>(
                m_Checkerboard,
                glm::vec4{ 0.4f + RandFloat(0.0f, 0.6f), 0.4f + RandFloat(0.0f, 0.6f), 0.9f, 1.0f });
            e.AddComponent<FX::VelocityComponent>(glm::vec2{ 0.0f, 0.0f }, 1.2f);
        }

        for (int i = 0; i < 15; ++i)
            SpawnMover();

        // Play modunun bakacagi kamera. Oyuncuya parent yapiliyor:
        // takip mantigi icin ozel bir koda gerek yok, hiyerarsi
        // (Faz 9) zaten bunu yapiyor.
        {
            auto cam = m_Scene->CreateEntity("Ana Kamera");
            cam.AddComponent<FX::CameraComponent>(6.0f, true);
            if (FX::Entity player = GetPlayer())
                cam.SetParent(player);
        }

        FX_INFO("Sahne kuruldu: %u entity", m_Scene->GetEntityCount());
    }

    void EditorApp::SpawnMover()
    {
        auto e = m_Scene->CreateEntity("Gezgin");

        auto& tf = e.GetComponent<FX::TransformComponent>();
        tf.Translation = { RandFloat(-8.0f, 8.0f), RandFloat(-8.0f, 8.0f), 0.0f };
        const float s = RandFloat(0.2f, 0.6f);
        tf.Scale = { s, s };

        if (RandFloat(0.0f, 1.0f) > 0.5f)
            e.AddComponent<FX::SpriteRendererComponent>(
                m_Circle, glm::vec4{ RandFloat(0.3f, 1.0f), RandFloat(0.3f, 1.0f),
                                     RandFloat(0.3f, 1.0f), 0.9f });
        else
            e.AddComponent<FX::SpriteRendererComponent>(
                glm::vec4{ RandFloat(0.3f, 1.0f), RandFloat(0.3f, 1.0f),
                           RandFloat(0.3f, 1.0f), 0.9f });

        e.AddComponent<FX::VelocityComponent>(
            glm::vec2{ RandFloat(-1.5f, 1.5f), RandFloat(-1.5f, 1.5f) },
            RandFloat(-2.0f, 2.0f));
    }

    FX::Entity EditorApp::GetPlayer()
    {
        if (!m_Scene || !m_PlayerUUID.IsValid())
            return {};

        // O(1) harita aramasi. Her karede cagirmak sorun degil ve
        // tutamak onbelleklemenin aksine GUVENLI: oyuncu silinmisse
        // gecersiz Entity doner, biz de fark ederiz.
        return m_Scene->FindEntityByUUID(m_PlayerUUID);
    }

    void EditorApp::SetStatus(const std::string& message)
    {
        m_StatusMessage = message;
        m_StatusTimer   = 4.0f;
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

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.80f, 0.35f, 1.0f));
        ImGui::SetWindowFontScale(1.8f);
        ImGui::TextUnformatted("FXEngine");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::TextDisabled("Bir proje ac veya yeni bir tane olustur.");
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

        BuildScene();
        m_HierarchyPanel.SetContext(m_Scene);
        m_HierarchyPanel.SetSelectedEntity(GetPlayer());

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

        // Doku onbellegi ESKI projenin yollarini tutuyor. Temizlemezsek
        // yeni projedeki "assets/textures/x.png" eski projenin ayni
        // isimli dosyasini dondururdu - izi surulmesi cok zor bir hata.
        m_TextureLibrary.Clear();
        m_Checkerboard.reset();
        m_Circle.reset();

        // Sahne listesi de eski projeye aitti.
        m_RecentScenes.clear();

        m_ContentBrowser.SetContext(&m_TextureLibrary);

        const auto& startScene = project->GetConfig().StartScene;
        if (!startScene.empty() &&
            FX::FileSystem::Exists(FX::FileSystem::ResolveProjectAsset(startScene)))
        {
            OpenScene(startScene);
        }
        else
        {
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
        m_PlayerUUID = FX::UUID{ 0 };
        m_ScenePath.clear();

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

        // Panel secimini temizliyoruz: secili entity KULLANICI tercihiydi,
        // yeni sahnede karsiligi olmayabilir. m_PlayerUUID'ye dokunmuyoruz -
        // kimlik dosyada yaziyor, ayni degerle geri geliyor (Faz 8).
        m_HierarchyPanel.SetContext(m_Scene);

        if (GetPlayer())
            FX_INFO("Oyuncu yuklemeden sonra UUID ile bulundu: %llu",
                    static_cast<unsigned long long>(m_PlayerUUID));
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

        FX_INFO("editor.json okundu (%zu son sahne, %zu son proje).",
                m_RecentScenes.size(), m_RecentProjects.size());
    }

    void EditorApp::SaveEditorConfig()
    {
        nlohmann::json doc;
        doc["RecentScenes"]   = m_RecentScenes;
        doc["RecentProjects"] = m_RecentProjects;

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
                m_HierarchyPanel.SetSelectedEntity(instance);
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

        m_HierarchyPanel.SetSelectedEntity(entity);
        SetStatus("Sprite olusturuldu: " + entity.GetName());
    }

    void EditorApp::OnScenePlay()
    {
        if (IsPlaying())
            return;

        // Oyun KOPYADA calisir. Duzenlenen sahneye hic dokunulmuyor;
        // Stop'ta kopyayi atmak yeterli, geri alma islemi gerekmiyor.
        m_RuntimeScene = FX::Scene::Copy(*m_EditorScene);
        m_Scene        = m_RuntimeScene.get();
        m_SceneState   = SceneState::Play;

        // Secim ESKI sahneye ait bir tutamak tasiyordu; yeni sahnede
        // anlamsiz. Faz 8'de ogrendigimiz ders: tutamak sahneye bagli,
        // kimlik degil.
        m_HierarchyPanel.SetContext(m_Scene);
        m_HierarchyPanel.SetSelectedEntity({});

        m_ScenePaused = false;

        if (FX::Entity cam = m_Scene->GetPrimaryCameraEntity())
        {
            FX_INFO("Play: sahne kamerasi '%s' (size=%.1f)", cam.GetName().c_str(),
                    cam.GetComponent<FX::CameraComponent>().OrthographicSize);
        }
        else
        {
            FX_WARN("Play: sahnede isaretli kamera yok, editor kamerasi kullanilacak.");
        }

        SetStatus("Play - oyun kopya sahnede calisiyor");
    }

    void EditorApp::OnSceneStop()
    {
        if (!IsPlaying())
            return;

        m_Scene      = m_EditorScene.get();
        m_SceneState = SceneState::Edit;
        m_RuntimeScene.reset();   // kopyada olan biten burada kayboluyor

        m_HierarchyPanel.SetContext(m_Scene);
        m_HierarchyPanel.SetSelectedEntity({});

        SetStatus("Stop - duzenleme sahnesine donuldu");
    }

    void EditorApp::OnWindowResize(std::uint32_t, std::uint32_t)
    {
        // Framebuffer ve kamera artik viewport paneline bagli.
        // Pencere boyutu degisince panel de degisecek ve
        // DrawViewportPanel bunu yakalayacak - burada is yok.
    }

    void EditorApp::OnUpdate(float dt)
    {
        m_Time += dt;

        if (m_StatusTimer > 0.0f)
            m_StatusTimer -= dt;

        // Iki kademeli kontrol: ImGui klavyeyi istiyorsa (metin kutusuna
        // yaziliyor) veya fare viewport'ta degilse kamera oynamamali.
        // Birincisi olmadan entity adina "Wall" yazarken 'W' kamerayi
        // kaydirir - ImGui kullanan uygulamalarda en sik yapilan hata.
        if (!m_ImGuiLayer.WantsKeyboard() && (m_ViewportHovered || m_ViewportFocused))
            m_EditorCamera.OnUpdate(dt);

        // FAZ 10 DEGISIKLIGI: sahne artik SADECE Play modunda calisiyor.
        //
        // Onceden editorde de nesneler hareket ediyordu; bir sprite'i
        // yerlestirmeye calisirken kacmasi duzenlemeyi imkansiz kilar.
        // Bir editorde Edit modu DURAGANDIR - bu bir eksiklik degil,
        // tanimin kendisi.
        if (IsPlaying() && !m_ScenePaused)
            m_Scene->OnUpdate(dt);
    }

    void EditorApp::OnRender(float /*alpha*/)
    {
        {
            static std::uint64_t s_LastTime = SDL_GetTicksNS();
            const std::uint64_t now = SDL_GetTicksNS();
            m_FpsTimer += static_cast<float>(now - s_LastTime) / 1.0e9f;
            s_LastTime = now;

            ++m_FpsFrames;
            if (m_FpsTimer >= 0.5f)
            {
                m_CurrentFps = static_cast<float>(m_FpsFrames) / m_FpsTimer;
                m_FpsFrames  = 0;
                m_FpsTimer   = 0.0f;
            }
        }

        // ===================================================================
        // 0) KARSILAMA EKRANI
        // ===================================================================
        // Proje secilene kadar editor arayuzu hic cizilmiyor. Sahneyi de
        // cizmiyoruz: hangi kokten okuyacagi henuz belli degil.
        if (m_ShowLauncher)
        {
            FX::RenderCommand::SetViewport(0, 0, GetWindow().GetWidth(),
                                           GetWindow().GetHeight());
            FX::RenderCommand::SetClearColor({ 0.05f, 0.05f, 0.06f, 1.0f });
            FX::RenderCommand::Clear();

            m_ImGuiLayer.Begin();
            DrawLauncher();
            m_ImGuiLayer.End(GetWindow().GetWidth(), GetWindow().GetHeight());

            // Modal diyaloglar cerceve DISINDA acilmali (bkz. Faz 12):
            // ImGui'nin ic durumu diyalog boyunca donar ve arkasi
            // siyah kalirdi.
            if (m_NewProjectRequested)  { m_NewProjectRequested  = false; NewProject();  }
            if (m_OpenProjectRequested) { m_OpenProjectRequested = false; OpenProject(); }

            if (!m_PendingProjectPath.empty())
            {
                const std::string path = m_PendingProjectPath;
                m_PendingProjectPath.clear();
                OpenProject(path);
            }

            return;
        }

        // ===================================================================
        // 1) SAHNEYI FRAMEBUFFER'A CIZ (ekrana degil)
        // ===================================================================
        // Viewport paneli buyumus/kucultulmusse framebuffer'i BURADA,
        // ImGui cercevesi acilmadan once yeniden olustur.
        if (m_ViewportSize.x > 0.0f && m_ViewportSize.y > 0.0f)
        {
            const auto w = static_cast<std::uint32_t>(m_ViewportSize.x);
            const auto h = static_cast<std::uint32_t>(m_ViewportSize.y);
            const auto& spec = m_Framebuffer->GetSpec();

            if (spec.Width != w || spec.Height != h)
            {
                m_Framebuffer->Resize(w, h);
            }
        }

        m_Framebuffer->Bind();

        FX::RenderCommand::SetClearColor({ 0.07f, 0.08f, 0.11f, 1.0f });
        FX::RenderCommand::Clear();

        // ID ekini -1'e doldur. glClear onu 0 yapardi ve 0 gecerli bir
        // entity kimligi - bos alana tiklayinca ilk entity secilirdi.
        m_Framebuffer->ClearAttachment(1, -1);

        FX::Renderer2D::ResetStats();

        // Play modunda SAHNENIN kamerasindan bakiliyor; oyunun gercekte
        // nasil gorunecegini ancak boyle gorursun. Sahnede isaretli bir
        // kamera yoksa editor kamerasina dusuyoruz - siyah ekran
        // gostermektense calismaya devam etmek daha faydali.
        const FX::OrthographicCamera* renderCamera = &m_EditorCamera.GetCamera();
        FX::OrthographicCamera sceneCamera{ -1.0f, 1.0f, -1.0f, 1.0f };

        if (IsPlaying())
        {
            if (FX::Entity camEntity = m_Scene->GetPrimaryCameraEntity())
            {
                const auto& cc = camEntity.GetComponent<FX::CameraComponent>();

                glm::mat4 world{ 1.0f };
                if (camEntity.HasComponent<FX::WorldTransformComponent>())
                    world = camEntity.GetComponent<FX::WorldTransformComponent>().Matrix;
                else
                    world = camEntity.GetComponent<FX::TransformComponent>().GetTransform();

                const float aspect = (m_ViewportSize.y > 0.0f)
                                   ? m_ViewportSize.x / m_ViewportSize.y : 1.0f;

                sceneCamera.SetProjectionFromAspect(aspect, cc.OrthographicSize);
                sceneCamera.SetPosition({ world[3][0], world[3][1], 0.0f });

                renderCamera = &sceneCamera;
            }
        }
        else
        {
            // Izgara sahneden ONCE: derinlik testi kapali oldugu icin sirayi
            // cizim belirliyor, yani izgara sprite'larin arkasinda kaliyor.
            DrawGrid();
        }

        m_Scene->OnRender(*renderCamera);

        // Duzenleme yardimcilari Play'de gorunmez: oyunun gercek
        // goruntusunu kirletirler.
        if (!IsPlaying())
        {
            // Kamera gizmosu PickEntity'den once: cizgileri kendi entity
            // ID'lerini ID ekine yaziyor, boylece kamera viewport'tan
            // tiklanarak secilebiliyor.
            DrawCameraGizmos();

            // Secim cercevesi sahneden SONRA: ayni sebeple her zaman ustte.
            DrawSelectionOutline();
            PickEntity();
        }

        m_Framebuffer->Unbind();

        // ===================================================================
        // 2) EKRANI TEMIZLE, ARAYUZU CIZ
        // ===================================================================
        // Viewport'u pencere boyutuna geri al: framebuffer::Bind onu
        // kendi boyutuna ayarlamisti, ImGui tum pencereye cizecek.
        FX::RenderCommand::SetViewport(0, 0, GetWindow().GetWidth(), GetWindow().GetHeight());
        FX::RenderCommand::SetClearColor({ 0.05f, 0.05f, 0.06f, 1.0f });
        FX::RenderCommand::Clear();

        m_ImGuiLayer.Begin();

        DrawMenuBar();
        DrawViewportPanel();
        DrawStatsPanel();
        m_HierarchyPanel.OnImGuiRender();
        m_ContentBrowser.OnImGuiRender();

        // Yerel dosya diyalogu isteyen tum istekler paneller cizildikten
        // SONRA ele aliniyor: diyalog modal ve programi bloklar. ImGui
        // cercevesinin ortasinda acmak, o kare boyunca ImGui'nin ic
        // durumunu dondurur ve diyalogun arkasi siyah kalir.
        if (m_ImportRequested || m_ContentBrowser.TakeImportRequest())
        {
            m_ImportRequested = false;
            ImportAssets();
        }

        if (m_NewProjectRequested)  { m_NewProjectRequested  = false; NewProject();  }
        if (m_OpenProjectRequested) { m_OpenProjectRequested = false; OpenProject(); }

        if (!m_PendingProjectPath.empty())
        {
            const std::string path = m_PendingProjectPath;
            m_PendingProjectPath.clear();
            OpenProject(path);
        }

        if (FX::Entity prefabRoot = m_HierarchyPanel.TakePrefabRequest())
        {
            const std::string absolute =
                FileDialogs::SaveFile(GetWindow(), FileDialogs::PrefabFilter());

            if (!absolute.empty())
            {
                FX::PrefabSerializer prefab(m_Scene, &m_TextureLibrary);
                const std::string rel = FX::FileSystem::MakeRelativeToProject(absolute);

                SetStatus(prefab.Save(prefabRoot, rel)
                              ? "Prefab kaydedildi: " + rel
                              : "Prefab kaydedilemedi: " + rel);
            }
        }

        if (m_ShowDemoWindow)
            ImGui::ShowDemoWindow(&m_ShowDemoWindow);

        m_ImGuiLayer.End(GetWindow().GetWidth(), GetWindow().GetHeight());
    }

    void EditorApp::DrawMenuBar()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Dosya"))
            {
                // Proje bolumu EN USTE: sahne islemleri bir projenin
                // icinde anlam kazanir, tersi degil.
                if (ImGui::MenuItem("Yeni Proje..."))
                    m_NewProjectRequested = true;
                if (ImGui::MenuItem("Proje Ac..."))
                    m_OpenProjectRequested = true;

                if (ImGui::BeginMenu("Son Projeler", !m_RecentProjects.empty()))
                {
                    std::string toOpen;
                    for (const auto& proj : m_RecentProjects)
                    {
                        if (ImGui::MenuItem(proj.c_str()))
                            toOpen = proj;
                    }

                    ImGui::Separator();
                    if (ImGui::MenuItem("Listeyi Temizle"))
                    {
                        m_RecentProjects.clear();
                        SaveEditorConfig();
                    }

                    ImGui::EndMenu();

                    if (!toOpen.empty())
                        m_PendingProjectPath = toOpen;
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Yeni Sahne", "Ctrl+N"))
                    NewScene();
                if (ImGui::MenuItem("Sahne Ac...", "Ctrl+O"))
                    OpenScene();

                if (ImGui::BeginMenu("Son Acilanlar", !m_RecentScenes.empty()))
                {
                    // Menu ogeleri uzerinde gezerken listeyi degistirmek
                    // (OpenScene -> PushRecentScene) gecersiz yineleyici
                    // demek; istegi biriktirip dongu bittikten sonra aciyoruz.
                    std::string toOpen;

                    for (const auto& scene : m_RecentScenes)
                    {
                        if (ImGui::MenuItem(scene.c_str()))
                            toOpen = scene;
                    }

                    ImGui::Separator();
                    if (ImGui::MenuItem("Listeyi Temizle"))
                    {
                        m_RecentScenes.clear();
                        SaveEditorConfig();
                    }

                    ImGui::EndMenu();

                    if (!toOpen.empty())
                        OpenScene(toOpen);
                }

                ImGui::Separator();
                if (ImGui::MenuItem("Varlik Ice Aktar...", "Ctrl+I"))
                    m_ImportRequested = true;

                ImGui::Separator();
                if (ImGui::MenuItem("Kaydet", "Ctrl+S"))
                    SaveScene();
                if (ImGui::MenuItem("Farkli Kaydet...", "Ctrl+Shift+S"))
                    SaveSceneAs();

                ImGui::Separator();
                if (ImGui::MenuItem("Cikis"))
                    Close();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Sahne"))
            {
                if (ImGui::MenuItem(m_ScenePaused ? "Devam Et" : "Duraklat"))
                    m_ScenePaused = !m_ScenePaused;

                if (ImGui::MenuItem("Entity Ekle (10)"))
                {
                    for (int i = 0; i < 10; ++i)
                        SpawnMover();
                }

                if (ImGui::MenuItem("Sahneyi Sifirla"))
                {
                    BuildScene();
                    m_HierarchyPanel.SetContext(m_Scene);
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Gizmo"))
            {
                if (ImGui::MenuItem("Kapali",  "Z", m_GizmoOperation == -1))
                    m_GizmoOperation = -1;
                if (ImGui::MenuItem("Tasi",    "X", m_GizmoOperation == ImGuizmo::TRANSLATE))
                    m_GizmoOperation = ImGuizmo::TRANSLATE;
                if (ImGui::MenuItem("Dondur",  "C", m_GizmoOperation == ImGuizmo::ROTATE))
                    m_GizmoOperation = ImGuizmo::ROTATE;
                if (ImGui::MenuItem("Olcekle", "B", m_GizmoOperation == ImGuizmo::SCALE))
                    m_GizmoOperation = ImGuizmo::SCALE;

                ImGui::Separator();

                if (ImGui::MenuItem("Yerel eksen", nullptr, m_GizmoMode == ImGuizmo::LOCAL))
                    m_GizmoMode = ImGuizmo::LOCAL;
                if (ImGui::MenuItem("Dunya ekseni", nullptr, m_GizmoMode == ImGuizmo::WORLD))
                    m_GizmoMode = ImGuizmo::WORLD;

                ImGui::Separator();
                ImGui::MenuItem("Kademeli hareket", "Ctrl", &m_SnapEnabled);
                ImGui::Separator();
                ImGui::TextDisabled("Ctrl basili = kademeli");
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Gorunum"))
            {
                ImGui::MenuItem("Izgara", "G", &m_ShowGrid);
                ImGui::MenuItem("Kamera cerceveleri", nullptr, &m_ShowCameraGizmos);
                if (ImGui::MenuItem("Secilene Odaklan", "F"))
                    FocusOnSelection();
                ImGui::Separator();

                if (ImGui::MenuItem("Kamerayi Sifirla"))
                {
                    m_EditorCamera.Reset();
                }
                if (ImGui::MenuItem("Panel Duzenini Sifirla"))
                    m_ImGuiLayer.ResetLayout();
                ImGui::MenuItem("ImGui Demo", nullptr, &m_ShowDemoWindow);
                ImGui::EndMenu();
            }

            // Kaydet/yukle geri bildirimi - birkac saniye gorunur kalir.
            // Sessiz basari, kullaniciya "oldu mu?" diye sorduran kotu
            // bir tasarimdir.
            if (m_StatusTimer > 0.0f)
            {
                ImGui::SameLine(ImGui::GetWindowWidth() - 620.0f);
                ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.6f, 1.0f), "%s",
                                   m_StatusMessage.c_str());
            }

            // Sagda durum bilgisi
            ImGui::SameLine(ImGui::GetWindowWidth() - 220.0f);
            ImGui::TextDisabled("%s | %.0f FPS",
                                m_ScenePaused ? "DURAKLATILDI" : "calisiyor",
                                m_CurrentFps);

            ImGui::EndMainMenuBar();
        }
    }

    void EditorApp::DrawViewportPanel()
    {
        // Viewport panelinde kenar boslugu ISTEMIYORUZ - goruntu
        // panelin tamamini kaplasin.
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Viewport");

        m_ViewportFocused = ImGui::IsWindowFocused();
        m_ViewportHovered = ImGui::IsWindowHovered();

        // Panelin ekran uzerindeki sinirlari. GetCursorScreenPos, icerik
        // alaninin sol UST kosesini verir - baslik cubugu haric.
        const ImVec2 contentMin = ImGui::GetCursorScreenPos();
        const ImVec2 panelSize  = ImGui::GetContentRegionAvail();

        m_ViewportBoundsMin = { contentMin.x, contentMin.y };
        m_ViewportBoundsMax = { contentMin.x + panelSize.x, contentMin.y + panelSize.y };

        // Sadece KAYDEDIYORUZ. Framebuffer'i burada yeniden olusturmak,
        // ImGui cercevesinin ortasinda texture silmek demek: ImGui o
        // kare icinde eski kimlige hala atifta bulunabiliyor ve
        // "Texture name does not refer to a texture object" hatasi aliniyor.
        // Yeniden boyutlandirma OnRender'in basinda, cerceve acilmadan once.
        if (panelSize.x > 0.0f && panelSize.y > 0.0f)
            m_ViewportSize = { panelSize.x, panelSize.y };

        // Kamera hem en-boy oranini hem ScreenToWorld'u bunlardan hesapliyor.
        m_EditorCamera.SetViewport(m_ViewportSize, m_ViewportBoundsMin, m_ViewportBoundsMax);

        // ===============================================================
        // Framebuffer texture'ini panelde goster.
        //
        // UV'LER TERS: (0,1) -> (1,0)
        // Sebep: OpenGL texture'lari SOL ALT kokenlidir, ImGui ise
        // SOL UST bekler. Duzeltmezsek sahne BAS ASAGI gorunur.
        // Faz 3'te stb_image icin yaptigimiz cevirmenin ayni mantigi,
        // bu sefer diger yonde.
        // ===============================================================
        const auto texID = static_cast<ImTextureID>(m_Framebuffer->GetColorAttachmentID(0));
        ImGui::Image(texID, panelSize, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

        // Icerik panelinden buraya birakma. Hedef, Image ogesinin hemen
        // ardindan geliyor cunku BeginDragDropTarget "son cizilen oge"
        // uzerinde calisir.
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kContentPayload))
            {
                const ImVec2 mouse = ImGui::GetMousePos();
                HandleContentDrop(static_cast<const char*>(payload->Data), mouse.x, mouse.y);
            }

            ImGui::EndDragDropTarget();
        }

        DrawGizmo();
        DrawViewportToolbar();
        // Kaydirma baslatma kosulu burada: kamera "fare viewport'ta mi"
        // veya "gizmo kullaniliyor mu" bilmemeli.
        const bool canPan = m_ViewportHovered && !ImGuizmo::IsUsing() && !ImGuizmo::IsOver();
        m_EditorCamera.OnImGuiInteract(canPan, m_ImGuiLayer.WantsKeyboard());

        ImGui::End();
        ImGui::PopStyleVar();
    }

    void EditorApp::DrawViewportToolbar()
    {
        ImGui::SetCursorScreenPos(ImVec2(m_ViewportBoundsMin.x + 8.0f,
                                         m_ViewportBoundsMin.y + 8.0f));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 6.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.09f, 0.11f, 0.85f));

        // AutoResize: icerik ne kadarsa cocuk pencere o kadar. Boyutu elle
        // hesaplamak, ileride buton eklendiginde sessizce bozulurdu.
        ImGui::BeginChild("##ViewportToolbar", ImVec2(0.0f, 0.0f),
                          ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY |
                          ImGuiChildFlags_AlwaysUseWindowPadding,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        // --- Play / Stop ---------------------------------------------------
        // Arac cubugunun EN BASINDA: editordeki en onemli tek dugme.
        {
            const bool playing = IsPlaying();

            ImGui::PushStyleColor(ImGuiCol_Button,
                playing ? ImVec4(0.65f, 0.22f, 0.22f, 1.0f)
                        : ImVec4(0.20f, 0.55f, 0.25f, 1.0f));

            if (ImGui::Button(playing ? "Stop" : "Play", ImVec2(52.0f, 0.0f)))
            {
                if (playing) OnSceneStop();
                else         OnScenePlay();
            }

            ImGui::PopStyleColor();
            ImGui::SetItemTooltip(playing
                ? "Duzenleme sahnesine don (kopyadaki degisiklikler atilir)"
                : "Sahnenin bir kopyasini calistir");

            ImGui::SameLine();

            // Duraklatma sadece Play'de anlamli.
            ImGui::BeginDisabled(!playing);
            if (ImGui::Button(m_ScenePaused ? "Devam" : "Duraklat", ImVec2(64.0f, 0.0f)))
                m_ScenePaused = !m_ScenePaused;
            ImGui::EndDisabled();

            ImGui::SameLine(0.0f, 14.0f);
        }

        // Play modunda duzenleme araclari kapali: kopyada yapilan
        // duzenleme Stop'ta zaten kaybolurdu, kullaniciya bunu
        // yaptirmak yaniltici olur.
        ImGui::BeginDisabled(IsPlaying());

        const auto toolButton = [this](const char* label, int op, const char* tip)
        {
            const bool active = (m_GizmoOperation == op);
            if (active)
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

            if (ImGui::Button(label, ImVec2(34.0f, 0.0f)))
                m_GizmoOperation = op;

            if (active)
                ImGui::PopStyleColor();

            ImGui::SetItemTooltip("%s", tip);
            ImGui::SameLine();
        };

        toolButton("Ok", -1,                  "Secim - gizmo kapali (Z)");
        toolButton("T",  ImGuizmo::TRANSLATE, "Tasi (X)");
        toolButton("R",  ImGuizmo::ROTATE,    "Dondur (C)");
        toolButton("S",  ImGuizmo::SCALE,     "Olcekle (B)");

        ImGui::SameLine(0.0f, 14.0f);

        const bool world = (m_GizmoMode == ImGuizmo::WORLD);
        if (ImGui::Button(world ? "Dunya" : "Yerel", ImVec2(52.0f, 0.0f)))
            m_GizmoMode = world ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
        ImGui::SetItemTooltip("Tutamaklarin ekseni: nesnenin kendi donusu mu,\n"
                              "dunya eksenleri mi? (Olceklemede her zaman yerel.)");

        ImGui::SameLine(0.0f, 14.0f);
        ImGui::Checkbox("Izgara", &m_ShowGrid);
        ImGui::SetItemTooltip("Izgarayi goster/gizle (G)");

        ImGui::SameLine();
        ImGui::Checkbox("Kamera", &m_ShowCameraGizmos);
        ImGui::SetItemTooltip("Kameralarin gorus alanini goster/gizle");

        ImGui::SameLine(0.0f, 14.0f);

        ImGui::Checkbox("Kademe", &m_SnapEnabled);
        ImGui::SetItemTooltip("Kapaliyken de Ctrl basili tutarak kademeli hareket edilir.");

        // Kademe degeri islem tipine gore degisir: derece ile birim ayni
        // kutuda gosterilemez.
        if (m_SnapEnabled && m_GizmoOperation >= 0)
        {
            float* value = &m_SnapTranslate;
            const char* fmt = "%.2f br";

            if (m_GizmoOperation == ImGuizmo::ROTATE)      { value = &m_SnapRotate; fmt = "%.0f°"; }
            else if (m_GizmoOperation == ImGuizmo::SCALE)  { value = &m_SnapScale;  fmt = "%.2fx"; }

            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            ImGui::DragFloat("##SnapValue", value, 0.05f, 0.01f, 90.0f, fmt);
        }

        ImGui::EndDisabled();

        // Toolbar uzerindeyken viewport "hovered" SAYILMAMALI: yoksa
        // butona tiklamak ayni zamanda arkadaki entity'yi seciyor ve
        // tekerlek zoom yapiyor. m_ViewportHovered yukarida, bu cocuk
        // pencere daha cizilmeden hesaplandigi icin burada duzeltiyoruz.
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
            m_ViewportHovered = false;

        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }

    namespace
    {
        // Gorunen yuksekligi kabaca `hedefCizgi` parcaya bolen "yuvarlak"
        // bir aralik secer: 1-2-5-10-20-50-100...
        //
        // Neden dogrudan yukseklik/50 degil? Cunku 0.37 gibi bir aralik
        // izgarayi okunamaz yapar. Insan gozu 1'in katlarini bekler;
        // zoom yaparken aralik surekli degil KADEMELI degismeli.
        float ChooseGridStep(float visibleHeight, float targetLines)
        {
            const float raw = visibleHeight / targetLines;
            if (raw <= 0.0f)
                return 1.0f;

            const float magnitude = std::pow(10.0f, std::floor(std::log10(raw)));
            const float n = raw / magnitude;

            float nice;
            if      (n < 1.5f) nice = 1.0f;
            else if (n < 3.5f) nice = 2.0f;
            else if (n < 7.5f) nice = 5.0f;
            else               nice = 10.0f;

            return nice * magnitude;
        }
    }

    void EditorApp::DrawGrid()
    {
        if (!m_ShowGrid || m_ViewportSize.y <= 0.0f)
            return;

        const float aspect        = m_ViewportSize.x / m_ViewportSize.y;
        const float visibleHeight = m_EditorCamera.GetZoom() * 2.0f;
        const float visibleWidth  = visibleHeight * aspect;

        const float step = ChooseGridStep(visibleHeight, 16.0f);

        // Gorunen alanin sinirlari. Bir adim tasma payi birakiyoruz ki
        // kaydirirken kenarda cizgi eksigi gorunmesin.
        const glm::vec3& camPos = m_EditorCamera.GetPosition();
        const float left   = camPos.x - visibleWidth  * 0.5f - step;
        const float right  = camPos.x + visibleWidth  * 0.5f + step;
        const float bottom = camPos.y - visibleHeight * 0.5f - step;
        const float top    = camPos.y + visibleHeight * 0.5f + step;

        const glm::vec4 thin { 1.0f, 1.0f, 1.0f, 0.10f };
        const glm::vec4 thick{ 1.0f, 1.0f, 1.0f, 0.22f };
        const glm::vec4 axisX{ 0.85f, 0.30f, 0.35f, 0.85f };
        const glm::vec4 axisY{ 0.40f, 0.80f, 0.40f, 0.85f };

        FX::RenderCommand::SetDepthTest(false);
        FX::Renderer2D::BeginScene(m_EditorCamera.GetCamera());

        // Her 5 adimda bir kalin cizgi: sayarken referans noktasi olmadan
        // izgara okunmaz. floor kullaniyoruz cunku negatif tarafta
        // tam sayiya dogru kesme (truncation) 0 civarinda desen bozar.
        const auto isMajor = [step](float v)
        {
            const long long k = static_cast<long long>(std::floor(v / step + 0.5f));
            return (k % 5) == 0;
        };

        const float startX = std::floor(left   / step) * step;
        const float startY = std::floor(bottom / step) * step;

        for (float x = startX; x <= right; x += step)
        {
            const glm::vec4& c = (std::fabs(x) < step * 0.25f) ? axisY
                                                               : (isMajor(x) ? thick : thin);
            FX::Renderer2D::DrawLine({ x, bottom, 0.0f }, { x, top, 0.0f }, c);
        }

        for (float y = startY; y <= top; y += step)
        {
            const glm::vec4& c = (std::fabs(y) < step * 0.25f) ? axisX
                                                               : (isMajor(y) ? thick : thin);
            FX::Renderer2D::DrawLine({ left, y, 0.0f }, { right, y, 0.0f }, c);
        }

        FX::Renderer2D::EndScene();
        FX::RenderCommand::SetDepthTest(true);
    }

    void EditorApp::DrawCameraGizmos()
    {
        if (!m_ShowCameraGizmos || m_ViewportSize.y <= 0.0f)
            return;

        auto view = m_Scene->GetRegistry().view<FX::CameraComponent>();
        if (view.begin() == view.end())
            return;

        const float aspect   = m_ViewportSize.x / m_ViewportSize.y;
        FX::Entity  selected = m_HierarchyPanel.GetSelectedEntity();

        // Ikon dunya uzayinda cizildigi icin boyutu zoom'a bagli olmali;
        // sabit birim verseydik uzaklasinca kaybolur, yakinlasinca
        // ekrani kaplardi.
        const float iconHalf = m_EditorCamera.GetZoom() * 0.035f;

        FX::RenderCommand::SetDepthTest(false);
        FX::Renderer2D::BeginScene(m_EditorCamera.GetCamera());

        for (auto handle : view)
        {
            FX::Entity cam{ handle, m_Scene };
            const auto& cc = view.get<FX::CameraComponent>(handle);

            glm::mat4 world{ 1.0f };
            if (cam.HasComponent<FX::WorldTransformComponent>())
                world = cam.GetComponent<FX::WorldTransformComponent>().Matrix;
            else
                world = cam.GetComponent<FX::TransformComponent>().GetTransform();

            const glm::vec2 pos{ world[3][0], world[3][1] };

            // Kameranin GERCEKTEN gordugu alan. Yukseklik component'ten,
            // genislik viewport'un en-boy oranindan - Play'de kullanilan
            // hesabin aynisi, yoksa cerceve yalan soylerdi.
            const float halfH = cc.OrthographicSize;
            const float halfW = halfH * aspect;

            const bool isSelected = (selected && selected == cam);

            glm::vec4 color = cc.Primary ? glm::vec4{ 0.95f, 0.85f, 0.25f, 0.55f }
                                         : glm::vec4{ 0.55f, 0.60f, 0.70f, 0.40f };
            if (isSelected)
                color.a = 1.0f;

            // Gorus dikdortgeni. Entity ID veriyoruz: cizgiye tiklayinca
            // kamera secilebiliyor. Kameranin sprite'i yok, yoksa
            // viewport'tan hic secilemezdi.
            const int id = static_cast<int>(handle);

            const glm::vec3 c0{ pos.x - halfW, pos.y - halfH, 0.0f };
            const glm::vec3 c1{ pos.x + halfW, pos.y - halfH, 0.0f };
            const glm::vec3 c2{ pos.x + halfW, pos.y + halfH, 0.0f };
            const glm::vec3 c3{ pos.x - halfW, pos.y + halfH, 0.0f };

            FX::Renderer2D::DrawLine(c0, c1, color, id);
            FX::Renderer2D::DrawLine(c1, c2, color, id);
            FX::Renderer2D::DrawLine(c2, c3, color, id);
            FX::Renderer2D::DrawLine(c3, c0, color, id);

            // Kameranin KENDI konumunu gosteren ikon: govde + one bakan
            // huni. Cerceve merkezi bos kalsaydi kameranin nerede
            // durdugu (ve hangi cercevenin ona ait oldugu) belirsiz olurdu.
            glm::vec4 iconColor = color;
            iconColor.a = isSelected ? 1.0f : 0.8f;

            const float h = iconHalf;
            const glm::vec3 b0{ pos.x - h,        pos.y - h * 0.7f, 0.0f };
            const glm::vec3 b1{ pos.x + h * 0.3f, pos.y - h * 0.7f, 0.0f };
            const glm::vec3 b2{ pos.x + h * 0.3f, pos.y + h * 0.7f, 0.0f };
            const glm::vec3 b3{ pos.x - h,        pos.y + h * 0.7f, 0.0f };

            FX::Renderer2D::DrawLine(b0, b1, iconColor, id);
            FX::Renderer2D::DrawLine(b1, b2, iconColor, id);
            FX::Renderer2D::DrawLine(b2, b3, iconColor, id);
            FX::Renderer2D::DrawLine(b3, b0, iconColor, id);

            // Huni: saga (+X) bakan ucgen. Yon, kameranin baktigi tarafi
            // degil (ortografik 2D'de hep -Z'ye bakar) sadece ikonun
            // "on" tarafini gosteriyor.
            const glm::vec3 l0{ pos.x + h * 0.3f, pos.y - h * 0.45f, 0.0f };
            const glm::vec3 l1{ pos.x + h * 1.1f, pos.y - h * 0.9f,  0.0f };
            const glm::vec3 l2{ pos.x + h * 1.1f, pos.y + h * 0.9f,  0.0f };
            const glm::vec3 l3{ pos.x + h * 0.3f, pos.y + h * 0.45f, 0.0f };

            FX::Renderer2D::DrawLine(l0, l1, iconColor, id);
            FX::Renderer2D::DrawLine(l1, l2, iconColor, id);
            FX::Renderer2D::DrawLine(l2, l3, iconColor, id);
        }

        FX::Renderer2D::EndScene();
        FX::RenderCommand::SetDepthTest(true);
    }

    void EditorApp::DrawSelectionOutline()
    {
        FX::Entity selected = m_HierarchyPanel.GetSelectedEntity();
        if (!selected || !selected.HasComponent<FX::TransformComponent>())
            return;

        // Dunya matrisi: parent zinciri zaten uygulanmis halde (Faz 9).
        // Yerel transform'u kullansaydik cocuk entity'lerin cercevesi
        // yanlis yerde cikardi.
        glm::mat4 world{ 1.0f };
        if (selected.HasComponent<FX::WorldTransformComponent>())
            world = selected.GetComponent<FX::WorldTransformComponent>().Matrix;
        else
            world = selected.GetComponent<FX::TransformComponent>().GetTransform();

        FX::RenderCommand::SetDepthTest(false);
        FX::Renderer2D::SetLineWidth(2.0f);
        FX::Renderer2D::BeginScene(m_EditorCamera.GetCamera());

        FX::Renderer2D::DrawRect(world, { 1.0f, 0.55f, 0.15f, 1.0f });

        FX::Renderer2D::EndScene();
        FX::Renderer2D::SetLineWidth(1.0f);
        FX::RenderCommand::SetDepthTest(true);
    }

    void EditorApp::FocusOnSelection()
    {
        FX::Entity selected = m_HierarchyPanel.GetSelectedEntity();
        if (!selected || !selected.HasComponent<FX::TransformComponent>())
        {
            SetStatus("Odaklanacak entity secili degil.");
            return;
        }

        glm::mat4 world{ 1.0f };
        if (selected.HasComponent<FX::WorldTransformComponent>())
            world = selected.GetComponent<FX::WorldTransformComponent>().Matrix;
        else
            world = selected.GetComponent<FX::TransformComponent>().GetTransform();

        // Matrisin 4. sutunu konumdur; ayristirmaya gerek yok.
        const glm::vec2 target{ world[3][0], world[3][1] };

        // Nesneyi ekrana sigdir: dunya uzayindaki genisligini olceginden
        // okuyup biraz pay birakiyoruz. Tam sinirina zoom yapmak nesneyi
        // ekranin kenarina yapistirirdi.
        const float scaleX = glm::length(glm::vec3(world[0]));
        const float scaleY = glm::length(glm::vec3(world[1]));
        const float extent = std::max(scaleX, scaleY);

        m_EditorCamera.FocusOn(target, extent);

        SetStatus("Odaklanildi: " + selected.GetComponent<FX::TagComponent>().Tag);
    }

    void EditorApp::PickEntity()
    {
        // Framebuffer BAGLI durumdayken cagriliyor (OnRender icinde).
        if (!m_ViewportHovered || ImGuizmo::IsOver() || ImGuizmo::IsUsing())
            return;

        // Space + sol tus kamerayi kaydiriyor, secim yapmiyor.
        if (m_EditorCamera.IsPanning() || ImGui::IsKeyDown(ImGuiKey_Space))
            return;

        if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            return;

        const ImVec2 mouse = ImGui::GetMousePos();

        float mx = mouse.x - m_ViewportBoundsMin.x;
        float my = mouse.y - m_ViewportBoundsMin.y;

        // OpenGL'in Y ekseni asagidan yukari; ImGui'nin yukaridan asagi.
        const float height = m_ViewportBoundsMax.y - m_ViewportBoundsMin.y;
        my = height - my;

        const int pixel = m_Framebuffer->ReadPixel(1, static_cast<int>(mx), static_cast<int>(my));

        if (pixel < 0)
        {
            m_HierarchyPanel.SetSelectedEntity({});
            return;
        }

        const auto handle = static_cast<entt::entity>(pixel);
        if (m_Scene->GetRegistry().valid(handle))
        {
            FX::Entity picked{ handle, m_Scene };
            m_HierarchyPanel.SetSelectedEntity(picked);
            FX_INFO("Secildi: %s", picked.GetComponent<FX::TagComponent>().Tag.c_str());
        }
    }

    void EditorApp::DrawGizmo()
    {
        FX::Entity selected = m_HierarchyPanel.GetSelectedEntity();
        if (IsPlaying() || !selected || m_GizmoOperation < 0)
            return;
        if (!selected.HasComponent<FX::TransformComponent>())
            return;

        ImGuizmo::SetOrthographic(true);
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(m_ViewportBoundsMin.x, m_ViewportBoundsMin.y,
                          m_ViewportBoundsMax.x - m_ViewportBoundsMin.x,
                          m_ViewportBoundsMax.y - m_ViewportBoundsMin.y);

        const glm::mat4& view = m_EditorCamera.GetCamera().GetViewMatrix();
        const glm::mat4& proj = m_EditorCamera.GetCamera().GetProjectionMatrix();

        auto& tc = selected.GetComponent<FX::TransformComponent>();

        // Gizmo DUNYA uzayinda calisir. Parent'i olan bir entity'nin
        // yerel matrisini verirsek tutamaklar yanlis yerde cikar.
        glm::mat4 parentWorld{ 1.0f };
        if (FX::Entity parent = selected.GetParent())
        {
            if (parent.HasComponent<FX::WorldTransformComponent>())
                parentWorld = parent.GetComponent<FX::WorldTransformComponent>().Matrix;
        }

        glm::mat4 transform = parentWorld * tc.GetTransform();

        // Kademe ya toolbar'dan surekli acik, ya da Ctrl ile anlik.
        const bool* keys = SDL_GetKeyboardState(nullptr);
        const bool snap  = m_SnapEnabled ||
                           keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL];

        float snapValue = m_SnapTranslate;
        if (m_GizmoOperation == ImGuizmo::ROTATE)     snapValue = m_SnapRotate;
        else if (m_GizmoOperation == ImGuizmo::SCALE) snapValue = m_SnapScale;

        const float snapValues[3] = { snapValue, snapValue, snapValue };

        // Olcekleme dunya uzayinda anlamsiz; ImGuizmo zaten yerele zorlar.
        const auto mode = (m_GizmoOperation == ImGuizmo::SCALE)
                        ? ImGuizmo::LOCAL
                        : static_cast<ImGuizmo::MODE>(m_GizmoMode);

        ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
                             static_cast<ImGuizmo::OPERATION>(m_GizmoOperation),
                             mode,
                             glm::value_ptr(transform),
                             nullptr,
                             snap ? snapValues : nullptr);

        if (ImGuizmo::IsUsing())
        {
            // Dunya -> yerel: parent'in tersiyle carp.
            const glm::mat4 local = glm::inverse(parentWorld) * transform;

            float translation[3], rotation[3], scale[3];
            ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(local),
                                                  translation, rotation, scale);

            tc.Translation = { translation[0], translation[1], translation[2] };
            tc.Rotation    = glm::radians(rotation[2]);
            tc.Scale       = { scale[0], scale[1] };
        }
    }

    void EditorApp::DrawStatsPanel()
    {
        ImGui::Begin("Istatistikler");

        const auto stats = FX::Renderer2D::GetStats();

        ImGui::Text("Render");
        ImGui::Separator();
        ImGui::Text("Draw call   : %u", stats.DrawCalls);
        ImGui::Text("Quad        : %u", stats.QuadCount);
        ImGui::Text("Cizgi       : %u", stats.LineCount);
        ImGui::Text("Kose        : %u", stats.GetVertexCount());
        ImGui::Text("FPS         : %.0f", m_CurrentFps);

        ImGui::Spacing();
        ImGui::Text("Proje");
        ImGui::Separator();
        if (auto project = FX::Project::GetActive())
        {
            ImGui::Text("Ad          : %s", project->GetConfig().Name.c_str());
            ImGui::TextWrapped("Kok: %s", project->GetDirectory().c_str());
        }
        else
        {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "Acik proje yok");
            ImGui::TextWrapped("Varliklar exe klasorunde. Kalici calisma icin "
                               "Dosya > Yeni Proje.");
        }
        ImGui::Spacing();

        ImGui::Text("Sahne");
        ImGui::Separator();
        ImGui::Text("Entity      : %u", m_Scene->GetEntityCount());
        ImGui::Text("Mod         : %s", IsPlaying() ? "Play (kopya)" : "Edit");
        ImGui::Text("Durum       : %s", m_ScenePaused ? "duraklatildi" : "calisiyor");

        ImGui::Spacing();
        ImGui::Text("Viewport");
        ImGui::Separator();
        ImGui::Text("Boyut       : %.0f x %.0f", m_ViewportSize.x, m_ViewportSize.y);
        ImGui::Text("Pencere     : %u x %u", GetWindow().GetWidth(), GetWindow().GetHeight());
        ImGui::Text("Odakta      : %s", m_ViewportFocused ? "evet" : "hayir");

        ImGui::Spacing();
        ImGui::Text("Kamera");
        ImGui::Separator();
        ImGui::Text("Konum       : (%.2f, %.2f)",
                    m_EditorCamera.GetPosition().x, m_EditorCamera.GetPosition().y);
        ImGui::SetNextItemWidth(-1.0f);
        float zoom = m_EditorCamera.GetZoom();
        if (ImGui::SliderFloat("##Zoom", &zoom,
                               FXEd::EditorCamera::kMinZoom, FXEd::EditorCamera::kMaxZoom,
                               "Zoom %.1f"))
            m_EditorCamera.SetZoom(zoom);

        ImGui::Spacing();
        bool vsync = GetWindow().IsVSync();
        if (ImGui::Checkbox("VSync", &vsync))
            GetWindow().SetVSync(vsync);

        ImGui::End();
    }

    void EditorApp::OnEvent(const SDL_Event& event)
    {
        // ISLETIM SISTEMINDEN SURUKLE-BIRAK.
        // ImGui'den ONCE bakiyoruz: bu bir OS olayi, ImGui'nin fare
        // durumuyla ilgisi yok ve ImGui onu tuketmez. Native surukleme
        // sirasinda ImGui'nin fare konumu guncellenmez, bu yuzden
        // koordinati olayin KENDISINDEN aliyoruz.
        if (event.type == SDL_EVENT_DROP_FILE)
        {
            if (event.drop.data)
                HandleExternalDrop(event.drop.data, event.drop.x, event.drop.y);
            return;
        }

        // TEKERLEK, WantCaptureMouse'a TABI DEGIL.
        //
        // Viewport'un kendisi de bir ImGui penceresi oldugu icin fare
        // uzerindeyken WantCaptureMouse HEP true doner. Asagidaki genel
        // kontrole biraksaydik zoom hicbir zaman calismazdi - nitekim
        // calismiyordu da.
        //
        // ImGui olayi yine gorsun (ic durumu guncellensin) ama karari biz
        // verelim: olcut "ImGui fareyi istiyor mu" degil, "fare viewport'ta
        // mi". Diger panellerde tekerlek kaydirmaya kaliyor.
        if (event.type == SDL_EVENT_MOUSE_WHEEL)
        {
            m_ImGuiLayer.OnEvent(event);

            // Koordinat olayin kendisinden: SDL pencere-goreli verir,
            // tek viewport'ta bu ImGui'nin ekran koordinatiyla ayni.
            if (m_ViewportHovered)
                m_EditorCamera.OnMouseScroll(event.wheel.y,
                                             event.wheel.mouse_x, event.wheel.mouse_y);
            return;
        }

        // ImGui once gorsun. true donerse olayi tuketmis demektir.
        if (m_ImGuiLayer.OnEvent(event))
            return;

        if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat)
            return;

        // Ctrl basili mi? SDL3'te modifier durumu event.key.mod'da gelir.
        // KMOD_CTRL, sol ve sag Ctrl'un ikisini birden kapsar.
        const bool ctrl  = (event.key.mod & SDL_KMOD_CTRL)  != 0;
        const bool shift = (event.key.mod & SDL_KMOD_SHIFT) != 0;

        switch (event.key.key)
        {
            case SDLK_ESCAPE:
                Close();
                break;

            case SDLK_SPACE:
                m_ScenePaused = !m_ScenePaused;
                break;

            case SDLK_N:
                if (ctrl) NewScene();
                break;

            case SDLK_S:
                if (ctrl && shift) SaveSceneAs();
                else if (ctrl)     SaveScene();
                break;

            case SDLK_O:
                if (ctrl) OpenScene();
                break;

            case SDLK_I:
                // Kisayol ImGui cercevesinin DISINDA isleniyor (OnEvent),
                // bu yuzden diyalogu dogrudan acmak guvenli.
                if (ctrl) ImportAssets();
                break;

            // Gizmo kisayollari - sadece viewport odaktayken, cunku
            // W/E/R ayni zamanda kamera tuslari.
            case SDLK_F:
                if (m_ViewportHovered) FocusOnSelection();
                break;

            case SDLK_G:
                if (m_ViewportHovered) m_ShowGrid = !m_ShowGrid;
                break;

            case SDLK_Z:
                if (m_ViewportHovered) m_GizmoOperation = -1;
                break;
            case SDLK_X:
                if (m_ViewportHovered) m_GizmoOperation = ImGuizmo::TRANSLATE;
                break;
            case SDLK_C:
                if (m_ViewportHovered) m_GizmoOperation = ImGuizmo::ROTATE;
                break;
            case SDLK_B:
                if (m_ViewportHovered) m_GizmoOperation = ImGuizmo::SCALE;
                break;

            default:
                break;
        }
    }

    void EditorApp::OnShutdown()
    {
        SaveEditorConfig();

        // SIRA ONEMLI: once ImGui (GL kaynaklarini birakir),
        // sonra framebuffer ve sahne, en son renderer.
        m_ImGuiLayer.Shutdown();
        m_Framebuffer.reset();

        // m_Scene sadece isaretci; sahiplik bu ikisinde.
        m_Scene = nullptr;
        m_RuntimeScene.reset();
        m_EditorScene.reset();

        FX::Renderer2D::Shutdown();

        FX_INFO("Editor kapaniyor.");
    }
}
