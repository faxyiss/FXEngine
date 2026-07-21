#include "FXEngine/Renderer/Shader.h"
#include "FXEngine/Core/Log.h"
#include "FXEngine/Core/FileSystem.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <sstream>
#include <vector>

namespace FX
{
    namespace
    {
        // Dosyayi tek seferde string'e okur. Basarisizsa bos string doner.
        std::string ReadFile(const std::string& path, bool& outOk)
        {
            // std::ios::binary: Windows'ta metin modunda "\r\n" -> "\n"
            // donusumu yapilir. Shader kaynagi icin sorun cikarmaz ama
            // dosya boyutu ile okunan miktar uyusmaz; binary okumak daha
            // ongorulebilir.
            std::ifstream file(path, std::ios::in | std::ios::binary);
            if (!file)
            {
                FX_CORE_ERROR("Shader dosyasi acilamadi: %s", path.c_str());
                outOk = false;
                return {};
            }

            std::ostringstream ss;
            ss << file.rdbuf();      // tum akisi tek hamlede kopyala
            outOk = true;
            return ss.str();
        }
    }

    Shader::Shader(const std::string& name,
                   const std::string& vertexSource,
                   const std::string& fragmentSource)
        : m_Name(name)
    {
        // --- 1) Iki asamayi ayri ayri derle ---------------------------------
        const std::uint32_t vs = CompileStage(GL_VERTEX_SHADER,   vertexSource,   "vertex");
        const std::uint32_t fs = CompileStage(GL_FRAGMENT_SHADER, fragmentSource, "fragment");

        if (vs == 0 || fs == 0)
        {
            // Birini derleyebilip digerini derleyemediysek, derleneni de sil.
            // Yoksa GPU belleginde sahipsiz bir nesne kalir (kaynak sizintisi).
            if (vs) glDeleteShader(vs);
            if (fs) glDeleteShader(fs);
            FX_CORE_ERROR("Shader '%s' olusturulamadi.", m_Name.c_str());
            return;
        }

        // --- 2) Program olustur ve asamalari bagla --------------------------
        m_RendererID = glCreateProgram();
        glAttachShader(m_RendererID, vs);
        glAttachShader(m_RendererID, fs);

        // Link: iki asamayi tek calistirilabilir programa birlestirir.
        // Burada tipik hata: vertex shader'in "out" degiskeni ile fragment
        // shader'in "in" degiskeni isim/tip olarak uyusmuyorsa link patlar.
        glLinkProgram(m_RendererID);

        GLint linked = GL_FALSE;
        glGetProgramiv(m_RendererID, GL_LINK_STATUS, &linked);
        if (linked == GL_FALSE)
        {
            GLint logLength = 0;
            glGetProgramiv(m_RendererID, GL_INFO_LOG_LENGTH, &logLength);
            std::vector<char> log(static_cast<std::size_t>(logLength) + 1, '\0');
            glGetProgramInfoLog(m_RendererID, logLength, nullptr, log.data());

            FX_CORE_ERROR("Shader '%s' link edilemedi:\n%s", m_Name.c_str(), log.data());

            glDeleteProgram(m_RendererID);
            m_RendererID = 0;
            glDeleteShader(vs);
            glDeleteShader(fs);
            return;
        }

        // --- 3) Temizlik ------------------------------------------------------
        // Link bittikten sonra ayri asama nesnelerine ihtiyac kalmaz;
        // program onlarin derlenmis halini zaten icine aldi.
        // Detach + Delete yapmazsak GPU'da gereksiz nesneler birikir.
        glDetachShader(m_RendererID, vs);
        glDetachShader(m_RendererID, fs);
        glDeleteShader(vs);
        glDeleteShader(fs);

        FX_CORE_INFO("Shader '%s' hazir (program id=%u).", m_Name.c_str(), m_RendererID);
    }

    std::unique_ptr<Shader> Shader::FromFiles(const std::string& name,
                                              const std::string& vertexPath,
                                              const std::string& fragmentPath)
    {
        // Yollari EXE KLASORUNE gore cozuyoruz, calisma dizinine gore degil.
        // Boylece program nereden baslatilirsa baslatilsin shader'lari bulur.
        const std::string vFull = FileSystem::ResolveEngineAsset(vertexPath);
        const std::string fFull = FileSystem::ResolveEngineAsset(fragmentPath);

        bool okV = false, okF = false;
        const std::string vSrc = ReadFile(vFull, okV);
        const std::string fSrc = ReadFile(fFull, okF);

        if (!okV || !okF)
        {
            FX_CORE_ERROR("Shader '%s' dosyalardan yuklenemedi.", name.c_str());
            return nullptr;
        }

        // make_unique degil: yapici public ama argumanlarin sirasi
        // burada dogrulanmis kaynak metinler, yol degil.
        return std::unique_ptr<Shader>(new Shader(name, vSrc, fSrc));
    }

