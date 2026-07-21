#include "EditorApp.h"
#include "Platform/FileDialogs.h"
#include "Panels/ComponentDrawer.h"
#include "Scripts/SpinScript.h"

#include <FXEngine/Core/Log.h>
#include <FXEngine/Core/FileSystem.h>
#include <FXEngine/Renderer/RenderCommand.h>
#include <FXEngine/Renderer/Renderer2D.h>
#include <FXEngine/Scene/Components.h>
#include <FXEngine/Scene/PrefabSerializer.h>

#include <imgui.h>
#include <ImGuizmo.h>

#include <SDL3/SDL.h>

#include <memory>
#include <string>

namespace FXEd
{
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

        // Game View'da secim yok, dolayisiyla entity ID eki de yok:
        // her karede bos yere bir R32I tampon temizlemeyelim.
        FX::FramebufferSpec gameSpec = fbSpec;
        gameSpec.Attachments = {
            FX::FramebufferTextureFormat::RGBA8,
            FX::FramebufferTextureFormat::DEPTH24STENCIL8
        };
        m_GameFramebuffer = std::make_unique<FX::Framebuffer>(gameSpec);

        // Script'ler her seyden once kayitli olmali: sahne yuklemesi
        // (StartScene) adlari cozmeye calisacak.
        RegisterEditorScripts();

        m_HierarchyPanel.SetSelection(&m_Selection);
        m_ContentBrowser.SetContext(&m_TextureLibrary);

        // Component meta tablosuna editore ozgu cizicileri bagla (A1):
        // doku slotu, kamera tekilligi, script listesi. Motor bunlari
        // bilmez; tablo yalnizca tasiyicidir.
        ComponentDrawer::RegisterEditorUI(&m_TextureLibrary);

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

    void EditorApp::SetStatus(const std::string& message)
    {
        m_StatusMessage = message;
        m_StatusTimer   = 4.0f;
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
        m_Selection.Clear();

        m_ScenePaused = false;

        // Script'ler burada dogar (Faz 16a). Kopya sahnede yasarlar,
        // yani Stop'ta yaptiklari her sey kopyayla birlikte yok olur.
        m_Scene->OnRuntimeStart();

        if (FX::Entity cam = m_Scene->GetPrimaryCameraEntity())
        {
            FX_INFO("Play: sahne kamerasi '%s' (size=%.1f)", cam.GetName().c_str(),
                    cam.GetComponent<FX::CameraComponent>().OrthographicSize);
        }
        else
        {
            FX_WARN("Play: sahnede isaretli kamera yok, editor kamerasi kullanilacak.");
        }

        // Oyunu gormek icin sekme degistirmek zorunda kalmayalim.
        m_FocusGameView = true;

        SetStatus("Play - oyun kopya sahnede calisiyor");
    }

    void EditorApp::OnSceneStop()
    {
        if (!IsPlaying())
            return;

        // Once script'leri durdur, SONRA kopyayi at: OnDestroy'un hala
        // gecerli bir sahne gormesi gerekiyor.
        m_RuntimeScene->OnRuntimeStop();

        m_Scene      = m_EditorScene.get();
        m_SceneState = SceneState::Edit;
        m_RuntimeScene.reset();   // kopyada olan biten burada kayboluyor

        m_HierarchyPanel.SetContext(m_Scene);
        m_Selection.Clear();

        m_FocusSceneView = true;

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

            ProcessProjectRequests();
            return;
        }

        RenderSceneView();
        RenderGameView();

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
        DrawScenePanel();
        DrawGamePanel();
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

        if (const std::string toOpen = m_ContentBrowser.TakeOpenRequest(); !toOpen.empty())
            OpenAsset(toOpen);

        ProcessProjectRequests();

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

