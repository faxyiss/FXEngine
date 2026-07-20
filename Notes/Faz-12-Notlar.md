# Faz 12 — Varlık Yönetimi ve Prefab

## Dosyalar

| Dosya | Değişiklik |
|---|---|
| **`src/Scene/EntitySerialization.h/.cpp`** (yeni) | Entity ↔ JSON çevrimi, ortak |
| `Scene/SceneSerializer.cpp` | Ortak çevrimi kullanacak şekilde inceltildi |
| **`Scene/PrefabSerializer.h/.cpp`** (yeni) | Entity ağacını kaydet / örnekle |
| `Core/FileSystem.*` | `MakeRelativeToBase()` |
| **`Editor/src/Platform/FileDialogs.*`** (yeni) | Win32 aç/kaydet diyalogları |
| **`Editor/src/Panels/ContentBrowserPanel.*`** (yeni) | `assets/` gezgini |
| `Panels/SceneHierarchyPanel.*` | Texture yuvası artık bırakma hedefi; "Prefab olarak kaydet" |
| `EditorApp.*` | Yeni/Aç/Kaydet/Farklı Kaydet, son açılanlar, viewport bırakma |
| `ImGuiLayer.cpp` | Varsayılan düzene "Icerik" paneli |

---

## 1. Prefab = sahne, ama kimlikler yeniden üretilir

İki serileştirici neredeyse aynı işi yapıyor. Aralarındaki **tek** anlamlı fark
kimliğe ne olduğu:

| | Sahne yükleme | Prefab örnekleme |
|---|---|---|
| UUID | dosyadakini **koru** | **yeni üret** |
| Amaç | dünyayı aynen geri getir | şablondan yeni kopya |
| Mevcut içerik | silinir | korunur, üzerine eklenir |

Sahne yüklerken UUID korunmak *zorunda*: `FollowComponent`'teki hedef
kimliği ancak o zaman bir şeye denk gelir (Faz 8).

Prefab örneklerken UUID korunamaz: aynı prefab'i sahneye iki kez koyabilmen
gerekiyor ve iki entity aynı kimliğe sahip olamaz.

### ID remapping

Yeni kimlik üretmenin bedeli: prefab **içindeki** referanslar (parent, follow
hedefi) hâlâ dosyadaki eski kimlikleri gösteriyor. Örnekleme sırasında bir
`eski → yeni` tablosu tutup bu referansları çeviriyoruz.

```cpp
std::unordered_map<UUID, Entity> remap;
// 1. geçiş: her entity yeni UUID ile oluşturulur, remap[eskiID] = yeni
// 2. geçiş: parent bağları remap üzerinden kurulur
// 3. geçiş: Follow hedefleri remap üzerinden çevrilir
```

**Prefab'in dışına bakan referanslara dokunmuyoruz.** `remap`'te bulunmayan
bir hedef, sahnede duran gerçek bir entity olabilir ve kullanıcının kastettiği
şey odur. "Bulamadım, sıfırla" demek sessizce veri kaybettirirdi.

### Kökün parent'ı silinir
Prefab kaydedilirken kök entity'nin `Parent` alanı dosyaya **yazılmaz**.
Yazsaydık, prefab'i başka bir sahnede örneklerken çözülemeyen bir referans
olurdu.

---

## 2. Serileştirme kodunun paylaşılması

`SerializeEntity` ve `ApplyComponents` `Engine/src/Scene/EntitySerialization.h`'a
taşındı. `include/` altında **değil** — bu bir iç detay, motorun public API'si
değil. Yarın JSON'dan binary formata geçersek dışarıya hiçbir şey yansımaz.

Kopyalasaydık: yeni bir component eklendiğinde iki yerde güncellemek gerekirdi
ve biri er geç unutulurdu. "Prefab'ta neden `Velocity` kaydedilmiyor?" hatası
tam olarak böyle doğar.

---

## 3. Varlık kimliği: yol mu, UUID mi?

Bu fazın asıl kavramsal sorusu.

**Bizim yaptığımız (yol = kimlik):**
```json
"Texture": "assets/textures/circle.png"
```
Basit, dosya elle okunabilir, ekstra altyapı yok.
**Kırılganlık:** dosyayı taşırsan/yeniden adlandırırsan tüm referanslar kopar.

