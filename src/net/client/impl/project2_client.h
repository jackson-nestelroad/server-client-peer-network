#ifndef NET_CLIENT_IMPL_PROJECT2_CLIENT_
#define NET_CLIENT_IMPL_PROJECT2_CLIENT_

#include <net/client/client.h>
#include <net/proto/async_message_service.h>
#include <util/state_machine.h>

#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace net {
namespace client {
namespace impl {

class Project2Client;

namespace states {

DEFINE_ASYNC_STATE(Project2Client, ConnectToServers);
DEFINE_ASYNC_STATE(Project2Client, SendEnquiry);
DEFINE_ASYNC_STATE(Project2Client, ReceiveEnquiryResponse);
DEFINE_STATE(Project2Client, Wait);
DEFINE_ASYNC_STATE(Project2Client, SendRead);
DEFINE_ASYNC_STATE(Project2Client, ReceiveReadResponse);
DEFINE_ASYNC_STATE(Project2Client, SendWrite);
DEFINE_ASYNC_STATE(Project2Client, ReceiveWriteResponse);
DEFINE_STOP_STATE(Project2Client, Stop);

}  // namespace states

/**
 * @brief Implementation for a client according to the specifications of
 * Project 2.
 *
 */
class Project2Client : public Client,
                       protected util::state_machine<Project2Client> {
   public:
    Project2Client(Components& components);

   private:
    /**
     * @brief A single server the client is connected to and can send operations
     * to.
     *
     */
    struct Server {
        Server(Connection&& moved_connection, Components& components);

        std::shared_ptr<Connection> connection;
        proto::AsyncMessageService message_service;
        bool operation_sent;
        bool performed_write;
    };

    void Run() override;

    void StopStateMachine();

    util::result<void, Error> ChangeServer();

    util::result<void, Error> ChangeFile();

    std::mutex mutex_;
    std::size_t num_servers_;
    std::vector<Server> servers_;
    std::vector<std::string> file_names_;
    std::unordered_set<net::Connection::id_t> servers_that_finished_my_write_;
    Server* current_server_;
    const std::string* current_file_name_;
    util::optional<typename mutex::DistributedMutualExclusionService::
                       mutex_operation_done_t>
        finished_critical_section_callback_;

   public:
    friend struct states::ConnectToServers;
    friend struct states::SendEnquiry;
    friend struct states::ReceiveEnquiryResponse;
    friend struct states::Wait;
    friend struct states::SendRead;
    friend struct states::ReceiveReadResponse;
    friend struct states::SendWrite;
    friend struct states::ReceiveWriteResponse;
};

}  // namespace impl
}  // namespace client
}  // namespace net

#endif  // NET_CLIENT_IMPL_PROJECT2_CLIENT_