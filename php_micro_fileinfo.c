/*
micro SAPI for PHP - php_micro_filesize.c
filesize reproduction utilities for micro

Copyright 2020 Longyan
Copyright 2022 Yun Dou <dixyes@gmail.com>

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "php.h"

#include "php_micro.h"
#include "php_micro_helper.h"

#include <stdint.h>
#if defined(PHP_WIN32)
#    include "win32/codepage.h"
#    include <windows.h>
#elif defined(__linux) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
#    include <elf.h>
#    if defined(__linux) || defined(__FreeBSD__)
#        include <sys/auxv.h> // for getauxval / elf_aux_info
#    else
#        include <sys/sysctl.h> // for sysctl
#    endif                      // defined(__linux) || defined(__FreeBSD__)
#    if defined(__LP64__)
typedef Elf64_Ehdr Elf_Ehdr;
typedef Elf64_Shdr Elf_Shdr;
typedef Elf64_Phdr Elf_Phdr;
#        define ELFCLASS ELFCLASS64
#    else
typedef Elf32_Ehdr Elf_Ehdr;
typedef Elf32_Phdr Elf_Phdr;
#        define ELFCLASS ELFCLASS32
#    endif
#elif defined(__APPLE__)
#    include <mach-o/dyld.h>
#    include <mach-o/getsect.h>
#    include <mach-o/fat.h>
#    if defined(__LP64__)
typedef struct mach_header_64 mach_header;
typedef struct segment_command_64 segment_command;
#        define MACH_HEADER_MAGIC   MH_MAGIC_64
#        define LOADCOMMAND_SEGMENT LC_SEGMENT_64
#    else
typedef struct mach_header mach_header;
typedef struct segment_command segment_command;
#        define LOADCOMMAND_SEGMENT LC_SEGMENT
#    endif
#else
#    error because we donot support that platform yet
#endif

#ifndef PHP_WIN32
#    include <errno.h>
#    include <fcntl.h>
#    include <sys/stat.h>
#    include <sys/types.h>
#    include <unistd.h>
#endif // ndef PHP_WIN32

const char *micro_get_filename(void);

static size_t _final_sfxsize = 0;
static size_t _sfxsize_limit = 0;
int _micro_init_sfxsize(void);

size_t micro_get_sfxsize(void) {
    return _final_sfxsize;
}

size_t micro_get_sfxsize_limit(void) {
    return _sfxsize_limit;
}

typedef struct _ext_ini_header_t {
    uint8_t magic[4];
    uint8_t len[4];
} ext_ini_header_t;

struct _ext_ini {
    size_t size;
    char *data;
} micro_ext_ini = {.size = 0, .data = NULL};

// shabby endian-independent check
#define checkmagic(var) \
    (var[0] != PHP_MICRO_INIMARK[0] || var[1] != PHP_MICRO_INIMARK[1] || var[2] != PHP_MICRO_INIMARK[2] || \
        var[3] != PHP_MICRO_INIMARK[3])

#ifdef PHP_WIN32
const wchar_t *micro_get_filename_w();
#endif // PHP_WIN32

int micro_fileinfo_init(void) {
    int ret = 0;
    size_t sfxsize;
    size_t filesize;
    uint32_t ini_len = 0;

    if (SUCCESS != _micro_init_sfxsize()) {
        return FAILURE;
    }

    sfxsize = micro_get_sfxsize();

#ifdef PHP_WIN32
    LARGE_INTEGER _filesize;
    LPCWSTR self_path = micro_get_filename_w();
    HANDLE handle = CreateFileW(self_path,
        FILE_ATTRIBUTE_READONLY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);
    if (INVALID_HANDLE_VALUE == handle) {
        ret = FAILURE;
        goto end;
    }
    if (!GetFileSizeEx(handle, &_filesize)) {
        fwprintf(stderr, L"failed get file size: %d\n", GetLastError());
        ret = GetLastError();
        goto end;
    }
    filesize = _filesize.QuadPart;
    if (filesize <= sfxsize) {
        fwprintf(stderr, L"no payload found.\n" PHP_MICRO_HINT, self_path);
        ret = FAILURE;
        goto end;
    }
#    define seekfile(x) \
        do { \
            _filesize.QuadPart = x; \
            SetFilePointer(handle, _filesize.LowPart, &_filesize.HighPart, FILE_BEGIN); \
        } while (0)
#    define readfile(dest, size, red) \
        do { \
            ReadFile(handle, dest, size, &red, NULL); \
        } while (0)
#    define closefile() \
        do { \
            if (INVALID_HANDLE_VALUE != handle) { \
                CloseHandle(handle); \
            } \
        } while (0)
#else
    const char *self_path = micro_get_filename();
    int fd = open(self_path, O_RDONLY);
    if (-1 == fd) {
        // TODO: tell failed here
        ret = errno;
        goto end;
    }
    struct stat stats;
    ret = stat(self_path, &stats);
    if (-1 == ret) {
        // TODO: tell failed here
        ret = errno;
        goto end;
    }
    filesize = stats.st_size;
    dbgprintf("%zd, %zd\n", sfxsize, filesize);
    if (filesize <= sfxsize) {
        fprintf(stderr, "no payload found.\n" PHP_MICRO_HINT, self_path);
        ret = FAILURE;
        goto end;
    }
#    define seekfile(x) \
        do { \
            lseek(fd, x, SEEK_SET); \
        } while (0)
#    define readfile(dest, size, red) \
        do { \
            red = read(fd, dest, size); \
        } while (0)
#    define closefile() \
        do { \
            if (-1 != fd) { \
                close(fd); \
            } \
        } while (0)
#endif // PHP_WIN32
    ext_ini_header_t ext_ini_header = {0};
    if (filesize <= sfxsize + sizeof(ext_ini_header)) {
        ret = SUCCESS;
        goto end;
    }
    // we may have extra ini configs.
    seekfile(sfxsize);
    int red = 0;
    readfile(&ext_ini_header, sizeof(ext_ini_header), red);
    if (red < 0) {
        // cannot read ini header
        ret = errno;
        goto end;
    } else if (sizeof(ext_ini_header) != red) {
        ret = SUCCESS;
        goto end;
    }

    if (checkmagic(ext_ini_header.magic)) {
        // bad magic, not an extra ini
        ret = SUCCESS;
        goto end;
    }
    // shabby ntohl
    ini_len = (ext_ini_header.len[0] << 24) + (ext_ini_header.len[1] << 16) + (ext_ini_header.len[2] << 8) +
        ext_ini_header.len[3];
    if (filesize <= sfxsize + sizeof(ext_ini_header) + ini_len) {
        // bad len, not an extra ini
        ret = SUCCESS;
        ini_len = 0;
        goto end;
    }
    micro_ext_ini.data = malloc(ini_len + 2);
    readfile(micro_ext_ini.data, ini_len, red);
    if (ini_len != red) {
        // cannot read file
        ret = errno;
        ini_len = 0;
        free(micro_ext_ini.data);
        micro_ext_ini.data = NULL;
        goto end;
    }
    // two '\0's like hardcoden inis
    micro_ext_ini.data[ini_len] = '\0';
    micro_ext_ini.data[ini_len + 1] = '\0';
    dbgprintf("using ext ini %s\n", micro_ext_ini.data);
    micro_ext_ini.size = ini_len + 1;
    ini_len += sizeof(ext_ini_header_t);

end:
    dbgprintf("embed ini len is %d\n", ini_len);
    _final_sfxsize = sfxsize + ini_len;
    closefile();
    return ret;
#undef seekfile
#undef readfile
#undef closefile
}

/**
 * In windows, this struct is stored as RC_DATA, with id PHP_MICRO_SFXSIZE_ID
 * In ELF, this struct is located at .sfxsize section
 * In Mach-O, for common macho, this struct is located at __DATA,__micro_sfxsize section
 * In Mach-O, for fat macho, this struct is located at
 *   start of the CPU_TYPE_X86 fat arch binary + 0x1000 (4096 is minimal size for a stub executable)
 * If sections not set,
 * size will be:
 * Windows uses the end of the last section, this supports UPX
 * ELF uses the end of the last section or last program header segment, this do not support UPX
 * Mach-O uses the end of __LINKEDIT
 * Mach-O fat binary uses the end of the last fat arch
 * limit will be 0 (no limit)
 */
