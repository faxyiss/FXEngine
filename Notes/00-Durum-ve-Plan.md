# FXEngine — Durum ve Plan

> **Bu dosya bir devir notudur.** Yeni bir sohbete başlarken önce bunu oku;
> projenin nerede olduğunu, nelerin neden böyle yapıldığını ve sıradaki
> işi buradan öğrenirsin.
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
| **Undo/Redo yok** | editörün tamamı | Gizmo ile yanlış sürükleme geri alınamıyor. Komut deseni; editörün tamamına dokunan mimari değişiklik. |

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

Fazlar 2026-07-20'de **alt fazlara bölündü** ve önlerine bir borç
kapatma turu (0.x) eklendi. Ayrıntı: `01-Yol-Haritasi-v2.md`.

### Güncel sıra

```
0.1 ✅ → 0.4 → 0.2 → 0.3 → 0.5
      → 13a-d → 16a-c → 0.6
      → 14 → 15 → 17a-d → 18b-d → 19 → 23 → 20a-d
```

### Borç turu (0.x)

| # | İş | Durum |
|---|---|---|
| 0.1 | Faz 22'nin gerçek veriyle doğrulanması | ✅ sahne v4, GUID eşleşiyor, eksik `.meta` eklendi |
| 0.4 | Dosya izleyici (`ReadDirectoryChangesW`) | ✅ GUID dışarıdan taşımada da korunuyor |
| 0.2 | `SelectionContext` — seçimi panelden çıkar | ✅ sahibi `EditorApp`, paneller tüketici |
| 0.3 | Entity çoklu seçimi | **sıradaki** |
| 0.5 | Catch2 + `UUID`/`SceneSerializer`/`AssetManager` testleri | |
| 0.6 | Faz 22 artıkları (Inspector doku ayarları, prefab GUID, `AssetDirectory`) | 16'dan sonra |

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
