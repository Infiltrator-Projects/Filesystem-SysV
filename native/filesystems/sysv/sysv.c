/* SPDX-License-Identifier: GPL-3.0-or-later
 * Independent System V Release 2 / Release 4 filesystem reader.
 * On-disk references: Linux v6.12 include/linux/sysv_fs.h, and the
 * retained fs/sysv superblock, inode, free-list and indirect-tree layouts.
 * No Linux VFS calls or external filesystem code; Common V2 host only.
 * Initial profile accepts clean, conventional 14-byte-name volumes at
 * partition offset zero. No guessing for signatureless V7 or SCO EAFS.
 */
#include <infiltratr/filesystem_module_v2.h>
#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>
enum { MAX_BLOCK=2048,INODE=64,DIRENT=16,MAX_NAME=14,SLOTS=8,
       MAX_DIR_ENTRIES=262144,MAX_ZONES=0xffffff,FREE_CACHE=50 };
#define MAGIC UINT32_C(0xfd187e20)
typedef struct {uint64_t bytes;uint32_t ptr[13];uint32_t type;} Tree;
typedef struct {Tree tree;uint32_t id,mode,uid,gid,type;} Node;
typedef struct {
    IfsFsHostRuntimeV2 host;
    uint32_t block,blocks,inodes,first,free_blocks,variant;
    unsigned be,r4;
    uint8_t *free_map;
    char label[7];
    uintptr_t token;
} Volume;
typedef struct {Node node;uint32_t length;char name[MAX_NAME+1];} Entry;
static Volume slots[SLOTS];
static uintptr_t generation;
static atomic_flag gate=ATOMIC_FLAG_INIT;
static int enter(void){return !atomic_flag_test_and_set_explicit(&gate,memory_order_acquire);}
static int leave(int rc){atomic_flag_clear_explicit(&gate,memory_order_release);return rc;}
static void copy(void *d,const void *s,size_t n){uint8_t *o=d;const uint8_t *p=s;while(n--)*o++=*p++;}
static void zero(void *d,size_t n){uint8_t *p=d;while(n--)*p++=0;}
static int same(const void *a,const void *b,size_t n)
{const uint8_t *p=a,*q=b;while(n--)if(*p++!=*q++)return 0;return 1;}
static uint32_t u16(const uint8_t *p,unsigned be){return be?((uint32_t)p[0]<<8|p[1]):((uint32_t)p[1]<<8|p[0]);}
static uint32_t u32(const uint8_t *p,unsigned be){return be?u16(p,be)<<16|u16(p+2,be):u16(p+2,be)<<16|u16(p,be);}
static uint32_t u24(const uint8_t *p,unsigned be)
{return be?((uint32_t)p[0]<<16|((uint32_t)p[1]<<8)|p[2]):((uint32_t)p[2]<<16|((uint32_t)p[1]<<8)|p[0]);}
static int rd(const Volume *v,uint64_t off,void *p,uint32_t n)
{return ifs_fs_host_read_bytes_v2(&v->host,off,p,n);}
static int cancel(const Volume *v){return v->host.cancelled&&v->host.cancelled(v->host.context);}
static int free_zone(const Volume *v,uint32_t b)
{return v->free_map&&(v->free_map[b/8]&(1u<<(b%8)));}
static int usable(const Volume *v,uint32_t b)
{return b>=v->first&&b<v->blocks&&!free_zone(v,b);}
static void dispose(Volume *v)
{if(v->free_map)v->host.release(v->host.context,v->free_map);zero(v,sizeof(*v));}
static int tree_valid(const Volume *v,const Tree *t)
{
    uint64_t fan=v->block/4,cap=10+fan+fan*fan+fan*fan*fan;
    return (t->bytes+v->block-1)/v->block<=cap;
}
/* Direct, single, double and triple indirect mappings. Zero pointers
 * represent genuine sparse regular-file holes, never missing directory
 * or symlink metadata. Every nonzero pointer is checked against the
 * actual free-list map, and repeated indirect blocks are rejected. */