typedef struct _micro_sfxsize_section_t {
    /* sfx executable size in bytes, big endian */
    uint8_t sizeBytes[8];
    /**
     * limit offset from the whole file begin
     * in bytes, big endian, maybe omit, omitted or 0 for no limit
     */
    uint8_t limitBytes[8];
} micro_sfxsize_section_t;

/*
 *   _micro_get_sfxsize - get (real) sfx size using platform-specific method (see above)
 */
int _micro_init_sfxsize(void) {
    micro_sfxsize_section_t sfxsizeSection = {{0}, {0}};
#define bytesntollh(bytes, ll) \
    do { \
        ll = (uint64_t)(bytes[0]) << 56 | (uint64_t)(bytes[1]) << 48 | (uint64_t)(bytes[2]) << 40 | \
            (uint64_t)(bytes[3]) << 32 | (uint64_t)(bytes[4]) << 24 | (uint64_t)(bytes[5]) << 16 | \
            (uint64_t)(bytes[6]) << 8 | (uint64_t)(bytes[7]); \
    } while (0)
#define read_sfxsize_section(size) \
    do { \
        if (size >= sizeof(uint64_t)) { \
            bytesntollh(sfxsizeSection.sizeBytes, _final_sfxsize); \
        } else { \
            fprintf(stderr, "bad sfxsize section\n"); \
            return FAILURE; \
        } \
        if (size >= 2 * sizeof(uint64_t)) { \
            bytesntollh(sfxsizeSection.limitBytes, _sfxsize_limit); \
            if (_sfxsize_limit < _final_sfxsize) { \
                fprintf(stderr, "bad sfxsize limit\n"); \
                return FAILURE; \
            } \
        } \
    } while (0)
#define update_sfxsize(newEnd) \
    do { \
        _final_sfxsize = _final_sfxsize > newEnd ? _final_sfxsize : newEnd; \
    } while (0)

#if defined(PHP_WIN32)
    // get resource
    const char *errMsg = "";
    DWORD resourceSize;
    HRSRC resource = FindResourceA(NULL, MAKEINTRESOURCEA(PHP_MICRO_SFXSIZE_ID), RT_RCDATA);
    HANDLE hFile = INVALID_HANDLE_VALUE;
    DWORD readBytes;
    IMAGE_DOS_HEADER dosHeader;
    IMAGE_NT_HEADERS ntHeader;
    IMAGE_SECTION_HEADER *sectionHeaders;
    dbgprintf("resource: %p\n", resource);
    if (NULL != resource) {
        resourceSize = SizeofResource(NULL, resource);
        // read as big endian
        if (resourceSize > sizeof(micro_sfxsize_section_t)) {
            resourceSize = sizeof(micro_sfxsize_section_t);
        }
        memcpy((void *)&sfxsizeSection, LockResource(LoadResource(NULL, resource)), resourceSize);
        read_sfxsize_section(resourceSize);
        dbgprintf("using resource: %zd, %zd\n", _final_sfxsize, _sfxsize_limit);
        return SUCCESS;
    }
    dbgprintf("FindResourceA: %08x\n", GetLastError());

    hFile = CreateFileW(micro_get_filename_w(),
        FILE_ATTRIBUTE_READONLY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);
    if (INVALID_HANDLE_VALUE == hFile) {
        dbgprintf("CreateFileW: %08x\n", GetLastError());
        errMsg = "cannot open self file";
        goto error;
    }

    if (TRUE != ReadFile(hFile, &dosHeader, sizeof(IMAGE_DOS_HEADER), &readBytes, NULL) ||
        sizeof(IMAGE_DOS_HEADER) != readBytes) {
        dbgprintf("ReadFile: %08x\n", GetLastError());
        errMsg = "cannot read dos header (corrupt micro sfx)";
        goto error;
    }

    if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE) {
        errMsg = "bad dos header (corrupt micro sfx)";
        goto error;
    }

    if (INVALID_SET_FILE_POINTER == SetFilePointer(hFile, dosHeader.e_lfanew, NULL, FILE_BEGIN)) {
        errMsg = "cannot seek to nt header (corrupt micro sfx)";
        goto error;
    }
    if (TRUE != ReadFile(hFile, &ntHeader, sizeof(IMAGE_NT_HEADERS), &readBytes, NULL) ||
        sizeof(IMAGE_NT_HEADERS) != readBytes) {
        dbgprintf("ReadFile: %08x\n", GetLastError());
        errMsg = "cannot read nt header (corrupt micro sfx)";
        goto error;
    }

    if (ntHeader.Signature != IMAGE_NT_SIGNATURE ||
        ntHeader.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER32) ||
        ntHeader.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC ||
        ntHeader.OptionalHeader.NumberOfRvaAndSizes != IMAGE_NUMBEROF_DIRECTORY_ENTRIES) {
        errMsg = "bad nt header (corrupt micro sfx)";
        goto error;
    }

    sectionHeaders = malloc(sizeof(IMAGE_SECTION_HEADER) * ntHeader.FileHeader.NumberOfSections);

    if (TRUE !=
            ReadFile(hFile,
                sectionHeaders,
                sizeof(IMAGE_SECTION_HEADER) * ntHeader.FileHeader.NumberOfSections,
                &readBytes,
                NULL) ||
        sizeof(IMAGE_SECTION_HEADER) * ntHeader.FileHeader.NumberOfSections != readBytes) {
        dbgprintf("ReadFile: %08x\n", GetLastError());
        dbgprintf("section headers: %d\n", ntHeader.FileHeader.NumberOfSections);
        dbgprintf("read: %d\n", readBytes);
        errMsg = "cannot read section headers (corrupt micro sfx)";
        goto error;
    }

    for (int i = 0; i < ntHeader.FileHeader.NumberOfSections; i++) {
        update_sfxsize(sectionHeaders[i].PointerToRawData + sectionHeaders[i].SizeOfRawData);
    }

