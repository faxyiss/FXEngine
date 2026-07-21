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

    std::shared_ptr<Texture2D> TextureLibrary::MissingTexture()
    {
        if (m_Missing)
            return m_Missing;

        // 2x2 damali: mor / siyah. RGBA, satir satir.
        constexpr unsigned char M[4] = { 255,   0, 255, 255 };  // mor
        constexpr unsigned char K[4] = {   0,   0,   0, 255 };  // siyah
        const unsigned char pixels[16] = {
            M[0],M[1],M[2],M[3],  K[0],K[1],K[2],K[3],
            K[0],K[1],K[2],K[3],  M[0],M[1],M[2],M[3],
        };

        TextureSpec spec;
        spec.MinFilter = TextureFilter::Nearest;   // damalar keskin kalsin
        spec.MagFilter = TextureFilter::Nearest;
        spec.GenerateMipmaps = false;

        m_Missing = std::make_shared<Texture2D>(2u, 2u, pixels, 4u, spec);
        return m_Missing;
    }

    std::shared_ptr<Texture2D> TextureLibrary::Load(const std::string& path)
    {
        if (path.empty())
            return nullptr;   // "doku atanmadi" - eksik degil, bos

        const auto it = m_Textures.find(path);
        if (it != m_Textures.end())
            return it->second;

        auto texture = std::make_shared<Texture2D>(path, SpecForAsset(path));

        if (!texture->IsValid())
        {
            FX_CORE_ERROR("TextureLibrary: '%s' yuklenemedi.", path.c_str());

            // BASARISIZ SONUCU ONBELLEGE ALMIYORUZ: dosya sonradan
            // duzeltilse bile hep eksik doku donerdi. Bedeli, bozuk bir
            // yolun her istendiginde tekrar denenmesi. Atanmis ama
            // yuklenemeyen dokuyu gorunmez birakmak yerine mor "eksik
            // doku" gosteriyoruz - sorun ekranda belli olsun.
            return MissingTexture();
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
