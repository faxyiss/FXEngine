#include "EditorApp.h"

#include <FXEngine/Core/Log.h>
#include <FXEngine/Core/Input.h>
#include <FXEngine/Renderer/RenderCommand.h>
#include <FXEngine/Renderer/Renderer2D.h>
#include <FXEngine/Scene/Components.h>

#include <imgui.h>
#include <ImGuizmo.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace FXEd
{
    // Viewport ustundeki cizim ve etkilesim: izgara, kamera gizmolari,
    // secim cercevesi, entity secme, ImGuizmo. (bolunmus tanimlar)

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

    // Scene View: EDITOR kamerasindan, duzenleme yardimcilariyla.
    // Play sirasinda da calisiyor - oyunu disaridan izleyip hata
    // ayiklamak icin (Unity davranisi).
    void EditorApp::RenderSceneView()
    {
        // Panel boyutu degistiyse framebuffer'i BURADA, ImGui cercevesi
        // acilmadan once yeniden olustur: cerceve ortasinda texture
        // silmek ImGui'yi gecersiz kimlige birakiyor.
        if (m_ViewportSize.x > 0.0f && m_ViewportSize.y > 0.0f)
        {
            const auto w = static_cast<std::uint32_t>(m_ViewportSize.x);
            const auto h = static_cast<std::uint32_t>(m_ViewportSize.y);
            const auto& spec = m_Framebuffer->GetSpec();

            if (spec.Width != w || spec.Height != h)
                m_Framebuffer->Resize(w, h);
        }

        m_Framebuffer->Bind();

        FX::RenderCommand::SetClearColor({ 0.07f, 0.08f, 0.11f, 1.0f });
        FX::RenderCommand::Clear();

        // ID ekini -1'e doldur. glClear onu 0 yapardi ve 0 gecerli bir
        // entity kimligi - bos alana tiklayinca ilk entity secilirdi.
        m_Framebuffer->ClearAttachment(1, -1);

        FX::Renderer2D::ResetStats();

        // Izgara sahneden ONCE: derinlik testi kapali oldugu icin sirayi
        // cizim belirliyor, yani izgara sprite'larin arkasinda kaliyor.
        DrawGrid();

        m_Scene->OnRender(m_EditorCamera.GetCamera());

        // Kamera gizmosu PickEntity'den once: cizgileri kendi entity
        // ID'lerini ID ekine yaziyor, boylece kamera viewport'tan
        // tiklanarak secilebiliyor.
        DrawCameraGizmos();

        // Secim cercevesi sahneden SONRA: ayni sebeple her zaman ustte.
        DrawSelectionOutline();
        PickEntity();

        m_Framebuffer->Unbind();
    }

    // Game View: SAHNE kamerasindan, hicbir duzenleme yardimcisi yok.
    // "Hangi kamera ciziyor?" sorusunun cevabi artik tek: bu panel her
    // zaman sahnenin birincil kamerasini gosterir.
    void EditorApp::RenderGameView()
    {
        if (m_GameViewportSize.x <= 0.0f || m_GameViewportSize.y <= 0.0f)
            return;

        const auto w = static_cast<std::uint32_t>(m_GameViewportSize.x);
        const auto h = static_cast<std::uint32_t>(m_GameViewportSize.y);

        if (const auto& spec = m_GameFramebuffer->GetSpec(); spec.Width != w || spec.Height != h)
            m_GameFramebuffer->Resize(w, h);

        m_GameFramebuffer->Bind();

        FX::RenderCommand::SetClearColor({ 0.0f, 0.0f, 0.0f, 1.0f });
        FX::RenderCommand::Clear();

        FX::Entity camEntity = m_Scene->GetPrimaryCameraEntity();
        if (!camEntity)
        {
            // Kamera yoksa siyah kaliyor. Editor kamerasina DUSMUYORUZ:
            // "kameran yok" gercegini gizlemek, oyunu calistirinca
            // bos ekran gormenin sebebini de gizlerdi. Panel bunun
            // yerine gorunur bir uyari yaziyor.
            m_GameFramebuffer->Unbind();
            return;
        }

        const auto& cc = camEntity.GetComponent<FX::CameraComponent>();

        const glm::mat4 world = camEntity.HasComponent<FX::WorldTransformComponent>()
            ? camEntity.GetComponent<FX::WorldTransformComponent>().Matrix
            : camEntity.GetComponent<FX::TransformComponent>().GetTransform();

        // En-boy orani CIZIM YUZEYINDEN geliyor, sahneden degil: ayni
        // sahne farkli pencere boyutlarinda acilabilir (bkz.
        // CameraComponent). Yuzeyin boyutuna DrawGamePanel karar veriyor;
        // oran kilitliyken panele degil hedef orana gore hesapliyor,
        // artan yer siyah bant olarak kaliyor.
        const float aspect = m_GameViewportSize.x / m_GameViewportSize.y;

        FX::OrthographicCamera camera{ -1.0f, 1.0f, -1.0f, 1.0f };
        camera.SetProjectionFromAspect(aspect, cc.OrthographicSize);
        camera.SetPosition({ world[3][0], world[3][1], 0.0f });

        m_Scene->OnRender(camera);

        m_GameFramebuffer->Unbind();
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
        FX::Entity  selected = m_Selection.GetPrimary();

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
        if (m_Selection.IsEmpty())
            return;

        FX::RenderCommand::SetDepthTest(false);
        FX::Renderer2D::SetLineWidth(2.0f);
        FX::Renderer2D::BeginScene(m_EditorCamera.GetCamera());

        const FX::Entity primary = m_Selection.GetPrimary();

        for (FX::Entity selected : m_Selection.GetAll())
        {
            if (!selected.HasComponent<FX::TransformComponent>())
                continue;

            // Dunya matrisi: parent zinciri zaten uygulanmis halde (Faz 9).
            // Yerel transform'u kullansaydik cocuk entity'lerin cercevesi
            // yanlis yerde cikardi.
            glm::mat4 world{ 1.0f };
            if (selected.HasComponent<FX::WorldTransformComponent>())
                world = selected.GetComponent<FX::WorldTransformComponent>().Matrix;
            else
                world = selected.GetComponent<FX::TransformComponent>().GetTransform();

            // Birincil turuncu, digerleri soluk: gizmonun hangi entity
            // uzerinde durdugu tek bakista belli olsun.
            const glm::vec4 color = (selected == primary)
                                        ? glm::vec4{ 1.0f, 0.55f, 0.15f, 1.0f }
                                        : glm::vec4{ 1.0f, 0.55f, 0.15f, 0.45f };

            FX::Renderer2D::DrawRect(world, color);
        }

        FX::Renderer2D::EndScene();
        FX::Renderer2D::SetLineWidth(1.0f);
        FX::RenderCommand::SetDepthTest(true);
    }

    void EditorApp::FocusOnSelection()
    {
        FX::Entity selected = m_Selection.GetPrimary();
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

        // Ctrl: ekle/cikar. Gizmo'nun kademe (snap) kisayolu da Ctrl ama
        // burasi zaten gizmonun uzerinde DEGILKEN calisiyor - cakismiyorlar.
        const bool ctrl = ImGui::GetIO().KeyCtrl;

        if (pixel < 0)
        {
            // Ctrl basiliyken bosluga tiklamak secimi silmemeli: coklu
            // secim yaparken isabet ettiremeyen bir tik her seyi goturur.
            if (!ctrl)
                m_Selection.Clear();
            return;
        }

        const auto handle = static_cast<entt::entity>(pixel);
        if (m_Scene->GetRegistry().valid(handle))
        {
            FX::Entity picked{ handle, m_Scene };

            if (ctrl)
                m_Selection.Toggle(picked);
            else
                m_Selection.Select(picked);

            FX_INFO("Secildi: %s", picked.GetComponent<FX::TagComponent>().Tag.c_str());
        }
    }

    void EditorApp::DrawGizmo()
    {
        FX::Entity selected = m_Selection.GetPrimary();
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
        const bool snap = m_SnapEnabled ||
                          FX::Input::IsKeyPressed(FX::Key::LeftCtrl) ||
                          FX::Input::IsKeyPressed(FX::Key::RightCtrl);

        float snapValue = m_SnapTranslate;
        if (m_GizmoOperation == ImGuizmo::ROTATE)     snapValue = m_SnapRotate;
        else if (m_GizmoOperation == ImGuizmo::SCALE) snapValue = m_SnapScale;

        const float snapValues[3] = { snapValue, snapValue, snapValue };

        // Olcekleme dunya uzayinda anlamsiz; ImGuizmo zaten yerele zorlar.
        const auto mode = (m_GizmoOperation == ImGuizmo::SCALE)
                        ? ImGuizmo::LOCAL
                        : static_cast<ImGuizmo::MODE>(m_GizmoMode);

        // Delta matrisi: ImGuizmo'nun bu karede uyguladigi DUNYA uzayi
        // donusumu. Coklu secimde digerlerine bunu uyguluyoruz - her
        // entity'yi birincilin mutlak konumuna tasimak yerine, hepsini
        // ayni miktarda oynatmak istiyoruz.
        glm::mat4 delta{ 1.0f };

        ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
                             static_cast<ImGuizmo::OPERATION>(m_GizmoOperation),
                             mode,
                             glm::value_ptr(transform),
                             glm::value_ptr(delta),
                             snap ? snapValues : nullptr);

        const bool usingNow = ImGuizmo::IsUsing();

        // Surukleme BASLADI: transform'lar bu karede yazilmadan ONCE tum
        // secili entity'lerin ESKI degerlerini yakala (undo icin tek adim).
        if (usingNow && !m_GizmoWasUsing)
        {
            m_GizmoStart.clear();
            for (FX::Entity e : m_Selection.GetAll())
            {
                if (!e.HasComponent<FX::TransformComponent>())
                    continue;
                const auto& t = e.GetComponent<FX::TransformComponent>();
                m_GizmoStart.push_back({ e, t.Translation, t.Rotation, t.Scale });
            }
        }

        if (usingNow)
        {
            // Dunya -> yerel: parent'in tersiyle carp.
            const glm::mat4 local = glm::inverse(parentWorld) * transform;

            float translation[3], rotation[3], scale[3];
            ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(local),
                                                  translation, rotation, scale);

            tc.Translation = { translation[0], translation[1], translation[2] };
            tc.Rotation    = glm::radians(rotation[2]);
            tc.Scale       = { scale[0], scale[1] };

            if (m_Selection.Count() > 1)
                ApplyGizmoDelta(delta, selected);
        }

        // Surukleme BIRAKILDI: eski (yakalanan) -> yeni (canli) tek komut.
        if (!usingNow && m_GizmoWasUsing)
            PushGizmoCommand();

        m_GizmoWasUsing = usingNow;
    }

    void EditorApp::PushGizmoCommand()
    {
        if (m_GizmoStart.empty())
            return;

        // Eski + yeni degerleri kimlikle (UUID degil, tutamak+sahne yeterli:
        // gizmo sirasinda sahne degismez) topla. Gercekten degisen yoksa
        // komut yazma.
        struct Rec { FX::Entity e; glm::vec3 t0, t1; float r0, r1; glm::vec2 s0, s1; };
        std::vector<Rec> recs;
        bool anyChanged = false;

        for (const GizmoSnapshot& snap : m_GizmoStart)
        {
            FX::Entity e = snap.Entity;
            if (!e || !e.HasComponent<FX::TransformComponent>())
                continue;
            const auto& t = e.GetComponent<FX::TransformComponent>();
            Rec r{ e, snap.Translation, t.Translation,
                   snap.Rotation, t.Rotation, snap.Scale, t.Scale };
            if (r.t0 != r.t1 || r.r0 != r.r1 || r.s0 != r.s1)
                anyChanged = true;
            recs.push_back(r);
        }

        m_GizmoStart.clear();

        if (!anyChanged)
            return;

        auto apply = [](const std::vector<Rec>& rs, bool useNew)
        {
            for (const Rec& r : rs)
            {
                FX::Entity e = r.e;
                if (!e || !e.HasComponent<FX::TransformComponent>())
                    continue;
                auto& tc = e.GetComponent<FX::TransformComponent>();
                tc.Translation = useNew ? r.t1 : r.t0;
                tc.Rotation    = useNew ? r.r1 : r.r0;
                tc.Scale       = useNew ? r.s1 : r.s0;
            }
        };

        EditCommand cmd;
        cmd.Name = (recs.size() > 1) ? "Gizmo: tasima (coklu)" : "Gizmo: tasima";
        cmd.Undo = [recs, apply]() { apply(recs, false); };
        cmd.Redo = [recs, apply]() { apply(recs, true); };
        m_Commands.Push(std::move(cmd));
    }

    void EditorApp::DeleteSelection()
    {
        if (IsPlaying() || m_Selection.IsEmpty())
            return;

        // Secim listesinin KOPYASI uzerinde geziyoruz: silme sirasinda
        // secimi de temizliyoruz.
        const std::vector<FX::Entity> selection = m_Selection.GetAll();
        int deleted = 0;

        for (FX::Entity entity : selection)
        {
            // Listedeki bir entity, daha once silinen baskasinin cocugu
            // olabilir; o zaten yok oldu.
            if (!entity || !m_Scene->GetRegistry().valid(entity.GetHandle()))
                continue;

            m_Scene->DestroyEntity(entity);
            ++deleted;
        }

        m_Selection.Clear();
        SetStatus(std::to_string(deleted) + " entity silindi");
    }

    void EditorApp::ApplyGizmoDelta(const glm::mat4& delta, FX::Entity primary)
    {
        for (FX::Entity entity : m_Selection.GetAll())
        {
            if (entity == primary || !entity.HasComponent<FX::TransformComponent>())
                continue;

            // Atasi da seciliyse ATLA: parent zaten oynadi, cocuk onunla
            // birlikte geldi. Bir kez daha uygularsak iki kat hareket eder.
            bool ancestorSelected = false;
            for (FX::Entity other : m_Selection.GetAll())
            {
                if (other != entity && other.IsAncestorOf(entity))
                {
                    ancestorSelected = true;
                    break;
                }
            }
            if (ancestorSelected)
                continue;

            auto& tc = entity.GetComponent<FX::TransformComponent>();

            glm::mat4 parentWorld{ 1.0f };
            if (FX::Entity parent = entity.GetParent())
            {
                if (parent.HasComponent<FX::WorldTransformComponent>())
                    parentWorld = parent.GetComponent<FX::WorldTransformComponent>().Matrix;
            }

            const glm::mat4 world    = parentWorld * tc.GetTransform();
            const glm::mat4 newLocal = glm::inverse(parentWorld) * (delta * world);

            float translation[3], rotation[3], scale[3];
            ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(newLocal),
                                                  translation, rotation, scale);

            tc.Translation = { translation[0], translation[1], translation[2] };
            tc.Rotation    = glm::radians(rotation[2]);
            tc.Scale       = { scale[0], scale[1] };
        }
    }

}
