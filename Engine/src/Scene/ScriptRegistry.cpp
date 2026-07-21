#include "FXEngine/Scene/ScriptRegistry.h"
#include "FXEngine/Core/Log.h"

#include <algorithm>
#include <unordered_map>

namespace FX
{
    namespace
    {
        std::unordered_map<std::string, ScriptRegistry::Factory> s_Factories;

        // Ayri tutuluyor cunku sirasi onemli (alfabetik) ve her
        // Inspector cizimi icin haritayi gezip siralamak israf olurdu.
        std::vector<std::string> s_Names;
    }

    void ScriptRegistry::Register(const std::string& name, Factory factory)
    {
        if (name.empty() || !factory)
        {
            FX_CORE_WARN("ScriptRegistry: gecersiz kayit yok sayildi.");
            return;
        }

        if (s_Factories.count(name))
        {
            FX_CORE_WARN("ScriptRegistry: '%s' zaten kayitli, ikincisi yok sayildi.",
                         name.c_str());
            return;
        }

        s_Factories[name] = std::move(factory);

        s_Names.push_back(name);
        std::sort(s_Names.begin(), s_Names.end());
    }

    ScriptableEntity* ScriptRegistry::Create(const std::string& name)
    {
        const auto it = s_Factories.find(name);
        if (it == s_Factories.end())
            return nullptr;

        return it->second();
    }

    bool ScriptRegistry::Contains(const std::string& name)
    {
        return s_Factories.count(name) != 0;
    }

    const std::vector<std::string>& ScriptRegistry::GetNames()
    {
        return s_Names;
    }

    void ScriptRegistry::Clear()
    {
        s_Factories.clear();
        s_Names.clear();
    }
}
