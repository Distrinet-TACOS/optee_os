#ifndef SCHEDULER_H
#define SCHEDULER_H

void register_task(const char *name, void (*func)(void));
void unregister_task(const char *name);

#endif /* SCHEDULER_H */