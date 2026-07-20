#include "FXEngine/Asset/AssetManager.h"
#include "FXEngine/Core/FileSystem.h"
#include "FXEngine/Core/Project.h"
#include "FXEngine/Core/Log.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>

namespace FX
{
    using json = nlohmann::json;

    // -----------------------------------------------------------------------
    // Tur cevrimleri
    // -----------------------------------------------------------------------
    const char* AssetTypeToString(AssetType type)
    {
        switch (type)
        {
            case AssetType::Texture: return "Texture";
            case AssetType::Scene:   return "Scene";
            case AssetType::Prefab:  return "Prefab";
            default:                 return "None";
        }
    }

    AssetType AssetTypeFromString(const std::string& text)
    {
        if (text == "Texture") return AssetType::Texture;
        if (text == "Scene")   return AssetType::Scene;
        if (text == "Prefab")  return AssetType::Prefab;
        return AssetType::None;
    }

    AssetType AssetTypeFromExtension(const std::string& extension)
    {
        std::string ext = extension;
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (!ext.empty() && ext.front() == '.')
            ext.erase(ext.begin());

        if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp")
            return AssetType::Texture;
        if (ext == "fxscene")  return AssetType::Scene;
        if (ext == "fxprefab") return AssetType::Prefab;

        return AssetType::None;
    }

    namespace
    {
        constexpr int kMetaVersion = 1;
        constexpr const char* kMetaExtension = ".meta";

        std::unordered_map<AssetHandle, AssetMetadata> s_Assets;   // GUID -> kayit
        std::unordered_map<std::string, AssetHandle>   s_ByPath;   // yol  -> GUID

        const std::string   s_EmptyPath;
        const AssetMetadata s_EmptyMeta;

        // Yollari TEK BIR bicimde tutuyoruz: '/' ayirici, kucuk harf
        // KARSILASTIRMASI yok (Windows dosya sistemi buyuk/kucuk harfe
        // duyarsiz ama yolu oldugu gibi saklamak dogru; sadece ayiriciyi
        // normalize ediyoruz).
        std::string Normalize(std::string p)
        {
            std::replace(p.begin(), p.end(), '\\', '/');
            return p;
        }
    }

    std::string AssetManager::MetaPathFor(const std::string& relativePath)
    {
        return relativePath + kMetaExtension;
    }

    bool AssetManager::IsMetaFile(const std::string& path)
    {
        const std::string ext = kMetaExtension;
        return path.size() > ext.size() &&
               path.compare(path.size() - ext.size(), ext.size(), ext) == 0;
    }

    void AssetManager::Clear()
    {
        s_Assets.clear();
        s_ByPath.clear();
    }

    std::size_t AssetManager::GetCount()
    {
        return s_Assets.size();
    }

    std::vector<AssetMetadata> AssetManager::GetAll()
    {
        std::vector<AssetMetadata> all;
        all.reserve(s_Assets.size());
        for (const auto& [handle, meta] : s_Assets)
            all.push_back(meta);
        return all;
    }

    bool AssetManager::WriteMeta(const AssetMetadata& meta)
    {
        json j;
        j["Version"] = kMetaVersion;
        j["Guid"]    = static_cast<std::uint64_t>(meta.Handle);
        j["Type"]    = AssetTypeToString(meta.Type);

        if (meta.Type == AssetType::Texture)
        {
            j["Texture"] = {
                { "Nearest",         meta.TextureSettings.Nearest },
                { "Repeat",          meta.TextureSettings.Repeat },
                { "GenerateMipmaps", meta.TextureSettings.GenerateMipmaps }
            };
        }

        const std::string full =
            FileSystem::ResolveProjectAsset(MetaPathFor(meta.RelativePath));

        std::ofstream out(full);
        if (!out)
        {
            FX_CORE_ERROR("Meta yazilamadi: %s", full.c_str());
            return false;
        }

        out << std::setw(4) << j << std::endl;
        return true;
    }

