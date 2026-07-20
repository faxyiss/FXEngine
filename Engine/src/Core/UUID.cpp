#include "FXEngine/Core/UUID.h"

#include <random>

namespace FX
{
    namespace
    {
        // "Fonksiyon icinde static" kalibi (Log.cpp'de FileSystem icin de
        // kullanmistik): motor tek thread'de basladigi surece bu guvenlidir.
        // random_device gercek (isletim sistemi kaynakli) entropi verir;
        // mt19937_64 ondan hizli, iyi dagilimli sayilar uretir - random_device'i
        // DOGRUDAN cagirmak yavastir, o yuzden sadece tohumlamak icin kullanilir.
        std::random_device s_RandomDevice;
        std::mt19937_64    s_Engine(s_RandomDevice());
        std::uniform_int_distribution<std::uint64_t> s_Distribution;
    }

    UUID::UUID()
        : m_UUID(s_Distribution(s_Engine))
    {
        // 0'i "gecersiz" olarak ayirdik, dolayisiyla asla uretmemeliyiz.
        // Olasilik 1/2^64 - pratikte hic olmaz. Ama "pratikte hic olmaz"
        // ile "olamaz" farkli seylerdir: tek satirlik bir korumayla
        // ikincisine cevirebiliyorsak ceviririz. Bulunmasi imkansiz
        // hatalarin kaynagi tam olarak bu tur ihmallerdir.
        while (m_UUID == 0)
            m_UUID = s_Distribution(s_Engine);
    }
}
