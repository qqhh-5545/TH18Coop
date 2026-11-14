/* Render the real HUD into a mock D3D command sink. No window or GPU. */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <d3d9.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static unsigned draw_calls,captures,applies,releases,checks;
static uintptr_t bound_texture;
static FILE *trace;

static unsigned personal_icons;
static unsigned native_cards,native_flushes,partial_icons;
static unsigned resource_faces,resource_outlines;
static unsigned life_labels,graze_labels,maxpoint_labels;
static unsigned small_numbers,big_numbers,yellow_tags,title_shade,wrong_bonus_numbers,fragment_colors;
static struct {unsigned role;} online_data,*online_map;
static unsigned life_required=3;
#define CHECK(e) do {if(!(e)) {fprintf(stderr,"HUD FAIL line %d: %s\n",__LINE__,#e);exit(1);}++checks;} while(0)
static D3DVIEWPORT9 requested;
static HRESULT mock_viewport(D3DVIEWPORT9 *v) {*v=requested;return S_OK;}
static HRESULT stateblock(IDirect3DStateBlock9 **s) {++captures;*s=(IDirect3DStateBlock9 *)1;return S_OK;}
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
#define IDirect3DDevice9_GetViewport(d,v) mock_viewport(v)
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

#undef IDirect3DDevice9_SetScissorRect
#define IDirect3DDevice9_SetScissorRect(d,r) ((void)0)
#define TC __attribute__((thiscall))
typedef int (TC *Callback)(void *);
#define ANM 0x4f56cc
#define OFF(p,o) ((unsigned char *)(p)+(o))
#define FIELD(p,o) (*(uint32_t *)OFF(p,o))
#define FLT(p,o) (*(float *)OFF(p,o))
static int mock_visible=1;
static int hud_battle_visible14(void){return mock_visible;}
static unsigned hint_count;
static char hint_kind;
static unsigned hint_glyphs;
static int transfer14_hint(unsigned id,float *x,float *y,char *kind,unsigned *count){
    if(id || !hint_count)return 0;
    *x=0;*y=380;*kind=hint_kind;*count=hint_count;return 1;
}
static void *manager,*menu;
static unsigned draft_owner,draft_char[3]={2,1,0},draft_style[3]={0,1,0};
static void *mock_ptr(uintptr_t address){if(address==ANM)return manager;if(address==0x4d8f68)return (void *)1;if(address==0x4db6a4)return menu;return *(void **)address;}
#define PTR(a) mock_ptr((uintptr_t)(a))
static unsigned mock_u32(unsigned a){if(a>=0x4d57a0 && a<0x4d5868)return life_required;return a==0x4f584c?1000000u:1234u;}
#define U32(a) mock_u32(a)
static struct {void *player;int ghost;} seats[3];
static unsigned players=2,ending;
static struct {unsigned quit;} *test;
static int32_t values[3][28];
static int32_t *resource(unsigned id,unsigned address){return &values[id][(address-0x4f5830)/4];}
static void log_line(const char *s,...){(void)s;}
static void mock_flush(void *p){CHECK(p==manager);}
#include "th14_hud_fixture.h"
static int TC root_stub(void *p){return 1;}
static HRESULT draw(D3DPRIMITIVETYPE type,UINT n,const void *data,UINT stride){
    CHECK(type==D3DPT_TRIANGLESTRIP && n==2 && stride==sizeof(Vertex14));
    const Vertex14 *v=data;
    float width=(v[1].x-v[0].x)/ui_sx,y0=(v[0].y+.5f-ui_y)/ui_sy;
    float footer=198+110*(players-1);
    if(bound_texture==0x10001){CHECK(fabsf(v[2].v-470.f/480)<.001f);}
    if(bound_texture==0x10006){CHECK(fabsf(width-72.f)<.01f);CHECK(fabsf(y0-(100+110*life_labels))<.01f);++life_labels;}
    if(bound_texture==0x10009){CHECK(fabsf(y0-footer-22)<.01f);++graze_labels;}
    if(bound_texture==0x1000a){CHECK(fabsf(y0-footer)<.01f);++maxpoint_labels;}
    if(bound_texture>=0x10023 && bound_texture<=0x1002d)++wrong_bonus_numbers;
    if(bound_texture>=0x200c4 && bound_texture<=0x200d3){CHECK(fabsf(width-8)<.01f);++small_numbers;}
    if(bound_texture>=0x200ef && bound_texture<=0x200fa){CHECK(fabsf(width-16)<.01f);++big_numbers;}
    if(bound_texture>=0x20062 && bound_texture<=0x200c3 && v[0].color==0xffffe040){CHECK(online_map && fabsf(y0-(91+110*online_map->role))<.01f);++yellow_tags;}
    if(bound_texture==0 && fabsf(width-224)<.01f && (v[0].color>>24)<=96 && y0>=342 && y0<430){CHECK(players==3);++title_shade;}
    if(bound_texture>=0x1000c && bound_texture<=0x10017 && v[0].color!=0xffffffff)++fragment_colors;
    DWORD hint_color=hint_kind=='L'?0xffffb7e4:0xffffd080;
    int hint=hint_count && v[0].color==hint_color;
    if(hint){const char expected[]={hint_kind,(char)('0'+hint_count),'/','5'};
        CHECK(hint_glyphs<sizeof(expected));CHECK(bound_texture==0x20000+98+(unsigned char)expected[hint_glyphs++]-32);}
    for(unsigned i=0;i<4;++i){float x=(v[i].x+.5f-ui_x)/ui_sx,y=(v[i].y+.5f-ui_y)/ui_sy;
        CHECK(isfinite(x) && isfinite(y));
        CHECK(x>=(hint?32.f:seats[0].player?415.99f:0) && x<=(hint?416.f:640.01f));CHECK(y>=(seats[0].player?89.99f:0) && y<=(seats[0].player?470.01f:480.01f));}
    if(trace)fprintf(trace,"%lu %.3f %.3f %.3f %.3f %lu %.6f %.6f %.6f %.6f\n",(unsigned long)bound_texture,(v[0].x+.5f-ui_x)/ui_sx,(v[0].y+.5f-ui_y)/ui_sy,(v[1].x-v[0].x)/ui_sx,(v[2].y-v[0].y)/ui_sy,(unsigned long)v[0].color,v[0].u,v[0].v,v[1].u,v[2].v);
    ++draw_calls;return S_OK;
}
static void *file(const char *name,unsigned tag){
    unsigned char *f=calloc(1,0x130),*rows=calloc(300,0x48);strcpy((char *)f+4,name);FIELD(f,0x118)=300;FIELD(f,0x11c)=(uintptr_t)rows;
    for(unsigned i=0;i<300;++i){void **t=malloc(sizeof(void *));*t=(void *)(uintptr_t)(tag+i);FIELD(rows+i*0x48,8)=(uintptr_t)t;FLT(rows+i*0x48,0x2c)=FLT(rows+i*0x48,0x30)=1;}
    return f;
}
int main(void){
    manager=calloc(1,0xbd0000);FIELD(manager,0xbc7b0c)=(uintptr_t)file("front.anm",0x10000);FIELD(manager,0xbc7b10)=(uintptr_t)file("ascii.anm",0x20000);
    draw_root_original=root_stub;seats[0].player=(void *)1;
    for(unsigned i=0;i<3;++i){*resource(i,0x4f5864)=i+1;*resource(i,0x4f5870)=2;*resource(i,0x4f5868)=i;*resource(i,0x4f586c)=i;*resource(i,0x4f5874)=7-i;*resource(i,0x4f5858)=350-i*50;*resource(i,0x4f585c)=400;}
    CHECK(hud_fragment14(4,8,1)==3 && hud_fragment14(7,8,1)==5);
    CHECK(hud_fragment14(2,5,0)==2 && hud_fragment14(2,3,0)==3);
    life_required=5;CHECK(hud_life_required14(0)==5);life_required=3;
    D3DVIEWPORT9 sizes[]={{0,0,640,480,0,1},{0,0,1280,960,0,1},{20,10,1920,1080,0,1},{0,0,960,720,0,1}};
    const char *fonts[]={"ascii.anm","ascii_960.anm","ascii_1280.anm"};
    void *ascii=(void *)(uintptr_t)FIELD(manager,0xbc7b10);
    for(unsigned font=0;font<3;++font){strcpy((char *)ascii+4,fonts[font]);
        seats[0].player=(void *)1;menu=NULL;
        for(unsigned count=2;count<=3;++count)for(unsigned v=0;v<4;++v)for(unsigned mode=0;mode<=count;++mode){players=count;requested=sizes[v];draw_calls=0;life_labels=graze_labels=maxpoint_labels=0;small_numbers=big_numbers=yellow_tags=title_shade=wrong_bonus_numbers=fragment_colors=0;
            online_map=mode?&online_data:NULL;online_data.role=mode?mode-1:0;
            CHECK(draw_root14(NULL)==1);CHECK(draw_calls>60);CHECK(life_labels==players && graze_labels==1 && maxpoint_labels==1);
            CHECK(small_numbers>0 && big_numbers>0 && wrong_bonus_numbers==0 && fragment_colors>0);CHECK(yellow_tags==(mode?2:0));CHECK(title_shade==(players==3?3:0));CHECK(captures==applies && applies==releases);}
        online_map=NULL;
        seats[0].player=NULL;menu=calloc(1,0x40);requested=sizes[0];
        for(unsigned count=2;count<=3;++count)for(unsigned state=6;state<=7;++state)for(unsigned owner=0;owner<count;++owner){players=count;FIELD(menu,0x1c)=state;draft_owner=owner;draw_calls=0;life_labels=graze_labels=maxpoint_labels=0;CHECK(draw_root14(NULL)==1);CHECK(draw_calls>60);CHECK(life_labels==0 && graze_labels==0 && maxpoint_labels==0);CHECK(captures==applies && applies==releases);}
        free(menu);menu=NULL;
    }
    seats[0].player=(void *)1;menu=NULL;mock_visible=0;draw_calls=0;CHECK(draw_root14(NULL)==1 && draw_calls==0);
    mock_visible=1;players=2;requested=sizes[2];
    for(unsigned type=0;type<2;++type)for(unsigned count=3;count<=4;++count){
        hint_kind=type?'P':'L';hint_count=count;hint_glyphs=draw_calls=life_labels=graze_labels=maxpoint_labels=0;
        CHECK(draw_root14(NULL)==1 && hint_glyphs==4);
    }
    hint_count=0;
    void *front=(void *)(uintptr_t)FIELD(manager,0xbc7b0c),*rows=(void *)(uintptr_t)FIELD(front,0x11c);unsigned texture=FIELD(OFF(rows,6*0x48),8);FIELD(OFF(rows,6*0x48),8)=0;draw_calls=0;CHECK(draw_root14(NULL)==1 && draw_calls==0);FIELD(OFF(rows,6*0x48),8)=texture;
    rows=(void *)(uintptr_t)FIELD(ascii,0x11c);texture=FIELD(OFF(rows,239*0x48),8);FIELD(OFF(rows,239*0x48),8)=0;draw_calls=0;CHECK(draw_root14(NULL)==1 && draw_calls==0);FIELD(OFF(rows,239*0x48),8)=texture;
    printf("PASS TH14 HUD: %u assertions, all 3 native ASCII filenames, 2/3 seat battle and character/weapon selection feedback, 640/960/1280 and offset viewports; D3D state restored\n",checks);return 0;
}

