#ifndef SCHEDULER_H
#define SCHEDULER_H

void register_task(const char *name, void (*func)(void));

#endif /* SCHEDULER_H */