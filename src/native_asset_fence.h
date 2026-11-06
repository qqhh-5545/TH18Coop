#ifndef TH18_NATIVE_ASSET_FENCE_H
#define TH18_NATIVE_ASSET_FENCE_H
/* Native ANM textures are external COM resources. A late HeapFree fence is
   insufficient:486fc0 has already released the image's COM pointer at487031.
   Fence the complete operation before any native resource side effect. */
static struct {void *load,*upload,*unload;int installed;} rb_asset_io;
static void *TC rb_asset_load(void *manager,int slot,const char *path){
    if(rb_request_precise())return NULL;
    return ((void *(TC *)(void *,int,const char *))rb_asset_io.load)(manager,slot,path);
}
static int TC rb_asset_upload(void *manager,void *file){
    if(rb_request_precise())return 0;
    return ((int(TC *)(void *,void *))rb_asset_io.upload)(manager,file);
}
static void TC rb_asset_unload(void *manager,int slot){
    if(rb_request_precise())return;
    ((void(TC *)(void *,int))rb_asset_io.unload)(manager,slot);
}
static int rb_assets_install(void){
    if(rb_asset_io.installed)return 1;
    const unsigned char load[]={0x55,0x8b,0xec,0x83,0xec,0x08};
    const unsigned char upload[]={0x55,0x8b,0xec,0x83,0xe4,0xf8};
    const unsigned char unload[]={0x55,0x8b,0xec,0x51,0x8b,0x45,0x08};
    rb_asset_io.load=trampoline(0x4867e0,sizeof(load),load);
    rb_asset_io.upload=trampoline(0x486920,sizeof(upload),upload);
    rb_asset_io.unload=trampoline(0x486fc0,sizeof(unload),unload);
    if(!rb_asset_io.load || !rb_asset_io.upload || !rb_asset_io.unload)return 0;
    if(!patch_branch(0x4867e0,rb_asset_load,sizeof(load),0xe9,load) ||
       !patch_branch(0x486920,rb_asset_upload,sizeof(upload),0xe9,upload) ||
       !patch_branch(0x486fc0,rb_asset_unload,sizeof(unload),0xe9,unload))return 0;
    rb_asset_io.installed=1;log_line("ROLLBACK native ANM load/upload/unload boundaries installed on owning game thread");return 1;
}
#endif
