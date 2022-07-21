#include "connection.h"

namespace net {

Connection::Connection(Socket&& socket) : socket(std::move(socket)) {}

}  // namespace net