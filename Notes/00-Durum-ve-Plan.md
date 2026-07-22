# FXEngine — Durum ve Plan

> **Bu dosya bir devir notudur.** Yeni bir sohbete başlarken önce bunu oku;
> projenin nerede olduğunu, nelerin neden böyle yapıldığını ve sıradaki
> işi buradan öğrenirsin.
>
> Mimarinin tamamı ve alınan yön kararları: **[02-Mimari.md](02-Mimari.md)**
>
> Sıradaki somut işler: **[03-Yapilacaklar.md](03-Yapilacaklar.md)**
>
> Son güncelleme: 2026-07-22 · Son commit: `9fa6694`
> **Aşama A + B, script alanları, 16c örnek oyun, A/B backlog grupları ve
> B-7 tamam.** Bu oturumda yapılanlar için **§2 "Son oturum"**, sıradaki iş
> için **[03-Yapilacaklar.md](03-Yapilacaklar.md)** ve **§5b**'ye bak.

---

## 1. Proje nedir

C++17 ile yazılan bir **2D oyun motoru + editör**. SDL3, OpenGL 3.3, EnTT,
ImGui, glm, nlohmann/json. Windows'ta geliştiriliyor (Linux'ta hiç
derlenmedi).

```
Engine/     motor kütüphanesi (FXEngine.lib) — editörü BİLMEZ
Editor/     editör uygulaması (FXEditor.exe) — motora bağlıdır
Tests/      FXTests.exe — yalnızca Engine'i test eder
Notes/      her fazın tasarım notları (Türkçe)
build/      CMake çıktısı
```

**Tek yönlü bağımlılık:** Engine → hiçbir şey, Editor → Engine.

> **B-1 sonrası:** `FXEngine` artık **paylaşımlı** (`FXEngine.dll`).
> Gerekçe [Asama-B-Plan.md](Asama-B-Plan.md) §1, uygulaması
> [Faz-B-Notlar.md](Faz-B-Notlar.md). Pratik sonuç: motorun `static`
> **veri**'sine header'daki bir gövdeden dokunma —
> `WINDOWS_EXPORT_ALL_SYMBOLS` veri sembolü aktarmaz, link kırılır.
> Erişimci `.cpp`'de tanımlanmalı.

---

## 2. Nerede kaldık

MVP (Faz 0–7) ve şu fazlar tamam: **8, 9, 10, 11, 12, 21, 22**,
**borç turu 0.1–0.7**, **13a + 13b**, **16a + 16b + 16c**, Faz 18'in
çizgi/ızgara kısmı, **Aşama A (A1–A5)**, **Aşama B (B-1…B-7)**,
**script alanları**, **yapısal Undo + ABI damgası**, ve
**[03-Yapilacaklar.md](03-Yapilacaklar.md) A + B grupları**.

### Aşama A + B oturumları (2026-07-21)

| Commit | İş |
|---|---|
| `4caa520` | **0.7** Refactor turu: `EditorApp.cpp` 2033→4 dosya, demo sahne ayrıldı |
| `e8d9d5c` | **A1** Component meta-veri sistemi (`ComponentRegistry`) |
| `ee41e55` · `50473d0` | **A2** Game View / Scene View ayrımı + genel oynatma şeridi |
| `e7e036d` | `EngineLogo` uygulama logosu |
| `34d59da` · `c62dabe` | **A3** Project Settings + Preferences |
| `136d8bc` · `671915d` | **A4** Script dosyası oluşturma + otomatik kayıt |
| `34f27db` · `46da685` | **Aşama B planı** yazıldı ve onaylandı |
| `cdabcde` | **B-1** `FXEngine` → `SHARED`; `Project::GetActive/HasActive` `.cpp`'ye taşındı |
| `5673bd0` | **B-2** `<proje>/.fxbuild/` iskelesi + `Game.dll` + `FXEngineConfig-<Config>.cmake` |
| `867b414` | **B-3** `GameLibrary` DLL yükleme + `FXEngineSelfTest` (EnTT sınır ölçümü GEÇTİ) |
| `0f00b4f` | **B-6** `Editor/src/Scripts/` kaldırıldı, "Yeni Script" projeye yazıyor |
| `3f72180` | **B-4** gölge kopya (`out/loaded/`) · **B-5** Derle düğmesi + konsol + Play koruması |

