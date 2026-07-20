#include "FXEngine/Renderer/TextureLibrary.h"
#include "FXEngine/Core/Log.h"
#include "FXEngine/Asset/AssetManager.h"

namespace FX
{
    TextureSpec TextureLibrary::SpecForAsset(const std::string& path)
    {
        TextureSpec spec;

        const AssetHandle handle = AssetManager::GetHandle(path);
        if (!handle.IsValid())
            return spec;   // proje disi veya henuz taranmamis: varsayilanlar

        const AssetMetadata& meta = AssetManager::GetMetadata(handle);
        if (meta.Type != AssetType::Texture)
            return spec;

        const auto& ts = meta.TextureSettings;

        spec.MinFilter = ts.Nearest ? TextureFilter::Nearest : TextureFilter::Linear;
        spec.MagFilter = spec.MinFilter;

        spec.WrapS = ts.Repeat ? TextureWrap::Repeat : TextureWrap::ClampToEdge;
        spec.WrapT = spec.WrapS;

        spec.GenerateMipmaps = ts.GenerateMipmaps;

        return spec;
    }

    std::shared_ptr<Texture2D> TextureLibrary::Load(const std::string& path)
    {
        if (path.empty())
            return nullptr;

        const auto it = m_Textures.find(path);
        if (it != m_Textures.end())
            return it->second;

        auto texture = std::make_shared<Texture2D>(path, SpecForAsset(path));

        if (!texture->IsValid())
        {
            FX_CORE_ERROR("TextureLibrary: '%s' yuklenemedi.", path.c_str());

            // BASARISIZ SONUCU ONBELLEGE ALMIYORUZ.
            // Alsaydik, dosya sonradan duzeltilse bile hep nullptr donerdi.
            // Bedeli: bozuk bir yol her istendiginde tekrar denenir.
            return nullptr;
        }

        m_Textures[path] = texture;
        return texture;
    }

    std::shared_ptr<Texture2D> TextureLibrary::Reload(const std::string& path)
    {
        const auto it = m_Textures.find(path);
        if (it == m_Textures.end())
            return Load(path);

        auto fresh = std::make_shared<Texture2D>(path, SpecForAsset(path));
        if (!fresh->IsValid())
        {
            FX_CORE_ERROR("TextureLibrary: '%s' yeniden yuklenemedi.", path.c_str());
            return it->second;   // eskisi durmaya devam etsin
        }

        // Onbellek girdisini DEGISTIRIYORUZ ama sahnedeki entity'ler eski
        // shared_ptr'i tutuyor olabilir. Bu yuzden cagiranin (Inspector)
        // sahneyi de guncellemesi gerekir; alternatifi bir dolayli
        // katman (AssetHandle -> Texture) eklemekti, o da butun cagri
        // yerlerini degistirmek demekti.
        m_Textures[path] = fresh;
        return fresh;
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