**Gerçek motorların yaptığı (UUID = kimlik):**
Her varlığın yanına bir `.meta` dosyası konur:
```
circle.png
circle.png.meta   ->  { "guid": 8412... }
```
Sahne dosyası yolu değil GUID'i saklar. Dosyayı taşıdığında `.meta` de onunla
gider ve referanslar hayatta kalır. Unity'nin `.meta` dosyaları tam olarak
budur; Unreal `.uasset` başlığında saklar.

**Neden şimdi yapmadık:** çalışması için bir *varlık veritabanının* (proje
taraması, GUID→yol tablosu, dosya izleyici, "meta'sı olmayan yeni dosya"
akışı) olması gerekiyor. Bu kendi başına bir faz. Teknik borç listesine
eklendi.

> Öğrenilecek genel ilke aynı: **kimlik ile konum ayrı şeylerdir.**
> Faz 8'de entity için öğrendik (UUID ≠ `entt::entity`), burada varlık için
> tekrar karşımıza çıktı.

---

## 4. Mutlak yol asla dosyaya yazılmaz

Dosya diyaloğu `C:\FXEngine\build\bin\assets\scenes\X.fxscene` döner.
Bunu sahne yoluna olduğu gibi yazsaydık proje başka bir makineye
kopyalandığında hiçbir şey bulunamazdı.

`FileSystem::MakeRelativeToBase()` bunu `assets/scenes/X.fxscene`'e çevirir —
`ResolveAsset`'in tam tersi.

```cpp
weakly_canonical(...)   // "..\" parçalarını ve sembolik bağları düzler
relative(full, base)
```
Düzleme adımı şart: `build/bin/../bin/assets` ile `build/bin/assets` aynı
yerdir ama düz karşılaştırma bunu göremez.

Dosya base klasörünün **dışındaysa** mutlak yol dönüyor ve uyarı basılıyor.
Sessizce yanlış bir göreceli yol üretmektense sorunu görünür kılmak daha iyi.

---

## 5. `OFN_NOCHANGEDIR`

Win32 dosya diyalogları varsayılan olarak **çalışma dizinini değiştirir**.
Bu bayrak olmadan bir sahne açtıktan sonra program başka bir dizinde
çalışıyormuş gibi davranır.

Bizde `FileSystem` yolları exe'ye göre çözdüğü için hasar sınırlı olurdu —
ama tam da bu yüzden hata *sessiz* kalır ve aylar sonra alakasız bir yerde
patlar. Win32'nin en çok unutulan bayraklarından biri.

---

## 6. Content browser: küçük resim = dosyanın kendisi

Ayrı bir ikon seti yok. `.png` dosyaları `TextureLibrary` üzerinden yüklenip
doğrudan `ImageButton` olarak çiziliyor; diğer dosyalar uzantıya göre renkli
düğme.

Yan faydası: tarayıcıda gördüğün her resim zaten önbelleğe girmiş oluyor,
sahneye sürüklediğinde ikinci kez diske gidilmiyor.

**`directory_iterator`'ın sırası işletim sistemine bağlıdır** (NTFS ile ext4
farklı sonuç verir). Klasörleri ve dosyaları ayrı gruplayıp kendimiz
sıralıyoruz, yoksa panel makineden makineye başka görünürdü.

### `ImGui::Begin`'in dönüşü kontrol ediliyor
Panel katlanmışsa ImGui widget'ları zaten atlar, ama **bizim** klasör okuma
ve doku yükleme işimiz yine de çalışırdı. Görünmeyen bir panel için disk
okumak boşuna.

---

## 7. Sürükle-bırak yükünde `'\0'` dahil gönderilir

```cpp
ImGui::SetDragDropPayload(kContentPayload, rel.c_str(), rel.size() + 1);
```
`+ 1` olmadan alan taraf `const char*` olarak okuyunca sonlandırıcı bulamaz
ve payload'ın dışına taşar. Entity payload'ında (Faz 9) bu sorun yoktu çünkü
orada sabit boyutlu bir `entt::entity` taşıyorduk.