error:
    if (INVALID_HANDLE_VALUE != hFile) {
        CloseHandle(hFile);
    }
    if (NULL != sectionHeaders) {
        free(sectionHeaders);
    }
    if (0 == _final_sfxsize) {
        fprintf(stderr, "failed get sfxsize: %s\n", errMsg);
        return FAILURE;
    }

    return SUCCESS;
#elif defined(__APPLE__)
    // get file header first
    const char *self_path = micro_get_filename();

    int fd = open(self_path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "cannot open self file: %s\n", self_path);
        return FAILURE;
    }

    // get file header
    uint32_t magic;
    if (sizeof(uint32_t) != read(fd, &magic, sizeof(uint32_t))) {
        fprintf(stderr, "cannot read file header magic: %s\n", self_path);
        return FAILURE;
    }
    if (magic == FAT_MAGIC || magic == FAT_CIGAM) {
        // fat file
        uint32_t nfat_arch;
        if (sizeof(uint32_t) != read(fd, &nfat_arch, sizeof(uint32_t))) {
            fprintf(stderr, "cannot read fat header nfat_arch: %s\n", self_path);
            return FAILURE;
        }
        nfat_arch = ntohl(nfat_arch);
        _final_sfxsize = 0;
        for (int i = 0; i < nfat_arch; i++) {
            struct fat_arch fat_arch;
            if (sizeof(fat_arch) != read(fd, &fat_arch, sizeof(fat_arch))) {
                fprintf(stderr, "cannot read fat arch: %s\n", self_path);
                return FAILURE;
            }
            // set to the end of the last fat arch
            uint32_t archOffset = ntohl(fat_arch.offset);
            uint32_t archSize = ntohl(fat_arch.size);
            _final_sfxsize = archOffset + archSize;
            if (fat_arch.cputype == htonl((cpu_type_t)CPU_TYPE_X86)) {
                // 0x1000 is minimal size for a stub executable
                // this will skip the stub executable
                if (lseek(fd, archOffset + 0x1000, SEEK_SET) != archOffset + 0x1000) {
                    fprintf(stderr, "cannot seek to fat macho: %s\n", self_path);
                    return FAILURE;
                }
                uint32_t sfxsizeSectionSize = archSize - 0x1000;
                if (sfxsizeSectionSize > sizeof(micro_sfxsize_section_t)) {
                    sfxsizeSectionSize = sizeof(micro_sfxsize_section_t);
                }
                if (sfxsizeSectionSize != read(fd, &sfxsizeSection, sfxsizeSectionSize)) {
                    fprintf(stderr, "cannot read fat macho: %s\n", self_path);
                    return FAILURE;
                }
                read_sfxsize_section(sfxsizeSectionSize);
                dbgprintf("using fat macho arch CPU_TYPE_X86: %zd, %zd\n", _final_sfxsize, _sfxsize_limit);
                return SUCCESS;
            }
        }

        dbgprintf("using end of fat macho: %zd, %zd\n", _final_sfxsize, _sfxsize_limit);
        return SUCCESS;
    }
    // get mach header
    mach_header *header = (mach_header *)_dyld_get_image_header(0);
    const segment_command *linkedit = NULL;
    unsigned long size = 0;

    const char *section = (const char *)getsectiondata(header, "__DATA", "__micro_sfxsize", &size);
    if (NULL == section) {
        linkedit = getsegbyname("__LINKEDIT");
        if (NULL == linkedit) {
            fprintf(stderr, "no __LINKEDIT segment found (corrupt micro sfx).\n");
            return FAILURE;
        }
        _final_sfxsize = linkedit->fileoff + linkedit->filesize;
        dbgprintf("no __DATA,__micro_sfxsize section found, use end of __LINKEDIT: %zd\n", _final_sfxsize);
        return SUCCESS;
    }

    if (size > sizeof(micro_sfxsize_section_t)) {
        size = sizeof(micro_sfxsize_section_t);
    }
    memcpy((void *)&sfxsizeSection, section, size);
    read_sfxsize_section(size);
    dbgprintf("using __DATA,__micro_sfxsize section: %zd, %zd\n", _final_sfxsize, _sfxsize_limit);

    return SUCCESS;
