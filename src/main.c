/**
 * @brief TCPlp application
 * 
 * Copyright (c) Alex Gavin, Brandon Lavinsky.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <logging/log.h>
LOG_MODULE_REGISTER(tcp_app, LOG_LEVEL_INF);

#include "common.h"

/* Common code */
void cmd_init(const struct shell *sh, int argc, char **argv) {
	int usr_spec_bufsize;

	/* Setup socket for TCP conn_sdections */
	if (sd >= 0) {
		LOG_ERR("Socket already initialized");
		return;
	} else if (conn_sd >= 0) {
		LOG_ERR("Connection already initialized");
		return;
	}

	/* Set buffer size if passed to init */
	if (argc == 2) {
		usr_spec_bufsize = atoi(argv[1]);

		if (usr_spec_bufsize > MAXBUFSIZE) {
			shell_print(sh, "Requested buffer size %d bytes larger than MAXBUFSIZE (%d) bytes", usr_spec_bufsize, MAXBUFSIZE);
			usr_spec_bufsize = MAXBUFSIZE;
		} else if (usr_spec_bufsize <= 0) {
			LOG_ERR("Requested buffer size must be positive");
			return;
		}

		shell_print(sh, "New buffer size is %d bytes, previously was %d bytes", usr_spec_bufsize, buf_size);
		buf_size = usr_spec_bufsize;
	}

	sd = socket(AF_INET6, SOCK_STREAM, 0);
	if (sd < 0) {
		LOG_ERR("Failed to create TCP socket (IPv6): %d", -errno);
		sd = 0;
		return;
	}
	shell_print(sh, "Initialized socket");
}

void cmd_quit(const struct shell *sh) {
	/* Tear down connection (if setup) and socket */
	if (sd < 0) {
		LOG_ERR("Must initialize session before running \"quit\"");
		return;
	}

	if (conn_sd >= 0) {
		shell_print(sh, "Closing connection");
		close(conn_sd);
	}

	shell_print(sh, "Closing socket");
	close(sd);
}

#if IS_LISTENER == 1
/* Listener-specific code */
void cmd_listen(const struct shell *sh) {
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

	shell_print(sh, "Waiting for TCP connection on port %d (IPv6)...", APP_PORT);
	conn_sd = accept(sd, (struct sockaddr *) &conn_addr, &conn_addr_len);
	if (conn_sd < 0) {
		LOG_ERR("IPv6 accept error (%d)", -errno);
		return;
	}

	inet_ntop(conn_addr.sin6_family, &conn_addr.sin6_addr, addr_str, sizeof(addr_str));
	shell_print(sh, "TCP (IPv6): Accepted connection from %s", addr_str);
}

void cmd_benchmark_recv(const struct shell *sh) {
	ssize_t bytes_in_benchmark, bytes_recvd, total_bytes_recvd;

	if (sd < 0) {
		LOG_ERR("Must initialize session before running \"bencmark_recv\"");
		return;
	} else if (conn_sd < 0) {
		LOG_ERR("Must listen for connections before running \"bencmark_recv\"");
		return;
	}

	/* 1. Determine total number of bytes to recv */
	shell_print(sh, "Waiting to receive number of bytes in benchmark");
	bytes_recvd = recv(conn_sd, &bytes_in_benchmark, sizeof(ssize_t), 0);
	if (bytes_recvd < 0) {
		LOG_ERR("Receive failed: %d", -errno);
		return;
	}

	/* 2. Notify connector to start benchmark by sending recvd value back */
	shell_print(sh, "Benchmark size %d bytes. Sending back to synchronize", bytes_in_benchmark);
	if (send(conn_sd, &bytes_in_benchmark, sizeof(ssize_t), 0) < 0) {
		LOG_ERR("Send failed: %d", -errno);
		return;
	}

	/* 3. Receive benchmark data in "buf_size" chunks */
	shell_print(sh, "Beginning benchmark");
	total_bytes_recvd = 0;
	while (total_bytes_recvd < bytes_in_benchmark
		&& (bytes_recvd = recv(conn_sd, recv_buf, sizeof(char)*buf_size, 0)) > 0) {
			total_bytes_recvd += bytes_recvd;
			LOG_DBG("Received %d bytes", bytes_recvd);
	}
	if (bytes_recvd < 0) {
		LOG_ERR("Receive failed: %d", -errno);
		cmd_quit();
		return;
	}

	/* 4. Send amount of data received to end benchmark */
	shell_print(sh, "Received %d bytes total", total_bytes_recvd);
	if (send(conn_sd, &total_bytes_recvd, sizeof(ssize_t), 0) < 0) {
		LOG_ERR("Send failed: %d", -errno);
		return;
	}
	shell_print(sh, "Ending benchmark");
}
#else
/* Connector-specific code */
void cmd_connect(const struct shell *sh) {
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
	shell_print(sh, "Successfully connected");
}

