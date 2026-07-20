# Faz 21 — Proje sistemi

Faz 12'de bilinçli olarak ertelenmiş bir sorun vardı: editör `<exe>/assets`'i
kök alıyordu, yani içe aktardığın her varlık `build/` altında yaşıyordu.
**`build/` klasörünü silmek günlük bir işlemdir** — ve her seferinde
kullanıcının emeğini de siliyordu.

## Dosyalar

| Dosya | Değişiklik |
|---|---|
| **`Core/Project.h/.cpp`** (yeni) | `.fxproject`, aktif proje |
| `Core/FileSystem.*` | İki kök: motor / proje |
| `Renderer/Shader.cpp` | `ResolveEngineAsset` |
| `Renderer/Texture.cpp`, `Scene*Serializer.cpp` | `ResolveProjectAsset` |
| `EditorApp.*` | Proje aç/oluştur, son projeler, açılış akışı |
| `Panels/ContentBrowserPanel.cpp`, `Platform/FileDialogs.cpp` | Proje kökü |

---

## 1. Asıl fikir: kurulu olduğun yer ≠ üzerinde çalıştığın yer

Word Program Files'tadır, belgen Masaüstünde. Bu ayrımı yapmayan araçlar
taşınabilir olmaz.

| | Nerede yaşar | Kim düzenler |
|---|---|---|
| **Motor varlıkları** (shader) | exe'nin yanı | motor geliştiricisi |
| **Proje varlıkları** (texture, sahne, prefab) | proje klasörü | kullanıcı |

`FileSystem` artık iki kök tutuyor ve çözümleme fonksiyonları ikiye ayrıldı:

```cpp
ResolveEngineAsset("assets/shaders/Line.vert")  // -> <exe>/assets/shaders/...
ResolveProjectAsset("assets/textures/x.png")    // -> <proje>/assets/textures/...
```

Tek bir `ResolveAsset` bırakıp "hangi kök?" sorusunu çağrı yerine
bırakmak cazipti. Ama o zaman **her yeni çağrı yeni bir karar** olurdu
ve biri er geç yanlış tarafı seçerdi. İki ayrı isim, kararı bir kez
veriyor.

`MakeRelativeToBase` de `MakeRelativeToProject` oldu: sahne dosyasına
yazılan yollar artık proje köküne göreli.

---

## 2. `.fxproject` kendi konumunu SAKLAMAZ

```json
{
    "Version": 1,
    "Name": "DenemeProje",
    "AssetDirectory": "assets",
    "StartScene": "assets/scenes/proje.fxscene"
}
```

Kök = `.fxproject` dosyasının bulunduğu klasör. Dosyanın içine bir kök
yolu yazsaydık proje taşındığında o yol yanlış olurdu — ve tam da bu
fazın çözdüğü sorun geri gelirdi.

Proje "kendini tarif eden bir klasör": taşınabilir, sürüm kontrolüne
girebilir, başka makinede açılabilir.

`Create` boş bir projede bile `assets/textures`, `assets/scenes`,
`assets/prefabs` klasörlerini kuruyor. Boş bir klasör ağacı, boş bir
klasörden çok daha öğreticidir — kullanıcı "nereye koyacağım?" diye
düşünmesin.

---

## 3. Proje ile `FileSystem` kökü BİRLİKTE değişir

```cpp
void Project::SetActive(const std::shared_ptr<Project>& project)
{
    s_Active = project;
    FileSystem::SetProjectDirectory(project ? project->m_Directory : "");
}
```

Ayrı ayrı yönetilseydi ikisi kaçınılmaz olarak birbirinden ayrışırdı:
"proje açıldı ama texture'ları bulunamıyor" gibi izi zor sürülen bir
hata. Tek giriş noktası bu ihtimali ortadan kaldırıyor.

---

## 4. Proje değişince doku önbelleği temizlenmeli

```cpp
m_TextureLibrary.Clear();
```

`TextureLibrary` **yola göre** anahtarlı. Temizlemeseydik yeni projedeki
`assets/textures/player.png`, eski projenin aynı isimli dosyasını
döndürürdü. İki farklı projede aynı dosya adı olması istisna değil kural.

Son açılan **sahneler** listesi de temizleniyor: o liste eski projeye
aitti.

---

## 5. Açılış sırası kritik

```
LoadEditorConfig()      // son projeleri oku
LoadStartupProject()    // <-- HER SEYDEN ONCE
  ...sonra dokular, sonra sahne
```

Önce doku yükleyip sonra proje açsaydık, eski kökten gelen dokular
önbellekte kalırdı. Bir fazın en sinsi hataları böyle sıralama
sorunlarından çıkar.

**Demo sahne artık sadece projesiz modda kuruluyor.** Bir proje açıksa
kullanıcının projesine bizim örnek nesnelerimizi doldurmak yanlış olurdu.

---

## 6. Klasör diyaloğu yerine "farklı kaydet"

"Yeni Proje" akışı klasör seçtirmiyor; kullanıcı `.fxproject` dosyasının
adını ve yerini seçiyor, kök de o dosyanın klasörü oluyor.

