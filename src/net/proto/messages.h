#ifndef NET_PROTO_MESSAGES_
#define NET_PROTO_MESSAGES_

#include <net/error.h>
#include <util/buffer.h>
#include <util/result.h>

#include <cstdint>
#include <string>
#include <vector>

namespace net {
namespace proto {

static constexpr std::size_t kOpcodeLength = 1;
static constexpr std::size_t kBodySizeLength = 4;
static constexpr std::size_t kMaxBodySize = (1ULL << 32) - 1;
static constexpr char kStringDelimiter[] = "\r\n";

/**
 * @brief Opcode for a message.
 *
 */
enum class Opcode : std::uint8_t {
    kOk = 0,
    kError = 1,
    kEstablishConnection = 2,
    kResponse = 3,
    kFileTransfer = 4,
    kTransmitData = 5,
    kFinished = 6,
    kEnquiry = 7,
    kRead = 8,
    kWrite = 9,
    kRequest = 100,
    kReply = 101,
    kShutdown = 200,
};

/**
 * @brief A generic message sent between a client and a server.
 *
 * A message is made up of a 1-byte opcode, 4-byte body length, and a body.
 *
 * Some messages are compound messages, which are used to initiate the transfer
 * of multiple messages at a time with connected meaning. For instance, a
 * `FileTransfer` message initiates several `TransmitData` messages that all
 * belong sequentially to the same file.
 *
 */
struct Message;

/**
 * @brief Message signaling OK to the client.
 *
 */
struct OkMessage {
    Message ToMessage() &&;
};

/**
 * @brief Message signaling an error on the server to the client.
 *
 */
struct ErrorMessage {
    std::string message;

    Message ToMessage() &&;
};

using node_id_t = std::uint8_t;
static constexpr node_id_t kNoId = std::numeric_limits<node_id_t>::max();

/**
 * @brief Message establishing a connection with a server.
 *
 */
struct EstablishConnectionMessage {
    node_id_t id;
    std::string message;

    Message ToMessage() &&;
};

/**
 * @brief Message that contains a response to some received message.
 *
 */
struct ResponseMessage {
    std::string message;

    Message ToMessage() &&;
};

/**
 * @brief Compound message initiating a file transfer. Must be followed by
 * several `TransmitData` messages and end with a `Finished` message.
 *
 */
struct FileTransferMessage {
    std::string file_name;

    Message ToMessage() &&;
};

namespace compound {

/**
 * @brief Message for transmitting generic data.
 *
 */
struct TransmitDataMessage {
    std::vector<std::uint8_t> data;

    Message ToMessage() &&;
};

/**
 * @brief Message indicating the completion of some operation.
 *
 */
struct FinishedMessage {
    Message ToMessage() &&;
};

}  // namespace compound

/**
 * @brief Message sent from client to server to get all file names available for
 * reading.
 *
 */
struct EnquiryMessage {
    Message ToMessage() &&;
};

/**
 * @brief Message sent from client to server to read a file from the server.
 *
 */
struct ReadMessage {
    std::string file_name;

    Message ToMessage() &&;
};

/**
 * @brief Message sent from client to server to append data to a file.
 *
 */
struct WriteMessage {
    std::string file_name;
    std::string line;

    Message ToMessage() &&;
};

namespace mutex {

/**
 * @brief Message containing a Lamport (logical) clock value.
 *
 */
struct LamportClock {
    std::size_t timestamp;
};

/**
 * @brief Message requesting access to the critical section.
 *
 */
struct RequestMessage : LamportClock {
    RequestMessage(std::size_t timestamp, std::string file_name);

    std::string file_name;

    Message ToMessage() &&;
};

/**
 * @brief Message replying to a `Request` message, allowing permission until
 * further notice.
 *
 */
struct ReplyMessage : LamportClock {
    ReplyMessage(std::size_t timestamp, std::string file_name);

    std::string file_name;

    Message ToMessage() &&;
};

}  // namespace mutex

struct Message {
    Opcode opcode;
    util::buffer body;

    Message() = default;
    Message(const Message &other) = default;
    Message &operator=(const Message &rhs) = default;
    Message(Message &&other) = default;
    Message &operator=(Message &&rhs) = default;

    util::result<OkMessage, Error> ToOk() &&;
    util::result<ErrorMessage, Error> ToError() &&;
    util::result<EstablishConnectionMessage, Error> ToEstablishConnection() &&;
    util::result<ResponseMessage, Error> ToResponse() &&;
    util::result<FileTransferMessage, Error> ToFileTransfer() &&;
    util::result<compound::TransmitDataMessage, Error> ToTransmitData() &&;
    util::result<compound::FinishedMessage, Error> ToFinished() &&;
    util::result<EnquiryMessage, Error> ToEnquiry() &&;
    util::result<ReadMessage, Error> ToRead() &&;
    util::result<WriteMessage, Error> ToWrite() &&;
    util::result<mutex::RequestMessage, Error> ToRequest() &&;
    util::result<mutex::ReplyMessage, Error> ToReply() &&;
};

}  // namespace proto
}  // namespace net

#endif  // NET_PROTO_MESSAGES_