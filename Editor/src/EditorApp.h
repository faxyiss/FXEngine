#pragma once

// ===========================================================================
// FXEditor - EditorApp
// ===========================================================================

#include <FXEngine/Core/Application.h>
#include <FXEngine/Renderer/Texture.h>
#include <FXEngine/Renderer/TextureLibrary.h>
#include <FXEngine/Renderer/OrthographicCamera.h>
#include <FXEngine/Renderer/Framebuffer.h>
#include <FXEngine/Scene/Scene.h>
#include <FXEngine/Scene/Entity.h>
#include <FXEngine/Core/UUID.h>
#include <FXEngine/Events/KeyEvent.h>
#include <FXEngine/Events/MouseEvent.h>

#include "SelectionContext.h"
#include "ImGuiLayer.h"
#include "EditorCamera.h"
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/ContentBrowserPanel.h"

#include <deque>
#include <memory>
#include <string>

namespace FXEd
{
    class EditorApp : public FX::Application
    {
    public:
        EditorApp();

    protected:
        void OnInit()               override;
        void OnShutdown()           override;
        void OnUpdate(float dt)     override;
        void OnRender(float alpha)  override;
        // Ham SDL: yalnizca ImGui backend'i ve isletim sistemi
        // surukle-birakma icin (Faz 13b'den sonra ikisi kaldi).
        void OnRawEvent(const SDL_Event& event) override;

        // Motor olaylari: kisayollar ve kamera.
        void OnEvent(FX::Event& event) override;

        bool OnKeyPressed(FX::KeyPressedEvent& e);
        void OnWindowResize(std::uint32_t width, std::uint32_t height) override;

    private:
        // --- Proje islemleri (Faz 21) -----------------------------------------
        void NewProject();                              // diyalog acar
        void OpenProject();                             // diyalog acar
        bool OpenProject(const std::string& filepath);  // dogrudan yukler
        void PushRecentProject(const std::string& path);

        // Ertelenmis proje istekleri; ImGui cercevesi kapandiktan sonra.
        void ProcessProjectRequests();

        // Karsilama ekrani: proje secilene kadar editor arayuzu yerine
        // bu ciziliyor. Proje acilmadan varlik yuklemek yanlis kokten
        // okumak demek olurdu.
        void DrawLauncher();

        // Ornek sahneyi kurar (karsilama ekranindaki "projesiz devam et").
        void StartWithoutProject();

        bool m_ShowLauncher = true;

        // --- Sahne dosyasi islemleri (Faz 12) ---------------------------------
        void NewScene();
        void SaveScene();                              // yol yoksa SaveSceneAs
        void SaveSceneAs();
        void OpenScene();                              // dosya diyalogu acar
        void OpenScene(const std::string& path);       // dogrudan yukler

        // Son acilan sahneler exe'nin yanindaki editor.json'da tutulur.
        void LoadEditorConfig();
        void SaveEditorConfig();
        void PushRecentScene(const std::string& path);

        // Secili tum entity'leri siler (Delete tusu).
        void DeleteSelection();

        // Coklu secimde gizmo birincil uzerinde duruyor; kalanlara ayni
        // dunya-uzayi deltasi uygulaniyor.
        void ApplyGizmoDelta(const glm::mat4& delta, FX::Entity primary);

        // Acik sahneyi projenin baslangic sahnesi yapar (GUID olarak).
        void SetAsStartScene();

        // Icerik panelinde cift tiklanan dosyayi turune gore acar.
        void OpenAsset(const std::string& relativePath);

        // Icerik panelinden viewport'a birakilan dosyayi ele alir.
        void HandleContentDrop(const std::string& relativePath, float screenX, float screenY);

        // Isletim sisteminden (Explorer'dan) surukleyip birakilan dosya.
        void HandleExternalDrop(const std::string& absolutePath, float screenX, float screenY);

        // "Ice Aktar..." -> dosya diyalogu -> Icerik paneline kopyala.
        void ImportAssets();

        void SetStatus(const std::string& message);

        // --- Script dosyasi olusturma (A4) ------------------------------------
        // Sablonu Editor/src/Scripts/ altina yazar ve sistem editorunde
        // acar. Derlenmesi icin yeniden derleme gerekiyor - bu sinir
        // bilincli, cozumu Asama B (oyun DLL'i).
        void DrawNewScriptModal();
        bool CreateScriptFile(const std::string& name, std::string& outPath);

        // ISTEK bayragi: bir kez tuketilip ImGui::OpenPopup cagriliyor.
        // "Acik mi?" durumunu ImGui'nin kendisi tutuyor - biz de tutup
        // her karede OpenPopup cagirsaydik popup her kare sifirlanir ve
        // dugme tiklamalari (basma+birakma iki kare surer) hic
        // tamamlanmazdi.
        bool m_NewScriptRequested = false;

