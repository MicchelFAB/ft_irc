#include "Server.hpp"

Server::Server(){}

Server::Server(int port, std::string pass) : _port(port), _pass(pass), _nfds(1), _cur_online(0) {
	_epoll_fd = epoll_create1(0);
	if (_epoll_fd == -1) {
		std::cerr << _RED << "Error creating epoll file descriptor" << _END << std::endl;
		exit(EXIT_FAILURE);
	}
	if (port < 1 || port > 65535){
		std::cerr << _RED << "Invalid port: port needs to be between 1 and 65535" << _END << std::endl;
		exit(EXIT_FAILURE);
	}
}

Server::~Server() {
	close(_socket_Server);
	close(_epoll_fd);
	for (std::map<int, Client *>::iterator it = clients.begin(); it != clients.end(); it++) {
		delete it->second;
	}
	clients.clear();

	for (std::map<std::string, Channel *>::iterator it = channels.begin(); it != channels.end(); ++it) {
		delete it->second;
	}
	channels.clear();

	if (bot){
		delete bot;
	}
}

int const &Server::get_socket() const{
	return _socket_Server;
}

void Server::binding(){
	struct addrinfo hints, *serverinfo, *tmp;
	int status;

	memset(&hints, 0, sizeof(hints));
	memset(&_events, 0, sizeof(_events));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = getprotobyname("TCP")->p_proto;

	int opt = 1;
	status = getaddrinfo("0.0.0.0", toString(_port).c_str(), &hints, &serverinfo);
	if (status != 0){
		std::cout << "ERROR ON GETTING ADDRESS INFO" << std::endl;
		exit (-1);
	}
	for (tmp = serverinfo; tmp != NULL; tmp = tmp->ai_next){
		this->_socket_Server = socket(tmp->ai_family, tmp->ai_socktype | O_NONBLOCK, tmp->ai_protocol);
		if (this->_socket_Server < 0)
			continue;
		setsockopt(this->_socket_Server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
		if (bind(_socket_Server, tmp->ai_addr, tmp->ai_addrlen) < 0){
			close(this->_socket_Server);
			continue;
		}
		break;
	}

	freeaddrinfo(serverinfo);

	if (tmp == NULL) {
		std::cerr << "Failed to bind to any address..." << std::endl;
		exit(EXIT_FAILURE);
	}
	

	if (listen(_socket_Server, MAX_CLIENTS) == -1) {
    	std::cerr << "Error in listen()" << std::endl;
    	exit(EXIT_FAILURE);
	}

	_events[0].data.fd = _socket_Server;
	_events[0].events = EPOLLIN | EPOLLET | EPOLLOUT;
	
	if(epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _socket_Server, _events) == -1){
		std::cerr << "Error adding socket to epoll" << std::endl;
		close(_socket_Server);
		exit(EXIT_FAILURE);
	}
	_cur_online++;

	bot = new Bot(1);
	std::cout << _RED << "BOT FD: " << bot->get_bot_fd() << _END << std::endl;
}