### Son oturumda yapılanlar (2026-07-21, ikinci tur)

| Commit | İş |
|---|---|
| `b4ffffd` | Space artık duraklatmıyor · Game paneli araç çubuğu yüzen overlay'e geçti (görünmüyordu) · "Yeni Script" içerik panelinde **istenen klasöre** yazıyor + script taraması **özyinelemeli** (`GLOB_RECURSE`) |
| `6e49cec` | Derleme konsolu: hataları **en üste kırmızı, satır kaydırmalı özet** olarak gösteriyor (dosya+satır görünür) |
| `d576ca0` | İçerik paneli: **kopyala / kes / yapıştır** (Ctrl+C/X/V) |
| `604d24d` | **Teknik borç:** çoklu seçimde Inspector artık hepsini düzenliyor · yüklenemeyen doku için **mor "eksik doku"** |
| `5beb62d` | Çoklu seçim: **component ekleme/silme** de tüm seçili entity'lere uygulanıyor |
| `496b5ba` | **A5 Undo/Redo** — `CommandStack` + Inspector alan düzenleme + gizmo dönüşümü |
| `2469052` | **Script alanları** — `OnReflect` reflection + `NativeScriptComponent::Fields` |

### Borç turu (2026-07-21, üçüncü tur)

[Faz-YapisalUndo-ve-ABI-Notlar.md](Faz-YapisalUndo-ve-ABI-Notlar.md)

| İş | Sonuç |
|---|---|
| **Yapısal Undo** | `EntitySnapshot`/`ComponentSnapshot` (motor) + `Structural` komutları (editör). Entity oluştur/sil, component ekle/sil, prefab ve doku bırakma geri alınabiliyor; çoklu seçim tek adım |
| **ABI damgası** | `FX_ENGINE_ABI_VERSION`; eski `Game.dll` **yüklenmeden önce** yakalanıyor, konsol + durum çubuğunda uyarı |
| **B-7 oyun projesi** | `.cpp` dosyaları da derleniyor; script tespiti dosya adına değil **içeriğe** bakıyor, script olmayan header'lar artık derlemeyi kırmıyor ([Faz-B7-Notlar.md](Faz-B7-Notlar.md)) |

### Son oturumda yapılanlar (2026-07-22) — 16c sonrası backlog turu

16c örnek oyunu (`C:\FXProjects\FirstFxGame`, topla-kaç) yazılınca çıkan
eksikler **[03-Yapilacaklar.md](03-Yapilacaklar.md)**'a toplandı ve A + B
grupları kapatıldı. Ayrıntı o dosyada; özet:

