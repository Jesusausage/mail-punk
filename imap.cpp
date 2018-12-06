#include "imap.hpp"


using namespace IMAP;
using namespace std;


Session::Session(function<void()> updateUI) :
    refreshUI(updateUI), messages(nullptr), no_of_messages(0)
{
    imap = mailimap_new(0, nullptr);    
}


Message** Session::getMessages()
{
    no_of_messages = fetchNoOfMessages();
    messages = new Message*[no_of_messages+1];
    
    auto set = mailimap_set_new_interval(1, 0);
    auto fetch_att = mailimap_fetch_att_new_uid();
    auto fetch_type = mailimap_fetch_type_new_fetch_att(fetch_att);
    clist* fetch_result;
    check_error(mailimap_fetch(imap, set, fetch_type, &fetch_result),
		"Could not fetch UIDs.\n\nError code:");

    int i = 0;
    clistiter* iter;
    for (iter = clist_begin(fetch_result); iter != nullptr; iter = clist_next(iter)) {
	auto msg_att = (mailimap_msg_att*)clist_content(iter);
	auto uid = fetchUID(msg_att);
	if (uid) {
	    messages[i] = new Message(this, uid);
	    messages[i]->setMessage();
	    i++;
	}
    }
    messages[i] = nullptr;

    mailimap_fetch_list_free(fetch_result);
    mailimap_fetch_type_free(fetch_type);
    mailimap_set_free(set);
    
    return messages;
}


void Message::setMessage()
{
    auto set = mailimap_set_new_single(uid); // Unary set, as uid is unique
    auto fetch_type = mailimap_fetch_type_new_fetch_att_list_empty();
    
    auto env_att = mailimap_fetch_att_new_envelope(); // Envelope contains the fields
    mailimap_fetch_type_new_fetch_att_list_add(fetch_type, env_att);
    
    auto section = mailimap_section_new(nullptr);
    auto body_att = mailimap_fetch_att_new_body_section(section);
    mailimap_fetch_type_new_fetch_att_list_add(fetch_type, body_att);
    
    clist* fetch_result;
    check_error(mailimap_uid_fetch(session->getImap(), set, fetch_type, &fetch_result),
		"Could not fetch message.\n\nError code:");

    clistiter* ptr = clist_begin(fetch_result); // Only one fetch result in list
    auto msg_att = (mailimap_msg_att*)clist_content(ptr);
    setMessageData(msg_att);

    mailimap_fetch_list_free(fetch_result);
    mailimap_fetch_type_free(fetch_type);
    mailimap_set_free(set);
    
}


void Message::setMessageData(mailimap_msg_att* msg_att)
{
    clistiter* iter;
    for (iter = clist_begin(msg_att->att_list); iter != nullptr; iter = clist_next(iter)) {
	auto item = (mailimap_msg_att_item*)clist_content(iter);

	if (item->att_type != MAILIMAP_MSG_ATT_ITEM_STATIC) // Contained in static att type
	    continue;

	if (item->att_data.att_static->att_type == MAILIMAP_MSG_ATT_ENVELOPE) { // For fields
	    if (item->att_data.att_static->att_data.att_env->env_subject)
		subject = item->att_data.att_static->att_data.att_env->env_subject;
	    
	    if (item->att_data.att_static->att_data.att_env->env_from->frm_list)
		setFrom(item->att_data.att_static->att_data.att_env->env_from->frm_list);
	}
	
	if (item->att_data.att_static->att_type == MAILIMAP_MSG_ATT_BODY_SECTION) { // For body
	    if (item->att_data.att_static->att_data.att_body_section->sec_body_part)
		body = item->att_data.att_static->att_data.att_body_section->sec_body_part;
	}
    }
}


void Message::setFrom(clist* frm_list)
{
    clistiter* iter; // List because possibly multiple senders
    for (iter = clist_begin(frm_list); iter != nullptr; iter = clist_next(iter)) {
	auto address = (mailimap_address*)clist_content(iter);
	
	if (address->ad_personal_name) {
	    from += address->ad_personal_name;
	    from += ", ";
	}
	
	if (address->ad_mailbox_name && address->ad_host_name) {
	    from += "<";
	    from += address->ad_mailbox_name;
	    from += "@";
	    from += address->ad_host_name;
	    from += ">; ";
	}
    }
}


uint32_t Session::fetchNoOfMessages() const
{
    auto status_att_list = mailimap_status_att_list_new_empty();
    mailimap_status_att_list_add(status_att_list, MAILIMAP_STATUS_ATT_MESSAGES);
    char mb[] = "INBOX";
    mailimap_mailbox_data_status* result;
    check_error(mailimap_status(imap, mb, status_att_list, &result),
     		"Could not retrieve no. of messages.\n\nError code:");

    auto status_info = (mailimap_status_info*)clist_content(clist_begin(result->st_info_list));
    auto value = status_info->st_value;

    mailimap_mailbox_data_status_free(result);
    mailimap_status_att_list_free(status_att_list);
    
    return value;
}
    

uint32_t Session::fetchUID(mailimap_msg_att* msg_att) const
{
    clistiter* iter;
    for (iter = clist_begin(msg_att->att_list); iter != nullptr; iter = clist_next(iter)) {
	auto item = (mailimap_msg_att_item*)clist_content(iter);
	
	if (item->att_type == MAILIMAP_MSG_ATT_ITEM_STATIC) { // Contained in static 
	    if (item->att_data.att_static->att_type == MAILIMAP_MSG_ATT_UID)
		return item->att_data.att_static->att_data.att_uid;
	}	
    }
    
    return 0;
}
    

void Session::connect(string const& server, size_t port)
{
    string error_msg = "Could not connect to '";
    error_msg += server;
    error_msg += "'.\n\nError code:";
    
    check_error(mailimap_socket_connect(imap, server.c_str(), port),
	        error_msg);
}


void Session::login(string const& userid, string const& password)
{
    string error_msg = "Could not log in '";
    error_msg += userid;
    error_msg += "'.\n\nError code:";
    
    check_error(mailimap_login(imap, userid.c_str(), password.c_str()),
		error_msg);
}


void Session::selectMailbox(string const& mailbox)
{
    string error_msg = "Could not select mailbox '";
    error_msg += mailbox;
    error_msg += "'.\n\nError code:";
    
    check_error(mailimap_select(imap, mailbox.c_str()),
		error_msg);
}


void Session::deleteAllExcept(uint32_t exceptionUID)
{
    for (int i = 0; i < no_of_messages; i++) {
	if (messages[i]->getUID() != exceptionUID)
	    delete messages[i];
    }
    delete [] messages;
}


Session::~Session()
{
    deleteAllExcept(-1); // Deletes all, since no uid is -1
    mailimap_logout(imap);
    mailimap_free(imap);
}


void Message::deleteFromMailbox()
{
    auto set = mailimap_set_new_single(uid);
    auto flag_list = mailimap_flag_list_new_empty();
    auto deleted = mailimap_flag_new_deleted();
    mailimap_flag_list_add(flag_list, deleted);
    auto store = mailimap_store_att_flags_new_set_flags(flag_list);
    
    check_error(mailimap_uid_store(session->getImap(), set, store),
		"Could not set delete flag.\n\nError code:");

    check_error(mailimap_expunge(session->getImap()),
		"Could not delete message.\n\nError code:");

    mailimap_store_att_flags_free(store);
    mailimap_set_free(set);

    session->deleteAllExcept(uid); // Cannot deallocate memory while still operating inside it!
    session->refreshUI();
    delete this; // Delete current object only after UI is refreshed
}
