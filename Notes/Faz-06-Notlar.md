# Faz 6 — ImGui Docking + Editör Panelleri

## Dosyalar
| Dosya | Katman | İş |
|---|---|---|
| `Engine/Renderer/Framebuffer.h/.cpp` | **Engine** | Ekrana değil texture'a çizim |
| `Editor/src/ImGuiLayer.h/.cpp` | **Editor** | ImGui yaşam döngüsü, docking |
| `Editor/src/Panels/SceneHierarchyPanel.h/.cpp` | **Editor** | Hierarchy + Inspector |
| `Editor/src/EditorApp.*` | **Editor** | Viewport, menü, istatistik panelleri |

## En önemli karar: ImGui nereye ait?

`Renderer2D`'yi Engine'e koyduk. ImGui'yi de koymalı mıyız? **Hayır.**

Engine'in görevi **oyun çalıştırmak**. Sevk edilen bir oyunda editör arayüzü
yoktur; ImGui'yi Engine'e koysaydık her oyun binary'si onu taşırdı.
(CMake'te `imgui` zaten sadece `FXEditor`'e link edilmişti — Faz 0'dan beri.)

Ama **Framebuffer Engine'e ait**. Çünkü "kendi çıktına texture olarak çiz"
bir **render yeteneğidir**, editör özelliği değil. Oyun tarafında da lazım:
ayna, minimap, post-process.

> **Sorulacak doğru soru:** "bunu kim kullanıyor?" değil,
> **"bu hangi katmanın sorumluluğu?"**

## Framebuffer

Normalde OpenGL çizimi doğrudan pencereye gider. Framebuffer bunu değiştirir:
çizim bir texture'a yazılır, sonra o texture istenen yerde kullanılır.

### İki farklı ek (attachment) türü
| Ek | Tür | Neden |
|---|---|---|
| Renk | **texture** | Sonra **okuyacağız** (ImGui'de göstereceğiz) |
| Derinlik | **renderbuffer** | Sadece test için, okumayacağız — daha ucuz |

### ⚠️ Üç tuzak
1. **`glCheckFramebufferStatus`'u atlama.** Eksik framebuffer'a çizim
   *sessizce* başarısız olur: hata yok, sadece boş ekran.
2. **`Bind()` viewport'u da ayarlamalı.** Framebuffer 800×600 ama viewport
   1280×720 kalırsa çizim texture'ın dışına taşar. Framebuffer kullanırken
   en sık yapılan hata.
3. **`Resize` içinde "aynı boyutsa çık" kontrolü şart.** ImGui her karede
   panel boyutunu bildirir; kontrol olmadan her karede texture yeniden
   oluşturulur ve performans çöker.

OpenGL'de bir texture'ın boyutunu *değiştirmek* diye bir şey yok — yeni
yaratıp eskisini silmek gerekir. Silmeyi unutursan her boyut değişiminde
GPU'da bir framebuffer daha birikir.

## ImGui ayrıntıları

### İki backend
ImGui iki parçadan oluşur:
- **Platform backend** (SDL3) → girdi, pencere, pano
- **Renderer backend** (OpenGL3) → çizim verisini GPU'ya gönderme

Bu ayrım sayesinde ImGui herhangi bir pencere/grafik kombinasyonuyla çalışır.

### Docking
`ImGuiConfigFlags_DockingEnable` — bu özellik ImGui'nin **docking dalında**
var, ana dalda yok. Faz 0'da bilerek o dalı çektik.

`DockSpaceOverViewport` + `PassthruCentralNode`: ortadaki alan şeffaf kalır,
viewport paneli oraya yerleşir.

**Viewports (pencere dışına sürükleme) bilerek kapalı.** SDL3 + OpenGL ile
ek context yönetimi gerektirir, hata kaynağıdır, MVP'de gereksiz.

### ⚠️ En sık yapılan ImGui hatası
```cpp
if (m_ImGuiLayer.WantsKeyboard()) return;   // bunu yazmazsan
```
Entity adını "**W**all" yazmaya çalışırken `W` kamerayı oynatır.
`io.WantCaptureKeyboard` / `WantCaptureMouse` kontrolü zorunludur.

Editörde **iki kademeli** kontrol var:
1. ImGui klavyeyi istiyor mu?
2. Fare viewport panelinin üzerinde mi?

### ⚠️ Viewport UV'leri ters
```cpp
ImGui::Image(texID, size, ImVec2(0,1), ImVec2(1,0));
```
OpenGL texture'ları **sol alt** kökenli, ImGui **sol üst** bekler.
Düzeltmezsek sahne baş aşağı görünür.

> Faz 3'te `stbi_set_flip_vertically_on_load` ile yaptığımız düzeltmenin
> aynı mantığı — bu sefer diğer yönde. Aynı kavram, farklı sınır.

### Kamera artık pencereye değil VIEWPORT'a bağlı
Paneller yer kapladığı için viewport her zaman pencereden küçüktür.
En-boy oranını pencereden hesaplasaydık sahne yamuk görünürdü.

## Panel tasarım kararları

### Hierarchy + Inspector neden tek sınıf?
İkisi de **aynı durumu** paylaşıyor: "hangi entity seçili". Ayırsaydık bu
durumu bir yerde tutup ikisine de geçirmemiz gerekirdi.

### `SetContext` seçimi temizler
Sahne değişince eski entity kimliği yeni sahnede **başka bir entity'ye**
denk gelir ve Inspector alakasız veri gösterir. Sinsi bir hata türü.

### View üzerinde gezerken entity silme
Dizi altımızdan değişir. Silme isteği bir değişkende biriktirilip
**döngü bittikten sonra** uygulanıyor.

### ImGui ID yığını
Aynı isimde iki entity varsa ikisi de aynı ID'yi alır, birine tıklayınca
diğeri seçilir. Entity kimliğini ID olarak vererek önlüyoruz.

### Arayüzde derece, veride radyan
Component radyan tutuyor (matematik fonksiyonları öyle istiyor), Inspector
derece gösteriyor. Çevrim tam **sınırda** yapılıyor —
Faz 5 notlarında planladığımız gibi.

### "Component Ekle" menüsü zaten olanları göstermiyor
Gösterseydik kullanıcı tıklar, `AddComponent`'teki assert tetiklenir,
program Debug'da durur.

> **Arayüz, geçersiz eylemi mümkün kılmamalı** — hata mesajı göstermekten iyidir.

## Yıkım sırası
```
ImGui → Framebuffer → Scene → Renderer2D
```
ImGui GL kaynakları tutuyor, en önce bırakılmalı.

## Test sonucu
```
ImGui hazir (surum 1.92.9 WIP, docking acik)
Sahne kuruldu: 25 entity
```
İlk denemede derlendi. GL debug callback **hiç hata basmadı** — framebuffer
eksik olsaydı orada görünürdü.

`imgui.ini` (panel düzeni) `.gitignore`'a eklendi: makineye/kullanıcıya özel.

## Onay
- [x] Derlendi
- [x] Çalıştı, GL hatası yok
- [ ] Kullanıcı onayı → Faz 7'ye geçiş
