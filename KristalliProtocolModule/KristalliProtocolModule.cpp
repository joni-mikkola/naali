// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"

#include "DebugOperatorNew.h"

#include "KristalliProtocolModule.h"
#include "KristalliProtocolModuleEvents.h"
#include "Profiler.h"
#include "EventManager.h"
#include "CoreStringUtils.h"
#include "ConsoleCommandUtils.h"
#include "UiAPI.h"
#include "UiMainWindow.h"
#include "ConsoleAPI.h"

#include <kNet.h>
#include <kNet/qt/NetworkDialog.h>

#include <algorithm>

using namespace kNet;

namespace KristalliProtocol
{

namespace
{
    const std::string moduleName("KristalliProtocol");

/*
    const struct
    {
        SocketTransportLayer transport;
        int portNumber;
    } destinationPorts[] = 
    {
        { SocketOverUDP, 2345 }, // The default Kristalli over UDP port.

        { SocketOverTCP, 2345 }, // The default Kristalli over TCP port.
        { SocketOverUDP, 123 }, // Network Time Protocol.

        { SocketOverTCP, 80 }, // HTTP.
        { SocketOverTCP, 443 }, // HTTPS.
        { SocketOverTCP, 20 }, // FTP Data.
        { SocketOverTCP, 21 }, // FTP Control.
        { SocketOverTCP, 22 }, // SSH.
        { SocketOverTCP, 23 }, // TELNET.
        { SocketOverUDP, 25 }, // SMTP. (Microsoft)
        { SocketOverTCP, 25 }, // SMTP.
        { SocketOverTCP, 110 }, // POP3 Server listen port.
        { SocketOverTCP, 995 }, // POP3 over SSL.
        { SocketOverTCP, 109 }, // POP2.
        { SocketOverTCP, 6667 }, // IRC.

        // For more info on the following windows ports, see: http://support.microsoft.com/kb/832017

        { SocketOverTCP, 135 }, // Windows RPC.
        { SocketOverUDP, 137 }, // Windows Cluster Administrator. / NetBIOS Name Resolution.
        { SocketOverUDP, 138 }, // Windows NetBIOS Datagram Service.
        { SocketOverTCP, 139 }, // Windows NetBIOS Session Service.

        { SocketOverUDP, 389 }, // Windows LDAP Server.
        { SocketOverTCP, 389 }, // Windows LDAP Server.

        { SocketOverTCP, 445 }, // Windows SMB.

        { SocketOverTCP, 5722 }, // Windows RPC.

        { SocketOverTCP, 993 }, // IMAP over SSL.

//        { SocketOverTCP, 1433 }, // SQL over TCP.
//        { SocketOverUDP, 1434 }, // SQL over UDP.

        { SocketOverUDP, 53 }, // DNS.
        { SocketOverTCP, 53 }, // DNS. Microsoft states it uses TCP 53 for DNS as well.
        { SocketOverUDP, 161 }, // SNMP agent port.
        { SocketOverUDP, 162 }, // SNMP manager port.
        { SocketOverUDP, 520 }, // RIP.
        { SocketOverUDP, 67 }, // DHCP client->server.
        { SocketOverUDP, 68 }, // DHCP server->client.
    };

    /// The number of different port choices to try from the list.
    const int cNumPortChoices = sizeof(destinationPorts) / sizeof(destinationPorts[0]);
*/
}

static const int cInitialAttempts = 1;
static const int cReconnectAttempts = 5;

KristalliProtocolModule::KristalliProtocolModule()
:IModule(NameStatic())
, serverConnection(0)
, server(0)
, reconnectAttempts(0)
, cleanup(false)
{
    serverIp_list_.clear();
    serverPort_list_.clear();
    serverTransport_list_.clear();
    reconnectAttempts_list_.clear();
    reconnectTimer_list_.clear();
    serverConnection_map_.clear();
    cleanupList.clear();
}

KristalliProtocolModule::~KristalliProtocolModule()
{
    Disconnect();
}

void KristalliProtocolModule::Load()
{
}

void KristalliProtocolModule::Unload()
{
    Disconnect();
}

void KristalliProtocolModule::PreInitialize()
{
    kNet::SetLogChannels(kNet::LogInfo | kNet::LogError | kNet::LogUser); // Enable all log channels.
}

void KristalliProtocolModule::Initialize()
{
    EventManagerPtr event_manager = framework_->GetEventManager();
    networkEventCategory = event_manager->RegisterEventCategory("Kristalli");
    event_manager->RegisterEvent(networkEventCategory, Events::NETMESSAGE_IN, "NetMessageIn");

    defaultTransport = kNet::SocketOverTCP;
    const boost::program_options::variables_map &options = framework_->ProgramOptions();
    if (options.count("protocol") > 0)
        if (QString(options["protocol"].as<std::string>().c_str()).trimmed().toLower() == "udp")
            defaultTransport = kNet::SocketOverUDP;
}

void KristalliProtocolModule::PostInitialize()
{
#ifdef KNET_USE_QT
    framework_->Console()->RegisterCommand(CreateConsoleCommand(
            "kNet", "Shows the kNet statistics window.", 
            ConsoleBind(this, &KristalliProtocolModule::OpenKNetLogWindow)));
#endif
}

void KristalliProtocolModule::Uninitialize()
{
    Disconnect();
}

#ifdef KNET_USE_QT
ConsoleCommandResult KristalliProtocolModule::OpenKNetLogWindow(const StringVector &)
{
    NetworkDialog *networkDialog = new NetworkDialog(0, &network);
    networkDialog->setAttribute(Qt::WA_DeleteOnClose);
    networkDialog->show();

    return ConsoleResultSuccess();
}
#endif

void KristalliProtocolModule::Update(f64 frametime)
{
    // Update method for multiconnection.
    if (!serverConnection_map_.isEmpty())
        connectionArrayUpdate();
    

    // Pulls all new inbound network messages and calls the message handler we've registered
    // for each of them.
    if (serverConnection)
        serverConnection->Process();


    // Note: Calling the above serverConnection->Process() may set serverConnection to null if the connection gets disconnected.
    // Therefore, in the code below, we cannot assume serverConnection is non-null, and must check it again.

    // Our client->server connection is never kept half-open.
    // That is, at the moment the server write-closes the connection, we also write-close the connection.
    // Check here if the server has write-closed, and also write-close our end if so.
    if (serverConnection && !serverConnection->IsReadOpen() && serverConnection->IsWriteOpen())
        serverConnection->Disconnect(0);

    
    // Process server incoming connections & messages if server up
    if (server)
    {
        server->Process();

        // In Tundra, we *never* keep half-open server->client connections alive. 
        // (the usual case would be to wait for a file transfer to complete, but Tundra messaging mechanism doesn't use that).
        // So, bidirectionally close all half-open connections.
        NetworkServer::ConnectionMap connections = server->GetConnections();
        for(NetworkServer::ConnectionMap::iterator iter = connections.begin(); iter != connections.end(); ++iter)
            if (!iter->second->IsReadOpen() && iter->second->IsWriteOpen())
                iter->second->Disconnect(0);
    }
    
    if ((!serverConnection || serverConnection->GetConnectionState() == ConnectionClosed ||
         serverConnection->GetConnectionState() == ConnectionPending) && serverIp.length() != 0)
    {
        const int cReconnectTimeout = 5 * 1000.f;
        if (reconnectTimer.Test())
        {
            if (reconnectAttempts)
            {
                PerformConnection();
                --reconnectAttempts;
            }
            else
            {
                LogInfo("Failed to connect to " + serverIp + ":" + ToString(serverPort));
                // If connection fails we just ignore it. Client has no use for information if initial connection failed.
                //framework_->GetEventManager()->SendEvent(networkEventCategory, Events::CONNECTION_FAILED, 0);
                reconnectTimer.Stop();
                serverIp = "";
            }
        }
        else if (!reconnectTimer.Enabled())
            reconnectTimer.StartMSecs(cReconnectTimeout);
    }

    // If connection was made, enable a larger number of reconnection attempts in case it gets lost
    if (serverConnection && serverConnection->GetConnectionState() == ConnectionOK)
    {
        // save succesfully established connection to messageconnection array with all connection properties.
        for (unsigned short connection = 0;; connection++)
        {
            if (!serverConnection_map_.contains(connection))
            {
                // Associative QMap stores all items sorted by Key. If array has one item terminated,
                // we fill it with next successful connection. Same is done in client.
                serverConnection_map_.insert(connection, serverConnection);
                serverIp_list_.insert(connection,serverIp);
                serverPort_list_.insert(connection,serverPort);
                serverTransport_list_.insert(connection, serverTransport);
                reconnectAttempts_list_.insert(connection, cReconnectAttempts);
                reconnectTimer.Reset();
                reconnectTimer_list_.insert(connection,reconnectTimer);

                serverConnection = 0;
                serverIp = "";
                serverPort = 0;

                break;
            }
        }
    }


    RESETPROFILER;
}

const std::string &KristalliProtocolModule::NameStatic()
{
    return moduleName;
}

void KristalliProtocolModule::Connect(const char *ip, unsigned short port, SocketTransportLayer transport)
{
    if (Connected() && serverConnection && serverConnection->RemoteEndPoint().IPToString() != serverIp)
        Disconnect();
    
    serverIp = ip;
    serverPort = port;
    serverTransport = transport;
    reconnectAttempts = cInitialAttempts; // Initial attempts when establishing connection
    
    if (!Connected())
        PerformConnection(); // Start performing a connection attempt to the desired address/port/transport
}

void KristalliProtocolModule::PerformConnection()
{
    if (Connected() && serverConnection)
    {
        serverConnection->Close();
//        network.CloseMessageConnection(serverConnection);
        serverConnection = 0;
    }

    // Connect to the server.
    serverConnection = network.Connect(serverIp.c_str(), serverPort, serverTransport, this);
    if (!serverConnection)
    {
        LogError("Unable to connect to " + serverIp + ":" + ToString(serverPort));
        return;
    }

    // For TCP mode sockets, set the TCP_NODELAY option to improve latency for the messages we send.
    if (serverConnection->GetSocket() && serverConnection->GetSocket()->TransportLayer() == kNet::SocketOverTCP)
        serverConnection->GetSocket()->SetNaglesAlgorithmEnabled(false);
}

void KristalliProtocolModule::Disconnect()
{
    // Clear the remembered destination server ip address so that the automatic connection timer will not try to reconnect.
    serverIp = "";
    reconnectTimer.Stop();
    
    if (serverConnection)
    {
        serverConnection->Disconnect();
//        network.CloseMessageConnection(serverConnection);
        ///\todo Wait? This closes the connection.
        serverConnection = 0;
    }


}

bool KristalliProtocolModule::StartServer(unsigned short port, SocketTransportLayer transport)
{
    StopServer();
    
    const bool allowAddressReuse = true;
    server = network.StartServer(port, transport, this, allowAddressReuse);
    if (!server)
    {
        LogError("Failed to start server on port " + ToString((int)port));
        throw Exception(("Failed to start server on port " + ToString((int)port) + ". Please make sure that the port is free and not used by another application. The program will now abort.").c_str());
    }
    
    LogInfo("Started server on port " + ToString((int)port));
    return true;
}

void KristalliProtocolModule::StopServer()
{
    if (server)
    {
        network.StopServer();
        connections.clear();
        LogInfo("Stopped server");
        server = 0;
    }
}

void KristalliProtocolModule::NewConnectionEstablished(kNet::MessageConnection *source)
{
    assert(source);
    if (!source)
        return;

    source->RegisterInboundMessageHandler(this);
    ///\todo Regression. Re-enable. -jj.
//    source->SetDatagramInFlowRatePerSecond(200);
    
    UserConnection* connection = new UserConnection();
    connection->userID = AllocateNewConnectionID();
    connection->connection = source;
    connections.push_back(connection);

    // For TCP mode sockets, set the TCP_NODELAY option to improve latency for the messages we send.
    if (source->GetSocket() && source->GetSocket()->TransportLayer() == kNet::SocketOverTCP)
        source->GetSocket()->SetNaglesAlgorithmEnabled(false);

    LogInfo("User connected from " + source->RemoteEndPoint().ToString() + ", connection ID " + ToString((int)connection->userID));
    
    Events::KristalliUserConnected msg(connection);
    framework_->GetEventManager()->SendEvent(networkEventCategory, Events::USER_CONNECTED, &msg);
}

void KristalliProtocolModule::ClientDisconnected(MessageConnection *source)
{
    // Delete from connection list if it was a known user
    for(UserConnectionList::iterator iter = connections.begin(); iter != connections.end(); ++iter)
        if ((*iter)->connection == source)
        {
            Events::KristalliUserDisconnected msg((*iter));
            framework_->GetEventManager()->SendEvent(networkEventCategory, Events::USER_DISCONNECTED, &msg);
            
            LogInfo("User disconnected, connection ID " + ToString((int)(*iter)->userID));
            delete(*iter);
            connections.erase(iter);
            return;
        }

    LogInfo("Unknown user disconnected");
}

void KristalliProtocolModule::HandleMessage(MessageConnection *source, message_id_t id, const char *data, size_t numBytes)
{
    assert(source);
    assert(data);

    try
    {
        Events::KristalliNetMessageIn msg(source, id, data, numBytes);

        framework_->GetEventManager()->SendEvent(networkEventCategory, Events::NETMESSAGE_IN, &msg);
    } catch(std::exception &e)
    {
        LogError("KristalliProtocolModule: Exception \"" + std::string(e.what()) + "\" thrown when handling network message id " +
            ToString(id) + " size " + ToString((int)numBytes) + " from client " + source->ToString());
    }
}

bool KristalliProtocolModule::HandleEvent(event_category_id_t category_id, event_id_t event_id, IEventData* data)
{
    return false;
}

u8 KristalliProtocolModule::AllocateNewConnectionID() const
{
    u8 newID = 1;
    for(UserConnectionList::const_iterator iter = connections.begin(); iter != connections.end(); ++iter)
        newID = std::max((int)newID, (int)((*iter)->userID+1));
    
    return newID;
}

UserConnection* KristalliProtocolModule::GetUserConnection(MessageConnection* source)
{
    for (UserConnectionList::iterator iter = connections.begin(); iter != connections.end(); ++iter)
        if ((*iter)->connection == source)
            return (*iter);

    return 0;
}

UserConnection* KristalliProtocolModule::GetUserConnection(u8 id)
{
    for (UserConnectionList::iterator iter = connections.begin(); iter != connections.end(); ++iter)
        if ((*iter)->userID == id)
            return (*iter);

    return 0;
}

} // ~KristalliProtocolModule namespace

