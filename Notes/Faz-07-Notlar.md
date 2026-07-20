# Faz 7 — JSON Serileştirme (MVP Tamam)

## Dosyalar
| Dosya | İş |
|---|---|
| `Engine/Renderer/TextureLibrary.h/.cpp` | Yol → texture önbelleği |
| `Engine/Scene/SceneSerializer.h/.cpp` | Sahne ↔ JSON |
| `Editor/src/EditorApp.*` | Ctrl+S / Ctrl+O, menü, durum mesajı |

## Bu faz, Faz 5'in sınavı

Faz 5'te "component **sadece veri** olmalı" diye ısrar ettim. Karşılığını
burada alıyoruz: component'ler saf veri olduğu için serileştirme **mekanik**
bir iş — her alanı yaz, geri okurken doldur.

İçlerinde işaretçiler, geri çağrımlar veya sistemlere referanslar olsaydı
bu imkânsıza yakın olurdu. Bir `std::function`'ı diske nasıl yazarsın?

> **İyi bir veri modelinin testi, serileştirilebilir olmasıdır.**

## Tek zor problem: texture'lar

`SpriteRendererComponent` bir `shared_ptr<Texture2D>` tutuyor. İşaretçi
diske yazılamaz — çalışma zamanı adresidir.

**Çözüm:** kimlik olarak **yol** yaz, yüklerken yolu tekrar texture'a çevir.

Ama 100 entity aynı dosyayı kullanıyorsa 100 kez mi yüklenecek?
→ **TextureLibrary** (asset cache). Yol → texture eşleme tablosu.

### Doğrulanan kanıt
Yükleme logunda **hiç** "Texture yuklendi" satırı çıkmadı.
25 entity iki texture'a atıfta bulunuyor, diske **hiç gidilmedi**.
Önbelleğin çalıştığının somut ölçümü.

### Başarısız yükleme önbelleğe alınmıyor
Alsaydık, dosya sonradan düzeltilse bile hep `nullptr` dönerdi.
Bedeli: bozuk bir yol her istendiğinde tekrar denenir — sahne yüklerken
bir kez olur, sorun değil.

## Tasarım kararları

### Neden `Scene::Save()` değil, ayrı bir sınıf?
Sahne, **nasıl saklandığını** bilmek zorunda değil. Yarın JSON yerine binary
format istersek `Scene`'e hiç dokunmayız; ikinci bir serileştirici yazarız.

> Veri ile onun **temsili** ayrı sorumluluklardır.

### `Version` alanı
Bugün kullanmıyoruz ama **sonradan eklemek çok zordur** — dosyalar çoktan
yazılmış olur. Format değişirse eski dosyaları tanıyıp dönüştürebilmek için.

### Savunmacı okuma
```cpp
tc.Rotation = t.value("Rotation", 0.0f);   // alan yoksa varsayılan
```
Sahne dosyası elle düzenlenebilir ve eski sürümlerle uyumluluk gerekir.
Bir alanın eksik olması programı **çökertmemeli** — serileştirmenin temel
kurallarından.

### `dump(2)` — insan okunabilir çıktı
Tek satır daha küçük olurdu ama sahne dosyasını metin editörüyle açıp
incelemek öğrenme süreci için değerli. **Boyut burada öncelik değil.**

### Dosyada radyan, arayüzde derece
Dosya **iç veriyi** yansıtmalı; çevrim sadece arayüz sınırında olmalı.
Faz 5'te kurduğumuz kuralın devamı.

### View sırası ters çevriliyor
EnTT view'ları **ters sırada** gezer (en son eklenen ilk). Faz 5'te
önemsizdi ama burada dosyadaki sırayı belirliyor. Ters çevirerek
kaydet/yükle sonrası Hierarchy'nin aynı sırada görünmesini sağlıyoruz.

### `create_directories` — `ofstream` klasör oluşturmaz
`assets/scenes/` yoksa kaydetme **sessizce** başarısız olurdu.
Kullanıcının klasörü elle açmasını beklemek kötü tasarım.
`std::filesystem` + `error_code` sürümü (motorda istisna kullanmıyoruz).

### İstisnayı kütüphane sınırında yakala
`nlohmann/json` **istisna atar**. Motorun geri kalanında istisna
kullanmıyoruz, ama bozuk bir JSON programı çökertmemeli.

```cpp
try { in >> root; }
catch (const json::parse_error& err) { ...; return false; }
```

İstisnayı 3rd-party sınırında yakalayıp kendi hata modelimize (bool dönüş)
çeviriyoruz. **Kütüphanelerle çalışırken standart yaklaşım.**

## ⚠️ Handle geçersizleşmesi — klasik tuzak

Yükleme tüm entity'leri yok edip yenilerini oluşturur.
Elde tutulan tutamaklar **artık geçersiz** — ama *geçerli görünürler*,
çünkü EnTT kimlikleri yeniden kullanır.

```cpp
m_PlayerEntity = {};                        // temizle
m_HierarchyPanel.SetContext(m_Scene.get()); // seçimi de temizler
```

Temizlemezsek Inspector alakasız veri gösterir veya assert tetiklenir.
**Tutamak tabanlı sistemlerin klasik tuzağı.**

## Kullanıcı geri bildirimi
Kaydet/yükle sonrası menü çubuğunda 4 saniyelik durum mesajı.
**Sessiz başarı, kullanıcıya "oldu mu?" diye sorduran kötü bir tasarımdır.**

## Dosya formatı
```json
{
  "Version": 1,
  "Scene": "Untitled",
  "Entities": [
    {
      "Tag": "Oyuncu",
      "Transform": {
        "Translation": [0.0, 0.0, 0.1],
        "Rotation": 0.0,
        "Scale": [1.0, 1.0]
      },
      "SpriteRenderer": {
        "Color": [1.0, 0.85, 0.3, 1.0],
        "Texture": "assets/textures/circle.png",
        "TilingFactor": 1.0
      },
      "Velocity": { "Linear": [0.0, 0.0], "Angular": 0.0 }
    }
  ]
}
```

## Test sonucu
```
Sahne kaydedildi: .../assets/scenes/Sahne.fxscene (25 entity)   17.877 bayt
Sahne yuklendi:   .../assets/scenes/Sahne.fxscene (25 entity)
```
Tur tamamlandı, GL hatası yok, assert tetiklenmedi.
Yükleme sırasında **sıfır** ek texture yüklemesi → önbellek çalışıyor.

## Onay
- [x] Derlendi
- [x] Kaydetme çalıştı, dosya doğru
- [x] Yükleme çalıştı, önbellek devrede
- [ ] Kullanıcı onayı → **MVP TAMAM**
