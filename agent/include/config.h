#ifndef CONFIG_H
#define CONFIG_H

#define CONFIG_PATH "/etc/ctfwatch/config.json"

typedef struct agent_config {
    char *server_url;
    char *token;
} agent_config_t;

agent_config_t *config_load(void);
void config_destroy(agent_config_t *config);

#endif