void Server::loop(){
	while(true){

		std::cout << "Waiting for connections..." << std::endl;
		_nfds = epoll_wait(_epoll_fd, _events, MAX_CLIENTS, 50);
		//usleep(5000);
		if (_nfds == -1) {
			std::cerr << "Error during epoll_wait: " << strerror(errno) << std::endl;
			exit(EXIT_FAILURE);
		}
		std::cout << _BOLD << _YELLOW << "AFTER THE EPOLL_CTL" << _END << std::endl;
		

		for(int i = 0; i < _cur_online; i++){
			std::cout << "AAAAAAAAAAAAAAAAA: " << _events[i].events << " FROM FD: " << _events[i].data.fd << " AND IN THE FOR IS THE: " << i << " FROM A TOTAL OF: " << _cur_online << std::endl;
			if (_events[i].events & EPOLLET)
				continue;
			if (_events[i].events & EPOLLHUP){
				if (clients.find(_events[i].data.fd) != clients.end()){
					std::cerr << _RED << "Client disconnected (FROM THE EPOLLHUP WITH CLIENT NO: " << _events[i].data.fd << "). Current onlines: " << _cur_online << _END << std::endl;
					std::istringstream iss("Unexpected disconnection by client");
					handleQuit(_events[i].data.fd, iss);
					break;
				}
			}
			if ((_events[i].events & EPOLLIN)) {
				if(_events[i].data.fd == _socket_Server){
					//std::cout << _RED << "DAMN FRICK(new)" << _END << std::endl;
					funct_new_client(i);
					break;
				}
				else if(clients.find(_events[i].data.fd) != clients.end() && clients[_events[i].data.fd]->empty_buffer()){
					std::cout << _PURPLE << "EPOLLIN (FROM CLIENT: " << clients[_events[i].data.fd]->get_client_fd() << ")" << _END << std::endl;
					//std::cout << _RED << "DAMN FRICK(not new)" << _END << std::endl;	
					//clients[_events[i].data.fd]->ready_in = 4;
					funct_not_new_client(i);
					if (!clients[_events[i].data.fd]->empty_buffer())
						break;
					continue;
				}
			}
			if ((_events[i].events & EPOLLOUT)) {
				if((clients.find(_events[i].data.fd) != clients.end() && !clients[_events[i].data.fd]->empty_buffer())){
					std::cout << _PURPLE << "EPOLLOUT (FROM CLIENT: " << clients[_events[i].data.fd]->get_client_fd() << ")" << _END << std::endl;
					handle_commands(_events[i].data.fd);
					break;
				}
			}
		}
	}
}

void Server::funct_new_client(int i){
	struct sockaddr_storage client_addr;
	socklen_t client_len = sizeof(client_addr);
	int newsocket = accept(_socket_Server, (struct sockaddr*)&client_addr, &client_len);
	if (newsocket == -1)
		std::cerr << "Error accepting new connection: " << strerror(errno) << std::endl;
	fcntl(newsocket, F_SETFL, O_NONBLOCK);

	_events[i].data.fd = newsocket;
	_events[i].events = EPOLLIN | EPOLLET | EPOLLOUT;
	this->clients.insert(std::make_pair(newsocket, new Client(newsocket)));
	clients[newsocket]->set_addr(client_addr);

	if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, newsocket, &_events[i]) == -1) {
		std::cerr << "Error adding new socket to epoll: " << strerror(errno) << std::endl;
		close(newsocket);
	}
	this->_cur_online++;
}

