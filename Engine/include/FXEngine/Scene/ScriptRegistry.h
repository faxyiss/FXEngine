#pragma once

// ===========================================================================
// FXEngine - ScriptRegistry
//
// AD -> SINIF eslemesi. 16a'da script'ler `Bind<T>()` ile derleme
// zamaninda baglaniyordu; bu iki seyi imkansiz kiliyordu:
//
//   1) SERILESTIRME. Sahne dosyasina bir fonksiyon isaretcisi yazilamaz.
//      Yazilabilecek tek sey addir - ve adi sinifa geri cevirecek bir
//      tabloya ihtiyac var.
//   2) Inspector'da script SECMEK. Elle yazilmis bir menu her yeni
//      script'te guncellenmeyi bekler ve biri mutlaka unutulur.
//
// Bu, Faz 8'in "kimlik ile konum ayri seylerdir" fikrinin baska bir
// yuzu: script'in KIMLIGI adidir, C++ tipi degil.
//
// Kayit uygulamanin isi (editor ya da oyun), motorun degil: motor hangi
// script'lerin var oldugunu bilemez.
// ===========================================================================

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace FX
{
    class ScriptableEntity;

    class ScriptRegistry
    {
    public:
        using Factory = std::function<ScriptableEntity*()>;

        // Ayni ad ikinci kez kaydedilirse ikincisi yok sayilir ve
        // uyari basilir: sessizce ezmek, hangi sinifin calistigini
        // belirsiz kilardi.
        static void Register(const std::string& name, Factory factory);

        template<typename T>
        static void Register(const std::string& name)
        {
            Register(name, []() -> ScriptableEntity* { return new T(); });
        }

        // Bilinmeyen ad icin nullptr. Bu, sahne dosyasinda kayitli
        // olmayan bir script gecen durumdur - proje derlenirken o sinif
        // kaldirilmis olabilir.
        static ScriptableEntity* Create(const std::string& name);

        static bool Contains(const std::string& name);

        // Inspector kendi listesini bundan dolduruyor. Kayit sirasina
        // gore degil ALFABETIK: menude aramak kolay olsun.
        static const std::vector<std::string>& GetNames();

        static void Clear();
    };
}