        // Metin kutusuna odak yalnizca ACILIS karesinde verilmeli.
        bool m_FocusScriptName = false;

        char m_NewScriptName[64] = "";

        // Sonsuz izgara. Aralik zoom'a gore 1-2-5-10 serisinde secilir.
        void DrawGrid();

        // Sahnedeki kameralarin gorus alanini ve konumunu cizer.
        // Kameranin sprite'i yoktur; bu cizim olmadan nereye baktigi
        // gorunmez ve viewport'tan secilemez.
        void DrawCameraGizmos();

        // Secili entity'nin sinirlarini cizer (dunya matrisiyle, yani
        // dondurulmus/olceklenmis nesnelerde de uzerine oturur).
        void DrawSelectionOutline();

        // Kamerayi secili entity'yi cerceveleyecek sekilde konumlandirir.
        void FocusOnSelection();

        // ImGui panellerini cizer.
        void DrawMenuBar();

        // Play/Stop/Duraklat - bir gorunume degil editorun durumuna ait,
        // bu yuzden menu cubugunun altinda genel bir serit.
        void DrawPlayBar();

        void DrawScenePanel();

        // Oyun goruntusu: sahne kamerasindan, duzenleme yardimcisi yok.
        void DrawGamePanel();

        // Sahneyi iki ayri framebuffer'a cizer (A2).
        void RenderSceneView();
        void RenderGameView();
        void DrawViewportToolbar();
        void DrawStatsPanel();

        // Ayar pencereleri (A3). Ayrilar cunku biri PROJEYE, digeri
        // KULLANICIYA ait - karisirsa yanlis dosyaya yazilir.
        void DrawGameViewToolbar();

        void DrawProjectSettingsWindow();
        void DrawPreferencesWindow();

        bool m_ShowProjectSettings = false;
        bool m_ShowPreferences     = false;
        void DrawGizmo();

        // Fare altindaki entity'yi framebuffer'in ID ekinden okur.
        void PickEntity();

        // --- Sahne -------------------------------------------------------------
        //
        // IKI SAHNE VAR (Faz 10):
        //   m_EditorScene  - duzenledigin sahne. Play sirasinda DOKUNULMAZ.
        //   m_RuntimeScene - Play'e basinca alinan kopya; oyun burada calisir.
        //
        // m_Scene her zaman "su an gosterilen ve guncellenen" sahneye
        // isaret eder. Play'de kopyayi, Edit'te orijinali gosterir.
        //
        // Neden kopya? Cunku oyun calisirken nesneler hareket eder, silinir,
        // olusur. Duzenlenen sahnede calistirsaydik Stop'a bastiginda
        // sahnen geri donusu olmayan bicimde degismis olurdu - Unity'deki
        // "play modunda yaptigin degisiklikler kayboldu" travmasinin
        // tersi: burada calismalarin kaybolurdu.
        std::unique_ptr<FX::Scene> m_EditorScene;
        std::unique_ptr<FX::Scene> m_RuntimeScene;

        // Sahip DEGIL, isaretci: yukaridaki ikisinden birini gosterir.
        FX::Scene* m_Scene = nullptr;

        enum class SceneState { Edit, Play };
        SceneState m_SceneState = SceneState::Edit;

        void OnScenePlay();
        void OnSceneStop();

        bool IsPlaying() const { return m_SceneState == SceneState::Play; }

        // --- Editor arayuzu ----------------------------------------------------
        // Secimin SAHIBI burasi (0.2). Viewport, gizmo, inspector ve
        // hierarchy paneli hepsi ayni nesneyi okuyor; hicbiri sahibi
        // degil. Panellerden once tanimli olmali - onlara isaretcisini
        // veriyoruz.
        SelectionContext    m_Selection;

        ImGuiLayer          m_ImGuiLayer;
        SceneHierarchyPanel m_HierarchyPanel;
        ContentBrowserPanel m_ContentBrowser;

        // Sahne artik dogrudan ekrana degil, BUNA ciziliyor.
        // Sonra texture'i ImGui panelinde gosteriyoruz.
        //
        // IKI FRAMEBUFFER (A2): Scene View editor kamerasindan ve
        // duzenleme yardimcilariyla, Game View sahne kamerasindan ve
        // yardimcisiz cizer. Tek framebuffer'i paylassalardi ikisi ayni
        // kareyi gosterirdi ve "hangi kamera ciziyor?" sorusu belirsiz
        // kalmaya devam ederdi.
        std::unique_ptr<FX::Framebuffer> m_Framebuffer;       // Scene View
        std::unique_ptr<FX::Framebuffer> m_GameFramebuffer;   // Game View

        glm::vec2 m_GameViewportSize{ 0.0f, 0.0f };

