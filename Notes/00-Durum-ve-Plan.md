# FXEngine — Durum ve Plan

> **Bu dosya bir devir notudur.** Yeni bir sohbete başlarken önce bunu oku;
> projenin nerede olduğunu, nelerin neden böyle yapıldığını ve sıradaki
> işi buradan öğrenirsin.
>
> Mimarinin tamamı ve alınan yön kararları: **[02-Mimari.md](02-Mimari.md)**
>
> Son güncelleme: 2026-07-20 · Son commit: `759e1b6`

---

## 1. Proje nedir

C++17 ile yazılan bir **2D oyun motoru + editör**. SDL3, OpenGL 3.3, EnTT,
ImGui, glm, nlohmann/json. Windows'ta geliştiriliyor (Linux'ta hiç
derlenmedi).

```
Engine/     motor kütüphanesi (FXEngine.lib) — editörü BİLMEZ
Editor/     editör uygulaması (FXEditor.exe) — motora bağlıdır
Notes/      her fazın tasarım notları (Türkçe)
build/      CMake çıktısı
```

**Tek yönlü bağımlılık:** Engine → hiçbir şey, Editor → Engine.

---

## 2. Nerede kaldık

MVP (Faz 0–7) ve şu fazlar tamam: **8, 9, 10, 11, 12, 21, 22**,
**borç turu 0.1–0.6**, **13a + 13b** + Faz 18'in çizgi/ızgara kısmı.

### Son oturumda yapılanlar

| Commit | İş |
|---|---|
| `759e1b6` | İçerik paneli: ızgarada çift tık çalışmıyordu; dosya açma eklendi |
| `72f8b57` | **0.4** `FX::FileWatcher` — dışarıdan taşımada GUID korunuyor |
| `54d81bf` | **0.2** `SelectionContext` — seçim panelden çıktı |
| `f03e08a` | **0.3** Entity çoklu seçimi (Ctrl/Shift, gizmoda delta, toplu silme) |
| `b0d9079` | **0.5** Catch2 + motor çekirdeği testleri |
| `d9d51b8` | **0.6** Doku ayarları arayüzü, `StartScene` GUID (`.fxproject` v2) |
| (bu) | **13a + 13b** `FX::Input`, `Event` sistemi, SDL çeviri katmanı |

### Şu an çalışan özellikler

- **Proje sistemi:** açılışta karşılama ekranı, `.fxproject`, son projeler,
  motor/proje varlık kökü ayrımı
- **Varlık sistemi:** GUID tabanlı kimlik, `.meta` dosyaları, taşıma
  referansları kırmıyor, doku ayarları dosya başına
- **Sahne:** entity UUID'leri, parent/child hiyerarşi, prefab, sürüm 4
  dosya formatı (eskiler otomatik dönüşüyor)
- **Play/Stop:** oyun kopya sahnede çalışıyor, Edit modu durağan,
  `CameraComponent` ile sahne kamerası
- **Viewport:** ızgara, seçim çerçevesi, gizmo (taşı/döndür/ölçekle),
  sağ tuşla kaydırma, imlece zoom, `F` odaklan, `G` ızgara
- **İçerik paneli:** ızgara/liste görünümü, çoklu seçim, sürükle-bırak
  taşıma, içe aktarma, önizleme, çift tıkla açma, dosya izleyici
- **Seçim:** `SelectionContext` (sahibi `EditorApp`), entity çoklu seçimi,
  gizmoyla toplu dönüşüm
- **Girdi:** `FX::Input` (sorgu) + `FX::Event` (olay); editörde ham SDL
  girdi kullanımı yok

---

## 3. Çalışma kuralları (kullanıcının tercihleri)

Bunlara uy, her seferinde sorma:

1. **Her faz sonunda commit + push** — hatırlatma bekleme.
2. **Kodda az yorum.** Öğretici blok yorumlar istenmiyor; *neden* öyle
   yapıldığını anlatan kısa yorumlar iyi, *ne* yaptığını anlatan uzun
   yorumlar hayır. Uzun açıklamalar `Notes/` altına veya sohbete.
3. **Her faz için `Notes/Faz-NN-Notlar.md`** yaz: kararlar, gerekçeler,
   test sonuçları, bilerek yapılmayanlar.
