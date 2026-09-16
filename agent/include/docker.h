#ifndef DOCKER_H
#define DOCKER_H

#define DOCKER_SOCK "/var/run/docker.sock"
#define CTFWATCH_NETWORK "ctfwatch-net"

char *get_docker_info(void);
char *get_docker_container_info(const char *container_id);
int docker_container_is_tty(const char *container_id);

#endif