#pragma once

// ===========================================================================
// FXEngine - Component meta-veri tablosu (A1)
//
// SORUN: Yeni bir component eklemek dort yere dokunmayi gerektiriyordu -
// Components.h, AllComponents, EntitySerialization, Inspector. Biri
// unutuldugunda hata SESSIZ oluyordu: "Play'e gecince kayboldu",
// "kaydedince ayar gitti".
//
// COZUM: Her component bir kez TARIF EDILIYOR; serilestirme, kopyalama
// ve Inspector cizimi bu tariften uretiliyor.
//
// Neden reflection kutuphanesi degil? Karar K2 (02-Mimari.md): acik ve
// okunabilir kalsin, hata mesajlari anlasilir olsun, bir gun C# sinirindan
// gecirilebilsin. Alan tablosu offset + tip tasiyor; bir dil koprusunun
// ihtiyaci olan sey tam olarak budur.
// ===========================================================================

#include "FXEngine/Scene/Entity.h"

#include <nlohmann/json_fwd.hpp>

#include <cstddef>
#include <deque>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <glm/glm.hpp>

namespace FX
{
    class TextureLibrary;

    enum class FieldType
    {
        Bool, Int, Float, Vec2, Vec3, Vec4,
        Color,       // Vec4 ama arayuzde renk secici
        String,
        EntityRef    // UUID tasir, arayuzde entity listesi
    };

    struct FieldInfo
    {
        // JSON anahtari. BU DOSYA FORMATININ PARCASI - degistirmek
        // eski sahneleri bozar.
        const char* Name  = nullptr;
        const char* Label = nullptr;   // Inspector etiketi
        FieldType   Type  = FieldType::Float;

        // Component isaretcisinden alanin adresini veren erisimci.
        //
        // Neden offsetof degil? std::string ve shared_ptr iceren
        // component'ler standart-layout OLMAYABILIR; orada offsetof
        // tanimsiz davranis. Uye isaretcisi sablon argumani olarak
        // gecince derleyici bu erisimciyi tipli ve tanimli uretiyor.
        // Bir dil koprusunun (C#) ihtiyaci olan sey de bu: alanin
        // adresi ve tipi.
        void* (*Get)(void* component) = nullptr;

        float DragSpeed = 0.05f;

        // Min == Max ise sinir yok.
        float Min = 0.0f;
        float Max = 0.0f;

        // Veri radyan, arayuz derece. Cevrim yalnizca Inspector'da yapilir;
        // dosyaya her zaman radyan yazilir.
        bool DegreesInUI = false;

        // Alan serilestirilir ama Inspector'da GENEL cizici ile
        // cizilmez - component'in ozel arayuzu onu kendi cizer.
        // (Kamera "Primary" tekillik mantigi, script ad listesi.)
        bool HiddenInInspector = false;
    };

    // Tip-silme kancalari: tablo calisma zamaninda gezilebilsin diye
    // sablon olmayan fonksiyon isaretcilerine indirgeniyor.
    struct ComponentInfo
    {
        // JSON anahtari + kimlik. Dosya formatinin parcasi.
        const char* Name  = nullptr;
        const char* Label = nullptr;   // Inspector basligi

        std::vector<FieldInfo> Fields;

        bool  (*Has)(Entity)          = nullptr;
        void* (*GetPtr)(Entity)       = nullptr;   // yoksa nullptr
        void* (*AddAndGetPtr)(Entity) = nullptr;   // varsa degistirir
        void  (*Add)(Entity)          = nullptr;   // varsayilanla ekle
        void  (*Remove)(Entity)       = nullptr;
        void  (*CopyTo)(Entity dst, Entity src) = nullptr;

        // Alan tablosunun ifade edemedigi veri (doku slotu gibi).
        // nullptr = yok. Bu kanca olmasa SpriteRenderer'in dokusu
        // sessizce kaybolurdu.
        void (*SaveExtra)(const void* component, nlohmann::json& out) = nullptr;
        void (*LoadExtra)(void* component, const nlohmann::json& in,
                          TextureLibrary* library) = nullptr;

        // Transform kaldirilamaz: her entity'nin konumu olmak zorunda.
        bool Removable = true;

        // Inspector'daki "Component Ekle" menusunde gorunsun mu?
        bool AddableFromMenu = true;

        // Bazi component'ler YAPISALDIR: Tag, Relationship, WorldTransform.
        // Sahne dosyasinda kendi nesneleri olarak degil entity duzeyinde
        // ("Tag", "Parent") yaziliyorlar, Inspector'da da ayri ele
        // aliniyorlar. Yine de tabloda duruyorlar cunku Scene::Copy
        // hepsini tasimak zorunda - listenin ikiye bolunmesi tam olarak
        // A1'in cozdugu hataydi.
        bool SerializedByTable = true;
        bool ShowInInspector   = true;

        // Component eklendikten hemen sonra calisir. Bagimliliklari olan
        // component'ler icin: Follow, Velocity olmadan sessizce
        // calismiyor - kullaniciyi bu tuzaga dusurmek yerine eksigi
        // tamamliyoruz.
        void (*OnAdded)(Entity) = nullptr;

        // Inspector'da alanlardan sonra cizilecek ozel arayuz (doku
        // slotu, kamera tekillik mantigi, script listesi).
        //
        // MOTOR BUNU CAGIRMAZ ve doldurmaz - editor kendi acilisinda
        // baglar. Yine de tarifin yaninda duruyor: "bu component'in ozel
        // bir arayuzu var" bilgisi de component tarifinin parcasi ve
        // ikinci bir tabloda yasamamali.
        //
        // std::function cunku editor tarafi kendi durumunu (doku
        // kutuphanesi) yakalamak zorunda.
        std::function<void(void* component, Entity owner)> DrawExtraUI;
    };

