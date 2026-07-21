# Aşama B — Oyun DLL'i + hot reload (PLAN)

> Bu bir **plan**, uygulama notu değil. Kararlar onaylanınca uygulanacak
> ve sonra `Faz-B-Notlar.md` yazılacak.
>
> Tarih: 2026-07-21

## Amaç

Bugün bir script'i değiştirmek **editörü yeniden derleyip yeniden
başlatmayı** gerektiriyor. Hedef:

```
script'i düzenle → "Derle" → ~5 sn → yeni davranış çalışıyor
                                      (editör hiç kapanmadı)
```

**Başarı ölçütü tek cümle:** Editör açıkken bir script'in `OnUpdate`'ini
değiştirip Derle'ye bastığında, sahne ve seçim kaybolmadan yeni kod
çalışmalı.

---

## 1. Asıl engel: "DLL yap" demek yetmiyor

`FXEngine` bugün **statik** kütüphane (`add_library(FXEngine STATIC ...)`).
Statik kalırsa hem `FXEditor.exe` hem `Game.dll` motorun **kendi
kopyasını** taşır. Motorun süreç genelinde tek olması gereken durumu ise
`.cpp` dosyalarında statik olarak duruyor:

| Durum | Nerede | İkiye bölünürse |
|---|---|---|
| `s_Factories`, `s_Names` | `ScriptRegistry.cpp` | **DLL kendi kopyasına kaydeder, editör hiç görmez** |
| `s_Entries` | `ComponentMeta.cpp` | Script'in gördüğü component tablosu boş |
| `s_ProjectDir` | `FileSystem.cpp` | Script varlık yollarını yanlış çözer |
| `s_Active` | `Project.cpp` | Script projeyi göremez |
| `s_Data` | `Renderer2D.cpp` | Script'ten çizim yapılamaz |

Yani sorun "DLL nasıl yüklenir" değil, **motor durumunun tekliği**.

### Karar önerisi B1: `FXEngine` paylaşımlı kütüphane olur

`add_library(FXEngine SHARED ...)` + `CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS`.

Neden bu işe yarıyor: yukarıdaki durumların **hepsi** ya fonksiyon-içi
statik (`ComponentRegistry::Entries()`) ya da `.cpp` dosya kapsamlı ve
yalnızca dışa aktarılan fonksiyonlar üzerinden erişiliyor. Veri sembolü
dışa aktarmaya gerek yok — `WINDOWS_EXPORT_ALL_SYMBOLS`'un veri
sembollerini kapsamaması bizi vurmaz.

**Bedeli:** `FXEngine.dll` artık exe'nin yanında taşınmak zorunda; testler
de ona bağlanacak. `Faz 0`'daki "tek exe" sadeliği gidiyor.

**Alternatifler ve neden değil:**

| Alternatif | Neden hayır |
|---|---|
| DLL motoru hiç bağlamasın, host bir C API tablosu geçsin | Script yazmak `GetComponent<T>()` yerine tablo çağrısı olurdu; öğrenme amacına ve okunabilirliğe aykırı |
| DLL yalnızca fabrika tablosu dışa aktarsın, motor statik kalsın | Script kodu motoru **kullanıyor**: `Input::IsKeyPressed` DLL'in kendi kopyasını okurdu → girdi hep boş görünürdü. Sessiz ve teşhis edilmesi çok zor bir hata |

---

## 2. Kararlar

### B2 — Script'ler nerede yaşar

**Öneri:** `<proje>/assets/scripts/*.h`

- Kullanıcının içgüdüsü buydu (projede zaten boş bir `Scripts` klasörü açtı)
- İçerik panelinde görünür, proje ile birlikte taşınır, sürüm kontrolüne girer
- `AssetManager` `.h` uzantısını tanımıyor, dolayısıyla `.meta` üretmiyor —
  çakışma yok

**Sahne dosyası script'e GUID ile değil ADLA referans vermeye devam eder.**
GUID *dosyayı* tanımlar, sahnenin ihtiyacı olansa *sınıf*. Faz 16b'nin
kararı ("script'in kimliği adıdır") aynen geçerli ve hot reload'ı mümkün
kılan şey de bu: DLL değişse de ad aynı kalır.

