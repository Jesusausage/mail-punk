#ifndef IMAP_H
#define IMAP_H
#include "imaputils.hpp"
#include <libetpan/libetpan.h>
#include <string>
#include <functional>

namespace IMAP {

    class Session;
    
    class Message {
    public:
	Message(Session* session, uint32_t uid) : session(session), uid(uid) {}

	/* Gets the body of the message. */
	std::string getBody() const { return body; }

	/* Gets the unique identifier of the message. */
	uint32_t getUID() const { return uid; }

	/* Gets a field of the message. */
	std::string getField(std::string fieldname) const
	{
	    if (fieldname == "From") return from;
	    else if (fieldname == "Subject") return subject;
	    else return "";
	}

	/* Deletes this message from its mailbox. */
	void deleteFromMailbox();

	/* Fetches and sets the body and fields of a message. */
	void setMessage();

    private:
	std::string body = "";
	std::string from = ""; // This is a field
	std::string subject = ""; // This is a field	
	uint32_t const uid;
	Session* session;

	void setMessageData(mailimap_msg_att* msg_att); // Set body and fields
	void setFrom(clist* frm_list); // Set the "From" field
    };

    class Session {
    public:
	Session(std::function<void()> updateUI);
	
	~Session();

	/* Fetch and set all messages in the mailbox, terminated by a nullptr. */
	Message** getMessages();

	/* Connect to server. */
	void connect(std::string const& server, size_t port = 143);

	/* Login to server. */
	void login(std::string const& userid, std::string const& password);
	
	/* Select a mailbox. */
	void selectMailbox(std::string const& mailbox);

	/* Deletes every message in the mailbox, except one (specified by uid). */
	void deleteAllExcept(uint32_t exceptionUID);

	/* Refresh the user interface. */
	std::function<void()> refreshUI;

	/* Gets the pointer to the session imap. */
	mailimap* getImap() const { return imap; }
	
    private:
	Message** messages; // Dynamically assigned array of messages pointers
	uint32_t no_of_messages;
	mailimap* imap;

	uint32_t fetchUID(mailimap_msg_att* msg_att) const; // Fetch uid from msg_att
        uint32_t fetchNoOfMessages() const;
    };
    
}

#endif /* IMAP_H */