#elif defined(__linux) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
    // get section header
    Elf_Ehdr ehdr;
    Elf_Shdr *shdrs = NULL, *shdr, *strtabhdr;
    Elf_Phdr *phdrs = NULL;
    int i;
    const char *errMsg = "";
    char *strtab = NULL;
    const char *self_path = micro_get_filename();

    int fd = open(self_path, O_RDONLY);
    if (fd < 0) {
        errMsg = "cannot open self file";
        goto error;
    }

    if (sizeof(Elf_Ehdr) != read(fd, &ehdr, sizeof(Elf_Ehdr))) {
        errMsg = "cannot read elf header";
        goto error;
    }

    if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0 || ehdr.e_ident[EI_CLASS] != ELFCLASS ||
        sizeof(Elf_Ehdr) != ehdr.e_ehsize || (ehdr.e_shentsize > 0 && sizeof(Elf_Shdr) != ehdr.e_shentsize) ||
        (ehdr.e_phentsize > 0 && sizeof(Elf_Phdr) != ehdr.e_phentsize)) {
        errMsg = "bad elf header (corrupt micro sfx)";
        goto error;
    }

    // read section headers
    if (ehdr.e_shnum > 0) {
        dbgprintf("section headers: %d, offset %016lx\n", ehdr.e_shnum, (uint64_t)ehdr.e_shoff);
        update_sfxsize(ehdr.e_shoff + ehdr.e_shnum * sizeof(Elf_Shdr));
        dbgprintf("%d: sfxsize: %zd\n", __LINE__, _final_sfxsize);

        shdrs = malloc(sizeof(Elf_Shdr) * ehdr.e_shnum);
        if (NULL == shdrs) {
            // impossible
            errMsg = "cannot allocate memory for section header";
            goto error;
        }

        if (lseek(fd, ehdr.e_shoff, SEEK_SET) != ehdr.e_shoff) {
            errMsg = "cannot seek to section header (corrupt micro sfx)";
            goto error;
        }
        if (read(fd, shdrs, sizeof(Elf_Shdr) * ehdr.e_shnum) != sizeof(Elf_Shdr) * ehdr.e_shnum) {
            errMsg = "cannot read section headers";
            goto error;
        }

        // read strtab
        strtabhdr = &shdrs[ehdr.e_shstrndx];
        strtab = malloc(strtabhdr->sh_size + 1);
        if (NULL == strtab) {
            // impossible
            errMsg = "cannot allocate memory for section header string table";
            goto error;
        }
        if (lseek(fd, strtabhdr->sh_offset, SEEK_SET) != strtabhdr->sh_offset) {
            errMsg = "cannot seek to section header string table";
            goto error;
        }
        if (read(fd, strtab, strtabhdr->sh_size) != strtabhdr->sh_size) {
            errMsg = "cannot read section header string table";
            goto error;
        }
        strtab[strtabhdr->sh_size] = '\0';

        for (i = 0; i < ehdr.e_shnum; i++) {
            shdr = &shdrs[i];
            if (shdr->sh_type == SHT_NOBITS) {
                // if no data, skip it
                continue;
            }
            update_sfxsize(shdr->sh_offset + shdr->sh_size);
            dbgprintf("%d: sfxsize: %zd\n", __LINE__, _final_sfxsize);
            if (shdr->sh_name > strtabhdr->sh_size) {
                errMsg = "bad section header name (corrupt micro sfx)";
                goto error;
            }
            dbgprintf("%016lx %016lx %s type %08x\n",
                (uint64_t)shdr->sh_offset,
                (uint64_t)shdr->sh_size,
                &strtab[shdr->sh_name],
                shdr->sh_type);
            dbgprintf("%016lx %08x\n", shdr->sh_flags, shdr->sh_info);
            if (shdr->sh_type != 0x1 /* application spec */ || shdr->sh_name > SHN_LORESERVE ||
                strncmp(&strtab[shdr->sh_name], ".sfxsize", 8)) {
                continue;
            }
            if (lseek(fd, shdr->sh_offset, SEEK_SET) != shdr->sh_offset) {
                errMsg = "cannot seek to .sfxsize section header (corrupt micro sfx)";
                goto error;
            }
            if (shdr->sh_size > sizeof(micro_sfxsize_section_t)) {
                // i'm lazy to create another size_t
                shdr->sh_size = sizeof(micro_sfxsize_section_t);
            }
            // read as big endian
            if (read(fd, &sfxsizeSection, shdr->sh_size) != shdr->sh_size) {
                errMsg = "cannot read .sfxsize section header (corrupt micro sfx)";
                goto error;
            }
            read_sfxsize_section(shdr->sh_size);
            // at this time, things must be allocated
            dbgprintf("using .sfxsize section: %zd, %zd\n", _final_sfxsize, _sfxsize_limit);
            close(fd);
            free(shdrs);
            free(strtab);
            return SUCCESS;
        }
    }

    // try to get elf size
    // _final_sfxsize is now max section size
    update_sfxsize(ehdr.e_shoff + ehdr.e_shnum * sizeof(Elf_Shdr));
    dbgprintf("%d: sfxsize: %zd\n", __LINE__, _final_sfxsize);

    phdrs = malloc(sizeof(Elf_Phdr) * ehdr.e_phnum);
    if (NULL == phdrs) {
        // impossible
        errMsg = "cannot allocate memory for program header";
        goto error;
    }

    if (lseek(fd, ehdr.e_phoff, SEEK_SET) != ehdr.e_phoff) {
        errMsg = "cannot seek to program header";
        goto error;
    }
    if (read(fd, phdrs, sizeof(Elf_Phdr) * ehdr.e_phnum) != sizeof(Elf_Phdr) * ehdr.e_phnum) {
        errMsg = "cannot read program header";
        goto error;
    }

    for (i = 0; i < ehdr.e_phnum; i++) {
        update_sfxsize(phdrs[i].p_offset + phdrs[i].p_filesz);
    }

