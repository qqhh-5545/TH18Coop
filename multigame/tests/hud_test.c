/* Render the real HUD into a mock D3D command sink. No window or GPU. */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <d3d9.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "coop_api.h"
static unsigned draw_calls,captures,applies,releases,checks;
static uintptr_t bound_texture;
static FILE *trace;
static HudStats fixture;
static unsigned personal_icons;
static unsigned native_cards,native_flushes,partial_icons;
static unsigned resource_faces,resource_outlines;
#define CHECK(e) do {if(!(e)) {fprintf(stderr,"HUD FAIL line %d: %s\n",__LINE__,#e);exit(1);}++checks;} while(0)
static HRESULT viewport(D3DVIEWPORT9 *v) {*v=(D3DVIEWPORT9){0,0,1280,960,0,1};return S_OK;}
static HRESULT stateblock(IDirect3DStateBlock9 **s) {*s=(IDirect3DStateBlock9 *)1;return S_OK;}
static HRESULT capture(void) {++captures;return S_OK;}
static HRESULT apply(void) {CHECK(draw_calls>0);++applies;return S_OK;}
static ULONG release(void) {++releases;return 0;}
static HRESULT draw(D3DPRIMITIVETYPE type,UINT n,const void *data,UINT stride);
#undef IDirect3DDevice9_GetViewport
#undef IDirect3DDevice9_CreateStateBlock
#undef IDirect3DStateBlock9_Capture
#undef IDirect3DStateBlock9_Apply
#undef IDirect3DStateBlock9_Release
#undef IDirect3DDevice9_SetVertexShader
#undef IDirect3DDevice9_SetPixelShader
#undef IDirect3DDevice9_SetFVF
#undef IDirect3DDevice9_SetTexture
#undef IDirect3DDevice9_SetTextureStageState
#undef IDirect3DDevice9_SetRenderState
#undef IDirect3DDevice9_SetSamplerState
#undef IDirect3DDevice9_DrawPrimitiveUP
#define IDirect3DDevice9_GetViewport(d,v) viewport(v)
#define IDirect3DDevice9_CreateStateBlock(d,t,s) stateblock(s)
#define IDirect3DStateBlock9_Capture(s) capture()
#define IDirect3DStateBlock9_Apply(s) apply()
#define IDirect3DStateBlock9_Release(s) release()
#define IDirect3DDevice9_SetVertexShader(d,s) ((void)0)
#define IDirect3DDevice9_SetPixelShader(d,s) ((void)0)
#define IDirect3DDevice9_SetFVF(d,s) ((void)0)
#define IDirect3DDevice9_SetTexture(d,s,t) (bound_texture=(uintptr_t)(t))
#define IDirect3DDevice9_SetTextureStageState(d,s,k,v) ((void)0)
#define IDirect3DDevice9_SetRenderState(d,k,v) ((void)0)
#define IDirect3DDevice9_SetSamplerState(d,s,k,v) ((void)0)
#define IDirect3DDevice9_DrawPrimitiveUP(d,t,n,p,s) draw(t,n,p,s)
#include "../src/hud.c"
void coop_hud_stats(HudStats *stats) {*stats=fixture;}
int coop_view_player(void) {return fixture.local_player;}
int coop_shared_keyboard(void) {return fixture.shared_keyboard;}
unsigned coop_hud_card_quads(const HudCard *card,float x,float y,float w,float h,HudNativeQuad *out,unsigned capacity) {
    CHECK(captures==applies+1 && applies==releases);
    CHECK(x>=432 && x+w<=630 && y>=86 && y+h<=470);
    CHECK(fabsf(w/h-.8f)<.001f);CHECK(card->category>=0 && card->category<=2 && capacity>=1);
    ++native_cards;*out=(HudNativeQuad){.x={x,x+w,x,x+w},.y={y,y,y+h,y+h},.u={0,1,0,1},.v={0,0,1,1},.color=0xffffffff,.file=31,.sprite=card->id};return 1;
}
void coop_hud_card_flush(void) {CHECK(captures==applies);++native_flushes;}
int coop_hud_capture_path(wchar_t *path,unsigned length) {(void)path;(void)length;return 0;}
static HRESULT draw(D3DPRIMITIVETYPE type,UINT n,const void *data,UINT stride) {
    CHECK(captures>applies);CHECK(type==D3DPT_TRIANGLELIST && n==2 && stride==sizeof(Vertex));
    const Vertex *v=data;
    if(v[0].color==0xff800000 || v[0].color==0xff808000) {
        unsigned glyph=(unsigned)(bound_texture&0xffff);
        CHECK((glyph>=13 && glyph<=22) || glyph==24 || glyph==27);
        float h=(v[2].y-v[0].y)/2;
        CHECK(fabsf(h-8.75f)<.01f || fabsf(h-5.25f)<.01f);++resource_faces;
    }
    if(v[0].color==0x80ffd0d0 || v[0].color==0x80ffffd0) {
        unsigned glyph=(unsigned)(bound_texture&0xffff);
        CHECK(glyph>=254 || glyph<=12);++resource_outlines;
    }
    if(bound_texture==0x10000+17 || bound_texture==0x10000+18 || bound_texture==0x10000+23 || bound_texture==0x10000+24) {
        CHECK(v[0].color==0xff8080ff);++partial_icons;
    }
    if(fabsf((v[0].x+.5f)/2-34)<.01f && fabsf((v[0].y+.5f)/2-432)<.01f && fabsf((v[1].x-v[0].x)/2-24)<.01f) {
        ++personal_icons;
    }
    for(unsigned i=0;i<6;++i) {
        CHECK(isfinite(v[i].x) && isfinite(v[i].y) && isfinite(v[i].u) && isfinite(v[i].v));
        CHECK(v[i].x>=-.5f && v[i].x<=1279.5f && v[i].y>=-.5f && v[i].y<=959.5f);
    }
    CHECK(v[1].x>=v[0].x && v[2].y>=v[0].y);
    if(trace) fprintf(trace,"%lu %.3f %.3f %.3f %.3f %lu %.5f %.5f %.5f %.5f\n",
        (unsigned long)bound_texture,(v[0].x+.5f)/2,(v[0].y+.5f)/2,
        (v[1].x-v[0].x)/2,(v[2].y-v[0].y)/2,(unsigned long)v[0].color,v[0].u,v[0].v,v[1].u,v[2].v);
    ++draw_calls;return S_OK;
}
static unsigned char *mock_file(const char *name,unsigned sprites,unsigned tag) {
    unsigned char *loaded=calloc(1,0x130),*rows=calloc(sprites,0x44),*images=calloc(256,0x18),*entry=calloc(1,128);
    CHECK(loaded && rows && images && entry);U32(loaded,0x110)=256;U32(loaded,0x118)=sprites;
    POINTER(loaded,0x11c)=rows;POINTER(loaded,0x124)=images;
    U32(entry,0x10)=64;strcpy((char *)entry+64,name);POINTER(images,0x10)=entry;
    for(unsigned i=0;i<sprites;++i) {
        unsigned image=i%256;U32(rows+i*0x44,8)=image;
        FP(rows+i*0x44,0x2c)=FP(rows+i*0x44,0x30)=1;
        FP(rows+i*0x44,0x34)=96;FP(rows+i*0x44,0x38)=64;
        *(uintptr_t *)(images+image*0x18)=tag+image;
    }
    if(!strncmp(name,"ascii/",6))for(unsigned n=0;n<15 && 269+n<sprites;++n) {
        FP(rows+(254+n)*0x44,0x34)=FP(rows+(254+n)*0x44,0x38)=16;
        FP(rows+(269+n)*0x44,0x34)=FP(rows+(269+n)*0x44,0x38)=14;
    }
    return loaded;
}
int main(int argc,char **argv) {
    void *mock=VirtualAlloc((void *)0x400000,0x180000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);CHECK(mock==(void *)0x400000);
    unsigned char *manager=calloc(1,0x3121000);CHECK(manager!=NULL);
    *(void **)0x51f65c=manager;*(void **)0x4ccdf8=(void *)1;
    POINTER(manager,0x312072c+5*4)=mock_file("front/front00.png",190,0x10000);
    POINTER(manager,0x312072c)=mock_file("ascii/ascii_960.png",320,0x20000);
    POINTER(manager,0x312072c+31*4)=mock_file("abcard",57,0x30000);
    POINTER(manager,0x312072c+9*4)=mock_file("player",1,0x40000);
    uint32_t *desc=(uint32_t *)0x4c53c0;
    for(unsigned i=0;i<57;++i) {desc[i*13+1]=i;desc[i*13+11]=i;}
    fixture.player_count=2;
    for(unsigned id=0;id<3;++id) {
        fixture.lives[id]=id?0:2;fixture.bombs[id]=3;fixture.power[id]=id?125:300;fixture.funds[id]=id?37:1234;
        fixture.x[id]=id?60:-60;fixture.y[id]=350;fixture.card_count[id]=64;
        fixture.life_frag[id]=1;fixture.bomb_frag[id]=2;
        for(unsigned n=0;n<64;++n) fixture.cards[id][n]=(HudCard){(int)(n%57),n==63,50,100,(int)(n%3),0};
    }
    fixture.ghost[1]=1;fixture.sprite_file[1]=9;fixture.sprite[1]=0;fixture.life_taps[0]=3;
    if(argc==2) trace=fopen(argv[1],"w");
    CHECK(coop_hud(NULL)==1);CHECK(front && ascii);CHECK(captures==1 && applies==1 && releases==1);
    if(trace) {fclose(trace);trace=NULL;}
    fixture.shop_owner=2;CHECK(coop_hud(NULL)==1);CHECK(captures==2 && applies==2 && releases==2);
    CHECK(draw_calls>100 && native_cards>0);CHECK(card_sprite(-1).texture==NULL);
    coop_menu_hud(1);coop_menu_hud(2);
    CHECK(captures==4 && applies==4 && releases==4);
    fixture.local_player=1;fixture.shared_keyboard=0;fixture.shop_owner=0;
    fixture.cards[1][63].id=5;fixture.cards[0][63].id=9;
    CHECK(coop_hud(NULL)==1);CHECK(captures==5 && applies==5 && releases==5);
    CHECK(personal_icons==0); /* The extension no longer replaces the native lower-left UI. */
    fixture.card_count[1]=0;CHECK(coop_hud(NULL)==1); /* Own empty inventory must not fall back to P1. */
    CHECK(personal_icons==0);
    coop_menu_hud(2);coop_menu_hud(1);CHECK(captures==8 && applies==8 && releases==8);
    fixture.player_count=3;fixture.local_player=2;CHECK(coop_hud(NULL)==1);coop_menu_hud(3);
    CHECK(captures==10 && applies==10 && releases==10 && native_cards>0 && native_flushes==10 && partial_icons>0);
    CHECK(resource_faces>0 && resource_faces==resource_outlines);
    unsigned faces=resource_faces;
    POINTER(manager,0x312072c)=mock_file("ascii/ascii_replacement.png",320,0x50000);
    CHECK(coop_hud(NULL)==1 && resource_faces>faces && resource_faces==resource_outlines);
    printf("PASS HUD: %u assertions, %u quads, %u native cards; 2P/3P bounds, state restoration, native fragment tint and categories\n",checks,draw_calls,native_cards);
    return 0;
}
