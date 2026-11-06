#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <d3d9.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "coop_api.h"
typedef struct {float x,y,z,rhw;DWORD color;float u,v;} Vertex;
typedef struct {IDirect3DTexture9 *texture;float u,v,u2,v2,w,h;} Sprite;
static IDirect3DDevice9 *device;
static unsigned char *front,*ascii;
static float scale,origin_x,origin_y;
static void capture_test_target(void) {
    wchar_t path[MAX_PATH],diagnostic[MAX_PATH+8];if(!coop_hud_capture_path(path,MAX_PATH))return;
    IDirect3DSurface9 *target=NULL,*copy=NULL;D3DSURFACE_DESC desc={0};D3DLOCKED_RECT locked={0};
    HRESULT result=IDirect3DDevice9_GetRenderTarget(device,0,&target);
    if(SUCCEEDED(result))result=IDirect3DSurface9_GetDesc(target,&desc);
    if(SUCCEEDED(result) && (!desc.Width || !desc.Height || (uint64_t)desc.Width*desc.Height>8000000 || (desc.Format!=D3DFMT_A8R8G8B8 && desc.Format!=D3DFMT_X8R8G8B8)))result=E_FAIL;
    if(SUCCEEDED(result))result=IDirect3DDevice9_CreateOffscreenPlainSurface(device,desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&copy,NULL);
    if(SUCCEEDED(result))result=IDirect3DDevice9_GetRenderTargetData(device,target,copy);
    if(SUCCEEDED(result))result=IDirect3DSurface9_LockRect(copy,&locked,NULL,D3DLOCK_READONLY);
    if(SUCCEEDED(result)) {
        FILE *file=_wfopen(path,L"wb");if(file) {
            BITMAPFILEHEADER header={0};BITMAPINFOHEADER info={0};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(info);header.bfSize=header.bfOffBits+desc.Width*desc.Height*4;
            info.biSize=sizeof(info);info.biWidth=(LONG)desc.Width;info.biHeight=-(LONG)desc.Height;info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;
            fwrite(&header,1,sizeof(header),file);fwrite(&info,1,sizeof(info),file);
            for(unsigned y=0;y<desc.Height;++y)fwrite((unsigned char *)locked.pBits+y*locked.Pitch,4,desc.Width,file);
            fclose(file);
        }
        IDirect3DSurface9_UnlockRect(copy);
    }
    swprintf(diagnostic,MAX_PATH+8,L"%ls.txt",path);FILE *note=_wfopen(diagnostic,L"wb");
    if(note){fprintf(note,"render_target=%ux%u format=%u multisample=%u result=%08lx\n",desc.Width,desc.Height,desc.Format,desc.MultiSampleType,(unsigned long)result);fclose(note);}
    if(copy)IDirect3DSurface9_Release(copy);if(target)IDirect3DSurface9_Release(target);
}
#define U32(p,o) (*(uint32_t *)((unsigned char *)(p)+(o)))
#define FP(p,o) (*(float *)((unsigned char *)(p)+(o)))
#define POINTER(p,o) (*(unsigned char **)((unsigned char *)(p)+(o)))

