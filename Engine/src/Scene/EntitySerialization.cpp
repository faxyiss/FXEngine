#include "Scene/EntitySerialization.h"

#include "FXEngine/Scene/ComponentMeta.h"
#include "FXEngine/Scene/Components.h"
#include "FXEngine/Core/Log.h"

namespace FX::Detail
{
    using json = nlohmann::json;

    namespace
    {
        json ToJson(const glm::vec2& v) { return json::array({ v.x, v.y }); }
        json ToJson(const glm::vec3& v) { return json::array({ v.x, v.y, v.z }); }
        json ToJson(const glm::vec4& v) { return json::array({ v.x, v.y, v.z, v.w }); }

        // Okurken savunmaci: alan yoksa veya boyutu yanlissa mevcut degeri
        // (component'in varsayilani) koru, cokme. Sahne dosyasi elle
        // duzenlenebilir olmali.
        template<int N>
        void ReadVec(const json& j, float* dst)
        {
            if (!j.is_array() || j.size() < N)
                return;

            for (int i = 0; i < N; ++i)
                dst[i] = j[i].get<float>();
        }

        void WriteField(const FieldInfo& f, const void* component, json& out)
        {
            // const_cast: erisimci tek bir imzayla uretiliyor; burada
            // yalnizca okuyoruz.
            void* p = f.Get(const_cast<void*>(component));

            switch (f.Type)
            {
            case FieldType::Bool:   out[f.Name] = *static_cast<bool*>(p);        break;
            case FieldType::Int:    out[f.Name] = *static_cast<int*>(p);         break;
            case FieldType::Float:  out[f.Name] = *static_cast<float*>(p);       break;
            case FieldType::Vec2:   out[f.Name] = ToJson(*static_cast<glm::vec2*>(p)); break;
            case FieldType::Vec3:   out[f.Name] = ToJson(*static_cast<glm::vec3*>(p)); break;
            case FieldType::Vec4:
            case FieldType::Color:  out[f.Name] = ToJson(*static_cast<glm::vec4*>(p)); break;
            case FieldType::String: out[f.Name] = *static_cast<std::string*>(p);  break;

            case FieldType::EntityRef:
                // Isaretci degil KIMLIK yaziliyor (Faz 8).
                out[f.Name] = static_cast<std::uint64_t>(
                    static_cast<EntityRef*>(p)->Target);
                break;
            }
        }

    }

    void ReadField(const FieldInfo& f, void* component, const json& in)
    {
        // Alan dosyada yoksa component'in varsayilani gecerli kalir.
        // Eski sahneler yeni alanlarla sessizce uyumlu oluyor.
        if (!in.contains(f.Name))
            return;

        const json& v = in[f.Name];
        void* p = f.Get(component);

        switch (f.Type)
        {
        case FieldType::Bool:   if (v.is_boolean())        *static_cast<bool*>(p)  = v.get<bool>();  break;
        case FieldType::Int:    if (v.is_number())         *static_cast<int*>(p)   = v.get<int>();   break;
        case FieldType::Float:  if (v.is_number())         *static_cast<float*>(p) = v.get<float>(); break;
        case FieldType::Vec2:   ReadVec<2>(v, &static_cast<glm::vec2*>(p)->x); break;
        case FieldType::Vec3:   ReadVec<3>(v, &static_cast<glm::vec3*>(p)->x); break;
        case FieldType::Vec4:
        case FieldType::Color:  ReadVec<4>(v, &static_cast<glm::vec4*>(p)->x); break;

        case FieldType::String:
            if (v.is_string()) *static_cast<std::string*>(p) = v.get<std::string>();
            break;

        case FieldType::EntityRef:
            // Hedef entity henuz olusturulmamis olabilir; sorun degil,
            // cozumleme her karede tekrarlaniyor.
            if (v.is_number_unsigned())
                static_cast<EntityRef*>(p)->Target = UUID(v.get<std::uint64_t>());
            break;
        }
    }