    AssetMetadata AssetManager::LoadOrCreateMeta(const std::string& relativePath,
                                                 AssetType type)
    {
        AssetMetadata meta;
        meta.RelativePath = relativePath;
        meta.Type         = type;

        const std::string metaFull =
            FileSystem::ResolveProjectAsset(MetaPathFor(relativePath));

        std::ifstream in(metaFull);
        if (in)
        {
            json j;
            try
            {
                in >> j;
                const auto guid = j.value("Guid", std::uint64_t{ 0 });
                if (guid != 0)
                {
                    meta.Handle = AssetHandle(guid);

                    if (type == AssetType::Texture && j.contains("Texture"))
                    {
                        const auto& t = j["Texture"];
                        meta.TextureSettings.Nearest =
                            t.value("Nearest", true);
                        meta.TextureSettings.Repeat =
                            t.value("Repeat", false);
                        meta.TextureSettings.GenerateMipmaps =
                            t.value("GenerateMipmaps", true);
                    }

                    // Tur .meta'da yaziyor ama UZANTIYA guveniyoruz:
                    // dosya yeniden adlandirilip uzantisi degismis
                    // olabilir ve o durumda gercek olan uzantidir.
                    return meta;
                }

                FX_CORE_WARN("Meta'da gecerli Guid yok, yeniden uretiliyor: %s",
                             relativePath.c_str());
            }
            catch (const std::exception& e)
            {
                FX_CORE_WARN("Meta bozuk (%s), yeniden uretiliyor: %s",
                             relativePath.c_str(), e.what());
            }
        }

        // .meta yok veya kullanilamaz -> yeni kimlik.
        //
        // DIKKAT: bu, o varliga isaret eden ESKI referanslari kirar.
        // Bu yuzden .meta dosyalari surum kontrolune girmeli; kullaniciya
        // da soyluyoruz.
        meta.Handle = AssetHandle();   // rastgele uretir
        WriteMeta(meta);

        return meta;
    }

    void AssetManager::ScanProject()
    {
        Clear();

        // Proje yoksa da tariyoruz: FileSystem o durumda exe klasorune
        // dusuyor ve oradaki ornek varliklarin da kimligi olmali.
        // "Projesiz devam et" modunda dokularin .meta ayarlari yine
        // gecerli olsun istiyoruz.
        const std::string assetDir = Project::HasActive()
                                   ? Project::GetActive()->GetConfig().AssetDirectory
                                   : std::string("assets");
        const std::string rootFull = FileSystem::ResolveProjectAsset(assetDir);

        std::error_code ec;
        if (!std::filesystem::exists(rootFull, ec))
        {
            FX_CORE_WARN("AssetManager: varlik klasoru yok: %s", rootFull.c_str());
            return;
        }

        int created = 0;

        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(rootFull, ec))
        {
            if (ec)
                break;
            if (!entry.is_regular_file(ec))
                continue;

            const std::string absolute = entry.path().string();

            // .meta dosyalarinin kendisi varlik degil.
            if (IsMetaFile(absolute))
                continue;

            const AssetType type = AssetTypeFromExtension(entry.path().extension().string());
            if (type == AssetType::None)
                continue;   // tanimadigimiz uzanti: .meta uretmiyoruz

            const std::string relative =
                Normalize(FileSystem::MakeRelativeToProject(absolute));

            const std::string metaFull =
                FileSystem::ResolveProjectAsset(MetaPathFor(relative));
            const bool hadMeta = std::filesystem::exists(metaFull, ec);

            AssetMetadata meta = LoadOrCreateMeta(relative, type);
            if (!hadMeta)
                ++created;

            // Ayni GUID iki dosyada olabilir: kullanici bir varligi
            // .meta'siyla birlikte KOPYALADIYSA. Sessizce birini
            // kaybetmek yerine ikincisine yeni kimlik veriyoruz.
            if (s_Assets.count(meta.Handle))
            {
                FX_CORE_WARN("Ayni GUID iki varlikta: '%s' ve '%s'. Ikincisine "
                             "yeni kimlik veriliyor.",
                             s_Assets[meta.Handle].RelativePath.c_str(),
                             relative.c_str());

                meta.Handle = AssetHandle();
                WriteMeta(meta);
                ++created;
            }

            s_Assets[meta.Handle] = meta;
            s_ByPath[relative]    = meta.Handle;
        }