static unsigned char *file_at(unsigned slot) {
    unsigned char *manager=*(unsigned char **)0x51f65c;
    return manager && slot<33?POINTER(manager,0x312072c+slot*4):NULL;
}
static unsigned char *file_named(const char *name) {
    for(unsigned i=0;i<33;++i) {
        unsigned char *loaded=file_at(i);
        if(!loaded || !U32(loaded,0x110)) continue;
        unsigned char *images=POINTER(loaded,0x124),*entry=images?POINTER(images,0x10):NULL;
        if(entry) {
            const char *actual=(char *)entry+U32(entry,0x10);
            if(!strcmp(actual,name) || (!strcmp(name,"ascii/ascii.png") &&
               !strncmp(actual,"ascii/ascii",11) && U32(loaded,0x118)>=320)) return loaded;
        }
    }
    return NULL;
}
static Sprite sprite_at(unsigned char *loaded,int id) {
    Sprite out={0};
    if(!loaded || id<0 || (unsigned)id>=U32(loaded,0x118)) return out;
    unsigned char *sprites=POINTER(loaded,0x11c),*images=POINTER(loaded,0x124);
    if(!sprites || !images) return out;
    unsigned char *sprite=sprites+id*0x44;unsigned image=U32(sprite,8)&255;
    if(image>=U32(loaded,0x110)) return out;
    out.texture=*(IDirect3DTexture9 **)(images+image*0x18);
    out.u=FP(sprite,0x24);out.v=FP(sprite,0x28);out.u2=FP(sprite,0x2c);out.v2=FP(sprite,0x30);
    out.h=FP(sprite,0x34);out.w=FP(sprite,0x38);return out;
}
static void quad(float x,float y,float w,float h,Sprite s,DWORD color) {
    float a=origin_x+x*scale-.5f,b=origin_y+y*scale-.5f,c=a+w*scale,d=b+h*scale;
    Vertex v[6]={{a,b,0,1,color,s.u,s.v},{c,b,0,1,color,s.u2,s.v},{a,d,0,1,color,s.u,s.v2},
        {a,d,0,1,color,s.u,s.v2},{c,b,0,1,color,s.u2,s.v},{c,d,0,1,color,s.u2,s.v2}};
    IDirect3DDevice9_SetTexture(device,0,(IDirect3DBaseTexture9 *)s.texture);
    IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_COLOROP,s.texture?D3DTOP_MODULATE:D3DTOP_SELECTARG2);
    IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_ALPHAOP,s.texture?D3DTOP_MODULATE:D3DTOP_SELECTARG2);
    IDirect3DDevice9_DrawPrimitiveUP(device,D3DPT_TRIANGLELIST,2,v,sizeof(Vertex));
}
static void rect(float x,float y,float w,float h,DWORD color) {Sprite s={0};quad(x,y,w,h,s,color);}
static void image(float x,float y,float w,float h,int id) {
    Sprite s=sprite_at(front,id);if(s.texture) quad(x,y,w,h,s,0xffffffff);
}
static void text(float x,float y,const char *line,DWORD color,float size) {
    for(;*line;++line,x+=size*.66f) {
        unsigned c=(unsigned char)*line;
        if(c>=32 && c<127 && c!=' ') {Sprite s=sprite_at(ascii,(int)c-32);if(s.texture) quad(x,y,size,size,s,color);}
    }
}
static float resource_number(float x,float y,const char *line,DWORD color,DWORD outline,float height) {
    /* Native HUD style10: digit0=269, paired style11 outline0=254.
       Use loaded ANM geometry/texture, including replacement-font patches. */
    for(;*line;++line) {
        unsigned c=(unsigned char)*line;int glyph=-1;
        if(c>='0' && c<='9')glyph=(int)(c-'0');
        else if(c=='.')glyph=11;else if(c==',')glyph=14;else if(c=='/')glyph=10;
        if(glyph>=0) {
            Sprite edge=sprite_at(ascii,254+glyph),face=sprite_at(ascii,269+glyph);
            if(edge.texture && edge.h>0 && face.texture) {
                float w=height*edge.w/edge.h,fw=height*face.w/edge.h,fh=height*face.h/edge.h;
                quad(x,y,w,height,edge,outline);
                quad(x+(w-fw)*.5f,y+(height-fh)*.5f,fw,fh,face,color);
            }
        }
        x+=height*.66f;
    }
    return x;
}
static Sprite card_sprite(int id) {
    Sprite out={0};const uint32_t *desc=(const uint32_t *)0x4c53c0;
    for(unsigned i=0;i<57;++i,desc+=13) if((int)desc[1]==id) {
        out=sprite_at(file_at(31),(int)desc[11]);
        break;
    }
    return out;
}
static void draw_players(const HudStats *stats) {
    for(unsigned id=0;id<(stats->player_count==3?3u:2u);++id) {
        float x=stats->x[id]+224,y=stats->y[id]+16;
        if(stats->ghost[id]) {
            text(x-16,y-24,"SPIRIT",0xc0efffff,8);
        }
        if(stats->lives[id]>=0 || stats->ghost[id]) {
            char label[24];snprintf(label,sizeof(label),"P%u",id+1);
            text(x-7,y-15,label,0xff102030,10);text(x-8,y-16,label,id?0xffa4ffff:0xffffefb0,10);
            if(stats->life_taps[id]>=3 || stats->power_taps[id]>=3) {
                snprintf(label,sizeof(label),"%c %d/5",stats->life_taps[id]?'L':'P',stats->life_taps[id]?stats->life_taps[id]:stats->power_taps[id]);
                rect(x-20,y-31,44,11,0xc0203040);text(x-18,y-31,label,0xffffffff,9);
            }
        }
    }
}