    void RemapReferences(Entity entity, const std::unordered_map<UUID, UUID>& remap)
    {
        // Tabloda yoksa DOKUNMA: disariya bakan referans hala gecerli.
        auto lookup = [&](UUID old) -> UUID
        {
            const auto it = remap.find(old);
            return it != remap.end() ? it->second : old;
        };

        // Component EntityRef alanlari - alan tablosundan (A1). Follow.Target
        // ve ileride eklenecek her EntityRef alani buradan gecer.
        for (const ComponentInfo& info : ComponentRegistry::GetAll())
        {
            if (!info.Has(entity))
                continue;

            void* comp = info.GetPtr(entity);
            for (const FieldInfo& f : info.Fields)
            {
                if (f.Type != FieldType::EntityRef)
                    continue;

                auto* ref = static_cast<EntityRef*>(f.Get(comp));
                ref->Target = lookup(ref->Target);
            }
        }

        // Script Entity alanlari (A-3) - alan tablosunda degil, dinamik
        // bir haritada yasiyorlar.
        if (entity.HasComponent<NativeScriptComponent>())
        {
            auto& nsc = entity.GetComponent<NativeScriptComponent>();
            for (auto& [name, val] : nsc.Fields)
            {
                if (val.Kind == ScriptFieldValue::Type::Entity)
                    val.E = lookup(val.E);
            }
        }
    }

    json SerializeComponent(const ComponentInfo& info, Entity entity)
    {
        json obj = json::object();

        for (const FieldInfo& f : info.Fields)
            WriteField(f, info.GetPtr(entity), obj);

        // Alan tablosunun ifade edemedigi veri (doku slotu).
        if (info.SaveExtra)
            info.SaveExtra(info.GetPtr(entity), obj);

        return obj;
    }

    void ApplyComponent(const ComponentInfo& info, Entity entity,
                        const json& obj, TextureLibrary* library)
    {
        if (!obj.is_object())
            return;

        // Varsayilanla olusturup uzerine dosyadakini yaziyoruz:
        // eksik alanlar component'in kendi varsayilanini alir,
        // fallback degerleri ikinci bir yerde tekrarlanmaz.
        void* component = info.AddAndGetPtr(entity);

        for (const FieldInfo& f : info.Fields)
            ReadField(f, component, obj);

        if (info.LoadExtra)
            info.LoadExtra(component, obj, library);
    }

    json SerializeEntity(Entity entity)
    {
        json e;

        // --- Yapisal alanlar: entity duzeyinde, component nesnesi degil ----
        if (entity.HasComponent<IDComponent>())
            e["ID"] = static_cast<std::uint64_t>(entity.GetComponent<IDComponent>().ID);

        if (entity.HasComponent<TagComponent>())
            e["Tag"] = entity.GetComponent<TagComponent>().Tag;

        // Sadece PARENT yaziliyor, cocuk listesi degil: cocuklar bundan
        // yeniden kurulabilir. Iki yonu de yazmak, ikisinin tutarsiz
        // kalabilecegi bir dosya formati demektir.
        if (entity.HasComponent<RelationshipComponent>())
        {
            const auto& rc = entity.GetComponent<RelationshipComponent>();
            if (rc.Parent.IsValid())
                e["Parent"] = static_cast<std::uint64_t>(rc.Parent);
        }

        // --- Component'ler: tamami meta tablosundan (A1) --------------------
        for (const ComponentInfo& info : ComponentRegistry::GetAll())
        {
            if (!info.SerializedByTable || !info.Has(entity))
                continue;

            e[info.Name] = SerializeComponent(info, entity);
        }

        return e;
    }

    void ApplyComponents(Entity entity, const json& j, TextureLibrary* library)
    {
        for (const ComponentInfo& info : ComponentRegistry::GetAll())
        {
            if (!info.SerializedByTable || !j.contains(info.Name))
                continue;

            ApplyComponent(info, entity, j[info.Name], library);
        }
    }
}
