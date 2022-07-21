#ifndef NET_SERVER_IMPL_PROJECT2_SERVICE_
#define NET_SERVER_IMPL_PROJECT2_SERVICE_

#include <net/proto/async_message_service.h>
#include <net/server/base_service.h>
#include <util/state_machine.h>

namespace net {
namespace server {
namespace impl {

class Project2Service;

namespace states {

DEFINE_ASYNC_STATE(Project2Service, AwaitMessage);
DEFINE_ASYNC_STATE(Project2Service, HandleEnquiry);
DEFINE_ASYNC_STATE(Project2Service, HandleRead);
DEFINE_ASYNC_STATE(Project2Service, HandleWrite);
DEFINE_ASYNC_STATE(Project2Service, HandleInvalidOpcode);
DEFINE_STOP_STATE(Project2Service, Stop);

}  // namespace states

/**
 * @brief Implementation for handling a client according to the specifications
 * of Project 2.
 *
 */
class Project2Service : public BaseService,
                        protected util::state_machine<Project2Service> {
   public:
    Project2Service(ServerComponents& components, Connection& client,
                    BaseConnectionHandler& owner);

    void Stop() override;

   private:
    void Run();

    proto::AsyncMessageService message_service_;
    proto::Message last_received_;

    friend struct states::AwaitMessage;
    friend struct states::HandleEnquiry;
    friend struct states::HandleRead;
    friend struct states::HandleWrite;
    friend struct states::HandleInvalidOpcode;
    friend struct states::Stop;
};

}  // namespace impl
}  // namespace server
}  // namespace net

#endif  // NET_SERVER_IMPL_PROJECT2_SERVICE_