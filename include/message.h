/**
 * Copyright (c) 2005 - 2006 Kroon Infomation Systems,
 *  with contributions from various authors.
 *
 * This file is distributed under GPLv2, please see
 * COPYING for more details.
 *
 * $Id$
 */
#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdint.h>
#include <semaphore.h>
#include <map>

#define MESSAGE_SIGNATURE_SIZE		128

class Message;

typedef Message* (*MessageFunctor)();

class Message {
private:
	static std::map<uint32_t, MessageFunctor> _functors;

	struct st_blob {
		uint32_t server_id;
		uint32_t message_id;
		uint32_t time;
		uint32_t message_type_id;
		uint8_t signature[MESSAGE_SIGNATURE_SIZE];
		uint8_t data[1];
	} __attribute__((packed));

	uint32_t _server_id;
	uint32_t _message_id;
	uint32_t _time;
	uint32_t _data_size;
	uint8_t *_data;
	uint8_t *_signature;
	uint32_t _blob_size;
	struct st_blob *_blob;
	sem_t   *_watcher;

	bool buildMessage(uint32_t server_id, uint32_t message_id,
			uint32_t time, uint8_t *signature, uint8_t *data,
			uint32_t data_len, struct st_blob *blob, uint32_t blob_size);
	void makeBlob();

	// Prevent copying
	Message(const Message &);
	Message &operator =(const Message &);
protected:
	/**
	 * This function needs to return the size requirements for
	 * storing this message into a binary blob.  It needs to
	 * add it's own requirements to that of it's parent class.
	 *
	 * In cases where the size cannot be determined accurately
	 * before actually storing, an upper bound should be returned.
	 *
	 * Protected since the makeBlob() function should be used to
	 * initiate the build process.
	 */
	virtual uint32_t storageRequired() = 0;

	/**
	 * This function is required to transform the message object
	 * into a binary blob.  It should return the number of bytes
	 * actually used (inclusive that of it's parent).  The buffer
	 * will be of size storageRequired(), even though the buffer
	 * size is passed explicitly.  In case of failure the function
	 * should return ~0U.
	 *
	 * Protected since the makeMessage() function should be used to
	 * initiate the build process.
	 */
	virtual uint32_t store(uint8_t *buffer, uint32_t size) = 0;

	/**
	 * This function needs to rebuild the internal message state
	 * from the blob it stored above.  Returns the number of bytes
	 * absorbed from the stream or ~0U on error.
	 *
	 * Protected since the buildMessage() function(s) should be used to
	 * initiate the load process.
	 */
	virtual uint32_t load(const uint8_t *buffer, uint32_t size) = 0;

	/**
	 * This function can be used by subclasses to check for zero-terminators
	 * in buffers in a particular buffer area.  It can specify how many
	 * strings there need to be in a particular area of a buffer.
	 */
	bool checkStringTerms(const uint8_t* buf, uint32_t sz, uint32_t nzeros = 1);

	/**
	 * This function is the core of message processing. The generic boilerplate
	 * (recording processing in the database and waking up appropriate callers)
	 * is handled by Message::process.
	 */
	virtual bool int_process() const = 0;
public:
	Message();
	virtual ~Message();

	uint32_t server_id() const { return _server_id; }
	uint32_t message_id() const { return _message_id; }
	uint32_t time() const { return _time; }

	/**
	 * Processes the message and records it as processed in the database.
	 * Also notifies any semaphore set with setWatcher.
	 */
	bool process();

	/**
	 * This function needs to return the message_type_id for
	 * the message type.  This must be the same for all instances
	 * of the same class of message, and should only be implemented
	 * by the lowest level message classes.
	 */
	virtual uint16_t message_type_id() const = 0;

	/**
	 * This function can be used to retrieve a pointer to the internal
	 * buffer used for storing the binary blob.  Returns true if values
	 * correctly set.  This blob includes server_id, message_id,
	 * time, message_type_id _and_ the signature.
	 */
	bool getBlob(const uint8_t ** buffer, uint32_t *size) const;

	/**
	 * This function should be called on newly constructed messages.
	 * It will assign as server_id and message_id _and_ send it off to
	 * all other servers, it will, however, not enqueue the message
	 * into the local message queue - that needs to be done by the caller
	 * _after_ calling makeMessage(), and only if returning true!
	 */
	bool makeMessage();

	/**
	 * Callback for DbCon derivitaves.  Will do nothing unless called
	 * indirectly via makeMessage()!
	 */
	void setMessageId(uint32_t id);

	/**
	 * Returns a pointer to the cryptographic signature for this
	 * message.
	 */
	const uint8_t* getSignature() const { return _signature; };

	/**
	 * Return a pointer to the internal data buffer, this buffer contains
	 * the Message dependant parts of the message as created by store().
	 * This buffer won't be encrypted.
	 */
	bool getData(const uint8_t ** buffer, uint32_t *size) const;

	/**
	 * Requests notification after the message is processed. The given
	 * semaphore is posted. Note that there can only be one watcher per
	 * message.
	 */
	void setWatcher(sem_t *watcher);

	/**
	 * All subclasses of Message should get registered via this call.
	 * This acts as a factory when reloading/importing Messages.
	 */
	static void registerMessageFunctor(uint32_t, MessageFunctor);

	/**
	 * Can be used by Db code for example to load messages.  This function
	 * transfers ownership of the memory pointed to by signature and data
	 * to the Message instance (iff successful).
	 */
	static Message* buildMessage(uint32_t server_id, uint32_t message_id,
			uint32_t message_type_id, uint32_t time, uint8_t *signature,
			uint8_t *data, uint32_t data_len);

	/**
	 * Used to load a message directly from a buffer.  This is the exact
	 * same data as returned by getBlob().  Again, ownership of the memory
	 * is transferred (iff successful).
	 */
	static Message* buildMessage(uint8_t* blob, uint32_t blob_len);
};

#endif
