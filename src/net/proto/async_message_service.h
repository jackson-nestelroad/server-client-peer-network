#ifndef NET_PROTO_ASYNC_MESSAGE_SERVICE_
#define NET_PROTO_ASYNC_MESSAGE_SERVICE_

#include <net/components.h>
#include <net/proto/messages.h>
#include <net/socket.h>
#include <util/optional.h>
#include <util/result.h>

#include <atomic>
#include <functional>
#include <vector>

namespace net {
namespace proto {

/**
 * @brief Service for reading and writing messages to sockets asynchronously.
 *
 * This service is not synchronized. Only one message can be read at a time, and
 * only one message can be written at a time.
 *
 */
class AsyncMessageService {
   public:
    using recv_callback_t = std::function<void(util::result<Message, Error>)>;
    using send_callback_t = std::function<void(util::result<void, Error>)>;

    AsyncMessageService(Socket& socket, Components& components);

    /**
     * @brief Reads a message from the socket asynchronously.
     *
     * @param callback Called when a full message is received
     */
    void ReadMessage(const recv_callback_t& callback);

    /**
     * @brief Writes a message to the socket asynchronously.
     *
     * @param msg Message to send
     * @param callback Called when the full message is written
     */
    void WriteMessage(Message&& msg, const send_callback_t& callback);

    /**
     * @brief Gets the location on the local system of the last file transferred
     * using a received `FileTransfer` message.
     *
     * @return std::string
     */
    std::string GetLastTransferFileName() const;

    /**
     * @brief The service is in the process of reading a message.
     *
     * @return true
     * @return false
     */
    bool ReadingMessage() const;

    /**
     * @brief The service is in the process of writing a message.
     *
     * @return true
     * @return false
     */
    bool WritingMessage() const;

   private:
    /**
     * @brief Polls the socket until it is ready to be read.
     *
     * @param callback
     */
    void PollForRead(const recv_callback_t& callback);

    /**
     * @brief Receive bytes from the socket, if needed.
     *
     * @param callback
     */
    void ReceiveBytes(const recv_callback_t& callback);

    /**
     * @brief Process all bytes in the socket's buffer.
     *
     * @return util::result<void, Error>
     */
    util::result<void, Error> ProcessAllBytes();

    /**
     * @brief Process bytes in the socket's buffer up to one messagee.
     *
     * @param bytes_available
     * @return util::result<bool, Error>
     */
    util::result<bool, Error> ProcessBytes(std::size_t bytes_available);

    /**
     * @brief Function which contains actual logic for converting bytes in the
     * socket's buffer to message data structures.
     *
     * @param bytes_available Number of bytes available to be used
     * @return util::result<bool, Error> True if more progress could be made,
     * false if no more progress could be made
     */
    util::result<bool, Error> ProcessBytesIntoCurrentMessage(
        std::size_t bytes_available);

    bool OpcodeStartsACompoundMessage(Opcode opcode);
    bool InACompoundMessage();
    util::result<void, Error> HandleCompoundMessageHeader();
    util::result<void, Error> HandleCompoundMessageFrame();

    /**
     * @brief Generates a new transfer file name.
     *
     * @return std::string
     */
    std::string MakeTransferFileName();

    /**
     * @brief Creates and returns the message associated with the last message
     * read, assuming it has been completed.
     *
     * For compound messages, this is the message header.
     *
     * @return Message
     */
    Message GetReturnedMessage();

    void ResetCurrentMessage();
    void ResetCompoundMessage();

    /**
     * @brief Checks if the reader is finished reading the current message.
     *
     * @return true
     * @return false
     */
    bool FinishedReadingCurrentMessage();

    /**
     * @brief Checks if the reader is finished reading the current compound
     * message.
     *
     * Returns false if the reader is not reading a compound message.
     *
     * @return true
     * @return false
     */
    bool FinishedReadingCompoundMessage();

    /**
     * @brief Checks if the reader should quit reading and return a message to
     * the user.
     *
     * @return true
     * @return false
     */
    bool FinishedReading();

    /**
     * @brief Fills the socket's output buffer with the entire message, which
     * may be made up of multiple messages if the given message is compound.
     *
     * @param msg
     * @return util::result<void, Error>
     */
    util::result<void, Error> FillOutputBuffer(Message&& msg);

    /**
     * @brief Puts a single message in the output buffer, regardless of its
     * type.
     *
     * @param msg
     * @return util::result<void, Error>
     */
    util::result<void, Error> PutMessageInOutputBuffer(Message&& msg);

    /**
     * @brief Polls the socket until it is ready to be written to.
     *
     * @param callback
     */
    void PollForWrite(const send_callback_t& callback);

    /**
     * @brief Sends bytes over the socket until all bytes are written.
     *
     * @param callback
     */
    void SendBytes(const send_callback_t& callback);

    /**
     * @brief Checks if the writer is finished writing the entire output buffer.
     *
     * @return true
     * @return false
     */
    bool FinishedSending();

    Socket& socket_;
    Components& components_;

    bool reading_;
    bool writing_;

    util::optional<Message> compound_message_parent_;
    std::string transfer_file_name_;
    bool finished_compound_;

    util::optional<Opcode> opcode_;
    util::optional<std::size_t> expected_;
    util::buffer body_;

    std::size_t attempting_to_send_;

    static std::atomic<std::size_t> file_transfer_count_;
};

}  // namespace proto
}  // namespace net

#endif  // NET_PROTO_ASYNC_MESSAGE_SERVICE_