typedef struct {HudCard card;float x,y,w,h;} QueuedCard;
static QueuedCard queued[128];static unsigned queued_count;
typedef struct {int sprite;DWORD color;} FragmentStyle;
static FragmentStyle fragments[2][3];static unsigned char *styled_front;
static void resource_styles(void) {
    if(styled_front==front)return;styled_front=front;
    for(unsigned kind=0;kind<2;++kind)for(unsigned n=0;n<3;++n) {
        fragments[kind][n]=(FragmentStyle){(kind?22:16)+(int)n,n?0xff8080ff:0xffffffff};
        if(!front || U32(front,0x114)<42)continue;
        unsigned char **scripts=(unsigned char **)POINTER(front,0x120);unsigned index=kind?40:32;
        unsigned char *ip=scripts[index],*end=scripts[index+1];
        if(!ip || end<=ip || end-ip>4096)continue;
        int matching=0;
        for(unsigned count=0;count<256 && ip+8<=end;++count) {
            unsigned op=*(uint16_t *)ip,len=*(uint16_t *)(ip+2);
            if(len<8 || ip+len>end)break;
            if(op==5 && len>=12)matching=U32(ip,8)==7+n;
            else if(matching && op==300 && len>=12)fragments[kind][n].sprite=(int)U32(ip,8);
            else if(matching && op==404 && len>=20)fragments[kind][n].color=0xff000000u|((U32(ip,8)&255)<<16)|((U32(ip,12)&255)<<8)|(U32(ip,16)&255);
            else if(matching && op==7)break;
            ip+=len;
        }
    }
}
static void resource_row(unsigned kind,float x,float y,int whole,int pieces,int compact) {
    float w=compact?11:13,h=w*1.25f,step=w*28/32;
    if(whole<0)whole=0;if(pieces<0)pieces=0;if(pieces>2)pieces=2;
    for(int n=0;n<7;++n) {
        int sprite=n<whole?(kind?25:19):n==whole?fragments[kind][pieces].sprite:kind?22:16;
        DWORD color=n==whole?fragments[kind][pieces].color:0xffffffff;
        Sprite icon=sprite_at(front,sprite);if(icon.texture)quad(x+n*step,y,w,h,icon,color);
    }
    float fx=x+(compact?0:27);image(fx,y+h+.5f,34,8.5f,7);
    char value[12];snprintf(value,sizeof(value),"%d/3",pieces);text(fx+35,y+h+1,value,0xffffffff,8);
}
static void queue_card(HudCard card,float x,float y,float w,float h) {
    if(queued_count<128)queued[queued_count++]=(QueuedCard){card,x,y,w,h};
}
static void draw_sidebar(const HudStats *stats) {
    static const DWORD accents[3]={0xfff2c777,0xff53d6e5,0xffc69be3};
    int compact=stats->player_count==3;unsigned seats=compact?3:2;queued_count=0;
    resource_styles();
    Sprite backdrop=sprite_at(front,1);float top=backdrop.v,span=backdrop.v2-top;
    backdrop.v=top+span*(86.0f/480.0f);backdrop.v2=top+span*(470.0f/480.0f);
    if(backdrop.texture)quad(416,86,224,384,backdrop,0xffffffff);
    char line[64];
    for(unsigned id=0;id<seats;++id) {
        float y=90+id*(compact?126:188),py=y+(compact?49:67);
        rect(431,y-2,197,.6f,0x90719ea0);snprintf(line,sizeof(line),"P%u",id+1);text(434,y,line,accents[id],11);
        if(id==(unsigned)stats->local_player)text(458,y+1,"YOU",accents[id],8.5f);
        if(stats->ghost[id])text(491,y+1,"SPIRIT",accents[id],8.5f);
        if(compact) {
            image(434,y+12,41,10.25f,6);image(536,y+12,55,11.25f,8);
            resource_row(0,434,y+24,stats->lives[id],stats->life_frag[id],1);
            resource_row(1,536,y+24,stats->bombs[id],stats->bomb_frag[id],1);
        }else {
            image(434,y+15,51,12.75f,6);image(434,y+40,58,12,8);
            resource_row(0,494,y+14,stats->lives[id],stats->life_frag[id],0);
            resource_row(1,494,y+39,stats->bombs[id],stats->bomb_frag[id],0);
        }
        image(434,py,34,8.5f,11);snprintf(line,sizeof(line),"%d.",stats->power[id]/100);
        float decimal=resource_number(470,py,line,0xff800000,0x80ffd0d0,10);
        snprintf(line,sizeof(line),"%02d",stats->power[id]%100);resource_number(decimal,py+4,line,0xff800000,0x80ffd0d0,6);
        image(536,py,34,8.5f,10);snprintf(line,sizeof(line),"%d",stats->funds[id]);
        float money_size=strlen(line)>8?52.0f/(strlen(line)*.66f):10;
        resource_number(574,py,line,0xff808000,0x80ffffd0,money_size);
        int count=stats->card_count[id];if(count<0)count=0;if(count>COOP_HUD_CARDS)count=COOP_HUD_CARDS;
        int groups[3][COOP_HUD_CARDS],totals[3]={0},selected=-1;
        for(int n=0;n<count;++n) {
            HudCard card=stats->cards[id][n];if(card.category<0 || card.category>2)continue;
            groups[card.category][totals[card.category]++]=n;
            if(card.category==0 && card.selected)selected=n;
        }
        float ay=y+(compact?78:100);image(434,y+(compact?63:84),compact?50:57,compact?12.5f:14.25f,12);
        int active_limit=compact?3:5,shown=0;
        if(selected<0 && totals[0])selected=groups[0][0];
        if(selected>=0) {queue_card(stats->cards[id][selected],434,ay,compact?28:40,compact?35:50);++shown;}
        for(int n=0;n<totals[0] && shown<active_limit;++n) {
            int source=groups[0][n];if(source==selected)continue;
            int slot=shown-1;float x=compact?468+slot*24:480+(slot%2)*25;
            float cy=ay+(compact?0:(slot/2)*31);queue_card(stats->cards[id][source],x,cy,compact?20:22,compact?25:27.5f);++shown;
        }
        if(!totals[0])text(437,ay+7,"--",0xff7d8c8d,10);
        float other_x=compact?516:534;
        int columns=compact?7:6,rows=compact?1:2;
        for(int group=1;group<3;++group) {
            float gy=y+(compact?(group==1?75:106):(group==1?96:151));
            image(other_x,gy-13,compact?48:49,12,group==1?13:14);
            int limit=columns*rows;
            for(int n=0;n<totals[group] && n<limit;++n)
                queue_card(stats->cards[id][groups[group][n]],other_x+(n%columns)*16,gy+(n/columns)*18.5f,14,17.5f);
            if(!totals[group])text(other_x+2,gy+2,"--",0xff7d8c8d,9);
        }
    }
    if(stats->shop_owner) {
        rect(157,16,246,17,0xdf173246);snprintf(line,sizeof(line),"P%d SHOP: %s",stats->shop_owner,
            stats->shop_owner!=stats->local_player+1 && !stats->shared_keyboard?"WAIT FOR TEAMMATE":stats->shared_keyboard && stats->shop_owner==2?"IJKL  G OK  H BACK":"ARROWS  Z OK  X BACK");
        text(162,19,line,0xffffffff,10);
    }
}
static IDirect3DStateBlock9 *begin_hud(void) {
    coop_hud_card_flush();
    device=*(IDirect3DDevice9 **)0x4ccdf8;if(!device) return NULL;
    D3DVIEWPORT9 vp;if(FAILED(IDirect3DDevice9_GetViewport(device,&vp))) return NULL;
    IDirect3DStateBlock9 *state=NULL;if(FAILED(IDirect3DDevice9_CreateStateBlock(device,D3DSBT_ALL,&state))) return NULL;
    if(FAILED(IDirect3DStateBlock9_Capture(state))) {IDirect3DStateBlock9_Release(state);return NULL;}
    scale=(float)vp.Width/640;if((float)vp.Height/480<scale) scale=(float)vp.Height/480;
    origin_x=vp.X+(vp.Width-640*scale)*.5f;origin_y=vp.Y+(vp.Height-480*scale)*.5f;
    front=file_named("front/front00.png");ascii=file_named("ascii/ascii.png");
    IDirect3DDevice9_SetVertexShader(device,NULL);IDirect3DDevice9_SetPixelShader(device,NULL);
    IDirect3DDevice9_SetFVF(device,D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1);
    IDirect3DDevice9_SetRenderState(device,D3DRS_ZENABLE,FALSE);IDirect3DDevice9_SetRenderState(device,D3DRS_ZWRITEENABLE,FALSE);
    IDirect3DDevice9_SetRenderState(device,D3DRS_LIGHTING,FALSE);IDirect3DDevice9_SetRenderState(device,D3DRS_FOGENABLE,FALSE);
    IDirect3DDevice9_SetRenderState(device,D3DRS_ALPHATESTENABLE,FALSE);IDirect3DDevice9_SetRenderState(device,D3DRS_CULLMODE,D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(device,D3DRS_ALPHABLENDENABLE,TRUE);IDirect3DDevice9_SetRenderState(device,D3DRS_SEPARATEALPHABLENDENABLE,FALSE);
    IDirect3DDevice9_SetRenderState(device,D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);IDirect3DDevice9_SetRenderState(device,D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
    IDirect3DDevice9_SetRenderState(device,D3DRS_BLENDOP,D3DBLENDOP_ADD);IDirect3DDevice9_SetRenderState(device,D3DRS_FILLMODE,D3DFILL_SOLID);
    IDirect3DDevice9_SetRenderState(device,D3DRS_COLORWRITEENABLE,15);IDirect3DDevice9_SetRenderState(device,D3DRS_SCISSORTESTENABLE,FALSE);
    IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_COLORARG1,D3DTA_TEXTURE);IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);
    IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);
    IDirect3DDevice9_SetTextureStageState(device,1,D3DTSS_COLOROP,D3DTOP_DISABLE);IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_TEXCOORDINDEX,0);
    IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE);
    IDirect3DDevice9_SetSamplerState(device,0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);IDirect3DDevice9_SetSamplerState(device,0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
    IDirect3DDevice9_SetSamplerState(device,0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);IDirect3DDevice9_SetSamplerState(device,0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);
    return state;
}
void coop_menu_hud(int owner) {
    IDirect3DStateBlock9 *state=begin_hud();if(!state) return;
    rect(12,8,616,31,0xe0182c40);
    char caption[64];snprintf(caption,sizeof(caption),"P%d  SELECT CHARACTER + STARTING CARDS",owner);
    text(20,11,caption,owner==1?0xffffdc9e:owner==2?0xff8cefff:0xffd0a8ff,11);
    const char *hint=coop_shared_keyboard() && owner==2?"IJKL MOVE / G OK / H BACK":"ARROWS / Z OK / X BACK";
    if(!coop_shared_keyboard() && owner!=coop_view_player()+1)hint="WAIT FOR TEAMMATE";
    text(20,25,hint,0xffffffff,9);
    IDirect3DStateBlock9_Apply(state);IDirect3DStateBlock9_Release(state);
}
int __attribute__((thiscall)) coop_hud(void *ignored) {
    (void)ignored;IDirect3DStateBlock9 *state=begin_hud();if(!state) return 1;
    HudStats stats;coop_hud_stats(&stats);draw_players(&stats);draw_sidebar(&stats);
    for(unsigned n=0;n<queued_count;++n) {
        QueuedCard *entry=&queued[n];HudNativeQuad native[24];
        unsigned count=coop_hud_card_quads(&entry->card,entry->x,entry->y,entry->w,entry->h,native,24);
        for(unsigned i=0;i<count;++i) {
            HudNativeQuad *q=&native[i];Sprite sprite=sprite_at(file_at((unsigned)q->file),q->sprite);if(!sprite.texture)continue;
            static const unsigned order[6]={0,1,2,2,1,3};Vertex vertices[6];
            for(unsigned k=0;k<6;++k){unsigned c=order[k];vertices[k]=(Vertex){origin_x+q->x[c]*scale-.5f,origin_y+q->y[c]*scale-.5f,0,1,q->color,q->u[c],q->v[c]};}
            IDirect3DDevice9_SetTexture(device,0,(IDirect3DBaseTexture9 *)sprite.texture);
            IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_COLOROP,D3DTOP_MODULATE);IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_ALPHAOP,D3DTOP_MODULATE);
            IDirect3DDevice9_SetRenderState(device,D3DRS_DESTBLEND,q->additive?D3DBLEND_ONE:D3DBLEND_INVSRCALPHA);
            IDirect3DDevice9_DrawPrimitiveUP(device,D3DPT_TRIANGLELIST,2,vertices,sizeof(Vertex));
        }
    }
    IDirect3DStateBlock9_Apply(state);IDirect3DStateBlock9_Release(state);capture_test_target();return 1;
}
