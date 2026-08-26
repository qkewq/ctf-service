#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include "configs.h"

#define MAX_LINE_SIZE 255
#define MAX_WORD_SIZE 64

typedef struct LINE{
	FILE *file;
	int line_num;
	int index;
	char line[MAX_LINE_SIZE];
} LINE;

int config_errors;

LINE *lopen(char *filepath, char *mode){
	LINE *line = calloc(1, sizeof(LINE));
	if(!line){
		return NULL;
	}

	line->file = fopen(filepath, mode);
	if(!line->file){
		free(line);
		return NULL;
	}

	memset(line->line, 0, MAX_LINE_SIZE);
	line->line_num = 0;
	line->index = 0;
}

int lclose(LINE *line){
	fclose(line->file);
	free(line);

	return 0;
}

int lfeof(LINE *line){
	return feof(line->file);
}

void free_addr(address_t *addr){
	address_t *next = NULL;
	while(addr){
		next = addr->next;
		free(addr);
		addr = next;
	}
}

void lerror_exp(LINE *line, char expected, char got){
	printf("Error in line %d:%d - Expected '%c' not '%c'\n",
           line->line_num, line->index + 1, expected, got);
}

void lerror_exp_s(LINE *line, char *expected, char *got){
	printf("Error in line %d - Expected \"%s\" not \"%s\"\n",
            line->line_num, expected, got);
}

void lerror_unk(LINE *line, char *unk){
	printf("Error in line %d - Unknown \"%s\"\n", line->line_num, unk);
}

char lgetc(LINE *line){
	return line->line[line->index++];
}

char lpeekc(LINE *line){
	return line->line[line->index];
}

void free_configs(config_t *configs){
	config_t *next = NULL;
	while(configs){
		next = configs->next;
		free(configs->name);
		free(configs->filepath);
		free_addr(configs->addr);
		free(configs);
		configs = next;
	}
}

void skip_whitespace(LINE *line){
	while(lpeekc(line) == ' ' || lpeekc(line) == '\t'){
		lgetc(line);
	}
}

int is_whitespace(LINE *line){
	if(lpeekc(line) == ' ' || lpeekc(line) == '\t'){
		return 1;
	}

	return 0;
}

int is_newline(LINE *line){
	if(lpeekc(line) == '\n' || lpeekc(line) == '\r'){
		return 1;
	}

	return 0;
}

int is_comment(LINE *line){
	if(lpeekc(line) == '#' || lpeekc(line) == ';'){
		return 1;
	}

	return 0;
}

int is_alpha(LINE *line){
	if(lpeekc(line) >= 'a' || lpeekc(line) <= 'z' ||
       lpeekc(line) >= 'A' || lpeekc(line) <= 'Z'){
		return 1;
	   }

	   return 0;
}

int lgetw(LINE *line, char *buffer, size_t buffer_size){
	int i = 0;
	for(i; i < buffer_size; i++){
		if(!is_alpha(line)){
			buffer[i] = '\0';
			return i;
		}
		buffer[i] = lgetc(line);
	}
	buffer[i - 1] = '\0';
	return i;
}

int lgetl(LINE *line){
	if(!fgets(line->line, MAX_LINE_SIZE, line->file)){
		line->line_num = 0;
		return -1;
	}

	line->line_num++;
	line->index = 0;

	return 0;
}

int lget_addr(LINE *line, char *buffer, size_t buffer_size){
	int i = 0;
	for(i; i < buffer_size; i++){
		if(!is_alpha(line) && lpeekc(line) < '0' && lpeekc(line) > '9' && lpeekc(line) != '.' && lpeekc(line) != ':'){
			buffer[i] = '\0';
			return i;
		}
		buffer[i] = lgetc(line);
	}
	buffer[i - 1] = '\0';
	return i;
}

int is_section(LINE *line){
	if(lpeekc(line) != '['){
		return 0;
	}

	lgetc(line);
	if(lpeekc(line) != '['){
		lerror_exp(line, '[', lpeekc(line));
		return 1;
	}

	skip_whitespace(line);

	char buffer[MAX_WORD_SIZE];
	lgetw(line, buffer, MAX_WORD_SIZE);
	if(strcmp(buffer, "Service") != 0){
		lerror_exp_s(line, "Service", buffer);
		return 1;
	}

	skip_whitespace(line);

	if(lpeekc(line) != ']'){
		lerror_exp(line, ']', lpeekc(line));
		return 1;
	}
	lgetc(line);
	if(lpeekc(line) != ']'){
		lerror_exp(line, ']', lpeekc(line));
	}

	return 1;
}

