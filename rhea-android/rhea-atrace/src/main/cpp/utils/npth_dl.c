/*
 * Copyright (C) 2021 ByteDance Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
//
// Created by puyingmin on 2019/12/18.
//
#define LOG_TAG "NPTH_DL"
#include <stddef.h>
#include <unistd.h>
#include <dlfcn.h>
#include <link.h>
#include <memory.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <malloc.h>
#include <inttypes.h>
#include <stdlib.h>
#include "npth_dl.h"
#include "logger.h"

struct elf_hash_struct
{
    size_t              nbucket;
    size_t              nchain;
    unsigned            *bucket;
    unsigned            *chain;
};

struct gnu_hash_struct
{
    size_t              nbucket;
    uint32_t            *bucket;
    uint32_t            *chain;
    uint32_t            maskwords;
    uint32_t            shift2;
    ElfW(Addr)          *bloomfilter;
};

#define ELF_HASH_MASK           (uint32_t)0x1
#define GNU_HASH_MASK           (uint32_t)0x2
#define HAS_ELF_HASH(_flag)     (((_flag) & ELF_HASH_MASK) == ELF_HASH_MASK)
#define HAS_GNU_HASH(_flag)     (((_flag) & GNU_HASH_MASK) == GNU_HASH_MASK)
#define SET_ELF_HASH(_flag)     do { (_flag) |= ELF_HASH_MASK; } while(0)
#define SET_GNU_HASH(_flag)     do { (_flag) |= GNU_HASH_MASK; } while(0)

typedef struct nsoinfo
{
    ElfW(Addr)              base;
    ElfW(Addr)              load_bias;
    char*                   path;
    size_t                  size;
    uint32_t                phnum;
    uint32_t                hash_flag;
    const ElfW(Phdr)        *phdr;
    const ElfW(Dyn)         *dynamic;
    const ElfW(Sym)         *dynsym;
    const char              *dynstr;

    const ElfW(Sym)         *symtab;
    const char              *strtab;
    size_t                  nsymtab;
    size_t                  nstrtab;
    struct gnu_hash_struct  gnu_hash;
    struct elf_hash_struct  elf_hash;
    void                    *mapdata;
    size_t                  mapsize;
} nsoinfo_t;

typedef int (*DL_ITERATE_PHDR_CB)(struct dl_phdr_info* , size_t, void*);
typedef int (*DL_ITERATE_PHDR)(DL_ITERATE_PHDR_CB, void*);

typedef struct
{
    const char*             name;               /* in */
    union
    {
        int                 need_path;          /* in */
        void*               padding1;
    };
    char*                   path;               /* out */
    ElfW(Addr)              base;               /* out */
    ElfW(Addr)              load_bias;          /* out */
    const ElfW(Phdr)*       phdr;               /* out */
    union
    {
        uint32_t            phnum;              /* out */
        void *              padding2;
    };
} dl_search_t;

char* get_path_from_maps(const void* address)
{
    char line[1024];
    uintptr_t start;

    FILE *f = fopen("/proc/self/maps", "r");
    if (f == NULL) return NULL;

    char * ret = NULL;
    while (fgets(line, sizeof(line), f))
    {
        if (1 == sscanf(line, "%"SCNxPTR"", &start) && (void *)start == address)
        {
            char* path = strchr(line, '/');
            if (NULL == path) break;

            char* last = strchr(path, ' ');
            if (NULL == last)
            {
                last = strchr(path, '\n');
                if (NULL == last) break;
            }

            *last = '\0';

            ret = strdup(path);
            break;
        }
    }

    fclose(f);
    return ret;
}

static ElfW(Addr) get_so_base(struct dl_phdr_info* info)
{
    const ElfW(Phdr) *phdr = info->dlpi_phdr;
    const ElfW(Phdr) *phdr_end = phdr + info->dlpi_phnum;

    ElfW(Addr) min_vaddr = INTPTR_MAX;

    for (phdr = info->dlpi_phdr; phdr < phdr_end; phdr++)
    {
        if (phdr->p_type != PT_LOAD) continue;

        if (min_vaddr <= phdr->p_vaddr) continue;

        min_vaddr = phdr->p_vaddr;
    }

    return min_vaddr == INTPTR_MAX ? 0 : info->dlpi_addr + min_vaddr;
}

