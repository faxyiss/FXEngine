# A3 — Project Settings + Preferences

> Karar K4'ün uygulanması (bkz. [02-Mimari.md](02-Mimari.md)).

## Koda bakınca çıkan tablo plandan farklıydı

K4 "ayarlar üç yere dağılmış" diyordu. Kodu taradığımda ilk ikisinin
**zaten doğru tarafta** olduğu çıktı:

| Nerede | İçerik | Doğru yerde mi? |
|---|---|---|
| `.fxproject` (v2) | Ad, varlık klasörü, `StartScene` | ✅ hepsi projeye ait |
| `editor.json` | Son sahneler, son projeler, içerik görünümü | ✅ hepsi kullanıcıya ait |
| **koda gömülü** | snap ×4, ızgara, kamera çerçeveleri, gizmo modu, kamera hızı | ❌ hiç kaydedilmiyordu |

Yani A3'ün asıl işi "üç yeri ikiye indirmek" değil, **koda gömülü sekiz
tercihi kurtarmak** ve A2'nin açıkta bıraktığı hedef çözünürlüğü eklemek
oldu.

Somut şikâyet: kademe değerini 0.25'e ayarlıyordun, editörü kapatıp
açınca 0.5'e dönüyordu. Sekiz ayarın hepsi böyleydi.

## Karar: `.fxproject` genişletildi, ayrı `ProjectSettings/` açılmadı

K4 ayrı bir `ProjectSettings/*.json` öngörüyordu. Uygulamadım çünkü
`.fxproject` **zaten** proje ayar dosyası ve zaten doğru içeriğe sahip;
tek bir ayar için yeni bir klasör + dosya + sürüm şeması icat etmek
karşılıksız bir maliyet olurdu.

K4'ün gerekçesi (ayarlar sık değişir, `.fxproject` değişmez → merge
çatışması) gerçek ama **henüz geçerli değil**: bugün tek bir proje ayarı
var. Fizik (Faz 17) ve sıralama katmanları (18c) geldiğinde dosya
gerçekten kalabalıklaşırsa ayrılır.

### `.fxproject` sürüm 3

```json
{
  "Version": 3,
  "Name": "Game1",
  "AssetDirectory": "assets",
  "StartScene": 13198501761230663990,
  "TargetResolution": [1920, 1080]
}
```

Yeni alan eklemek normalde sürüm artırmaz (eski dosya yine açılır), ama
Game View'ın davranışı buna bağlandığı için sürümü işaretledim.

**Sürüm 2 dosyaları kırılmıyor:** `TargetResolution` yoksa varsayılan
1920×1080. Bu bir iddia değil, testte sabit —
`"Surum 2 projesi acilinca varsayilan cozunurluk aliyor"`.

Ayrıca elle düzenlenmiş dosyada `[0,0]` yazsa da varsayılana düşüyor:
sıfıra bölme üretmesin.

## Game View: serbest ↔ kilitli