error:
    if (shdrs) {
        free(shdrs);
    }
    if (strtab) {
        free(strtab);
    }
    if (phdrs) {
        free(phdrs);
    }
    if (fd >= 0) {
        close(fd);
    }
    if (0 == _final_sfxsize) {
        fprintf(stderr, "failed get sfxsize: %s\n", errMsg);
        return FAILURE;
    }
    return SUCCESS;
#endif
#undef bytesntollh
#undef read_sfxsize_section
#undef update_sfxsize
}

#ifdef PHP_WIN32

const wchar_t *micro_get_filename_w() {
    static LPWSTR self_filename = NULL;
    // dbgprintf("fuck %S\n", self_filename);
    if (self_filename) {
        return self_filename;
    }
    DWORD self_filename_chars = MAX_PATH;
    self_filename = malloc(self_filename_chars * sizeof(WCHAR));
    DWORD wapiret = 0;

    DWORD (*myGetModuleFileNameExW)(HANDLE, HMODULE, LPWSTR, DWORD);

    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    myGetModuleFileNameExW = (void *)GetProcAddress(hKernel32, "K32GetModuleFileNameExW");
    if (NULL == myGetModuleFileNameExW) {
        HMODULE hPsapi = GetModuleHandleW(L"psapi.dll");
        myGetModuleFileNameExW = (void *)GetProcAddress(hPsapi, "GetModuleFileNameExW");
    }
    if (NULL == myGetModuleFileNameExW) {
        dbgprintf("cannot get self path via win32api\n");
        return NULL;
    }
    while (self_filename_chars ==
        (wapiret = myGetModuleFileNameExW(GetCurrentProcess(), NULL, self_filename, self_filename_chars))) {
        dbgprintf("wapiret is %d\n", wapiret);
        dbgprintf("lensize is %d\n", self_filename_chars);
        if (
#    if WINVER < _WIN32_WINNT_VISTA
            ERROR_SUCCESS
#    else
            ERROR_INSUFFICIENT_BUFFER
#    endif
            == GetLastError()) {
            self_filename_chars += MAX_PATH;
            self_filename = realloc(self_filename, self_filename_chars * sizeof(WCHAR));
        } else {
            dbgprintf("cannot get self path\n");
            return NULL;
        }
    };
    dbgprintf("wapiret is %d\n", wapiret);
    dbgprintf("lensize is %d\n", self_filename_chars);

    if (wapiret > MAX_PATH && memcmp(L"\\\\?\\", self_filename, 4 * sizeof(WCHAR))) {
        dbgprintf("\\\\?\\-ize self_filename\n");
        LPWSTR buf = malloc((wapiret + 5) * sizeof(WCHAR));
        memcpy(buf, L"\\\\?\\", 4 * sizeof(WCHAR));
        memcpy(buf + 4, self_filename, wapiret * sizeof(WCHAR));
        buf[wapiret + 4] = L'\0';
        free(self_filename);
        self_filename = buf;
    }

    dbgprintf("self is %S\n", self_filename);

    return self_filename;
}