        // Game View projenin hedef en-boy oranina kilitlensin mi?
        // Kapaliyken panel ne kadarsa o oranda cizer (A2'nin davranisi).
        // Kullanici tercihi: editor.json'a yaziliyor.
        bool m_GameViewLocked = false;

        // Play'e basinca Game, Stop'ta Scene one gelsin. ImGui'ye
        // cerceve icinde soylenmeli; istek bayrakta bekliyor.
        bool m_FocusGameView  = false;
        bool m_FocusSceneView = false;

        // Viewport panelinin PIKSEL boyutu. Pencere boyutundan farklidir:
        // paneller yer kapladigi icin viewport her zaman daha kucuktur.
        // Kamera en-boy orani BUNA gore hesaplanmali, pencereye gore degil.
        glm::vec2 m_ViewportSize{ 0.0f, 0.0f };

        // Viewport panelinin ekran uzerindeki sinirlari. Fare konumunu
        // panel-yerel koordinata cevirmek icin gerekli.
        glm::vec2 m_ViewportBoundsMin{ 0.0f, 0.0f };
        glm::vec2 m_ViewportBoundsMax{ 0.0f, 0.0f };

        // Fare viewport panelinin uzerinde mi? Kamera kisayollarini
        // sadece o zaman calistiracagiz.
        bool m_ViewportHovered = false;
        bool m_ViewportFocused = false;

        // ImGuizmo islem tipi (ImGuizmo::OPERATION). -1 = gizmo kapali.
        int m_GizmoOperation = -1;

        // ImGuizmo::MODE - tutamaklar nesnenin kendi ekseninde mi (LOCAL)
        // yoksa dunya ekseninde mi (WORLD) duracak. ImGuizmo.h burada
        // dahil edilmedigi icin int; degerler .cpp'de atanir.
        int m_GizmoMode = 0;   // = ImGuizmo::LOCAL

        // Kademeli hareket. Ctrl basiliyken de gecici olarak acilir.
        bool  m_SnapEnabled   = false;
        float m_SnapTranslate = 0.5f;
        float m_SnapRotate    = 15.0f;
        float m_SnapScale     = 0.25f;

        // Proje diyaloglari da modal: ayni sebeple cerceve sonuna
        // birakiliyor (bkz. m_ImportRequested).
        bool        m_NewProjectRequested  = false;
        bool        m_OpenProjectRequested = false;
        std::string m_PendingProjectPath;

        // Menuden gelen ice aktarma istegi. Yerel diyalog modaldir ve
        // ImGui cercevesinin ortasinda acilamaz; cerceve bittikten sonra
        // isleniyor.
        bool m_ImportRequested = false;

        // --- Kaynaklar ---------------------------------------------------------
        // Kutuphane, yol -> texture eslemesini tutar. Sahne yuklerken
        // JSON'daki yollari texture'a cevirmek icin gerekli, ve ayni
        // dosyanin tekrar tekrar yuklenmesini onler.
        FX::TextureLibrary m_TextureLibrary;

        // Karsilama ekranindaki logo. MOTOR varligi, projeye ait degil:
        // proje acilmadan once yuklenip tutuluyor, cunku proje acilinca
        // "assets/..." artik proje kokune cozulur.
        std::shared_ptr<FX::Texture2D> m_Logo;

        std::shared_ptr<FX::Texture2D> m_Checkerboard;
        std::shared_ptr<FX::Texture2D> m_Circle;

        // Acik sahnenin yolu. BOS = "henuz kaydedilmedi"; Ctrl+S bu durumda
        // Farkli Kaydet diyalogunu acar. Faz 7'deki sabit yol kalkti.
        std::string m_ScenePath;

        // En yenisi basta. deque cunku hem basa ekleyip hem sondan
        // atiyoruz; vector'de bas ekleme her seferinde tum diziyi kaydirirdi.
        std::deque<std::string> m_RecentScenes;
        static constexpr std::size_t kMaxRecentScenes = 8;

        // Son acilan projeler. Sahnelerin aksine bunlar MUTLAK yol
        // tasiyor: proje kokunun kendisi soz konusu oldugunda "neye
        // goreceli?" sorusunun cevabi yok.
        std::deque<std::string> m_RecentProjects;
        static constexpr std::size_t kMaxRecentProjects = 8;

        // Son islemin sonucu - menude kullaniciya geri bildirim icin.
        std::string m_StatusMessage;
        float       m_StatusTimer = 0.0f;

        // --- Kamera ------------------------------------------------------------
        EditorCamera m_EditorCamera;

        bool m_ShowGrid        = true;
        bool m_ShowCameraGizmos = true;

        // --- Durum -------------------------------------------------------------
        float m_Time = 0.0f;
        bool  m_ScenePaused = false;

        float m_FpsTimer   = 0.0f;
        int   m_FpsFrames  = 0;
        float m_CurrentFps = 0.0f;

        bool m_ShowDemoWindow = false;
    };
}
