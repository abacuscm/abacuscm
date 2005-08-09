#ifndef __CLIENTACTION_H__
#define __CLIENTACTION_H__

#include <map>
#include <string>

class ClientConnection;
class MessageBlock;
class Message;

class ClientAction {
private:
	static std::map<int, std::map<std::string, ClientAction*> > actionmap;
protected:
	bool triggerMessage(ClientConnection *cc, Message*);
	virtual bool int_process(ClientConnection *cc, MessageBlock *mb) = 0;
public:
	ClientAction();
	virtual ~ClientAction();


	static bool registerAction(int user_type, std::string action, ClientAction *ca);
	static bool process(ClientConnection *cc, MessageBlock *mb);
};

#endif
