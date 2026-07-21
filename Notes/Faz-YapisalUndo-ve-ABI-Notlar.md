# Yapısal Undo + Motor/DLL ABI damgası

> Borç turu, 2026-07-21 (üçüncü oturum). İki ayrı borç, tek oturumda:
> A5'in kapsamadığı **yapısal işlemler** ve Aşama B'nin bıraktığı
> **sessiz DLL uyumsuzluğu**.

---

## 1. Neden bu ikisi

Aşama A + B + script alanları bittikten sonra açık kalan borçların
listesi çıkarıldı (bkz. 00-Durum-ve-Plan.md §4). İkisi 16c'ye
başlamadan kapatılmalıydı:

- **Yapısal Undo yok.** A5 yalnızca alan düzenlemesini ve gizmo
  dönüşümünü geri alabiliyordu. Yani kullanıcının en çok korktuğu işlem
  — yanlışlıkla silmek — geri alınamıyordu. Örnek oyun yazarken sahne
  kurmak sürekli entity ekleyip silmek demek.
- **DLL uyumsuzluğu sessizdi.** `ScriptableEntity`'ye `OnReflect`
  eklendiğinde vtable değişti; eski `Game.dll` uyumsuz hale geldi ama
  bunu **hiçbir yer söylemiyordu**. "Bir kez Derle'ye bas" bilgisi
  kodda değil devir notunda yazıyordu. Motorun kendi kuralı bunu
  reddediyor: *sessiz yanlışlık yerine görünür uyarı.*

---

## 2. Yapısal Undo

### 2.1 Asıl sorun: silineni geri getirmek

Alan düzenlemesini geri almak kolaydı — eski değeri closure'da tut,
geri yaz. Silmeyi geri almak farklı: silinen şey bir entity **ağacı**,
component'leriyle, hiyerarşisiyle, UUID'siyle.

**Karar: JSON metni olarak dondur.** Yeni bir "entity kopyalama" yolu
yazmak yerine sahne kaydetmenin zaten doğrulanmış yolunu kullanıyoruz.

Gerekçe: ikinci bir yol yazsaydık, yeni bir component eklendiğinde biri
güncellenip diğeri unutulurdu — A1'in çözdüğü hatanın aynısı. Bedeli
birkaç kilobayt metin ve bir ayrıştırma; Undo adımı başına hiçbir şey.

### 2.2 Motor tarafı — `EntitySnapshot`

Yeni public API (`Engine/include/FXEngine/Scene/EntitySnapshot.h`):

| Sınıf | İş |
|---|---|
| `EntitySnapshot` | Bir entity + **tüm alt ağacı**; `Capture` / `Restore` |
| `ComponentSnapshot` | Tek component'in verisi; component sil/geri al için |

**UUID'ler korunuyor.** Geri konan entity *aynı* entity'dir, benzeri
değil — başka entity'lerin ona tutan `EntityRef` referansları geri alma
sonrasında da hedefini bulur. (Faz 8'in taşıyıcı fikri: kimlik ile konum
ayrı şeylerdir.)

İki gerçek incelik:

1. **İki geçişli restore.** Önce herkes var olsun, sonra parent bağları
   kurulsun. Tek geçişte kurulsaydı bir çocuğun parent'ı henüz oluşmamış
   olabilirdi — sahne yüklemenin öğrendiği ders.
2. **Kökün parent'ı hâlâ yaşıyorsa bağ da geri geliyor.** Ağacın
   ortasından bir dal silindiğinde geri alma dalı *yerine* koymalı, köke
   değil. Parent artık yoksa entity kökte kalıyor, çökmüyor.

`ApplyComponents`/`SerializeEntity` içindeki component gövdesi
`SerializeComponent`/`ApplyComponent` olarak ayrıldı — `ComponentSnapshot`
onları kullanıyor, ikinci bir kopya oluşmadı.

**Testler (`Tests/EntitySnapshot.test.cpp`, 5 test):** aynı UUID ile geri
gelme, alt ağaç + dışarıdaki parent bağı, iki kez restore'un ikinci kopya
üretmemesi, component değerinin (varsayılanın değil) geri gelmesi, sahip
olunmayan component'in boş snapshot vermesi.

### 2.3 Editör tarafı — `Structural`

`Editor/src/Commands/StructuralCommands.{h,cpp}`. Beş giriş noktası:

```
CreateEntity(ad, parent)              PushCreated(entity, etiket)
DestroyEntities(entity listesi)
AddComponent(info, hedefler)          RemoveComponent(info, hedefler)
```

**Neden ayrı bir dosya?** Aynı işlem üç yerden tetikleniyor: hierarchy
sağ tık menüsü, Inspector'daki `X`, Delete tuşu. Üç yere ayrı komut
yazmak, birini unutmanın cezasını sessiz yapardı — "bazen geri alınıyor,
bazen alınmıyor".

