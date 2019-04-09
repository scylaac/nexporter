
#include "httpserver.h"
#include "common.h"
#include "config.h"
#include "exporters.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define LISTEN_MAX_QUEUE 10
#define EXPORT_BUFSIZE 102400


static int listen_socket, conn;

void start_export (Config* conf)
{
	printf (MESG_INFO "Starting nexporter server...\n");
	
	socklen_t addrlen;
	struct sockaddr_in address = {
		.sin_family = AF_INET,
		.sin_addr.s_addr = conf -> address,
		.sin_port = htons (conf -> port)
	};

	if ((listen_socket = socket (AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf (MESG_FAIL "Cannot create socket.\n");
		exit (ERROR_CREATE_SOCKET);
	}

	/*	Make sure we can force-restart our server	*/
	static const int optval = 1;
	setsockopt (listen_socket, SOL_SOCKET, SO_REUSEADDR,
		(const void*) &optval, sizeof (int));
	
	if (bind (listen_socket, (struct sockaddr*) &address, sizeof(address)) < 0)
	{
		printf (MESG_FAIL "Cannot bind to address (maybe it's already in use).\n");
		exit (ERROR_SOCKET_BIND);
	}
	
	while (true)
	{
		if (listen (listen_socket, LISTEN_MAX_QUEUE) < 0)
		{
			printf (MESG_FAIL "Cannot listen on socket.\n");
			exit (ERROR_SOCKET_LISTEN);
		}

		if ((conn = accept (listen_socket, 
			(struct sockaddr*) &address, &addrlen)) < 0)
		{
			printf (MESG_FAIL "Cannot accept connection.\n");
			exit (ERROR_SOCKET_ACCEPT);
		}

		printf (MESG_INFO "Client request received.\n");


		ssize_t buf_length, total_length = 0;
		static char content_length_stmt_buf [64];
		int content_length_stmt_length;
		static char export_buf [EXPORT_BUFSIZE];

		for (int i = 0; i < NUM_EXPORTS; i++)
		{
			if (conf -> export_flags[i])
			{
				//printf (MESG_WARN "%lx\n", (unsigned long) exporters[i]);
				buf_length = exporters[i] (export_buf, EXPORT_BUFSIZE);
				if (buf_length == -1)
					printf (MESG_FAIL "exporter %d failed.\n", i);
				else
					total_length += buf_length;
					
			}
		}
		
		content_length_stmt_length = sprintf (
			content_length_stmt_buf,
			"Content-Length: %lu\n", 
			total_length
		);

		printf (MESG_DONE "total_length: %lu, accurate: %lu\n", 
			total_length, strlen (export_buf));


		/*
		static char line_buf [128];
		while (get_line (conn, line_buf, 128) > 0);
			printf (MESG_INFO "%s", line_buf);
		*/

		write (conn, "HTTP/1.1 200 OK\n", 16);
		write (conn, content_length_stmt_buf, content_length_stmt_length);
		write (conn, "Content-Type: text/plain\n", 25);
		write (conn, "Connection: close\n\n", 19);		// This is important
		write (conn, export_buf, total_length);	

		//close (conn);		// Do not close the connection actively
	}
}
