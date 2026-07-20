#include <catch2/catch_test_macros.hpp>

#include <FXEngine/Core/UUID.h>

#include <set>
#include <unordered_map>

TEST_CASE("UUID varsayilan yapici rastgele ve gecerli uretir", "[uuid]")
{
    FX::UUID a;
    FX::UUID b;

    CHECK(a.IsValid());
    CHECK(b.IsValid());
    CHECK(static_cast<std::uint64_t>(a) != static_cast<std::uint64_t>(b));
}

TEST_CASE("UUID(0) gecersizdir", "[uuid]")
{
    // 0 "bos kimlik" icin ayrilmis; entity'nin kimliksiz kalmasi bu
    // deger disinda mumkun olmamali.
    FX::UUID empty{ 0 };
    CHECK_FALSE(empty.IsValid());
}

TEST_CASE("UUID degerinden yeniden olusturulabilir", "[uuid]")
{
    // Serilestirmenin dayandigi ozellik: diske yazilan sayi, geri
    // okundugunda AYNI kimligi verir.
    FX::UUID original;
    const std::uint64_t raw = original;

    FX::UUID restored{ raw };
    CHECK(static_cast<std::uint64_t>(restored) == raw);
}

TEST_CASE("Coklu uretimde carpisma olmuyor", "[uuid]")
{
    // 64 bit rastgelede 10 bin uretimde carpisma pratikte imkansiz;
    // bu test carpismayi degil, uretecin tohumunun her cagrida
    // sifirlanmadigini dogruluyor - o hata gercekten olur.
    std::set<std::uint64_t> seen;
    for (int i = 0; i < 10000; ++i)
        seen.insert(FX::UUID());

    CHECK(seen.size() == 10000);
}

TEST_CASE("UUID unordered_map anahtari olabilir", "[uuid]")
{
    // Scene'in UUID -> entity haritasi buna dayaniyor.
    std::unordered_map<FX::UUID, int> map;

    FX::UUID key;
    map[key] = 42;

    CHECK(map.at(key) == 42);
    CHECK(map.find(FX::UUID{ 12345 }) == map.end());
}