static int dl_iterate_phdr_cb(struct dl_phdr_info* info, size_t size, void* data)
{
    dl_search_t* search = (dl_search_t *)data;

    if (info->dlpi_name == NULL) return 0;

    const char* cmp1 = search->name;
    const char* cmp2 = info->dlpi_name;

    if ('/' == cmp1[0] && '/' != cmp2[0])
    {
        cmp1 = strrchr(cmp1, '/') + 1;
    }
    else if ('/' != cmp1[0] && '/' == cmp2[0])
    {
        cmp2 = strrchr(cmp2, '/') + 1;
    }

    if (0 != strcmp(cmp1, cmp2)) return 0;

    search->base = get_so_base(info);
    search->load_bias = info->dlpi_addr;
    search->phdr = info->dlpi_phdr;
    search->phnum = (uint32_t)(info->dlpi_phnum);

    if ('/' == info->dlpi_name[0])
    {
        if (search->need_path)
        {
            search->path = strdup(info->dlpi_name);
        }
    }
    else if ('/' == search->name[0] || search->need_path)
    {
        char* path = get_path_from_maps((const void *)search->base);

        if (NULL == path) return 0;

        if ('/' == search->name[0])
        {
            if (0 != strcmp(search->name, path))
            {
                free(path);
                return 0;
            }
        }

        if (search->need_path)
        {
            search->path = path;
        }
        else
        {
            free(path);
        }
    }

    return 1;
}

static uint32_t elf_hash(const char *name)
{
    const uint8_t *ptr = (const uint8_t *) name;
    uint32_t h = 0;
    uint32_t g;

    while (*ptr)
    {
        h = (h << 4) + *ptr++;
        g = h & 0xf0000000;
        h ^= g;
        h ^= g >> 24;
    }

    return h;
}

static uint32_t gnu_hash(const char *name)
{
    uint32_t h = 5381;
    const uint8_t* ptr =  (const uint8_t*)(name);

    while (*ptr != 0)
    {
        h += (h << 5) + *ptr++; // h*33 + c = h + h * 32 + c = h + h << 5 + c
    }

    return h;
}

static uint32_t get_sym_index_with_elf_hash(nsoinfo_t* si, const char* symname)
{
    struct elf_hash_struct* hash_struct = &si->elf_hash;

    uint32_t hash = elf_hash(symname);
    uint32_t idx = hash_struct->bucket[hash % hash_struct->nbucket];

    for ( ; idx != 0; idx = hash_struct->chain[idx])
    {
        const ElfW(Sym)* s = si->dynsym + idx;

        if (strcmp(si->dynstr + s->st_name, symname) == 0) {
            break;
        }
    }
    return idx;
}

static uint32_t get_sym_index_with_gnu_hash(nsoinfo_t* si, const char* symname)
{
    struct gnu_hash_struct* hash_struct = &si->gnu_hash;

    uint32_t hash = gnu_hash(symname);
    uint32_t h2 = hash >> hash_struct->shift2;
    uint32_t bloom_mask = sizeof(ElfW(Addr))*8;
    uint32_t word_num = (hash / bloom_mask) & hash_struct->maskwords;
    ElfW(Addr) bloom_word = hash_struct->bloomfilter[word_num];

    uint32_t n;
    uint32_t idx = 0;

    if (((0x1u & (bloom_word >> (hash % bloom_mask)) & (bloom_word >> (h2 % bloom_mask))) != 0)
        && ((n = hash_struct->bucket[hash % hash_struct->nbucket])!= 0))
    {
        do
        {
            const ElfW(Sym) *s = si->dynsym + n;
            if (((hash_struct->chain[n] ^ hash) >> 0x1u) == 0 &&
                strcmp(si->dynstr + s->st_name, symname) == 0)
            {
                idx = n;
                break;
            }
        } while ((hash_struct->chain[n++] & 0x1u) == 0);
    }
    return idx;
}

static uint32_t get_sym_index(nsoinfo_t* si, const char* symname)
{
    uint32_t index = 0;

    if (HAS_GNU_HASH(si->hash_flag))
    {
        index =  get_sym_index_with_gnu_hash(si, symname);
    }

    if (index == 0 && HAS_ELF_HASH(si->hash_flag))
    {
        index = get_sym_index_with_elf_hash(si, symname);
    }

    return index;
}

