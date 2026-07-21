# Borç 0.7 — Refactor turu (A1 öncesi)

> Amaç: A1'e (component meta-veri sistemi) girmeden önce, A1'in
> **dokunmayacağı** yerlerdeki birikmiş dağınıklığı temizlemek.
> Davranış değişikliği hedeflenmedi.

## Kapsam dışı bırakılanlar ve nedeni

| Yer | Neden dokunulmadı |
|---|---|
| `EntitySerialization.cpp` | A1 bunu tablodan üretilecek şekilde yeniden yazacak — şimdi refactor etmek çöpe atılacak iş |
| `SceneHierarchyPanel::DrawComponents` | Aynı sebep; A1'in otomatik Inspector çizimi buranın yerine geçecek |
| `AllComponents` / `Components.h` | A1'in tam merkezi |
| `Renderer2D` global durum (`s_Data`) | Gerçek borç ama refactor değil, yeniden tasarım. Ayrı iş. |

Yani refactor bilinçli olarak A1'in çalışma alanının **dışında** tutuldu;
iki iş birbirinin üstüne yazmasın.

## Yapılanlar

### R3 — `OnRender`'daki kopya blok
Karşılama ekranı dalı ile editör dalı, ertelenmiş proje isteklerini
(`m_NewProjectRequested` / `m_OpenProjectRequested` / `m_PendingProjectPath`)
**birebir iki kere** işliyordu. Tek `ProcessProjectRequests()` oldu.

Mimari notundaki "aynı bilgi iki yerde durmasın" kuralının doğrudan
ihlaliydi: yeni bir ertelenmiş istek eklenirken biri unutulursa hata
sessiz olurdu ("launcher'da çalışıyor, editörde çalışmıyor").

### R2 — Demo sahnesi editörden çıktı
`BuildScene` + `SpawnMover` + `m_PlayerUUID` + `GetPlayer` (~150 satır)
Faz 5–8'in *kanıt sahnesiydi*, editörün kalıcı sorumluluğu değil. Ama
`EditorApp`'in üye durumunu kirletiyordu: `NewScene` `m_PlayerUUID`'yi
sıfırlıyor, `OpenScene` onun hakkında log basıyordu.

→ `Editor/src/SampleScene.{h,cpp}`, serbest fonksiyonlar:

```cpp
FX::UUID Build(FX::Scene&, checkerboard, circle);   // oyuncu UUID'sini döner
void SpawnMover(FX::Scene&, circle);
```

`m_PlayerUUID` ve `GetPlayer()` tamamen silindi — sahne kimliği artık
demo kodunun yerel bilgisi, editörün kalıcı durumu değil.

**Bilinçli küçük davranış değişikliği:** "Sahneyi Sıfırla" artık seçimi
temizliyor. Eskiden temizlemiyordu; sahne yıkıldığı için `SelectionContext`
ölü entity tutuyordu.

### R1 — `EditorApp.cpp` bölündü (2033 → 4 dosya)

Sınıf aynı, header aynı; yalnızca tanımlar dağıtıldı:

| Dosya | İçerik | Satır |
|---|---|---|
| `EditorApp.cpp` | yaşam döngüsü: ctor, `OnInit`, Play/Stop, `OnUpdate`, `OnRender`, olaylar, `OnShutdown` | 511 |
| `EditorApp_Project.cpp` | karşılama ekranı, proje, sahne dosyası, `editor.json`, içe aktarma, sürükle-bırak | 679 |
| `EditorApp_UI.cpp` | menü çubuğu, viewport paneli, araç çubuğu, istatistikler | 477 |
| `EditorApp_Viewport.cpp` | ızgara, kamera gizmoları, seçim çerçevesi, `PickEntity`, ImGuizmo | 483 |

**Neden şimdi:** A2 (Game View) ve A5 (Undo/Redo) tam bu dosyaya
dokunacak. Bölünmemiş hâlde her ikisi de 2000 satırlık dosyada gezinmek
demekti.

Include blokları dosya başına daraltıldı (hepsine kopyalanmadı) —
`EditorApp_UI.cpp` artık `nlohmann/json`, `SDL3`, `<filesystem>`,
`<fstream>` görmüyor.

### R4 — `.meta` yolu tek yerde
`AssetManager::MetaPathFor` zaten vardı ama `ContentBrowserPanel` üç
yerde `.meta` uzantısını **elle** ekliyordu (`path.string() + ".meta"`).
Uzantı kuralı bir gün değişse ikisi ayrışır, `.meta` geride kalır ve
GUID kopardı — tam olarak Faz 22'nin engellemek için kurulduğu hata.

### R5 — `Shader::FromFiles` ham `new` döndürüyordu
`Shader*` → `std::unique_ptr<Shader>`. İki çağrı yeri `.reset(...)`
yerine doğrudan atama yapıyor. Teknik borç listesinden bir madde düştü.

## Doğrulama

| | Öncesi | Sonrası |
|---|---|---|
| `FXTests` | 41 test / 145 assertion | 41 test / 145 assertion |
| Derleme | temiz | temiz |
| Editör açılışı | — | 6 sn ayakta, çökme yok |

Motor koduna dokunan tek şey R4 ve R5; ikisi de testlerin kapsadığı
davranışı değiştirmiyor ve testler aynı sonucu veriyor.

## Elle test edilmesi gerekenler (GUI)

1. Karşılama ekranı → "Projesiz Devam Et" → örnek sahne kuruluyor ve
   **Oyuncu** seçili geliyor mu?
2. Karşılama ekranından "Yeni Proje" / "Son projeler"den açma → diyalog
   düzgün açılıyor mu (R3'ün asıl riski burada)?
3. Editör içinden Dosya → Yeni/Aç Proje → aynı kontrol.
4. Sahne → "Entity Ekle (10)" ve "Sahneyi Sıfırla" çalışıyor mu?
   Sıfırladıktan sonra Inspector boş mu (seçim temizleniyor)?
5. İçerik paneli → bir dosyayı başka klasöre sürükle → `.meta` da taşındı
   mı, GUID korundu mu (R4)?
6. İçerik paneli → bir varlığı sil → yanındaki `.meta` da silindi mi?
7. Viewport: ızgara, gizmo, seçim çerçevesi, `F` odaklan, sağ tuş kaydırma
   (R1'in taşıdığı kod).

## Yapılmadı

- `ContentBrowserPanel.cpp` (1308 satır) bölünmedi. Bölünebilir ama
  `EditorApp` kadar acil değil ve yaklaşan fazlar oraya dokunmuyor.
- `Renderer2D` global durumu duruyor (bkz. teknik borç).
