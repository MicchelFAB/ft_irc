#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include "colors.hpp"
class Client;

class Channel
{
private:
    std::string name;
	//! CHANGE ALSO THIS FOR STRINGS AND NOT INT
    std::map<int , Client *> users;
    //TODO: BANNED USERS
    
    Channel(const Channel &cp);
	Channel &operator=(const Channel &orign);
public:
    Channel();
    Channel(const std::string name);
    ~Channel();

    void addUser(Client &client);
    void removeUser(std::string user);
    std::string const &getName(void) const;
    std::set<int> const &getUsers(void) const;
	std::string getUser(std::string const user) const;
    void setName(std::string name);
    void setUser(int id, Client *client);
    
};

#include "Client.hpp"

#endif