static int physical(const Volume *v,const Tree *t,uint64_t logical,uint32_t *out)
{
    if(!tree_valid(v,t)||logical>=(t->bytes+v->block-1)/v->block)return IFS_FS_CORRUPT;
    uint32_t fan=v->block/4,b=0,seen[3];unsigned depth=0;
    uint64_t divisor=1;
    if(logical<10)b=t->ptr[logical];
    else{
        logical-=10;uint64_t span=fan;
        for(depth=1;depth<=3;++depth){
            if(logical<span)break;
            logical-=span;span*=fan;
        }
        if(depth>3)return IFS_FS_CORRUPT;
        b=t->ptr[9+depth];divisor=span;
    }
    uint8_t data[MAX_BLOCK];
    for(unsigned i=0;;++i){
        if(!b){if(t->type!=IFS_FS_ENTRY_FILE)return IFS_FS_CORRUPT;*out=0;return IFS_FS_OK;}
        if(!usable(v,b))return IFS_FS_CORRUPT;
        for(unsigned j=0;j<i;++j)if(seen[j]==b)return IFS_FS_CORRUPT;
        if(i==depth){*out=b;return IFS_FS_OK;}
        seen[i]=b;if(cancel(v))return IFS_FS_BUSY;
        int rc=rd(v,(uint64_t)b*v->block,data,v->block);if(rc)return rc;
        divisor/=fan;uint64_t idx=logical/divisor;logical%=divisor;
        if(idx>=fan)return IFS_FS_CORRUPT;
        b=u32(data+4*idx,v->be);
    }
}
static int tree_read(const Volume *v,const Tree *t,uint64_t at,void *out,uint32_t n)
{
    if((!out&&n)||at>t->bytes||n>t->bytes-at)return IFS_FS_INVALID;
    uint8_t *p=out;
    while(n){
        if(cancel(v))return IFS_FS_BUSY;
        uint32_t b,skip=(uint32_t)(at%v->block),chunk=v->block-skip;
        if(chunk>n)chunk=n;
        int rc=physical(v,t,at/v->block,&b);if(rc)return rc;
        if(b){rc=rd(v,(uint64_t)b*v->block+skip,p,chunk);if(rc)return rc;}
        else zero(p,chunk);
        p+=chunk;at+=chunk;n-=chunk;
    }
    return IFS_FS_OK;
}
static int inode(const Volume *v,uint32_t id,Node *out)
{
    if(!id||id>v->inodes)return IFS_FS_CORRUPT;
    uint8_t raw[INODE];int rc=rd(v,(uint64_t)2*v->block+(uint64_t)(id-1)*INODE,raw,INODE);if(rc)return rc;
    Node n={.id=id,.mode=u16(raw,v->be),.uid=u16(raw+4,v->be),.gid=u16(raw+6,v->be)};
    if(!u16(raw+2,v->be))return IFS_FS_CORRUPT;
    n.tree.bytes=u32(raw+8,v->be);
    for(unsigned i=0;i<13;++i)n.tree.ptr[i]=u24(raw+12+3*i,v->be);
    switch(n.mode&0170000u){
    case 0040000u:n.type=IFS_FS_ENTRY_DIRECTORY;break;
    case 0100000u:n.type=IFS_FS_ENTRY_FILE;break;
    case 0120000u:n.type=IFS_FS_ENTRY_LINK;break;
    default:return IFS_FS_UNSUPPORTED;
    }
    n.tree.type=n.type;
    if(!tree_valid(v,&n.tree)||(n.type==IFS_FS_ENTRY_DIRECTORY&&n.tree.bytes%DIRENT))return IFS_FS_CORRUPT;
    *out=n;return IFS_FS_OK;
}
static int root_check(const Volume *v)
{
    Node root;int rc=inode(v,2,&root);if(rc)return rc;
    if(root.type!=IFS_FS_ENTRY_DIRECTORY||root.tree.bytes<2*DIRENT)return IFS_FS_CORRUPT;
    uint8_t data[2*DIRENT];rc=tree_read(v,&root.tree,0,data,sizeof(data));if(rc)return rc;
    if(u16(data,v->be)!=2||u16(data+DIRENT,v->be)!=2||
       !same(data+2,".\0",2)||!same(data+DIRENT+2,"..\0",3))return IFS_FS_CORRUPT;
    return IFS_FS_OK;
}
/* The superblock free cache is a stack. The last entry popped is the
 * next cache block, itself free. Validate the complete chain and exact
 * advertised count, refusing cycles, duplicate blocks and invalid zones. */
