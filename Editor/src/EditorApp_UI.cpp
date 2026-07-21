#include "EditorApp.h"
#include "SampleScene.h"

#include <FXEngine/Core/Log.h>
#include <FXEngine/Core/Project.h>
#include <FXEngine/Renderer/Renderer2D.h>
#include <FXEngine/Scene/Components.h>

#include <imgui.h>
#include <imgui_internal.h>   // BeginViewportSideBar
#include <ImGuizmo.h>

#include <memory>
#include <string>

namespace FXEd
{
    // ImGui panelleri: menu cubugu, viewport paneli, arac cubugu,
    // istatistikler. (EditorApp'in bolunmus tanimlari)

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
                        SampleScene::SpawnMover(*m_Scene, m_Circle);
                }

                if (ImGui::MenuItem("Sahneyi Sifirla"))
                {
                    m_EditorScene = std::make_unique<FX::Scene>();
                    m_Scene       = m_EditorScene.get();
                    SampleScene::Build(*m_Scene, m_Checkerboard, m_Circle);
                    m_HierarchyPanel.SetContext(m_Scene);
                    m_Selection.Clear();
                }

                ImGui::Separator();

                // Baslangic sahnesi .fxproject'te GUID olarak duruyor;
                // once bu ayari yapacak bir yer yoktu, dosyayi elle
                // duzenlemek gerekiyordu.
                const bool canSetStart =
                    FX::Project::HasActive() && !m_ScenePath.empty();

                if (ImGui::MenuItem("Baslangic Sahnesi Yap", nullptr, false, canSetStart))
                    SetAsStartScene();

                if (!canSetStart)
                {
                    ImGui::TextDisabled(FX::Project::HasActive()
                                            ? "  (once sahneyi kaydet)"
                                            : "  (once bir proje ac)");
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

            // Sagda yalnizca FPS. Play/Edit durumu oynatma seridinde:
            // burada "calisiyor" yaziyordu ama Edit modunda sahne
            // duragan - yanlis bilgiydi.
            ImGui::SameLine(ImGui::GetWindowWidth() - 220.0f);
            ImGui::TextDisabled("%.0f FPS", m_CurrentFps);

            ImGui::EndMainMenuBar();
        }
    }

    // Play/Stop editordeki en onemli dugme ve bir GORUNUME degil
    // editorun durumuna ait. Scene panelinin icinde durdugu surece
    // Game sekmesine gecince kayboluyordu; artik menu cubugunun
    // altinda, her zaman gorunur bir seritte.
    void EditorApp::DrawPlayBar()
    {
        const float height = ImGui::GetFrameHeight() + 10.0f;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 5.0f));

        // Dockspace'ten ONCE cagriliyor (bkz. ImGuiLayer::BeginDockspace):
        // calisma alanindan kendi payini ayirmasi gerekiyor.
        const bool open = ImGui::BeginViewportSideBar(
            "##PlayBar", ImGui::GetMainViewport(), ImGuiDir_Up, height,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoScrollbar);

        ImGui::PopStyleVar();

        if (!open)
        {
            ImGui::End();
            return;
        }

        const bool playing = IsPlaying();

        // Dugmeleri ortala: goz once oraya gidiyor ve pencere
        // genisligi degistikce yeri kaymiyor.
        const float groupWidth = 52.0f + 8.0f + 64.0f;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - groupWidth) * 0.5f);

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

        // Play'de hangi sahnede oldugun solda yaziyor: kopya mi orijinal mi
        // sorusu Play sirasinda en sik sorulan sey.
        ImGui::SameLine(12.0f);
        if (!playing)
            ImGui::TextDisabled("Edit");
        else if (m_ScenePaused)
            ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.35f, 1.0f), "DURAKLATILDI");
        else
            ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.25f, 1.0f), "PLAY (kopya sahne)");

        ImGui::End();
    }

    void EditorApp::DrawScenePanel()
    {
        // Panelde kenar boslugu ISTEMIYORUZ - goruntu tamamini kaplasin.
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        // Stop'ta Scene one gelsin. Begin'den ONCE cagriliyor: ImGui
        // pencere adiyla calisiyor ve pencere henuz acilmadi.
        if (m_FocusSceneView)
        {
            m_FocusSceneView = false;
            ImGui::SetWindowFocus("Scene");
        }

        ImGui::Begin("Scene");

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

    void EditorApp::DrawGamePanel()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        if (m_FocusGameView)
        {
            m_FocusGameView = false;
            ImGui::SetWindowFocus("Game");
        }

        ImGui::Begin("Game");

        const ImVec2 panelSize = ImGui::GetContentRegionAvail();

        // Sadece kaydediyoruz; yeniden boyutlandirma RenderGameView'de,
        // ImGui cercevesi acilmadan once (bkz. DrawScenePanel).
        if (panelSize.x > 0.0f && panelSize.y > 0.0f)
            m_GameViewportSize = { panelSize.x, panelSize.y };

        if (m_Scene->GetPrimaryCameraEntity())
        {
            // UV'ler ters: OpenGL sol-alt kokenli, ImGui sol-ust bekler.
            const auto texID =
                static_cast<ImTextureID>(m_GameFramebuffer->GetColorAttachmentID(0));
            ImGui::Image(texID, panelSize, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
        }
        else
        {
            // Sessizce siyah ekran gostermek "oyun neden gorunmuyor?"
            // sorusunu doguruyordu. Sebebi soyluyoruz.
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f),
                               "Sahnede birincil kamera yok.");
            ImGui::TextDisabled("Bir entity'ye Camera component'i ekleyip");
            ImGui::TextDisabled("\"Birincil\" isaretle.");
        }

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

}