Win32'de klasör seçme ayrı bir API (`SHBrowseForFolder` / `IFileDialog`)
ve daha kötü bir kullanıcı deneyimi sunuyor. Aynı sonucu zaten elimizde
olan diyalogla alıyorsak ikinci bir API'ye gerek yok.

---

## 7. Proje yoksa ne olur?

`FileSystem` proje kökü boşken base dizine düşüyor — yani **Faz 21 öncesi
davranış aynen korunuyor**. Editör projesiz de çalışıyor.

Ama bu durum **sessiz değil**: açılışta uyarı basılıyor ve İstatistikler
panelinde turuncu "Açık proje yok" yazıyor. Sessizce eski davranışa
dönmek, tam da bu fazın çözdüğü sorunu geri getirirdi.

Geriye dönük uyumluluk ile doğru davranış arasında seçim yapmak yerine
ikisini birden veriyoruz: eski projeler çalışmaya devam ediyor, yeni
kullanıcı doğru yola yönlendiriliyor.

---

## 8. Karşılama ekranı (launcher)

Editör artık boş bir sahne ve **karşılama ekranıyla** açılıyor. Proje
seçilene kadar editör arayüzü hiç çizilmiyor.

Neden otomatik son projeyi açmıyoruz? Çünkü **proje seçilmeden varlık
yüklenemez**: varlık yolları ve doku önbelleği projeye göreli çözülüyor.
Önce yükleyip sonra proje açmak, eski kökten gelen dokuları önbellekte
bırakırdı. Karşılama ekranı bu sıralamayı yapı gereği doğru kılıyor.

`OnRender` en başta ayrılıyor:

```cpp
if (m_ShowLauncher) { ...launcher... return; }   // sahne bile cizilmiyor
```

### Tek tık açıyor, çift tık değil

İlk halinde çift tık istiyordu. Karşılama ekranında **"seçmek" diye bir
eylem yok** — tıkladığın projeyi açmak istiyorsundur. Çift tık istemek,
hiçbir karşılığı olmayan bir ara durum (seçili ama açılmamış) üretirdi.

### Kayıp projeler gizlenmiyor

Dosyası bulunamayan proje listeden silinmiyor; kırmızı ve "(bulunamadı)"
etiketiyle gösteriliyor. Kullanıcı projesinin nerede olduğunu hatırlamak
isteyebilir; sessizce kaybolması kafa karıştırır. Sağ tık → "Listeden
Kaldır" ile kendisi temizliyor.

### Düzen ayrıntıları

İçerik 760 piksellik ortalanmış bir **çocuk pencerede**. Grup değil çocuk
pencere, çünkü `Separator` içinde bulunduğu pencerenin genişliğini kullanır
— grupta olsaydı ekranın tamamına yayılıp ortalanmış içerikle hizasız
görünürdü.

Liste boşken de aynı yüksekliği kaplıyor: liste dolunca düğmelerin yeri
değişirse kullanıcı her açılışta farklı bir düzenle karşılaşır.

## Test sonuçları

Geçici öz-test (sonra kaldırıldı):

```
proje olusturuldu       : evet
klasorler               : assets=1 scenes=1
FileSystem koku         : C:/Users/.../Temp/FXProjeTest/
proje varligi  -> C:/Users/.../Temp/FXProjeTest/assets/x.png
motor varligi  -> C:\FXEngine\build\bin\assets/shaders/Line.vert
sahne diskte            : 1   (build/ altinda DEGIL)
yeniden acildi          : ad=DenemeProje baslangic=assets/scenes/proje.fxscene
kapatildi, kok          : C:\FXEngine\build\bin\   (exe'ye dondu)
```

Ortadaki iki satır fazın özü: aynı anda iki farklı kök, doğru varlık
doğru yere.

## Onay
- [x] Derlendi (uyarısız)
- [x] Proje oluşturma, klasör yapısı, `.fxproject` yazma
- [x] Yükleme, `StartScene`, yeniden açma
- [x] Motor/proje varlık ayrımı
- [x] Proje kapatınca exe köküne dönüş
- [x] Projesiz mod hâlâ çalışıyor (uyarı ile)
- [x] Karşılama ekranı: son projeler, aç/oluştur, projesiz devam
- [x] Launcher'dan proje açma (uçtan uca doğrulandı)
- [ ] Kullanıcı onayı

## Yapılmayanlar → sonraki iş
- **`.meta` dosyaları + GUID tabanlı `AssetManager`** — roadmap'te "bunun
  üstüne" diye yazılmıştı, kendi fazını hak ediyor. Varlık kimliği hâlâ
  **dosya yolu**: taşımak/yeniden adlandırmak referansları koparıyor.
  Doku ayarlarının dosya başına saklanamaması da (`TextureLibrary`
  uyarısı) buna bağlı.
- **Proje ayarları penceresi** — `Name`/`StartScene` sadece dosyadan
  düzenlenebiliyor.
- **`AssetDirectory` gerçekten kullanılmıyor** — `.fxproject`'te saklanıyor
  ama content browser hâlâ `assets`'i varsayıyor. Alanı şimdiden
  yazıyoruz ki format değişmesin.
