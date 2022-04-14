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
	if (sd >= 0) {
		LOG_ERR("Socket already initialized");
		return;
	} else if (conn_sd >= 0) {
		LOG_ERR("Connection already initialized");
		return;
	}

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
	ssize_t bytes_in_benchmark, bytes_recvd, total_bytes_recvd;

	if (sd < 0) {
		LOG_ERR("Must initialize session before running \"bencmark_recv\"");
		return;
	} else if (conn_sd < 0) {
		LOG_ERR("Must listen for connections before running \"bencmark_recv\"");
		return;
	}

	/* 1. Determine total number of bytes to recv */
	LOG_DBG("Waiting to receive number of bytes in benchmark");
	bytes_recvd = recv(conn_sd, &bytes_in_benchmark, sizeof(ssize_t), 0);
	if (bytes_recvd < 0) {
		LOG_ERR("Receive failed: %d", -errno);
		return;
	}

	/* 2. Notify connector to start benchmark by sending recvd value back */
	LOG_DBG("Benchmark size %d bytes. Sending back to synchronize", bytes_in_benchmark);
	if (send(conn_sd, &bytes_in_benchmark, sizeof(ssize_t), 0) < 0) {
		LOG_ERR("Send failed: %d", -errno);
		return;
	}

	/* 3. Receive benchmark data */
	LOG_DBG("Beginning benchmark");
	total_bytes_recvd = 0;
	while (total_bytes_recvd < bytes_in_benchmark
		&& (bytes_recvd = recv(conn_sd, recv_buf, sizeof(char)*bytes_in_benchmark, 0)) > 0) {
			total_bytes_recvd += bytes_recvd;
			LOG_DBG("Received %d bytes", bytes_recvd);
	}
	if (bytes_recvd < 0) {
		LOG_ERR("Receive failed: %d", -errno);
		cmd_quit();
		return;
	}

	/* 4. Send amount of data received to end benchmark */
	LOG_DBG("Received %d bytes total", total_bytes_recvd);
	if (send(conn_sd, &total_bytes_recvd, sizeof(ssize_t), 0) < 0) {
		LOG_ERR("Send failed: %d", -errno);
		return;
	}
	LOG_DBG("Ending benchmark");
}
#else
void cmd_benchmark_send() {
	ssize_t bytes_in_benchmark, bytes_recvd, bytes_sent, ret, total_bytes_sent;

	if (sd < 0) {
		LOG_ERR("Must initialize session before running \"bencmark_recv\"");
		return;
	} else if (conn_sd < 0) {
		LOG_ERR("Must listen for connections before running \"bencmark_recv\"");
		return;
	}

	/* 0. Initialize benchmark parameters */
	bytes_in_benchmark = 2*MAXBUFSIZE;

	/* 1. Send listener number of bytes to receive */
	LOG_DBG("Sending number of bytes in benchmark (%d bytes)", bytes_in_benchmark);
	ret = send(conn_sd, &bytes_in_benchmark, sizeof(ssize_t), 0);
	if (ret < 0) {
		LOG_ERR("Send failed: %d", -errno);
		return;
	}

	/* 2. Receive value sent to connector, syncing devices before running test */
	LOG_DBG("Receiving back to synchronize");
	if (recv(conn_sd, &bytes_recvd, sizeof(ssize_t), 0) < 0) {
		LOG_ERR("Receive failed: %d", -errno);
		return;
	}

	/* 3. Begin benchmark */
	LOG_DBG("Beginning benchmark");
	// TODO: Begin timer
	total_bytes_sent = 0;
	do {
		bytes_sent = send(conn_sd, send_buf, sizeof(char)*MAXBUFSIZE, 0);
		total_bytes_sent += bytes_sent;
		LOG_DBG("Sent %d bytes", bytes_sent);
	} while (bytes_sent > 0 && total_bytes_sent < bytes_in_benchmark);

	if (bytes_sent < 0) {
		LOG_ERR("Send failed: %d", -errno);
		return;
	}

	/* 4. End benchmark, verifying number of bytes sent
	 *    equal to number of bytes received by listener
	 */
	LOG_DBG("Receiving number of bytes sent");
	ret = recv(conn_sd, &bytes_recvd, sizeof(ssize_t), 0);
	// TODO: end timer

	if (ret < 0) {
		LOG_ERR("Receive failed: %d", -errno);
		return;
	} else if (bytes_in_benchmark != bytes_recvd) {
		LOG_ERR("Bytes received (%d) not equal to total bytes sent (%d)", bytes_recvd, bytes_in_benchmark);
		return;
	}
	LOG_DBG("Bytes sent (%d) equals total bytes sent (%d)", bytes_recvd, bytes_in_benchmark);

	/* 5. Calculate and print goodput */
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