| Commit | İş |
|---|---|
| `94eeb1e` | **[03-Yapilacaklar.md](03-Yapilacaklar.md)** — 16c'de çarpan eksiklerin tam listesi (A motor delikleri, B editör, C prefab, D metin/sıralama, E fizik…) |
| `024b5f4` | **A-1** Script'ten entity silme: `Scene::DestroyEntityDeferred` + kare-sonu kuyruğu, `ScriptableEntity::Destroy()`. 16c'de yıldızı silemeyip ışınlıyorduk |
| `5812d8c` | **A-3** Entity referansı script alanı: `Visit(EntityRef&)` + Inspector seçici. `FindEntityByName` yerine Inspector'dan hedef. `EntityRef` kendi header'ına çıktı |
| `91626c6` · `ebdb5ed` | **B-2** çoğalt/kopyala-yapıştır (`Scene::DuplicateEntity`, Ctrl+D/C/V) · **B-1** hiyerarşide sıra (çocuk + kök, kaydet/yükle'de korunuyor) |
| `a41b1fe` | **Sürükle-bırak sıralama** (Unity tarzı): satır arasına/üzerine bırakma, `Scene::PlaceEntity`. 18c render sırasının temeli |
| `065adf8` | **A-2** Runtime spawn: `ScriptableEntity::Instantiate(prototip)` = `DuplicateEntity`. `ScriptSystem::Update` artık anlık kopya üzerinde geziyor (spawn güvenli) |
| `9fa6694` | **"Yeni Script" → `.h` + `.cpp` seçeneği** (B-7'nin açtığı bölünmüş yapı editörden) |

Ayrıca: **izin allowlist'i** (`.claude/settings.json`) — cmake/git/FXTests
artık sormuyor. Ortak **`Detail::RemapReferences`** yazılınca
PrefabSerializer'ın script EntityRef alanını ıskalayan gizli açığı da
kapandı.

**A ve B grupları BİTTİ.** Kalan: A-2'nin prototip-kendi-script'ini-çalıştırma
sınırı (F: aktif/pasif bayrağı), sürükle-bırakta iki satır arası ince ayar.

### Şu an çalışan özellikler

- **Proje sistemi:** karşılama ekranı, `.fxproject` **sürüm 3**
  (ad, varlık klasörü, `StartScene` GUID, hedef çözünürlük), son projeler.
  `FXEditor.exe <yol.fxproject>` ile doğrudan açılabiliyor
- **Varlık sistemi:** GUID kimlik, `.meta`, taşıma referansları kırmıyor,
  doku ayarları dosya başına, dosya izleyici
- **Sahne:** entity UUID'leri, hiyerarşi, prefab, sürüm 4 dosya formatı
- **Component'ler tek yerde tanımlı (A1):** `ComponentMeta.cpp` →
  `RegisterBuiltins`. Serileştirme, `Scene::Copy` ve Inspector üçü de
  oradan besleniyor. Yeni component = **tek yere yazmak**
- **Scene View / Game View ayrı (A2)** · **Ayarlar ayrı (A3)**
- **Oyun kodu ayrı DLL (Aşama B):** `<proje>/assets/` altındaki `.h` ve
  `.cpp` dosyaları `Game.dll`'e derleniyor, **Derle** düğmesiyle editör
  kapanmadan yeniden yükleniyor (gölge kopya). Derleme hataları konsolda
  dosya+satır. Script = `FX::ScriptableEntity`'den türeyen sınıf (B-7);
  yardımcı header'lar serbest
- **Script alanları:** `OnReflect` ile bildirilen alanlar Inspector'da,
  değer component'te veri olarak, Play'de instance'a uygulanıyor
- **Undo/Redo:** Ctrl+Z / Ctrl+Shift+Z — Inspector alanları, gizmo (A5)
  **+ yapısal işlemler:** entity oluştur/sil, component ekle/sil, prefab
  ve doku bırakma. Çoklu seçim tek adım
- **Eski `Game.dll` yakalanıyor:** motor ABI damgası uyuşmazsa DLL
  yüklenmiyor, konsol açılıp "Derle'ye bas" deniyor
- **Çoklu seçim:** alan düzenleme + component ekle/sil hepsine uygulanıyor
- **İçerik paneli:** kopyala/kes/yapıştır, yeniden adlandır, sil, sürükle-bırak
- **Play/Stop:** kopya sahnede, Edit modu durağan
- **Girdi:** `FX::Input` (sorgu) + `FX::Event` (olay)
- **Script'ten oyun kontrolü:** entity silme (`Destroy`), spawn
  (`Instantiate(prototip)`), hedef referansı (`EntityRef` alanı)
- **Testler:** `FXTests` **77 test / 352 assertion**

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
| ~~**Undo/Redo yok**~~ | ✅ A5: alan düzenleme + gizmo · ✅ **yapısal**: entity oluştur/sil, component ekle/sil, prefab/doku bırakma, **çoğaltma, hiyerarşi taşıma/sıralama** ([Faz-YapisalUndo-ve-ABI-Notlar.md](Faz-YapisalUndo-ve-ABI-Notlar.md), [03-Yapilacaklar.md](03-Yapilacaklar.md)). Kalan: yeniden adlandırma, script alanları |
| ~~**Eski `Game.dll` sessizce uyumsuz**~~ | ✅ `FX_ENGINE_ABI_VERSION`: uyumsuz DLL yüklenmiyor, konsol + durum çubuğunda uyarı |

### Orta
| Sorun | Nerede |
|---|---|
| ~~Script alanları Inspector'dan ayarlanamıyor~~ | ✅ `OnReflect` + `NativeScriptComponent::Fields` ([Faz-ScriptAlanlari-Notlar.md](Faz-ScriptAlanlari-Notlar.md)). **Not:** vtable değişti, projelerde bir kez "Derle" gerekiyor |
| Prefab bağlantısı yok (örnek kaynağından bağımsız, override sistemi yok) | `PrefabSerializer` — sıradaki büyük iş (03-Yapilacaklar.md C) |
| A-2 spawn prototipi kendi script'ini çalıştırıyor (aktif/pasif bayrağı yok) | `03-Yapilacaklar.md` F |
| Linux/macOS dosya diyalogları boş gövde, dosya izleyici yok | `FileDialogs.cpp`, `FileWatcher.cpp` |
| ~~Çoklu seçimde Inspector yalnızca birincili düzenliyor~~ | ✅ kapatıldı: değişen alan + component ekleme/silme tüm seçili entity'lere uygulanıyor (`ComponentDrawer`, `SceneHierarchyPanel`) |

### Düşük
- **Türkçe/ASCII-dışı karakterli proje yolu açılmıyor** (2026-07-21'de
  ölçüldü). `C:\...\Benim Oyunum\` (boşluklu ASCII) **çalışıyor**;
  `C:\...\Benim Oyunum çğü\` **açılmıyor**, `.fxbuild` bile üretilmiyor.
  Sebep büyük olasılıkla `main(int, char**)` ANSI `argv` + dar string
  `std::filesystem::path` (Win32 A-API'leri). Çözüm: `wmain`/UTF-16 ya da
  `SetConsoleCP`/manifest ile UTF-8. **Türkçe klasör adı kullanma.**
- `Renderer2D` global durum (`s_Data` statik), batch bölme yolu hacky
- `ContentBrowserPanel.cpp` ~1400 satır — bölünebilir (acil değil)
- `GetRegistry()` çok açık — doğrudan `create`/`destroy` UUID haritasını bozar
- `Renderer2D` ve `TextureLibrary` test dışı (OpenGL bağlamı ister)
- Sadece Windows'ta denendi
- `glLineWidth` > 1.0 garanti değil (seçim çerçevesi 2.0 istiyor)
- ~~Yüklenemeyen doku görünmez kalıyor~~ ✅ mor "eksik doku" gösteriliyor (`TextureLibrary`)

---

## 5. Faz planı

Fazlar 2026-07-20'de **alt fazlara bölündü** ve önlerine bir borç
kapatma turu (0.x) eklendi. Ayrıntı: `01-Yol-Haritasi-v2.md`.

### Güncel sıra

```
✅ borç turu 0.1–0.7  ✅ 13a  ✅ 13b  ✅ 16a  ✅ 16b  ✅ A1  ✅ A2  ✅ A3  ✅ A4
✅ Aşama B (B-1…B-6)  ✅ A5 Undo/Redo  ✅ Script alanları (OnReflect)
✅ Yapısal Undo  ✅ Motor/DLL ABI damgası  ✅ B-7 (oyun projesi .h+.cpp)
▶  16c örnek oyun  →  18c sıralama katmanı  →  17 fizik  →  14/15 animasyon
   → sonra C: C# kararı (aşağıdaki değerlendirmeye bak)
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
| 0.7 | A1 öncesi refactor turu — [Borc-07-Refactor.md](Borc-07-Refactor.md) | ✅ `EditorApp.cpp` 2033→4 dosya, demo sahne ayrıldı, `.meta` yolu tek yerde |

**2026-07-21: yön kararı alındı.** Editör temel araçları örnek oyunun
önüne alındı, C# scripting ertelendi. Gerekçeler ve mimarinin tamamı:
**[02-Mimari.md](02-Mimari.md)** — yeni bir şey eklemeden önce oku.

13c/13d ertelendi, gerekçesi [Faz-13-Notlar.md](Faz-13-Notlar.md)'de.

---

## 5b. YENİ OTURUMDA İLK İŞ

Aşama A/B, script alanları, 16c örnek oyun ve backlog A + B grupları
**bitti**. Sıradaki iş **"Önerilen sıra"**da (aşağıda): D-1 metin ya da
18c render sıralaması. Somut madde listesi **[03-Yapilacaklar.md](03-Yapilacaklar.md)**'da.

### Aşama C (C#) değerlendirmesi — 2026-07-21 (hâlâ geçerli)

K1'in ön koşulları (A + B) tamamlandı, yani karar noktası geldi.
**Değerlendirme sonucu: şimdi C# yapılmamalı.** Gerekçeler:

1. **C#'ın asıl gerekçesi ortadan kalktı.** K1 "asıl acı nokta dil değil
   iterasyon hızı" diyordu; Aşama B onu çözdü (Derle → ~saniyeler).
2. **Hangi API'lerin gerektiği henüz bilinmiyor.** K6'nın dediği gibi
   örnek oyun (16c) yazılmadan eksikler görünmez. Doğrulanmamış bir API
   yüzeyi için interop bağlaması yazmak, oyun yazılınca **iki kez**
   yazmak demek.
3. **Oyun yapmayı engelleyen şey dil değil:** fizik (17), animasyon
   (14/15), ses (23), metin (19), sıralama katmanı (18c) yok. C# bunları
   1–2 ay dondurur.

C#'ın gerçek kazancı **crash güvenliği** (C++ script hatası editörü
düşürür, C# sadece exception verir) ve reflection'ın bedava gelmesi.
Bunlar gerçek ama şu an 1–2 ayı hak etmiyor.

**Ara yol olarak Lua** düşünülebilir: entegrasyon gün mertebesinde,
derleme adımı yok, script hatası çökertmez. Bedeli yine bağlama katmanı.

**Karar: önce 16c örnek oyun yaz (C++), sonra dil kararını gerçek veriyle
ver.**

### 16c yazıldı — çıkan eksikler

`C:\FXProjects\FirstFxGame` (topla-kaç: Player + 5 Star + Enemy, dört
script + ortak `shared/Game.cpp`). Amacına ulaştı; çıkan eksiklerin
tamamı **[03-Yapilacaklar.md](03-Yapilacaklar.md)**'da. En kritik üçü:

1. **Script'ten entity silinemiyor/yaratılamıyor** — `ScriptSystem` view
   gezerken registry değişemez. Yıldız silinemedi, uzağa taşındı.
2. **Script alanı olarak entity referansı yok** — her karede
   `FindEntityByName("Player")`, hem `O(n)` hem yeniden adlandırmada
   sessizce bozuluyor.
3. **Prefab bağı yok** — `Star`'a script atayınca beş örneği tek tek
   düzeltmek gerekti.

### Önerilen sıra

16c yazıldı ve çıkan eksiklerin **A + B grupları kapatıldı** (yukarıdaki
"Son oturum" tablosu). Kalan sıra:

1. ~~16c örnek oyun~~ ✅ · ~~A grubu (A-1/A-2/A-3)~~ ✅ · ~~B grubu~~ ✅
2. **D-1 metin** (ekranda skor) — 16c'de somut eksikti, skor sadece log'da
3. **18c render sıralaması** — hiyerarşi sırası altyapısı artık hazır
   (`PlaceEntity` + kök/çocuk sırası kalıcı); "çizim sırası = hiyerarşi"
   ya da `OrderInLayer` bunun üstüne oturur
4. **C prefab bağı + Apply/Revert** (en büyük editör işi, ikiye bölünecek)
5. **17 fizik** → **14/15 animasyon** → **23 ses**
6. Sonra Aşama C'yi (C#) yeniden değerlendir.

Gerekçeler ve maddelerin tamamı: **[03-Yapilacaklar.md](03-Yapilacaklar.md)**

### Aşama C (C#) — hâlâ ertelendi

K1'in ön koşulları (A + B) tamamlandı ama karar değişmedi: **şimdi C#
yapılmamalı.** İterasyon hızını Aşama B çözdü; oyun yapmayı engelleyen
şey dil değil eksik sistemler (fizik, animasyon, ses, metin). Ayrıntı
aşağıda (§ "Aşama C değerlendirmesi").

### Yarım kalan / bilinen sorunlar

- **Türkçe karakterli proje yolu açılmıyor** (§4'te ayrıntı). Boşluk
  sorun değil, ASCII-dışı karakter sorun.
- **A-2 spawn prototipi kendi script'ini de çalıştırıyor** — sahnedeki
  gerçek bir entity olduğu için. Çözüm: entity "aktif/pasif" bayrağı
  (03-Yapilacaklar.md F). Şimdilik prototipi ekran dışına koy.
- **Undo hâlâ kapsamıyor:** entity yeniden adlandırma, script alanları.
  (Yapısal ekle/sil/çoğalt/taşıma ve gizmo/alan kapsandı.)
- **Eski `Game.dll` artık sessiz değil** ama yine de bir kez **Derle**
  gerekiyor: ABI damgası uyumsuzluğu tespit ediyor, otomatik derlemiyor.
- **Sürükle-bırakta** iki satır arası ince ayar dışında hiyerarşi tam.

---

## 5c. (Arşiv) Aşama B tamamlandı

**Plan:** [Asama-B-Plan.md](Asama-B-Plan.md) · **yapılanlar:**
[Faz-B-Notlar.md](Faz-B-Notlar.md).

Aşama B'nin altı adımı da bitti: motor `FXEngine.dll` (B-1), her proje
kendi `Game.dll`'ini derliyor (B-2), editör onu yükleyip EnTT tip
kimliğini sınırda ölçüyor (B-3, en ciddi risk geçti), editör artık hiç
oyun kodu taşımıyor (B-6), gölge kopya ile açıkken yeniden derleme
mümkün (B-4), oynatma şeridindeki **Derle** düğmesi cmake'i çalıştırıp
sonucu bir konsol panelinde gösteriyor ve Play sırasında önce Stop ediyor
(B-5).

**Kullanıcının onaylayacağı son GUI adımı:** script'i değiştir → Derle →
sahne/seçim kaybolmadan yeni kod çalışsın. **FXTest2'nin bozuk `Spin.h`'ı**
Derle → konsolda hata testinin örneği.

**Sıradaki: A5 Undo/Redo.** A1'in alan tablosu sayesinde generic "alanı
değiştir" komutu ucuzladı. 0.2/0.3 (seçim + çoklu seçim) zaten hazır.

**Neden A5'ten önce:** kullanıcı her script değişikliğinde editörü
yeniden derleyip yeniden başlatmak zorunda; bu her gün acıtıyor.
A5 (Undo) gecikmekten zarar görmüyor, A1'in alan tablosu onu zaten
ucuzlattı.

**Planın özeti (ayrıntı dosyada):**

1. Asıl engel "DLL nasıl yüklenir" değil, **motor durumunun tekliği**.
   `FXEngine` statik olduğu için exe ve `Game.dll` `ScriptRegistry`,
   `ComponentMeta`, `FileSystem`, `Project`, `Renderer2D` durumlarının
   **ayrı kopyalarını** taşırdı → DLL kendi kopyasına kaydeder, editör
   hiç görmez.
2. Çözüm: `FXEngine` → `SHARED`. **B-1 bu ve en riskli adım.**
3. Script'ler `<proje>/assets/scripts/` altına taşınır; sahne referansı
   **adla** kalır (Faz 16b kararı hot reload'ı mümkün kılan şey).
4. Gölge kopya ile DLL kilidi aşılır; Play sırasında yeniden yükleme
   **yasak** (vtable'lar geçersizleşir).
5. En ciddi risk: **EnTT tip kimliği DLL sınırında**. B-3'ün ilk işi bunu
   ölçmek — teoriye güvenilmeyecek.

**Kullanıcının verdiği kararlar:**
- `Spin`/`Move` editörde yerleşik **kalmayacak**, örnek projeye taşınacak
  (B-3 sırasında; öncesinde taşınırsa hiç derlenmezler)
- Derleme **elle "Derle" düğmesiyle** tetiklenecek, otomatik sonra

**Uyarı:** `Editor/src/Scripts/Jump.h` kullanıcının A4 testinden kalan
bir dosya. Aşama B'de script'ler projeye taşınırken silinecek ya da
örnek projeye gidecek.

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