static int load_free(Volume *v,const uint8_t *sb)
{
    size_t bytes=(v->blocks+7u)/8;
    v->free_map=v->host.allocate(v->host.context,bytes);
    if(!v->free_map)return IFS_FS_NO_MEMORY;
    zero(v->free_map,bytes);
    unsigned off=v->r4?12:8,count=u16(sb+(v->r4?8:6),v->be);
    const uint8_t *p=sb+off;uint8_t raw[MAX_BLOCK];uint32_t total=0;
    for(;;){
        if(cancel(v))return IFS_FS_BUSY;
        if(count>FREE_CACHE)return IFS_FS_CORRUPT;
        uint32_t next=0;
        while(count){
            uint32_t b=u32(p+4*(--count),v->be);
            if(!b){if(count)return IFS_FS_CORRUPT;next=0;break;}
            if(!usable(v,b)||total>=v->free_blocks)return IFS_FS_CORRUPT;
            v->free_map[b/8]|=(uint8_t)(1u<<(b%8));++total;next=b;
        }
        if(!next)break;
        int rc=rd(v,(uint64_t)next*v->block,raw,v->block);if(rc)return rc;
        count=u16(raw,v->be);p=raw+(v->r4?4:2);
    }
    return total==v->free_blocks?IFS_FS_OK:IFS_FS_CORRUPT;
}
static int discover(const IfsFsHostRuntimeV2 *h,Volume *out)
{
    if(!ifs_fs_host_v2_valid(h))return IFS_FS_INVALID;
    Volume v={0};size_t n=h->structure_bytes;if(n>sizeof(v.host))n=sizeof(v.host);
    copy(&v.host,h,n);v.host.structure_bytes=(uint32_t)n;
    if(h->volume_bytes<1024)return IFS_FS_NOT_FOUND;
    uint8_t sb[512];int rc=rd(&v,512,sb,sizeof sb);if(rc)return rc;
    if(u32(sb+504,0)!=MAGIC){if(u32(sb+504,1)!=MAGIC)return IFS_FS_NOT_FOUND;v.be=1;}
    uint32_t type=u32(sb+508,v.be),state=u32(sb+500,v.be);
    if(type<1||type>3)return IFS_FS_UNSUPPORTED;
    v.block=256u<<type;v.first=u16(sb,v.be);
    /* Use actual clean-state invariants rather than a timestamp guess.
     * Reject dirty or ambiguous images; this reader cannot replay them. */
    int r2=state==UINT32_C(0xcb096f43),r4=state==UINT32_C(0x7c269d38)-u32(sb+420,v.be);
    if(r2==r4)return IFS_FS_UNSUPPORTED;
    v.r4=(unsigned)r4;v.variant=(r4?3u:1u)+v.be;
    v.blocks=u32(sb+(r4?4:2),v.be);v.free_blocks=u32(sb+(r4?432:426),v.be);
    if(v.blocks>MAX_ZONES||v.first<=2||v.first>=v.blocks||
       v.blocks>h->volume_bytes/v.block||v.free_blocks>v.blocks-v.first)
        return IFS_FS_CORRUPT;
    uint64_t inodes=(uint64_t)(v.first-2)*v.block/INODE;
    if(inodes>65535||inodes<2||u16(sb+(r4?436:430),v.be)>inodes||
       u16(sb+(r4?212:208),v.be)>100)return IFS_FS_CORRUPT;
    v.inodes=(uint32_t)inodes;copy(v.label,sb+(r4?440:432),6);
    rc=load_free(&v,sb);if(!rc)rc=root_check(&v);
    if(rc){dispose(&v);return rc;}
    *out=v;return IFS_FS_OK;
}
static Volume *mounted(void *p)
{if(p)for(unsigned i=0;i<SLOTS;++i)if(slots[i].token==(uintptr_t)p)return &slots[i];return NULL;}
static void info(const Volume *v,IfsFsVolumeInfoV2 *o)
{*o=(IfsFsVolumeInfoV2){.structure_bytes=sizeof(*o),.variant=v->variant,.max_name_bytes=MAX_NAME,
.block_bytes=v->block,.total_bytes=(uint64_t)v->blocks*v->block,.free_bytes=(uint64_t)v->free_blocks*v->block};}
static int probe_impl(const IfsFsHostRuntimeV2 *h,uint32_t *variant)
{if(!variant)return IFS_FS_INVALID;Volume v={0};int rc=discover(h,&v);if(!rc)*variant=v.variant;dispose(&v);return rc;}
static int mount_impl(const IfsFsHostRuntimeV2 *h,void **out,IfsFsVolumeInfoV2 *o)
{
    if(!out||!o||o->structure_bytes<sizeof(*o))return IFS_FS_INVALID;
    *out=NULL;if(!ifs_fs_host_v2_valid(h))return IFS_FS_INVALID;
    if(!(h->capabilities&IFS_FS_HOST_READ_ONLY))return IFS_FS_ACCESS;
    unsigned i=0;while(i<SLOTS&&slots[i].token)++i;
    if(i==SLOTS||generation==UINTPTR_MAX)return IFS_FS_BUSY;
    int rc=discover(h,&slots[i]);if(rc)return rc;
    slots[i].token=++generation;*out=(void *)slots[i].token;info(&slots[i],o);return IFS_FS_OK;
}
static int unmount_impl(void *p)
{Volume *v=mounted(p);if(!v)return IFS_FS_INVALID;dispose(v);return IFS_FS_OK;}
static int child(const Volume *v,const Node *dir,const char *name,uint32_t length,uint32_t index,Entry *out)
{
    if(dir->type!=IFS_FS_ENTRY_DIRECTORY)return IFS_FS_NOT_FOUND;
    if(dir->tree.bytes/DIRENT>MAX_DIR_ENTRIES)return IFS_FS_UNSUPPORTED;
    int found=0;Entry result;
    for(uint64_t at=0;at<dir->tree.bytes;at+=DIRENT){
        uint8_t raw[DIRENT];int rc=tree_read(v,&dir->tree,at,raw,DIRENT);if(rc)return rc;
        uint32_t id=u16(raw,v->be);if(!id)continue;
        if(id>v->inodes)return IFS_FS_CORRUPT;
        Entry e;uint32_t n=0;
        while(n<MAX_NAME&&raw[2+n])++n;
        if(!n)return IFS_FS_CORRUPT;
        for(uint32_t i=0;i<n;++i)if(raw[2+i]=='/'||raw[2+i]==92)return IFS_FS_CORRUPT;
        copy(e.name,raw+2,n);e.name[n]=0;e.length=n;
        if((n==1&&e.name[0]=='.')||(n==2&&same(e.name,"..",2)))continue;
        if(name?(length==n&&same(name,e.name,n)):index==0){
            if(found&&name)return IFS_FS_CORRUPT;
            rc=inode(v,id,&e.node);if(rc)return rc;
            if(e.node.type==IFS_FS_ENTRY_DIRECTORY&&id==dir->id)return IFS_FS_CORRUPT;
            result=e;found=1;if(!name){*out=result;return IFS_FS_OK;}
        }
        if(!name&&index)--index;
    }
    if(!found)return name?IFS_FS_NOT_FOUND:IFS_FS_END;
    *out=result;return IFS_FS_OK;
}
static int resolve(const Volume *v,const char *path,Entry *out)
{
    if(!path||*path!='/')return IFS_FS_INVALID;
    Entry e={.length=1,.name="/"};int rc=inode(v,2,&e.node);if(rc)return rc;
    ++path;
    for(unsigned depth=0;*path;++depth){
        if(depth>=256)return IFS_FS_INVALID;
        uint32_t n=0;while(n<=MAX_NAME&&path[n]&&path[n]!='/')++n;
        if(!n||n>MAX_NAME||(n==1&&path[0]=='.')||(n==2&&same(path,"..",2)))return IFS_FS_INVALID;
        Node dir=e.node;rc=child(v,&dir,path,n,0,&e);if(rc)return rc;
        path+=n;if(*path=='/'){++path;if(!*path)return IFS_FS_INVALID;}
    }
    *out=e;return IFS_FS_OK;
}
static int emit(const Entry *e,IfsFsEntryV2 *o)
{
    if(!o||o->structure_bytes<sizeof(*o)||!o->name)return IFS_FS_INVALID;
    if(o->name_capacity<=e->length)return IFS_FS_NO_SPACE;
    char *name=o->name;uint32_t cap=o->name_capacity;
    *o=(IfsFsEntryV2){.structure_bytes=sizeof(*o),.type=e->node.type,.size_bytes=e->node.tree.bytes,
      .name=name,.name_capacity=cap,.name_bytes=e->length};
    copy(name,e->name,e->length+1);return IFS_FS_OK;
}
static int stat_impl(void *p,const char *path,IfsFsEntryV2 *o)
{Volume *v=mounted(p);if(!v)return IFS_FS_INVALID;Entry e;int rc=resolve(v,path,&e);return rc?rc:emit(&e,o);}
static int list_impl(void *p,const char *path,uint32_t i,IfsFsEntryV2 *o)
{Volume *v=mounted(p);if(!v)return IFS_FS_INVALID;Entry dir,e;int rc=resolve(v,path,&dir);if(!rc)rc=child(v,&dir.node,NULL,0,i,&e);return rc?rc:emit(&e,o);}
static int read_impl(void *p,const char *path,uint64_t at,void *out,uint32_t n)
{Volume *v=mounted(p);if(!v)return IFS_FS_INVALID;Entry e;int rc=resolve(v,path,&e);if(rc)return rc;return e.node.type==IFS_FS_ENTRY_FILE?tree_read(v,&e.node.tree,at,out,n):IFS_FS_UNSUPPORTED;}
static int readlink_impl(void *p,const char *path,char *out,uint32_t cap,uint32_t *n)
{
    if(!n)return IFS_FS_INVALID;
    *n=0;Volume *v=mounted(p);if(!v||!out)return IFS_FS_INVALID;
    Entry e;int rc=resolve(v,path,&e);if(rc)return rc;
    if(e.node.type!=IFS_FS_ENTRY_LINK)return IFS_FS_INVALID;
    if(!e.node.tree.bytes||e.node.tree.bytes>=cap||e.node.tree.bytes>4095)return IFS_FS_NO_SPACE;
    uint32_t len=(uint32_t)e.node.tree.bytes;
    char target[4096];rc=tree_read(v,&e.node.tree,0,target,len);if(rc)return rc;
    for(uint32_t i=0;i<len;++i)if(!target[i])return IFS_FS_CORRUPT;
    copy(out,target,len);out[len]=0;*n=len;return IFS_FS_OK;
}
static int query_impl(void *p,IfsFsVolumeInfoV2 *o)
{Volume *v=mounted(p);if(!v||!o||o->structure_bytes<sizeof(*o))return IFS_FS_INVALID;info(v,o);return IFS_FS_OK;}
static int metadata_impl(void *p,const char *path,IfsFsMetadataV2 *o)
{
    Volume *v=mounted(p);if(!v||!o||o->structure_bytes<sizeof(*o))return IFS_FS_INVALID;
    Entry e;int rc=resolve(v,path,&e);if(rc)return rc;
    *o=(IfsFsMetadataV2){.structure_bytes=sizeof(*o),.mask=IFS_FS_META_GET_MODE,
       .mode=e.node.mode,.owner=e.node.uid,.group=e.node.gid};return IFS_FS_OK;
}
static int label_impl(void *p,char *out,uint32_t cap,uint32_t *n)
{
    if(!n)return IFS_FS_INVALID;
    *n=0;Volume *v=mounted(p);if(!v||!out)return IFS_FS_INVALID;
    uint32_t len=0;while(len<6&&v->label[len])++len;
    if(cap<=len)return IFS_FS_NO_SPACE;
    copy(out,v->label,len);out[len]=0;*n=len;return IFS_FS_OK;
}
#define WRAP(name,args,call) static int name args{if(!enter())return IFS_FS_BUSY;return leave(call);}
WRAP(probe,(const IfsFsHostRuntimeV2 *h,uint32_t *v),probe_impl(h,v))
WRAP(mount,(const IfsFsHostRuntimeV2 *h,void **p,IfsFsVolumeInfoV2 *o),mount_impl(h,p,o))
WRAP(unmount,(void *p),unmount_impl(p))
WRAP(stat_path,(void *p,const char *s,IfsFsEntryV2 *o),stat_impl(p,s,o))
WRAP(list_path,(void *p,const char *s,uint32_t i,IfsFsEntryV2 *o),list_impl(p,s,i,o))
WRAP(read_path,(void *p,const char *s,uint64_t a,void *b,uint32_t n),read_impl(p,s,a,b,n))
WRAP(readlink_path,(void *p,const char *s,char *b,uint32_t c,uint32_t *n),readlink_impl(p,s,b,c,n))
WRAP(query,(void *p,IfsFsVolumeInfoV2 *o),query_impl(p,o))
WRAP(metadata,(void *p,const char *s,IfsFsMetadataV2 *o),metadata_impl(p,s,o))
WRAP(label_path,(void *p,char *s,uint32_t c,uint32_t *n),label_impl(p,s,c,n))
static const IfsFsCoreOpsV2 core={
 .header={2,sizeof(core),IFS_FS_GROUP_CORE,0,IFS_FS_CORE_PROBE|IFS_FS_CORE_MOUNT|IFS_FS_CORE_STAT|IFS_FS_CORE_LIST|IFS_FS_CORE_READ},
 .probe=probe,.mount=mount,.unmount=unmount,.stat=stat_path,.directory_entry=list_path,.read=read_path};
