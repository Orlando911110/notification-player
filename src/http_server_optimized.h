#ifndef HTTP_SERVER_OPTIMIZED_H
#define HTTP_SERVER_OPTIMIZED_H

#include <stdint.h>

typedef struct http_server_opt_s http_server_opt_t;

// 创建HTTP服务器（带线程池）
http_server_opt_t* http_server_opt_create(int port, int max_workers, int max_connections);

// 销毁HTTP服务器
void http_server_opt_destroy(http_server_opt_t *server);

// 运行服务器（阻塞）
void http_server_opt_run(http_server_opt_t *server);

// 停止服务器
void http_server_opt_stop(http_server_opt_t *server);

#endif