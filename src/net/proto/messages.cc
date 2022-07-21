#include "messages.h"

#include <util/buffer.h>
#include <util/bytes.h>

#define ASSERT_OPCODE(expected)                            \
    if (opcode != expected) {                              \
        return Error::Create("Bad opcode for conversion"); \
    }

namespace net {
namespace proto {

util::result<OkMessage, Error> Message::ToOk() && {
    ASSERT_OPCODE(Opcode::kOk);
    return OkMessage{};
}

util::result<ErrorMessage, Error> Message::ToError() && {
    ASSERT_OPCODE(Opcode::kError)
    return ErrorMessage{body.to_string()};
}

util::result<EstablishConnectionMessage, Error>
Message::ToEstablishConnection() && {
    ASSERT_OPCODE(Opcode::kEstablishConnection);
    std::uint8_t id = util::bytes::extract<1>(body);
    return EstablishConnectionMessage{id, body.to_string()};
}

util::result<ResponseMessage, Error> Message::ToResponse() && {
    ASSERT_OPCODE(Opcode::kResponse);
    return ResponseMessage{body.to_string()};
}

util::result<FileTransferMessage, Error> Message::ToFileTransfer() && {
    ASSERT_OPCODE(Opcode::kFileTransfer);
    return FileTransferMessage{body.to_string()};
}

util::result<compound::TransmitDataMessage, Error>
Message::ToTransmitData() && {
    ASSERT_OPCODE(Opcode::kTransmitData);
    return compound::TransmitDataMessage{body.get_many(body.size())};
}

util::result<compound::FinishedMessage, Error> Message::ToFinished() && {
    ASSERT_OPCODE(Opcode::kFinished);
    return compound::FinishedMessage{};
}

util::result<EnquiryMessage, Error> Message::ToEnquiry() && {
    ASSERT_OPCODE(Opcode::kEnquiry);
    return EnquiryMessage{};
}

util::result<ReadMessage, Error> Message::ToRead() && {
    ASSERT_OPCODE(Opcode::kRead);
    return ReadMessage{body.to_string()};
}

util::result<WriteMessage, Error> Message::ToWrite() && {
    ASSERT_OPCODE(Opcode::kWrite);
    std::size_t size = body.size();
    auto file_name = body.get_until(kStringDelimiter);
    auto line = body.to_string();
    return WriteMessage{{file_name.begin(), file_name.end()}, line};
}

util::result<mutex::RequestMessage, Error> Message::ToRequest() && {
    ASSERT_OPCODE(Opcode::kRequest);
    auto clock = util::bytes::extract<sizeof(std::size_t)>(body);
    auto file_name = body.to_string();
    return mutex::RequestMessage{clock, file_name};
}

util::result<mutex::ReplyMessage, Error> Message::ToReply() && {
    ASSERT_OPCODE(Opcode::kReply);
    auto clock = util::bytes::extract<sizeof(std::size_t)>(body);
    auto file_name = body.to_string();
    return mutex::ReplyMessage{clock, file_name};
}

Message OkMessage::ToMessage() && { return {Opcode::kOk, {}}; }

Message ErrorMessage::ToMessage() && {
    return {Opcode::kError, util::buffer(std::move(message))};
}

Message EstablishConnectionMessage::ToMessage() && {
    auto msg = Message{Opcode::kEstablishConnection};
    util::bytes::insert<1>(msg.body, id);
    msg.body.put_iter(message.begin(), message.end());
    return msg;
}

Message ResponseMessage::ToMessage() && {
    return {Opcode::kResponse, util::buffer(std::move(message))};
}

Message FileTransferMessage::ToMessage() && {
    return Message{Opcode::kFileTransfer, util::buffer(std::move(file_name))};
}

Message compound::TransmitDataMessage::ToMessage() && {
    return {Opcode::kTransmitData, util::buffer(std::move(data))};
}

Message compound::FinishedMessage::ToMessage() && {
    return {Opcode::kFinished};
}

Message EnquiryMessage::ToMessage() && { return Message{Opcode::kEnquiry}; }

Message ReadMessage::ToMessage() && {
    return Message{Opcode::kRead, util::buffer(std::move(file_name))};
}

Message WriteMessage::ToMessage() && {
    auto msg = Message{Opcode::kWrite};
    msg.body.put_iter(file_name.begin(), file_name.end(), true);
    msg.body.put(kStringDelimiter, sizeof(kStringDelimiter) - 1, true);
    msg.body.put_iter(line.begin(), line.end(), true);
    return msg;
}

mutex::RequestMessage::RequestMessage(std::size_t timestamp,
                                      std::string file_name)
    : LamportClock{timestamp}, file_name(file_name) {}

Message mutex::RequestMessage::ToMessage() && {
    auto msg = Message{Opcode::kRequest};
    util::bytes::insert<sizeof(std::size_t)>(msg.body, timestamp);
    msg.body.put_iter(file_name.begin(), file_name.end());
    return msg;
}

mutex::ReplyMessage::ReplyMessage(std::size_t timestamp, std::string file_name)
    : LamportClock{timestamp}, file_name(file_name) {}

Message mutex::ReplyMessage::ToMessage() && {
    auto msg = Message{Opcode::kReply};
    util::bytes::insert<sizeof(std::size_t)>(msg.body, timestamp);
    msg.body.put_iter(file_name.begin(), file_name.end());
    return msg;
}

}  // namespace proto
}  // namespace net