# FXEngine — Yapılacaklar

> **Yaşayan liste.** Yol haritası (`01-Yol-Haritasi-v2.md`) fazları anlatır,
> bu dosya *sıradaki somut işleri* tutar. Bir iş bitince buradan silinir,
> yerine faz notu yazılır.
>
> Oluşturma: 2026-07-21, 16c örnek oyunu (`FirstFxGame` topla-kaç) yazıldıktan
> sonra. Maddelerin çoğu o oyunu yazarken **çarparak** bulundu.

---

## A. Motor delikleri — oyun yazmayı fiilen engelliyor

### A-1. Script'ten entity silme ✅ (2026-07-22)

`Scene::DestroyEntityDeferred` + `FlushDestroyQueue` (OnUpdate sonunda),
`ScriptableEntity::Destroy()/Destroy(other)`, oyun sırasında silinen
entity'de `OnDestroy` çağrılıyor. 5 birim testi. Silme artık kare
sonunda, view gezilirken değil.

**Kalan (A-1b):** runtime'da entity *yaratma* (spawn). `CreateEntity`
zaten var ama script'ten çağrılınca aynı `view` sorunu — yeni entity
de gecikmeli kuyruğa girmeli ya da A-2 (prefab) ile birlikte çözülmeli.

### A-2. Runtime'da spawn ✅ (2026-07-22)

**Karar: dosya-tabanlı prefab değil, sahne içi prototipten spawn.**
`ScriptableEntity::Instantiate(Entity/EntityRef prototype)` →
`Scene::DuplicateEntity`. Dosya yolu / GUID / TextureLibrary sorunlarının
üçünü de atlıyor: doku zaten shared_ptr olarak yüklü, yeni kimlik üretme
`DuplicateEntity`'de test edilmiş. A-3 ile birleşiyor — prototip bir
`EntityRef` alanında tutulup Inspector'dan atanıyor.

**View güvenliği:** `ScriptSystem::Update` artık canlı view yerine
entity'lerin **anlık kopyası** üzerinde geziyor → script içinde spawn
etmek yineleyicileri bozmuyor, aynı karede spawn edilen bir kare bekliyor.
`ScriptSystem::StartPending` (Scene::OnUpdate sonunda) spawn edilenin
`OnCreate`'ini o karenin sonunda çağırıyor, `OnUpdate` sonraki kareden.
Spawn **anında** dönüyor, script hemen konumunu ayarlayabiliyor.

Kullanım:
```cpp
FX::EntityRef m_MermiPrefab;   // OnReflect ile Inspector'a
FX::Entity m = Instantiate(m_MermiPrefab);
m.GetComponent<FX::TransformComponent>().Translation = AtesNoktasi();
```
2 birim testi (`[spawn]`).

**Dikkat (bilinen sınır):** prototip sahnede gerçek bir entity olduğu
için **kendi script'i de Play'de çalışır**. Bir mermi prototipi ekranda
durup kendi mantığını koşturur. Çözüm: bir "aktif/pasif" (enabled) bayrağı
ya da prototipi ekran dışına koymak. Şimdilik ikincisi. → yeni madde F/A.

### A-3. Script alanı olarak entity referansı ✅ (2026-07-22)

`ScriptFieldVisitor::Visit(name, EntityRef&)`, `ScriptFieldValue::Type::Entity`
(UUID tutar), Inspector'da entity seçici, uçtan uca serileştirme + apply.
`EntityRef` kendi header'ına çıkarıldı (`Components.h ↔ ScriptFields.h`
dairesel bağımlılığını kırmak için). Entity seçici combo'su
`EntityRefCombo` olarak component ve script alanlarınca paylaşılıyor.
Birim testi: reflect → serialize → deserialize → apply → resolve.

Script artık `FindEntityByName` yerine:
```cpp
FX::EntityRef m_Target;
void OnReflect(FX::ScriptFieldVisitor& v) override { v.Visit("Hedef", m_Target); }
// kullanim: GetScene()->FindEntityByUUID(m_Target.Target)
```

---

