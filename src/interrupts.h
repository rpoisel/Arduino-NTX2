#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <Arduino.h>

class InterruptGuard final
{
  public:
  InterruptGuard() { noInterrupts(); }
  ~InterruptGuard() { interrupts(); }

  private:
  InterruptGuard(InterruptGuard const&) = delete;
  InterruptGuard& operator=(InterruptGuard const&) = delete;
};


#endif /* INTERRUPTS_H */