#ifndef incl_Interfaces_ComminicationServiceInterface_h
#define incl_Interfaces_ComminicationServiceInterface_h

#include "CoreStdIncludes.h"
#include "ServiceInterface.h"

// TODO: rename all interface like. ContactInterface
// TODO: Consider use of weak pointers instead smartpointers

/*
 * ----------------------------------------------------------------------------------
 * CommunicationServiceInterface : Interfaces for to use CommunicationModule services
 * ----------------------------------------------------------------------------------
 */

namespace Communication
{
	// A single contact information of individual contact.
	// todo: should be renamed to ContactAddress ?
	// todo: separate interface and implementation
	// keys: 
	// * protocol
	// * account
	class ContactInfo
	{
	public: 
		virtual ~ContactInfo() {};
		virtual void SetProperty(std::string key, std::string value);
		virtual std::string GetProperty(std::string key);
		virtual std::vector<std::string> GetProperties();
	protected:
		std::map<std::string, std::string> properties_;
	};
	typedef boost::shared_ptr<ContactInfo> ContactInfoPtr;
	typedef std::vector<ContactInfoPtr> ContactInfoList;

	// presence status of one individual contact
	// todo: online_status {unknow, online, offline}
	//       online_message string
	//       something extra?
	//     * could these be just properties ?
	class PresenceStatus
	{
	public:
		virtual ~PresenceStatus() {};
		virtual void SetOnlineStatus(bool status) = 0;
		virtual bool GetOnlineStatus() = 0;
		virtual void SetOnlineMessage(std::string message) = 0;
		virtual std::string GetOnlineMessage() = 0;
	};
	typedef boost::shared_ptr<PresenceStatus> PresenceStatusPtr;

	// Individual contact in contact list 
	// todo: presence status should only exist with friend contact: -> IsFriend() ?
	class Contact
	{
	public:
		virtual ~Contact() {};
		virtual void SetName(std::string name) = 0;
		virtual std::string GetName() = 0;
		virtual PresenceStatusPtr GetPresenceStatus() = 0;
		virtual ContactInfoList GetContactInfos() = 0;
	};
	typedef boost::shared_ptr<Contact> ContactPtr;
	typedef std::vector<ContactPtr> ContactList;
	typedef boost::shared_ptr<ContactList> ContactListPtr;

	// A participiant of session
	// todo: add information about communication methods available
	class Participiant
	{
	public:
		virtual ~Participiant() {};
		virtual std::string GetName() = 0;
		virtual ContactPtr GetContactData() = 0;
	};
    typedef boost::shared_ptr<Participiant> ParticipiantPtr;
	typedef std::vector<ParticipiantPtr> ParticipientList;
	typedef boost::shared_ptr<ParticipientList> ParticipientListPtr;

	// Communication session (IM, voip, video, etc.)
	// 
	// Send Events:
	// * InvitationsResponse(bool)
	// * ParticipientLeft(Participient)
	// * ParticipientJoined(Participient)
	class Session
	{
	public:
		virtual ~Session() {};
		virtual void SendInvitation(ContactPtr c) = 0;
		virtual void Kick(Participiant *p) = 0; // TODO: -> ParticipiantPtr
		virtual void Close() = 0;
		virtual ParticipientListPtr GetParticipients() = 0;
	};
	typedef boost::shared_ptr<Session> SessionPtr;

	// todo: rename to MessageInterface or IMessage ???
	class Message
	{
	public:
		virtual ~Message() {};
		virtual std::string GetTimeStamp() = 0; // todo: change to proper timestamp type
		virtual ParticipiantPtr GetAuthor() = 0; 
		virtual std::string GetSessionId() = 0; // todo: Do we need this ??
//		virtual bool IsPrivate() = 0;
	private:
	};

	// chat message
	// todo: more components like attachment (link, file, request, etc.)
	class IMMessage
	{
	public:
		virtual ~IMMessage() {};
		virtual void SetText(std::string text) = 0;
		virtual std::string GetText() = 0;
	};
	typedef boost::shared_ptr<IMMessage> IMMessagePtr;
	typedef std::vector<IMMessagePtr> IMMessageList;
	typedef boost::shared_ptr<IMMessageList> IMMessageListPtr;

	// Text chat session
	// Send Events:
	// * MessageReceived
	class IMSession
	{
	public:
		virtual ~IMSession() {};
		virtual void SendIMMessage(IMMessagePtr m) = 0;
		virtual IMMessageListPtr GetMessageHistory() = 0;
	};
	typedef boost::shared_ptr<IMSession> IMSessionPtr;
	typedef std::vector<IMSessionPtr> IMSessionList;
	typedef std::vector<IMSessionList> IMSessionListPtr;
	
	// Login information for communication server connection
	// todo: rename to Account ?
	// todo: separate interface and implementation
	//
	// info: properties are communication manager and protocol specific:
	// eg.
	// * protocol
	// * account
	// * server
	// * server_port
	// * password
	class Credentials
	{
	public: 
		virtual ~Credentials() {};
		virtual void SetProperty(std::string key, std::string value);
		virtual std::string GetProperty(std::string key);
		virtual std::vector<std::string> GetProperties();
	protected:
		std::map<std::string, std::string> properties_;
	};
	typedef boost::shared_ptr<Credentials> CredentialsPtr;

	// todo: possibility to join chat rooms
	// Send Events:
	// * SessionInvitation
	// * PresenceUpdate
	// * ConnectionStateUpdate
	// * MessageReceived (maybe we will not allow messages without session? instead we'll always create one)
	class CommunicationServiceInterface : public Foundation::ServiceInterface
	{
	public:
		virtual ~CommunicationServiceInterface() {}
		virtual void OpenConnection(CredentialsPtr c) = 0;
		virtual void CloseConnection() = 0; 
		virtual IMSessionPtr CreateIMSession(ContactInfoPtr contact) = 0;
		virtual ContactListPtr GetContactList() = 0;
		virtual void PublishPresence(PresenceStatusPtr p) = 0;
		virtual IMMessagePtr CreateIMMessage(std::string text) = 0;
		virtual void SendFriendRequest(ContactInfoPtr contact_info) = 0;
	};
	typedef boost::shared_ptr<CommunicationServiceInterface> CommunicationServicePtr;

} // end of namespace: Communication

#endif // incl_Interfaces_ComminicationServiceInterface_h
