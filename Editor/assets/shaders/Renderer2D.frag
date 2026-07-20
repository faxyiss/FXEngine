#version 330 core

out vec4 o_Color;

in vec4  v_Color;
in vec2  v_TexCoord;
flat in float v_TexIndex;
in float v_TilingFactor;

// 32 texture ayni anda bagli. Her kose hangisini kullanacagini
// v_TexIndex ile soyluyor.
uniform sampler2D u_Textures[32];

void main()
{
    vec2 uv = v_TexCoord * v_TilingFactor;
    vec4 texColor;

    // ===================================================================
    // NEDEN switch, neden u_Textures[int(v_TexIndex)] DEGIL?
    // ===================================================================
    // GLSL 3.30 spesifikasyonu, sampler dizilerinin yalnizca SABIT bir
    // ifade ile indekslenmesine izin verir. Degisken ile indeksleme
    // (u_Textures[i]) GL 4.0+ ve "dynamically uniform" kosuluyla gecerlidir.
    //
    // Bazi suruculer 3.3'te de kabul eder - ve tam olarak bu yuzden
    // tehlikelidir: senin makinede calisir, baskasinda derlenmez.
    // switch ile her dal SABIT indeks kullanir; her yerde gecerlidir.
    //
    // Cirkin gorunuyor ama tasinabilirligin bedeli bu. GL 4.6 veya
    // bindless texture kullanan modern motorlarda bu blok kaybolur.
    int index = int(v_TexIndex);
    switch (index)
    {
        case  0: texColor = texture(u_Textures[ 0], uv); break;
        case  1: texColor = texture(u_Textures[ 1], uv); break;
        case  2: texColor = texture(u_Textures[ 2], uv); break;
        case  3: texColor = texture(u_Textures[ 3], uv); break;
        case  4: texColor = texture(u_Textures[ 4], uv); break;
        case  5: texColor = texture(u_Textures[ 5], uv); break;
        case  6: texColor = texture(u_Textures[ 6], uv); break;
        case  7: texColor = texture(u_Textures[ 7], uv); break;
        case  8: texColor = texture(u_Textures[ 8], uv); break;
        case  9: texColor = texture(u_Textures[ 9], uv); break;
        case 10: texColor = texture(u_Textures[10], uv); break;
        case 11: texColor = texture(u_Textures[11], uv); break;
        case 12: texColor = texture(u_Textures[12], uv); break;
        case 13: texColor = texture(u_Textures[13], uv); break;
        case 14: texColor = texture(u_Textures[14], uv); break;
        case 15: texColor = texture(u_Textures[15], uv); break;
        case 16: texColor = texture(u_Textures[16], uv); break;
        case 17: texColor = texture(u_Textures[17], uv); break;
        case 18: texColor = texture(u_Textures[18], uv); break;
        case 19: texColor = texture(u_Textures[19], uv); break;
        case 20: texColor = texture(u_Textures[20], uv); break;
        case 21: texColor = texture(u_Textures[21], uv); break;
        case 22: texColor = texture(u_Textures[22], uv); break;
        case 23: texColor = texture(u_Textures[23], uv); break;
        case 24: texColor = texture(u_Textures[24], uv); break;
        case 25: texColor = texture(u_Textures[25], uv); break;
        case 26: texColor = texture(u_Textures[26], uv); break;
        case 27: texColor = texture(u_Textures[27], uv); break;
        case 28: texColor = texture(u_Textures[28], uv); break;
        case 29: texColor = texture(u_Textures[29], uv); break;
        case 30: texColor = texture(u_Textures[30], uv); break;
        case 31: texColor = texture(u_Textures[31], uv); break;
        default: texColor = vec4(1.0, 0.0, 1.0, 1.0); break;  // magenta = hata
    }

    // Slot 0 her zaman 1x1 BEYAZ texture'dir. Beyazla carpmak rengi
    // degistirmez -> duz renkli quad'lar da bu ayni yoldan gecer.
    // Shader'da "texture var mi?" diye dallanmaya gerek kalmaz.
    vec4 result = texColor * v_Color;

    // Tamamen saydam pikselleri at: blending acikken bile bunlar
    // derinlik tamponuna yazilip arkalarindakini gizlerdi.
    if (result.a < 0.01)
        discard;

    o_Color = result;
}