void Server::funct_not_new_client(int i){
	int extra_bytes = 0;
	char buffer_ptr[1024];
	extra_bytes = recv(_events[i].data.fd,  buffer_ptr, sizeof(buffer_ptr), 0);
	/* std::cout << _CYAN << "EPOLLIN VALUE: " << EPOLLIN << _END << std::endl;
	std::cout << _CYAN << "EPOLLOUT VALUE: " << EPOLLOUT << _END << std::endl;
	std::cout << _CYAN << "EPOLLET VALUE: " << EPOLLET << _END << std::endl;
	std::cout << _PURPLE << _events[i].events << _END << std::endl; */

	if (extra_bytes == -1)
	{
		int err_code = 0;
    	socklen_t len = sizeof(err_code);
		if (getsockopt(_events[i].data.fd, SOL_SOCKET, SO_ERROR, &err_code, &len) == -1){
        	std::cout << _RED << "Error on getsocket()" << _END << std::endl;
			if (this->clients.find(_events[i].data.fd) == this->clients.end()) {
				return ;
			}
			throw std::invalid_argument("SOME ERROR");
		}
		else if(err_code == EAGAIN || err_code == EWOULDBLOCK)
				std::cout << _YELLOW << "Temporary recv() error" << _END << std::endl;
        else if(err_code != 0){

            errno = err_code;
			if (epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, _events[i].data.fd, NULL) == -1) {
				std::cerr << "Error removing socket from epoll(not new client): " << strerror(errno) << std::endl;
				return;
			}
			else
			{
				close(_events[i].data.fd);
				std::cerr << _RED << "Error in recv(). Current onlines: " << _cur_online  << " WITH ERROR NO: " << err_code << _END << std::endl;
				end_connection(_events[i].data.fd);
			}
		}
	}
	if (extra_bytes > 0){
		//_events[i].events = EPOLLOUT;
		clients[_events[i].data.fd]->set_bytes_received(clients[_events[i].data.fd]->get_bytes_received() + extra_bytes);
		buffer_ptr[extra_bytes] = '\0';
		std::cout << _PURPLE << buffer_ptr << std::endl;
		this->clients[_events[i].data.fd]->add_to_buffer(buffer_ptr);
		//std::cout << _CYAN << "here" << _END << std::endl;
	}
	else if (extra_bytes == 0)
	{
		/* if (epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, _events[i].data.fd, NULL) == -1)
		{
			std::cerr << "Error removing socket from epoll(not new client): " << strerror(errno) << std::endl;
		}
		else
		{
			std::cerr << _RED << "Client disconnected (FROM THE NEW STUFF). Current onlines: " << _cur_online << _END << std::endl;
			std::istringstream iss("Unexpected disconnection by client");
			handleQuit(_events[i].data.fd, iss);
		} */
		return;
	}
	if (this->clients.find(_events[i].data.fd) == this->clients.end() && this->clients[_events[i].data.fd] != 0)
		return;
	std::string command(this->clients[_events[i].data.fd]->get_buffer());
	if (this->clients[_events[i].data.fd]->get_buffer().find("\n") == std::string::npos){
		std::cout << ">>" << this->clients[_events[i].data.fd]->get_buffer() << "<<" << std::endl;	
		return;
	}
	if (!command.empty() && command[command.size() - 1] == '\r')
	{
		command.erase(command.end() - 1);
	}
	//handle_commands(_events[i].data.fd, command);
	//clients[_events[i].data.fd]->add_to_buffer(command);
	memset(buffer_ptr, 0, 1024);
	/* std::cout << _RED << "COMMAND SENT BY CLIENT: " << _events[i].data.fd << " " << _END << _GREEN << command << _RED << "END OF COMMAND" << _END << std::endl;
	if (this->clients.find(_events[i].data.fd) != this->clients.end() && this->clients[_events[i].data.fd] != 0) { 
		this->clients[_events[i].data.fd]->set_bytes_received(0);
		this->clients[_events[i].data.fd]->clean_buffer();
	} */
}

void Server::handle_commands(int fd){
	/* std::istringstream commandStream(clients[fd]->get_buffer());
	clients[fd]->clean_buffer();
    std::string line;
    while (std::getline(commandStream, line, '\n')) {
        std::istringstream iss(line);
        std::string cmd;
		iss >> cmd; */
	std::string line = clients[fd]->get_first_buffer();
	std::istringstream iss(line);
	std::string cmd;
	iss >> cmd;
	std::cout << _RED << cmd << _END << std::endl << std::endl;
	std::cout << _CYAN << clients[fd]->get_buffer() << _END << std::endl;
	std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
	clients[fd]->ready_in = 1;
	if (cmd == "AUTH")
		handleAuth(fd);
	else if (cmd == "PASS")
		handlePass(fd, iss);
	else if (cmd == "NICK")
		handleNick(fd, iss);
	else if (cmd == "USER")
		handleUser(fd, iss);
	else if (cmd == "JOIN")
		handleJoin(fd, iss);
	else if (cmd == "PRIVMSG")
		handlePrivmsg(fd, iss);
	else if (cmd == "PART")
		handlePart(fd, iss);
	else if (cmd == "QUIT")
		handleQuit(fd, iss);
	else if (cmd == "PING")
		handlePing(fd, iss);
	else if (cmd == "MODE")
		handleMode(fd, iss);
	else if (cmd == "KICK")
		handleKick(fd, iss);
	else if (cmd == "INVITE")
		handleInvite(fd, iss);
	else if (cmd == "TOPIC")
		handleTopic(fd, iss);
	else if (cmd == "WHO")
		handleWho(fd, iss);
	else if (cmd == "WHOIS")
		handleWhois(fd, iss);
	else if (cmd == "LIST")
		handleList(fd);
	//}
}

