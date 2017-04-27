/* to count running chat processes */

#ifndef SHM_BUFFER_H
#define SHM_BUFFER_H

#include <stdio.h>

#define BUF_STRLEN 80

int open_shm_buffer(void);
void close_shm_buffer(void);
int add_str_to_shm(char *str);
char * get_str_from_shm(void);

#endif