        FX_CORE_INFO("AssetManager: %zu varlik (%d yeni .meta).",
                     s_Assets.size(), created);
    }

    AssetHandle AssetManager::GetHandle(const std::string& relativePath)
    {
        const auto it = s_ByPath.find(Normalize(relativePath));
        return (it != s_ByPath.end()) ? it->second : AssetHandle(0);
    }

    const std::string& AssetManager::GetPath(AssetHandle handle)
    {
        const auto it = s_Assets.find(handle);
        return (it != s_Assets.end()) ? it->second.RelativePath : s_EmptyPath;
    }

    bool AssetManager::Exists(AssetHandle handle)
    {
        return s_Assets.count(handle) != 0;
    }

    const AssetMetadata& AssetManager::GetMetadata(AssetHandle handle)
    {
        const auto it = s_Assets.find(handle);
        return (it != s_Assets.end()) ? it->second : s_EmptyMeta;
    }

    AssetHandle AssetManager::Register(const std::string& relativePath)
    {
        const std::string relative = Normalize(relativePath);

        // Zaten kayitliysa mevcut kimligi dondur: ayni dosyaya iki GUID
        // vermek tam da kacindigimiz sey.
        if (const AssetHandle existing = GetHandle(relative); existing.IsValid())
            return existing;

        const AssetType type =
            AssetTypeFromExtension(std::filesystem::path(relative).extension().string());

        if (type == AssetType::None)
            return AssetHandle(0);

        AssetMetadata meta = LoadOrCreateMeta(relative, type);

        s_Assets[meta.Handle] = meta;
        s_ByPath[relative]    = meta.Handle;

        return meta.Handle;
    }

    void AssetManager::OnAssetMoved(const std::string& oldRelative,
                                    const std::string& newRelative)
    {
        const std::string oldRel = Normalize(oldRelative);
        const std::string newRel = Normalize(newRelative);

        const auto it = s_ByPath.find(oldRel);
        if (it == s_ByPath.end())
        {
            // Tabloda yoktu (tanimadigimiz uzanti olabilir). Yeni yerinde
            // kayitli olmasi gerekiyorsa Register halleder.
            Register(newRel);
            return;
        }

        const AssetHandle handle = it->second;
        s_ByPath.erase(it);

        // ISIN OZU BURASI: yol degisti, KIMLIK DEGISMEDI. Sahne
        // dosyalarindaki referanslar GUID tuttugu icin hala gecerli.
        s_Assets[handle].RelativePath = newRel;
        s_ByPath[newRel]              = handle;

        // .meta dosyasi da tasinmali; tasinmadiysa (icerik paneli
        // tasimadiysa) yeniden yaziyoruz ki bir sonraki taramada
        // ayni kimlik bulunsun.
        std::error_code ec;
        const std::string newMeta =
            FileSystem::ResolveProjectAsset(MetaPathFor(newRel));

        if (!std::filesystem::exists(newMeta, ec))
            WriteMeta(s_Assets[handle]);

        // Eski .meta arta kalmissa temizle.
        const std::string oldMeta =
            FileSystem::ResolveProjectAsset(MetaPathFor(oldRel));
        if (std::filesystem::exists(oldMeta, ec))
            std::filesystem::remove(oldMeta, ec);
    }

    bool AssetManager::UpdateTextureSettings(AssetHandle handle,
                                             const TextureImportSettings& settings)
    {
        const auto it = s_Assets.find(handle);
        if (it == s_Assets.end() || it->second.Type != AssetType::Texture)
            return false;

        it->second.TextureSettings = settings;
        return WriteMeta(it->second);
    }

    void AssetManager::OnAssetDeleted(const std::string& relativePath)
    {
        const std::string relative = Normalize(relativePath);

        const auto it = s_ByPath.find(relative);
        if (it == s_ByPath.end())
            return;

        s_Assets.erase(it->second);
        s_ByPath.erase(it);

        std::error_code ec;
        const std::string metaFull =
            FileSystem::ResolveProjectAsset(MetaPathFor(relative));
        if (std::filesystem::exists(metaFull, ec))
            std::filesystem::remove(metaFull, ec);
    }
}
