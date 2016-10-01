#ifndef CONFIGURATOR_H
#define CONFIGURATOR_H
/*
** Returns the connection specifications for the desired
** mode and communication implementation.
** comm_mode is one of the three modes detailed in comms.h
** mode is either 1 (for the server) or 0 (for a client).
*/
char * configuration(char * config_file,int comm_mode,int mode);
#endif
