# A2 — Game View / Scene View ayrımı

> Karar K3'ün uygulanması (bkz. [02-Mimari.md](02-Mimari.md)).

## Sorun

Tek bir "Viewport" paneli vardı ve iki işi birden yapıyordu:

- **"Hangi kamera çiziyor?" belirsizdi.** Edit modunda editör kamerası,
  Play'e basınca sessizce sahne kamerası. Aynı panel, farklı anlam.
- **Düzenleme yardımcıları oyunun üstüne biniyordu.** Izgara, gizmo ve
  seçim çerçevesi Play'de `if (!IsPlaying())` ile gizleniyordu — yani
  oyunu izlerken hata ayıklama araçların yoktu, Edit'e dönmen gerekiyordu.
- Sahne kamerası yoksa **sessizce** editör kamerasına düşülüyordu:
  "kameran yok" gerçeği gizleniyordu.

## Çözüm

İki panel, iki framebuffer, iki net anlam:

| | Scene | Game |
|---|---|---|
| Kamera | Editör kamerası — **her zaman** | Sahnenin birincil kamerası — **her zaman** |
| Izgara / gizmo / seçim çerçevesi | Var | Yok |
| Entity seçimi (`PickEntity`) | Var | Yok |
| Play sırasında | Canlı çalışır | Canlı çalışır |
| Kamera yoksa | — | Görünür uyarı |

Play'e basınca Game sekmesi öne gelir, Stop'ta Scene. İkisi aynı dock
düğümünde sekme olarak duruyor (Unity düzeni).

### Alınan kararlar

| Karar | Seçim | Gerekçe |
|---|---|---|
| En-boy oranı | **Serbest** (panel ne kadarsa) | Hedef çözünürlük kavramı A3'e ait; A2'ye çekmek Project Settings'i buraya taşırdı |
| Play davranışı | **Otomatik Game'e geç** | "Oyunu göremiyorum" şaşkınlığını önler |
| Scene View Play'de | **Editör kamerasından canlı** | Oyunu dışarıdan izleyip hata ayıklamayı sağlar — eski davranışta bu mümkün değildi |

"Maximize on Play" **yapılmadı**: düzen kaydetme/geri yükleme işi ekliyor
ve sekme değişimi zaten sorunu çözüyor.

### Kamera yoksa editör kamerasına düşmüyoruz

Eski davranış "siyah ekran göstermektense çalışmaya devam et" idi. A2'de
tersine döndü: Game View siyah kalıyor ve panel **neden** olduğunu
yazıyor + ne yapılacağını söylüyor. Gerekçe, mimarinin taşıyıcı
fikirlerinden biri — *sessiz yanlışlık yerine görünür uyarı*. Sahne
kamerasının varlığı oyunun çalışması için şart; onu gizlemek "oyun neden
boş?" sorusunu doğuruyordu.

### Game View'ın framebuffer'ı daha ince

Scene View'ın framebuffer'ında entity ID eki (`R32I`) var — seçim onu
okuyor. Game View'da seçim yok, dolayısıyla o ek de yok. Aynı spec'i
paylaşsalardı her karede boşuna bir `R32I` tampon temizlenirdi.

### Düzen geçişi

"Viewport" paneli "Scene" + "Game" oldu. Eski `imgui.ini`'de "Game"
yok — düzeni yeniden kurmasak yeni paneller yerleşmemiş halde ortada
yüzerdi. `ImGuiLayer::IniHasWindow("Game")` bunu tespit edip varsayılan
düzeni bir kez yeniden kuruyor.

**Doğrulandı:** çalıştırmadan önce ini'de `[Window][Game]` yoktu,
çalıştırmadan sonra hem `[Window][Scene]` hem `[Window][Game]` var.

## Doğrulama

| | Sonuç |
|---|---|
| Derleme | temiz |
| `FXTests` | 48 test / 223 assertion — motor koduna dokunulmadı, aynı |
| Editör açılışı | 6 sn ayakta, çökme yok |
| Düzen geçişi | ini'ye Scene + Game yazıldı ✅ |

**Görsel doğrulama yapılmadı** — panellerin gerçekten doğru kamerayı
gösterip göstermediği elle test edilmeli.

## Elle test edilmesi gerekenler (GUI)

1. Editörü aç → ortada **Scene** ve **Game** diye iki sekme olmalı.
2. Scene sekmesi: ızgara, seçim çerçevesi, gizmo çalışıyor mu?
3. Game sekmesi: **ızgara ve gizmo görünmemeli**; sahne kamerasının
   gördüğü kadarı görünmeli.
4. **Play'e bas** → Game sekmesi kendiliğinden öne gelmeli.
5. Play sırasında Scene sekmesine geç → oyun canlı akmalı, gizmo ve
   ızgara **hâlâ görünür** olmalı, entity seçebilmelisin.
6. **Stop** → Scene sekmesi öne gelmeli.
7. Ana Kamera'yı sil (veya "Birincil" işaretini kaldır) → Game View
   turuncu "Sahnede birincil kamera yok" uyarısı göstermeli.
8. Kamerayı hareket ettir → Game View'daki görüntü kaymalı.
9. Game panelini yeniden boyutlandır → görüntü bozulmadan uymalı.
10. Scene panelini yeniden boyutlandır → gizmo ve seçim hâlâ doğru
    yerde olmalı (fare koordinat dönüşümü).
11. İçerik panelinden **Scene**'e resim sürükle → sprite oluşmalı
    (bırakma hedefi Game'de yok, orada bir şey olmamalı).

## Yapılmadı

- **Hedef çözünürlük / sabit en-boy oranı** → A3 (Project Settings).
- **Maximize on Play** → düzen kaydetme işi; sekme değişimi yeterli.
- **Game View'da istatistik kaplaması** (FPS, draw call) — Istatistikler
  paneli zaten var.
- `Renderer2D::ResetStats()` yalnızca Scene View'da çağrılıyor, yani
  istatistikler **iki görünümün toplamını** gösteriyor. Sahne iki kez
  çiziliyor artık; draw call sayısı bunu yansıtır. Yanıltıcı değil ama
  A3'te "hangi görünümün istatistiği?" diye ayrılabilir.
