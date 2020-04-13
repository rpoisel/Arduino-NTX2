#include <scheduling.h>

#include <Arduino.h>

void Task::execute()
{
  if (!active)
  {
    return;
  }
  auto curTime = millis();
  if (curTime - lastRun < period)
  {
    return;
  }

  lastRun = curTime;
  active = false;
  run();
}

void Task::schedule(Period when)
{
  period = when;
  active = true;
}
