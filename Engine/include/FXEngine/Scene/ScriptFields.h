#pragma once

// ===========================================================================
// FXEngine - Script alan reflection'i
//
// PROBLEM: Bir script'in (Gezgin gibi) `m_Speed` alanini editörden
// ayarlamak istiyoruz. Ama script Game.dll'de yasiyor ve DLL yeniden
// yuklendiginde INSTANCE'lar yok olup yeniden yaratiliyor - instance'a
// yazilan deger kaybolur.
//
// COZUM: Deger INSTANCE'ta degil, NativeScriptComponent'te VERI olarak
// (ad -> deger haritasi) durur. Play'de instance yaratilinca haritadaki
// override'lar uygulanir. Editör de ayni haritayi duzenler.
//
// Script hangi alanlari acacagini OnReflect'te bildirir. Tek bir gezi,
// farkli ziyaretcilerle: uygula (harita->instance), ciz (Inspector),
// serilestir. Reflection kutuphanesi degil, acik bir ziyaretci - motorun
// A1'deki alan tablosuyla ayni felsefe.
// ===========================================================================

#include "FXEngine/Scene/EntityRef.h"

#include <glm/glm.hpp>

#include <string>
#include <unordered_map>

namespace FX
{
    // Script OnReflect icinde her alanini Visit ile bildirir. Varsayilan
    // govdeler bos: bir ziyaretci yalnizca ilgilendigi tipleri ezer.
    class ScriptFieldVisitor
    {
    public:
        virtual ~ScriptFieldVisitor() = default;

        virtual void Visit(const char* name, float& value)       { (void)name; (void)value; }
        virtual void Visit(const char* name, int& value)         { (void)name; (void)value; }
        virtual void Visit(const char* name, bool& value)        { (void)name; (void)value; }
        virtual void Visit(const char* name, glm::vec2& value)   { (void)name; (void)value; }
        virtual void Visit(const char* name, glm::vec3& value)   { (void)name; (void)value; }
        virtual void Visit(const char* name, glm::vec4& value)   { (void)name; (void)value; }
        virtual void Visit(const char* name, std::string& value) { (void)name; (void)value; }

        // Hedef entity: script tarafinda FindEntityByName yerine. Inspector'da
        // entity secici cizilir, deger UUID olarak kaydedilir, Play'de
        // instance'a uygulanir. 16c'deki her karede ada gore arama bunu
        // gerektirdi.
        virtual void Visit(const char* name, EntityRef& value)   { (void)name; (void)value; }
    };

    // Bir script alaninin override degeri. NativeScriptComponent'te
    // ad -> deger olarak tutulur, sahne dosyasina serilestirilir.
    struct ScriptFieldValue
    {
        enum class Type { Float, Int, Bool, Vec2, Vec3, Vec4, String, Entity };
        Type        Kind = Type::Float;
        float       F  = 0.0f;
        int         I  = 0;
        bool        B  = false;
        glm::vec2   V2 { 0.0f };
        glm::vec3   V3 { 0.0f };
        glm::vec4   V4 { 0.0f };
        std::string S;
        UUID        E{ 0 };   // Entity turu icin: hedefin UUID'si
    };

    using ScriptFieldMap = std::unordered_map<std::string, ScriptFieldValue>;

    // Haritadaki override'lari bir instance'in alanlarina yazar. Play'de
    // (OnCreate'ten ONCE) ve editör önizlemesinde kullaniliyor. Haritada
    // olmayan alan dokunulmaz - script'in kendi varsayilani kalir.
    class ScriptFieldApplier : public ScriptFieldVisitor
    {
    public:
        explicit ScriptFieldApplier(const ScriptFieldMap& values) : m_Values(&values) {}

        void Visit(const char* name, float& v) override
        { if (const auto* e = Find(name, ScriptFieldValue::Type::Float))  v = e->F; }
        void Visit(const char* name, int& v) override
        { if (const auto* e = Find(name, ScriptFieldValue::Type::Int))    v = e->I; }
        void Visit(const char* name, bool& v) override
        { if (const auto* e = Find(name, ScriptFieldValue::Type::Bool))   v = e->B; }
        void Visit(const char* name, glm::vec2& v) override
        { if (const auto* e = Find(name, ScriptFieldValue::Type::Vec2))   v = e->V2; }
        void Visit(const char* name, glm::vec3& v) override
        { if (const auto* e = Find(name, ScriptFieldValue::Type::Vec3))   v = e->V3; }
        void Visit(const char* name, glm::vec4& v) override
        { if (const auto* e = Find(name, ScriptFieldValue::Type::Vec4))   v = e->V4; }
        void Visit(const char* name, std::string& v) override
        { if (const auto* e = Find(name, ScriptFieldValue::Type::String)) v = e->S; }
        void Visit(const char* name, EntityRef& v) override
        { if (const auto* e = Find(name, ScriptFieldValue::Type::Entity)) v.Target = e->E; }

    private:
        const ScriptFieldValue* Find(const char* name, ScriptFieldValue::Type kind) const
        {
            const auto it = m_Values->find(name);
            if (it == m_Values->end() || it->second.Kind != kind)
                return nullptr;
            return &it->second;
        }

        const ScriptFieldMap* m_Values;
    };
}
