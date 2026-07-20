# FXEngine — Durum ve Plan

> **Bu dosya bir devir notudur.** Yeni bir sohbete başlarken önce bunu oku;
> projenin nerede olduğunu, nelerin neden böyle yapıldığını ve sıradaki
> işi buradan öğrenirsin.
>
> Son güncelleme: 2026-07-20 · Son commit: `5d2b18a`

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

MVP (Faz 0–7) ve şu fazlar tamam: **8, 9, 10, 11, 12, 21, 22** + Faz 18'in
çizgi/ızgara kısmı.

### Bu oturumda yapılanlar (17 commit)

| Commit | İş |
|---|---|
| `f03d78b` | Faz 12 bitirildi: doku yükleme hataları, varsayılan Nearest |
| `1b0d469` `59751bc` | Viewport kamerası: fare ile kaydırma, imlece zoom, Q/E kaldırıldı |
| `f209fbc` | **Faz 18 (kısmi):** çizgi render, ızgara, seçim çerçevesi, F odaklan |
| `930a5bb` `11e8343` `1c759d9` | **Faz 10:** Play/Stop, `Scene::Copy`, `CameraComponent`, `EditorCamera` |
| `c736e8c` | Kamera gizmosu (görüş alanı + ikon, viewport'tan seçilebilir) |
| `7021b9f` `863dc25` | **Faz 21:** proje sistemi, `.fxproject`, karşılama ekranı |
| `e24d0db` `4f3e184` `cd7f042` | İçerik paneli: taşıma, liste görünümü, çoklu seçim |
| `b1d7d7a` `c346c32` `5d2b18a` | **Faz 22:** `AssetManager`, GUID kimlik, `.meta` dosyaları |

Toplam: 43 dosya, ~4850 satır eklendi.

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
  taşıma, içe aktarma, önizleme

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

Derleme: `cmake --build build --config Debug`
Çalışan editör exe'yi kilitler — `Get-Process FXEditor | Stop-Process -Force`.

---

## 4. Bilinen sorunlar ve teknik borç

Öncelik sırasına göre:

### Yüksek
| Sorun | Nerede | Not |
|---|---|---|
| **Dosya izleyici yok** | `ContentBrowserPanel`, `AssetManager` | Explorer'dan yapılan değişikliği tablo görmez. Varlık sistemi GUID'e geçtiği için artık daha kritik. `ReadDirectoryChangesW` gerekli. |
| **Undo/Redo yok** | editörün tamamı | Gizmo ile yanlış sürükleme geri alınamıyor. Komut deseni; editörün tamamına dokunan mimari değişiklik. |
| **Seçim `SceneHierarchyPanel` içinde** | `SceneHierarchyPanel.h` | Viewport, gizmo, inspector hepsi okuyor; panel sahibi değil tüketicisi olmalı. Entity çoklu seçiminin ön adımı. |

### Orta
| Sorun | Nerede |
|---|---|
| Prefab / `StartScene` referansları hâlâ yol tabanlı | `PrefabSerializer`, `Project` |
| Inspector'da doku ayarları arayüzü yok (`.meta` elle düzenleniyor) | altyapı hazır: `UpdateTextureSettings`, `Reload` |
| İçerik paneli görünüm tercihi kaydedilmiyor | `editor.json`'a yazılabilir |
| `AssetDirectory` gerçekten kullanılmıyor (`assets` varsayılıyor) | `ContentBrowserPanel` |
| Prefab bağlantısı yok (örnek kaynağından bağımsız) | `PrefabSerializer` |
| Linux/macOS dosya diyalogları boş gövde | `FileDialogs.cpp` |

### Düşük
- `Renderer2D` global durum (`s_Data` statik), batch bölme yolu hacky
- `Shader::FromFiles` ham `new` döndürüyor
- `GetRegistry()` çok açık — doğrudan `create`/`destroy` UUID haritasını bozar
- Hiç birim testi yok (Catch2 en çok serializer ve matematikte işe yarar)
- Sadece Windows'ta denendi
- `glLineWidth` > 1.0 garanti değil (seçim çerçevesi 2.0 istiyor)

---

## 5. Faz planı

### Sıradaki: **Faz 13 — Girdi soyutlaması**
Küçük ve Faz 16'nın önkoşulu.
- `FX::Input` (`IsKeyPressed`, `GetMousePosition`), SDL'den bağımsız
  `KeyCode`/`MouseButton` enum'ları
- Motor içi `Event` sınıfları, SDL → motor olayı çeviri katmanı
- `Layer`/`LayerStack`
- Girdi eşlemesi ("Zıpla" = Space **veya** gamepad A)

> **Öğrenilecek:** platform soyutlaması nerede biter, aşırı soyutlamanın
> maliyeti.

### Sonra: **Faz 16 — Native script component**
Motorun ilk defa gerçekten *oyun* çalıştırdığı yer. Önkoşulları (Faz 10
play modu, Faz 13 input) hazır olacak.
- `NativeScriptComponent` (`OnCreate`/`OnUpdate`/`OnDestroy`)
- `ScriptableEntity` taban sınıfı
- `ScriptSystem` — Play'de çalışır, Edit'te çalışmaz
- Örnek: `PlayerController`, `FollowTarget`

### Sonra sırasıyla
| Faz | İş | Not |
|---|---|---|
| 14 | Sprite sheet / `SubTexture2D` | 15'in önkoşulu |
| 15 | Animasyon (`AnimationClip`, `AnimatorComponent`) | |
| 17 | Fizik (Box2D), `Simulate` modu buraya | Faz 10'da `Simulate` bilerek ertelendi |
| 18 | Kalan render: `DrawCircle`, sıralama katmanı, shader hot reload | çizgi/ızgara kısmı yapıldı |
| 19 | Metin (font atlası) ve ses (miniaudio) | |
| 20 | Undo/Redo, profiling, testler, CI | Undo'yu geciktirme, pahalılaşıyor |

### Sıralamada dikkat

- **Undo/Redo'dan (Faz 20) önce entity çoklu seçimi yapılmalı.** Undo
  komutları "hangi entity'ler üzerinde?" sorusunu cevaplar; tek-seçim
  varsayımıyla yazılırsa her komut yeniden yazılır.
- **Entity çoklu seçiminden önce `SelectionContext`** ayrılmalı (seçim
  panelden çıkarılmalı). İçerik panelindeki dosya çoklu seçimi bitti,
  entity tarafı yapılmadı.
- `Simulate` modu fizik gelmeden anlamsız — Faz 17'ye bırakıldı.

---

## 6. Öneriler

**Bir sonraki oturumda ilk yapılacak:** kullanıcının `Game1` projesini
(`Desktop/FXEngineProjects/FxProject/`) açıp sürüm ≤3 sahnelerin sorunsuz
geldiğini doğrula. Faz 22'de sahne formatı 4'e çıktı; dönüşüm test
projesiyle doğrulandı ama gerçek veriyle değil.

**`.meta` dosyaları sürüm kontrolüne girmeli.** `.gitignore`'da `*.meta`
varsa kaldır — `.meta` kaybolursa varlık yeni GUID alır ve tüm referanslar
kopar.

**Dosya izleyiciyi erken yap.** Küçük bir iş (bir thread + `ReadDirectoryChangesW`)
ama varlık tablosu artık diskle senkron olmak zorunda; şu an bu tutarlılık
tamamen içerik panelinin doğru çağrı yapmasına bağlı ve bu kırılgan.

**Faz 16'dan önce küçük bir örnek oyun yaz.** Motor artık sahne
düzenleyip çalıştırabiliyor; basit bir şey (topla-kaç) yapmak hangi
API'lerin eksik olduğunu tasarım tartışmasından daha hızlı gösterir.

**Birim testleri (Catch2) Faz 20'yi beklemesin.** `SceneSerializer`,
`AssetManager` ve `UUID` en çok fayda sağlayacak yerler; ikisi de artık
karmaşık ve elle test etmesi pahalı.

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
