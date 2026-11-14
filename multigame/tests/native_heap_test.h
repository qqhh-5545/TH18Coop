/* Exercise the exact native allocation wrappers against a private process
   heap. No IAT patch, game process or file output is involved. */
static unsigned native_heap_physical_frees;
static unsigned native_asset_calls[3];
static void *TC native_asset_load_probe(void *manager,int slot,const char *path){CHECK(manager==(void *)0x1234 && slot==7 && !strcmp(path,"fixture.anm"));++native_asset_calls[0];return (void *)0x5678;}
static int TC native_asset_upload_probe(void *manager,void *file){CHECK(manager==(void *)0x1234 && file==(void *)0x5678);++native_asset_calls[1];return 7;}
static void TC native_asset_unload_probe(void *manager,int slot){CHECK(manager==(void *)0x1234 && slot==7);++native_asset_calls[2];}
static void test_native_asset_fence(void){
    rb_asset_io.load=native_asset_load_probe;rb_asset_io.upload=native_asset_upload_probe;rb_asset_io.unload=native_asset_unload_probe;
    rb_runtime_speculating=1;rb_runtime_barrier=0;
    CHECK(!rb_asset_load((void *)0x1234,7,"fixture.anm") && rb_runtime_barrier);
    CHECK(!rb_asset_upload((void *)0x1234,(void *)0x5678));rb_asset_unload((void *)0x1234,7);
    CHECK(!native_asset_calls[0] && !native_asset_calls[1] && !native_asset_calls[2]);
    rb_runtime_speculating=rb_runtime_barrier=0;
    CHECK(rb_asset_load((void *)0x1234,7,"fixture.anm")== (void *)0x5678);
    CHECK(rb_asset_upload((void *)0x1234,(void *)0x5678)==7);rb_asset_unload((void *)0x1234,7);
    CHECK(native_asset_calls[0]==1 && native_asset_calls[1]==1 && native_asset_calls[2]==1 && !rb_runtime_barrier);
    memset(&rb_asset_io,0,sizeof(rb_asset_io));puts("PASS ANM resource fence: speculative load/upload/unload blocked before COM/heap effects; accurate calls preserve native arguments, results and once-only disposal");
}
static BOOL WINAPI native_stream_read_probe(HANDLE file,LPVOID buffer,DWORD size,LPDWORD read,LPOVERLAPPED overlapped){(void)file;(void)buffer;(void)overlapped;*read=size;return TRUE;}
static DWORD WINAPI native_stream_thread_probe(void *unused){(void)unused;DWORD read=0;return rb_file_read(INVALID_HANDLE_VALUE,NULL,4,&read,NULL) && read==4;}
static void test_native_world_digest(void){
    void *a=VirtualAlloc(NULL,15121192,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE),*b=VirtualAlloc(NULL,15121192,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);CHECK(a && b);
    const uint32_t seed=2166136261u;
    FIELD(a,0xc0)=(uint32_t)(uintptr_t)OFF(a,0xfc);FIELD(a,0xfc)=(uint32_t)(uintptr_t)OFF(a,0xec);
    FIELD(b,0xc0)=(uint32_t)(uintptr_t)OFF(b,0xfc);FIELD(b,0xfc)=(uint32_t)(uintptr_t)OFF(b,0xec);
    CHECK(online_hash_bullets(seed,a)==online_hash_bullets(seed,b));
    FIELD(b,0x5000)=123;CHECK(online_hash_bullets(seed,a)==online_hash_bullets(seed,b));
    FIELD(b,0xec+0x638)=1;CHECK(online_hash_bullets(seed,a)!=online_hash_bullets(seed,b));
    memset(a,0,15121192);memset(b,0,15121192);
    FIELD(a,0x14+0xc74)=FIELD(b,0x14+0xc74)=4;
    CHECK(online_hash_items(seed,a)==online_hash_items(seed,b));
    FIELD(b,0x14+0xc94+0xc30)=99;CHECK(online_hash_items(seed,a)==online_hash_items(seed,b));
    FIELD(b,0x14+0xc30)=1;CHECK(online_hash_items(seed,a)!=online_hash_items(seed,b));
    memset(a,0,15121192);memset(b,0,15121192);
    FIELD(a,0x18c)=(uint32_t)(uintptr_t)OFF(a,0x300);FIELD(a,0x300)=(uint32_t)(uintptr_t)OFF(a,0x400);
    FIELD(b,0x18c)=(uint32_t)(uintptr_t)OFF(b,0x300);FIELD(b,0x300)=(uint32_t)(uintptr_t)OFF(b,0x400);
    FIELD(OFF(a,0x400),0x6830)=FIELD(OFF(b,0x400),0x6830)=19;
    CHECK(online_hash_enemies(seed,a)==online_hash_enemies(seed,b));
    FIELD(OFF(b,0x400),0x6220)=100;CHECK(online_hash_enemies(seed,a)!=online_hash_enemies(seed,b));
    VirtualFree(a,0,MEM_RELEASE);VirtualFree(b,0,MEM_RELEASE);
    puts("PASS native world digest: independent addresses agree; active bullet movement, item movement and enemy HP divergence detected; inactive pool padding ignored");
}
static unsigned native_graphics_releases,native_graphics_resets;
static ULONG WINAPI native_graphics_release_probe(IUnknown *object){(void)object;++native_graphics_releases;return 0;}
static HRESULT WINAPI native_graphics_reset_probe(IDirect3DDevice9 *device,D3DPRESENT_PARAMETERS *parameters){
    CHECK(device==rb_device_reset.device && parameters->BackBufferWidth==640);
    CHECK(!rb_graphics.active && !rb_graphics.state && !rb_graphics.target && !rb_graphics.depth && !rb_graphics.texture && !rb_control_active);
    ++native_graphics_resets;return native_graphics_resets==1?D3DERR_DEVICELOST:S_OK;
}
static void test_native_graphics_reset(void){
    IDirect3DDevice9Vtbl table={0};table.Reset=native_graphics_reset_probe;IDirect3DDevice9 device={&table};
    IUnknownVtbl release_table={0};release_table.Release=native_graphics_release_probe;IUnknown object={&release_table};
    CHECK(rb_graphics_hook_reset(&device) && device.lpVtbl!=&table && rb_graphics_hook_reset(&device));
    CHECK(rb_device_reset.original==&table && device.lpVtbl->Present==table.Present);
    rb_graphics.state=(IDirect3DStateBlock9 *)&object;rb_graphics.target=rb_graphics.depth=rb_graphics.offscreen=rb_graphics.offdepth=rb_graphics.resolved=(IDirect3DSurface9 *)&object;
    rb_graphics.texture=(IDirect3DTexture9 *)&object;rb_graphics.active=rb_control_active=1;
    D3DPRESENT_PARAMETERS parameters={0};parameters.BackBufferWidth=640;
    rb_runtime_speculating=1;rb_runtime_barrier=0;
    CHECK(IDirect3DDevice9_Reset(&device,&parameters)==D3DERR_DEVICELOST && rb_runtime_barrier && !native_graphics_resets && !native_graphics_releases && rb_graphics.active);
    rb_runtime_speculating=rb_runtime_barrier=0;
    CHECK(IDirect3DDevice9_Reset(&device,&parameters)==D3DERR_DEVICELOST && native_graphics_releases==7 && native_graphics_resets==1);
    CHECK(IDirect3DDevice9_Reset(&device,&parameters)==S_OK && native_graphics_releases==7 && native_graphics_resets==2);
    rb_graphics_unhook_reset();CHECK(device.lpVtbl==&table && !rb_device_reset.device);
    puts("PASS renderer reset: instance-only vtable, existing proxy preserved, speculative Reset blocked, all private resources released once before native retry");
}
static BOOL WINAPI native_heap_count_release(HANDLE heap,DWORD flags,LPVOID pointer) {
    BOOL result=HeapFree(heap,flags,pointer);if(result && pointer)++native_heap_physical_frees;return result;
}
static int __attribute__((stdcall)) native_geometry_probe(const float *position,const float *size,unsigned a,unsigned b) {
    CHECK(position[0]==11 && position[1]==22 && position[2]==33);
    CHECK(size[0]==32 && size[1]==123 && size[2]==0 && a==0x12 && b==0x34);return 7;
}
static void test_native_heap_transaction(void) {
    test_native_asset_fence();
    unsigned char *regions=VirtualAlloc(NULL,8192,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);DWORD protection;
    CHECK(regions);void *scheduler=calloc(1,0x58),*node=calloc(1,8),*callback=regions+4096-20;CHECK(scheduler && node);
    FIELD(scheduler,0x18)=(uint32_t)(uintptr_t)node;FIELD(node,0)=(uint32_t)(uintptr_t)callback;FIELD(callback,8)=0x453460;
    CHECK(VirtualProtect(regions+4096,4096,PAGE_READONLY,&protection));
    CHECK(valid(regions+4095,2) && valid(regions,8192));
    CHECK(online_has_game_thread(scheduler)); /* Never bypass the network gate because a callback spans regions. */
    CHECK(VirtualProtect(regions+4096,4096,PAGE_NOACCESS,&protection) && !valid(regions+4095,2));
    free(scheduler);free(node);VirtualFree(regions,0,MEM_RELEASE);
    // The sparse production scan must cover exactly the same global words
    // as the independent full-range oracle, for both qualification modes.
    for(int runtime=0;runtime<2;++runtime){rb_runtime_enabled=runtime;int equal=1;
        uintptr_t begin=rb_global_spans[0].begin-0xa000,end=rb_global_spans[11].end+4096;
        for(uintptr_t at=begin;at<end;at+=4){int covered=0;for(unsigned i=0;i<sizeof(rb_global_spans)/sizeof(rb_global_spans[0]);++i)
            if(rb_global_span_active(&rb_global_spans[i]) && at>=rb_global_spans[i].begin && at<rb_global_spans[i].end)covered=1;
            if(covered!=!!rb_probe_global_owned(at))equal=0;}CHECK(equal);}
    rb_runtime_enabled=0;
    test_native_world_digest();
    test_native_graphics_reset();
    stub(0x448f10,native_geometry_probe);float origin[3]={11,22,33},rectangle[3]={32,123,NAN};
    typedef int (__attribute__((stdcall)) *Geometry)(const float *,const float *,unsigned,unsigned);
    CHECK(((Geometry)rb_rect_zero_z_bridge)(origin,rectangle,0x12,0x34)==7 && rectangle[2]==0);
    memset(&rb_audit,0,sizeof(rb_audit));rb_audit.heap=GetProcessHeap();rb_audit.allocate=HeapAlloc;rb_audit.release=native_heap_count_release;rb_audit.resize=HeapReAlloc;rb_audit.size=HeapSize;
    rb_audit.entries=VirtualAlloc(NULL,65536*sizeof(RbHeapEntry),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);CHECK(rb_audit.entries);
    InitializeCriticalSection(&rb_audit.lock);rb_audit.enabled=rb_audit.tracking=1;
    unsigned char *a=rb_audit_allocate(rb_audit.heap,HEAP_ZERO_MEMORY,32);CHECK(a);memset(a,1,32);
    // HeapWalk may precede publication of an already-allocated native root.
    // Only its exact live pointer/size can promote the initial observation.
    rb_audit.entries[0].observed=0;
    CHECK(!rb_audit_adopt_pool(a,31) && !rb_audit.entries[0].observed);
    CHECK(rb_audit_adopt_pool(a,32) && rb_audit.entries[0].observed==2);
    CHECK(!rb_audit_adopt_pool(a+1,31));
    rb_probe.qualify=1;rb_tx_begin();CHECK(rb_tx.mode==1 && !rb_tx.failed);
    unsigned char *b=rb_audit_allocate(rb_audit.heap,HEAP_ZERO_MEMORY,64),*c=rb_audit_allocate(rb_audit.heap,0,16);CHECK(b && c);
    unsigned char original_c[16];memcpy(original_c,c,16);memset(b,42,64);memset(c,8,16);memset(a,99,32);
    CHECK(rb_audit_release(rb_audit.heap,0,a));CHECK(rb_audit_release(rb_audit.heap,0,b));rb_tx_seal();
    CHECK(a[0]==99 && b[0]==42 && rb_tx.count==4 && rb_audit.entries[0].held && rb_audit.entries[1].held);
    for(unsigned i=0;i<rb_audit.count;++i){rb_audit.entries[i].live=i<rb_tx.before_count?rb_tx.live[i]:0;if(i>=rb_tx.before_count)rb_audit.entries[i].held=1;}
    memset(a,1,32);rb_audit.frees=rb_tx.before_frees;rb_tx.mode=2;rb_tx.cursor=0;
    CHECK(rb_audit_allocate(rb_audit.heap,HEAP_ZERO_MEMORY,64)==b);for(unsigned i=0;i<64;++i)CHECK(!b[i]);
    CHECK(rb_audit_allocate(rb_audit.heap,0,16)==c && !memcmp(c,original_c,16));memset(b,42,64);memset(c,8,16);memset(a,99,32);
    CHECK(rb_audit_release(rb_audit.heap,0,a));CHECK(rb_audit_release(rb_audit.heap,0,b));
    CHECK(!rb_tx.failed && rb_tx.cursor==4 && rb_audit.frees==rb_tx.after_frees && rb_audit.count==rb_tx.after_count);
    unsigned released=native_heap_physical_frees;rb_tx_commit();CHECK(!rb_tx.failed && !rb_audit.entries[0].live && !rb_audit.entries[0].held && !rb_audit.entries[1].held && rb_audit.entries[2].live && c[0]==8);
    CHECK(native_heap_physical_frees==released+2);rb_tx_commit();CHECK(native_heap_physical_frees==released+2);
    CHECK(rb_audit_release(rb_audit.heap,0,c));
    // A future frame kills an object still alive at the admitted first step,
    // and births another object that never existed at that first step. Undo
    // the whole window: retain the former, discard only the future birth.
    a=rb_audit_allocate(rb_audit.heap,HEAP_ZERO_MEMORY,32);CHECK(a);rb_tx_begin();a[0]=11;
    unsigned first_count=rb_audit.count,first_frees=rb_audit.frees;
    b=rb_audit_allocate(rb_audit.heap,0,16);CHECK(b);b[0]=25;CHECK(rb_audit_release(rb_audit.heap,0,a));rb_tx_seal();
    CHECK(rb_tx.count==2 && rb_audit.entries[first_count-1].held && !rb_audit.entries[first_count-1].live);
    rb_probe_reset_lifetimes();a[0]=11;
    CHECK(rb_audit.entries[first_count-1].live && !rb_audit.entries[first_count].live && rb_audit.frees==first_frees);
    released=native_heap_physical_frees;rb_tx_commit();CHECK(native_heap_physical_frees==released+1 && a[0]==11);
    rb_tx_commit();CHECK(native_heap_physical_frees==released+1);rb_audit.count=first_count;
    CHECK(rb_audit_release(rb_audit.heap,0,a));
    // Two corrections can return to the same retained checkpoint. A birth
    // abandoned and physically freed by the first correction still has a
    // registry index: the second undo must not resurrect its quarantine and
    // free that address a second time (including allocator address reuse).
    a=rb_audit_allocate(rb_audit.heap,HEAP_ZERO_MEMORY,32);CHECK(a);
    CHECK(rb_init(&rb_live.journal,2,256,8));RbNativeFrame *retained=&rb_live.frames[0];
    retained->frame=0;retained->before_count=rb_audit.count;retained->before_frees=rb_audit.frees;
    retained->live=VirtualAlloc(NULL,65536,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);CHECK(retained->live);
    for(unsigned i=0;i<rb_audit.count;++i)retained->live[i]=(unsigned char)rb_audit.entries[i].live;
    released=native_heap_physical_frees;
    for(unsigned correction=0;correction<2;++correction){CHECK(rb_begin(&rb_live.journal,0) && rb_touch(&rb_live.journal,a,32) && rb_end(&rb_live.journal));a[0]=55;
        unsigned index=rb_audit.count;b=rb_audit_allocate(rb_audit.heap,0,16+correction*16);CHECK(b && rb_audit.entries[index].physical);
        CHECK(rb_live_undo(0,0) && !a[0]);rb_live_gc(UINT32_MAX,1);
        CHECK(native_heap_physical_frees==released+correction+1 && !rb_audit.entries[index].physical && !rb_audit.entries[index].held);}
    rb_live_gc(UINT32_MAX,1);CHECK(native_heap_physical_frees==released+2);
    // A stream created after a retained checkpoint is owned by the external
    // resource system. Undo and orphan collection must leave it alive; a
    // speculative main-thread disposal must wait for an exact-input frame.
    // Decimal caller IDs are metadata, not engine addresses to relocate.
    rb_runtime_enabled=1;rb_fixture_origin=4757042; // native return 0x489632
    unsigned stream_index=rb_audit.count;unsigned char *stream=rb_audit_allocate(rb_audit.heap,HEAP_ZERO_MEMORY,160);CHECK(stream);stream[88]=37;
    rb_fixture_origin=0;
    CHECK(rb_begin(&rb_live.journal,0) && rb_touch(&rb_live.journal,a,32) && rb_end(&rb_live.journal));a[0]=77;
    released=native_heap_physical_frees;CHECK(rb_live_undo(0,0) && !a[0]);rb_live_gc(UINT32_MAX,1);
    CHECK(rb_audit.entries[stream_index].live && rb_audit.entries[stream_index].physical && !rb_audit.entries[stream_index].held && stream[88]==37);
    CHECK(native_heap_physical_frees==released);
    rb_tx_begin();rb_runtime_speculating=1;rb_runtime_barrier=0;
    CHECK(rb_audit_release(rb_audit.heap,0,stream) && rb_runtime_barrier && rb_audit.entries[stream_index].physical && native_heap_physical_frees==released && !rb_tx.count);
    rb_runtime_speculating=rb_runtime_barrier=0;rb_tx_seal();rb_tx_commit();unsigned world_frees=rb_audit.frees;
    CHECK(rb_audit_release(rb_audit.heap,0,stream) && native_heap_physical_frees==released+1 && rb_audit.frees==world_frees);
    // Temporary providers born and disposed in one transaction cannot be
    // referenced by older checkpoints and do not belong in the world ledger.
    rb_tx_begin();rb_runtime_speculating=1;rb_fixture_origin=4757564; // native return 0x48983c
    stream=rb_audit_allocate(rb_audit.heap,HEAP_ZERO_MEMORY,168);CHECK(stream && !rb_tx.count);
    rb_fixture_origin=0;CHECK(rb_audit_release(rb_audit.heap,0,stream) && !rb_runtime_barrier && !rb_tx.count && !rb_tx.failed && rb_audit.frees==world_frees);
    rb_runtime_speculating=0;rb_tx_seal();rb_tx_commit();rb_runtime_enabled=0;
    rb_destroy(&rb_live.journal);VirtualFree(retained->live,0,MEM_RELEASE);retained->live=NULL;CHECK(rb_audit_release(rb_audit.heap,0,a));
    // Resize aborts restoration but commits the exact-input pass safely.
    a=rb_audit_allocate(rb_audit.heap,0,32);CHECK(a);rb_tx_begin();a=rb_audit_resize(rb_audit.heap,0,a,128);CHECK(a && rb_tx.failed);rb_tx_seal();rb_tx_commit();CHECK(rb_audit_release(rb_audit.heap,0,a));
    // Recording capacity failure keeps the real allocation alive for the
    // exact-input path; it cannot enter a partial restoration transaction.
    rb_tx_begin();rb_tx.used=16u*1024u*1024u;a=rb_audit_allocate(rb_audit.heap,0,8);CHECK(a && rb_tx.failed);rb_tx_seal();rb_tx_commit();CHECK(rb_audit_release(rb_audit.heap,0,a));
    rb_tx_begin();rb_replaying=1;DWORD written=123;
    CHECK(!rb_audit_write(INVALID_HANDLE_VALUE,"x",1,&written,NULL) && !written && GetLastError()==ERROR_RETRY && rb_tx.failed);
    rb_replaying=0;rb_tx_seal();rb_tx_commit();
    // Speculative file operations must stop before opening/truncating a file
    // or advancing an existing file cursor. Null native function pointers
    // ensure these paths cannot silently reach an actual OS operation.
    memset(&rb_files,0,sizeof(rb_files));rb_runtime_speculating=1;rb_runtime_barrier=0;
    CHECK(rb_file_open_a("must-not-create",GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL)==INVALID_HANDLE_VALUE && rb_runtime_barrier);
    CHECK(rb_file_open_w(L"must-not-create",GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL)==INVALID_HANDLE_VALUE);
    written=77;CHECK(!rb_file_read(INVALID_HANDLE_VALUE,NULL,0,&written,NULL) && !written);
    CHECK(rb_file_seek(INVALID_HANDLE_VALUE,10,NULL,FILE_BEGIN)==INVALID_SET_FILE_POINTER);
    LARGE_INTEGER delta={0};CHECK(!rb_file_seek_ex(INVALID_HANDLE_VALUE,delta,NULL,FILE_BEGIN));
    CHECK(!rb_file_remove("must-not-delete") && !rb_file_flush(INVALID_HANDLE_VALUE));
    rb_runtime_thread=GetCurrentThreadId();rb_runtime_barrier=0;rb_files.read=native_stream_read_probe;
    HANDLE stream_thread=CreateThread(NULL,0,native_stream_thread_probe,NULL,0,NULL);CHECK(stream_thread && WaitForSingleObject(stream_thread,3000)==WAIT_OBJECT_0);
    DWORD stream_result=0;CHECK(GetExitCodeThread(stream_thread,&stream_result) && stream_result==1 && !rb_runtime_barrier);CloseHandle(stream_thread);
    written=77;CHECK(!rb_file_read(INVALID_HANDLE_VALUE,NULL,4,&written,NULL) && !written && rb_runtime_barrier);
    rb_runtime_thread=0;rb_files.read=NULL;
    rb_runtime_speculating=rb_runtime_barrier=0;
    // Track first and last pages, an unchanged write, and a partial final
    // page. Undo itself writes pages; the next checkpoint must resynchronise
    // its mirror before it resets tracking, including writes between frames.
    unsigned char *watched=rb_watch_allocate(3*4096+7);CHECK(watched);RbWatchRegion *watch=rb_watch_find(watched);CHECK(watch);
    memset(watched,7,watch->size);CHECK(rb_watch_begin(watch) && watch->copied==watch->size);
    RbJournal journal;CHECK(rb_init(&journal,3,5*4096,8));CHECK(rb_begin(&journal,0));watched[1]=19;watched[3*4096+6]=21;watched[4096]=7;
    CHECK(rb_watch_store_undo(watch,&journal) && rb_end(&journal));CHECK(rb_last(&journal)->count==2 && rb_last(&journal)->used==4096+7);
    CHECK(rb_undo_to(&journal,0) && watched[1]==7 && watched[3*4096+6]==7);
    watched[2*4096+2]=32;CHECK(rb_watch_begin(watch) && watch->shadow[2*4096+2]==32);
    CHECK(rb_begin(&journal,1));watched[2*4096+2]=33;CHECK(rb_watch_store_undo(watch,&journal) && rb_end(&journal));CHECK(rb_undo_to(&journal,1) && watched[2*4096+2]==32);
    CHECK(rb_watch_begin(watch));CHECK(rb_begin(&journal,2));watched[3]=44;CHECK(rb_watch_store_undo(watch,&journal) && rb_end(&journal));
    CHECK(rb_watch_begin(watch));CHECK(rb_begin(&journal,3));watched[3]=55;watched[4098]=66;CHECK(rb_watch_store_undo(watch,&journal) && rb_end(&journal));
    CHECK(rb_undo_to(&journal,2) && watched[3]==7 && watched[4098]==7);
    CHECK(rb_watch_begin(watch) && rb_begin(&journal,4));watched[4095]=11;watched[4096]=12;watched[8192]=13;watched[3*4096+6]=14;
    CHECK(rb_watch_store_undo(watch,&journal) && rb_end(&journal) && rb_last(&journal)->count==1 && rb_last(&journal)->used==watch->size);
    CHECK(rb_undo_to(&journal,4) && watched[4095]==7 && watched[4096]==7 && watched[8192]==7 && watched[3*4096+6]==7);
    rb_destroy(&journal);rb_watch_destroy(watch);CHECK(!rb_watch_find(watched));
    rb_watch_enabled=1;a=rb_audit_allocate(rb_audit.heap,HEAP_ZERO_MEMORY,8012280);CHECK(a && rb_watch_find(a));CHECK(rb_audit_size(rb_audit.heap,0,a)==8012280);
    rb_tx_begin();CHECK(rb_audit_release(rb_audit.heap,0,a) && rb_watch_find(a));rb_tx_seal();rb_tx_commit();CHECK(!rb_watch_find(a));rb_watch_enabled=0;
    RbAudioParameter parameters[96]={0};uint32_t statuses[96]={0},definition[4]={0,0,0,1};int32_t ids[12],counts[12]={0},positions[12*128]={0};
    for(unsigned i=0;i<12;++i)ids[i]=-1;CHECK(sizeof(RbAudioParameter)==24);
    parameters[2].buffer=(void *)1;parameters[2].definition=definition;ids[0]=2;counts[0]=3;positions[0]=10;positions[1]=-20;positions[2]=40;
    CHECK(rb_audio_drain(ids,counts,positions,parameters,statuses));CHECK(ids[0]==-1 && !counts[0] && parameters[2].pan==10 && statuses[2]==5);
    ids[0]=2;counts[0]=-1;CHECK(rb_audio_drain(ids,counts,positions,parameters,statuses));CHECK(parameters[2].was_playing==1 && !statuses[2]);
    ids[0]=3;counts[0]=-1;parameters[3].was_playing=9;CHECK(rb_audio_drain(ids,counts,positions,parameters,statuses) && !parameters[3].was_playing);
    ids[0]=96;counts[0]=0;CHECK(!rb_audio_drain(ids,counts,positions,parameters,statuses) && ids[0]==96);
    ids[0]=2;counts[0]=61;CHECK(!rb_audio_drain(ids,counts,positions,parameters,statuses) && ids[0]==2);
    puts("PASS native SFX virtual drain: no COM playback, pan averaging, looping/stop status, absent buffers and invalid-command boundaries");
    rb_probe.qualify=0;rb_audit.enabled=rb_audit.tracking=0;DeleteCriticalSection(&rb_audit.lock);VirtualFree(rb_audit.entries,0,MEM_RELEASE);
    VirtualFree(rb_tx.initial,0,MEM_RELEASE);VirtualFree(rb_tx.live,0,MEM_RELEASE);memset(&rb_audit,0,sizeof(rb_audit));memset(&rb_tx,0,sizeof(rb_tx));
    puts("PASS native heap transaction: quarantine, exact address/initial-byte reuse, mixed birth/death replay, future-only discard and earlier revival, physical frees once, resize and capacity boundaries");
}
