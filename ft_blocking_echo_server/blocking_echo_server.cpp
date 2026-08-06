/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   blocking_echo_server.cpp                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aaycan <aaycan@student.42kocaeli.com.tr    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/06 17:33:19 by aaycan            #+#    #+#             */
/*   Updated: 2026/08/06 17:33:22 by aaycan           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

static int	allow_address_reuse(int server_fd);
static int	bind_socket(int server_fd, struct sockaddr_in *address);
static int	start_listening(int server_fd);
static int	accept_connection(int server_fd);
static void	handle_client(int client_fd);

int main()
{
    int					server_fd;
    struct sockaddr_in	address;

	server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); //AF_INET = ipv4 (TCP is the standard protocol that provides SOCK_STREAM semantics for AF_INET.)
	if (server_fd == -1)
	{
		std::cerr << "Error: socket() failed" << std::endl;
		return (1);
	}
	std::cout << "Socket created, fd = " << server_fd << std::endl;
	address = sockaddr_in();
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY; // bind to all available network interfaces on this machine
	address.sin_port = htons(8080); // byte order
	if (allow_address_reuse(server_fd) == 1)
		return (1);
	if (bind_socket(server_fd, &address) == 1)
		return (1);
	if (start_listening(server_fd) == 1)
		return (1);
	if (accept_connection(server_fd) == 1)
		return (1);
	close(server_fd);
	std::cout << "Socket closed" << std::endl;
	return (0);
}

static int	allow_address_reuse(int server_fd)
{
	int	opt;
	int	sockopt_result;

	opt = 1;
	sockopt_result = setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (sockopt_result == -1)
	{
		std::cerr << "Error: setsockopt() failed" << std::endl;
		close(server_fd);
		return (1);
	}
	return (0);
}

static int	bind_socket(int server_fd, struct sockaddr_in *address)
{
	int	bind_result;

	bind_result = bind(server_fd, (struct sockaddr *)address, sizeof(*address));
	if (bind_result == -1)
	{
		std::cerr << "Error: bind() failed" << std::endl;
		close(server_fd);
		return (1);
	}
	std::cout << "Socket bound to port 8080" << std::endl;
	return (0);
}

static int	start_listening(int server_fd)
{
	int	listen_result;

	listen_result = listen(server_fd, 10);
	if (listen_result == -1)
	{
		std::cerr << "Error: listen() failed" << std::endl;
		close(server_fd);
		return (1);
	}
	std::cout << "Server listening on port 8080..." << std::endl;
	return (0);
}

static int	accept_connection(int server_fd)
{
	struct sockaddr_in	client_address;
	socklen_t			client_len;
	int					client_fd;

	client_len	= sizeof(client_address);
	while (1)
	{
		client_fd = accept(server_fd, (struct sockaddr *)&client_address, &client_len);
		if (client_fd == -1)
		{
			std::cerr << "Error: accept() failed" << std::endl;
			close(server_fd);
			return (1);
		}
		std::cout << "Client connected, fd = " << client_fd << std::endl;
		handle_client(client_fd);
	}
}

static void	handle_client(int client_fd)
{
	char	buffer[1024];
	ssize_t	bytes_read;

	while (1)
	{
		bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
		if (bytes_read <= 0)
		{
    		if (bytes_read == -1)
    		    std::cerr << "Error: read() failed" << std::endl;
    		close(client_fd);
    		std::cout << "Client disconnected, fd = " << client_fd << std::endl;
    		return ;
		}
		std::string received(buffer, bytes_read);
		std::cout << "Recieved: " << received;
		if (received == "exit\r\n")
		{
			close(client_fd);
			std::cout << "Client disconnected, fd = " << client_fd << std::endl;
			return ;
		}
		write(client_fd, received.c_str(), received.size());
	}
}