**Komutlar entity'yi UUID ile tutuyor, entt tutamağı ile değil.** Silinen
bir entity geri konunca entt kimliği değişir, UUID değişmez. Gizmo
komutu (A5) tutamak tutabiliyordu çünkü orada hiçbir şey silinmiyor.

`PushCreated` ayrı bir giriş: prefab örnekleme ve viewport'a doku bırakma
entity'yi **kurduktan sonra** kaydediyor, çünkü redo'nun geri koyacağı
snapshot son hali olmalı.

### 2.4 Bağlanan yerler

| Nereden | Komut |
|---|---|
| Hierarchy → Boş Entity Oluştur | `CreateEntity` |
| Hierarchy → Alt Entity Ekle | `CreateEntity` (parent'lı) |
| Hierarchy → Entity'yi/Seçilenleri Sil | `DestroyEntities` |
| Delete tuşu (`EditorApp::DeleteSelection`) | `DestroyEntities` |
| Inspector → Component Ekle | `AddComponent` |
| Inspector → component başlığındaki `X` | `RemoveComponent` |
| Viewport'a doku bırakma | `PushCreated` |
| Viewport'a prefab bırakma | `PushCreated` |

Hepsi çoklu seçimle çalışıyor ve **tek bir Undo adımı** oluşturuyor:
üç entity silmek üç kez Ctrl+Z gerektirmiyor.

### 2.5 Yan düzeltme: "Alt Entity Ekle" döngü içindeydi

Menü, registry view'ı üzerinde gezilirken entity yaratıp parent bağı
kuruyordu. Eski kod parent bağını erteliyordu ama yaratmayı ertelemiyordu.
İstek artık `m_CreateChildOf`'ta birikip döngü bittikten sonra
işleniyor — motorun "yapıyı değiştiren işlemler döngü dışında" kuralı.

### 2.6 `RebindScene`

Sahne işaretçisi altı farklı yerde değişiyor (Play, Stop, NewScene,
OpenScene, projesiz mod, Sahneyi Sıfırla). Yapısal komutların da sahneyi
bilmesi gerekince yedinci bir "burayı da güncelle" noktası doğdu.
Hepsi tek bir `EditorApp::RebindScene()` altına alındı — birinde unutulan
bağlama, komutların **ölü bir sahneye** dokunması demekti.

Undo geçmişi her sahne değişiminde zaten temizleniyor (`m_Commands.Clear()`),
yani bir komutun yakaladığı sahne her zaman yaşayan sahne.

---

## 3. ABI damgası

`Engine/include/FXEngine/Core/EngineABI.h` → `FX_ENGINE_ABI_VERSION` (şu an **1**).

Akış:

```
Engine header  ──derleme zamanı──►  Game.dll: FXEngineABIVersion() = N
                                            │
Editor (kendi N'i) ◄── karşılaştır ─────────┘
```

- Üretilen `GameMain.cpp` sabiti `FXEngineABIVersion` olarak dışa aktarıyor.
- `GameLibrary::Load` **script kaydından önce** kontrol ediyor. Uyumsuzsa
  DLL bırakılıyor, `FXRegisterScripts` hiç çağrılmıyor: uyumsuz bir DLL'in
  script'ini kaydetmek, Play'e basıldığında yanlış vtable girişine
  dallanmak demek.
- **Damgası olmayan DLL uyumsuz sayılıyor** — bu damgadan önceki her
  derleme öyle. Bedeli bir kez fazladan derleme.
- Uyumsuzluk `HasAbiMismatch()` ile ayırt ediliyor. "Henüz derlenmemiş
  proje" ile "yeniden derlenmesi gereken proje" ayrı şeyler: ilki normal
  (sessiz log), ikincisi **derleme konsolunu açıyor + durum çubuğuna
  yazıyor**.

**Sayı ne zaman artırılır** (header'da da yazıyor): `ScriptableEntity`'ye
sanal fonksiyon eklenince/çıkınca/sıraları değişince, `ScriptFieldVisitor`
arayüzü değişince, DLL sınırından geçen bir tipin yerleşimi değişince.

---

## 4. Bilerek yapılmayanlar

- **Parent değiştirme (sürükle-bırak) geri alınamıyor.** Yapısal ama
  yıkıcı değil; kullanıcı elle geri sürükleyebiliyor. Sonraki tura.
- **Entity yeniden adlandırma geri alınamıyor** (Inspector'daki isim
  kutusu). Tag alanı A1 tablosunda yapısal işaretli olduğu için generic
  alan komutundan geçmiyor.
- **Undo hâlâ script alanlarını kapsamıyor** — `NativeScriptComponent::Fields`
  A1 tablosunda değil. Ayrı bir iş.
- **ABI sayısı elle artırılıyor.** Otomatik (header hash'i gibi) bir
  şey denenmedi: yanlış pozitif üretip her derlemede "yeniden derle"
  demesi, hiç uyarmamaktan daha kötü olurdu.

---

## 5. Test

Motor: `FXTests` **56 test / 266 assertion** — hepsi geçiyor.
Editör tarafı elle test edilecek (bkz. devir notu).