int link_addr(LINE *line, config_t *configs, char *val, int family){
	address_t *addr = calloc(1, sizeof(address_t));
	if(!addr){
		return -1;
	}
	int pton_ret = inet_pton(family, val, &addr->addr);
	if(pton_ret == -1){
		printf("Error in line %d - Invalid family", line->line_num);
		free(addr);
		return 0;
	}
	else if(pton_ret == 0){
		printf("Error in line %d - Invalid address", line->line_num);
		free(addr);
		return 0;
	}

	addr->family = family;
	if(family == AF_INET){
		addr->addr_len = sizeof(struct sockaddr_in);
	}
	else{
		addr->addr_len = sizeof(struct sockaddr_in6);
	}

	addr->next = configs->addr;
	configs->addr = addr;

	return 1;
}

int parse_kv(LINE *line, config_t *configs){
	if(!configs){
		printf("Expected \"[[Service]]\" section before key value pair\n");
		return -1;

	}
	char key[MAX_WORD_SIZE];
	char val[MAX_WORD_SIZE];
	char delim;
	int val_size;

	lgetw(line, key, MAX_WORD_SIZE);
	skip_whitespace(line);
	delim = lgetc(line);
	if(delim != '='){
		lerror_exp(line, '=', delim);
		config_errors++;
	}
	skip_whitespace(line);
	if(strcmp(key, "addr") == 0 || strcmp(key, "addr6") == 0){
		val_size = lget_addr(line, val, MAX_WORD_SIZE);
	}
	else{
		val_size = lgetw(line, val, MAX_WORD_SIZE);
	}
	skip_whitespace(line);
	if(!is_newline(line)){
		lerror_exp(line, '\n', lpeekc(line));
		config_errors++;
	}

	if(strcmp(key, "name") == 0){
		char *name = calloc(val_size, sizeof(char));
		if(!name){
			return -1;
		}
		memcpy(name, val, val_size);
		configs->name = name;
		configs->present_configs |= PST_NAME;
	}
	else if(strcmp(key, "filepath") == 0){
		char *filepath = calloc(val_size, sizeof(char));
		if(!filepath){
			return -1;
		}
		memcpy(filepath, val, val_size);
		configs->filepath = filepath;
		configs->present_configs |= PST_PATH;
	}
	else if(strcmp(key, "type") == 0){
		if(strcmp(val, "exectuable") == 0){
			configs->type = EXECUTABLE;
			configs->present_configs |= PST_TYPE;
		}
		else{
			lerror_unk(line, val);
			config_errors++;
		}
	}
	else if(strcmp(key, "port") == 0){
		configs->port = atoi(val);
		configs->present_configs |= PST_PORT;
	}
	else if(strcmp(key, "addr") == 0){
		int link_ret = link_addr(line, configs, val, AF_INET);
		if(link_ret == -1){
			return -1;
		}
		else if(link_ret == 1){
			configs->present_configs |= PST_ADDR;
		}
	}
	else if(strcmp(key, "addr6") == 0){
		int link_ret = link_addr(line, configs, val, AF_INET6);
		if(link_ret == -1){
			return -1;
		}
		else if(link_ret == 1){
			configs->present_configs |= PST_ADDR;
		}
	}
	else{
		lerror_unk(line, key);
		config_errors++;
	}

	return 0;
}

/* Returns -1 on error or the number of configs in the list */
/* Prints errors */
int parse_configs(char *config_file, config_t **return_pointer){
	if(!config_file || !return_pointer){
		return -1;
	}

	config_errors = 0;

	LINE *line = lopen(config_file, "r");
	config_t *configs = NULL;
	int num_configs = 0;

	while(lgetl(line) != -1){
		skip_whitespace(line);
		if(is_comment(line) || is_newline(line)){
			continue;
		}

		if(is_section(line)){
			config_t *new_config = calloc(1, sizeof(config_t));
			new_config->next = configs;
			configs = new_config;
		}

		if(parse_kv(line, configs) == -1){
			lclose(line);
			*return_pointer = NULL;
			free_configs(configs);
			config_errors++;
			return -1;
		}
	}

	if(!lfeof(line)){
		lclose(line);
		*return_pointer = NULL;
		free_configs(configs);
		config_errors++;
		return -1;
	}

	lclose(line);
	*return_pointer = configs;
	return num_configs;
}

int config_nerrors(){
	return config_errors;
}
