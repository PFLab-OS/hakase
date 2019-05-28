#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <toshokan/hakase/elf_loader.h>
#include <toshokan/hakase/loader16.h>
#include <toshokan/memory.h>
#include <toshokan/result.h>
#include <unistd.h>
#include <iostream>
#include "shared.h"

extern char friend_mem_start[];
extern char friend_mem_end[];
uint64_t *const mem = reinterpret_cast<uint64_t *>(friend_mem_start);

int check_bootparam() {
  FILE *cmdline_fp = fopen("/proc/cmdline", "r");
  if (!cmdline_fp) {
    perror("failed to open `cmdline`");
    return -1;
  }

  char buf[256];
  buf[fread(buf, 1, 255, cmdline_fp)] = '\0';
  if (!strstr(buf, "memmap=0x70000$4K memmap=0x40000000$0x40000000")) {
    std::cerr << "error: physical memory is not isolated for toshokan."
              << std::endl;
    return -1;
  }

  fclose(cmdline_fp);

  return 0;
}

int mmap_friend_mem() {
  int mem_fd = open("/sys/module/friend_loader/call/mem", O_RDWR);
  if (mem_fd < 0) {
    perror("Open call failed");
    return -1;
  }

  void *mmapped_addr = mmap(mem, DEPLOY_PHYS_MEM_SIZE, PROT_READ | PROT_WRITE,
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

void pagetable_init() {
  static const size_t k256TB = 256UL * 1024 * 1024 * 1024 * 1024;
  static const size_t k512GB = 512UL * 1024 * 1024 * 1024;
  static const size_t k1GB = 1024UL * 1024 * 1024;
  static const size_t k2MB = 2UL * 1024 * 1024;

  pml4t.entry[(DEPLOY_PHYS_ADDR_START % k256TB) / k512GB] =
      reinterpret_cast<size_t>(&pdpt) | (1 << 0) | (1 << 1) | (1 << 2);
  pdpt.entry[(DEPLOY_PHYS_ADDR_START % k512GB) / k1GB] =
      reinterpret_cast<size_t>(&pd) | (1 << 0) | (1 << 1) | (1 << 2);

  static_assert((DEPLOY_PHYS_ADDR_START % k1GB) == 0, "");
  static_assert(DEPLOY_PHYS_MEM_SIZE <= k1GB, "");
  for (size_t addr = DEPLOY_PHYS_ADDR_START; addr < DEPLOY_PHYS_ADDR_END;
       addr += k2MB) {
    pd.entry[(addr % k1GB) / k2MB] =
        addr | (1 << 0) | (1 << 1) | (1 << 2) | (1 << 7);
  }
}

int test_main() {
  extern uint8_t __start_friend_bin, __stop_friend_bin;
  size_t friend_bin_size =
      static_cast<size_t>(&__stop_friend_bin - &__start_friend_bin);

  Loader16 loader16;
  ElfLoader elfloader(&__start_friend_bin, friend_bin_size);

  if (check_bootparam() < 0) {
    return -1;
  }

  assert(friend_mem_start == reinterpret_cast<char *>(DEPLOY_PHYS_ADDR_START));
  assert(friend_mem_end == reinterpret_cast<char *>(DEPLOY_PHYS_ADDR_END));

  if (mmap_friend_mem() < 0) {
    return -1;
  }

  if (elfloader.Deploy().IsError()) {
    std::cerr << "error: failed to deploy elf binary" << std::endl;
    return -1;
  }

  Elf64_Off entry = elfloader.GetEntry();
  assert(entry < 0xFFFFFFFF);
  if (loader16.Init(entry) < 0) {
    std::cerr << "error: failed to init friend16 region" << std::endl;
    return -1;
  }

  pagetable_init();
  sync_flag = 0;

  int cpunum = 0;

  for (int i = 1;; i++) {
    char buf[20];
    sprintf(buf, "/dev/friend_cpu%d", i);
    if (open(buf, O_RDONLY) < 0) {
      cpunum = i - 1;
      break;
    }
  }

  sleep(1);

  return (sync_flag == cpunum);
}

int main(int argc, const char **argv) {
  if (test_main() > 0) {
    printf("\e[32m%s: PASSED\e[m\n", argv[0]);
    return 0;
  } else {
    printf("\e[31m%s: FAILED\e[m\n", argv[0]);
    return 255;
  }
}