#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <wincrypt.h>
#include "data_profile.h"

static int profile_matches(const wchar_t *path) {
    static const BYTE expected[32]={0x5a,0xeb,0x74,0xb1,0x99,0x39,0xa2,0x9a,0x8c,0xf0,0x6e,0x4f,0xe7,0x77,0x7a,0xaf,0x91,0x39,0xe9,0x34,0x78,0x50,0x7e,0x03,0x9f,0x43,0x31,0x81,0x71,0x02,0x0e,0x9f};
    HCRYPTPROV provider=0;HCRYPTHASH hash=0;BYTE digest[32],buffer[65536];DWORD size=32,n;
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);
    if(file==INVALID_HANDLE_VALUE) return 0;
    int okay=CryptAcquireContextW(&provider,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT) && CryptCreateHash(provider,CALG_SHA_256,0,0,&hash);
    while(okay) {if(!ReadFile(file,buffer,sizeof(buffer),&n,NULL)){okay=0;break;}if(!n)break;okay=CryptHashData(hash,buffer,n,0);}
    if(okay) okay=CryptGetHashParam(hash,HP_HASHVAL,digest,&size,0) && size==32 && !memcmp(digest,expected,32);
    if(hash)CryptDestroyHash(hash);if(provider)CryptReleaseContext(provider,0);CloseHandle(file);return okay;
}

static int remote_data_matches(HANDLE process,const wchar_t *path) {
    char actual[4096];SIZE_T size=0;
    return ReadProcessMemory(process,(void *)0x568c61,actual,sizeof(actual),&size) && size==sizeof(actual) && data_path_matches(actual,path);
}

int wmain(int argc,wchar_t **argv) {
    if(argc==3 && !wcscmp(argv[1],L"--check-profile")) {
        int okay=game_target_allowed(argv[2]) && profile_matches(argv[2]);
        wprintf(L"%ls\n",okay?L"Verified selected game core":L"Rejected target path or build");return okay?0:1;
    }
    if(argc<3) {fwprintf(stderr,L"Usage: TH18LocalCoop.exe --attach PID [DLL] | --activate PID\n");return 2;}
    DWORD pid=wcstoul(argv[2],NULL,10);
    if(!pid) return 2;
    if(!wcscmp(argv[1],L"--activate")) {
        wchar_t active_path[MAX_PATH];DWORD active_size=MAX_PATH;
        HANDLE active_process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_VM_READ,FALSE,pid);
        int active_ok=active_process && QueryFullProcessImageNameW(active_process,0,active_path,&active_size) &&
            game_target_allowed(active_path) && profile_matches(active_path) && remote_data_matches(active_process,active_path);
        if(active_process)CloseHandle(active_process);
        if(!active_ok) {fwprintf(stderr,L"Activation rejected: target is outside the configured game directory.\n");return 1;}
        wchar_t name[100]; swprintf(name,100,L"Local\\TH18LocalCoop-%lu-activate",(unsigned long)pid);
        HANDLE event=OpenEventW(EVENT_MODIFY_STATE,FALSE,name);
        if(!event) {fwprintf(stderr,L"Co-op loader is not ready in PID %lu (error %lu).\n",pid,GetLastError());return 1;}
        SetEvent(event);CloseHandle(event);wprintf(L"Requested P2 activation on the next live gameplay update.\n");return 0;
    }
    if(wcscmp(argv[1],L"--attach")) return 2;
    wchar_t dll[MAX_PATH],process_path[MAX_PATH];
    if(argc>3) GetFullPathNameW(argv[3],MAX_PATH,dll,NULL);
    else {GetModuleFileNameW(NULL,dll,MAX_PATH);wchar_t *s=wcsrchr(dll,L'\\');wcscpy(s+1,L"th18_coop.dll");}
    HANDLE process=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_CREATE_THREAD|PROCESS_VM_OPERATION|PROCESS_VM_WRITE|PROCESS_VM_READ,FALSE,pid);
    if(!process) {fwprintf(stderr,L"OpenProcess failed: %lu\n",GetLastError());return 1;}
    DWORD len=MAX_PATH; BOOL wow=FALSE;
    if(!QueryFullProcessImageNameW(process,0,process_path,&len) ||
        !wcsrchr(process_path,L'\\') || _wcsicmp(wcsrchr(process_path,L'\\')+1,L"th18.exe") ||
        !IsWow64Process(process,&wow) || !wow) {
        fwprintf(stderr,L"Target must be a running 32-bit th18.exe.\n");CloseHandle(process);return 1;
    }
    if(GetFileAttributesW(dll)==INVALID_FILE_ATTRIBUTES) {fwprintf(stderr,L"DLL missing: %ls\n",dll);CloseHandle(process);return 1;}
    if(!game_target_allowed(process_path) || !profile_matches(process_path)) {fwprintf(stderr,L"Unsupported game path or core build; Steam is excluded.\n");CloseHandle(process);return 1;}
    if(!remote_data_matches(process,process_path)) {
        fwprintf(stderr,L"Game is not using the private co-op data directory. Exit and start through the co-op launcher.\n");CloseHandle(process);return 1;
    }
    wchar_t ready_name[100];swprintf(ready_name,100,L"Local\\TH18LocalCoop-%lu-activate",(unsigned long)pid);
    HANDLE ready=OpenEventW(EVENT_MODIFY_STATE,FALSE,ready_name);
    if(ready) {CloseHandle(ready);CloseHandle(process);wprintf(L"Extension already loaded.\n");return 0;}
    uint32_t player=0;SIZE_T read=0;
    if(!ReadProcessMemory(process,(void *)0x4cf410,&player,4,&read) || read!=4) {fwprintf(stderr,L"Game is still starting. Try again when the title screen appears.\n");CloseHandle(process);return 1;}
    if(player) {fwprintf(stderr,L"Return to the game title screen before attaching this experiment.\n");CloseHandle(process);return 1;}
    SIZE_T bytes=(wcslen(dll)+1)*sizeof(wchar_t),written=0;
    void *remote=VirtualAllocEx(process,NULL,bytes,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!remote || !WriteProcessMemory(process,remote,dll,bytes,&written) || written!=bytes) {
        fwprintf(stderr,L"Cannot prepare DLL path: %lu\n",GetLastError());if(remote)VirtualFreeEx(process,remote,0,MEM_RELEASE);CloseHandle(process);return 1;
    }
    HANDLE thread=CreateRemoteThread(process,NULL,0,(LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"LoadLibraryW"),remote,0,NULL);
    if(!thread) {fwprintf(stderr,L"Cannot load extension: %lu\n",GetLastError());VirtualFreeEx(process,remote,0,MEM_RELEASE);CloseHandle(process);return 1;}
    DWORD wait=WaitForSingleObject(thread,10000),result=0;
    if(wait==WAIT_OBJECT_0) {GetExitCodeThread(thread,&result);VirtualFreeEx(process,remote,0,MEM_RELEASE);}
    CloseHandle(thread);CloseHandle(process);
    if(wait!=WAIT_OBJECT_0 || !result) {fwprintf(stderr,L"Extension did not finish loading; see coop.log if present.\n");return 1;}
    wprintf(L"Extension loaded in PID %lu. F7 in live gameplay activates experimental P2.\n",pid);
    return 0;
}