void cmd_benchmark_send(const struct shell *sh, size_t argc, char **argv) {
	uint32_t cur_cycle_count, time_start_ms, time_delta_ms, thousand_times_goodput;
	ssize_t bytes_in_benchmark, bytes_recvd, bytes_sent, ret, total_bytes_sent;

	if (sd < 0) {
		LOG_ERR("Must initialize session before running \"bencmark_recv\"");
		return;
	} else if (conn_sd < 0) {
		LOG_ERR("Must listen for connections before running \"bencmark_recv\"");
		return;
	}

	/* 0. Initialize benchmark parameters */
	if (argc > 1) {
		bytes_in_benchmark = (ssize_t) atoi(argv[1]);

		if (bytes_in_benchmark <= 0) {
			LOG_ERR("Requested benchmark size must be greater than 0");
			return;
		} else if (bytes_in_benchmark < buf_size) {
			shell_warn(sh, "Requested benchmark size (%d bytes) is smaller than buffer size (%d bytes)", bytes_in_benchmark, buf_size);
			buf_size = bytes_in_benchmark;
		}
	} else {
		bytes_in_benchmark = (72 << 10);
	}

	/* 1. Send listener number of bytes to receive */
	shell_print(sh, "Sending number of bytes in benchmark (%d bytes)", bytes_in_benchmark);
	ret = send(conn_sd, &bytes_in_benchmark, sizeof(ssize_t), 0);
	if (ret < 0) {
		LOG_ERR("Send failed: %d", -errno);
		return;
	}

	/* 2. Receive value sent to connector, syncing devices before running test */
	shell_print(sh, "Receiving back to synchronize");
	if (recv(conn_sd, &bytes_recvd, sizeof(ssize_t), 0) < 0) {
		LOG_ERR("Receive failed: %d", -errno);
		return;
	}

	/* 3. Begin benchmark */
	shell_print(sh, "Beginning benchmark");
	cur_cycle_count = k_cycle_get_32();
	time_start_ms = k_cyc_to_ms_ceil32(cur_cycle_count);
	total_bytes_sent = 0;
	do {
		bytes_sent = send(conn_sd, send_buf, sizeof(char)*buf_size, 0);
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
	shell_print(sh, "Receiving number of bytes sent");
	ret = recv(conn_sd, &bytes_recvd, sizeof(ssize_t), 0);

	cur_cycle_count = k_cycle_get_32();
	time_delta_ms = k_cyc_to_ms_ceil32(cur_cycle_count) - time_start_ms;

	if (ret < 0) {
		LOG_ERR("Receive failed: %d", -errno);
		return;
	} else if (bytes_in_benchmark != bytes_recvd) {
		LOG_ERR("Bytes received (%d) not equal to total bytes sent (%d)", bytes_recvd, bytes_in_benchmark);
		return;
	}
	shell_print(sh, "Benchmark complete. Sent %u bytes in %u milliseconds", bytes_recvd, time_delta_ms);

	/* 5. Calculate and print goodput */
	thousand_times_goodput = (1000 * (bytes_in_benchmark << 3) + (time_delta_ms >> 1)) / time_delta_ms;
	shell_print(sh, "Goodput: %u.%03u kb/s", thousand_times_goodput / 1000, thousand_times_goodput % 1000);
}
#endif

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
	LOG_INF("Listener device running");
	memset(&recv_buf, 'o', MAXBUFSIZE);
#else
	LOG_INF("Connector device running");
	memset(&send_buf, 'x', MAXBUFSIZE);
#endif
}