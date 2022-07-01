#ifndef __HTTPSERVER_H__
#define __HTTPSERVER_H__

#include <pthread.h>

//	creates a new job request
void *handle_connection(void *socket);

#endif
