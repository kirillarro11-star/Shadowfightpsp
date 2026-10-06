#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspaudio.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

PSP_MODULE_INFO("ShadowFightLite", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define SW 480
#define SH 272
#define BW 512
#define GY 220

static unsigned int __attribute__((aligned(64))) fb[BW * SH];
#define RGB(r,g,b) (0xFF000000u | ((unsigned)(r)<<16) | ((unsigned)(g)<<8) | (unsigned)(b))

/* =========================================================
   ПРИМИТИВЫ
   ========================================================= */
static void fillRect(int x,int y,int w,int h,unsigned int c){
    int x0=x,y0=y,x1=x+w,y1=y+h;
    if(x0<0)x0=0; if(y0<0)y0=0;
    if(x1>SW)x1=SW; if(y1>SH)y1=SH;
    if(x1<=x0||y1<=y0)return;
    for(int yy=y0;yy<y1;yy++){
        unsigned int* r=&fb[yy*BW+x0];
        int n=x1-x0; for(int i=0;i<n;i++) r[i]=c;
    }
}
static void px(int x,int y,unsigned int c){
    if(x<0||x>=SW||y<0||y>=SH)return;
    fb[y*BW+x]=c;
}
static void line(int x0,int y0,int x1,int y1,unsigned int c,int t){
    int dx=abs(x1-x0),dy=-abs(y1-y0);
    int sx=x0<x1?1:-1,sy=y0<y1?1:-1;
    int err=dx+dy,r=t/2;
    while(1){
        for(int oy=-r;oy<=r;oy++)for(int ox=-r;ox<=r;ox++)px(x0+ox,y0+oy,c);
        if(x0==x1&&y0==y1)break;
        int e2=2*err;
        if(e2>=dy){err+=dy;x0+=sx;}
        if(e2<=dx){err+=dx;y0+=sy;}
    }
}
static void circle(int cx,int cy,int r,unsigned int c){
    for(int y=-r;y<=r;y++){
        int w=(int)sqrtf((float)(r*r-y*y));
        for(int x=-w;x<=w;x++)px(cx+x,cy+y,c);
    }
}

/* =========================================================
   ШРИФТ — ЛАТИНИЦА + КИРИЛЛИЦА (UTF-8)
   ========================================================= */
typedef struct { unsigned char ch; unsigned char d[5]; } Glyph;
static const Glyph FONT[]={
    {' ',{0,0,0,0,0}},{'.',{0,0x60,0x60,0,0}},{'-',{0x08,0x08,0x08,0x08,0x08}},
    {':',{0,0x36,0x36,0,0}},{'/',{0x20,0x10,0x08,0x04,0x02}},{'!',{0,0,0x5F,0,0}},
    {'0',{0x3E,0x51,0x49,0x45,0x3E}},{'1',{0,0x42,0x7F,0x40,0}},
    {'2',{0x42,0x61,0x51,0x49,0x46}},{'3',{0x21,0x41,0x45,0x4B,0x31}},
    {'4',{0x18,0x14,0x12,0x7F,0x10}},{'5',{0x27,0x45,0x45,0x45,0x39}},
    {'6',{0x3C,0x4A,0x49,0x49,0x30}},{'7',{0x01,0x71,0x09,0x05,0x03}},
    {'8',{0x36,0x49,0x49,0x49,0x36}},{'9',{0x06,0x49,0x49,0x29,0x1E}},
    {'A',{0x7E,0x11,0x11,0x11,0x7E}},{'B',{0x7F,0x49,0x49,0x49,0x36}},
    {'C',{0x3E,0x41,0x41,0x41,0x22}},{'D',{0x7F,0x41,0x41,0x22,0x1C}},
    {'E',{0x7F,0x49,0x49,0x49,0x41}},{'F',{0x7F,0x09,0x09,0x09,0x01}},
    {'G',{0x3E,0x41,0x49,0x49,0x7A}},{'H',{0x7F,0x08,0x08,0x08,0x7F}},
    {'I',{0,0x41,0x7F,0x41,0}},{'J',{0x20,0x40,0x41,0x3F,0x01}},
    {'K',{0x7F,0x08,0x14,0x22,0x41}},{'L',{0x7F,0x40,0x40,0x40,0x40}},
    {'M',{0x7F,0x02,0x0C,0x02,0x7F}},{'N',{0x7F,0x04,0x08,0x10,0x7F}},
    {'O',{0x3E,0x41,0x41,0x41,0x3E}},{'P',{0x7F,0x09,0x09,0x09,0x06}},
    {'Q',{0x3E,0x41,0x51,0x21,0x5E}},{'R',{0x7F,0x09,0x19,0x29,0x46}},
    {'S',{0x46,0x49,0x49,0x49,0x31}},{'T',{0x01,0x01,0x7F,0x01,0x01}},
    {'U',{0x3F,0x40,0x40,0x40,0x3F}},{'V',{0x1F,0x20,0x40,0x20,0x1F}},
    {'W',{0x7F,0x20,0x18,0x20,0x7F}},{'X',{0x63,0x14,0x08,0x14,0x63}},
    {'Y',{0x03,0x04,0x78,0x04,0x03}},{'Z',{0x61,0x51,0x49,0x45,0x43}},
    {',',{0,0x50,0x30,0,0}},
};

typedef struct { unsigned char b1,b2; unsigned char d[5]; } CyrGlyph;
static const CyrGlyph CYR[]={
    {0xD0,0x90,{0x7E,0x11,0x11,0x11,0x7E}}, // А
    {0xD0,0x91,{0x7F,0x49,0x49,0x49,0x31}}, // Б
    {0xD0,0x92,{0x7F,0x49,0x49,0x49,0x36}}, // В
    {0xD0,0x93,{0x7F,0x01,0x01,0x01,0x01}}, // Г
    {0xD0,0x94,{0x7E,0x21,0x21,0x21,0x7F}}, // Д
    {0xD0,0x95,{0x7F,0x49,0x49,0x49,0x41}}, // Е
    {0xD0,0x81,{0x7F,0x4B,0x49,0x4B,0x41}}, // Ё
    {0xD0,0x96,{0x63,0x14,0x7F,0x14,0x63}}, // Ж
    {0xD0,0x97,{0x22,0x41,0x49,0x49,0x36}}, // З
    {0xD0,0x98,{0x7F,0x10,0x08,0x04,0x7F}}, // И
    {0xD0,0x99,{0x7F,0x10,0x0A,0x05,0x7F}}, // Й
    {0xD0,0x9A,{0x7F,0x08,0x14,0x22,0x41}}, // К
    {0xD0,0x9B,{0x3F,0x40,0x40,0x40,0x7F}}, // Л
    {0xD0,0x9C,{0x7F,0x02,0x0C,0x02,0x7F}}, // М
    {0xD0,0x9D,{0x7F,0x08,0x08,0x08,0x7F}}, // Н
    {0xD0,0x9E,{0x3E,0x41,0x41,0x41,0x3E}}, // О
    {0xD0,0x9F,{0x7F,0x01,0x01,0x01,0x7F}}, // П
    {0xD0,0xA0,{0x7F,0x09,0x09,0x09,0x06}}, // Р
    {0xD0,0xA1,{0x3E,0x41,0x41,0x41,0x22}}, // С
    {0xD0,0xA2,{0x01,0x01,0x7F,0x01,0x01}}, // Т
    {0xD0,0xA3,{0x27,0x48,0x48,0x48,0x3F}}, // У
    {0xD0,0xA4,{0x1C,0x22,0x7F,0x22,0x1C}}, // Ф
    {0xD0,0xA5,{0x63,0x14,0x08,0x14,0x63}}, // Х
    {0xD0,0xA6,{0x7F,0x40,0x40,0x40,0x6F}}, // Ц
    {0xD0,0xA7,{0x07,0x08,0x08,0x08,0x7F}}, // Ч
    {0xD0,0xA8,{0x7F,0x40,0x7F,0x40,0x7F}}, // Ш
    {0xD0,0xA9,{0x7F,0x40,0x7F,0x40,0x6F}}, // Щ
    {0xD0,0xAA,{0x01,0x3F,0x48,0x48,0x30}}, // Ъ
    {0xD0,0xAB,{0x7F,0x48,0x30,0,0x7F}},    // Ы
    {0xD0,0xAC,{0x7F,0x48,0x48,0x48,0x30}}, // Ь
    {0xD0,0xAD,{0x22,0x41,0x49,0x49,0x3E}}, // Э
    {0xD0,0xAE,{0x7F,0x08,0x3E,0x41,0x3E}}, // Ю
    {0xD0,0xAF,{0x46,0x29,0x19,0x09,0x7F}}, // Я
    // строчные
    {0xD0,0xB0,{0x20,0x54,0x54,0x54,0x78}}, // а
    {0xD0,0xB1,{0x7F,0x48,0x44,0x44,0x38}}, // б
    {0xD0,0xB2,{0x38,0x54,0x54,0x54,0x28}}, // в
    {0xD0,0xB3,{0x7C,0x04,0x04,0x04,0x04}}, // г
    {0xD0,0xB4,{0x38,0x44,0x44,0x44,0x7C}}, // д
    {0xD0,0xB5,{0x38,0x54,0x54,0x54,0x18}}, // е
    {0xD1,0x91,{0x38,0x56,0x54,0x56,0x18}}, // ё
    {0xD0,0xB6,{0x44,0x28,0x7C,0x28,0x44}}, // ж
    {0xD0,0xB7,{0x48,0x54,0x54,0x54,0x24}}, // з
    {0xD0,0xB8,{0x7C,0x20,0x10,0x08,0x7C}}, // и
    {0xD0,0xB9,{0x7C,0x20,0x14,0x0A,0x7C}}, // й
    {0xD0,0xBA,{0x7C,0x10,0x28,0x44,0x00}}, // к
    {0xD0,0xBB,{0x3C,0x40,0x40,0x40,0x7C}}, // л
    {0xD0,0xBC,{0x7C,0x08,0x10,0x08,0x7C}}, // м
    {0xD0,0xBD,{0x7C,0x10,0x10,0x10,0x7C}}, // н
    {0xD0,0xBE,{0x38,0x44,0x44,0x44,0x38}}, // о
    {0xD0,0xBF,{0x7C,0x04,0x04,0x04,0x7C}}, // п
    {0xD1,0x80,{0x7C,0x14,0x14,0x14,0x08}}, // р
    {0xD1,0x81,{0x38,0x44,0x44,0x44,0x20}}, // с
    {0xD1,0x82,{0x04,0x04,0x7C,0x04,0x04}}, // т
    {0xD1,0x83,{0x0C,0x50,0x50,0x50,0x3C}}, // у
    {0xD1,0x84,{0x18,0x24,0x7C,0x24,0x18}}, // ф
    {0xD1,0x85,{0x44,0x28,0x10,0x28,0x44}}, // х
    {0xD1,0x86,{0x7C,0x40,0x40,0x40,0x7C}}, // ц
    {0xD1,0x87,{0x0C,0x10,0x10,0x10,0x7C}}, // ч
    {0xD1,0x88,{0x7C,0x40,0x7C,0x40,0x7C}}, // ш
    {0xD1,0x89,{0x7C,0x40,0x7C,0x40,0x6C}}, // щ
    {0xD1,0x8A,{0x04,0x3C,0x50,0x50,0x20}}, // ъ
    {0xD1,0x8B,{0x7C,0x50,0x20,0x00,0x7C}}, // ы
    {0xD1,0x8C,{0x7C,0x50,0x50,0x50,0x20}}, // ь
    {0xD1,0x8D,{0x28,0x44,0x54,0x54,0x38}}, // э
    {0xD1,0x8E,{0x7C,0x10,0x38,0x44,0x38}}, // ю
    {0xD1,0x8F,{0x4C,0x34,0x14,0x14,0x7C}}, // я
};

static const unsigned char* lookupGlyph(const char* t, int* bytes){
    unsigned char b1 = (unsigned char)t[0];
    if (b1 < 0x80){
        *bytes = 1;
        for (unsigned int i=0;i<sizeof(FONT)/sizeof(FONT[0]);i++)
            if (FONT[i].ch == b1) return FONT[i].d;
        return FONT[0].d;
    }
    if (b1 == 0xD0 || b1 == 0xD1){
        unsigned char b2 = (unsigned char)t[1];
        *bytes = 2;
        for (unsigned int i=0;i<sizeof(CYR)/sizeof(CYR[0]);i++)
            if (CYR[i].b1==b1 && CYR[i].b2==b2) return CYR[i].d;
        return FONT[0].d;
    }
    *bytes = 1;
    return FONT[0].d;
}

static void drawChar(int x,int y,const unsigned char* g,unsigned int col,int s){
    for(int c=0;c<5;c++){
        unsigned char b=g[c];
        for(int r=0;r<7;r++) if(b&(1<<r)) fillRect(x+c*s,y+r*s,s,s,col);
    }
}
static void drawText(int x,int y,const char* t,unsigned int col,int s){
    int cx=x;
    while(*t){
        if(*t=='\n'){cx=x;y+=8*s;t++;continue;}
        int bytes;
        const unsigned char* g = lookupGlyph(t,&bytes);
        drawChar(cx,y,g,col,s);
        cx += 6*s;
        t += bytes;
    }
}
static int textW(const char* t,int s){
    int w=0,mx=0;
    while(*t){
        if(*t=='\n'){if(w>mx)mx=w;w=0;t++;continue;}
        int bytes;
        lookupGlyph(t,&bytes);
        w += 6*s;
        t += bytes;
    }
    return w>mx?w:mx;
}
static void drawTextC(int y,const char* t,unsigned int col,int s){
    int w=textW(t,s);
    drawText((SW-w)/2,y,t,col,s);
}

/* =========================================================
   ЗВУК — синтез через sceAudio
   ========================================================= */
#define AUDIO_SAMPLES 1024
static short gAudioBuf[AUDIO_SAMPLES*2];
static int gAudioReady = 0;

typedef struct {
    int freq;         // 0 = noise
    int duration;     // ms remaining
    int maxDuration;
    int type;         // 0=sine, 1=square, 2=noise
    int vol;          // 0-100
} SoundSrc;
#define MAX_SND 6
static SoundSrc gSnd[MAX_SND];
static int gMusicTimer = 0;
static int gMusicNote = 0;
static int gMusicType = 0;    // 0 = off, 1 = menu, 2 = battle

static void audioInit(void){
    if (sceAudioSRCChReserve(AUDIO_SAMPLES,44100,2) < 0) return;
    gAudioReady = 1;
}
static void sndPlay(int freq,int dur,int type,int vol){
    if (!gAudioReady) return;
    for (int i=0;i<MAX_SND;i++){
        if (gSnd[i].duration <= 0){
            gSnd[i].freq=freq; gSnd[i].duration=dur;
            gSnd[i].maxDuration=dur; gSnd[i].type=type; gSnd[i].vol=vol;
            return;
        }
    }
}
static void audioTick(void){
    if (!gAudioReady) return;
    for (int i=0;i<AUDIO_SAMPLES;i++){
        int sample = 0;
        for (int s=0;s<MAX_SND;s++){
            if (gSnd[s].duration <= 0) continue;
            float env = (float)gSnd[s].duration / (float)gSnd[s].maxDuration;
            int sVal = 0;
            if (gSnd[s].type == 2){
                sVal = (rand() & 0xFFFF) - 0x8000;
            } else {
                int period = 44100 / (gSnd[s].freq > 0 ? gSnd[s].freq : 1);
                if (period < 2) period = 2;
                int phase = (i + s*77) % period;
                if (gSnd[s].type == 0)
                    sVal = (phase < period/2) ? 0x4000 : -0x4000;
                else
                    sVal = (phase < period/2) ? 0x5000 : -0x5000;
            }
            sample += (int)(sVal * env * gSnd[s].vol / 100.0f / 2);
        }
        if (sample > 32767) sample = 32767;
        if (sample < -32768) sample = -32768;
        gAudioBuf[i*2] = sample;
        gAudioBuf[i*2+1] = sample;
    }
    sceAudioSRCOutputBlocking(0x8000, gAudioBuf);
    for (int s=0;s<MAX_SND;s++)
        if (gSnd[s].duration > 0)
            gSnd[s].duration -= (AUDIO_SAMPLES * 1000 / 44100);
}
/* SFX обёртки — как в игре */
static void sfxHit(void){ sndPlay(180,90,2,70); sndPlay(80,140,0,50); }
static void sfxHeavy(void){ sndPlay(120,180,2,80); sndPlay(55,220,0,60); }
static void sfxBlock(void){ sndPlay(900,70,0,50); }
static void sfxWhoosh(void){ sndPlay(300,60,2,25); }
static void sfxSelect(void){ sndPlay(660,60,1,40); sndPlay(880,80,1,35); }
static void sfxWin(void){
    sndPlay(440,140,1,50); sndPlay(554,140,1,50);
    sndPlay(659,280,1,55);
}
static void sfxLose(void){
    sndPlay(330,180,1,50); sndPlay(220,320,1,50);
}

static void musicTick(int ms){
    if (!gAudioReady) return;
    gMusicTimer -= ms;
    if (gMusicTimer > 0) return;
    if (gMusicType == 1){ /* меню — низкий дрон */
        int scale[] = {110, 130, 146, 164, 196, 220, 261};
        sndPlay(scale[gMusicNote % 7], 1500, 0, 25);
        if (gMusicNote % 3 == 0) sndPlay(55, 2200, 0, 20);
        gMusicTimer = 1400;
        gMusicNote++;
    } else if (gMusicType == 2){ /* бой — ритм */
        int beat = gMusicNote % 8;
        if (beat % 2 == 0) sndPlay(60, 120, 0, 60);       // кик
        if (beat == 2 || beat == 6) sndPlay(0, 80, 2, 40); // снейр
        if (beat % 4 == 0) sndPlay(220, 150, 0, 15);
        if (beat == 0 || beat == 5) sndPlay(330, 200, 0, 12);
        gMusicTimer = 240;
        gMusicNote++;
    } else {
        gMusicTimer = 100;
    }
}
static void musicSet(int type){ gMusicType = type; gMusicTimer = 0; gMusicNote = 0; }

/* =========================================================
   ИГРОВЫЕ ДАННЫЕ
   ========================================================= */
typedef struct {
    const char* name;
    int type;      // 0 knife 1 fist 2 sai 3 baton 4 sword 5 machete 6 dagger 7 sting 8 scythe 9 tonfa 10 nunchaku
    int dmg,range;
    float dur,hs,he,cd;
} WeaponDef;
static const WeaponDef WEAPONS[11]={
    {"НОЖИ",        0, 6,64,0.24f,0.05f,0.14f,0.24f},
    {"КАСТЕТЫ",     1, 5,54,0.20f,0.04f,0.11f,0.19f},
    {"САИ",         2, 7,66,0.28f,0.06f,0.16f,0.30f},
    {"СТ.ДУБИНКИ",  3, 9,70,0.36f,0.09f,0.20f,0.42f},
    {"МЕЧ НИНДЗЯ",  4,10,80,0.32f,0.08f,0.18f,0.38f},
    {"МАЧЕТЕ",      5,12,74,0.40f,0.11f,0.22f,0.48f},
    {"КИНЖАЛЫ",     6, 8,60,0.22f,0.05f,0.13f,0.24f},
    {"ЖАЛО",        7, 9,72,0.26f,0.06f,0.15f,0.28f},
    {"КРОВ.ЖНЕЦ",   8,14,88,0.45f,0.12f,0.26f,0.55f},
    {"ЯР.ТОНФЫ",    9, 8,66,0.26f,0.06f,0.15f,0.28f},
    {"НУНЧАКИ",    10, 7,74,0.30f,0.08f,0.17f,0.32f},
};

typedef struct {
    const char* name;
    int hp,dmg,range,wt;
    float speed,cd;
    unsigned int color;
    int boss;
} EnemyDef;
static const EnemyDef ENEMIES[6]={
    {"ШИН",     60, 6,64,0,130.0f,0.95f,RGB(124,124,144),0},
    {"КИРПИЧ",  85, 9,70,3,140.0f,0.90f,RGB(141,122,104),0},
    {"ИГЛА",    80, 8,66,2,170.0f,0.75f,RGB(160,110,200),0},
    {"ПРИЗРАК",100,10,80,4,175.0f,0.72f,RGB(108,168,216),0},
    {"ЩЕГОЛЬ", 120,11,74,5,190.0f,0.65f,RGB(216,168,80),0},
    {"РЫСЬ",   180,14,88,8,210.0f,0.58f,RGB(217,144,60),1},
};

typedef struct {
    const char* name;
    unsigned int skyTop,skyMid,skyBot,wall,ground,accent;
} LocDef;
static const LocDef LOCS[3]={
    {"ЗАБРОШЕННЫЙ ДВОР",RGB(10,13,26),RGB(26,16,48),RGB(44,16,56),RGB(20,12,34),RGB(12,7,20),RGB(123,63,255)},
    {"НОЧНАЯ КРЫША",    RGB(5,8,21),  RGB(15,26,53),RGB(30,20,64),RGB(10,10,24),RGB(6,8,16), RGB(79,163,255)},
    {"ДРЕВНЯЯ ПЕЩЕРА",  RGB(21,8,8),  RGB(42,16,8), RGB(51,26,8), RGB(26,10,5), RGB(16,7,5), RGB(255,139,58)},
};

typedef struct { const char* name; int done; } MoveDef;
static MoveDef MOVES[7]={
    {"ПРЯМОЙ УДАР",0},{"ПИНОК",0},{"БЛОК",0},
    {"ПРЫЖОК",0},{"ДВОЙНОЙ УДАР",0},{"ТРОЙНОЙ УДАР",0},{"УДАР В ПРЫЖКЕ",0},
};

/* Диалоги */
typedef struct {
    const char* speaker;
    const char* lines[5];
    int count;
} Dialog;
static const Dialog DIALOGS[6]={
    {"ШИН",{ "Кто ты такой? Я не трачу время на червей.", "Рысь поручил охранять этот зал. Проходи — если сможешь." },2},
    {"КИРПИЧ",{ "Ха! Ещё один глупец, решивший бросить вызов Рыси!", "Я раздавлю тебя, как букашку!" },2},
    {"ИГЛА",{ "Ты не должен был приходить сюда.", "Орден превыше всего, и я докажу это, избавившись от тебя!" },2},
    {"ПРИЗРАК",{ "Ты не первый, кто бросил вызов Рыси,", "и не последний, кто падёт от моего клинка.", "Хозяин ждёт тебя." },3},
    {"ЩЕГОЛЬ",{ "Наконец-то достойный противник!", "Покажи мне, на что ты способен. Ну же, не разочаруй меня!" },2},
    {"РЫСЬ",{ "Кто ты такой? Я не буду тратить своё время на жалкого червя!", "Это Шин, слабейший ученик моего Ордена. Победи его... если сможешь.", "Тогда, возможно, ты заслужишь право сразиться со мной." },3},
};

/* =========================================================
   СОСТОЯНИЕ
   ========================================================= */
typedef struct {
    float x,y,vx,vy;
    int hp,maxHp;
    int facing,onGround,blocking,hitDone;
    float attackT,cdT,stunT,hurtT;
    unsigned int color;
    int isPlayer,isBoss;
    int wt,dmg,range;
    float dur,hs,he,maxCd,speed;
    float aiT;
    int aiMode;
    int cfgIdx;
} Fighter;
typedef struct { float x,y; float swing,swingV; float hitFlash; } Bag;

#define ST_MENU      0
#define ST_WEAPONS   1
#define ST_CAMPAIGN  2
#define ST_TRAINING  3
#define ST_LEARN     4
#define ST_FIGHT     5
#define ST_END       6
#define ST_INVENTORY 7
#define ST_DIALOG    8

static int gState=ST_MENU, gMode=0;
static int gWeapon=0, gEnemyIdx=0, gLocIdx=0;
static int gUnlockedEnemies=1;
static int gUnlockedMoves[7]={1,0,0,0,0,0,0};
static int gCurrentMoveId=-1;
static Fighter gPlayer,gEnemy;
static Bag gBag;
static int gBagActive=0;
static float gMsgT=0; static char gMsg[128];
static float gShakeT=0;
static int gTransition=0, gPlayerWon=0;
static int gComboCount=0; static float gComboT=0;
static int gDialogIdx=0, gDialogLine=0;
static int gReturnTimer=0; // 0 = нет, >0 = тикает
static int gInvFromState=ST_MENU;

static int gWasCross=0,gWasLeft=0,gWasRight=0,gWasUp=0,gWasDown=0;
static int gWasCircle=0,gWasSquare=0,gWasSelect=0,gWasL=0;
static unsigned int gLastUs=0;

static float dt(void){
    unsigned int now=sceKernelGetSystemTimeLow();
    float d=(now-gLastUs)/1000000.0f;
    gLastUs=now;
    if(d>0.05f)d=0.05f;
    if(d<0)d=0;
    return d;
}
static void setMsg(const char* t,float time){
    strncpy(gMsg,t,sizeof(gMsg)-1); gMsg[sizeof(gMsg)-1]=0; gMsgT=time;
}

/* =========================================================
   ИНИЦИАЛИЗАЦИЯ БОЙЦОВ
   ========================================================= */
static void initPlayer(void){
    memset(&gPlayer,0,sizeof(gPlayer));
    const WeaponDef* w=&WEAPONS[gWeapon];
    gPlayer.isPlayer=1;
    gPlayer.hp=gPlayer.maxHp=100;
    gPlayer.x=SW*0.25f; gPlayer.y=GY;
    gPlayer.facing=1; gPlayer.onGround=1;
    gPlayer.color=RGB(26,10,42);
    gPlayer.wt=w->type; gPlayer.dmg=w->dmg; gPlayer.range=w->range;
    gPlayer.dur=w->dur; gPlayer.hs=w->hs; gPlayer.he=w->he;
    gPlayer.maxCd=w->cd;
}
static void initEnemy(int idx){
    memset(&gEnemy,0,sizeof(gEnemy));
    const EnemyDef* e=&ENEMIES[idx];
    gEnemy.isPlayer=0;
    gEnemy.hp=gEnemy.maxHp=e->hp;
    gEnemy.x=SW*0.75f; gEnemy.y=GY;
    gEnemy.facing=-1; gEnemy.onGround=1;
    gEnemy.color=e->color;
    gEnemy.isBoss=e->boss;
    gEnemy.wt=e->wt; gEnemy.dmg=e->dmg; gEnemy.range=e->range;
    gEnemy.dur=0.32f; gEnemy.hs=0.09f; gEnemy.he=0.20f;
    gEnemy.maxCd=e->cd;
    gEnemy.speed=e->speed;
    gEnemy.cfgIdx=idx;
}
static void initBag(void){
    memset(&gBag,0,sizeof(gBag));
    gBag.x=SW*0.7f; gBag.y=GY; gBagActive=1;
}
static void startBattle(int idx){
    gEnemyIdx=idx; initPlayer(); initEnemy(idx);
    gBagActive=0; gTransition=0; gComboCount=0; gComboT=0;
    gState=ST_FIGHT; gMode=0;
    char b[64];
    snprintf(b,sizeof(b),"РАУНД %d\n%s",idx+1,ENEMIES[idx].name);
    setMsg(b,1.6f);
    musicSet(2);
    sndPlay(440,150,1,50);
}
static void startTraining(int loc){
    gLocIdx=loc; initPlayer(); initBag();
    gTransition=0; gComboCount=0; gComboT=0;
    gState=ST_FIGHT; gMode=1;
    setMsg(LOCS[loc].name,1.4f);
    musicSet(2);
}
static void startLearn(int moveId){
    gCurrentMoveId=moveId; initPlayer(); initBag();
    gTransition=0; gComboCount=0; gComboT=0;
    gState=ST_FIGHT; gMode=2;
    setMsg(MOVES[moveId].name,1.4f);
    musicSet(2);
}
static void startDialog(int idx){
    gDialogIdx=idx; gDialogLine=0;
    // Если диалога нет — сразу бой
    if (DIALOGS[idx].count == 0){ startBattle(idx); return; }
    gState=ST_DIALOG;
    musicSet(1);
}

/* =========================================================
   БОЙ
   ========================================================= */
static void physics(Fighter* f,float d){
    f->vy+=1800.0f*d;
    f->x+=f->vx*d; f->y+=f->vy*d;
    if(f->y>=GY){f->y=GY;f->vy=0;f->onGround=1;}
    else f->onGround=0;
    if(f->x<28){f->x=28;if(f->vx<0)f->vx=0;}
    if(f->x>SW-28){f->x=SW-28;if(f->vx>0)f->vx=0;}
    if(f->onGround && f->stunT<=0) f->vx*=0.85f;
}
static void checkHit(Fighter* a,Fighter* b){
    if(a->attackT<=0||a->hitDone)return;
    float el=a->dur-a->attackT;
    if(el<a->hs||el>a->he)return;
    float dx=b->x-a->x;
    if(fabsf(dx)>a->range)return;
    if((dx>0?1:-1)!=a->facing && fabsf(dx)>8)return;
    if(fabsf(a->y-b->y)>80)return;
    a->hitDone=1;
    float dmg=(float)a->dmg;
    if(b->blocking){dmg*=0.22f;b->vx=a->facing*80;b->stunT=0.10f; sfxBlock();}
    else{b->vx=a->facing*190;b->stunT=0.26f;b->hurtT=0.22f;
         if(dmg>=10) sfxHeavy(); else sfxHit();}
    b->hp-=(int)dmg; if(b->hp<0)b->hp=0;
    gShakeT=0.14f; gComboCount++; gComboT=1.6f;
}
static void checkHitBag(Fighter* a){
    if(!gBagActive||a->attackT<=0||a->hitDone)return;
    float el=a->dur-a->attackT;
    if(el<a->hs||el>a->he)return;
    float dx=gBag.x-a->x;
    if(fabsf(dx)>a->range)return;
    if((dx>0?1:-1)!=a->facing && fabsf(dx)>8)return;
    if(fabsf(a->y-gBag.y)>120)return;
    a->hitDone=1;
    gBag.swingV+=a->facing*3.5f;
    gBag.hitFlash=1.0f;
    gComboCount++; gComboT=1.6f;
    sfxHit();
}
static void enemyAI(float d){
    Fighter* e=&gEnemy; Fighter* p=&gPlayer;
    if(e->stunT>0)return;
    float dx=p->x-e->x, dist=fabsf(dx);
    int dir=dx>0?1:-1;
    e->aiT-=d;
    if(e->aiT<=0){
        e->aiT=e->isBoss?(0.18f+(rand()%25)/100.0f):(0.28f+(rand()%40)/100.0f);
        if(dist<=e->range*0.92f && e->cdT<=0 && (rand()%100)<(e->isBoss?88:72))
            e->aiMode=2;
        else if(dist>e->range*1.05f) e->aiMode=0;
        else { int r=rand()%100;
            if(r<22)e->aiMode=1; else if(r<45)e->aiMode=3; else e->aiMode=0; }
    }
    e->blocking=0;
    if(e->aiMode==0){ e->vx=dir*e->speed; }
    else if(e->aiMode==2){
        e->vx*=0.7f;
        if(e->cdT<=0 && dist<=e->range){
            e->attackT=e->dur; e->cdT=e->maxCd; e->hitDone=0; sfxWhoosh();
        }
    }
    else if(e->aiMode==1){ e->blocking=1; e->vx=-dir*40; }
    else e->vx*=0.85f;
}
static void update(float d){
    if(gState!=ST_FIGHT)return;
    Fighter* p=&gPlayer;

    if(gBagActive){ p->facing=(gBag.x>=p->x)?1:-1; }
    else {
        Fighter* e=&gEnemy;
        if(p->attackT<=0&&p->stunT<=0) p->facing=(e->x>=p->x)?1:-1;
        if(e->attackT<=0&&e->stunT<=0) e->facing=(p->x>=e->x)?1:-1;
    }
    p->cdT-=d; if(p->cdT<0)p->cdT=0;
    p->stunT-=d; if(p->stunT<0)p->stunT=0;
    p->hurtT-=d; if(p->hurtT<0)p->hurtT=0;
    if(p->attackT>0){p->attackT-=d; if(p->attackT<0)p->attackT=0;}

    if(p->stunT>0){p->vx*=0.9f;}
    else if(p->attackT>0){p->vx*=0.84f;}
    else if(gWasSquare){p->vx*=0.7f; p->blocking=1;}
    else {
        p->blocking=0;
        float ax=0;
        if(gWasLeft)ax-=1; if(gWasRight)ax+=1;
        p->vx=ax*280;
    }
    if(!gBagActive){
        Fighter* e=&gEnemy;
        e->cdT-=d; if(e->cdT<0)e->cdT=0;
        e->stunT-=d; if(e->stunT<0)e->stunT=0;
        e->hurtT-=d; if(e->hurtT<0)e->hurtT=0;
        if(e->attackT>0){e->attackT-=d; if(e->attackT<0)e->attackT=0;}
        enemyAI(d); physics(e,d);
    }
    physics(p,d);
    if(gBagActive){
        gBag.swingV*=0.94f;
        gBag.swing+=gBag.swingV*d; gBag.swing*=0.985f;
        if(gBag.swing>0.9f)gBag.swing=0.9f;
        if(gBag.swing<-0.9f)gBag.swing=-0.9f;
        gBag.hitFlash-=d*3; if(gBag.hitFlash<0)gBag.hitFlash=0;
        checkHitBag(p);
    } else {
        checkHit(p,&gEnemy); checkHit(&gEnemy,p);
    }
    if(gShakeT>0)gShakeT-=d;
    if(gMsgT>0)gMsgT-=d;
    if(gComboT>0){gComboT-=d; if(gComboT<=0)gComboCount=0;}

    // Проверка обучения
    if(gMode==2 && gCurrentMoveId>=0 && !gUnlockedMoves[gCurrentMoveId]){
        if(gCurrentMoveId==0 && p->attackT>0) gUnlockedMoves[0]=1;
        if(gCurrentMoveId==1 && p->attackT>0 && gWasSquare) gUnlockedMoves[1]=1;
        if(gCurrentMoveId==3 && !p->onGround) gUnlockedMoves[3]=1;
        if(gCurrentMoveId==6 && !p->onGround && p->attackT>0) gUnlockedMoves[6]=1;
        if(gUnlockedMoves[gCurrentMoveId]){
            setMsg("ИЗУЧЕНО!",1.2f); sfxWin();
            gReturnTimer = 1500;
        }
    }

    // Конец боя
    if(!gTransition && !gBagActive){
        if(gEnemy.hp<=0){
            gTransition=1;
            if(gEnemyIdx<5){
                if(gUnlockedEnemies<gEnemyIdx+2) gUnlockedEnemies=gEnemyIdx+2;
                setMsg("ПОБЕДА!",1.2f);
                sfxWin();
                gReturnTimer = 1500;
            } else {
                setMsg("АКТ I ПРОЙДЕН",1.4f);
                gPlayerWon=1; sfxWin();
                gReturnTimer = 1800;
            }
        } else if(p->hp<=0){
            gTransition=1;
            setMsg("ПОРАЖЕНИЕ",1.4f); sfxLose();
            gReturnTimer = 1500;
        }
    }

    // Возврат в меню после победы
    if(gReturnTimer>0){
        gReturnTimer -= (int)(d*1000);
        if(gReturnTimer<=0){
            if(gMode==0){ gState=ST_CAMPAIGN; }
            else if(gMode==2){ gState=ST_LEARN; }
            else { gState=ST_TRAINING; }
            musicSet(1);
            gTransition=0;
        }
    }
}

/* =========================================================
   ОТРИСОВКА
   ========================================================= */
static void drawWeapon(int wt,float hx,float hy){
    if(wt==0||wt==6||wt==7){
        line((int)hx,(int)hy,(int)hx+18,(int)hy-6,RGB(215,227,234),4);
        line((int)hx-5,(int)hy,(int)hx+4,(int)hy,RGB(90,74,58),5);
    } else if(wt==1){
        circle((int)hx,(int)hy,6,RGB(255,183,77));
        circle((int)hx,(int)hy,3,RGB(255,143,0));
    } else if(wt==2){
        line((int)hx-3,(int)hy,(int)hx+16,(int)hy,RGB(200,216,224),3);
        line((int)hx+13,(int)hy-5,(int)hx+13,(int)hy+3,RGB(200,216,224),2);
    } else if(wt==3){
        line((int)hx-4,(int)hy,(int)hx+26,(int)hy,RGB(90,74,58),6);
        circle((int)hx+24,(int)hy,4,RGB(42,42,42));
    } else if(wt==4){
        line((int)hx-2,(int)hy,(int)hx+36,(int)hy,RGB(208,220,232),4);
        line((int)hx+34,(int)hy-3,(int)hx+46,(int)hy,RGB(208,220,232),3);
        line((int)hx-8,(int)hy,(int)hx+2,(int)hy,RGB(122,90,58),5);
    } else if(wt==5){
        line((int)hx,(int)hy,(int)hx+38,(int)hy-3,RGB(200,208,216),5);
        line((int)hx-6,(int)hy,(int)hx+2,(int)hy,RGB(90,74,58),6);
    } else if(wt==8){
        line((int)hx-8,(int)hy,(int)hx+6,(int)hy-2,RGB(141,110,99),4);
        for(int a=-40;a<=30;a+=6){
            float r=13.0f, rad=a*3.14159f/180.0f;
            px((int)(hx+10+cosf(rad)*r),(int)(hy-6+sinf(rad)*r),RGB(230,238,245));
            px((int)(hx+11+cosf(rad)*r),(int)(hy-6+sinf(rad)*r),RGB(230,238,245));
        }
    } else if(wt==9){
        line((int)hx-3,(int)hy,(int)hx+20,(int)hy,RGB(138,106,74),4);
        line((int)hx+8,(int)hy,(int)hx+8,(int)hy+10,RGB(138,106,74),4);
    } else if(wt==10){
        line((int)hx-6,(int)hy,(int)hx+8,(int)hy,RGB(58,42,26),4);
        line((int)hx+8,(int)hy,(int)hx+18,(int)hy+5,RGB(136,136,136),1);
        line((int)hx+18,(int)hy+5,(int)hx+30,(int)hy+2,RGB(58,42,26),4);
    }
}
static void drawFighter(Fighter* f){
    if(!f)return;
    int dir=f->facing;
    unsigned int col=f->isPlayer?RGB(26,10,42):(f->hurtT>0?RGB(255,91,91):f->color);
    float bx=f->x, by=f->y;

    for(int i=-22;i<=22;i++) px((int)bx+i,(int)by+2,RGB(20,20,20));

    if(f->isPlayer){
        for(int i=-30;i<=30;i++) for(int j=-50;j<=50;j++){
            float d=sqrtf((float)(i*i+j*j));
            if(d<32 && d>26) px((int)bx+i,(int)(by-60+j),RGB(60,20,100));
        }
    }
    if(f->isBoss){
        for(int i=-30;i<=30;i++) for(int j=-50;j<=50;j++){
            float d=sqrtf((float)(i*i+j*j));
            if(d<32 && d>26) px((int)bx+i,(int)(by-60+j),RGB(120,50,10));
        }
    }

    float lx0=bx-5, ly0=by-32, lx1=bx-10, ly1=by-1;
    float rx0=bx+5, ly2=by-32, rx1=bx+10, ry1=by-1;
    if(!f->onGround){ lx1=bx-14; rx1=bx+14; }
    line((int)lx0,(int)ly0,(int)lx1,(int)ly1,col,6);
    line((int)rx0,(int)ly2,(int)rx1,(int)ry1,col,6);
    line((int)bx,(int)(by-60),(int)bx,(int)(by-30),col,14);
    circle((int)bx,(int)(by-72),9,col);

    unsigned int eyeCol = f->isPlayer?RGB(185,140,255):(f->isBoss?RGB(255,204,51):RGB(255,136,153));
    px((int)bx-3,(int)(by-74),eyeCol); px((int)bx-2,(int)(by-74),eyeCol);
    px((int)bx+3,(int)(by-74),eyeCol); px((int)bx+2,(int)(by-74),eyeCol);

    float frontX=bx+11, frontY=by-42;
    float backX=bx-10, backY=by-44;
    if(f->attackT>0){
        float prog=1.0f-(f->attackT/f->dur);
        float swing=sinf(prog*3.14159f);
        if(swing<0)swing=0; if(swing>1)swing=1;
        frontX=bx+9+(f->range*0.75f)*swing*dir;
        frontY=by-52+8*(1-swing);
    } else if(f->blocking){
        frontX=bx+12*dir; frontY=by-55;
        backX=bx+4*dir; backY=by-52;
    }
    line((int)bx,(int)(by-58),(int)backX,(int)backY,col,5);
    line((int)bx,(int)(by-58),(int)frontX,(int)frontY,col,6);
    drawWeapon(f->wt,frontX,frontY);
    if(f->wt==0||f->wt==6) drawWeapon(f->wt,backX,backY);
    if(f->hurtT>0) circle((int)bx,(int)(by-50),22,RGB(255,255,255));
}
static void drawBag(void){
    if(!gBagActive)return;
    float bx=gBag.x, by=gBag.y;
    for(int i=-20;i<=20;i++) px((int)bx+i,(int)by+2,RGB(20,20,20));
    line((int)bx,(int)(by-160),(int)bx,(int)(by-130+gBag.swing*20),RGB(100,100,100),2);
    float off=gBag.swing*30;
    float cy=by-100;
    unsigned int body = gBag.hitFlash>0?RGB(255,85,102):RGB(90,42,42);
    unsigned int edge = gBag.hitFlash>0?RGB(255,170,136):RGB(138,58,58);
    for(int j=-70;j<=70;j++){
        int w=(int)(28*(1-fabsf(j)/80.0f));
        if(w<2)w=2;
        for(int i=-w;i<=w;i++)
            px((int)(bx+off*(j+70)/140)+i,(int)(cy+j),body);
    }
    line((int)(bx+off),(int)(cy-70),(int)(bx+off),(int)(cy+70),edge,2);
    line((int)(bx-28),(int)cy,(int)(bx+28),(int)cy,edge,2);
    circle((int)(bx+off),(int)(cy-70),6,RGB(58,26,26));
}
static void drawBackground(void){
    const LocDef* loc;
    LocDef battleLoc = {"",RGB(10,13,26),RGB(26,16,48),RGB(44,16,56),RGB(20,12,34),RGB(12,7,20),RGB(123,63,255)};
    if(gMode==1||gMode==2) loc=&LOCS[gLocIdx];
    else loc=&battleLoc;

    for(int y=0;y<GY;y++){
        float t=(float)y/GY;
        unsigned int c;
        if(t<0.5f){
            float u=t*2.0f;
            int r=(int)((loc->skyTop>>16&0xFF)*(1-u)+(loc->skyMid>>16&0xFF)*u);
            int g=(int)((loc->skyTop>>8&0xFF)*(1-u)+(loc->skyMid>>8&0xFF)*u);
            int b=(int)((loc->skyTop&0xFF)*(1-u)+(loc->skyMid&0xFF)*u);
            c=RGB(r,g,b);
        } else {
            float u=(t-0.5f)*2.0f;
            int r=(int)((loc->skyMid>>16&0xFF)*(1-u)+(loc->skyBot>>16&0xFF)*u);
            int g=(int)((loc->skyMid>>8&0xFF)*(1-u)+(loc->skyBot>>8&0xFF)*u);
            int b=(int)((loc->skyMid&0xFF)*(1-u)+(loc->skyBot&0xFF)*u);
            c=RGB(r,g,b);
        }
        unsigned int* row=&fb[y*BW];
        for(int x=0;x<SW;x++) row[x]=c;
    }
    int mx=SW-100,my=45;
    for(int y=-40;y<=40;y++)for(int x=-40;x<=40;x++){
        float d2=(float)(x*x+y*y);
        if(d2<1600.0f){
            float a=1.0f-sqrtf(d2)/40.0f;
            if(a<0)a=0;
            int base=fb[(my+y)*BW+mx+x]&0xFFFFFF;
            int br=(base>>16)&0xFF,bg=(base>>8)&0xFF,bb=base&0xFF;
            int nr=br+(int)((255-br)*a*0.35f);
            int ng=bg+(int)((235-bg)*a*0.35f);
            int nb=bb+(int)((190-bb)*a*0.35f);
            fb[(my+y)*BW+mx+x]=RGB(nr,ng,nb);
        }
    }
    circle(mx,my,20,RGB(255,242,205));
    int hts[]={60,40,55,30,50,45,58,38};
    int step=SW/7;
    for(int i=0;i<8;i++){
        int x0=i*step-step/2;
        int top=GY-hts[i];
        fillRect(x0,top,step+2,GY-top,loc->wall);
    }
    fillRect(0,GY-3,SW,3,loc->accent);
    fillRect(0,GY,SW,SH-GY,loc->ground);
}

/* HUD */
static void drawBar(int x,int y,int w,int h,float ratio,unsigned int c,int flip){
    if(ratio<0)ratio=0; if(ratio>1)ratio=1;
    fillRect(x-1,y-1,w+2,h+2,RGB(0,0,0));
    fillRect(x,y,w,h,RGB(40,20,60));
    int fw=(int)(w*ratio);
    if(flip) fillRect(x+(w-fw),y,fw,h,c);
    else     fillRect(x,y,fw,h,c);
}
static void drawHUD(void){
    if(gState!=ST_FIGHT)return;
    int bw=170,bh=10,top=12;
    drawBar(14,top,bw,bh,(float)gPlayer.hp/gPlayer.maxHp,RGB(79,195,247),0);
    if(!gBagActive){
        drawBar(SW-14-bw,top,bw,bh,(float)gEnemy.hp/gEnemy.maxHp,
            gEnemy.isBoss?RGB(255,68,68):RGB(255,82,82),1);
        drawText(14,top+bh+4,"ВЫ",RGB(160,140,210),1);
        int w=textW(ENEMIES[gEnemyIdx].name,1);
        drawText(SW-14-w,top+bh+4,ENEMIES[gEnemyIdx].name,RGB(160,140,210),1);
    }
    if(gComboCount>1){
        char b[16]; snprintf(b,sizeof(b),"x%d",gComboCount);
        int w=textW(b,2);
        drawText((SW-w)/2,60,b,RGB(255,200,60),2);
    }
    if(gMsgT>0){
        drawTextC(SH*0.32f+2,gMsg,RGB(0,0,0),2);
        drawTextC(SH*0.32f,gMsg,RGB(240,230,255),2);
    }
    if(gMode==2&&gCurrentMoveId>=0)
        drawTextC(SH-30,MOVES[gCurrentMoveId].name,RGB(200,180,255),1);
}

/* Фоновый градиент для экранов */
static void drawGradBg(unsigned int t,unsigned int m,unsigned int b){
    for(int y=0;y<SH;y++){
        float u=(float)y/SH;
        unsigned int c;
        if(u<0.5f){
            float k=u*2;
            int r=(int)((t>>16&0xFF)*(1-k)+(m>>16&0xFF)*k);
            int g=(int)((t>>8&0xFF)*(1-k)+(m>>8&0xFF)*k);
            int bb=(int)((t&0xFF)*(1-k)+(m&0xFF)*k);
            c=RGB(r,g,bb);
        } else {
            float k=(u-0.5f)*2;
            int r=(int)((m>>16&0xFF)*(1-k)+(b>>16&0xFF)*k);
            int g=(int)((m>>8&0xFF)*(1-k)+(b&0xFF)*k);
            int bb=(int)((m&0xFF)*(1-k)+(b&0xFF)*k);
            c=RGB(r,g,bb);
        }
        unsigned int* row=&fb[y*BW];
        for(int x=0;x<SW;x++) row[x]=c;
    }
}

/* Экраны */
static void drawMenu(void){
    drawGradBg(RGB(10,7,20),RGB(27,18,48),RGB(7,7,12));
    drawTextC(28,"SHADOW FIGHT",RGB(232,213,255),4);
    drawTextC(70,"LITE",RGB(232,213,255),4);
    drawTextC(108,"АКТ I — ПЕРЕРОЖДЕНИЕ",RGB(160,140,210),1);
    drawTextC(150,"X - КАМПАНИЯ",RGB(240,230,255),1);
    drawTextC(168,"O - ТРЕНИРОВКА",RGB(240,230,255),1);
    drawTextC(186,"КВАДРАТ - ОБУЧЕНИЕ",RGB(240,230,255),1);
    drawTextC(204,"ТРЕУГОЛЬНИК - АРСЕНАЛ",RGB(240,230,255),1);
    drawTextC(232,"SELECT - ВЫХОД В МЕНЮ ИЗ ЛЮБОГО ЭКРАНА",RGB(140,130,170),1);
}
static void drawWeapons(void){
    drawGradBg(RGB(10,7,20),RGB(27,18,48),RGB(7,7,12));
    drawTextC(6,"АРСЕНАЛ",RGB(232,213,255),2);
    for(int i=0;i<11;i++){
        int col=i%2, row=i/2;
        int x=20+col*(SW/2-10);
        int y=28+row*22;
        unsigned int c=(i==gWeapon)?RGB(160,110,255):RGB(35,25,55);
        fillRect(x,y,SW/2-30,20,c);
        drawText(x+4,y+6,WEAPONS[i].name,RGB(255,255,255),1);
    }
    drawTextC(SH-16,"ВВЕРХ/ВНИЗ - ВЫБОР   X - ВЗЯТЬ",RGB(160,140,210),1);
}
static void drawCampaign(void){
    drawGradBg(RGB(10,7,20),RGB(27,18,48),RGB(7,7,12));
    drawTextC(6,"АКТ I — ВЫБОР ПРОТИВНИКА",RGB(232,213,255),2);
    for(int i=0;i<6;i++){
        int y=28+i*22;
        int unlocked=i<gUnlockedEnemies;
        int done=i<gUnlockedEnemies-1;
        unsigned int c=done?RGB(30,30,40):
            (ENEMIES[i].boss?RGB(80,20,20):(unlocked?RGB(50,40,80):RGB(20,15,30)));
        fillRect(20,y,SW-40,20,c);
        if(unlocked){
            if(done) drawText(24,y+6,"[V]",RGB(100,255,100),1);
            else     drawText(24,y+6,"[ ]",RGB(200,200,200),1);
            drawText(46,y+6,ENEMIES[i].name,RGB(255,255,255),1);
            char hp[16]; snprintf(hp,sizeof(hp),"HP %d",ENEMIES[i].hp);
            drawText(SW-90,y+6,hp,RGB(200,180,220),1);
            if(ENEMIES[i].boss) drawText(160,y+6,"БОСС",RGB(255,180,80),1);
        } else {
            drawText(24,y+6,"[X]",RGB(100,90,120),1);
            drawText(46,y+6,"ЗАБЛОКИРОВАНО",RGB(100,90,120),1);
        }
    }
    drawTextC(SH-30,"X - НАЧАТЬ   L - ИНВЕНТАРЬ   SELECT - МЕНЮ",RGB(160,140,210),1);
}
static void drawTraining(void){
    drawGradBg(RGB(10,7,20),RGB(27,18,48),RGB(7,7,12));
    drawTextC(18,"ТРЕНИРОВКА",RGB(232,213,255),3);
    drawTextC(56,"ВЫБЕРИ ЛОКАЦИЮ",RGB(160,140,210),1);
    for(int i=0;i<3;i++){
        int y=76+i*40;
        unsigned int c=(i==gLocIdx)?RGB(160,110,255):RGB(50,40,80);
        fillRect(40,y,SW-80,32,c);
        drawText(50,y+10,LOCS[i].name,RGB(255,255,255),2);
    }
    drawTextC(SH-16,"ВВЕРХ/ВНИЗ   X - НАЧАТЬ",RGB(160,140,210),1);
}
static void drawLearn(void){
    drawGradBg(RGB(10,7,20),RGB(27,18,48),RGB(7,7,12));
    drawTextC(6,"ОБУЧЕНИЕ ПРИЁМАМ",RGB(232,213,255),2);
    for(int i=0;i<7;i++){
        int y=28+i*22;
        int unlocked=(i==0)||gUnlockedMoves[i-1];
        int done=gUnlockedMoves[i];
        unsigned int c=done?RGB(30,60,30):(unlocked?RGB(50,40,80):RGB(20,15,30));
        fillRect(20,y,SW-40,20,c);
        char b[40];
        snprintf(b,sizeof(b),"%s %s",done?"[V]":"[ ]",MOVES[i].name);
        drawText(28,y+6,b,unlocked?RGB(255,255,255):RGB(100,90,120),1);
    }
    drawTextC(SH-16,"X - ИЗУЧИТЬ   O - НАЗАД",RGB(160,140,210),1);
}
static void drawInventory(void){
    drawGradBg(RGB(5,3,10),RGB(20,10,35),RGB(5,3,10));
    drawTextC(6,"ИНВЕНТАРЬ",RGB(232,213,255),2);
    drawTextC(26,"ОРУЖИЕ АКТА I",RGB(160,140,210),1);
    for(int i=0;i<11;i++){
        int col=i%2, row=i/2;
        int x=20+col*(SW/2-10);
        int y=44+row*22;
        unsigned int c=(i==gWeapon)?RGB(160,110,255):RGB(35,25,55);
        fillRect(x,y,SW/2-30,20,c);
        drawText(x+4,y+6,WEAPONS[i].name,RGB(255,255,255),1);
    }
    const WeaponDef* w=&WEAPONS[gWeapon];
    char buf[64];
    snprintf(buf,sizeof(buf),"УРОН: %d  ДИСТ: %d",w->dmg,w->range);
    drawTextC(SH-44,buf,RGB(255,200,60),1);
    drawTextC(SH-26,"X - ВЗЯТЬ   L/O - ЗАКРЫТЬ",RGB(160,140,210),1);
}
static void drawDialogScreen(void){
    drawGradBg(RGB(3,2,8),RGB(20,8,30),RGB(3,2,8));
    const Dialog* d=&DIALOGS[gDialogIdx];
    // Портрет — силуэт говорящего
    Fighter tmp;
    memset(&tmp,0,sizeof(tmp));
    tmp.x = SW*0.2f; tmp.y = GY-40; tmp.facing=1;
    tmp.color = ENEMIES[gDialogIdx].color;
    tmp.isBoss = ENEMIES[gDialogIdx].boss;
    tmp.wt = ENEMIES[gDialogIdx].wt;
    tmp.attackT = 0; tmp.blocking = 0; tmp.hurtT = 0;
    tmp.onGround = 1; tmp.isPlayer = 0;
    drawFighter(&tmp);

    // Имя и текст
    drawText(160, 30, d->speaker, RGB(255,200,80), 2);
    // Разделитель
    fillRect(160, 50, SW-180, 2, RGB(160,110,255));

    const char* line = d->lines[gDialogLine];
    int y = 70;
    int startX = 170;
    // Перенос по словам (простой)
    char buf[256];
    strncpy(buf,line,sizeof(buf)-1); buf[sizeof(buf)-1]=0;
    // Рисуем построчно — по ~30 символов
    int cx = startX;
    int cy = y;
    char word[64];
    int wi = 0;
    for(int i=0; buf[i]; i++){
        char ch = buf[i];
        if(ch==' '){
            word[wi]=0;
            int wpx = textW(word,1);
            if(cx + wpx > SW-20){
                cx = startX; cy += 12;
            }
            drawText(cx,cy,word,RGB(240,230,255),1);
            cx += wpx + 6;
            wi = 0;
        } else if(wi<60){ word[wi++]=ch; }
    }
    if(wi>0){
        word[wi]=0;
        int wpx = textW(word,1);
        if(cx + wpx > SW-20){ cx=startX; cy+=12; }
        drawText(cx,cy,word,RGB(240,230,255),1);
    }

    // Подсказка
    drawTextC(SH-20,"НАЖМИ X ПРОДОЛЖИТЬ",RGB(255,200,60),1);
}
static void drawEnd(void){
    drawGradBg(RGB(10,7,20),RGB(27,18,48),RGB(7,7,12));
    if(gPlayerWon){
        drawTextC(70,"АКТ I ПРОЙДЕН",RGB(232,213,255),4);
        drawTextC(140,"РЫСЬ ПОВЕРЖЕН",RGB(240,230,255),2);
    } else {
        drawTextC(70,"ПОРАЖЕНИЕ",RGB(255,120,120),4);
        char b[64]; snprintf(b,sizeof(b),"ТЕБЯ ОДОЛЕЛ: %s",ENEMIES[gEnemyIdx].name);
        drawTextC(140,b,RGB(200,180,220),1);
    }
    drawTextC(SH-30,"X - В МЕНЮ",RGB(240,230,255),1);
}

/* =========================================================
   ВВОД
   ========================================================= */
static void readInput(void){
    SceCtrlData pad;
    sceCtrlReadBufferPositive(&pad,1);
    int l  = (pad.Buttons & PSP_CTRL_LEFT)   != 0;
    int r  = (pad.Buttons & PSP_CTRL_RIGHT)  != 0;
    int u  = (pad.Buttons & PSP_CTRL_UP)     != 0;
    int d  = (pad.Buttons & PSP_CTRL_DOWN)   != 0;
    int x  = (pad.Buttons & PSP_CTRL_CROSS)  != 0;
    int o  = (pad.Buttons & PSP_CTRL_CIRCLE) != 0;
    int sq = (pad.Buttons & PSP_CTRL_SQUARE) != 0;
    int tr = (pad.Buttons & PSP_CTRL_TRIANGLE) != 0;
    int sel= (pad.Buttons & PSP_CTRL_SELECT) != 0;
    int Lb = (pad.Buttons & PSP_CTRL_LTRIGGER) != 0;

    /* SELECT — выход в меню */
    if (sel && !gWasSelect){
        if (gState != ST_MENU){
            gState = ST_MENU; gMode = 0; gBagActive = 0;
            gTransition = 0; gReturnTimer = 0; gMsgT = 0;
            musicSet(1); sfxSelect();
            gWasCross=gWasCircle=gWasSquare=1;
            gWasLeft=gWasRight=gWasUp=gWasDown=1;
            gWasSelect=1; gWasL=1;
            return;
        }
    }
    /* L — инвентарь */
    if (Lb && !gWasL){
        if (gState == ST_MENU || gState == ST_CAMPAIGN || gState == ST_FIGHT){
            gInvFromState = gState;
            gState = ST_INVENTORY;
        } else if (gState == ST_INVENTORY){
            gState = gInvFromState;
        }
    }

    if(gState==ST_MENU){
        if(x&&!gWasCross){ gState=ST_CAMPAIGN; sfxSelect(); }
        else if(o&&!gWasCircle){ gState=ST_TRAINING; sfxSelect(); }
        else if(sq&&!gWasSquare){ gState=ST_LEARN; sfxSelect(); }
        else if(tr&&!gWasCross){ gState=ST_WEAPONS; sfxSelect(); }
    }
    else if(gState==ST_WEAPONS){
        if(d&&!gWasDown) gWeapon=(gWeapon+1)%11;
        if(u&&!gWasUp)   gWeapon=(gWeapon+10)%11;
        if((x&&!gWasCross)||(o&&!gWasCircle)){ gState=ST_MENU; sfxSelect(); }
    }
    else if(gState==ST_CAMPAIGN){
        if(x&&!gWasCross) startDialog(gUnlockedEnemies-1);
        if(o&&!gWasCircle) gState=ST_MENU;
    }
    else if(gState==ST_INVENTORY){
        if(d&&!gWasDown) gWeapon=(gWeapon+1)%11;
        if(u&&!gWasUp)   gWeapon=(gWeapon+10)%11;
        if(x&&!gWasCross){
            if(gInvFromState==ST_FIGHT) initPlayer();
            sfxSelect();
        }
        if((o&&!gWasCircle)||(Lb&&!gWasL)) gState=gInvFromState;
    }
    else if(gState==ST_TRAINING){
        if(d&&!gWasDown) gLocIdx=(gLocIdx+1)%3;
        if(u&&!gWasUp)   gLocIdx=(gLocIdx+2)%3;
        if(x&&!gWasCross) startTraining(gLocIdx);
        if(o&&!gWasCircle) gState=ST_MENU;
    }
    else if(gState==ST_LEARN){
        if(x&&!gWasCross){
            for(int i=0;i<7;i++){
                if(!gUnlockedMoves[i] && (i==0||gUnlockedMoves[i-1])){
                    startLearn(i); break;
                }
            }
        }
        if(o&&!gWasCircle) gState=ST_MENU;
    }
    else if(gState==ST_DIALOG){
        if(x&&!gWasCross){
            gDialogLine++;
            sfxSelect();
            if(gDialogLine >= DIALOGS[gDialogIdx].count){
                startBattle(gDialogIdx);
            }
        }
    }
    else if(gState==ST_FIGHT){
        gWasLeft=l; gWasRight=r; gWasSquare=sq;
        if(o&&!gWasUp){
            if(gPlayer.onGround&&gPlayer.stunT<=0&&gPlayer.attackT<=0){
                gPlayer.vy=-600; gPlayer.onGround=0;
                sndPlay(420,80,0,40);
            }
        }
        if(x&&!gWasCross){
            if(gPlayer.cdT<=0&&gPlayer.attackT<=0&&gPlayer.stunT<=0){
                gPlayer.attackT=gPlayer.dur;
                gPlayer.cdT=gPlayer.maxCd;
                gPlayer.hitDone=0; gPlayer.blocking=0;
                sfxWhoosh();
            }
        }
    }
    else if(gState==ST_END){
        if(x&&!gWasCross) gState=ST_MENU;
    }

    gWasLeft=l; gWasRight=r; gWasUp=u; gWasDown=d;
    gWasCross=x; gWasCircle=o; gWasSquare=sq;
    gWasSelect=sel; gWasL=Lb;
    (void)tr;
}

/* =========================================================
   MAIN
   ========================================================= */
int main(void){
    sceDisplaySetMode(0,SW,SH);
    sceDisplaySetFrameBuf((void*)fb,BW,PSP_DISPLAY_PIXEL_FORMAT_8888,
                          PSP_DISPLAY_SETBUF_IMMEDIATE);
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    audioInit();
    gLastUs=sceKernelGetSystemTimeLow();
    musicSet(1);

    while(1){
        sceDisplayWaitVblankStart();
        float d=dt();
        readInput();
        update(d);
        musicTick((int)(d*1000));
        audioTick();

        if(gState==ST_MENU)           drawMenu();
        else if(gState==ST_WEAPONS)   drawWeapons();
        else if(gState==ST_CAMPAIGN)  drawCampaign();
        else if(gState==ST_TRAINING)  drawTraining();
        else if(gState==ST_LEARN)     drawLearn();
        else if(gState==ST_INVENTORY) drawInventory();
        else if(gState==ST_DIALOG)    drawDialogScreen();
        else if(gState==ST_END)       drawEnd();
        else if(gState==ST_FIGHT){
            drawBackground();
            if(gBagActive) drawBag();
            else drawFighter(&gEnemy);
            drawFighter(&gPlayer);
            drawHUD();
        }
    }
    return 0;
}
