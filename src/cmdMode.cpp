#include "Server.hpp"

int	Server::checkMode(int fd, std::string &target, std::string &mode, std::string &arg) {
	if (!target.empty() && target[0] != '#' && mode.empty()) {
		sendCode(fd, "401", clients[fd]->get_nick(), target + " :No such nick/channel");
		return -1;
	}
	if (target.empty()) { // /MODE <+modos>
		sendCode(fd, "461", "", "Not enough parameters in MODE");
		return -1;
	}
	if (!target.empty() && target[0] == '#' && mode.empty() && arg.empty()) { //MODE <#canal>  
		if (channels.find(target) == channels.end()) {
			sendCode(fd, "403", clients[fd]->get_nick(), target + " :No such channel");
			return -1;
		}
		sendCode(fd, "324", clients[fd]->get_nick(), target + " " + channels[target]->get_modes());
		return -1;
	}
	if (!target.empty() && target[0] == '#') {
		if (channels.find(target) == channels.end()) {
			sendCode(fd, "401", clients[fd]->get_nick(), target + " :No such nick/channel");
			return -1;
		}
		if (channels[target]->get_users().find(fd) == channels[target]->get_users().end()) {
			sendCode(fd, "404", clients[fd]->get_nick(), target + " :Cannot send to channel");
			return -1;
		}
		if (channels[target]->get_operators().find(fd) == channels[target]->get_operators().end()) {
			sendCode(fd, "482", clients[fd]->get_nick(), target + " :You're not channel operator");
			return -1;
		}
	}
	return 0;
}

void	Server::genericSendMode(int fd, std::string target, char mode, std::string arg, char sign) {
	bool flag = false;
	if (sign == '+')
		flag = this->channels[target]->add_modes(mode);
	else if (sign == '-')
		flag = this->channels[target]->remove_modes(mode);
	if (flag == true || mode == 'l' || mode == 'k' || mode == 'o') {
		print_client(fd, clients[fd]->get_mask() + "MODE " + target + " " + sign + mode + " " + arg + "\r\n");
		_ToAll(channels[target], fd, "MODE " + target + " " + sign + mode + " " + arg + "\r\n");
	}
}


//MODE <#canal> <+modos> [parâmetros]: Para definir modos em canais
void Server::handleMode(int fd, std::istringstream &command) {
	std::string target, mode, arg;
	command >> target >> mode >> arg;
	if (arg == this->bot->get_name()){
		sendCode(fd, "403", clients[fd]->get_nick(), target + " :MODE command cannot be used on bot");
		return ;
	}
	if (checkMode(fd, target, mode, arg) == -1)
		return ;
	if (target[0] != '#') {
		sendCode(fd, "403", clients[fd]->get_nick(), target + " :MODE command only for Users");
		return ;
	}
	int user_fd = channels[target]->get_by_name(arg);

	if (mode[0] == '+') {
		for (size_t i = mode.size() - 1; i > 0; i--) {
			if (mode[i] == 'i' && arg.empty()) { // Invite only
				channels[target]->set_invite_channel(true);
				genericSendMode(fd, target, mode[i], arg, '+');
			}
			else if (mode[i] == 'o') { // Give operator
				if (arg.empty()) {
					sendCode(fd, "696", clients[fd]->get_nick(), target + " +o " + arg + " :You must specify a parameter for the operator mode. Syntax: <nickname>");
					continue;
				}
				channels[target]->add_operator(get_client(user_fd));
				genericSendMode(fd, target, mode[i], arg, '+');
			}
			else if (mode[i] == 'k') { // Set key
				if(arg.empty()) {
					sendCode(fd, "696", clients[fd]->get_nick(), target + " +k " + arg + " :You must specify a parameter for the key mode. Syntax: <key>");
					continue;
				}
				channels[target]->set_key(arg);
				genericSendMode(fd, target, mode[i], arg, '+');
			}
			else if (mode[i] == 'l') { // Set limit
				if(arg.empty()) {
					sendCode(fd, "696", clients[fd]->get_nick(), target + " +l " + arg + " :You must specify a parameter for the limit mode. Syntax: <limit>");
					continue;
				}
				std::stringstream ss(arg);
				double limit;
				ss >> limit;
				if (limit < 1 || limit > MAX_CLIENTS) {
					sendCode(fd, "696", clients[fd]->get_nick(), target + " l " + arg + " :Invalid limit mode parameter. Syntax: <limit>");
				}
				else {
					channels[target]->set_limit(limit);
					genericSendMode(fd, target, mode[i], arg, '+');
				}
			}
			else if (mode[i] == 't') { // Set topic
				genericSendMode(fd, target, mode[i], arg, '+');
			}
		}
	}
	else if (mode[0] == '-') {
		for (size_t i = mode.size() - 1; i > 0; i--) {
			if (mode[i] == 'i' && arg.empty()) { // Remove Invite only
				channels[target]->set_invite_channel(false);
				genericSendMode(fd, target, mode[i], arg, '-');
			}
			else if (mode[i] == 'o') { // Remove operator
				if (arg.empty()) {
					sendCode(fd, "696", clients[fd]->get_nick(), target + " -o " + arg + " :You must specify a parameter for the operator mode. Syntax: <nickname>");
					continue;
				}
				channels[target]->remove_oper(arg);
				genericSendMode(fd, target, mode[i], arg, '-');
			}
			else if (mode[i] == 'k') { // Remove key
				if (arg.empty()) {
					sendCode(fd, "696", clients[fd]->get_nick(), target + " -k " + arg + " :You must specify a parameter for the key mode. Syntax: <key>");
					continue;
				}
				channels[target]->set_key("");
				genericSendMode(fd, target, mode[i], arg, '-');
			}
			else if (mode[i] == 'l' && arg.empty()) { // Remove limit
				channels[target]->set_limit(MAX_CLIENTS);
				genericSendMode(fd, target, mode[i], arg, '-');
			}
			else if (mode[i] == 't') { // Remove topic
				genericSendMode(fd, target, mode[i], arg, '-');
			}
		}
	}

	else
		sendCode(fd, "472", clients[fd]->get_nick(), target + " :is not a recognised channel mode");
}
