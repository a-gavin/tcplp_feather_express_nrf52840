/**
 * @brief TCPlp application
 * 
 * Copyright (c) Alex Gavin, Brandon Lavinsky.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <logging/log.h>
LOG_MODULE_REGISTER(tcp_app, LOG_LEVEL_DBG);

#include "common.h"

void cmd_init() {
	/* Setup socket for TCP conn_sdections */
	sd = socket(AF_INET6, SOCK_STREAM, 0);
	if (sd < 0) {
		LOG_ERR("Failed to create TCP socket (IPv6): %d", -errno);
		sd = 0;
		return;
	}
	LOG_DBG("Initialized socket");
}

#if IS_LISTENER == 1
void cmd_listen() {
	int ret;
	struct sockaddr_in6 host_addr;
	struct sockaddr_in6 conn_addr;
	socklen_t host_addr_len = sizeof(host_addr);
	socklen_t conn_addr_len = sizeof(conn_addr);
	char addr_str[32];

	if (sd < 0) {
		LOG_ERR("Must initialize session before running \"listen\"");
		return;
	}

	/* Bind, listen, and accept incoming TCP connections (blocking) */
	(void)memset(&host_addr, 0, host_addr_len);
	host_addr.sin6_family = AF_INET6;
	host_addr.sin6_port = htons(APP_PORT);
	net_ipaddr_copy(&host_addr.sin6_addr, &in6addr_any);

	ret = bind(sd, (struct sockaddr *) &host_addr, host_addr_len);
	if (ret < 0) {
		LOG_ERR("Failed to bind TCP socket (IPv6): %d", -errno);
		return;
	}

	ret = listen(sd, LISTEN_BACKLOG_NUM);
	if (ret < 0) {
		LOG_ERR("Failed to listen on TCP socket (IPv6): %d", -errno);
		ret = -errno;
	}

	LOG_DBG("Waiting for TCP connection on port %d (IPv6)...", APP_PORT);
	conn_sd = accept(sd, (struct sockaddr *) &conn_addr, &conn_addr_len);
	if (conn_sd < 0) {
		LOG_ERR("IPv6 accept error (%d)", -errno);
		return;
	}

	inet_ntop(conn_addr.sin6_family, &conn_addr.sin6_addr, addr_str, sizeof(addr_str));
	LOG_DBG("TCP (IPv6): Accepted connection from %s", log_strdup(addr_str));
}
#else
void cmd_connect() {
	struct sockaddr_in6 conn_addr;
	socklen_t conn_addr_len = sizeof(conn_addr);

	if (sd < 0) {
		LOG_ERR("Must initialize session before running \"listen\"");
		return;
	}

	/* Connect to listening device */
	(void)memset(&conn_addr, 0, conn_addr_len);
	conn_addr.sin6_family = AF_INET6;
	conn_addr.sin6_port = htons(APP_PORT);
	if (inet_pton(AF_INET6, "2001:db8::1", &(conn_addr.sin6_addr)) != 1) {
		LOG_ERR("Failed to translate address");
	}

	conn_sd = connect(sd, (struct sockaddr *) &conn_addr, conn_addr_len);
	if (conn_sd < 0) {
		LOG_ERR("Failed to connect: %d", -errno);
	}
	LOG_DBG("Successfully connected");
}
#endif

#if IS_LISTENER == 1
void cmd_benchmark_recv() {
	if (sd < 0) {
		LOG_ERR("Must initialize session before running \"bencmark_recv\"");
		return;
	} else if (conn_sd < 0) {
		LOG_ERR("Must listen for connections before running \"bencmark_recv\"");
		return;
	}

	ssize_t bytes_recvd = recv(conn_sd, recv_buf, sizeof(char)*MAXBUFSIZE, 0);
	LOG_DBG("Num bytes recv: %d", bytes_recvd);
	if (bytes_recvd < 0) {
		LOG_ERR("Receive failed: %d", -errno);
		return;
	} else if (bytes_recvd != MAXBUFSIZE) {
		LOG_ERR("Receive failed: received %d bytes, not %d bytes", bytes_recvd, MAXBUFSIZE);
		return;
	}
	recv_buf[MAXBUFSIZE] = '\0';
	LOG_DBG("Recv output: %s", log_strdup(recv_buf));
}
#else
void cmd_benchmark_send() {
	if (sd < 0) {
		LOG_ERR("Must initialize session before running \"bencmark_recv\"");
		return;
	} else if (conn_sd < 0) {
		LOG_ERR("Must listen for connections before running \"bencmark_recv\"");
		return;
	}

	ssize_t bytes_sent = send(conn_sd, send_buf, sizeof(char)*MAXBUFSIZE, 0);
	LOG_DBG("Num bytes sent: %d", bytes_sent);
	if (bytes_sent < 0) {
		LOG_ERR("Send failed: %d", -errno);
		return;
	} else if (bytes_sent != MAXBUFSIZE) {
		LOG_ERR("Send failed: sent %d bytes, not %d bytes", bytes_sent, MAXBUFSIZE);
		return;
	}
}
#endif

void cmd_quit() {
	/* Tear down connection (if setup) and socket */
	if (sd < 0) {
		LOG_ERR("Must initialize session before running \"quit\"");
		return;
	}

	if (conn_sd >= 0) {
		LOG_DBG("Closing connection.");
		close(conn_sd);
	}

	LOG_DBG("Closing socket.");
	close(sd);
}

void main() {
#if DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_shell_uart), zephyr_cdc_acm_uart)
	/* Setup UART-based shell */
	int ret;
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

	LOG_DBG("Waiting for host to be ready to communicate");

	/* Data Terminal Ready - check if host is ready to communicate */
	while (!dtr) {
		ret = uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
		if (ret) {
			LOG_ERR("Failed to get Data Terminal Ready line state: %d", ret);
			continue;
		}
		k_msleep(100);
	}

	/* Data Carrier Detect Modem - mark connection as established */
	(void)uart_line_ctrl_set(dev, UART_LINE_CTRL_DCD, 1);
	/* Data Set Ready - the NCP SoC is ready to communicate */
	(void)uart_line_ctrl_set(dev, UART_LINE_CTRL_DSR, 1);
#else
#error Application requires UART-based shell
#endif

#if IS_LISTENER == 1
	LOG_DBG("Listener device running");
	memset(&recv_buf, 'o', MAXBUFSIZE);
#else
	LOG_DBG("Connector device running");
	memset(&send_buf, 'x', MAXBUFSIZE);
#endif
}