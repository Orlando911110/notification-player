#ifndef HTTPS_SERVER_OPTIMIZED_H
#define HTTPS_SERVER_OPTIMIZED_H

#include <stdint.h>
#include <openssl/ssl.h>

typedef struct https_server_opt_s https_server_opt_t;

// 创建HTTPS服务器（带SSL支持）
https_server_opt_t* https_server_opt_create(
    int port, 
    int max_workers, 
    int max_connections,
    const char *cert_file,      // 证书文件路径
    const char *key_file        // 私钥文件路径
);

void https_server_opt_destroy(https_server_opt_t *server);
void https_server_opt_run(https_server_opt_t *server);
void https_server_opt_stop(https_server_opt_t *server);

#endif