4. **Yol haritasını güncelle** (`Notes/01-Yol-Haritasi-v2.md`): tamamlanan
   maddeleri işaretle, teknik borç listesini güncel tut.
5. **Türkçe yaz** — kod yorumları ASCII Türkçe (ş→s, ı→i), notlar tam Türkçe.
6. **Doğrula, iddia etme.** Derle, çalıştır, mümkünse otomatik test yaz
   (geçici öz-test → doğrula → kaldır). Ekran görüntüsü alarak görsel
   kontrol yapılabiliyor (PowerShell + `System.Drawing`).

### Test yöntemi

Editör bir GUI uygulaması; test için:
- Geçici öz-test kodu `OnInit`'e eklenir, log'a basar, sonra kaldırılır
- PowerShell + Win32 API ile tıklama/sürükleme simüle edilebilir
- Ekran görüntüsü alınıp `Read` ile incelenebilir
- `Start-Process ... -RedirectStandardOutput` ile log okunur

Motor çekirdeği (Engine) artık **birim testli** (0.5):
`cmake --build build --config Debug --target FXTests` → `ctest -C Debug`
(veya doğrudan `build/bin/FXTests.exe`). Yeni bir serializer/asset
davranışı eklerken önce oraya bir test yaz.

Derleme: `cmake --build build --config Debug`
Çalışan editör exe'yi kilitler — `Get-Process FXEditor | Stop-Process -Force`.

**Klavye simülasyonu tuzağı:** `keybd_event` ile Delete göndermek numpad
nokta üretiyor; gerçek `SDLK_DELETE` için `PostMessage(WM_KEYDOWN, 0x2E)`
gerekiyor ve o da pencereyi öne getirmiyor. Klavye kısayolları elle test
edilmeli.

---

## 4. Bilinen sorunlar ve teknik borç

Öncelik sırasına göre:

### Yüksek
| Sorun | Nerede | Not |
|---|---|---|
| **Undo/Redo yok** | editörün tamamı | Gizmo ile yanlış sürükleme geri alınamıyor. Komut deseni; editörün tamamına dokunan mimari değişiklik. |

### Orta
| Sorun | Nerede |
|---|---|
| Prefab bağlantısı yok (örnek kaynağından bağımsız, override sistemi yok) | `PrefabSerializer` |
| Linux/macOS dosya diyalogları boş gövde, dosya izleyici yok | `FileDialogs.cpp`, `FileWatcher.cpp` |
| Çoklu seçimde Inspector yalnızca birincili düzenliyor | `SceneHierarchyPanel` |

### Düşük
- `Renderer2D` global durum (`s_Data` statik), batch bölme yolu hacky
- `Shader::FromFiles` ham `new` döndürüyor
- `GetRegistry()` çok açık — doğrudan `create`/`destroy` UUID haritasını bozar
- `Renderer2D` ve `TextureLibrary` test dışı (OpenGL bağlamı ister)
- Sadece Windows'ta denendi
- `glLineWidth` > 1.0 garanti değil (seçim çerçevesi 2.0 istiyor)

---

## 5. Faz planı

Fazlar 2026-07-20'de **alt fazlara bölündü** ve önlerine bir borç
kapatma turu (0.x) eklendi. Ayrıntı: `01-Yol-Haritasi-v2.md`.

### Güncel sıra

```
✅ borç turu 0.1–0.6  ✅ 13a  ✅ 13b  ✅ 16a  ✅ 16b
▶  A1 component meta → A2 Game View → A3 Settings
   → A4 script dosyası → A5 Undo/Redo
   → B: oyun DLL'i + hot reload
   → C: C# kararı
   → 16c → 14 → 15 → 18b → 17a-d → 18c → 19 → 23 → 18d
   → 13c → 13d → 20c → 20d
```

### Borç turu (0.x)