    Shader::~Shader()
    {
        if (m_RendererID)
            glDeleteProgram(m_RendererID);
    }

    std::uint32_t Shader::CompileStage(std::uint32_t type, const std::string& source,
                                       const char* stageName) const
    {
        const std::uint32_t id = glCreateShader(type);

        // glShaderSource char** ister (birden fazla kaynak parcasi
        // birlestirebilsin diye). Biz tek parca veriyoruz.
        const char* src = source.c_str();
        glShaderSource(id, 1, &src, nullptr);
        glCompileShader(id);

        GLint compiled = GL_FALSE;
        glGetShaderiv(id, GL_COMPILE_STATUS, &compiled);
        if (compiled == GL_FALSE)
        {
            // HATA MESAJINI OKUMAK SART. Bu blogu yazmazsak shader hatasi
            // sessizce gecer, ekran siyah kalir ve sebebini asla bulamazsin.
            // Shader hata ayiklamanin %90'i bu birkac satirdir.
            GLint logLength = 0;
            glGetShaderiv(id, GL_INFO_LOG_LENGTH, &logLength);
            std::vector<char> log(static_cast<std::size_t>(logLength) + 1, '\0');
            glGetShaderInfoLog(id, logLength, nullptr, log.data());

            FX_CORE_ERROR("Shader '%s' %s asamasi derlenemedi:\n%s",
                          m_Name.c_str(), stageName, log.data());

            glDeleteShader(id);
            return 0;
        }

        return id;
    }

    void Shader::Bind() const
    {
        glUseProgram(m_RendererID);
    }

    void Shader::Unbind() const
    {
        // 0 = "hicbir program" . Genelde gerekmez ama hata ayiklamada
        // "yanlislikla onceki shader'la ciziyorum" durumunu ortaya cikarir.
        glUseProgram(0);
    }

    int Shader::GetUniformLocation(const std::string& name)
    {
        // Onbellekte var mi?
        const auto it = m_UniformLocationCache.find(name);
        if (it != m_UniformLocationCache.end())
            return it->second;

        const int location = glGetUniformLocation(m_RendererID, name.c_str());

        if (location == -1)
        {
            // -1 iki sebepten gelebilir ve IKISI DE cok sik yasanir:
            //   1) Ismi yanlis yazdin
            //   2) Uniform shader'da tanimli ama HIC KULLANILMIYOR -> derleyici
            //      onu optimize edip atmis. Kod dogru gorunur ama uniform yoktur.
            // Bu yuzden uyari veriyoruz ama programi durdurmuyoruz.
            FX_CORE_WARN("Shader '%s': '%s' uniform'u bulunamadi "
                         "(ismi yanlis olabilir ya da shader'da kullanilmadigi icin atilmis).",
                         m_Name.c_str(), name.c_str());
        }

        // -1'i de onbellege atiyoruz ki her karede tekrar sorup uyari basmayalim.
        m_UniformLocationCache[name] = location;
        return location;
    }

    void Shader::SetInt(const std::string& name, int value)
    {
        glUniform1i(GetUniformLocation(name), value);
    }

    void Shader::SetIntArray(const std::string& name, const int* values, std::uint32_t count)
    {
        glUniform1iv(GetUniformLocation(name), static_cast<GLsizei>(count), values);
    }

    void Shader::SetFloat(const std::string& name, float value)
    {
        glUniform1f(GetUniformLocation(name), value);
    }

    void Shader::SetFloat2(const std::string& name, const glm::vec2& value)
    {
        glUniform2f(GetUniformLocation(name), value.x, value.y);
    }

    void Shader::SetFloat3(const std::string& name, const glm::vec3& value)
    {
        glUniform3f(GetUniformLocation(name), value.x, value.y, value.z);
    }

    void Shader::SetFloat4(const std::string& name, const glm::vec4& value)
    {
        glUniform4f(GetUniformLocation(name), value.x, value.y, value.z, value.w);
    }

    void Shader::SetMat4(const std::string& name, const glm::mat4& value)
    {
        // glm::value_ptr, matrisin ic dizisinin adresini verir.
        // GL_FALSE = "transpoze etme". glm zaten OpenGL'in bekledigi
        // sutun-oncelikli (column-major) duzende saklar.
        glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
    }
}