Konvansiyon A4'ten devam: `Zipla.h` → `class FXEd::Zipla` → `"Zipla"`.

### B3 — Derleme nasıl tetiklenir

**Öneri:** Önce **elle "Derle" düğmesi** (oynatma şeridinde), otomatik
tetikleme sonra.

Gerekçe: dosya izleyiciyle otomatik derleme, yarım yazılmış bir dosyayı
derlemeye kalkıp sürekli hata basar. Elle tetikleme "ne zaman derlendiğini
biliyorum" duygusunu verir. Otomatik seçenek bir Tercih olarak sonradan
eklenebilir (`0.4`'ün `FileWatcher`'ı hazır).

Derleyen: `cmake --build` (CMake zaten şart, kullanıcıda var).

### B4 — DLL kilidi: gölge kopya

Windows yüklü DLL'i kilitler; üzerine yazılamaz. Standart çözüm:

```
build/Game.dll  →  kopyala  →  build/Game.loaded.dll  →  LoadLibrary
```

Derleme orijinali yazar, kilit kopyada. Yeniden yükleme: `FreeLibrary` →
kopyala → `LoadLibrary`.

Bunu bu oturumda bizzat yaşadık: çalışan `FXEditor.exe` yüzünden
`LNK1168: yazma için açılamıyor` aldık.

### B5 — Yeniden yükleme ne zaman YASAK

**Play sırasında yeniden yükleme yok.** Script örnekleri DLL'in kodunu
işaret eden vtable'lar taşıyor; DLL boşaltılınca hepsi geçersiz işaretçi
olur — çökme *sonra* ve alakasız bir yerde gelir.

Sıra kesin olmalı:

```
Derle isteği
  → Play modundaysan: önce Stop (kullanıcıya söylenerek)
  → ScriptSystem::OnRuntimeStop  (tüm Instance'lar silinir)
  → ScriptRegistry::Clear()
  → FreeLibrary
  → cmake --build
  → LoadLibrary + kayıtları oku
```

`NativeScriptComponent::Instance` ham işaretçi ve `nullptr`'a çekiliyor
(zaten `OnRuntimeStop` bunu yapıyor) — bu yüzden Stop'tan sonra sarkan
işaretçi kalmıyor. **Doğrulanacak:** `m_EditorScene` ve `m_RuntimeScene`
ikisinde de.

### B6 — Editörün yerleşik script'leri ne olur

`Spin` ve `Move` bugün `Editor/src/Scripts/` altında ve editöre derleniyor.

**Öneri:** Kalsınlar, "yerleşik örnekler" olarak. `ScriptRegistry` iki
kaynaktan beslenir: editörün yerleşikleri + projenin DLL'i. Aynı ad iki
yerde varsa mevcut davranış geçerli (ikincisi yok sayılır + uyarı).

Böylece projesiz modda da script denenebilir ve A4'ün ürettiği
`ScriptRegistrations.h` mekanizması boşa gitmez.

---

## 3. Adımlar

Her adımın kendi doğrulaması var; biri kırmızıysa sonrakine geçilmez.

| # | İş | Doğrulama |
|---|---|---|
| **B-1** | `FXEngine` → `SHARED`, `FXTests` ve `FXEditor` ona bağlanır | Derleme temiz, **51 test yeşil**, editör açılıyor. Davranış değişikliği sıfır olmalı |
| **B-2** | Boş bir `Game.dll` hedefi + `<proje>/assets/scripts/` tarama, `FXEngine`'e bağlı | DLL üretiliyor, editör onu **yüklemiyor** henüz |
| **B-3** | DLL'den `FXRegisterScripts()` dışa aktarımı; editör yükleyip kayıtları alıyor | Projedeki bir script Inspector listesinde görünüyor, Play'de çalışıyor |
| **B-4** | Gölge kopya + `FreeLibrary`/`LoadLibrary` döngüsü | Editör açıkken DLL elle yeniden derlenip "Yeniden Yükle" ile alınabiliyor |
| **B-5** | Editörden "Derle" düğmesi (`cmake --build`), çıktısı bir konsol panelinde | Derleme hatası **editörde okunuyor**, editör çökmüyor |
| **B-6** | Play koruması + otomatik Stop + durum mesajları | Play sırasında Derle → önce Stop, sonra derleme; çökme yok |