extern "C" void POCO_LIBRARY_API SetProfiler(Foundation::Profiler *profiler);
void SetProfiler(Foundation::Profiler *profiler)
{
    Foundation::ProfilerSection::SetProfiler(profiler);
}

using namespace KristalliProtocol;

POCO_BEGIN_MANIFEST(IModule)
   POCO_EXPORT_CLASS(KristalliProtocolModule)
POCO_END_MANIFEST

void KristalliProtocolModule::connectionArrayUpdate()
{
    unsigned short connection = 0;

    // Iterator which is used to process messageConnection array items
    QMutableMapIterator<unsigned short, Ptr(kNet::MessageConnection)> iter(serverConnection_map_);

    // Iterator which points to last item in messageConnection array map.
    QMutableMapIterator<unsigned short, Ptr(kNet::MessageConnection)> iterLast(serverConnection_map_);
    iterLast.toBack();
    iterLast.previous();

    // Pulls all new inbound network messages and calls the message handler we've registered
    // for each of them.
    while (iter.hasNext())
    {
        iter.next();

        // If we are processing last serverConnection item we enable cleanup flag
        if (iter.value() == iterLast.value())
            cleanup = true;

        if (iter.value())
            iter.value()->Process();

        // Note: Calling the above tempConnection->Process() may set tempConnection to null if the connection gets disconnected.
        // Therefore, in the code below, we cannot assume tempConnection is non-null, and must check it again.

        // Our client->server connection is never kept half-open.
        // That is, at the moment the server write-closes the connection, we also write-close the connection.
        // Check here if the server has write-closed, and also write-close our end if so.
        if (iter.value() && !iter.value()->IsReadOpen() && iter.value()->IsWriteOpen())
            iter.value()->Disconnect(0);


        if ((!iter.value() || iter.value()->GetConnectionState() == ConnectionClosed ||
             iter.value()->GetConnectionState() == ConnectionPending) && serverIp_list_[connection].length() != 0)
        {
            const int cReconnectTimeout = 5 * 1000.f;
            if (reconnectTimer_list_[connection].Test())
            {
                if (reconnectAttempts_list_[connection])
                {
                    PerformReconnection(iter, connection);
                    --reconnectAttempts_list_[connection];
                }
                else
                {
                    LogInfo("Failed to connect to " + serverIp_list_[connection] + ":" + ToString(serverPort_list_[connection]));
                    // Message which declares what connection we are removing
                    Events::KristalliConnectionFailed msg(connection);
                    // This sends an event to client::logout() which calls KristalliProtocolModule->Disconnect()
                    // which sets serverConnection to zero.
                    LogInfo("Sending message: CONNETION_FAILED");
                    framework_->GetEventManager()->SendEvent(networkEventCategory, Events::CONNECTION_FAILED, &msg);
                    // When we have iterated through whole connection array we remove all marked connection properties from list/map
                    cleanupList.append(connection);
                }
            }
            else if (!reconnectTimer_list_[connection].Enabled())
                reconnectTimer_list_[connection].StartMSecs(cReconnectTimeout);
        }

        // If connection was made, enable a larger number of reconnection attempts in case it gets lost
        if (iter.value() && iter.value()->GetConnectionState() == ConnectionOK)
            reconnectAttempts_list_[connection] = cReconnectAttempts;
        // This variable keeps track of which properties we handle currently from QLists (IP,PORT, etc)
        connection++;

        // If current messageconnection was last connection to be processed from QMap we clean up the QMap
        if (cleanup)
            removeConnectionProperties();

    }
}

