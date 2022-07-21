#include "base_service.h"

#include <net/server/base_connection_handler.h>
#include <net/server/server_components.h>

namespace net {
namespace server {

BaseService::BaseService(ServerComponents& components, Connection& client,
                         BaseConnectionHandler& owner)
    : components_(components), client_(client), owner_(owner) {}

BaseService::~BaseService() {}

util::result<void, Error> BaseService::Start() {
    RETURN_IF_ERROR(Initialize());
    Run();
    return util::ok;
}

void BaseService::Stop() { owner_.Stop(); }

void BaseService::ScheduleRun() {
    components_.common.thread_pool.Schedule([this]() { Run(); });
}

util::result<void, Error> BaseService::Initialize() { return util::ok; }

void BaseService::Run() {}

}  // namespace server
}  // namespace net