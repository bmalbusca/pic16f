#ifndef PROC_H
#define PROC_H
#include <stdio.h>
#include "threads.h"

// macros
#define DELTA(X,Y) X > Y ? X - Y : Y - X

void checkThresholds(char* localmem, int iread, int iwrite, int alat, int alal, cyg_sem_t* rs_localmem, cyg_mutex_t* rs_stdin);
void calcStatistics(char* localmem, int iwrite, int mem_filled, int* max, int* min, int* mean, unsigned int range[6]);
void find(char* localmem, int iwrite, int mem_filled, int *start_i, int* end_i, unsigned int range[6]);

#endif
