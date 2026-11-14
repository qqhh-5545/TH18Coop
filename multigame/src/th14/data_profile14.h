#ifndef COOP_DATA_PROFILE_H
#define COOP_DATA_PROFILE_H
#include <windows.h>
#include <wchar.h>
#include <string.h>
static int game_target_allowed(const wchar_t *path) {
    size_t n=wcslen(path);if(n>=170)return 0;
    const wchar_t *slash=wcsrchr(path,L'\\');if(!slash || _wcsicmp(slash+1,L"th14.exe"))return 0;
    for(const wchar_t *p=path;*p;++p)if(!_wcsnicmp(p,L"\\steamapps\\",11))return 0;
    wchar_t sibling[MAX_PATH];wcscpy(sibling,path);wchar_t *base=wcsrchr(sibling,L'\\')+1;
    const wchar_t *names[]={L"steam_appid.txt",L"steam_api.dll",L"steam_api64.dll"};
    for(unsigned i=0;i<3;++i){wcscpy(base,names[i]);if(GetFileAttributesW(sibling)!=INVALID_FILE_ATTRIBUTES)return 0;}
    return 1;
}
static int data_path_matches_ansi(const char *actual,const wchar_t *game_exe) {
    wchar_t expected[MAX_PATH];char encoded[4*MAX_PATH];
    if(wcslen(game_exe)>=MAX_PATH) return 0;
    wcscpy(expected,game_exe);wchar_t *slash=wcsrchr(expected,L'\\');
    if(!slash) return 0;
    const wchar_t *suffix=L"\\coop-data\\ShanghaiAlice\\th14";
    if((size_t)(slash-expected)+wcslen(suffix)>=MAX_PATH) return 0;
    wcscpy(slash,suffix);
    if(!WideCharToMultiByte(CP_ACP,0,expected,-1,encoded,sizeof(encoded),NULL,NULL)) return 0;
    size_t len=strlen(encoded),n=0;while(n<4096 && actual[n]) ++n;
    if(n==4096) return 0;
    while(n && (actual[n-1]=='\\' || actual[n-1]=='/')) --n;
    if(n==len && !_strnicmp(actual,encoded,len))return 1;
    /* Online sessions are private descendants, never an arbitrary APPDATA. */
    wcscpy(expected,game_exe);slash=wcsrchr(expected,L'\\');
    if((size_t)(slash-expected)+22>=MAX_PATH)return 0;
    wcscpy(slash,L"\\coop-data\\sessions\\");
    if(!WideCharToMultiByte(CP_ACP,0,expected,-1,encoded,sizeof(encoded),NULL,NULL))return 0;
    len=strlen(encoded);
    int viewer=0;
    if(n<len+32 || _strnicmp(actual,encoded,len)) {
        wcscpy(slash,L"\\coop-data\\replay-sessions\\");
        if(!WideCharToMultiByte(CP_ACP,0,expected,-1,encoded,sizeof(encoded),NULL,NULL))return 0;
        len=strlen(encoded);viewer=1;
        if(n<len+32 || _strnicmp(actual,encoded,len))return 0;
    }
    for(size_t i=0;i<32;++i) {
        char c=actual[len+i];if(!((c>='0' && c<='9')||(c>='a' && c<='f')))return 0;
    }
    const char *tail=actual+len+32;
    const char *host="\\host\\ShanghaiAlice\\th14",*guest="\\guest\\ShanghaiAlice\\th14",*third="\\guest-p3\\ShanghaiAlice\\th14";
    size_t left=n-len-32;
    if(viewer){const char *watch="\\viewer\\ShanghaiAlice\\th14";return left==strlen(watch)&&!_strnicmp(tail,watch,left);}
    return (left==strlen(host) && !_strnicmp(tail,host,left)) || (left==strlen(guest) && !_strnicmp(tail,guest,left)) || (left==strlen(third) && !_strnicmp(tail,third,left));
}
static int data_path_matches(const char *actual,const wchar_t *game_exe) {
    if(data_path_matches_ansi(actual,game_exe))return 1;
    // thcrap's Unicode shim stores UTF-8 instead of the native ACP path.
    size_t n=0;while(n<4096 && actual[n])++n;if(n==4096)return 0;
    wchar_t decoded[4096];char ansi[4096];BOOL substituted=FALSE;
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,actual,-1,decoded,4096))return 0;
    if(!WideCharToMultiByte(CP_ACP,WC_NO_BEST_FIT_CHARS,decoded,-1,ansi,4096,NULL,&substituted) || substituted)return 0;
    return data_path_matches_ansi(ansi,game_exe);
}
#endif
