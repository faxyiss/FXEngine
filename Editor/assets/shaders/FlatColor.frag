#version 330 core

// ===========================================================================
// FRAGMENT SHADER - her PIKSEL icin bir kez calisir.
// Isi: o pikselin rengini belirlemek.
//
// "Fragment" ile "piksel" tam ayni sey degil: fragment, bir pikseli
// olusturmaya ADAY veridir. Derinlik testi veya blending sonucunda
// ekrana hic ulasmayabilir. Ama pratikte ikisi ayni anlamda kullanilir.
// ===========================================================================

// Bu shader'in ciktisi: ekrana yazilacak renk.
// GL 3.3'te "out vec4" ile kendimiz tanimliyoruz (eski gl_FragColor
// kullanimdan kalkti). Isim serbest.
out vec4 o_Color;

// Tum pikseller icin ayni renk -> uniform.
// "Tek renkli quad" tam olarak bu demek.
uniform vec4 u_Color;

void main()
{
    o_Color = u_Color;
}
