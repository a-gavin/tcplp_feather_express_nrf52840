/**
 * @brief TCPlp application
 * 
 * Copyright (c) Alex Gavin, Brandon Lavinsky.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#define APP_PORT	2424
#define LISTEN_BACKLOG_NUM 1

#include <logging/log.h>
LOG_MODULE_REGISTER(tcp_app, LOG_LEVEL_DBG);

#include <zephyr.h>
#include <shell/shell.h>

#include <net/socket.h>
#include <net/net_ip.h>

#include <drivers/uart.h>
#include <usb/usb_device.h>

// TODO: Application shell interface
//SHELL_STATIC_SUBCMD_SET_CREATE(app_cmds,
//	SHELL_CMD(connect, NULL,
//		  "Connect to other benchmark device\n",
//		  test),
//	SHELL_SUBCMD_SET_END
//);

//SHELL_STATIC_SUBCMD_SET_CREATE(app_cmds,
//	SHELL_CMD(start, NULL,
//		  "Connect to other benchmark device\n",
//		  start_benchmark),
//	SHELL_SUBCMD_SET_END
//);

//SHELL_CMD_REGISTER(tcp_app, &app_cmds,
//		   "Benchmark application commands", NULL);

void main() {
	int ret;

#if DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_shell_uart), zephyr_cdc_acm_uart)
	/* Setup UART-based shell */
	const struct device *dev;
	uint32_t dtr = 0U;

	ret = usb_enable(NULL);
	if (ret != 0) {
		LOG_ERR("Failed to enable USB");
		return;
	}

	dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_shell_uart));
	if (dev == NULL) {
		LOG_ERR("Failed to find specific UART device");
		return;
	}

	LOG_INF("Waiting for host to be ready to communicate");

	/* Data Terminal Ready - check if host is ready to communicate */
	while (!dtr) {
		ret = uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
		if (ret) {
			LOG_ERR("Failed to get Data Terminal Ready line state: %d",
				ret);
			continue;
		}
		k_msleep(100);
	}

	/* Data Carrier Detect Modem - mark connection as established */
	(void)uart_line_ctrl_set(dev, UART_LINE_CTRL_DCD, 1);
	/* Data Set Ready - the NCP SoC is ready to communicate */
	(void)uart_line_ctrl_set(dev, UART_LINE_CTRL_DSR, 1);
#endif

#ifdef IS_LISTENER
	/* Setup and listen on port APP_PORT for incoming TCP connections */
	int sd, conn;
	struct sockaddr_in6 host_addr;
	socklen_t host_addr_len = sizeof(host_addr);

	struct sockaddr_in conn_addr;
	socklen_t conn_addr_len = sizeof(conn_addr);

	(void)memset(&host_addr, 0, host_addr_len);
	host_addr.sin6_family = AF_INET6;
	host_addr.sin6_port = htons(APP_PORT);


	sd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	if (sd < 0) {
		LOG_ERR("Failed to create TCP socket (IPv6): %d", errno);
		return;
	}

	ret = bind(sd, (struct sockaddr *) &host_addr, host_addr_len);
	if (ret < 0) {
		LOG_ERR("Failed to bind TCP socket (IPv6): %d", errno);
		return;
	}

	ret = listen(sd, LISTEN_BACKLOG_NUM);
	if (ret < 0) {
		LOG_ERR("Failed to listen on TCP socket (IPv6): %d", errno);
		ret = -errno;
	}
	LOG_INF("Waiting for TCP connection on port %d (IPv6)...", APP_PORT);


	conn = accept(sd, (struct sockaddr *) &conn_addr, &conn_addr_len);
	if (conn < 0) {
		LOG_ERR("IPv6 accept error (%d)", -errno);
		return;
	}
	LOG_INF("TCP (IPv6)): Accepted connection");

	LOG_INF("Sleeping for 5 sec");
	k_msleep(5000);

	LOG_INF("Shutting down connection");
	close(sd);
#endif // IS_LISTENER
}