static void fill_elf_hash_struct(nsoinfo_t* si, ElfW(Addr) header)
{
    struct elf_hash_struct* hash_struct = &si->elf_hash;
    hash_struct->nbucket = ((uint32_t*)header)[0];
    hash_struct->nchain  = ((uint32_t*)header)[1];
    hash_struct->bucket  = (uint32_t*)(header + 8);
    hash_struct->chain   = (uint32_t*)(header + 8 + hash_struct->nbucket * 4);

    SET_ELF_HASH(si->hash_flag);
}

static void fill_gnu_hash_struct(nsoinfo_t* si, ElfW(Addr) header)
{
    struct gnu_hash_struct* hash_struct = &si->gnu_hash;

    hash_struct->nbucket   = ((uint32_t*)header)[0];
    // skip symndx
    hash_struct->maskwords = ((uint32_t*)header)[2];
    hash_struct->shift2    = ((uint32_t*)header)[3];

    hash_struct->bloomfilter = (ElfW(Addr)*)(header + 16);
    hash_struct->bucket = (uint32_t*)(hash_struct->bloomfilter + hash_struct->maskwords);
    // amend chain for symndx = header[1]
    hash_struct->chain = hash_struct->bucket + hash_struct->nbucket - ((uint32_t*)header)[1];

    if (!powerof2(hash_struct->maskwords))
    {
        LOGE(LOG_TAG, "gnu_maskwords=%d", hash_struct->maskwords);
        return;
    }
    hash_struct->maskwords--;

    SET_GNU_HASH(si->hash_flag);
}

static nsoinfo_t* alloc_soinfo()
{
    return (nsoinfo_t *)calloc(1, sizeof(nsoinfo_t));
}

static void free_soinfo(nsoinfo_t* si)
{
    if (si != NULL)
    {
        if (si->path != NULL)
        {
            free(si->path);
        }
        if (si->mapdata != 0)
        {
            munmap(si->mapdata, si->mapsize);
        }
        free(si);
    }
}

static nsoinfo_t* parse_soinfo(dl_search_t* search)
{
    nsoinfo_t* si = alloc_soinfo();
    if (si == NULL) return NULL;

    si->base      = search->base;
    si->phdr      = search->phdr;
    si->phnum     = search->phnum;
    si->path      = search->path;
    si->load_bias = search->load_bias;
    si->hash_flag = 0;

    ElfW(Addr) dynamic = 0;
    for(const ElfW(Phdr) *ph = si->phdr; ph < si->phdr + si->phnum; ph++)
    {
        if (ph->p_type == PT_DYNAMIC)
        {
            dynamic = ph->p_vaddr;
        }
    }

    if (dynamic == 0) goto err;

    si->dynamic = (ElfW(Dyn) *)(dynamic + si->load_bias);

    for (const ElfW(Dyn)* d = si->dynamic; d->d_tag != DT_NULL; ++d)
    {
        switch (d->d_tag)
        {
        case DT_HASH:
            fill_elf_hash_struct(si, si->load_bias + d->d_un.d_ptr);
            break;

        case DT_GNU_HASH:
            fill_gnu_hash_struct(si, si->load_bias + d->d_un.d_ptr);
            break;

        case DT_STRTAB:
            si->dynstr = (const char *)(si->load_bias + d->d_un.d_ptr);
            break;

        case DT_SYMTAB:
            si->dynsym = (ElfW(Sym) *)(si->load_bias + d->d_un.d_ptr);
            break;

        default:
            break;
        }
    }

    if (si->dynsym != NULL && si->dynstr != NULL && si->hash_flag != 0)
    {
        return si;
    }

err:
    free_soinfo(si);

    return NULL;
}

static void* get_so_info(const char* so_name, int need_path)
{
    DL_ITERATE_PHDR dl_iterater = (DL_ITERATE_PHDR)npth_dliterater();

    if (dl_iterater == NULL)
    {
        LOGE(LOG_TAG, "get_so_info annot found dl_iterate_phdr, err msg=%s",dlerror());
        return NULL;
    }

    dl_search_t search;

    memset(&search, 0 , sizeof(dl_search_t));

    search.name = so_name;
    search.need_path = need_path;

    if (1 != dl_iterater(&dl_iterate_phdr_cb, &search))
    {
        LOGI(LOG_TAG, "get_so_info cannot found %s", so_name);
        return NULL;
    }

    if (search.base == 0 || search.load_bias == 0 || search.phdr == NULL || search.phnum == 0)
    {
        LOGE(LOG_TAG, "ehdr=%p, load_bias=%p phdr=%p, phnum=%uz",
             (void *)search.base, (void *)search.load_bias, (const void *)search.phdr, search.phnum);
        return NULL;
    }

    return (void*)parse_soinfo(&search);
}