void KristalliProtocolModule::removeConnectionProperties()
{
    for (int counter = 0; counter < cleanupList.length(); counter++)
    {
        serverIp_list_.remove(cleanupList[counter]);
        serverPort_list_.remove(cleanupList[counter]);
        serverTransport_list_.remove(cleanupList[counter]);
        reconnectAttempts_list_.remove(cleanupList[counter]);
        reconnectTimer_list_.remove(cleanupList[counter]);
        serverConnection_map_.remove(cleanupList[counter]);
    }
    cleanup = false;
    cleanupList.clear();
}

void KristalliProtocolModule::PerformReconnection(QMutableMapIterator<unsigned short, Ptr(kNet::MessageConnection)> &iterator, unsigned short connection)
{
    if (iterator.value())
    {
        iterator.value()->Close();
//        network.CloseMessageConnection(serverConnection);
        iterator.value() = 0;
    }

    // Connect to the server.
    iterator.value() = network.Connect(serverIp_list_[connection].c_str(), serverPort_list_[connection], serverTransport_list_[connection], this);
    if (!iterator.value())
    {
        LogError("Unable to connect to " + serverIp_list_[connection] + ":" + ToString(serverPort_list_[connection]));
        return;
    }

    // For TCP mode sockets, set the TCP_NODELAY option to improve latency for the messages we send.
    if (iterator.value()->GetSocket() && iterator.value()->GetSocket()->TransportLayer() == kNet::SocketOverTCP)
        iterator.value()->GetSocket()->SetNaglesAlgorithmEnabled(false);
}

void KristalliProtocolModule::Disconnect(bool fail, unsigned short con)
{
    if (serverConnection_map_.isEmpty())
        return;

    // Clear the remembered destination server ip address so that the automatic connection timer will not try to reconnect.
    serverIp_list_[con] = "";
    reconnectTimer_list_[con].Stop();

    for (QMutableMapIterator<unsigned short, Ptr(kNet::MessageConnection)> iterator(serverConnection_map_); iterator.hasNext();)
    {
        iterator.next();
        if (iterator.key() == con)
        {
           iterator.value()->Disconnect();
           iterator.value() = 0;
        }
    }
    if (!fail)
    {
        cleanupList.append(con);
        removeConnectionProperties();
    }
}
