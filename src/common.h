/**
 * @brief TCPlp application
 * 
 * Copyright (c) Alex Gavin, Brandon Lavinsky.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <zephyr.h>
#include <shell/shell.h>

#include <net/socket.h>
#include <net/net_ip.h>

#include <drivers/uart.h>
#include <usb/usb_device.h>

#if IS_LISTENER != 1
#include <kernel.h>
#include <sys/time_units.h>
#endif

#define APP_PORT			2424
#define LISTEN_BACKLOG_NUM 	10

#if IS_LISTENER == 1
// OpenThread TCPlp receive buffer size (OPENTHREAD_CONFIG_CLI_TCP_RECEIVE_BUFFER_SIZE)
#define MAXBUFSIZE			2599
#else
// OpenThread TCPlp send buffer size (OPENTHREAD_CONFIG_CLI_MAX_LINE_LENGTH)
#define MAXBUFSIZE			384
#endif

/* Redefine shell_print to no-op when debugging
 * to enable non-CLI usage of benchmark funcs
 */
#if DEBUG == 1
#ifdef shell_print
#undef shell_print
#define shell_print(...) ((void) 0)
#endif
#endif

/* Globals */
int sd = -1, conn_sd = -1;
int buf_size = MAXBUFSIZE;

#if IS_LISTENER == 1
// OpenThread TCPlp receive buffer size (OPENTHREAD_CONFIG_CLI_TCP_RECEIVE_BUFFER_SIZE)
char recv_buf[MAXBUFSIZE];
#else
// OpenThread TCPlp send buffer size (OPENTHREAD_CONFIG_CLI_MAX_LINE_LENGTH)
char send_buf[MAXBUFSIZE];
#endif

/* Prototypes */
void cmd_init();
void cmd_listen();
void cmd_benchmark_recv();
void cmd_connect();
void cmd_benchmark_send();
void cmd_quit();

/* Application shell interface */
SHELL_STATIC_SUBCMD_SET_CREATE(app_cmds,
	SHELL_CMD(init, NULL, "Initialize session\n", cmd_init),
#if IS_LISTENER == 1
	SHELL_CMD(listen, NULL, "Listen for connections (blocking)\n", cmd_listen),
	SHELL_CMD(benchmark_recv, NULL, "Receive data for benchmark\n", cmd_benchmark_recv),
#else
	SHELL_CMD(connect, NULL, "Connect to listening device\n", cmd_connect),
	SHELL_CMD_ARG(benchmark_send, NULL, "Send data for benchmark\n", cmd_benchmark_send, 0, 1),
#endif
	SHELL_CMD(quit, NULL, "Quit session\n", cmd_quit),
	SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(tcp_app, &app_cmds,
		   "Benchmark application commands", NULL);