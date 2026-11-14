#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "../src/th14/data_profile14.h"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"profile fail line %u\n",__LINE__);return 1;}}while(0)
int main(void){
    const wchar_t *game=L"D:\\private\\中文\\th14.exe";
    const wchar_t *profiles[]={L"D:\\private\\中文\\coop-data\\ShanghaiAlice\\th14",L"D:\\private\\中文\\coop-data\\sessions\\0123456789abcdef0123456789abcdef\\guest-p3\\ShanghaiAlice\\th14"};
    char bytes[4096];
    for(unsigned i=0;i<2;++i){CHECK(WideCharToMultiByte(CP_UTF8,0,profiles[i],-1,bytes,sizeof(bytes),NULL,NULL));CHECK(data_path_matches(bytes,game));CHECK(!data_path_matches(bytes,L"D:\\other\\th14.exe"));}
    CHECK(!data_path_matches("D:\\private\\coop-data\\ShanghaiAlice\\th18",game));
    memset(bytes,'x',sizeof(bytes));CHECK(!data_path_matches(bytes,game));
    puts("PASS TH14 native profile: UTF8 local/session isolation and unrelated/unterminated path rejection");return 0;
}
