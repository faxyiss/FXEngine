#pragma once

// ===========================================================================
// FXEngine - Shader
//
// Shader, GPU'da calisan bir programdir. Iki parcadan olusur:
//   VERTEX shader   -> her KOSE icin bir kez calisir. Isi: kosenin ekranda
//                      nereye dusecegini hesaplamak.
//   FRAGMENT shader -> her PIKSEL icin bir kez calisir. Isi: o pikselin
//                      rengini belirlemek.
//
// Ikisi ayri derlenir, sonra tek bir "program" olarak birlestirilir (link).
// Bu sinif o surecin tamamini sarmalar ve hatalari okunur sekilde raporlar.
// ===========================================================================

#include <cstdint>
#include <string>
#include <unordered_map>

#include <glm/glm.hpp>

namespace FX
{
    class Shader
    {
    public:
        // Iki olusturma yolu var:
        //   - Kaynak metinden (test/gomulu shader'lar icin)
        //   - Dosyadan (normal kullanim)
        // Statik fabrika fonksiyonu kullaniyoruz cunku iki yapici da
        // "iki string alir" olurdu ve hangisinin dosya yolu oldugu belirsiz kalirdi.
        Shader(const std::string& name,
               const std::string& vertexSource,
               const std::string& fragmentSource);

        static Shader* FromFiles(const std::string& name,
                                 const std::string& vertexPath,
                                 const std::string& fragmentPath);

        ~Shader();

        // GPU kaynagi sahibi -> kopyalama yasak (bkz. Window'daki ayni gerekce).
        Shader(const Shader&)            = delete;
        Shader& operator=(const Shader&) = delete;

        // Bu shader'i "aktif program" yap. Sonraki cizim cagrilari bunu kullanir.
        void Bind() const;
        void Unbind() const;

        bool IsValid() const { return m_RendererID != 0; }
        const std::string& GetName() const { return m_Name; }

        // --- Uniform'lar ------------------------------------------------------
        // "uniform" = tum kose/pikseller icin AYNI kalan deger.
        // (Kose basina degisenler "attribute", pikseller arasi interpolasyona
        // ugrayanlar "varying/in-out" olarak adlandirilir.)
        // Ornek: nesnenin rengi bir uniform'dur; nesnenin kose pozisyonu degildir.
        void SetInt(const std::string& name, int value);

        // Tamsayi DIZISI. Batch renderer'da sampler2D u_Textures[32]
        // uniform'una [0,1,2,...,31] yazmak icin gerekiyor.
        void SetIntArray(const std::string& name, const int* values, std::uint32_t count);

        void SetFloat(const std::string& name, float value);
        void SetFloat2(const std::string& name, const glm::vec2& value);
        void SetFloat3(const std::string& name, const glm::vec3& value);
        void SetFloat4(const std::string& name, const glm::vec4& value);
        void SetMat4(const std::string& name, const glm::mat4& value);

    private:
        // Uniform'a deger yazmak icin once onun GPU'daki "konumunu" (location)
        // ogrenmek gerekir. glGetUniformLocation her cagrildiginda surucude
        // string karsilastirmasi yapar - her karede yuzlerce kez cagirmak israf.
        // Bu yuzden sonuclari onbellege aliyoruz.
        int GetUniformLocation(const std::string& name);

        // Tek bir shader asamasini derler. Basarisizsa 0 doner ve loglar.
        std::uint32_t CompileStage(std::uint32_t type, const std::string& source,
                                   const char* stageName) const;

        std::uint32_t m_RendererID = 0;   // OpenGL program nesnesinin kimligi
        std::string   m_Name;

        // mutable degil cunku Set* fonksiyonlari zaten const degil.
        std::unordered_map<std::string, int> m_UniformLocationCache;
    };
}
