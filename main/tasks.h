#ifndef TASKS_H
#define TASKS_H


void gpio_isr_callback(void* arg);

void tasks_init(void);
void tasks_start(void);

#endif

