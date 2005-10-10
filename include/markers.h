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

	typedef struct SubData {
		uint32_t user_id;
		uint32_t prob_id;
		uint32_t time;
		uint32_t hash;
		ClientConnection* issued;
	};

	pthread_mutex_t _lock;
	std::map<ClientConnection*, SubData*> _issued;
	std::list<ClientConnection*> _markers;
	std::list<SubData*> _problems;

	void issue(ClientConnection*, SubData *);
	void enqueueSubmission(SubData *);

	Markers();
	~Markers();
public:
	void enqueueMarker(ClientConnection*);
	void preemptMarker(ClientConnection*);

	bool putMark(ClientConnection*, MessageBlock*);
	void enqueueSubmission(uint32_t user_id, uint32_t prob_id, uint32_t time);

	static Markers& getInstance();
};

#endif
