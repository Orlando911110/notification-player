#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <stdint.h>

typedef struct http_server_s http_server_t;

// 创建HTTP服务器
http_server_t* http_server_create(int port);

// 销毁HTTP服务器
void http_server_destroy(http_server_t *server);

// 轮询事件
void http_server_poll(http_server_t *server, int timeout_ms);

// 获取服务器端口
int http_server_get_port(http_server_t *server);

#endif