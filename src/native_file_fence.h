#ifndef TH18_NATIVE_FILE_FENCE_H
#define TH18_NATIVE_FILE_FENCE_H
/* Native file operations are exact-input lifecycle boundaries. In particular
   CREATE_ALWAYS must be stopped before truncation, not merely at WriteFile.
   Normal/private-profile file semantics remain native on admitted frames. */
static struct {
    HANDLE (WINAPI *open_a)(LPCSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE);
    HANDLE (WINAPI *open_w)(LPCWSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE);
    BOOL (WINAPI *read)(HANDLE,LPVOID,DWORD,LPDWORD,LPOVERLAPPED);
    DWORD (WINAPI *seek)(HANDLE,LONG,PLONG,DWORD);
    BOOL (WINAPI *seek_ex)(HANDLE,LARGE_INTEGER,PLARGE_INTEGER,DWORD);
    BOOL (WINAPI *remove)(LPCSTR);BOOL (WINAPI *flush)(HANDLE);
} rb_files;
static HANDLE WINAPI rb_file_open_a(LPCSTR path,DWORD access,DWORD share,LPSECURITY_ATTRIBUTES security,DWORD disposition,DWORD flags,HANDLE template_file) {
    if(rb_request_precise()){SetLastError(ERROR_RETRY);return INVALID_HANDLE_VALUE;}
    return rb_files.open_a(path,access,share,security,disposition,flags,template_file);
}
static HANDLE WINAPI rb_file_open_w(LPCWSTR path,DWORD access,DWORD share,LPSECURITY_ATTRIBUTES security,DWORD disposition,DWORD flags,HANDLE template_file) {
    if(rb_request_precise()){SetLastError(ERROR_RETRY);return INVALID_HANDLE_VALUE;}
    return rb_files.open_w(path,access,share,security,disposition,flags,template_file);
}
static BOOL WINAPI rb_file_read(HANDLE file,LPVOID buffer,DWORD size,LPDWORD read,LPOVERLAPPED overlapped) {
    if(rb_request_precise()){if(read)*read=0;SetLastError(ERROR_RETRY);return FALSE;}return rb_files.read(file,buffer,size,read,overlapped);
}
static DWORD WINAPI rb_file_seek(HANDLE file,LONG offset,PLONG high,DWORD method) {
    if(rb_request_precise()){SetLastError(ERROR_RETRY);return INVALID_SET_FILE_POINTER;}return rb_files.seek(file,offset,high,method);
}
static BOOL WINAPI rb_file_seek_ex(HANDLE file,LARGE_INTEGER offset,PLARGE_INTEGER position,DWORD method) {
    if(rb_request_precise()){SetLastError(ERROR_RETRY);return FALSE;}return rb_files.seek_ex(file,offset,position,method);
}
static BOOL WINAPI rb_file_remove(LPCSTR path){if(rb_request_precise()){SetLastError(ERROR_RETRY);return FALSE;}return rb_files.remove(path);}
static BOOL WINAPI rb_file_flush(HANDLE file){if(rb_request_precise()){SetLastError(ERROR_RETRY);return FALSE;}return rb_files.flush(file);}
static int rb_files_install(void) {
    if(!rb_runtime_enabled)return 1;
    const uintptr_t addresses[]={0x4ad084,0x4ad19c,0x4ad090,0x4ad0a4,0x4ad180,0x4ad0c0,0x4ad18c};
    const char *names[]={"CreateFileA","CreateFileW","ReadFile","SetFilePointer","SetFilePointerEx","DeleteFileA","FlushFileBuffers"};
    void *replacements[]={(void *)rb_file_open_a,(void *)rb_file_open_w,(void *)rb_file_read,(void *)rb_file_seek,(void *)rb_file_seek_ex,(void *)rb_file_remove,(void *)rb_file_flush};
    void **originals[]={ (void **)&rb_files.open_a,(void **)&rb_files.open_w,(void **)&rb_files.read,(void **)&rb_files.seek,(void **)&rb_files.seek_ex,(void **)&rb_files.remove,(void **)&rb_files.flush};
    /* Chain the game's current imports, including an already-installed
       translation/resource loader. Do not replace its texture/file routing
       with raw kernel calls. The fixed EXE hash identifies these IAT slots. */
    for(unsigned i=0;i<7;++i){MEMORY_BASIC_INFORMATION region;*originals[i]=PTR(addresses[i]);
        if(!*originals[i] || *originals[i]==replacements[i] || !VirtualQuery(*originals[i],&region,sizeof(region)) || region.State!=MEM_COMMIT ||
           (region.Protect&PAGE_GUARD) || !(region.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))){log_line("Rollback file import is not callable: %s",names[i]);return 0;}}
    for(unsigned i=0;i<7;++i)if(!rb_audit_swap(addresses[i],*originals[i],replacements[i]))return 0;
    uintptr_t geometry=0x40f4de;DWORD old;
    if(*(unsigned char *)geometry!=0xe8 || geometry+5+*(int32_t *)(geometry+1)!=0x448f10)return 0;
    if(!VirtualProtect((void *)geometry,5,PAGE_EXECUTE_READWRITE,&old))return 0;
    *(int32_t *)(geometry+1)=(int32_t)((uintptr_t)rb_rect_zero_z_bridge-geometry-5);
    FlushInstructionCache(GetCurrentProcess(),(void *)geometry,5);VirtualProtect((void *)geometry,5,old,&old);
    /* The compiler also inlined the rectangle query in420e..: width/height
       are ESP+38/+3c, but its copied ESP+40 Z lane is uninitialized too.
       The following TEST edx overwrites flags before any consumer. */
    const unsigned char inline_rect[]={0x8b,0x44,0x24,0x40,0x89,0x81,0xb4,0x07,0x00,0x00};
    if(memcmp((void *)0x420ef1,inline_rect,sizeof(inline_rect)))return 0;
    if(!VirtualProtect((void *)0x420ef1,4,PAGE_EXECUTE_READWRITE,&old))return 0;
    memcpy((void *)0x420ef1,"\x31\xc0\x90\x90",4);FlushInstructionCache(GetCurrentProcess(),(void *)0x420ef1,4);VirtualProtect((void *)0x420ef1,4,old,&old);
    return 1;
}
#endif
