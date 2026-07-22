# Faz C-1 — Prefab bağı (salt okunur bağlantı)

> Prefab sisteminin (03-Yapilacaklar.md **C**) ilk parçası. Amaç dar
> tutuldu: örnek ile kaynağı arasında **bağı kur, kalıcı kıl, göster** —
> Apply/Revert/override yok (onlar C-2…C-4).

Son güncelleme: 2026-07-22

---

## Ne yapıldı

`Instantiate` bugüne kadar bağı **koparıyordu**: örnek kaynağını bilmezdi.
Artık her örnek entity kendi kaynağını taşıyor.

### Yeni component — `PrefabInstanceComponent`

Saf veri, iki **kimlik**:

```cpp
struct PrefabInstanceComponent
{
    AssetHandle Prefab{ 0 };   // hangi .fxprefab (GUID, yol DEGIL)
    UUID        SourceId{ 0 };  // o dosyadaki kaynak entity'nin UUID'i
};
```

İkisi de kimlik çünkü diske yazılabilen tek şey kimliktir (Faz 8/12
ilkesi). Yol saklamıyoruz: prefab dosyası taşınınca bağ kopmasın diye
GUID. `SourceId` alt ağaçtaki **her** düğüme kendi kaynağıyla basılıyor —
C-2 Revert bunu kaynak↔örnek eşlemesi olarak kullanacak.

### Kayıt — tek yer, `ComponentMeta.cpp`

```cpp
Register<PrefabInstanceComponent>("PrefabInstance", "Prefab Baglantisi")
    .Extra(SavePrefabLink, LoadPrefabLink)   // iki UUID; alan tablosu UUID bilmiyor
    .HiddenInAddMenu()                        // menuden eklenmez
    .NotInInspector();                        // genel dongude cizilmez
```

- **`.Extra`**: iki UUID'yi JSON'a yazar/okur (SpriteRenderer'ın doku
  GUID'i ile aynı desen). `SerializedByTable` açık kaldığı için sahne
  dosyasına otomatik giriyor, `Scene::Copy` `CopyTo` ile taşıyor.
- **Yeni builder `.NotInInspector()`**: `ShowInInspector=false` yapar ama
  `Structural()`'dan farkı — **dosyaya yazılmaya devam eder**. Bağın hem
  serileşmesi hem de Inspector'da genel component olarak *görünmemesi*
  gerekiyordu; ikisini birden veren bir bayrak yoktu.
- **`NotRemovable` bilerek KONMADI**: "Bağı Kır" aynı yapısal
  `RemoveComponent` yolunu kullanabilsin diye. `NotInInspector` olduğu
  için genel 'X' zaten hiç çizilmiyor, yani `Removable=true`'nun görünür
  bir yan etkisi yok.

### Örnekleme damgası — `PrefabSerializer::Instantiate`

Prefab handle bir kez `AssetManager::GetHandle(filepath)` ile alınıyor,
döngüde her yeni entity'ye `Prefab` + kendi `SourceId`'si basılıyor.
Handle geçersizse (proje dışı prefab) bağ yine kuruluyor, Inspector
"kayıp" gösteriyor.

### Kayıt sırasında bağı temizleme — `PrefabSerializer::Save`

Bir prefab **örneğini** yeni bir prefab olarak kaydedebiliriz; kaynak
dosya başka bir prefab'a bağlı kalmamalı. `Save` her entity'nin JSON'undan
`PrefabInstance`'ı siliyor (kökün `Parent`'ını sildiği gibi).

### Sahne dosyası sürüm 4 → 5

Anlam değişikliği (sahneler artık prefab bağı taşıyabilir). Eski sahneler
sorunsuz açılıyor (bağ yoksa component yok); kaydedilince 5'e geçerler.

### Inspector — kimlik bloğunda göster

`SceneHierarchyPanel::DrawComponents`, UUID/Isim/Parent'ın hemen altında:

- **"Prefab: Kutu"** (mavi) + hover'da tam yol tooltip'i
- Kaynak bulunamıyorsa **"Prefab: &lt;kayıp kaynak&gt;"** (turuncu uyarı) —
  sessiz gizleme yerine görünür uyarı
- **"Bağı Kır"** düğmesi → örneği **tüm alt ağacıyla** kaynağından koparır
  (kökü koparıp çocukları bağlı bırakmak tutarsız olurdu), yapısal
  `RemoveComponent` ile Undo'ya bağlı

Component tablosunda `NotInInspector` olduğu için bu blok elle çiziliyor;
UUID/Tag/Parent gibi "kimlik" bilgisi, düzenlenebilir bir component değil.

---

## Testler

`Tests/Prefab.test.cpp` — **3 test / 28 assertion** (`[prefab]`):

1. **Örnek kaynağına bağlanıyor**: `Prefab == handle`, `SourceId ==`
   kaynak kök UUID'i, çocuk da kendi `SourceId`'siyle damgalı, örnek yeni
   kimlik almış.
2. **Sahne round-trip'i koruyor**: örnekle → kaydet → yükle → bağ aynı.
3. **Kaydetmek bağı yazmıyor**: örneği yeni prefab olarak kaydet, dosyada
   hiçbir entity'de `PrefabInstance` anahtarı yok.

Tüm paket: **80 test / 380 assertion** geçiyor (öncesi 77/352).

---

## Bilerek yapılmayanlar (sonraki parçalara)

- **Apply / Revert / override takibi** — C-2…C-4. Bağ bugün salt okunur.
- **Kaynak değişince açık örnekler tazelensin** — C-5.
- **"Aç" düğmesi** (prefab'ı düzenlemek için aç) — prefab düzenleme kipi
  ayrı bir iş; C-1'de yok. Bugün yalnızca ad + yol tooltip'i.
- **Nested prefab** (prefab içinde prefab örneği) — damga alt ağacın
  tamamına basılıyor ama iç içe kaynak zincirinin semantiği ele alınmadı.
- **Bağı Kır seçili tek entity yerine alt ağaca uygulanıyor** — bilinçli;
  Unity'nin "unpack" davranışına yakın ama "unpack completely" değil.