| # | İş | Durum |
|---|---|---|
| 0.1 | Faz 22'nin gerçek veriyle doğrulanması | ✅ sahne v4, GUID eşleşiyor, eksik `.meta` eklendi |
| 0.4 | Dosya izleyici (`ReadDirectoryChangesW`) | ✅ GUID dışarıdan taşımada da korunuyor |
| 0.2 | `SelectionContext` — seçimi panelden çıkar | ✅ sahibi `EditorApp`, paneller tüketici |
| 0.3 | Entity çoklu seçimi | ✅ Ctrl/Shift, gizmoda delta, toplu silme |
| 0.5 | Catch2 + `UUID`/`Scene`/`SceneSerializer`/`AssetManager` testleri | ✅ 26 test / 80 assertion |
| 0.6 | Faz 22 artıkları (Inspector doku ayarları, `StartScene` GUID, `AssetDirectory`) | ✅ `.fxproject` sürüm 2 |

**2026-07-21: yön kararı alındı.** Editör temel araçları örnek oyunun
önüne alındı, C# scripting ertelendi. Gerekçeler ve mimarinin tamamı:
**[02-Mimari.md](02-Mimari.md)** — yeni bir şey eklemeden önce oku.

13c/13d ertelendi, gerekçesi [Faz-13-Notlar.md](Faz-13-Notlar.md)'de.

---

## 5b. YENİ OTURUMDA İLK İŞ: A1 — component meta-veri sistemi

**Sorun:** Bugün yeni bir component eklemek **dört yere** dokunmayı
gerektiriyor ve biri unutulduğunda hata *sessiz* oluyor:

| Yer | Unutulursa |
|---|---|
| `Components.h` — struct | — |
| `AllComponents` listesi | "Play'e geçince component kayboldu" |
| `EntitySerialization.cpp` | "Kaydedip açınca ayar gitti" |
| `SceneHierarchyPanel.cpp` — Inspector bloğu | Component görünmez |

**Hedef:** Her component **tek bir yerde** tanımlansın: adı, alan listesi
(ad / tip / görünen etiket / aralık), serileştirmesi, Inspector çizimi.
Yöntem: elle yazılan kayıt + alan listesi (makro sihri yok, harici
reflection kütüphanesi yok — karar K2, `02-Mimari.md`).

### Önerilen adımlar

1. **Alan tipi kümesini belirle:** `bool`, `int`, `float`, `vec2`, `vec3`,
   `string`, `color`, `AssetHandle`, `EntityRef`. Bugün kullanılan her
   component alanı bu kümeye sığmalı.
2. **`ComponentRegistry`** — `Register<T>("SpriteRenderer", alanlar…)`.
   Alan tanımı: etiket + üye işaretçisi (`&T::Field`) + isteğe bağlı
   min/max/adım.
3. **Serileştirmeyi tablodan üret** — `EntitySerialization`'ın elle
   yazılmış blokları yerine tablo üzerinden döngü. *Uyum şart:* üretilen
   JSON mevcut sahne dosyalarıyla birebir aynı olmalı, yoksa sürüm 5
   gerekir. Önce testle doğrula.
4. **Inspector'ı tablodan çiz** — tip başına tek çizim fonksiyonu.
   Özel durumlar (doku slotu, kamera "Primary" mantığı) tabloya
   "özel çizici" olarak bağlanabilir.
5. **Script alanlarını aç** — `ScriptableEntity` türevleri de alan
   bildirebilsin (`m_Speed` artık koda gömülü değil, Inspector'dan
   ayarlanır ve sahneye kaydedilir).
6. **Geçiş kademeli olsun:** önce iki basit component (`Velocity`,
   `Camera`) tabloya taşınıp eski yol yanında dursun; testler yeşilse
   kalanlar taşınsın, sonra eski kod silinsin.

### Dikkat

- **Sahne dosyası formatı kırılmamalı.** 41 birim testi bunu koruyor;
  A1 sırasında `SceneSerializer` testleri kırmızı yanarsa dur.
- **`AllComponents` (Scene::Copy) da tablodan beslenmeli** — yoksa yine
  iki yerde iki liste olur.
- **Alan işaretçileri C# köprüsünün dayanacağı yapı** (karar K1); tabloyu
  tasarlarken "bunu bir gün dil sınırından geçirebilir miyim?" diye sor.

### A1'den sonra sırayla

