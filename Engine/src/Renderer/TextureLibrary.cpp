#include "FXEngine/Renderer/TextureLibrary.h"
#include "FXEngine/Core/Log.h"

namespace FX
{
    std::shared_ptr<Texture2D> TextureLibrary::Load(const std::string& path,
                                                    const TextureSpec& spec)
    {
        if (path.empty())
            return nullptr;

        // Onbellekte var mi?
        const auto it = m_Textures.find(path);
        if (it != m_Textures.end())
            return it->second;

        auto texture = std::make_shared<Texture2D>(path, spec);

        if (!texture->IsValid())
        {
            FX_CORE_ERROR("TextureLibrary: '%s' yuklenemedi.", path.c_str());

            // BASARISIZ SONUCU DA ONBELLEGE ALMIYORUZ.
            // Alsaydik, dosya sonradan duzeltilse bile hep nullptr donerdi.
            // Ama dikkat: bu, bozuk bir yolun her istendiginde tekrar
            // denenmesi demek. Sahne yuklerken bir kez olur, sorun degil.
            return nullptr;
        }

        m_Textures[path] = texture;
        return texture;
    }

    void TextureLibrary::Add(const std::string& path, const std::shared_ptr<Texture2D>& texture)
    {
        if (path.empty() || !texture)
            return;

        m_Textures[path] = texture;
    }

    std::shared_ptr<Texture2D> TextureLibrary::Get(const std::string& path) const
    {
        const auto it = m_Textures.find(path);
        return (it != m_Textures.end()) ? it->second : nullptr;
    }

    bool TextureLibrary::Exists(const std::string& path) const
    {
        return m_Textures.find(path) != m_Textures.end();
    }

    void TextureLibrary::Clear()
    {
        m_Textures.clear();
    }
}
