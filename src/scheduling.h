#ifndef SCHEDULING_H
#define SCHEDULING_H

using Period = unsigned long; // millis(), micros(), ...

class Task
{
  public:
  Task() : active(false), lastRun(0), period(0) {}
  virtual ~Task() {}
  virtual void run() = 0;
  void execute();
  void schedule(Period when);

  private:
  Task(Task const&) = delete;
  Task& operator=(Task const&) = delete;

  bool active;
  Period lastRun;
  Period period;
};

#endif /* SCHEDULING_H */