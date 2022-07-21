#include "async_message_service.h"

#include <util/bytes.h>
#include <util/console.h>

#include <fstream>

namespace net {
namespace proto {

std::atomic<std::size_t> AsyncMessageService::file_transfer_count_ = {0};

AsyncMessageService::AsyncMessageService(Socket& socket, Components& components)
    : socket_(socket),
      components_(components),
      reading_(false),
      writing_(false),
      expected_(),
      attempting_to_send_() {}

bool AsyncMessageService::ReadingMessage() const { return reading_; }

bool AsyncMessageService::WritingMessage() const { return writing_; }

void AsyncMessageService::ResetCurrentMessage() {
    opcode_ = util::none;
    expected_ = util::none;
    body_ = util::buffer();
}

void AsyncMessageService::ResetCompoundMessage() {
    compound_message_parent_ = util::none;
    finished_compound_ = false;
    ResetCurrentMessage();
}

void AsyncMessageService::ReadMessage(const recv_callback_t& callback) {
    reading_ = true;
    auto my_callback = [this, callback](util::result<Message, Error> result) {
        reading_ = false;
        callback(std::move(result));
    };
    ResetCompoundMessage();

    // There may be extra bytes in the input buffer from a previous read.
    auto res = ProcessAllBytes();
    if (res.is_err()) {
        my_callback(res.err());
        return;
    }

    ReceiveBytes(my_callback);
}

void AsyncMessageService::PollForRead(const recv_callback_t& callback) {
    auto res = socket_.Poll(PollOption::kRead);
    if (res.is_err()) {
        callback({util::err, std::move(res).err()});
    } else {
        auto status = res.ok();
        switch (status) {
            case PollStatus::Success: {
                components_.thread_pool.Schedule(
                    [this, callback]() { ReceiveBytes(callback); });
            } break;
            case PollStatus::Expire: {
                callback(Error::Create("Message reader timed out"));
            } break;
            default: {
                callback(Error::Create("Poll failed"));
            } break;
        }
    }
}

Message AsyncMessageService::GetReturnedMessage() {
    return compound_message_parent_.has_value()
               ? std::move(compound_message_parent_.value())
               : Message{opcode_.value(), std::move(body_)};
}

void AsyncMessageService::ReceiveBytes(const recv_callback_t& callback) {
    if (FinishedReading()) {
        // Finished reading, return out the finished message.
        callback(GetReturnedMessage());
    } else {
        // Receive whatever is in the socket.
        auto result = socket_.Receive();
        if (result.is_err()) {
            callback(result.err());
            return;
        }

        // Process the bytes received.
        auto res = ProcessAllBytes();

        if (res.is_err()) {
            callback(res.err());
        } else if (FinishedReading()) {
            callback(GetReturnedMessage());
        } else {
            // Schedule the next read, if needed.
            components_.thread_pool.Schedule(
                [this, callback]() { PollForRead(callback); });
        }
    }
}

bool AsyncMessageService::FinishedReadingCurrentMessage() {
    return opcode_.has_value() && expected_.has_value() &&
           expected_.value() == body_.size();
}

bool AsyncMessageService::FinishedReadingCompoundMessage() {
    if (compound_message_parent_.has_value()) {
        return finished_compound_;
    }
    return false;
}

bool AsyncMessageService::FinishedReading() {
    return FinishedReadingCompoundMessage() || FinishedReadingCurrentMessage();
}

bool AsyncMessageService::OpcodeStartsACompoundMessage(Opcode opcode) {
    // Only one message type is compound for now.
    return opcode == Opcode::kFileTransfer;
}

bool AsyncMessageService::InACompoundMessage() {
    return compound_message_parent_.has_value();
}

util::result<void, Error> AsyncMessageService::ProcessAllBytes() {
    // Keep processing bytes in the socket's output buffer until there are
    // no more, or until we have finished reading a complete message.
    bool progress_can_be_made = true;
    while (progress_can_be_made && !FinishedReading()) {
        std::size_t bytes_available = socket_.Input().size();
        if (bytes_available == 0) {
            // No more bytes in the output buffer. If we are at this point,
            // we likely need to read more bytes from the socket.
            break;
        }

        auto res = ProcessBytes(bytes_available);
        RETURN_IF_ERROR(res);
        progress_can_be_made = res.ok();
    }

    return util::ok;
}

util::result<bool, Error> AsyncMessageService::ProcessBytes(
    std::size_t bytes_available) {
    auto res = ProcessBytesIntoCurrentMessage(bytes_available);
    RETURN_IF_ERROR(res);

    if (FinishedReadingCurrentMessage()) {
        // We have finished reading a complete message, but compound
        // messages may requires multiple messages in a row to be read. The
        // logic below handles compound messages.
        if (OpcodeStartsACompoundMessage(opcode_.value())) {
            // We are starting a compound message, so we'll keep reading!
            RETURN_IF_ERROR(HandleCompoundMessageHeader());
            ResetCurrentMessage();
        } else if (InACompoundMessage()) {
            // We are already in a compound message, so we take the current
            // message, execute it, then clear it so we can keep reading.
            RETURN_IF_ERROR(HandleCompoundMessageFrame());
            ResetCurrentMessage();
        }
    }

    return res.ok();
}

util::result<void, Error> AsyncMessageService::HandleCompoundMessageHeader() {
    compound_message_parent_ = Message{opcode_.value(), std::move(body_)};

    switch (opcode_.value()) {
        case Opcode::kFileTransfer: {
            // Transfer files are sent in chunks, so we must have a file to
            // write these chunks to as they come in.
            std::string file_name = MakeTransferFileName();
            auto res = components_.temp_file_service.CreateFile(file_name);
            if (res.is_err()) {
                return res.err();
            }
            transfer_file_name_ = res.ok();
        } break;
        default:
            return Error::Create("Invalid compound message opcode");
    }

    return util::ok;
}

util::result<bool, Error> AsyncMessageService::ProcessBytesIntoCurrentMessage(
    std::size_t bytes_available) {
    // A message is made up of an opcode, body size, and the body itself.
    // The logic here assures all parts are read in order.

    if (!opcode_.has_value()) {
        if (bytes_available < kOpcodeLength) {
            return false;
        }

        opcode_ = static_cast<Opcode>(socket_.Input().get());
        bytes_available -= kOpcodeLength;
    }

    if (!expected_.has_value()) {
        if (bytes_available < kBodySizeLength) {
            return false;
        }

        expected_ = util::bytes::extract<kBodySizeLength>(socket_.Input());
        if (expected_.value() != 0) {
            body_.reserve(expected_.value());
        }
        bytes_available -= kBodySizeLength;
    }

    // Continually read out the entire body. The length of the body is given
    // in the message header.
    std::size_t expected = expected_.value();
    std::size_t currently_have = body_.size();
    if (expected > currently_have) {
        std::size_t bytes_to_use =
            std::min(bytes_available, expected - currently_have);
        auto bytes = socket_.Input().get_many(bytes_to_use);
        body_.put_iter(bytes.begin(), bytes.end());
    }

    // For all we know, there could be more progress to make! This only
    // matters for compound messages.
    return true;
}

util::result<void, Error> AsyncMessageService::HandleCompoundMessageFrame() {
    if (!InACompoundMessage() || !FinishedReadingCurrentMessage()) {
        return Error::Create(
            "Cannot execute a message that has not been completely read");
    }

    Message& parent = compound_message_parent_.value();

    switch (parent.opcode) {
        case Opcode::kFileTransfer: {
            switch (opcode_.value()) {
                case Opcode::kTransmitData: {
                    // Write the data chunk to the file.
                    std::ofstream file(transfer_file_name_,
                                       std::ios::binary | std::ios::app);
                    auto views = body_.view();
                    for (auto& view : views) {
                        std::copy(view.data, view.data + view.size,
                                  std::ostreambuf_iterator<char>(file));
                    }
                } break;
                case Opcode::kFinished:
                    finished_compound_ = true;
                    break;
                default:
                    return Error::Create("Invalid file transfer message frame");
            }
        } break;
        default:
            return Error::Create("Invalid compound message opcode");
    }

    return util::ok;
}

std::string AsyncMessageService::MakeTransferFileName() {
    std::size_t transfer_id = ++file_transfer_count_;
    return util::string::stream("transfer_", transfer_id, ".data");
}

std::string AsyncMessageService::GetLastTransferFileName() const {
    return transfer_file_name_;
}

void AsyncMessageService::WriteMessage(Message&& msg,
                                       const send_callback_t& callback) {
    writing_ = true;
    auto my_callback = [this, callback](util::result<void, Error> result) {
        writing_ = false;
        callback(std::move(result));
    };
    auto res = FillOutputBuffer(std::move(msg));
    if (res.is_err()) {
        my_callback(res.err());
    } else {
        SendBytes(my_callback);
    }
}

util::result<void, Error> AsyncMessageService::FillOutputBuffer(Message&& msg) {
    attempting_to_send_ = 0;
    if (OpcodeStartsACompoundMessage(msg.opcode)) {
        switch (msg.opcode) {
            case Opcode::kFileTransfer: {
                // Split the file into multiple chunks, depending on whether
                // we are a client or server. This is part of the project
                // specifications.

                auto res = std::move(msg).ToFileTransfer();
                RETURN_IF_ERROR(res);
                FileTransferMessage& msg = res.ok();

                std::ifstream file(msg.file_name, std::ios::binary);
                if (!file) {
                    return Error::Create("Could not open file for transfer");
                }

                // Write compound message header, which indicates a file
                // transfer has been initiated.
                RETURN_IF_ERROR(
                    PutMessageInOutputBuffer(std::move(msg).ToMessage()));

                std::size_t chunk_size = components_.options.server ? 200 : 100;
                bool more_chunks = true;
                while (more_chunks) {
                    auto chunk = compound::TransmitDataMessage{};
                    chunk.data.resize(chunk_size);

                    // Read out at most `chunk_size` bytes from the file.
                    std::streamsize bytes_read = file.readsome(
                        reinterpret_cast<char*>(chunk.data.data()), chunk_size);

                    if (bytes_read < 0) {
                        return Error::Create(
                            "Failed to read next chunk from file");
                    } else if (bytes_read == 0) {
                        more_chunks = false;
                        break;
                    } else if (bytes_read < chunk_size) {
                        more_chunks = false;
                    }

                    util::safe_debug::log("Sending", bytes_read,
                                          "bytes in a file transfer chunk");

                    // Put the file in the buffer.
                    RETURN_IF_ERROR(
                        PutMessageInOutputBuffer(std::move(chunk).ToMessage()));
                }

                // Mark the message as finished.
                RETURN_IF_ERROR(PutMessageInOutputBuffer(
                    compound::FinishedMessage{}.ToMessage()));

            } break;
            default: {
                return Error::Create(util::string::stream(
                    "Opcode ", static_cast<int>(msg.opcode),
                    " is not a compound message opcode"));
            }
        }
    } else {
        // Only one message, just put it in directly.
        RETURN_IF_ERROR(PutMessageInOutputBuffer(std::move(msg)));
    }

    return util::ok;
}

util::result<void, Error> AsyncMessageService::PutMessageInOutputBuffer(
    Message&& msg) {
    std::size_t body_size = msg.body.size();
    if (body_size > kMaxBodySize) {
        return Error::Create("Body size exceeds maximum");
    }

    util::buffer& output = socket_.Output();

    output.put(&msg.opcode, kOpcodeLength, true);
    std::uint32_t size = static_cast<std::uint32_t>(body_size);
    util::bytes::insert<kBodySizeLength>(output, size);
    output.move_buffer(msg.body, true);

    // Used for detecting when we have finished sending all of a message to
    // the endpoint.
    attempting_to_send_ += kOpcodeLength + kBodySizeLength + body_size;
    return util::ok;
}

void AsyncMessageService::PollForWrite(const send_callback_t& callback) {
    auto res = socket_.Poll(PollOption::kWrite);
    if (res.is_err()) {
        callback({util::err, std::move(res).err()});
    } else {
        auto status = res.ok();
        switch (status) {
            case PollStatus::Success: {
                components_.thread_pool.Schedule(
                    [this, callback]() { SendBytes(callback); });
            } break;
            case PollStatus::Expire: {
                callback(Error::Create("Message writer timed out"));
            } break;
            default: {
                callback(Error::Create("Poll failed"));
            } break;
        }
    }
}

void AsyncMessageService::SendBytes(const send_callback_t& callback) {
    if (FinishedSending()) {
        callback(util::ok);
    } else {
        // Send the output buffer.
        auto result = socket_.Send();
        if (result.is_err()) {
            callback(result.err());
            return;
        }

        std::size_t bytes_sent = result.ok();
        if (bytes_sent > attempting_to_send_) {
            attempting_to_send_ = 0;
        } else {
            attempting_to_send_ -= bytes_sent;
        }

        if (FinishedSending()) {
            callback(util::ok);
        } else {
            components_.thread_pool.Schedule(
                [this, callback]() { PollForWrite(callback); });
        }
    }
}

bool AsyncMessageService::FinishedSending() { return attempting_to_send_ == 0; }

}  // namespace proto
}  // namespace net