#ifndef TH18_NATIVE_GRAPHICS_REPLAY_H
#define TH18_NATIVE_GRAPHICS_REPLAY_H
#ifndef COBJMACROS
#define COBJMACROS
#endif
#include <d3d9.h>
/* Test-only offscreen native Draw, with the verified471b87..471bf5 envelope.
   Native renderer state is reset as in normal frames; nothing is Presented.
   The D3D state/targets are restored alongside each CPU rewind. */
static struct {
    IDirect3DDevice9 *device;IDirect3DStateBlock9 *state;
    IDirect3DSurface9 *target,*depth,*offscreen,*offdepth;D3DVIEWPORT9 viewport;
    IDirect3DTexture9 *texture;IDirect3DSurface9 *resolved;D3DSURFACE_DESC desc;
    unsigned draws;int failed,active;
} rb_graphics;
static struct {IDirect3DDevice9 *device;const IDirect3DDevice9Vtbl *original;IDirect3DDevice9Vtbl shadow;} rb_device_reset;
static int rb_graphics_enabled(void){return rb_probe.serial>=130 && rb_probe.serial<=139;}
static void rb_graphics_release(void) {
    if(rb_graphics.state)IDirect3DStateBlock9_Release(rb_graphics.state);
    if(rb_graphics.target)IDirect3DSurface9_Release(rb_graphics.target);
    if(rb_graphics.depth)IDirect3DSurface9_Release(rb_graphics.depth);
    if(rb_graphics.offscreen)IDirect3DSurface9_Release(rb_graphics.offscreen);
    if(rb_graphics.offdepth)IDirect3DSurface9_Release(rb_graphics.offdepth);
    if(rb_graphics.resolved)IDirect3DSurface9_Release(rb_graphics.resolved);
    if(rb_graphics.texture)IDirect3DTexture9_Release(rb_graphics.texture);
    memset(&rb_graphics,0,sizeof(rb_graphics));
}
static HRESULT WINAPI rb_graphics_device_reset(IDirect3DDevice9 *device,D3DPRESENT_PARAMETERS *parameters) {
    /* Keep an existing d3d9/translation proxy in the call chain. Default-pool
       targets and state blocks must be released before the native Reset. */
    if(rb_runtime_speculating){rb_runtime_barrier=1;return D3DERR_DEVICELOST;}
    const IDirect3DDevice9Vtbl *original=rb_device_reset.original;
    rb_control_active=0;rb_graphics_release();
    if(!original || !rb_runtime_reset_boundary())return D3DERR_INVALIDCALL;
    HRESULT result=original->Reset(device,parameters);
    log_line("ROLLBACK renderer reset hr=%08lx; private targets recreate on next Draw",(unsigned long)result);return result;
}
static int rb_graphics_hook_reset(IDirect3DDevice9 *device) {
    if(rb_device_reset.device==device && device->lpVtbl==&rb_device_reset.shadow)return 1;
    if(!valid((void *)device->lpVtbl,sizeof(IDirect3DDevice9Vtbl)))return 0;
    rb_device_reset.device=device;rb_device_reset.original=device->lpVtbl;rb_device_reset.shadow=*device->lpVtbl;
    rb_device_reset.shadow.Reset=rb_graphics_device_reset;
    DWORD old;if(!VirtualProtect(device,sizeof(void *),PAGE_READWRITE,&old))return 0;
    InterlockedExchangePointer((void *volatile *)device,&rb_device_reset.shadow);VirtualProtect(device,sizeof(void *),old,&old);return 1;
}
static void rb_graphics_unhook_reset(void) {
    IDirect3DDevice9 *device=rb_device_reset.device;
    if(device && valid(device,sizeof(void *)) && device->lpVtbl==&rb_device_reset.shadow){DWORD old;
        if(VirtualProtect(device,sizeof(void *),PAGE_READWRITE,&old)){InterlockedExchangePointer((void *volatile *)device,(void *)rb_device_reset.original);VirtualProtect(device,sizeof(void *),old,&old);}}
    memset(&rb_device_reset,0,sizeof(rb_device_reset));
}
static int rb_graphics_rewind(void) {
    if(!rb_graphics.active)return 1;IDirect3DDevice9 *d=rb_graphics.device;
    HRESULT hr=IDirect3DDevice9_SetRenderTarget(d,0,rb_graphics.target);
    if(SUCCEEDED(hr))hr=IDirect3DDevice9_SetDepthStencilSurface(d,rb_graphics.depth);
    if(SUCCEEDED(hr))hr=IDirect3DStateBlock9_Apply(rb_graphics.state);
    if(SUCCEEDED(hr))hr=IDirect3DDevice9_SetViewport(d,&rb_graphics.viewport);
    if(FAILED(hr))rb_graphics.failed=1;return SUCCEEDED(hr);
}
static int rb_graphics_begin(void) {
    rb_graphics_release();if(!rb_graphics_enabled() && !rb_control_requested)return 1;
    IDirect3DDevice9 *d=PTR(0x4ccdf8);if(!d || FAILED(IDirect3DDevice9_TestCooperativeLevel(d)))return 0;
    if(!rb_graphics_hook_reset(d))return 0;
    rb_graphics.device=d;D3DSURFACE_DESC desc={0},depth={0};
    HRESULT hr=IDirect3DDevice9_GetRenderTarget(d,0,&rb_graphics.target);
    if(SUCCEEDED(hr))hr=IDirect3DSurface9_GetDesc(rb_graphics.target,&desc);
    if(SUCCEEDED(hr))hr=IDirect3DDevice9_GetViewport(d,&rb_graphics.viewport);
    if(SUCCEEDED(hr))hr=IDirect3DDevice9_CreateStateBlock(d,D3DSBT_ALL,&rb_graphics.state);
    if(SUCCEEDED(hr))hr=IDirect3DStateBlock9_Capture(rb_graphics.state);
    if(SUCCEEDED(hr))hr=IDirect3DDevice9_CreateRenderTarget(d,desc.Width,desc.Height,desc.Format,desc.MultiSampleType,desc.MultiSampleQuality,FALSE,&rb_graphics.offscreen,NULL);
    if(SUCCEEDED(hr))hr=IDirect3DDevice9_CreateTexture(d,desc.Width,desc.Height,1,D3DUSAGE_RENDERTARGET,desc.Format,D3DPOOL_DEFAULT,&rb_graphics.texture,NULL);
    if(SUCCEEDED(hr))hr=IDirect3DTexture9_GetSurfaceLevel(rb_graphics.texture,0,&rb_graphics.resolved);
    HRESULT depth_hr=IDirect3DDevice9_GetDepthStencilSurface(d,&rb_graphics.depth);
    if(SUCCEEDED(hr) && SUCCEEDED(depth_hr) && rb_graphics.depth){hr=IDirect3DSurface9_GetDesc(rb_graphics.depth,&depth);
        if(SUCCEEDED(hr))hr=IDirect3DDevice9_CreateDepthStencilSurface(d,depth.Width,depth.Height,depth.Format,depth.MultiSampleType,depth.MultiSampleQuality,TRUE,&rb_graphics.offdepth,NULL);}
    if(FAILED(hr)){log_line("RB_GRAPHICS skipped resources hr=%08lx",(unsigned long)hr);rb_graphics_release();return 0;}
    rb_graphics.desc=desc;rb_graphics.active=1;return 1;
}
static int rb_graphics_draw(void) {
    if(!rb_graphics.active)return 1;
    IDirect3DDevice9 *d=rb_graphics.device;
    HRESULT hr=IDirect3DDevice9_SetRenderTarget(d,0,rb_graphics.offscreen);
    if(SUCCEEDED(hr))hr=IDirect3DDevice9_SetDepthStencilSurface(d,rb_graphics.offdepth);
    if(SUCCEEDED(hr))hr=IDirect3DDevice9_SetViewport(d,&rb_graphics.viewport);
    if(SUCCEEDED(hr))hr=IDirect3DDevice9_Clear(d,0,NULL,D3DCLEAR_TARGET|(rb_graphics.offdepth?D3DCLEAR_ZBUFFER:0),0xff000000,1,0);
    if(SUCCEEDED(hr))hr=IDirect3DDevice9_BeginScene(d);
    if(FAILED(hr)){rb_graphics.failed=1;log_line("RB_GRAPHICS failed begin hr=%08lx",(unsigned long)hr);return 0;}
    ((void (__cdecl *)(void))0x47e6e0)();U32(0x4cd918)=255;
    ((void (TC *)(void *))0x47e730)(PTR(0x51f65c));U32(0x4cd918)=0;
    IDirect3DDevice9_SetRenderState(d,D3DRS_FOGENABLE,FALSE);
    rb_control_drawing=1;int result=scene_draw();rb_control_drawing=0;((void (TC *)(void *))0x47e730)(PTR(0x51f65c));IDirect3DDevice9_SetTexture(d,0,NULL);
    hr=IDirect3DDevice9_EndScene(d);++rb_graphics.draws;
    if(SUCCEEDED(hr))hr=IDirect3DDevice9_StretchRect(d,rb_graphics.offscreen,NULL,rb_graphics.resolved,NULL,D3DTEXF_NONE);
    if(FAILED(hr) || result<0)rb_graphics.failed=1;
    /* The real app sets its camera mode to2 before the next Root. */
    U32(0x4cd5e0)=2;
    return !rb_graphics.failed;
}
static int rb_graphics_leave_output(void) {
    D3DVIEWPORT9 viewport;IDirect3DDevice9 *d=rb_graphics.device;
    HRESULT hr=IDirect3DDevice9_GetViewport(d,&viewport);
    if(SUCCEEDED(hr))hr=IDirect3DDevice9_SetRenderTarget(d,0,rb_graphics.target);
    if(SUCCEEDED(hr))hr=IDirect3DDevice9_SetDepthStencilSurface(d,rb_graphics.depth);
    if(SUCCEEDED(hr))hr=IDirect3DDevice9_SetViewport(d,&viewport);return SUCCEEDED(hr);
}
static int rb_graphics_blit(void) {
    if(!rb_graphics.active || !rb_graphics.texture)return 0;
    IDirect3DDevice9 *d=rb_graphics.device;IDirect3DStateBlock9 *saved=NULL;
    HRESULT hr=IDirect3DDevice9_CreateStateBlock(d,D3DSBT_ALL,&saved);if(FAILED(hr))return 0;
    IDirect3DStateBlock9_Capture(saved);
    typedef struct {float x,y,z,rhw;DWORD color;float u,v;} V;
    float w=(float)rb_graphics.desc.Width,h=(float)rb_graphics.desc.Height;
    V q[4]={{-.5f,-.5f,0,1,0xffffffff,0,0},{w-.5f,-.5f,0,1,0xffffffff,1,0},{-.5f,h-.5f,0,1,0xffffffff,0,1},{w-.5f,h-.5f,0,1,0xffffffff,1,1}};
    D3DVIEWPORT9 viewport={0,0,rb_graphics.desc.Width,rb_graphics.desc.Height,0,1};
    IDirect3DDevice9_SetViewport(d,&viewport);IDirect3DDevice9_SetFVF(d,D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1);
    IDirect3DDevice9_SetVertexShader(d,NULL);IDirect3DDevice9_SetPixelShader(d,NULL);IDirect3DDevice9_SetTexture(d,0,(IDirect3DBaseTexture9 *)rb_graphics.texture);
    IDirect3DDevice9_SetRenderState(d,D3DRS_ZENABLE,FALSE);IDirect3DDevice9_SetRenderState(d,D3DRS_ZWRITEENABLE,FALSE);IDirect3DDevice9_SetRenderState(d,D3DRS_ALPHABLENDENABLE,FALSE);IDirect3DDevice9_SetRenderState(d,D3DRS_ALPHATESTENABLE,FALSE);IDirect3DDevice9_SetRenderState(d,D3DRS_FOGENABLE,FALSE);IDirect3DDevice9_SetRenderState(d,D3DRS_CULLMODE,D3DCULL_NONE);IDirect3DDevice9_SetRenderState(d,D3DRS_SCISSORTESTENABLE,FALSE);
    IDirect3DDevice9_SetTextureStageState(d,0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);IDirect3DDevice9_SetTextureStageState(d,0,D3DTSS_COLORARG1,D3DTA_TEXTURE);IDirect3DDevice9_SetTextureStageState(d,0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);IDirect3DDevice9_SetTextureStageState(d,0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);IDirect3DDevice9_SetTextureStageState(d,1,D3DTSS_COLOROP,D3DTOP_DISABLE);
    IDirect3DDevice9_SetTextureStageState(d,0,D3DTSS_TEXCOORDINDEX,0);IDirect3DDevice9_SetTextureStageState(d,0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE);
    IDirect3DDevice9_SetRenderState(d,D3DRS_COLORWRITEENABLE,15);IDirect3DDevice9_SetRenderState(d,D3DRS_SRGBWRITEENABLE,FALSE);IDirect3DDevice9_SetRenderState(d,D3DRS_FILLMODE,D3DFILL_SOLID);
    IDirect3DDevice9_SetSamplerState(d,0,D3DSAMP_SRGBTEXTURE,FALSE);IDirect3DDevice9_SetSamplerState(d,0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);IDirect3DDevice9_SetSamplerState(d,0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);
    IDirect3DDevice9_SetSamplerState(d,0,D3DSAMP_MINFILTER,D3DTEXF_POINT);IDirect3DDevice9_SetSamplerState(d,0,D3DSAMP_MAGFILTER,D3DTEXF_POINT);
    hr=IDirect3DDevice9_DrawPrimitiveUP(d,D3DPT_TRIANGLESTRIP,2,q,sizeof(V));IDirect3DStateBlock9_Apply(saved);IDirect3DStateBlock9_Release(saved);return SUCCEEDED(hr);
}
static void __cdecl rb_control_reset(void){if(!rb_control_active)((void (__cdecl *)(void))0x47e6e0)();}
static void TC rb_control_flush(void *manager){if(!rb_control_active)((void (TC *)(void *))0x47e730)(manager);}
static int rb_control_install(void) {
    const uintptr_t sites[]={0x471b95,0x471baa,0x471bd6};
    for(unsigned i=0;i<3;++i){uintptr_t at=sites[i],target=i?0x47e730:0x47e6e0;if(*(unsigned char *)at!=0xe8 || at+5+*(int32_t *)(at+1)!=target)return 0;}
    for(unsigned i=0;i<3;++i){uintptr_t at=sites[i];DWORD old;if(!VirtualProtect((void *)at,5,PAGE_EXECUTE_READWRITE,&old))return 0;
        *(int32_t *)(at+1)=(int32_t)((uintptr_t)(i?(void *)rb_control_flush:(void *)rb_control_reset)-at-5);FlushInstructionCache(GetCurrentProcess(),(void *)at,5);VirtualProtect((void *)at,5,old,&old);}
    return 1;
}
#endif