Game panelinin üstünde bir açılır liste: **Serbest** (A2'nin davranışı)
veya **hedef orana kilitli**.

**Bir tasarım hatasından döndüm.** İlk yazdığımda kamerayı hedef orana
kilitliyor ama görüntüyü panelin tamamına yayıyordum — bu bant değil
**esneme** üretirdi. Doğrusu: karar panelin, renderer'ın değil.

Artık `DrawGamePanel` çizim yüzeyinin boyutunu hesaplıyor (hedef oranı
panele *sığdırarak*, kırpmadan), `RenderGameView` sadece o yüzeyin
oranını kullanıyor. Artan yer siyah bant kalıyor.

```
panel geniş → yanlarda bant      panel dar → altta/üstte bant
┌──┬────────┬──┐                 ┌────────────┐
│██│ oyun   │██│                 ├────────────┤
└──┴────────┴──┘                 │    oyun    │
                                 ├────────────┤
                                 └────────────┘
```

## İki ayrı ayar penceresi

Ayrılar çünkü **karışırsa yanlış dosyaya yazılır**. Her pencere
kendi kimliğini üstte yazıyor:

| Pencere | Yazdığı yer | Sürüm kontrolü | İçerik |
|---|---|---|---|
| **Proje Ayarları** | `.fxproject` | Girer | Hedef çözünürlük (+ ad/varlık klasörü salt okunur) |
| **Tercihler** | `editor.json` | Girmez | Kamera hızı, kademe ×4, ızgara, kamera çerçeveleri, içerik görünümü, Game View kilidi |

Görünüm menüsünden açılıyor; Game View araç çubuğundaki "Ayarlar..."
düğmesi doğrudan Proje Ayarları'nı açıyor.

Proje Ayarları'nda **açık bir "Kaydet" düğmesi** var: `.fxproject`
diske yazmak proje dosyasını değiştirmek demek, bunu sessizce yapmak
doğru değil. Tercihler ise çıkışta zaten kaydediliyor (düğme sadece
"gitti mi?" merakını gidermek için).

16:9 / 16:10 / 4:3 / 1:1 hazır düğmeleri: elle 1920×1080 yazmak zorunda
kalma.

## Doğrulama

| | Sonuç |
|---|---|
| Derleme | temiz |
| `FXTests` | 48 → **51 test / 238 assertion** |
| Tercihlerin kalıcılığı | **Ölçüldü** — editör açılıp kapatıldı, `editor.json`'a 8 yeni anahtar yazıldı |
| Mevcut proje (`Game1.fxproject`) | v2, testle kanıtlandığı gibi varsayılan alıp açılacak |

Eklenen 3 test: çözünürlük gidiş-dönüş, sürüm 2 uyumu, bozuk değerin
varsayılana düşmesi.

**Görsel doğrulama yapılmadı** — bantların gerçekten doğru yerde olduğu
elle test edilmeli.

## Elle test edilmesi gerekenler (GUI)

1. Görünüm → **Tercihler** → kamera hızını değiştir, editörü kapat/aç →
   **değer korunmalı**.
2. Aynısını kademe değerleri ve ızgara için yap.
3. Görünüm → **Proje Ayarları** → hedef çözünürlüğü 4:3 yap → Kaydet →
   `.fxproject`'te `"Version": 3` ve `"TargetResolution": [1024,768]`
   olmalı.
4. Game sekmesi → açılır listeden **hedef orana kilitli** seç → panel
   geniş olduğunda **yanlarda siyah bant** çıkmalı, görüntü esnememeli.
5. Panel oranını değiştir → bantlar yer değiştirmeli (yanlar ↔ alt/üst).
6. **Serbest**'e dön → bant kalmamalı, görüntü paneli doldurmalı.
7. Game View araç çubuğundaki "Ayarlar..." → Proje Ayarları açılmalı.
8. Proje açık değilken (karşılama ekranından "projesiz devam et") →
   Proje Ayarları menü öğesi **pasif** olmalı, açılır listede kilitli
   seçenek seçilememeli.
9. Proje Ayarları'nda değişiklik yapıp **kaydetmeden** kapat → dosya
   değişmemeli.

## Yapılmadı

- **Ayrı `ProjectSettings/` klasörü** — gerekçesi yukarıda; Faz 17/18c'de
  yeniden değerlendirilecek.
- **Kısayol tuşları ve tema** Preferences'a girmedi — kısayollar bugün
  koda gömülü ve yeniden bağlanabilir değil; ayrı bir iş (13d action
  mapping ile birlikte anlamlı).
- **Oyunun kendisi hedef çözünürlüğü kullanmıyor.** Bugün yalnızca Game
  View'ın oranını belirliyor. Bağımsız bir oyun çalıştırıcısı (Aşama B)
  pencere boyutunu da buradan alacak.
- `StartScene` `.fxproject`'te kaldı — zaten doğru yerde.
