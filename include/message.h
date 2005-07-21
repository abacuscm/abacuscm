#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include <stdint.h>

#define MESSAGE_SIGNATURE_SIZE		128

class Message {
private:
	uint32_t _server_id;
	uint32_t _message_id;
	uint32_t _time;
	uint32_t _blob_size;
	uint8_t *_blob;
	uint8_t *_signature;
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
	virtual uint32_t storageRequired();

	/**
	 * This function is required to transform the message object
	 * into a binary blob.  It should return the number of bytes
	 * actually used (inclusive that of it's parent).  The buffer
	 * will be of size storageRequired(), even though the buffer
	 * size is passed explicitly.  In case of failure the function
	 * should return ~0U.
	 *
	 * Remember to call the superclasses!
	 *
	 * Protected since the makeBlob() function should be used to
	 * initiate the build process.
	 */
	virtual uint32_t store(uint8_t *buffer, uint32_t size);

	/**
	 * This function needs to rebuild the internal message state
	 * from the blob it stored above.  Returns the number of bytes
	 * absorbed from the stream or ~0U on error.
	 *
	 * Remember to call the superclasses!
	 *
	 * Protected since the loadBlob() function should be used to
	 * initiate the load process.
	 */
	virtual uint32_t load(const uint8_t *buffer, uint32_t size);
public:
	Message();
	virtual ~Message();

	/**
	 * This function is a placeholder for implementing the
	 * functionality required to act upon the message.
	 */
	virtual void process() const = 0;

	uint32_t server_id() const { return _server_id; };
	uint32_t message_id() const { return _message_id; };
	uint32_t time() const { return _time; };
	
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
	 * correctly set.  This blob does not include server_id, message_id,
	 * time or message_type_id!!!
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

	const uint8_t* getSignature() const { return _signature; };
};

#endif