    void EditorApp::OnRawEvent(const SDL_Event& event)
    {
        // Burada SADECE SDL_Event isteyen isler kaldi (Faz 13b): ImGui
        // backend'i ve isletim sistemi surukle-birakma. Kisayollar ve
        // kamera artik motor olaylarini kullaniyor - OnEvent'e bak.

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

        // ImGui her olayi gormeli (ic durumunu guncelliyor). Tuketip
        // tuketmedigini motora bildiriyoruz; tukettiyse ayni girdi
        // OnEvent'te ikinci kez islenmez.
        const bool consumed = m_ImGuiLayer.OnEvent(event);

        // TEKERLEK, WantCaptureMouse'a TABI DEGIL.
        //
        // Viewport'un kendisi de bir ImGui penceresi oldugu icin fare
        // uzerindeyken WantCaptureMouse HEP true doner. Genel kontrole
        // biraksaydik zoom hicbir zaman calismazdi - nitekim calismiyordu
        // da. Olcut "ImGui fareyi istiyor mu" degil, "fare viewport'ta mi".
        if (event.type == SDL_EVENT_MOUSE_WHEEL)
            return;

        if (consumed)
            MarkRawEventHandled();
    }

    void EditorApp::OnEvent(FX::Event& event)
    {
        FX::EventDispatcher dispatcher(event);

        dispatcher.Dispatch<FX::MouseScrolledEvent>(
            [this](FX::MouseScrolledEvent& e)
            {
                if (!m_ViewportHovered)
                    return false;

                m_EditorCamera.OnMouseScroll(e.GetYOffset(), e.GetMouseX(), e.GetMouseY());
                return true;
            });

        dispatcher.Dispatch<FX::KeyPressedEvent>(
            [this](FX::KeyPressedEvent& e) { return OnKeyPressed(e); });
    }

    bool EditorApp::OnKeyPressed(FX::KeyPressedEvent& e)
    {
        // Basili tutmak kisayolu tekrarlamamali: Ctrl+S'i basili tutmak
        // on kez kaydetmesin.
        if (e.IsRepeat())
            return false;

        const bool ctrl  = e.GetMods().Ctrl;
        const bool shift = e.GetMods().Shift;

        switch (e.GetKey())
        {
            case FX::Key::Escape:
                Close();
                return true;

            case FX::Key::Space:
                m_ScenePaused = !m_ScenePaused;
                return true;

            case FX::Key::N:
                if (ctrl) { NewScene(); return true; }
                return false;

            case FX::Key::S:
                if (ctrl && shift) { SaveSceneAs(); return true; }
                if (ctrl)          { SaveScene();   return true; }
                return false;

            case FX::Key::O:
                if (ctrl) { OpenScene(); return true; }
                return false;

            case FX::Key::I:
                // Kisayol ImGui cercevesinin DISINDA isleniyor, bu yuzden
                // diyalogu dogrudan acmak guvenli.
                if (ctrl) { ImportAssets(); return true; }
                return false;

            case FX::Key::Delete:
                if (m_ViewportHovered) { DeleteSelection(); return true; }
                return false;

            // Gizmo kisayollari - sadece viewport odaktayken, cunku
            // W/E/R ayni zamanda kamera tuslari.
            case FX::Key::F:
                if (m_ViewportHovered) { FocusOnSelection(); return true; }
                return false;

            case FX::Key::G:
                if (m_ViewportHovered) { m_ShowGrid = !m_ShowGrid; return true; }
                return false;

            case FX::Key::Z:
                if (m_ViewportHovered) { m_GizmoOperation = -1; return true; }
                return false;
            case FX::Key::X:
                if (m_ViewportHovered) { m_GizmoOperation = ImGuizmo::TRANSLATE; return true; }
                return false;
            case FX::Key::C:
                if (m_ViewportHovered) { m_GizmoOperation = ImGuizmo::ROTATE; return true; }
                return false;
            case FX::Key::B:
                if (m_ViewportHovered) { m_GizmoOperation = ImGuizmo::SCALE; return true; }
                return false;

            default:
                return false;
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