**A2 Game View** (ayrı pencere, sahne kamerası, gizmo/ızgara yalnız Scene
View'da) → **A3 Settings** (`ProjectSettings/` vs `editor.json` ayrımı) →
**A4 script dosyası + şablon** → **A5 Undo/Redo** (A1'in alan tablosu
sayesinde generic "alanı değiştir" komutu).

### Bölünmüş fazlar

| Faz | Parçalar |
|---|---|
| 13 | 13a enum+`Input` · 13b `Event`+SDL çevirisi · 13c `Layer`/`LayerStack` · 13d action mapping |
| 16 | 16a `ScriptSystem` çekirdeği · 16b editör entegrasyonu · 16c örnekler + küçük oyun |
| 14, 15 | bölünmedi (zaten küçük) |
| 17 | 17a Box2D+`Rigidbody2D` · 17b collider'lar · 17c `Simulate`+debug çizim · 17d çarpışma geri çağrımı |
| 18 | 18b `DrawCircle`+debug katmanı · 18c sıralama katmanı · 18d shader hot reload |
| 19 / 23 | metin ve ses **ayrıldı** — birbiriyle ilgisiz iki iş |
| 20 | 20a Undo çekirdeği · 20b komutların yayılması · 20c profiling · 20d CI+tema |

### Sıralamada dikkat

- **Undo/Redo'dan (20) önce 0.2 → 0.3 zinciri.** Undo komutları "hangi
  entity'ler üzerinde?" sorusunu cevaplar; tek-seçim varsayımıyla
  yazılırsa her komut yeniden yazılır.
- `Simulate` modu fizik gelmeden anlamsız — 17c'ye bırakıldı.
- 17c'nin collider debug çizimi 18b'nin `DrawCircle`'ını bekliyor;
  18d ise 0.4'ün dosya izleyicisini kullanacak.

---

## 6. Öneriler

**Motor varlıklarının `.meta`'ları kaynakta durmalı.** `.meta` dosyaları
`build/bin/assets` altında üretiliyor ve kaynağa geri yazılmıyor; kaynakta
yoksa her temiz derlemede varlık **yeni GUID** alıyor. 0.1'de
`circle.png.meta` bu yüzden elle kaynağa alındı. Motora yeni bir varlık
eklerken `.meta`'sını da commit et.

**Faz 16'dan önce küçük bir örnek oyun yaz.** Motor artık sahne
düzenleyip çalıştırabiliyor; basit bir şey (topla-kaç) yapmak hangi
API'lerin eksik olduğunu tasarım tartışmasından daha hızlı gösterir.
Bu yüzden 16c'ye eklendi.

**İçerik paneli görünüm tercihi kaydedilmiyor** — proje her açılışta
Izgara'ya dönüyor. `editor.json`'a bir satır; 0.6'da halledilebilir.

---

## 7. Mimarinin taşıyıcı fikirleri

Yeni bir şey eklerken bunlara uy:

**Kimlik ile konum ayrı şeylerdir.** Üç kez uygulandı: entity (Faz 8,
`UUID` ≠ `entt::entity`), proje (Faz 21, kurulu olduğun yer ≠ çalıştığın
yer), varlık (Faz 22, GUID ≠ dosya yolu).

**Aynı bilgi iki yerde durmasın.** Component listesi (`AllComponents`),
serileştirme (`EntitySerialization`), içerik paneli etkileşimleri
(`DrawEntryInteractions`) hep bu yüzden tek yerde toplandı. İki kopya
er geç ayrışır.

**Yapıyı değiştiren işlemler döngü dışında.** ImGui listesi gezilirken
`rename`/`destroy` çağırmak yineleyicileri bozar; istekler biriktirilip
çerçeve sonunda işlenir.

**Modal diyalog ImGui çerçevesinin ortasında açılmaz.** Yerel diyalog
programı bloklar; istek bayrağa yazılıp çerçeve bittikten sonra açılır.

**Sessiz yanlışlık yerine görünür uyarı** — ama uyarı doğru olmalı.
Geçersizleşen bir uyarıyı silmek, tutmaktan iyidir.

**Editörün varsayımı motora sızmasın.** Motor editörü bilmez; `EditorCamera`,
`FileDialogs`, paneller hep `Editor/` altında.
