/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <infiltratr/filesystem_module_v2.h>
#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
enum {N=256,MAX=N*2048};
static uint8_t disk[MAX],saved[MAX];static unsigned big,B,sector,allocs,fail;
static void p16(uint8_t *p,uint32_t n){p[big?0:1]=n>>8;p[big?1:0]=n;}
static void p32(uint8_t *p,uint32_t n){p16(p+(big?0:2),n>>16);p16(p+(big?2:0),n);}
static void p24(uint8_t *p,uint32_t n){p[big?0:2]=n>>16;p[1]=n>>8;p[big?2:0]=n;}
static uint8_t *blk(unsigned n){return disk+n*B;}
static void ino(unsigned id,unsigned mode,unsigned bytes,unsigned ptr)
{uint8_t *p=blk(2)+(id-1)*64;p16(p,mode);p16(p+2,1);p16(p+4,123);p16(p+6,456);p32(p+8,bytes);p24(p+12,ptr);}
static void dent(uint8_t *p,unsigned id,const char *name){p16(p,id);memcpy(p+2,name,strlen(name));}
static void fixture(unsigned r4)
{
    memset(disk,0,sizeof disk);uint8_t *sb=disk+512;
    p16(sb,4);p32(sb+(r4?4:2),N);p16(sb+(r4?8:6),2);
    /* Two cached free blocks, followed by a terminal free-list block. */
    p32(sb+(r4?12:8),200);p32(sb+(r4?16:12),201);
    p16(blk(200),2);p32(blk(200)+(r4?4:2),0);p32(blk(200)+(r4?8:6),202);
    p32(sb+(r4?432:426),3);p32(sb+420,1000000000);
    memcpy(sb+(r4?440:432),"SYSV",4);
    p32(sb+500,r4?0x7c269d38u-1000000000u:0xcb096f43u);
    p32(sb+504,0xfd187e20);p32(sb+508,B==512?1:B==1024?2:3);
    ino(2,0040755,7*16,4);ino(3,0100644,10*B+37,5);
    ino(4,0040755,3*16,6);ino(5,0100644,3,7);ino(6,0120777,9,8);
    /* A second file spans the double/triple-indirect boundary without
     * allocating its sparse logical range. The physical tree is tiny. */
    uint32_t fan=B/4, double_at=10+fan, triple_at=double_at+fan*fan;
    ino(7,0100644,triple_at*B+1,0);
    p24(blk(2)+6*64+12+33,11);p32(blk(11),12);p32(blk(12),13);
    p24(blk(2)+6*64+12+36,14);p32(blk(14),15);p32(blk(15),16);p32(blk(16),17);
    blk(13)[0]=0x9b;blk(17)[0]=0x5c;
    /* File has direct data, valid sparse holes, and a single indirect tail. */
    p24(blk(2)+2*64+12+30,9);p32(blk(9),10);
    dent(blk(4),2,".");dent(blk(4)+16,2,"..");dent(blk(4)+32,3,"proof.bin");
    dent(blk(4)+48,4,"dir");dent(blk(4)+64,6,"link");dent(blk(4)+80,5,"abcdefghijklmn");dent(blk(4)+96,7,"deep");
    dent(blk(6),4,".");dent(blk(6)+16,2,"..");dent(blk(6)+32,5,"child");
    memcpy(blk(7),"yes",3);memcpy(blk(8),"proof.bin",9);
    for(unsigned i=0;i<B;++i){blk(5)[i]=(uint8_t)(i*31+7);blk(10)[i]=(uint8_t)(i*17+3);}
    memcpy(saved,disk,sizeof disk);
}
static int rd(void *c,uint64_t off,void *out,uint32_t n)
{(void)c;if(fail||off>sizeof disk||n>sizeof disk-off||off%sector||n%sector)return -1;memcpy(out,disk+off,n);return 0;}
static void *alloc(void *c,size_t n){(void)c;void *p=malloc(n);if(p)++allocs;return p;}
static void drop(void *c,void *p){(void)c;if(p){assert(allocs);--allocs;free(p);}}
int main(int argc,char **argv)
{
    if(argc==4&&!strcmp(argv[1],"--emit")){
        B=1024;big=0;fixture(1);
        FILE *img=fopen(argv[2],"wb"),*proof=fopen(argv[3],"wb");assert(img&&proof);
        assert(fwrite(disk,1,sizeof disk,img)==sizeof disk);
        assert(fwrite(blk(5),1,B,proof)==B);
        uint8_t empty[2048]={0};
        for(unsigned i=1;i<10;++i)assert(fwrite(empty,1,B,proof)==B);
        assert(fwrite(blk(10),1,37,proof)==37);
        assert(!fclose(img)&&!fclose(proof));return 0;
    }
    assert(argc==2);void *lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);if(!lib){puts(dlerror());return 1;}
    IfsFsModuleApiEntryV2 entry;*(void **)(&entry)=dlsym(lib,IFS_FS_COMMON_EXPORT_V2);assert(entry);
    const IfsFsModuleApiV2 *api=entry();assert(ifs_fs_api_v2_valid(api,"sysv"));
    const IfsFsCoreOpsV2 *core=(const void *)ifs_fs_group_v2(api,IFS_FS_GROUP_CORE);
    const IfsFsVolumeOpsV2 *vol=(const void *)ifs_fs_group_v2(api,IFS_FS_GROUP_VOLUME);
    const IfsFsMetadataOpsV2 *meta=(const void *)ifs_fs_group_v2(api,IFS_FS_GROUP_METADATA);
    assert(core&&vol&&meta&&!core->write);
    for(big=0;big<2;++big)for(unsigned r4=0;r4<2;++r4)for(B=512;B<=2048;B*=2){
      fixture(r4);
      for(sector=512;sector<=4096;sector*=8){
        IfsFsHostRuntimeV2 h={.abi_version=2,.structure_bytes=sizeof(h),.capabilities=IFS_FS_HOST_READ_ONLY,
          .volume_bytes=sizeof disk,.logical_sector_bytes=sector,.physical_sector_bytes=sector,.maximum_transfer_bytes=4096,
          .read_at=rd,.allocate=alloc,.release=drop};
        uint32_t variant=0;assert(core->probe(&h,&variant)==0&&variant==(r4?3u:1u)+big&&!allocs);
        void *v=NULL,*other=NULL;IfsFsVolumeInfoV2 i={.structure_bytes=sizeof i};
        assert(core->mount(&h,&v,&i)==0&&v&&i.total_bytes==N*B&&i.free_bytes==3*B);
        assert(core->mount(&h,&other,&i)==0&&other!=v);
        char name[64];IfsFsEntryV2 e={.structure_bytes=sizeof e,.name=name,.name_capacity=sizeof name};
        assert(core->directory_entry(v,"/",0,&e)==0&&!strcmp(name,"proof.bin"));
        assert(core->directory_entry(v,"/",3,&e)==0&&!strcmp(name,"abcdefghijklmn"));
        assert(core->directory_entry(v,"/",4,&e)==0&&!strcmp(name,"deep"));
        assert(core->directory_entry(v,"/",5,&e)==IFS_FS_END);
        assert(core->stat(v,"/proof.bin",&e)==0&&e.size_bytes==10*B+37);
        unsigned char buf[2048];assert(core->read(v,"/proof.bin",0,buf,B)==0&&!memcmp(buf,blk(5),B));
        memset(buf,255,B);assert(core->read(v,"/proof.bin",B,buf,B)==0);
        for(unsigned j=0;j<B;++j)assert(!buf[j]);
        assert(core->read(v,"/proof.bin",10*B,buf,37)==0&&!memcmp(buf,blk(10),37));
        uint64_t fan=B/4,double_at=10+fan,triple_at=double_at+fan*fan;
        assert(core->read(v,"/deep",double_at*B,buf,1)==0&&buf[0]==0x9b);
        assert(core->read(v,"/deep",triple_at*B,buf,1)==0&&buf[0]==0x5c);
        assert(core->read(v,"/deep",triple_at*B-1,buf,2)==0&&!buf[0]&&buf[1]==0x5c);
        p32(blk(16),14);assert(core->read(v,"/deep",triple_at*B,buf,1)==IFS_FS_CORRUPT);
        memcpy(disk,saved,sizeof disk);
        assert(core->read(v,"/dir/child",0,buf,3)==0&&!memcmp(buf,"yes",3));
        uint32_t n;assert(meta->readlink(v,"/link",name,sizeof name,&n)==0&&n==9&&!strcmp(name,"proof.bin"));
        assert(vol->label(v,name,sizeof name,&n)==0&&n==4&&!strcmp(name,"SYSV"));
        p32(blk(9),9);assert(core->read(v,"/proof.bin",10*B,buf,1)==IFS_FS_CORRUPT);
        p32(blk(9),201);assert(core->read(v,"/proof.bin",10*B,buf,1)==IFS_FS_CORRUPT);
        memcpy(disk,saved,sizeof disk);
        fail=1;assert(core->stat(v,"/",&e)!=0);fail=0;
        assert(core->unmount(v)==0&&core->unmount(v)==IFS_FS_INVALID&&core->unmount(other)==0&&!allocs);
        p32(blk(200)+(r4?8:6),201);assert(core->probe(&h,&variant)==IFS_FS_CORRUPT&&!allocs);
        memcpy(disk,saved,sizeof disk);
        assert(!memcmp(disk,saved,sizeof disk));
      }
    }
    puts("SysV R2/R4, endian, geometry, free-chain, sparse and indirect reads PASS");dlclose(lib);return 0;
}
