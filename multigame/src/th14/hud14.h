#ifndef TH14_HUD_H
#define TH14_HUD_H
#include <d3d9.h>
static void *draw_root_original;
typedef struct {float x,y,z,rhw;DWORD color;float u,v;} Vertex14;
static float ui_sx,ui_sy,ui_x,ui_y;
static void title_shade14(IDirect3DDevice9 *device,float top,float bottom,DWORD a,DWORD b) {
    float left=ui_x+416*ui_sx-.5f,right=ui_x+640*ui_sx-.5f;
    float y0=ui_y+top*ui_sy-.5f,y1=ui_y+bottom*ui_sy-.5f;
    Vertex14 v[4]={{left,y0,0,1,a,0,0},{right,y0,0,1,a,0,0},
                   {left,y1,0,1,b,0,0},{right,y1,0,1,b,0,0}};
    IDirect3DDevice9_DrawPrimitiveUP(device,D3DPT_TRIANGLESTRIP,2,v,sizeof(*v));
}
static void quad14(IDirect3DDevice9 *device,void *texture,float x,float y,float w,float h,float u0,float v0,float u1,float v1,DWORD color) {
    Vertex14 v[4]={{ui_x+x*ui_sx-.5f,ui_y+y*ui_sy-.5f,0,1,color,u0,v0},{ui_x+(x+w)*ui_sx-.5f,ui_y+y*ui_sy-.5f,0,1,color,u1,v0},{ui_x+x*ui_sx-.5f,ui_y+(y+h)*ui_sy-.5f,0,1,color,u0,v1},{ui_x+(x+w)*ui_sx-.5f,ui_y+(y+h)*ui_sy-.5f,0,1,color,u1,v1}};
    IDirect3DDevice9_SetTexture(device,0,texture);IDirect3DDevice9_DrawPrimitiveUP(device,D3DPT_TRIANGLESTRIP,2,v,sizeof(*v));
}
static void *front14(void) {
    void *manager=PTR(ANM);if(!manager)return NULL;
    for(unsigned i=0;i<26;++i){void *file=(void *)(uintptr_t)FIELD(manager,0xbc7b0c+i*4);if(file && strstr((const char *)OFF(file,4),"front.anm"))return file;}return NULL;
}
static void sprite14(IDirect3DDevice9 *device,void *file,unsigned number,float x,float y,float w,float h,DWORD color) {
    if(!file)return;unsigned count=FIELD(file,0x118);void *table=(void *)(uintptr_t)FIELD(file,0x11c);
    if(!table || count>10000)return;
    if(number<count){void *s=OFF(table,number*0x48);
        void *d3d=(void *)(uintptr_t)FIELD(s,8);if(!d3d)return;
        quad14(device,PTR(d3d),x,y,w,h,FLT(s,0x24),FLT(s,0x28),FLT(s,0x2c),FLT(s,0x30),color);return;}
}
static void *hud_file14(const char *name) {
    void *manager=PTR(ANM);if(!manager)return NULL;
    for(unsigned i=0;i<26;++i){void *file=(void *)(uintptr_t)FIELD(manager,0xbc7b0c+i*4);if(file && strstr((const char *)OFF(file,4),name))return file;}return NULL;
}
static void hud_text14(IDirect3DDevice9 *device,void *ascii,const char *text,float x,float y,float size,DWORD color) {
    for(;*text;++text,x+=size){unsigned c=(unsigned char)*text;if(c>=32 && c<127 && c!=' ')sprite14(device,ascii,c-32,x,y,size,size,color);}
}
static void hud_numeric14(IDirect3DDevice9 *device,void *ascii,const char *text,float right,float y,int small,DWORD color) {
    float step=small?8:13,x=right-strlen(text)*step;
    for(;*text;++text,x+=step){unsigned c=(unsigned char)*text,n;
        if(c>='0' && c<='9')n=(small?196:239)+c-'0';
        else if(c=='/')n=small?206:249;
        else if(c=='.')n=small?211:250;
        else if(c=='-' && small)n=208;
        else continue;
        sprite14(device,ascii,n,x,y,small?8:16,small?10:16,color);
    }
}
static void hud_power14(IDirect3DDevice9 *device,void *ascii,int value,int maximum,float right,float y) {
    char text[16];float x=right-94;
    value=value<0?0:value;maximum=maximum<0?0:maximum;
    snprintf(text,sizeof(text),"%d",value/100);hud_numeric14(device,ascii,text,x+13,y,0,0xffffa060);
    snprintf(text,sizeof(text),".%02d",value%100);hud_numeric14(device,ascii,text,x+37,y+6,1,0xffffa060);
    hud_numeric14(device,ascii,"/",x+51,y,0,0xffffa060);
    snprintf(text,sizeof(text),"%d",maximum/100);hud_numeric14(device,ascii,text,x+66,y,0,0xffffa060);
    snprintf(text,sizeof(text),".%02d",maximum%100);hud_numeric14(device,ascii,text,x+90,y+6,1,0xffffa060);
}
static unsigned hud_life_required14(unsigned id) {
    unsigned index=(unsigned)*resource(id,0x4f586c);
    // This is the live native requirement table, including thcrap adjustments.
    if(index>=50)return 999999;
    unsigned required=U32(0x4d57a0+index*4);return required?required:1;
}
static unsigned hud_fragment14(unsigned parts,unsigned required,int bomb) {
    unsigned tier=(unsigned)((uint64_t)parts*(bomb?6:5)/required);return tier>5?5:tier;
}
static DWORD hud_fragment_color14(unsigned tier) {
    static const DWORD colors[]={0xffffffff,0xff8080ff,0xff8080ff,0xff9090ff,0xffa0a0ff,0xffc0c0ff};return colors[tier>5?5:tier];
}
static void hud_rule14(IDirect3DDevice9 *device,float x,float y,float width,DWORD color) {
    IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_COLOROP,D3DTOP_SELECTARG2);
    IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG2);
    // Original ANM mirrored half-quads: opaque center, transparent ends.
    for(unsigned half=0;half<2;++half){float a=x+half*width*.5f,b=a+width*.5f;
        DWORD left=half?color:color&0xffffff,right=half?color&0xffffff:color;
        Vertex14 v[4]={{ui_x+a*ui_sx-.5f,ui_y+y*ui_sy-.5f,0,1,left,0,0},{ui_x+b*ui_sx-.5f,ui_y+y*ui_sy-.5f,0,1,right,0,0},{ui_x+a*ui_sx-.5f,ui_y+(y+1.6f)*ui_sy-.5f,0,1,left,0,0},{ui_x+b*ui_sx-.5f,ui_y+(y+1.6f)*ui_sy-.5f,0,1,right,0,0}};
        IDirect3DDevice9_SetTexture(device,0,NULL);IDirect3DDevice9_DrawPrimitiveUP(device,D3DPT_TRIANGLESTRIP,2,v,sizeof(*v));}
    IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_COLOROP,D3DTOP_MODULATE);
    IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_ALPHAOP,D3DTOP_MODULATE);
}
static int hud_assets_ready14(void *file,unsigned first,unsigned last) {
    if(!file || FIELD(file,0x118)<=last)return 0;
    void *table=(void *)(uintptr_t)FIELD(file,0x11c);if(!table)return 0;
    for(unsigned n=first;n<=last;++n){void *texture=(void *)(uintptr_t)FIELD(OFF(table,n*0x48),8);if(!texture || !PTR(texture))return 0;}
    return 1;
}
static void hud_draft14(IDirect3DDevice9 *device,void *ascii,unsigned state) {
    IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_COLOROP,D3DTOP_SELECTARG2);
    IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG2);
    quad14(device,NULL,326,66,302,66,0,0,0,0,0xc0181024);
    IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_COLOROP,D3DTOP_MODULATE);
    IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_ALPHAOP,D3DTOP_MODULATE);
    char line[80];snprintf(line,sizeof(line),"P%u / %u  %s",draft_owner+1,players,state==6?"SELECT CHARACTER":"SELECT WEAPON");
    hud_text14(device,ascii,line,334,70,10,0xffffe090);
    static const char *names[]={"REIMU","MARISA","SAKUYA"};
    for(unsigned id=0;id<players;++id){DWORD color=id<draft_owner?0xffa0ffa0:id==draft_owner?0xffffe090:0xffaaa0b0;
        if(id<draft_owner)snprintf(line,sizeof(line),"P%u READY: %s %c",id+1,names[draft_char[id]<3?draft_char[id]:0],'A'+(draft_style[id]&1));
        else snprintf(line,sizeof(line),"P%u %s",id+1,id==draft_owner?"> SELECTING...":"WAITING");
        hud_text14(device,ascii,line,334,88+12*id,8,color);
    }
    snprintf(line,sizeof(line),"P%u  SHOT: CONFIRM   BOMB: BACK",draft_owner+1);
    hud_text14(device,ascii,line,328,466,7.6f,0xffffffff);
}
static void capture14(IDirect3DDevice9 *device) {
    if(!test || test->quit!=2)return;test->quit=0;
    wchar_t path[MAX_PATH];if(!GetEnvironmentVariableW(L"TH14_COOP_CAPTURE",path,MAX_PATH))return;
    IDirect3DSurface9 *source=NULL,*copy=NULL;D3DSURFACE_DESC desc;D3DLOCKED_RECT locked;
    if(FAILED(IDirect3DDevice9_GetRenderTarget(device,0,&source)))return;
    if(FAILED(IDirect3DSurface9_GetDesc(source,&desc)) || (desc.Format!=D3DFMT_A8R8G8B8 && desc.Format!=D3DFMT_X8R8G8B8))goto done;
    if(FAILED(IDirect3DDevice9_CreateOffscreenPlainSurface(device,desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&copy,NULL)) || FAILED(IDirect3DDevice9_GetRenderTargetData(device,source,copy)))goto done;
    if(SUCCEEDED(IDirect3DSurface9_LockRect(copy,&locked,NULL,D3DLOCK_READONLY))) {
        FILE *f=_wfopen(path,L"wb");if(f){BITMAPFILEHEADER head={0};BITMAPINFOHEADER info={0};head.bfType=0x4d42;head.bfOffBits=sizeof(head)+sizeof(info);head.bfSize=head.bfOffBits+desc.Width*desc.Height*4;
            info.biSize=sizeof(info);info.biWidth=desc.Width;info.biHeight=-(int)desc.Height;info.biPlanes=1;info.biBitCount=32;
            fwrite(&head,sizeof(head),1,f);fwrite(&info,sizeof(info),1,f);for(unsigned y=0;y<desc.Height;++y)fwrite((unsigned char *)locked.pBits+y*locked.Pitch,desc.Width*4,1,f);fclose(f);log_line("Captured own game render target");}
        IDirect3DSurface9_UnlockRect(copy);
    }
done:if(copy)IDirect3DSurface9_Release(copy);if(source)IDirect3DSurface9_Release(source);
}
static int TC draw_root14(void *registry) {
    int result=((Callback)draw_root_original)(registry);
    IDirect3DDevice9 *device=PTR(0x4d8f68);void *manager=PTR(ANM);
    void *menu=PTR(0x4db6a4);unsigned menu_state=menu?FIELD(menu,0x1c):0;
    int selecting=!seats[0].player && !ending && (menu_state==6 || menu_state==7);
    if(!device || !manager || (!seats[0].player && !selecting) || ending){if(device)capture14(device);return result;}
    // Native font names change with resolution: ascii / ascii_960 / ascii_1280.
    void *front=front14(),*ascii=hud_file14("ascii");
    if(!ascii || (!selecting && !front)){capture14(device);return result;}
    if(!selecting && (!hud_battle_visible14() || !hud_assets_ready14(front,1,23) || !hud_assets_ready14(ascii,0,94) || !hud_assets_ready14(ascii,98,250))){capture14(device);return result;}
    ((void (TC *)(void *))0x475eb0)(manager);
    D3DVIEWPORT9 viewport;if(FAILED(IDirect3DDevice9_GetViewport(device,&viewport)))return result;
    IDirect3DStateBlock9 *state=NULL;if(FAILED(IDirect3DDevice9_CreateStateBlock(device,D3DSBT_ALL,&state)))return result;
    float scale=fminf(viewport.Width/640.f,viewport.Height/480.f);ui_sx=ui_sy=scale;
    ui_x=viewport.X+(viewport.Width-640*scale)*.5f;ui_y=viewport.Y+(viewport.Height-480*scale)*.5f;
    IDirect3DDevice9_SetVertexShader(device,NULL);IDirect3DDevice9_SetPixelShader(device,NULL);
    IDirect3DDevice9_SetFVF(device,D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1);
    IDirect3DDevice9_SetRenderState(device,D3DRS_ZENABLE,FALSE);IDirect3DDevice9_SetRenderState(device,D3DRS_ZWRITEENABLE,FALSE);
    IDirect3DDevice9_SetRenderState(device,D3DRS_ALPHABLENDENABLE,TRUE);IDirect3DDevice9_SetRenderState(device,D3DRS_BLENDOP,D3DBLENDOP_ADD);
    IDirect3DDevice9_SetRenderState(device,D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);IDirect3DDevice9_SetRenderState(device,D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
    IDirect3DDevice9_SetRenderState(device,D3DRS_CULLMODE,D3DCULL_NONE);IDirect3DDevice9_SetRenderState(device,D3DRS_COLORWRITEENABLE,15);
    IDirect3DDevice9_SetRenderState(device,D3DRS_FOGENABLE,FALSE);IDirect3DDevice9_SetRenderState(device,D3DRS_LIGHTING,FALSE);
    RECT clip={(LONG)(ui_x+(selecting?0:416)*scale),(LONG)(ui_y+(selecting?0:90)*scale),(LONG)(ui_x+640*scale),(LONG)(ui_y+(selecting?480:470)*scale)};
    IDirect3DDevice9_SetScissorRect(device,&clip);IDirect3DDevice9_SetRenderState(device,D3DRS_SCISSORTESTENABLE,TRUE);
    IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_COLOROP,D3DTOP_MODULATE);IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_COLORARG1,D3DTA_TEXTURE);IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);
    IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_ALPHAOP,D3DTOP_MODULATE);IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);
    IDirect3DDevice9_SetTextureStageState(device,1,D3DTSS_COLOROP,D3DTOP_DISABLE);IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_TEXCOORDINDEX,0);IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE);
    IDirect3DDevice9_SetSamplerState(device,0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);IDirect3DDevice9_SetSamplerState(device,0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
    IDirect3DDevice9_SetSamplerState(device,0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);IDirect3DDevice9_SetSamplerState(device,0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);
    if(selecting){hud_draft14(device,ascii,menu_state);IDirect3DStateBlock9_Apply(state);IDirect3DStateBlock9_Release(state);capture14(device);return result;}
    void *table=(void *)(uintptr_t)FIELD(front,0x11c),*bg=OFF(table,0x48),*texture=(void *)(uintptr_t)FIELD(bg,8);
    if(texture){float top=FLT(bg,0x28),span=FLT(bg,0x30)-top;quad14(device,PTR(texture),416,90,224,380,FLT(bg,0x24),top+span*90/480,FLT(bg,0x2c),top+span*470/480,0xffffffff);}
    if(players==3){
        // Shade only the title band, before drawing data. Feather both ends.
        IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_COLOROP,D3DTOP_SELECTARG2);
        IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG2);
        IDirect3DDevice9_SetTexture(device,0,NULL);
        title_shade14(device,342,360,0,0x60000000);
        title_shade14(device,360,412,0x60000000,0x60000000);
        title_shade14(device,412,430,0x60000000,0);
        IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_COLOROP,D3DTOP_MODULATE);
        IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_ALPHAOP,D3DTOP_MODULATE);
    }
    char text[64];
    // Same native sizes for two or three seats; only remove vertical dead space.
    for(unsigned id=0;id<players;++id){float x=428,y=100+110*id;int lives=*resource(id,0x4f5864),bombs=*resource(id,0x4f5870),lf=*resource(id,0x4f5868),bf=*resource(id,0x4f5874);
        unsigned required=hud_life_required14(id);lf=lf<0?0:lf;bf=bf<0?0:bf>7?7:bf;
        snprintf(text,sizeof(text),"P%u",id+1);DWORD own=online_map && online_map->role==id?0xffffe040:0xffffffff;
        for(unsigned j=0;text[j];++j)sprite14(device,ascii,98+(unsigned char)text[j]-32,x+8*j,y-9,8,8,own);
        sprite14(device,front,6,x,y,72,18,0xffffffff);sprite14(device,front,8,x,y+38,72,18,0xffffffff);sprite14(device,front,11,x+16,y+76,72,18,0xffffffff);
        for(int j=0;j<8;++j){unsigned ht=hud_fragment14((unsigned)lf,required,0),bt=hud_fragment14((unsigned)bf,8,1);
            unsigned heart=j<lives?17:j==lives?12+ht:12,star=j<bombs?23:j==bombs?18+bt:18;
            sprite14(device,front,heart,514+13*j,y,16,20,j==lives?hud_fragment_color14(ht):0xffffffff);
            sprite14(device,front,star,514+13*j,y+38,16,20,j==bombs?hud_fragment_color14(bt):0xffffffff);}
        for(unsigned kind=0;kind<2;++kind){float fy=y+38*kind;sprite14(device,front,7,508,fy+16,72,18,0xffffffff);
            if(!kind && required>=999999)strcpy(text,"----");else snprintf(text,sizeof(text),"%u/%u",kind?(unsigned)bf:(unsigned)lf,kind?8:required);
            hud_numeric14(device,ascii,text,622,fy+21,1,0xffffffff);}
        hud_rule14(device,x,y+32,192,0xffff20ff);hud_rule14(device,x,y+70,192,0xff40ff40);hud_rule14(device,x+16,y+92,192,0xffff4040);
        hud_power14(device,ascii,*resource(id,0x4f5858),*resource(id,0x4f585c),622,y+75);
    }
    float footer=198+110*(players-1);
    sprite14(device,front,10,444,footer,80,18,0xffffffff);sprite14(device,front,9,444,footer+22,72,18,0xffffffff);
    snprintf(text,sizeof(text),"%u",U32(0x4f584c)/100);hud_numeric14(device,ascii,text,622,footer-1,0,0xff60d0ff);
    snprintf(text,sizeof(text),"%u",U32(0x4f5840));hud_numeric14(device,ascii,text,622,footer+21,0,0xffffffff);
    hud_rule14(device,444,footer+16,192,0xff4040ff);hud_rule14(device,444,footer+38,192,0xff404040);
    // Native playfield is x=32..416, y=16..464, with the player x centered.
    RECT field_clip={(LONG)(ui_x+32*scale),(LONG)(ui_y+16*scale),
                     (LONG)(ui_x+416*scale),(LONG)(ui_y+464*scale)};
    for(unsigned id=0;id<players;++id){
        float x,y;char kind;unsigned count;
        if(!transfer14_hint(id,&x,&y,&kind,&count))continue;
        IDirect3DDevice9_SetScissorRect(device,&field_clip);
        char hint[]={kind,' ',(char)('0'+count),'/','5',0};
        x=fmaxf(34.f,fminf(374.f,224.f+x-20.f));
        y=fmaxf(18.f,fminf(454.f,16.f+y-25.f-9.f*id));
        for(unsigned j=0;hint[j];++j)if(hint[j]!=' ')
            sprite14(device,ascii,98+(unsigned char)hint[j]-32,x+8*j,y,8,8,
                     kind=='L'?0xffffb7e4:0xffffd080);
    }
    IDirect3DStateBlock9_Apply(state);IDirect3DStateBlock9_Release(state);
    capture14(device);return result;
}
#endif