Ayrıca entity payload'ı bir *tutamak* taşıyordu (sadece o kare geçerli);
bu payload bir *yol* taşıyor — kendi kendine yeten, kopyalanabilir bir veri.

---

## 8. Modal diyalog ImGui çerçevesinin ortasında açılmaz

"Prefab olarak kaydet" isteği panelde **biriktirilip** tüm paneller
çizildikten sonra işleniyor:

```cpp
if (FX::Entity root = m_HierarchyPanel.TakePrefabRequest()) { ... }
```

Yerel dosya diyaloğu programı **bloklar**. `Begin`/`End` çiftinin ortasında
açarsak ImGui'nin iç durumu o süre boyunca donar ve pencere yeniden
çizilmediği için diyalogun arkası siyah kalır.

Faz 9'daki "yapıyı değiştiren işlemler döngü dışında" kuralının aynısı,
farklı bir sebeple: orada yineleyici güvenliği, burada çerçeve bütünlüğü.

---

## 9. `editor.json` — kullanıcının verisi, sahnenin değil

Son açılan sahneler exe'nin yanında, `imgui.ini` ile aynı yerde tutuluyor.

Sahne dosyasına yazamazdık: bunlar sahnenin değil **kullanıcının** verisi ve
başka bir sahne açıldığında da geçerli kalmalı. Aynı ayrım `imgui.ini` için
de geçerli — panel düzeni projeye değil kişiye aittir.

Listeye alınırken var olmayan dosyalar eleniyor: tıklandığında hata verecek
bir menü öğesi göstermenin anlamı yok.

`std::deque` kullanıldı çünkü hem başa ekleyip hem sondan atıyoruz;
`vector`'de baş ekleme her seferinde tüm diziyi kaydırırdı.

---

## 10. Kaydedilmemiş sahne

`m_ScenePath` artık **boş başlıyor** (Faz 7'deki sabit yol kalktı).
`Ctrl+S` yol yoksa "Farklı Kaydet" diyaloğunu açıyor.

Sessizce varsayılan bir yola yazmak, kullanıcının dosyasını nereye
koyduğumuzu bilmemesi demek olurdu.

---

## Test sonuçları

Prefab yuvarlak yolculuğu (geçici öz-test ile doğrulandı, sonra kaldırıldı):

```
Prefab kaydedildi: assets/prefabs/Tank.fxprefab (3 entity)
Prefab orneklendi: ... (3 entity)     x2
ornek1=1319662813890401005  ornek2=876974452053469450   -> kimlikler farkli
cocuk1=1  cocuk2=1                                       -> hiyerarsi korundu
Sahne kaydedildi: SelfTest.fxscene (37 entity)   -> 31 + 2x3
Sahne yuklendi:   SelfTest.fxscene (37 entity, 6 hiyerarsi bagi)
```

Dosya doğrulaması:
```
Root 6025627949449957460
Tank Govde  | Parent (yok)              | Tex assets/textures/checkerboard.png
Tank Kule   | Parent 6025627949449957460 | Tex assets/textures/circle.png
Tank Namlu  | Parent 15543082939155263147| Tex (yok)
```

Yol göreceleştirme:
```
<exe>/assets/scenes/X.fxscene  ->  assets/scenes/X.fxscene
C:\Windows\notepad.exe         ->  (uyari) mutlak yol korundu
```

Çalışma: GL hatası yok, temiz kapanış.

## Onay
- [x] Derlendi (uyarısız)
- [x] Prefab kaydet/örnekle, kimlik yeniden eşleme
- [x] Yol göreceleştirme
- [x] GL hatası yok
- [ ] Kullanıcı onayı

## Ertelenenler → teknik borç
- **AssetManager (UUID tabanlı varlık kimliği)** — `.meta` dosyaları + varlık
  veritabanı gerektiriyor, kendi fazı olmalı.
- Linux/macOS dosya diyalogları (`FileDialogs.cpp` orada boş gövde derliyor).
- Content browser'da yeniden adlandırma / silme / klasör oluşturma.
- Prefab **bağlantısı** yok: örnek, kaynağından bağımsız. Gerçek prefab
  sistemlerinde örnek kaynağa bağlı kalır ve kaynak değişince güncellenir
  (üzerine yazılan alanlar "override" olarak saklanır).
