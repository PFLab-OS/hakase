#include "common/channel.h"

class InterruptController {
public:
  InterruptController(F2H &f2h) : _f2h(f2h) {
  }
  InterruptController() = delete;
private:
  F2H &_f2h;
};