    namespace Detail
    {
        template<typename M> constexpr FieldType DeduceType()
        {
            if constexpr (std::is_same_v<M, bool>)           return FieldType::Bool;
            else if constexpr (std::is_same_v<M, int>)       return FieldType::Int;
            else if constexpr (std::is_same_v<M, float>)     return FieldType::Float;
            else if constexpr (std::is_same_v<M, glm::vec2>) return FieldType::Vec2;
            else if constexpr (std::is_same_v<M, glm::vec3>) return FieldType::Vec3;
            else if constexpr (std::is_same_v<M, glm::vec4>) return FieldType::Vec4;
            else if constexpr (std::is_same_v<M, std::string>) return FieldType::String;
            else if constexpr (std::is_same_v<M, EntityRef>) return FieldType::EntityRef;
            else static_assert(sizeof(M) == 0, "Desteklenmeyen alan tipi.");
        }
    }

    // Zincirlemeli kayit arayuzu. Tipli kalir (yanlis uye isaretcisi
    // derlenmez), sakladigi sey tipsizdir.
    template<typename T>
    class ComponentBuilder
    {
    public:
        explicit ComponentBuilder(ComponentInfo& info) : m_Info(&info) {}

        // Uye isaretcisi SABLON ARGUMANI: derleyici erisimciyi derleme
        // zamaninda uretebilsin diye. Yanlis bir uye verilirse derlenmez.
        template<auto Member>
        ComponentBuilder& Field(const char* name, const char* label)
        {
            using M = std::remove_reference_t<decltype(std::declval<T&>().*Member)>;

            FieldInfo f;
            f.Name  = name;
            f.Label = label;
            f.Type  = Detail::DeduceType<M>();
            f.Get   = [](void* c) -> void* { return &(static_cast<T*>(c)->*Member); };

            m_Info->Fields.push_back(f);
            return *this;
        }

        // vec4'u renk olarak gostermek istedigimizde.
        ComponentBuilder& AsColor()
        {
            m_Info->Fields.back().Type = FieldType::Color;
            return *this;
        }

        ComponentBuilder& Speed(float s)
        {
            m_Info->Fields.back().DragSpeed = s;
            return *this;
        }

        ComponentBuilder& Range(float min, float max)
        {
            m_Info->Fields.back().Min = min;
            m_Info->Fields.back().Max = max;
            return *this;
        }

        // Veri radyan, arayuz derece.
        ComponentBuilder& Degrees()
        {
            m_Info->Fields.back().DegreesInUI = true;
            return *this;
        }

        // Serilestirilir ama genel cizici ile cizilmez.
        ComponentBuilder& CustomUI()
        {
            m_Info->Fields.back().HiddenInInspector = true;
            return *this;
        }

        ComponentBuilder& OnAdded(void (*fn)(Entity))
        {
            m_Info->OnAdded = fn;
            return *this;
        }

        ComponentBuilder& NotRemovable()   { m_Info->Removable = false;       return *this; }
        ComponentBuilder& HiddenInAddMenu(){ m_Info->AddableFromMenu = false; return *this; }

        // Yapisal component: yalnizca kopyalamaya katilir.
        ComponentBuilder& Structural()
        {
            m_Info->SerializedByTable = false;
            m_Info->ShowInInspector   = false;
            m_Info->AddableFromMenu   = false;
            m_Info->Removable         = false;
            return *this;
        }

        ComponentBuilder& Extra(void (*save)(const void*, nlohmann::json&),
                                void (*load)(void*, const nlohmann::json&, TextureLibrary*))
        {
            m_Info->SaveExtra = save;
            m_Info->LoadExtra = load;
            return *this;
        }

    private:
        ComponentInfo* m_Info;
    };

    class ComponentRegistry
    {
    public:
        // Kayit sirasi Inspector'daki gosterim sirasidir.
        template<typename T>
        static ComponentBuilder<T> Register(const char* name, const char* label)
        {
            ComponentInfo& info = Entries().emplace_back();
            info.Name  = name;
            info.Label = label;

            info.Has          = [](Entity e) { return e.HasComponent<T>(); };
            info.GetPtr       = [](Entity e) -> void* {
                return e.HasComponent<T>() ? &e.GetComponent<T>() : nullptr;
            };
            info.AddAndGetPtr = [](Entity e) -> void* {
                return &e.AddOrReplaceComponent<T>();
            };
            info.Add          = [](Entity e) { e.AddOrReplaceIfMissing<T>(); };
            info.Remove       = [](Entity e) {
                if (e.HasComponent<T>()) e.RemoveComponent<T>();
            };
            info.CopyTo       = [](Entity dst, Entity src) {
                if (src.HasComponent<T>())
                    dst.AddOrReplaceComponent<T>(src.GetComponent<T>());
            };

            return ComponentBuilder<T>(info);
        }

        // deque: referanslar ekleme sirasinda gecerli kaliyor.
        // Ilk erisimde motor component'leri kendiliginden kaydedilir -
        // "RegisterBuiltins cagirmayi unutma" diye sessiz bir hata
        // sinifi yaratmiyoruz.
        static const std::deque<ComponentInfo>& GetAll();

        static ComponentInfo* Find(std::string_view name);

        // Motorun kendi component'lerini kaydeder. Idempotent.
        static void RegisterBuiltins();

        // Yalnizca test icin: tabloyu bosaltip yeniden kurulabilir yapar.
        static void Reset();

    private:
        static std::deque<ComponentInfo>& Entries();
    };
}