static const IfsFsVolumeOpsV2 volume={.header={2,sizeof(volume),IFS_FS_GROUP_VOLUME,0,IFS_FS_VOLUME_QUERY|IFS_FS_VOLUME_LABEL_GET},.query=query,.label=label_path};
static const IfsFsMetadataOpsV2 meta={.header={2,sizeof(meta),IFS_FS_GROUP_METADATA,0,IFS_FS_META_GET_MODE|IFS_FS_META_READLINK},.get=metadata,.readlink=readlink_path};
static const IfsFsGroupHeaderV2 *group(uint32_t id,uint32_t lo,uint32_t hi)
{if(lo>2||hi<2)return NULL;if(id==IFS_FS_GROUP_CORE)return &core.header;if(id==IFS_FS_GROUP_VOLUME)return &volume.header;if(id==IFS_FS_GROUP_METADATA)return &meta.header;return NULL;}
static const IfsFsModuleApiV2 api={.abi_version=2,.structure_bytes=sizeof(api),.format_id="sysv",
#ifdef IFS_BOOT_FIRMWARE
 .runtime_profiles=IFS_FS_PROFILE_FREESTANDING|IFS_FS_PROFILE_FIRMWARE,
#else
 .runtime_profiles=IFS_FS_PROFILE_HOSTED,
#endif
 .group_mask=IFS_FS_GROUP_BIT(IFS_FS_GROUP_CORE)|IFS_FS_GROUP_BIT(IFS_FS_GROUP_VOLUME)|IFS_FS_GROUP_BIT(IFS_FS_GROUP_METADATA),.query_group=group};
__attribute__((visibility("default")))const IfsFsModuleApiV2 *infiltrator_filesystem_api_v2(void){return &api;}