void Server::create_channel(const std::string &channelName, int fd){
	std::map<std::string, Channel *>::iterator it = channels.find(channelName);
	if (it == channels.end()){
		Channel *channel = new Channel(channelName, this->clients[fd]);
		channels.insert(std::make_pair(channelName, channel));
		channels[channelName]->add_modes('t');
		if (bot){
			std::cout << _RED <<"BEFORE"  << _END << std::endl;
			this->bot->add_channel(channelName, *this->channels[channelName]);
			std::cout << _RED <<"MIDDLE" << _END << std::endl;
			this->channels[channelName]->add_bot(get_bot());
			std::cout << _RED <<"AFTER" << _END << std::endl;
			print_client(bot->get_bot_fd(), bot->get_mask() + "JOIN :" + channelName + "\r\n");
		}

	}
}

Channel *Server::get_channel(const std::string name)  {
	if (channels.find(name) != channels.end())
		return channels[name];
	return NULL;
}

Client &Server::get_client(int fd){
	std::map<int, Client *>::iterator it = clients.find(fd);
	return *it->second;
}

Bot &Server::get_bot(){
	return *bot;
}

void Server::_ToAll(int ori_fd, std::string message){
	std::set<std::string> channelList = findInChannel(ori_fd);
	while (!channelList.empty()){
		std::string channelName = *channelList.begin();
		std::map<int, Client *> all_users = channels[channelName]->get_users();
		std::map<int, Client *>::iterator it = all_users.begin();
		while (it != all_users.end()){
			if (ori_fd != it->first){
				print_client(it->first, message);
			}
			it++;
		}
		channelList.erase(channelList.begin());
	}
}

void Server::_ToAll(Channel *channel, int ori_fd, std::string message){
	std::map<int, Client *> all_users = channel->get_users();
	std::map<int, Client *>::iterator it = all_users.begin();
	std::string rep = this->clients[ori_fd]->get_mask();
	rep.append(message);
	while (it != all_users.end()){
		if (ori_fd != it->first)
			print_client(it->first, rep);
		it++;
	}
}

// The purpose of this is to locate the user on a channel among all the channels stored on the server.
// if so, it returns the list with the name of the channels, otherwise it returns null.
std::set<std::string> Server::findInChannel(int fd){
	std::set<std::string> channelList;
	std::map<std::string, Channel *>::iterator it = channels.begin();
	while (it != channels.end()){
		std::map<int, Client *> users = it->second->get_users();
		if (users.find(fd) != users.end()) {
			channelList.insert(it->first);
		}
		it++;
	}
	return channelList;
}

void Server::print_client(int client_fd, std::string str){
	send(client_fd, str.c_str(), str.size(), MSG_NOSIGNAL);
}
 
void Server::sendCode(int fd, std::string num, std::string nickname, std::string message){
	if (nickname.empty())
		nickname = "*";
	print_client(fd, ":server " + num + " " + nickname + " " + message + "\r\n");
}

int Server::_sendall(int destfd, std::string message)
{
	int total = 0;
	int bytesleft = message.length();
	int b;

	while (total < (int)message.length())
	{
		b = send(destfd, message.c_str() + total, bytesleft, MSG_NOSIGNAL);
		if (b == -1) break;
		total += b;
		bytesleft -= b;
	}
	return (b == -1 ? -1 : 0);
}

bool Server::findNick(std::string nick){
	for(std::map<int, Client *>::iterator it = clients.begin(); it != clients.end(); it++){
		if (it->second->get_nick() == nick)
			return true;
	}
	return false;
}

void Server::end_connection(int fd){
	delete clients[fd];
	this->clients.erase(fd);
	this->_cur_online--;
}
