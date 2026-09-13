#ifndef DOCKER_H
#define DOCKER_H

#define DOCKER_SOCK "/var/run/docker.sock"
#define CTFWATCH_NETWORK "ctfwatch-net"

char *get_docker_info(void);

#endif