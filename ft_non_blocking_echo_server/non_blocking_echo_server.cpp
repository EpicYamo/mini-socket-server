/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   non_blocking_echo_server.cpp                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aaycan <aaycan@student.42kocaeli.com.tr    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/08/06 17:32:59 by aaycan            #+#    #+#             */
/*   Updated: 2026/08/18 05:06:21 by aaycan           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <vector>
#include <map>
#include <csignal>
#include <cerrno>

static volatile sig_atomic_t	g_shutdown = 0;

static int	allow_address_reuse(int server_fd);
static int	bind_socket(int server_fd, struct sockaddr_in *address);
static int	start_listening(int server_fd);
static int	set_non_blocking(int fd);
static void	run_server(int server_fd);
static void handle_new_connection(int server_fd, std::vector<pollfd> &poll_fds);
static bool	handle_client_read(std::vector<pollfd> &poll_fds, size_t index,
				std::vector<int> &to_remove,
				std::map<int, std::string> &pending_writes);
static void	handle_client_write(std::vector<pollfd> &poll_fds, size_t index,
				std::vector<int> &to_remove,
				std::map<int, std::string> &pending_writes);
static void	remove_closed_clients(std::vector<pollfd> &poll_fds,
				std::vector<int> &to_remove,
				std::map<int, std::string> &pending_writes);
static void	handle_sigint(int signum);

int main()
{
    int					server_fd;
    struct sockaddr_in	address;

	signal(SIGINT, handle_sigint);
	server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (server_fd == -1)
	{
		std::cerr << "Error: socket() failed" << std::endl;
		return (1);
	}
	std::cout << "Socket created, fd = " << server_fd << std::endl;
	address = sockaddr_in();
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(8080);
	if (allow_address_reuse(server_fd) == 1)
		return (1);
	if (bind_socket(server_fd, &address) == 1)
		return (1);
	if (start_listening(server_fd) == 1)
		return (1);
	if (set_non_blocking(server_fd) == 1)
		return (1);
	run_server(server_fd);
	close(server_fd);
	std::cout << "Socket closed" << std::endl;
	return (0);
}

static void	handle_sigint(int signum)
{
	(void)signum;
	g_shutdown = 1;
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

static int	set_non_blocking(int fd)
{
	int	flags;

	flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
	{
		std::cerr << "Error: fcntl(F_GETFL) failed" << std::endl;
		close(fd);
		return (1);
	}
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
	{
		std::cerr << "Error: fcntl(F_SETFL) failed" << std::endl;
		close(fd);
		return (1);
	}
	return (0);
}

static void	run_server(int server_fd)
{
	std::vector<pollfd>			poll_fds;
	pollfd						server_entry;
	int							poll_result;
	std::vector<int>			to_remove;
	std::map<int, std::string>	pending_writes;

	server_entry.fd = server_fd;
	server_entry.events = POLLIN;
	server_entry.revents = 0;
	poll_fds.push_back(server_entry);
	while (!g_shutdown)
	{
		poll_result = poll(poll_fds.data(), poll_fds.size(), -1);
		if (poll_result == -1)
		{
			if (errno == EINTR)
				continue ;
			std::cerr << "Error: poll() failed" << std::endl;
			break ;
		}
		to_remove.clear();
		for (size_t i = 0; i < poll_fds.size(); i++)
		{
			bool	still_connected;
			
			still_connected = true;
			if (poll_fds[i].fd == server_fd)
			{
				if (poll_fds[i].revents & POLLIN)
					handle_new_connection(server_fd, poll_fds);
				continue ;
			}
			if (poll_fds[i].revents & POLLIN)
				still_connected = handle_client_read(poll_fds, i, to_remove, pending_writes);
			if (still_connected && (poll_fds[i].revents & POLLOUT))
				handle_client_write(poll_fds, i, to_remove, pending_writes);
		}
		remove_closed_clients(poll_fds, to_remove, pending_writes);
	}
	for (size_t i = 0; i < poll_fds.size(); i++)
	{
		if (poll_fds[i].fd != server_fd)
			close(poll_fds[i].fd);
	}
	std::cout << "[Shutting down the server]" << std::endl;
}

static void handle_new_connection(int server_fd, std::vector<pollfd> &poll_fds)
{
	struct sockaddr_in	client_address;
	socklen_t			client_len;
	int					client_fd;
	pollfd				client_entry;

	client_len = sizeof(client_address);
	client_fd = accept(server_fd, (struct sockaddr *)&client_address, &client_len);
	if (client_fd == -1)
		std::cerr << "Error: accept() failed" << std::endl;
	else
	{
		if (set_non_blocking(client_fd) == 1)
			close(client_fd);
		else
		{
			client_entry.fd = client_fd;
			client_entry.events = POLLIN;
			client_entry.revents = 0;
			poll_fds.push_back(client_entry);
			std::cout << "Client connected, fd = " << client_fd << std::endl;
		}
	}
}

static bool	handle_client_read(std::vector<pollfd> &poll_fds, size_t index,
				std::vector<int> &to_remove,
				std::map<int, std::string> &pending_writes)
{
	char	buffer[1024];
	ssize_t	bytes_read;
	int		client_fd;

	client_fd = poll_fds[index].fd;
	bytes_read = read(client_fd, buffer, sizeof(buffer));
	if (bytes_read <= 0)
	{
		if (bytes_read == -1)
			std::cerr << "Error: read() failed" << std::endl;
		close(client_fd);
		std::cout << "Client disconnected, fd = " << client_fd << std::endl;
		to_remove.push_back(client_fd);
		return (false);
	}
	std::string received(buffer, bytes_read);
	std::cout << "Received: " << received;
	pending_writes[client_fd] += received;
	poll_fds[index].events |= POLLOUT;
	return (true);
}

static void	remove_closed_clients(std::vector<pollfd> &poll_fds,
				std::vector<int> &to_remove,
				std::map<int, std::string> &pending_writes)
{
	size_t	i;
	size_t	j;

	i = 0;
	while (i < to_remove.size())
	{
		pending_writes.erase(to_remove[i]);
		j = 0;
		while (j < poll_fds.size())
		{
			if (poll_fds[j].fd == to_remove[i])
			{
				poll_fds.erase(poll_fds.begin() + j);
				break ;
			}
			j++;
		}
		i++;
	}
}

static void	handle_client_write(std::vector<pollfd> &poll_fds, size_t index,
				std::vector<int> &to_remove,
				std::map<int, std::string> &pending_writes)
{
	int										client_fd;
	std::map<int, std::string>::iterator	it;
	ssize_t									bytes_written;

	client_fd = poll_fds[index].fd;
	it = pending_writes.find(client_fd);
	if (it == pending_writes.end())
	{
		poll_fds[index].events &= ~POLLOUT;
		return ;
	}
	bytes_written = write(client_fd, it->second.c_str(), it->second.size());
	if (bytes_written == -1)
	{
		std::cerr << "Error: write() failed" << std::endl;
		close(client_fd);
		std::cout << "Client disconnected, fd = " << client_fd << std::endl;
		to_remove.push_back(client_fd);
		return ;
	}
	it->second.erase(0, bytes_written);
	if (it->second.empty())
	{
		pending_writes.erase(it);
		poll_fds[index].events &= ~POLLOUT;
	}
}
