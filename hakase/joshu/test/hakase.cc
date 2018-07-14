#include "interrupt/hakase.h"
#include "elf_loader/hakase.h"
#include "tests/test.h"

int test_main(F2H &f2h, H2F &h2f, I2H &i2h, int argc, const char **argv) {
  auto ic = InterruptController(i2h);

  if (argc < 3) {
    return 1;
  }

  // Load joshu.bin
  auto file = ElfLoader::ElfFile::Load(argv[1]);
  if (!file) {
    return 1;
  }

  {
    auto r = file->Init(h2f);
    if (r.IsError()) {
      r.IgnoreError();
      return 1;
    }
    r.Unwrap();
  }
  
  ElfLoader sl(h2f, std::move(file));

  {
    auto r = sl.Deploy();
    if (r.IsError()) {
      r.IgnoreError();
      return 1;
    }
    r.Unwrap();
  }

  {
    auto r = sl.Execute(1);
    if (r.IsError()) {
      r.IgnoreError();
      return 1;
    }
    r.Unwrap();
  }


  int16_t type;
  f2h.WaitNewSignal(type);
  if (type != 6) {
    return 1;
  }

  f2h.Return(0);

  // Load friend.bin
  auto file2 = ElfLoader::ElfFile::Load(argv[2]);
  if (!file2) {
    return 1;
  }

  {
    auto r = file2->Init(h2f);
    if (r.IsError()) {
      r.IgnoreError();
      return 1;
    }
    r.Unwrap();
  }
  
  ElfLoader sl2(h2f, std::move(file2));

  {
    auto r = sl2.Deploy();
    if (r.IsError()) {
      r.IgnoreError();
      return 1;
    }
    r.Unwrap();
  }

  {
    auto r = sl2.Execute(1);
    if (r.IsError()) {
      r.IgnoreError();
      return 1;
    }
    r.Unwrap();
  }


  f2h.WaitNewSignal(type);
  if (type != 7) {
    return 1;
  }

  return 0;
}
