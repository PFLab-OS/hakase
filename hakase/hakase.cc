#include <assert.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <toshokan/hakase/elf_loader.h>
#include <toshokan/hakase/export.h>
#include <toshokan/hakase/hakase.h>
#include <toshokan/hakase/loader16.h>
#include <toshokan/memory.h>
#include <toshokan/offload.h>
#include <toshokan/symbol.h>
#include <toshokan/version.h>
#include <unistd.h>
#include <iostream>

extern int SHARED_SYMBOL(__toshokan_sync_hakase_and_friend);

static int check_bootparam() {
  FILE *cmdline_fp = fopen("/proc/cmdline", "r");
  if (!cmdline_fp) {
    perror("failed to open `cmdline`");
    return -1;
  }

  char buf[256];
  buf[fread(buf, 1, 255, cmdline_fp)] = '\0';
  if (!strstr(buf, "memmap=8K$0x70000 memmap=1G$4G")) {
    std::cerr << "error: physical memory is not isolated for toshokan."
              << std::endl;
    return -1;
  }

  fclose(cmdline_fp);

  return 0;
}

static int check_version() {
  FILE *version_fp = fopen("/sys/module/friend_loader/info/version", "r");
  if (!version_fp) {
    perror("failed to open `version`");
    return -1;
  }

  char buf[256];
  buf[fread(buf, 1, 255, version_fp)] = '\0';
  if (!strstr(buf, kmod_version_string)) {
    std::cerr << "error: version mismatched" << std::endl;
    return -1;
  }
  fclose(version_fp);

  return 0;
}

/*extern char friend_mem_start[];
extern char friend_mem_end[];
static uint64_t *const mem = reinterpret_cast<uint64_t *>(friend_mem_start);*/
static uint64_t *const mem =
    reinterpret_cast<uint64_t *>(DEPLOY_PHYS_ADDR_START);

static int mmap_friend_mem() {
  int mem_fd = open("/dev/friend_mem", O_RDWR);
  if (mem_fd < 0) {
    perror("Open call failed");
    return -1;
  }

  void *mmapped_addr =
      mmap(mem, DEPLOY_PHYS_MEM_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC,
           MAP_SHARED | MAP_FIXED, mem_fd, 0);
  if (mmapped_addr == MAP_FAILED) {
    perror("mmap operation failed...");
    return -1;
  }
  assert(reinterpret_cast<void *>(mem) == mmapped_addr);

  close(mem_fd);

  // zero clear (only 4MB, because it is too slow to clear whole memory)
  memset(mem, 0, 1024 * 4096);
  return 0;
}

extern Offloader SHARED_SYMBOL(__toshokan_offloader);
static void export_init() {
  extern ExportSymbolContainer __export_symbol_start[], __export_symbol_end[];
  for (ExportSymbolContainer *c = __export_symbol_start;
       c != __export_symbol_end; c++) {
    *(c->shared_var_ptr) = c->hakase_var_ptr;
  }
}

// @return: if success, return 0. if not, return minus value.
int setup() {
  setbuf(stdout, NULL);

  extern uint8_t __start_friend_bin, __stop_friend_bin;
  size_t friend_bin_size =
      static_cast<size_t>(&__stop_friend_bin - &__start_friend_bin);

  Loader16 loader16;
  ElfLoader elfloader(&__start_friend_bin, friend_bin_size);

  if (check_version() < 0) {
    return -1;
  }

  if (check_bootparam() < 0) {
    return -1;
  }

  //  assert(friend_mem_start == reinterpret_cast<char
  //  *>(DEPLOY_PHYS_ADDR_START)); assert(friend_mem_end ==
  //  reinterpret_cast<char *>(DEPLOY_PHYS_ADDR_END));

  if (mmap_friend_mem() < 0) {
    return -1;
  }

  if (!elfloader.Deploy()) {
    std::cerr << "error: failed to deploy elf binary" << std::endl;
    return -1;
  }

  Elf64_Off entry = elfloader.GetEntry();
  if (loader16.Init(entry) < 0) {
    std::cerr << "error: failed to init friend16 region" << std::endl;
    return -1;
  }

  export_init();
  return 0;
}

// max : maximum number of cores which will boot
//       if it is 0 or < 0, no limits.
int boot(int max) {
  for (int i = 1;; i++) {
    char buf[20];
    sprintf(buf, "/dev/friend_cpu%d", i);
    if (open(buf, O_RDONLY) < 0) {
      return i - 1;
    }
    if (i == max) {
      return i;
    }
  }
}

void offloader_tryreceive() {
  while (SHARED_SYMBOL(__toshokan_offloader).TryReceive()) {
    asm volatile("pause" ::: "memory");
  }
}

bool is_friend_stopped() {
  return (SHARED_SYMBOL(__toshokan_sync_hakase_and_friend) == 1);
}