__attribute__ ((visibility ("default")))
void* npth_dlopen(const char* so_name)
{
    return get_so_info(so_name, 0);
}

__attribute__ ((visibility ("default")))
void* npth_dlopen_full(const char* so_name)
{
    return get_so_info(so_name, 1);
}

__attribute__ ((visibility ("default")))
void* npth_dlsym(void* handle, const char* sym_str)
{
    if (handle == NULL || sym_str == NULL) return NULL;

    nsoinfo_t *si = (nsoinfo_t *) handle;

    uint32_t index = get_sym_index(si, sym_str);

    if (index != 0 && (si->dynsym + index)->st_shndx != SHN_UNDEF)
    {
        return (void*)(si->load_bias + (si->dynsym + index)->st_value);
    }
    return NULL;
}

static int load_symtab(nsoinfo_t *si)
{
    int fd = open(si->path, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
    {
        LOGE(LOG_TAG, "load_symtab open file err, errno=%d path=%s", errno, si->path);
        return -1;
    }

    off_t size = lseek(fd,0L,SEEK_END);

    lseek(fd,0L,SEEK_SET);

    ElfW(Ehdr) *ehdr = (ElfW(Ehdr) *) si->base;

    if (size > 0 && (size_t)(ehdr->e_shoff + ehdr->e_shentsize * ehdr->e_shnum) > (size_t)size)
    {
        close(fd);
        LOGE(LOG_TAG, "load_symtab err size=%zu, end=%zu",
                (size_t)size, (size_t)ehdr->e_shoff + ehdr->e_shentsize * ehdr->e_shnum);
        return -2;
    }

    size_t sh_map_size = (ehdr->e_shentsize * ehdr->e_shnum + (ehdr->e_shoff & 0xffful) + 0xfff) / 0x1000 * 0x1000;

    void* sh_map = mmap(NULL, sh_map_size, PROT_READ, MAP_PRIVATE, fd, (off_t)(ehdr->e_shoff & (~0xffful)));

    if (MAP_FAILED == sh_map) goto out;

    ElfW(Shdr) *section_header = (ElfW(Shdr) *) ((ElfW(Addr))sh_map + (ehdr->e_shoff & 0xffful));

    ElfW(Shdr) *shdr;
    ElfW(Shdr) *symtab_shdr = NULL;
    ElfW(Shdr) *strtab_shdr = NULL;

    for (size_t i = 0; i < ehdr->e_shnum; i ++)
    {
        if (i == (size_t)ehdr->e_shstrndx) continue;

        shdr = section_header + i;

        if (NULL == shdr) continue;

        if (shdr->sh_link >= ehdr->e_shnum) continue;

        if (SHT_SYMTAB == shdr->sh_type && shdr->sh_addr == 0)
        {
            symtab_shdr = shdr;
        }
        else if(SHT_STRTAB ==  shdr->sh_type && shdr->sh_addr == 0)
        {
            strtab_shdr = shdr;
        }
    }

    if (symtab_shdr != NULL && strtab_shdr != NULL)
    {
        ElfW(Off) map_off;
        size_t map_size;

        if (symtab_shdr->sh_offset < strtab_shdr->sh_offset)
        {
            map_off = symtab_shdr->sh_offset;
            map_size = (size_t)(strtab_shdr->sh_offset - symtab_shdr->sh_offset) + strtab_shdr->sh_size;
        }
        else
        {
            map_off = strtab_shdr->sh_offset;
            map_size = (size_t)(symtab_shdr->sh_offset - strtab_shdr->sh_offset) + strtab_shdr->sh_size;
        }

        si->mapsize = (map_size + (map_off & 0xffful) + 0xfff) / 0x1000 * 0x1000;

        void* mem = mmap(NULL, si->mapsize, PROT_READ, MAP_PRIVATE, fd, (off_t)(map_off & (~0xffful)));
        if (MAP_FAILED == mem) goto out;

        si->mapdata = mem;

        si->symtab = (ElfW(Sym) *)((ElfW(Addr))si->mapdata + (map_off & 0xffful) +
                                    (ElfW(Addr))(symtab_shdr->sh_offset - map_off));
        si->nsymtab = symtab_shdr->sh_size / symtab_shdr->sh_entsize;


        si->strtab = (const char*)((ElfW(Addr))si->mapdata + (map_off & 0xffful) +
                                   (ElfW(Addr))(strtab_shdr->sh_offset - map_off));
        si->nstrtab = strtab_shdr->sh_size;
    }

out:
    if (fd > 0) close(fd);

    if (MAP_FAILED != sh_map) munmap(sh_map, sh_map_size);

    return 0;
}

__attribute__ ((visibility ("default")))
void* npth_dlsym_full_with_size(void* handle, const char* sym_str, size_t* sym_size_ptr)
{
    if (handle == NULL || sym_str == NULL) return NULL;

    nsoinfo_t *si = (nsoinfo_t *) handle;

    if (NULL == si->path)
    {
        LOGE(LOG_TAG, "so path is NULL, must us npth_dlopen_full() to open so file");
        return NULL;
    }

    if (si->mapdata == 0) load_symtab(si);

    if (si->symtab == NULL || si->strtab == NULL)
    {
        LOGE(LOG_TAG, "symtab=%zx, strtab=%zx", (size_t)si->symtab, (size_t)si->strtab);
        return NULL;
    }

    void* addr = NULL;

    for (size_t i = 0; i < si->nsymtab; i++)
    {
        const ElfW(Sym)* s = si->symtab + i;

        if (s == NULL) break;

        if (s->st_shndx != SHN_UNDEF  && strcmp(si->strtab +s->st_name, sym_str) == 0)
        {
            if (NULL != sym_size_ptr)
            {
                *sym_size_ptr = s->st_size;
            }
            addr =  (void*)(si->load_bias + s->st_value);
            break;
        }
    }
    return addr;
}

__attribute__ ((visibility ("default")))
void* npth_dlsym_full(void* handle, const char* sym_str)
{
    return npth_dlsym_full_with_size(handle, sym_str, NULL);
}


__attribute__ ((visibility ("default")))
void npth_dlclose(void* handle) {
    free_soinfo((nsoinfo_t *)handle);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-align"

static int elf_read(int fd, ElfW(Off) off, void* buf, size_t cnt)
{
    if ((off_t)off != (off_t)lseek(fd, (off_t)off, SEEK_SET))
    {
        LOGE(LOG_TAG, "lseek faild, errno=%d", errno);
        return -1;
    }

    if ((ssize_t)cnt != read(fd, buf, cnt))
    {
        return -1;
    }

    return 0;
}
#define MAX_BUILD_ID_NUM 160
#define NOTE_ALIGN(size)  (((size) + 3ul) & ~3ul)

typedef struct {
    const char* path;   // in
    char* buildid;      // out
} iterate_arg_t;

static int dl_iterate_phdr_cb_for_build_id(struct dl_phdr_info* info, size_t size, void* data)
{
    if (info->dlpi_name == NULL) return 0;
    iterate_arg_t* arg = (iterate_arg_t *)data;

    const char* cmp1 = arg->path;
    const char* cmp2 = info->dlpi_name;

    if ('/' == cmp1[0] && '/' != cmp2[0])
    {
        cmp1 = strrchr(cmp1, '/') + 1;
    }
    else if ('/' != cmp1[0] && '/' == cmp2[0])
    {
        cmp2 = strrchr(cmp2, '/') + 1;
    }

    if (0 != strcmp(cmp1, cmp2)) return 0;

    const ElfW(Phdr) *phdr = info->dlpi_phdr;
    const ElfW(Phdr) *phdr_end = phdr + info->dlpi_phnum;

    ElfW(Addr) base = get_so_base(info);

    for (phdr = info->dlpi_phdr; phdr < phdr_end; phdr++)
    {
        if(phdr->p_type != PT_NOTE) continue;

        const ElfW(Nhdr) *nhdr = (const ElfW(Nhdr) *)(base + phdr->p_offset);
        const char *nhdr_end = ((const char*)nhdr + phdr->p_memsz - sizeof(*nhdr));

        while((const char*)nhdr < nhdr_end)
        {
            if (nhdr->n_type != NT_GNU_BUILD_ID) {
                nhdr = (const ElfW(Nhdr) *) ((const char *) nhdr + sizeof(*nhdr) +
                                             NOTE_ALIGN(nhdr->n_namesz) +
                                             NOTE_ALIGN(nhdr->n_descsz));
                continue;
            }

            if (nhdr->n_descsz > MAX_BUILD_ID_NUM) return -1;

            char* build_id = malloc(2 * nhdr->n_descsz + 1);
            if (build_id == NULL) return -1;

            const char* addr = (const char*)nhdr + sizeof(*nhdr) +  NOTE_ALIGN(nhdr->n_namesz);

            for(size_t i = 0; i < nhdr->n_descsz; i++)
            {
                sprintf(build_id + 2 * i, "%02hhx", addr[i]);
            }

            build_id[2 * nhdr->n_descsz] = 0;
            arg->buildid = build_id;
            return 1;
        }
    }
    return -1;
}

static char* get_buildid_from_memory(const char* path)
{
    DL_ITERATE_PHDR dl_iterater = (DL_ITERATE_PHDR)npth_dliterater();

    if (dl_iterater == NULL)
    {
        LOGI(LOG_TAG, "get_buildid cannot found dl_iterate_phdr, err msg=%s",dlerror());
        return NULL;
    }
    iterate_arg_t iterate_args = {path, NULL};

    if (1 != dl_iterater(&dl_iterate_phdr_cb_for_build_id, &iterate_args))
    {
        LOGI(LOG_TAG, "get_buildid cannot found %s", path);
        return NULL;
    }
    return iterate_args.buildid;
}

static char* get_buildid_from_file(const char* path)
{
    if (NULL == path) return NULL;

    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
    {
        LOGE(LOG_TAG, "open %s faild, errno=%d", path, errno);
        return NULL;
    }

    char* build_id = NULL;

    ElfW(Ehdr) ehdr;

    if (0 != elf_read(fd, 0, &ehdr, sizeof(ehdr))) goto out;

#define MAX_SECTION_HEEADER_NUM 64
    if (ehdr.e_shnum > MAX_SECTION_HEEADER_NUM)
    {
        LOGE(LOG_TAG, "too many of setction headers = %d", ehdr.e_shnum);
        goto out;
    }

    ElfW(Shdr) shdr_tab[MAX_SECTION_HEEADER_NUM];

    if (0 != elf_read(fd, ehdr.e_shoff, &shdr_tab, ehdr.e_shnum*sizeof(ElfW(Shdr)))) goto out;

    for (size_t i = 0; i < ehdr.e_shnum; i ++)
    {
        if (shdr_tab[i].sh_type != SHT_NOTE) continue;
        if (shdr_tab[i].sh_size < sizeof(ElfW(Nhdr))) continue;

        ElfW(Off) name_off = shdr_tab[ehdr.e_shstrndx].sh_offset + shdr_tab[i].sh_name;

        char name_buf[sizeof(".note.gnu.build-id")];

        if (0 != elf_read(fd, name_off, name_buf, sizeof(name_buf))) goto out;

        if (0 != strcmp(name_buf, ".note.gnu.build-id")) continue;

        ElfW(Nhdr) nhdr;

        if (0 != elf_read(fd, shdr_tab[i].sh_offset, &nhdr, sizeof(nhdr))) goto out;

        if (0 == nhdr.n_descsz || nhdr.n_descsz > shdr_tab[i].sh_size) continue;

        if (nhdr.n_descsz > MAX_BUILD_ID_NUM) continue;

        build_id = (char *)malloc(nhdr.n_descsz*2 + 1);

        ElfW(Off) id_offset = shdr_tab[i].sh_offset + sizeof(nhdr) + NOTE_ALIGN(nhdr.n_namesz);

        char id_buf[MAX_BUILD_ID_NUM];

        if (0 != elf_read(fd, id_offset, id_buf, nhdr.n_descsz)) goto out;

        for (i = 0; i < nhdr.n_descsz; i++)
        {
            sprintf(build_id + 2 * i, "%02hhx", id_buf[i]);
        }
        build_id[2 * nhdr.n_descsz] = 0;
        break;
    }
    out:
    if (fd > 0) close(fd);

    return build_id;
}

__attribute__ ((visibility ("default")))
char* npth_dlbuildid(const char* path)
{
    if (path == NULL) return NULL;

    char* build_id = get_buildid_from_memory(path);

    if (build_id == NULL) build_id = get_buildid_from_file(path);

    return build_id;
}

#ifndef __LP64__

typedef struct link_map link_map_t;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"

typedef struct ksoinfo {
    char            name[128];
    const void*     phdr;
    size_t          phnum;
    void*           entry;
    void*           base;
    unsigned        size;

    uint32_t        unused1;
    void*           dynamic;

    uint32_t        unused2;
    uint32_t        unused3;

    struct ksoinfo* next;
    unsigned        flags;

    const char*     strtab;
    void*           symtab;

    size_t          nbucket;
    size_t          nchain;
    unsigned*       bucket;
    unsigned*       chain;

    unsigned*       plt_got;
    void*           plt_rel;
    size_t          plt_rel_count;

    void*           rel;
    size_t          rel_count;

    void*           preinit_array;
    size_t          preinit_array_count;

    void*           init_array;
    size_t          init_array_count;
    void*           fini_array;
    size_t          fini_array_count;

    void*           init_func;
    void*           fini_func;

    unsigned*       ARM_exidx;
    size_t          ARM_exidx_count;

    size_t          ref_count;
    link_map_t      link_map;

    char            constructors_called;

    void*           load_bias;

    char            has_text_relocations;
    char            has_DT_SYMBOLIC;
} ksoinfo_t;
#pragma clang diagnostic pop

#define is_dl_info(_si) ((_si)->flags == 1 && (_si)->strtab != 0 && (_si)->symtab != 0 \
&& (_si)->nbucket == 1 && (_si)->nchain == 8 && (_si)->bucket != 0 && (_si)->chain != 0 \
&& (_si)->constructors_called == 0 && (_si)->has_text_relocations == 0 \
&& (_si)->has_DT_SYMBOLIC == 1 && (_si)->load_bias == 0)

static int k_dl_iterate_phdr(int (*cb)(struct dl_phdr_info *info, size_t size, void *data), void *data)
{
    ksoinfo_t* solist = dlopen(NULL, RTLD_LOCAL);

    if (solist == NULL) return 1;

    int rv = 0;
    for (ksoinfo_t* si = solist; si != NULL; si = si->next) {
        struct dl_phdr_info dl_info;
        dl_info.dlpi_addr = si->link_map.l_addr;
        dl_info.dlpi_name = si->link_map.l_name;
        dl_info.dlpi_phdr = si->phdr;
        dl_info.dlpi_phnum = (ElfW(Half))si->phnum;
        rv = cb(&dl_info, sizeof(struct dl_phdr_info), data);
        if (rv != 0) {
            break;
        }
    }

    dlclose(solist);

    return rv;
}

#endif

__attribute__ ((visibility ("default")))
void* npth_dliterater(void)
{
    void* handle = dlopen("libdl.so", RTLD_NOW);

    if (NULL == handle) return NULL;

    void *iterate = dlsym(handle, "dl_iterate_phdr");

#ifndef __LP64__
    if (iterate == NULL /* && api_level < 21 */)
    {
        struct ksoinfo* si = (struct ksoinfo *)handle;
        if(is_dl_info(si))
        {
            return (void*)&k_dl_iterate_phdr;
        }
    }
#endif
    dlclose(handle);

    return iterate;
}

#pragma clang diagnostic pop

char* get_routine_so_path(size_t routine, size_t* start, size_t* end) {
    char *path = NULL;
    char line[1024] = {0};
    char seg_addr[64] = {0};
    FILE *f = fopen("/proc/self/maps", "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            if (1 == sscanf(line, "%[^ ]", seg_addr)) {
                char* seperator = strchr(seg_addr, '-');
                if (seperator) {
                    *seperator = 0;
                } else {
                    continue;
                }
                *start = strtoll(seg_addr, NULL, 16);
                *end = strtoll(seperator+1, NULL, 16);
                if (routine >= *start && routine <= *end) {
                    path = strchr(line, '/');
                    if (path) {
                        char* last = strrchr(path, ' ');
                        if (NULL == last) {
                            last = strrchr(path, '\n');
                        }
                        if (last) {
                            *last = '\0';
                        }
                    }
                    break;
                } else {
                    *start = 0;
                    *end = 0;
                }
            }
        }
        fclose(f);
    }
    if (path) {
        return strdup(path);
    } else {
        return NULL;
    }
}