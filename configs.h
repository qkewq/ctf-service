#ifndef CONFIGS_H
#define CONFIGS_H

#define MAX_ADDR_SIZE 16
#define DEFAULT_CONFIG_FILE "/etc/ctf-service.conf"

typedef enum service_types_t{
	EMPTY = 0,
	EXECUTABLE,
} service_types_t;

typedef enum present_configs_t{
	PST_NAME = 0x01,
	PST_PATH = 0x02,
	PST_PORT = 0x04,
	PST_TYPE = 0x08,
	PST_ADDR = 0x10,
} present_configs_t;

typedef struct address_t{
	struct address_t *next;
	sa_family_t family;
	struct sockaddr_storage addr;
	size_t addr_len;
} address_t;

typedef struct config_t{
	struct config_t *next;
	char *name;
	char *filepath;
	service_types_t type;
	struct address_t *addr;
	uint16_t port;
	uint8_t present_configs;
} config_t;

int parse_configs(char *config_file, config_t **return_pointer);
void free_configs(config_t *configs);
int config_nerrors();

#endif