## B. Editör — sahne düzenleme

### B-1. Hiyerarşide sıra değiştirme ✅ (2026-07-22)

`Entity::MoveInParent(±1)` hem çocukları (`RelationshipComponent.Children`)
hem de **kökleri** (`Scene::m_RootOrder`) kaydırıyor. Hiyerarşi menüsünde
"Yukarı/Aşağı Taşı" her entity'de, Undo'ya bağlı. Serileştirme hiyerarşi
(DFS) sırasında yazıyor + kök listesi → sıra kaydet/yükle sonrası
korunuyor. Panel kökleri `GetRootEntities` ile diziyor. Kök listesi
`CreateEntity`/`SetParent`/`DestroyEntity`/`Copy`'de bakımlı. 6 birim testi.

Sahne dosyası formatı **değişmedi** (sürüm 4): kök sırası ayrı bir alan
değil, entity'lerin dosyadaki yazılma sırasından okunuyor — eski sahneler
sorunsuz açılıyor.

**Sürükle-bırak (2026-07-22):** `Scene::PlaceEntity(moved, parent, index)`
ilkeli — detach + index'e ekle, döngüsel reddediyor. Hiyerarşide bir
entity'yi başka bir satırın **üst ucuna** (önüne), **alt ucuna** (arkasına)
ya da **ortasına** (çocuğu) bırakabiliyorsun; ince sarı çizgi/çerçeve nereye
düşeceğini gösteriyor (Unity davranışı). Kökler arası da çalışıyor.
`Structural::ReorderTo` ile Undo'ya bağlı. 3 birim testi (`[place]`).

Bu, ileride **render sıralaması** (18c: aynı Z'deki sprite'ların çizim
sırası) için de temel — sıra artık kullanıcının denetiminde ve kalıcı.

### B-2. Hiyerarşide kopyala / yapıştır / çoğalt ✅ (2026-07-22)

`Scene::DuplicateEntity` (yeni UUID'ler, iç referansları içe çeviriyor,
kaynağın kardeşi), Ctrl+D çoğalt / Ctrl+C-Ctrl+V kopyala-yapıştır,
hiyerarşi menüsü + Düzen menüsü, hepsi Undo'ya bağlı, çoklu seçim tek adım.
Ortak `Detail::RemapReferences` hem Follow hem A-3 script referanslarını
çeviriyor — bu arada **PrefabSerializer'ın script EntityRef alanını ıskalayan
gizli açığı da kapandı**. 5 birim testi.

**Kalan:** pano UUID tutuyor (aynı sahne içi); sahneler arası kopyalama
snapshot gerektirir — sonraki tur.

### B-3. Undo'nun kalanı

Yapısal ekle/sil kapsandı (2026-07-21). Kalanlar:

- [ ] Parent değiştirme (sürükle-bırak)
- [ ] Entity yeniden adlandırma (Inspector'daki isim kutusu)
- [ ] Script alanları (`NativeScriptComponent::Fields` A1 tablosunda değil)

---

## C. Prefab sistemi — çekirdek TAMAM (C-1…C-4), yapısal kısım kaldı

**Bugünkü durum:** bağ (C-1), Revert (C-2), override takibi (C-3) ve
Apply (C-4) çalışıyor. 16c'nin "beş yıldızı tek tek düzelt" acısı kapandı:
bir örnekte değiştir → Apply → kaynak + diğer örnekler güncellenir,
override'lar korunur. Kalan: yapısal değişikliklerin (çocuk/component
ekle-sil) semantiği ve kenar durumlar (C-5).

### C-1. Bağ + Inspector'da göster ✅ (2026-07-22)