### Doğrulama için otomatik test

Çoğu adım GUI gerektiriyor ama **B-3 ve B-4 birim testlenebilir**:
`FXTests` içinde küçük bir test DLL'i üretip yükle/boşalt döngüsünü ve
kayıtların gelip gittiğini ölçmek mümkün. Bunu yapmak, "çalışıyor gibi
görünüyor"dan çıkmanın tek yolu.

---

## 4. Riskler

| Risk | Neden ciddi | Ne yapacağız |
|---|---|---|
| **EnTT tip kimliği DLL sınırında** | `registry.get<T>()` tip kimliğine dayanıyor; iki tarafta farklı çıkarsa component'ler **sessizce bulunamaz** | B-3'te ilk iş: DLL'den `GetComponent<TransformComponent>()` çağırıp gerçekten aynı veriye eriştiğini ölçmek. EnTT tip adı tabanlı hash kullanıyor, teoride tutmalı — **teoriye güvenmeyeceğiz** |
| **CRT uyuşmazlığı** | DLL'de `new`, motorda `delete` (bugün `OnRuntimeStop` böyle yapıyor) — farklı heap'lerde çökme | Her iki hedef de aynı dinamik CRT (`/MD`) kullanacak; ayrıca yıkımı DLL tarafına almayı değerlendireceğiz |
| **Sarkan script örneği** | DLL boşaltılırken bir `Instance` hayatta kalırsa çökme çok sonra gelir | B5'teki sıra zorunlu; boşaltmadan önce iki sahnede de `Instance == nullptr` doğrulanacak |
| **Derleyici şartı** | Kullanıcıda MSVC + CMake olmalı | Kabul ediliyor. Bu, C#'ın (Aşama C) asıl gerekçesi — K1'de yazılı |
| **Hata mesajı görünmezliği** | Derleme hatası konsola gidip kaybolursa "neden çalışmıyor?" doğar | B-5'te çıktı editörde bir panelde gösterilecek |
| **Hata ayıklama** | Script'e breakpoint koymak DLL'e attach gerektirir | Sınır olarak yazılacak; Visual Studio'dan editöre attach çalışır |

---

## 5. Kapsam dışı (bilerek)

- **Script'lerin kendi component'lerini tanımlaması.** DLL boşalınca
  `ComponentRegistry`'de öksüz tip kalırdı; ayrı bir tasarım gerekir.
- **Otomatik derleme** (dosya değişince) — B3'te gerekçesi yazılı, sonra.
- **Script alanlarının Inspector'dan ayarlanması** — A1'den beri bekliyor.
  Aşama B **bunu kolaylaştırmıyor**, sadece yerini netleştiriyor: alan
  değerleri component'te veri olarak durmak zorunda, çünkü DLL yeniden
  yüklendiğinde örnekler yok olup yeniden yaratılıyor. Yani B'den sonra
  yapılmalı, önce değil.
- **Linux/macOS** (`dlopen`) — bugün zaten yalnızca Windows deneniyor.

---

## 6. Sıra üzerine not

Bu plan **A5 (Undo/Redo) öncesine** alındı. Gerekçe: iterasyon hızı her
gün acıtıyor, Undo ara sıra. A5 gecikmekten zarar görmüyor çünkü A1'in
alan tablosu onu zaten ucuzlattı.

**Ama dürüst olalım:** Aşama B, A1 kadar büyük bir iş ve motoru statikten
paylaşımlıya çevirmek her şeye dokunuyor. B-1 kırmızı yanarsa geri dönüp
A5'i yapmak ve B'yi yeniden planlamak meşru bir seçenek.
