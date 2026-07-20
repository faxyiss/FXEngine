#pragma once

// ===========================================================================
// FXEngine - UUID
//
// IKI TUR KIMLIK VARDIR ve bunlari karistirmak Faz 7'de bize dert oldu:
//
//   entt::entity -> CALISMA ZAMANI kimligi. Hizli ama gecici: EnTT silinen
//                   kimlikleri geri donusturur, yukleme sirasinda herkes
//                   yeni kimlik alir. Diske YAZILAMAZ.
//
//   FX::UUID     -> KALICI kimlik. Entity dogarken bir kez uretilir,
//                   sahne dosyasina yazilir, yuklemede aynen geri gelir.
//                   "Su dusman su hedefi takip etsin" gibi referanslar
//                   ancak bununla kurulabilir.
//
// TASARIM: 64-bit rastgele sayi. "Gercek" UUID'ler (RFC 4122) 128 bittir;
// bizim capimizda (tek kullanici, tek sahne, binlerce entity) 64 bit
// carpisma olasiligi pratik olarak sifirdir ve uint64_t olarak tasimak,
// hash'lemek, JSON'a yazmak cok daha ucuzdur. Hazel ve bircok kucuk motor
// ayni tercihi yapar.
//
// 0 degeri "gecersiz/bos" anlamina ayrilmistir (entt::null gibi).
// ===========================================================================

#include <cstdint>
#include <functional>

namespace FX
{
    class UUID
    {
    public:
        // Varsayilan yapici RASTGELE bir kimlik uretir.
        // "Bos UUID istiyorum" dersen UUID(0) yazarsin - bilerek boyle:
        // yeni entity'nin kimliksiz kalmasi kazara bile mumkun olmamali.
        UUID();

        // Belirli bir degerden (dosyadan okurken) olusturma.
        // explicit DEGIL: "uuid == 0" gibi karsilastirmalar ve JSON'dan
        // gelen uint64_t'nin dogrudan atanabilmesi icin.
        UUID(std::uint64_t uuid) : m_UUID(uuid) {}

        UUID(const UUID&) = default;
        UUID& operator=(const UUID&) = default;

        // uint64_t'ye seffaf donusum: JSON'a yazmak, loglamak,
        // karsilastirmak icin. UUID aslinda "anlam yuklu bir sayi"dir;
        // sayiligini gizlemek fayda degil kulfet getirirdi.
        operator std::uint64_t() const { return m_UUID; }

        bool IsValid() const { return m_UUID != 0; }

    private:
        std::uint64_t m_UUID;
    };
}

// std::unordered_map<UUID, ...> kullanabilmek icin hash ozellestirmesi.
// UUID zaten rastgele uretildigi icin kendisi mukemmel bir hash'tir -
// tekrar hash'lemeye gerek yok, aynen donduruyoruz.
namespace std
{
    template<>
    struct hash<FX::UUID>
    {
        std::size_t operator()(const FX::UUID& uuid) const noexcept
        {
            return static_cast<std::size_t>(static_cast<std::uint64_t>(uuid));
        }
    };
}