`PrefabInstanceComponent { AssetHandle Prefab; UUID SourceId; }` (saf veri,
`ComponentMeta`'da tek yere kayıt, yeni builder `.NotInInspector()`).
`Instantiate` alt ağacın **her** entity'sine kendi `SourceId`'siyle
damga basıyor; `Save` bağı dosyaya yazmıyor (kaynak başka prefab'a bağlı
kalmasın). Sahne **sürüm 5**. Inspector'da kimlik bloğunda "Prefab: &lt;ad&gt;"
+ hover yol + **Bağı Kır** (alt ağaç, Undo'ya bağlı). Kayıp kaynak turuncu
uyarı. 3 birim testi (`[prefab]`). Ayrıntı:
[Faz-C1-Notlar.md](Faz-C1-Notlar.md).

### C-2. Revert (kaynak → örnek) ✅ (2026-07-22)

`PrefabSerializer::RevertInstance` — `SourceId` eşleşen örnek entity'lerin
component'lerini kaynaktan yeniden yazıyor. Örnekte eklenen component'ler
kalkıyor, silinenler geri geliyor, iç referanslar örnek kimliğine yeniden
eşleniyor. Korunan: UUID'ler, hiyerarşi, bağ, **kök konumu**. Inspector'da
**Revert** düğmesi (`InstanceRoot` ile kökten), Undo destroy+restore
snapshot ile. 2 birim testi. Ayrıntı:
[Faz-C2-Notlar.md](Faz-C2-Notlar.md).

### C-3. Override takibi ✅ (2026-07-23)

`FX::PrefabOverrides` — **diff tabanlı** (açık takip değil): kaynak dosya
önbellekli (`last_write_time` ile kendiliğinden tazelenir), fark her
soruluşta hesaplanıyor. `OverriddenFields` + `RevertField` (tek alan).
EntityRef alanları uzay çevrimiyle kıyaslanıyor (örnek↔kaynak kimliği).
Inspector'da sapan alanın etiketi **prefab mavisi**, değer widget'ına sağ
tık → "Kaynak degerine dondur" (çoklu seçim + Undo). 3 birim testi.
Ayrıntı: [Faz-C3-Notlar.md](Faz-C3-Notlar.md).

### C-4. Apply (örnek → kaynak) ✅ (2026-07-23)

`PrefabOverrides::ApplyInstance` — örneği kaynak dosyaya yazar (kimlikler
kaynak uzayında kalır, kök konumu sızmaz) ve sahnedeki **diğer örnekleri**
tazeler: override edilmemiş alanlar + Extra blob (doku, script alanları)
yeni kaynağa çekilir, override'lar korunur. Undo: eski dosya baytları +
diğer örnek snapshot'ları. Inspector'da **Apply** düğmesi. 2 birim testi.
16c'nin "beş yıldızı tek tek düzelt" acısı kapandı. Ayrıntı:
[Faz-C4-Notlar.md](Faz-C4-Notlar.md).

### Kalan (C-5)

- [ ] **C-5 Yapısal + kenar durumlar** — örnekte eklenen/silinen çocuk
      entity ve component'lerin Apply/Revert/yayılma semantiği; kaynak
      dışarıdan değişince açık sahnedeki örnekler tazelensin (FileWatcher);
      prefab silinirse "kayıp" uyarısı davranışı; nested prefab; doku ve
      script alanı override GÖSTERGESİ (yayılma zaten çalışıyor);
      alan başına Apply

**Not:** en büyük editör işi. C-1 bitti; en pahalı parça C-3/C-4 (override
semantiği + yazma yolu).

---

## D. Görünürlük ve render

### D-1. Ekranda metin (Faz 19)

16c'de skor **yalnızca log'da**. Ekranda yazı olmadan yapılan şey oyun gibi
hissettirmiyor.

- [ ] Bitmap font yükleme + `Renderer2D::DrawText`
- [ ] Dünya uzayında mı ekran uzayında mı? (ikisi de gerekecek)

### D-2. Sıralama katmanı (Faz 18c)

Aynı Z'deki sprite'ların sırası **keyfî** (EnTT view sırası + `GL_LESS`).
16c'de üç nesne olduğu için çarpmadı; on nesnede çarpar.

- [ ] `SortingLayer` + `OrderInLayer` alanları
- [ ] Çizmeden önce sıralama
- [ ] Sprite'larda derinlik yazmayı kapat (`glDepthMask(GL_FALSE)`)

---

## E. Sistemler

- [ ] **17a-d fizik/çarpışma.** 16c'de mesafe kontrolü elle yazıldı
      (`Distance(...) < 0.8`). Dikdörtgen çarpışma, duvar, itme yok.
      En büyük iş; alt fazlara bölünmüş durumda
- [ ] **14 sprite sheet / 15 animasyon**
- [ ] **23 ses**
- [ ] **Sahne geçişi.** 16c'de "yeniden başla" yok; oyun durumu global
      `static`'lerde ve Stop→Play ile sıfırlanıyor. Seviye/menü kavramı yok

---

## F. Küçük editör / motor borçları

- [ ] **Entity "aktif/pasif" (enabled) bayrağı.** A-2 spawn prototipi
      sahnede kendi script'ini de çalıştırıyor. Pasif entity: render yok,
      script yok, ama sahnede durur ve prototip olarak kopyalanabilir.
      Unity `GameObject.SetActive(false)`. Küçük ama birçok yerde işe yarar.

- [ ] **Derlenmemiş script uyarısı** — dosya `assets/`'te var ama `Game.dll`'de
      yok; şu an hiçbir yerde görünmüyor. Dosya sayısı ↔ kayıt sayısı
      karşılaştırması yeterli
- [x] **"Yeni Script" → `.h` + `.cpp` seçeneği** ✅ (2026-07-22) Modalda
      onay kutusu; işaretliyse `<ad>.h` (bildirim) + `<ad>.cpp` (gövde)
      üretiliyor, ikisi de açılıyor. `.cpp` içindeki `#include "<ad>.h"`
      aynı klasörden çözülüyor (doğrulandı: proje DLL'i derlendi, kayıt
      header'dan alındı). Tek header hâlâ varsayılan.
- [ ] **İçerik panelinde "Yeni Dosya"** (herhangi bir uzantı)
- [ ] `ContentBrowserPanel.cpp` ~1400 satır — bölünebilir

---

## G. Bilinen hatalar

- [ ] **ASCII-dışı karakterli proje yolu açılmıyor.** `C:\...\Benim Oyunum çğü\`
      açılmıyor, `.fxbuild` bile üretilmiyor. Sebep: `main(int, char**)` ANSI
      `argv` + dar `std::filesystem::path`. Çözüm `wmain`/UTF-16 ya da manifest
      ile UTF-8. **Geçici kural: Türkçe klasör adı kullanma**
- [ ] `Renderer2D` global durum (`s_Data` statik); batch bölme yolu hacky
- [ ] `GetRegistry()` çok açık — doğrudan `create`/`destroy` UUID haritasını bozar
- [ ] `FollowSystem` `Scene`'e bağımlı (diğer sistemler yalnız registry alıyor)
- [ ] `glLineWidth > 1.0` garanti değil (seçim çerçevesi 2.0 istiyor)
- [ ] Linux/macOS: dosya diyalogları boş gövde, dosya izleyici yok, hiç derlenmedi
- [ ] `Renderer2D` ve `TextureLibrary` test dışı (OpenGL bağlamı ister)

---

## Önerilen sıra

```
A-1  script'ten entity sil/yarat        ← model deliği, küçük, testi yazılabilir
A-2  runtime prefab örnekleme
A-3  script alanı olarak entity referansı
     ─────────── burada "script gerçek oyun yazabiliyor" eşiği geçilir
B-1  hiyerarşide sıra          B-2  kopyala/yapıştır
C    prefab bağı + Inspector + Apply/Revert   ← en büyük editör işi, ikiye bölünecek
D-1  metin        D-2  sıralama katmanı
E    17 fizik → 14/15 animasyon → 23 ses
```

**Gerekçe:** A grubu bir *özellik* değil, motorun eksik kalan bir
kavramı — kapanmadan yazılacak her oyun 16c'nin yaptığı numarayı
tekrarlamak zorunda. B ve C editörü kullanılabilir kılıyor; C en büyük
kazancı veriyor ama en pahalısı, o yüzden A'dan sonra.
