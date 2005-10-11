#ifndef __MARKERS_H__
#define __MARKERS_H__

#include <stdint.h>
#include <pthread.h>
#include <map>
#include <list>

class ClientConnection;
class MessageBlock;

class Markers {
private:
	static Markers _instance;

	pthread_mutex_t _lock;
	std::map<ClientConnection*, uint32_t> _issued;
	std::list<ClientConnection*> _markers;
	std::list<uint32_t> _problems;

	void issue(ClientConnection*, uint32_t);
	void real_enqueueSubmission(uint32_t);

	Markers();
	~Markers();
public:
	void enqueueMarker(ClientConnection*);
	void preemptMarker(ClientConnection*);

	bool putMark(ClientConnection*, MessageBlock*);
	void enqueueSubmission(uint32_t);

	static Markers& getInstance();
};

#endif
