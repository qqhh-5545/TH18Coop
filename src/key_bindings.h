#ifndef COOP_KEY_BINDINGS_H
#define COOP_KEY_BINDINGS_H
/* Physical bindings are local preferences, never synchronized game rules. */
static unsigned kb_keys[3][9]={{VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT,'Z',VK_SHIFT,'X','C','D'}, {'I','K','J','L','G','F','H','U','O'}, {VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT,'Z',VK_SHIFT,'X','C','D'}};
static int kb_native[3]={1,0,1};
static uint32_t kb_previous;
static const uint32_t kb_masks[9]={0x10,0x20,0x40,0x80,1,8,2,0x400,0x800};
static uint32_t kb_sample(unsigned profile,SHORT (WINAPI *read_key)(int)) {
    uint32_t held=0;for(unsigned i=0;i<9;++i)if(read_key((int)kb_keys[profile][i])&0x8000)held|=kb_masks[i];return held;
}
static uint32_t kb_online(uint32_t native_input,SHORT (WINAPI *read_key)(int)) {
    return kb_native[2]?native_input:(native_input&~0xcfbu)|kb_sample(2,read_key);
}
static void kb_load(const wchar_t *config) {
    wchar_t path[MAX_PATH];wcscpy(path,config);wchar_t *slash=wcsrchr(path,L'\\');if(!slash)return;wcscpy(slash+1,L"keybindings.ini");
    const wchar_t *sections[]={L"LocalP1",L"LocalP2",L"Online"};
    const wchar_t *names[]={L"Up",L"Down",L"Left",L"Right",L"Shoot",L"Focus",L"Bomb",L"Card",L"Cycle"};
    for(unsigned p=0;p<3;++p){unsigned codes[9];int okay=1;
        for(unsigned i=0;i<9;++i){codes[i]=GetPrivateProfileIntW(sections[p],names[i],kb_keys[p][i],path);if(codes[i]<8 || codes[i]>254 || codes[i]==VK_ESCAPE || codes[i]==VK_RETURN)okay=0;for(unsigned j=0;j<i;++j)if(codes[i]==codes[j])okay=0;}
        if(okay){memcpy(kb_keys[p],codes,sizeof(codes));kb_native[p]=p==1?0:GetPrivateProfileIntW(sections[p],L"UseNative",1,path)!=0;}
    }
}
static void kb_local(void *input) {
    if(kb_native[0])return;
    uint32_t held=FIELD(input,0)&~0xcfbu;
    if(GetForegroundWindow()==(HWND)(uintptr_t)U32(0x568c30))held|=kb_sample(0,GetAsyncKeyState);
    FIELD(input,0)=held;FIELD(input,4)=kb_previous;kb_previous=held;
}
#endif