const char *micro_get_filename(void) {
    static char *self_filename = NULL;
    if (NULL != self_filename) {
        return self_filename;
    }
    int self_filename_len;
    LPWCH self_filename_w = micro_get_filename_w();
    if (NULL == self_filename_w) {
        return NULL;
    }

    self_filename_len = WideCharToMultiByte(CP_UTF8, 0, self_filename_w, -1, NULL, 0, NULL, NULL);
    if (0 == self_filename_len) {
        // null string? wtf?
        return NULL;
    }

    self_filename = malloc(self_filename_len);
    if (NULL == self_filename) {
        // impossible
        return NULL;
    }

    if (0 == WideCharToMultiByte(CP_UTF8, 0, self_filename_w, -1, self_filename, self_filename_len, NULL, NULL)) {
        // impossible
        free(self_filename);
        return NULL;
    }

    return self_filename;
}

#elif defined(__linux)
// use getauxval AT_EXECFN
const char *micro_get_filename(void) {
    static char *self_filename = NULL;
    if (NULL == self_filename) {
        self_filename = malloc(PATH_MAX);
        (void)realpath((const char *)getauxval(AT_EXECFN), self_filename);
    }
    return self_filename;
}
#elif defined(__FreeBSD__)
// use elf_aux_info AT_EXECPATH
const char *micro_get_filename(void) {
    static char *self_filename = NULL;
    char filename[PATH_MAX];
    if (NULL == self_filename) {
        self_filename = malloc(PATH_MAX);
        if (0 != elf_aux_info(AT_EXECPATH, filename, sizeof(filename))) {
            return NULL;
        }
        (void)realpath((const char *)filename, self_filename);
    }
    return self_filename;
}
#elif defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
// use sysctl
const char *micro_get_filename(void) {
    static char *self_filename = NULL;
    int mib[4];
    size_t len;
    char *buf;
    if (NULL == self_filename) {
        self_filename = malloc(PATH_MAX);
#    if defined(__NetBSD__) && defined(KERN_PROC_PATHNAME)
        mib[0] = CTL_KERN;
        mib[1] = KERN_PROC_ARGS;
        mib[2] = getpid();
        mib[3] = KERN_PROC_PATHNAME;
#    elif defined(__DragonFly__)
        mib[0] = CTL_KERN;
        mib[1] = KERN_PROC;
        mib[2] = KERN_PROC_PATHNAME;
        mib[3] = -1;
#    else // openbsd or netbsd without KERN_PROC_PATHNAME
        // fall back to argv[0], this is fragile
        // if
        //    - argv is changed
        //    - parent sets argv[0] to another value
        //    - called in $PATH
        // there will be a wrong value
        // so if you understand this limit, comment the following line
#        error "not support this system yet"
        mib[0] = CTL_KERN;
        mib[1] = KERN_PROC_ARGS;
        mib[2] = getpid();
        mib[3] = KERN_PROC_ARGV;
#    endif

        if (sysctl(mib, 4, NULL, &len, NULL, 0) != 0) {
            return NULL;
        }

        buf = malloc(len);
        if (NULL == buf) {
            // impossible
            return NULL;
        }

        if (sysctl(mib, 4, buf, &len, NULL, 0) != 0) {
            free(buf);
            return NULL;
        }

#    if (defined(__NetBSD__) && defined(KERN_PROC_PATHNAME)) || defined(__DragonFly__)
        (void)realpath(buf, self_filename);
#    elif defined(__OpenBSD__)
        (void)realpath(((char **)buf)[0], self_filename);
#    else
#        error "not support this system yet"
#    endif
        free(buf);
    }
    return self_filename;
}
#elif defined(__APPLE__)
const char *micro_get_filename(void) {
    static char *self_path = NULL;
    if (NULL == self_path) {
        uint32_t len = 0;
        if (-1 != _NSGetExecutablePath(NULL, &len)) {
            goto error;
        }
        self_path = malloc(len);
        if (NULL == self_path) {
            goto error;
        }
        if (0 != _NSGetExecutablePath(self_path, &len)) {
            goto error;
        }

        // realpath it
        char *real_path = malloc(PATH_MAX);
        if (NULL == real_path) {
            goto error;
        }
        if (NULL == realpath(self_path, real_path)) {
            goto error;
        }
        free(self_path);
        self_path = real_path;
    }
    return self_path;
error:
    if (NULL != self_path) {
        self_path[0] = '\0';
    }
    return NULL;
}
#else
#    error "not support this system yet"
#endif

