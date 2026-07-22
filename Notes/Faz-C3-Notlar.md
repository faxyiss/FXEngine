# Faz C-3 — Prefab override takibi

> Prefab sisteminin üçüncü parçası. C-1 bağı kurdu, C-2 topluca Revert
> etti; C-3 "**hangi alan** sapmış?" sorusunu cevaplıyor: Inspector'da
> vurgulu etiket + alan başına "kaynak değerine döndür". C-4 (Apply) bu
> tespitin üstüne oturacak.

Son güncelleme: 2026-07-23 · Önce oku: [Faz-C2-Notlar.md](Faz-C2-Notlar.md)

---

## Yöntem kararı: açık takip değil, DIFF

Override bilgisi **ikinci bir yerde saklanmıyor** — kaynak + örnek zaten
var, fark her soruluşta hesaplanıyor ("aynı bilgi iki yerde durmasın").
Açık takip (Unity'nin modifications listesi) her alan düzenlemesinin bir
seti güncellemesini gerektirirdi; unutulan bir güncelleme sessizce yanlış
gösterirdi. Diff'in bedeli kare başına küçük JSON kıyasları; kaynak dosya
önbellekte.

## Yeni modül — `FX::PrefabOverrides`

`Engine/include/FXEngine/Scene/PrefabOverrides.h` + `src/Scene/PrefabOverrides.cpp`:

- **`OverriddenFields(instance, info)`** → kaynaktan farklı alan adları.
  Kaynak yoksa boş; component kaynakta hiç yoksa (örnekte eklenmiş) tüm
  alanlar.
- **`RevertField(instance, info, fieldName)`** → tek alanı kaynak değerine
  döndürür. Dönüş: değer gerçekten değişti mi (aynıysa `false`, gereksiz
  undo adımı oluşmaz).

### Önbellek

`handle → { yazma zamanı, SourceId → entity json }`. Her sorguda
`last_write_time` kıyaslanıyor; dosya değişince (Apply, dış düzenleme)
kendiliğinden tazeleniyor — ayrı bir "cache boşalt" çağrısı yok.

### EntityRef uzay çevrimi — kritik detay

Örneğin iç referansı **örnek** kimliği taşır, kaynak dosya **kaynak**
kimliği. Doğrudan kıyas el değmemiş `Follow.Target`'ı bile "sapmış"
gösterirdi. Çözüm:

- **Kıyas:** örnek hedefi kaynak uzayına çevrilir (hedef aynı prefab'ın
  damgalı üyesiyse `SourceId`'si alınır), sonra kıyaslanır.
- **RevertField:** kaynak değer örnek uzayına çevrilir — sahnede değil
  **bu örneğin kendi alt ağacında** aranır (`InstanceRootOf` +
  `FindBySourceId`), çünkü aynı prefab'ın birden fazla örneği olabilir.
  Bulunamazsa dış referanstır, olduğu gibi kullanılır.

### `Detail::ReadField` açıldı

`EntitySerialization`'ın alan-okuma adımı iç header'da görünür yapıldı.
`RevertField` bunu **`ApplyComponent` yerine** kullanıyor: `ApplyComponent`
`LoadExtra`'yı da çalıştırır ve `NativeScript`'in `LoadExtra`'sı
`Fields`'ı koşulsuz temizler — tek alan geri alınırken script/doku verisi
etkilenmemeli.

## Editör — Inspector

- **Vurgulu etiket:** sapan alanın etiketi prefab mavisiyle çiziliyor
  (kalın font atlası yok; renk aynı işi görüyor). `s_LabelOverridden`
  bayrağı alan döngüsünde kurulup temizleniyor; `Bool` alanının ayrı
  etiket yolu da aynı yardımcıya (`LabelText`) bağlandı.
- **Sağ tık → "Kaynak degerine dondur":** değer widget'ına sağ tık.
  Yalnızca alan gerçekten sapmışsa aktif. Çoklu seçimde her hedef **kendi**
  kaynağına döner. Mevcut `PushFieldCommand` ile geri alınabilir (eski
  değerler zaten alan döngüsünde yakalanıyor).

## Testler

3 yeni test (`[prefab]`, toplam 8 / 69 assertion):

1. **Tespit:** el değmemiş çocukta sapma yok; kökte yalnız `Translation`
   (Instantiate override'ı); rotasyon değişince listeye giriyor.
2. **RevertField:** tek alan dönüyor, diğer override korunuyor; ikinci
   çağrı `false` (değişmedi).
3. **EntityRef:** el değmemiş `Follow.Target` sapma sayılmıyor (uzay
   çevrimi); bozulunca tespit; revert hedefi **örneğin** köküne döndürüyor.

Tüm paket: **85 test / 421 assertion** (öncesi 82/398).

## Bilerek yapılmayanlar

- **Apply** (örnek → kaynak, alan başına veya topluca) — C-4. Tespit artık
  hazır, "neyi uyguluyorum?" cevaplanabiliyor.
- **Doku (Extra) override göstergesi** — `TextureHandle` alan tablosunda
  değil; sprite dokusunun sapması işaretlenmiyor. C-4/C-5'te ele alınacak.
- **Script alanı (NativeScript.Fields) override göstergesi** — dinamik
  harita, alan tablosunda değil. Aynı şekilde sonraya.
- **Component düzeyi gösterge** — "bu component örnekte eklenmiş" bilgisi
  hesaplanıyor (tüm alanlar sapmış döner) ama başlıkta ayrı bir rozet yok.
