# FXEngine — Yapılacaklar

> **Yaşayan liste.** Yol haritası (`01-Yol-Haritasi-v2.md`) fazları anlatır,
> bu dosya *sıradaki somut işleri* tutar. Bir iş bitince buradan silinir,
> yerine faz notu yazılır.
>
> Oluşturma: 2026-07-21, 16c örnek oyunu (`FirstFxGame` topla-kaç) yazıldıktan
> sonra. Maddelerin çoğu o oyunu yazarken **çarparak** bulundu.

---

## A. Motor delikleri — oyun yazmayı fiilen engelliyor

### A-1. Script'ten entity yaratma / silme ▲ EN ACİL

**Sorun:** `ScriptSystem::Update` `view<NativeScriptComponent>` üzerinde
gezerken script içinden `DestroyEntity` çağırmak yineleyicileri bozuyor.
16c'de yıldızı toplayınca **silemedik**, `9999, 9999`'a ışınladık.

Bu bir özellik eksiği değil **model deliği**: motorda "oyun sırasında dünya
değişir" kavramı yok. Mermi, düşman dalgası, patlama, can hakkı — hiçbiri
yazılamaz.

**Çözüm:** `Scene`'de gecikmeli komut kuyruğu. Script `Destroy(entity)` /
`Instantiate(...)` çağırır, istek biriktirilir, sistemler bittikten sonra
işlenir. Motorun zaten dört yerde uyguladığı *"yapıyı değiştiren işlemler
döngü dışında"* kuralının beşincisi.

- [ ] `Scene::DestroyEntityDeferred(Entity)` + kuyruk, `OnUpdate` sonunda boşaltma
- [ ] `ScriptableEntity` üzerinden erişim (`Destroy()`, `Destroy(other)`)
- [ ] Silinen entity'nin script'i: `OnDestroy` çağrılmalı
- [ ] Aynı entity iki kez silinirse çökmemeli
- [ ] Birim testi (penceresiz yazılabilir)

### A-2. Runtime'da prefab örnekleme

A-1'in ikizi. `PrefabSerializer::Instantiate` var ama script'ten
çağrılamıyor (editöre ait bir yol gibi duruyor).

- [ ] Script'ten `Instantiate(prefabAdı/GUID, konum)`
- [ ] Yeni entity aynı karede mi yaşamaya başlasın, sonraki karede mi? (karar)

### A-3. Script alanı olarak entity referansı

**Sorun:** 16c'deki üç script de her karede `FindEntityByName("Player")`
çağırıyor. Hem `O(n)`, hem kırılgan: entity'yi yeniden adlandırınca oyun
**sessizce** bozuluyor.

Parçalar zaten var: `EntityRef` (Faz 8), `FieldType::EntityRef` (A1).
Eksik olan tek şey `ScriptFieldVisitor`'da `Visit(name, EntityRef&)`.

- [ ] Visitor'a `EntityRef` aşırı yüklemesi
- [ ] Inspector'da entity seçici (component alanlarındakinin aynısı)
- [ ] Serileştirme (`ScriptFieldValue`'ya UUID türü)

---

## B. Editör — sahne düzenleme

### B-1. Hiyerarşide sıra değiştirme

Şu an sıra: çocuklar `RelationshipComponent`'in çocuk listesi sırasında,
**kökler ise `entt` view sırasında** — yani kullanıcının denetiminde değil.

- [ ] Çocuklar için "Üste Al" / "Alta Koy" (liste içinde kaydırma)
- [ ] Sürükle-bırakla iki satır **arasına** bırakma (şu an sadece "üzerine"
      bırakma var, o da parent yapıyor)
- [ ] **Kökler için sıra kavramı yok** — çözülmesi gerek: ya bir
      `SceneOrder` alanı ya da köklerin de bir listede tutulması.
      Sahne dosyası formatını etkiler (sürüm 5?)
- [ ] Sıra serileştirilmeli, yoksa her yüklemede karışır
- [ ] Undo'ya bağlanmalı

### B-2. Hiyerarşide kopyala / yapıştır / çoğalt

İçerik panelinde var (Ctrl+C/X/V), hiyerarşide **yok**.

- [ ] Ctrl+C / Ctrl+V / Ctrl+D (çoğalt) — entity ağacını kopyalar
- [ ] Kopyada **yeni UUID'ler** üretilmeli (prefab örneklemenin yaptığı gibi;
      `EntitySnapshot` UUID *korur*, bu yüzden doğrudan kullanılamaz — kimlik
      yeniden üretme adımı gerekiyor)
- [ ] İç referanslar kopyanın içine bakmalı (A→B referansı, kopyada A'→B')
- [ ] Yapıştırma hedefi: seçili entity'nin altına mı, kardeşi olarak mı? (karar)
- [ ] Undo'ya bağlanmalı

### B-3. Undo'nun kalanı

Yapısal ekle/sil kapsandı (2026-07-21). Kalanlar:

- [ ] Parent değiştirme (sürükle-bırak)
- [ ] Entity yeniden adlandırma (Inspector'daki isim kutusu)
- [ ] Script alanları (`NativeScriptComponent::Fields` A1 tablosunda değil)

---

## C. Prefab sistemi — bugün "kopyala-yapıştır"dan ibaret

**Bugünkü durum:** `Instantiate` prefab'ı sahneye açar ve **bağ kopar**.
Örnek, kaynağını bilmez; kaynak değişince örnekler güncellenmez; örnekte
yapılan değişikliğin "override" olduğu bilgisi hiçbir yerde durmaz.

16c'de bu doğrudan canımızı yaktı: `Star`'a script atadıktan sonra beş
yıldızın hepsini tek tek düzeltmek ya da prefab'ı yeniden kaydedip
hepsini silip yeniden sürüklemek gerekti.

- [ ] **Bağ:** örnekte prefab GUID'i + kaynak entity kimliği tutulmalı
      (yeni component: `PrefabInstanceComponent`?)
- [ ] **Inspector'da görünsün:** "Prefab: Star.fxprefab" başlığı + `Aç` düğmesi
- [ ] **Apply** — örnekteki değişiklikleri kaynağa yaz (tüm örnekler güncellenir)
- [ ] **Revert** — örneği kaynağa döndür
- [ ] **Override takibi:** hangi alan örnekte değiştirilmiş? Unity kalın yazıyla
      gösterir. Bu olmadan Apply "neyi uyguluyorum?" sorusunu cevaplayamaz
- [ ] Kaynak değişince açık sahnedeki örnekler tazelensin
- [ ] Prefab silinirse/taşınırsa örnekler ne olacak? (GUID koruyor ama karar gerek)
- [ ] Sahne dosyası formatı etkilenir — sürüm artışı

**Not:** bu, listedeki en büyük editör işi. Tek oturumda bitmez; en az
"bağ + Inspector'da göster" ve "Apply/Revert" diye ikiye bölünmeli.

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

## F. Küçük editör borçları

- [ ] **Derlenmemiş script uyarısı** — dosya `assets/`'te var ama `Game.dll`'de
      yok; şu an hiçbir yerde görünmüyor. Dosya sayısı ↔ kayıt sayısı
      karşılaştırması yeterli
- [ ] **"Yeni Script" → `.h` + `.cpp` seçeneği** (B-7 `.cpp`'yi açtı ama editör
      hâlâ tek header üretiyor)
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