size_t micro_get_filename_len(void) {
    static size_t _micro_filename_l = -1;
    if (-1 == _micro_filename_l) {
        _micro_filename_l = strlen(micro_get_filename());
    }
    return _micro_filename_l;
}

// deprecated
#if MICRO_USE_OLD_PHAR_HOOK
int is_stream_self(php_stream *stream) {
    dbgprintf("checking %s\n", stream->orig_path);
#    ifdef PHP_WIN32
    LPCWSTR stream_path_w = php_win32_ioutil_any_to_w(stream->orig_path);
    size_t stream_path_w_len = wcslen(stream_path_w);
    LPCWSTR my_path_w = micro_get_filename_w();
    size_t my_path_w_len = wcslen(my_path_w);
    dbgprintf("with self: %S\n", my_path_w);
    if (my_path_w_len == stream_path_w_len && 0 == wcscmp(stream_path_w, my_path_w)) {
#    else
    const char *stream_path = stream->orig_path;
    size_t stream_path_len = strlen(stream_path);
    const char *my_path = micro_get_filename();
    size_t my_path_len = strlen(my_path);
    dbgprintf("with self: %s\n", my_path);
    if (my_path_len == stream_path_len && 0 == strcmp(stream_path, my_path)) {
#    endif
        dbgprintf("is self\n");
        return 1;
    }
    dbgprintf("not self\n");
    return 0;
}
#endif

PHP_FUNCTION(micro_get_self_filename) {
    RETURN_STRING(micro_get_filename());
}

PHP_FUNCTION(micro_get_sfx_filesize) {
    zend_error(E_DEPRECATED, "micro_get_sfx_filesize is deprecated, use micro_get_sfxsize instead");
    RETURN_LONG(micro_get_sfxsize());
}

PHP_FUNCTION(micro_get_sfxsize) {
    RETURN_LONG(micro_get_sfxsize());
}

PHP_FUNCTION(micro_get_sfxsize_limit) {
    RETURN_LONG(micro_get_sfxsize_limit());
}
