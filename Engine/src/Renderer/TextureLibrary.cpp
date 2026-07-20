#include "FXEngine/Renderer/TextureLibrary.h"
#include "FXEngine/Core/Log.h"

namespace FX
{
    std::shared_ptr<Texture2D> TextureLibrary::Load(const std::string& path)
    {
        // Ayar talep edilmedi -> uyarilacak bir sey yok.
        return LoadInternal(path, TextureSpec{}, false);
    }

    std::shared_ptr<Texture2D> TextureLibrary::Load(const std::string& path,
                                                    const TextureSpec& spec)
    {
        return LoadInternal(path, spec, true);
    }

    std::shared_ptr<Texture2D> TextureLibrary::LoadInternal(const std::string& path,
                                                            const TextureSpec& spec,
                                                            bool warnOnSpecMismatch)
    {
        if (path.empty())
            return nullptr;

        // Onbellekte var mi?
        const auto it = m_Textures.find(path);
        if (it != m_Textures.end())
        {
            // DIKKAT: onbellek YOLA gore anahtarli, spec'e gore degil.
            // Ayni dosya farkli bir spec ile ikinci kez istenirse ILK
            // yuklemenin spec'i gecerli kalir - sessizce.
            //
            // Sessiz birakmiyoruz cunku belirtisi cok dolayli: "sahneyi
            // yeniden yukleyince zeminin dosemesi bozuldu" gibi.
            //
            // Ama uyari SADECE aciktan ayar isteyene basiliyor: icerik
            // paneli gibi "ne varsa ver" diyen cagiranlar icin yanlis
            // alarm olurdu.
            //
            // Dogru cozum dosya basina kalici doku ayari (.fxmeta) -> Faz 21.
            // O zamana kadar bir dosyanin spec'i, ilk yuklendigi yerdedir.
            if (warnOnSpecMismatch && m_Specs.count(path) && !(m_Specs[path] == spec))
                FX_CORE_WARN("TextureLibrary: '%s' farkli bir spec ile istendi; "
                             "ilk yuklemenin ayarlari kullanilacak.", path.c_str());

            return it->second;
        }

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
        m_Specs[path]    = spec;
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
        m_Specs.clear();
    }
}
