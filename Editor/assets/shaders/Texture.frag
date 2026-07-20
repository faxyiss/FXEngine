#version 330 core

out vec4 o_Color;

// Vertex shader'dan gelen, INTERPOLE EDILMIS UV koordinati.
// Isim ve tip vertex shader'daki "out" ile birebir ayni olmali,
// yoksa program link edilmez.
in vec2 v_TexCoord;

// sampler2D bir texture'in KENDISI degil, hangi SLOTU okuyacagini
// soyleyen bir tamsayidir. C++ tarafinda SetInt("u_Texture", 0)
// diyerek "0 numarali slotu oku" demis oluyoruz.
uniform sampler2D u_Texture;

// Renk tonu (tint). Beyaz (1,1,1,1) = texture'i oldugu gibi goster.
// Carparak uyguluyoruz: bu sayede tek bir gri sprite'i kirmizi, mavi,
// yesil olarak tekrar tekrar kullanabilirsin - bellek tasarrufu.
uniform vec4 u_Color;

// Doku kaydirma/tekrar carpani. UV'yi olceklersek desen tekrarlanir
// (WrapS/T = Repeat ise). Zemin dosemek icin klasik yontem.
uniform float u_TilingFactor;

void main()
{
    // texture() fonksiyonu: verilen UV'de texture'i orneklar.
    // Filtreleme (Nearest/Linear) ve mipmap secimi burada devreye girer -
    // GPU bunlari otomatik yapar, biz sadece parametrelerini kurmustuk.
    vec4 texColor = texture(u_Texture, v_TexCoord * u_TilingFactor);

    // Alfa kanali 0 olan pikselleri tamamen atiyoruz.
    //
    // NEDEN? Blending acikken saydam pikseller yine de DERINLIK TAMPONUNA
    // yazilir. Bu, arkalarindaki nesnelerin cizilmemesine yol acar -
    // "gorunmez dikdortgen digerlerini kesiyor" seklindeki klasik hata.
    // discard, o pikseli tamamen iptal eder.
    if (texColor.a < 0.01)
        discard;

    o_Color = texColor * u_Color;
}
