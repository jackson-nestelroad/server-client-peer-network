#include "buffer.h"

#include <cstring>

namespace util {

buffer::buffer(std::size_t size)
    : capacity_(size), read_(0), write_(0), full_(false) {
    if (size == 0) {
        throw buffer_exception("Size must be greater than zero");
    }

    data_ = new std::uint8_t[size];
}

buffer::buffer(const void* data, std::size_t size)
    : capacity_(size), read_(0), write_(0), full_(true) {
    if (size == 0) {
        // If size is 0, there is no guarantee the given data is valid.
        // However, we still want to create a valid buffer, so just fall back on
        // the default case.
        capacity_ = default_size;
        full_ = false;
        data_ = new std::uint8_t[capacity_];
    } else {
        // We should have valid data.
        if (!data) {
            throw buffer_exception("Data cannot be NULL");
        }
        data_ = new std::uint8_t[size];
        std::memmove(data_, data, size);
    }
}

buffer::buffer(std::string&& data) : buffer(data.data(), data.size()) {}

buffer::buffer(std::vector<std::uint8_t>&& data)
    : buffer(data.data(), data.size()) {}

buffer::~buffer() {
    if (data_) {
        delete[] data_;
    }
}

buffer::buffer(const buffer& other) : buffer(other.capacity()) {
    copy_into(other);
}

buffer& buffer::operator=(const buffer& rhs) {
    reset();
    copy_into(rhs);
    return *this;
}

buffer::buffer(buffer&& other) noexcept
    : data_(other.data_),
      capacity_(other.capacity_),
      read_(other.read_),
      write_(other.write_),
      full_(other.full_) {
    if (this != &other) {
        other.data_ = nullptr;
        other.capacity_ = 0;
        other.reset();
    }
}

buffer& buffer::operator=(buffer&& rhs) noexcept {
    std::swap(data_, rhs.data_);
    std::swap(capacity_, rhs.capacity_);
    std::swap(read_, rhs.read_);
    std::swap(write_, rhs.write_);
    std::swap(full_, rhs.full_);
    return *this;
}

void buffer::reset() {
    read_ = 0;
    write_ = 0;
    full_ = false;
}

bool buffer::full() const { return full_; }

bool buffer::empty() const { return !full_ && read_ == write_; }

std::size_t buffer::capacity() const { return capacity_; }

std::size_t buffer::size() const {
    if (full_) {
        return capacity_;
    }

    if (write_ >= read_) {
        return write_ - read_;
    } else {
        return capacity_ + write_ - read_;
    }
}

std::size_t buffer::space_remaining() const { return capacity_ - size(); }

std::size_t buffer::space_remaining_until_end() const {
    if (read_ > write_) {
        return read_ - write_;
    } else {
        return capacity_ - write_;
    }
}

void buffer::advance_write(std::size_t by) {
    if (by == 0) {
        return;
    }
    if (full_ || space_remaining() < by) {
        throw buffer_exception("Buffer overflow");
    }

    // Advance write pointer.
    write_ += by;

    // Exceeded capacity, circle back.
    if (write_ >= capacity_) {
        // Same as modulo, but the earlier guard assured that the advance by
        // number is not greater than the amount of space remaining in the
        // buffer. The most we will exceed capacity is the capacity itself
        // (buffer is initially empty), which will put write_ at read_, and mark
        // the buffer as full.
        write_ -= capacity_;
    }

    full_ = read_ == write_;
}

void buffer::advance_read(std::size_t by) {
    if (by == 0) {
        return;
    }

    if (by > size()) {
        throw buffer_exception("Buffer overflow");
    }

    // Advance the read pointer.
    read_ += by;
    full_ = false;

    // Same logic as advance_write.
    if (read_ >= capacity_) {
        read_ -= capacity_;
    }
}

void buffer::put(std::uint8_t byte, bool allow_resize) {
    put(&byte, 1, allow_resize);
}

void buffer::put(const void* data, std::size_t size, bool allow_resize) {
    if (!data) {
        throw buffer_exception("Cannot store nullptr");
    }

    if (size > space_remaining()) {
        if (allow_resize) {
            resize(size);
        } else {
            throw buffer_exception("Buffer overflow");
        }
    }

    // At this point, we know we can fit the data.

    std::size_t first_write_max_size = space_remaining_until_end();
    if (first_write_max_size >= size) {
        // We can fit this data without any circling back.
        std::memcpy(data_ + write_, data, size);
    } else {
        // Write to the end of the buffer, then circle back and write the rest.
        std::memcpy(data_ + write_, data, first_write_max_size);
        std::memcpy(
            data_,
            static_cast<const std::uint8_t*>(data) + first_write_max_size,
            size - first_write_max_size);
    }

    advance_write(size);
}

void buffer::move_buffer(util::buffer& other, bool allow_resize) {
    std::size_t written = 0;
    auto views = other.view();
    for (auto& view : views) {
        std::size_t write_size =
            allow_resize ? view.size : std::min(space_remaining(), view.size);
        put(view.data, write_size, allow_resize);
        written += write_size;
    }
    other.consume(written);
}

void buffer::read_into(void* dest, std::size_t size) {
    std::size_t first_write_max_size = space_remaining_until_end();
    if (first_write_max_size >= size) {
        // We can fit this data without any circling back.
        std::memcpy(dest, data_ + read_, size);
    } else {
        // Write to the end of the buffer, then circle back and write the rest.
        std::memcpy(dest, data_ + read_, first_write_max_size);
        std::memcpy(static_cast<std::uint8_t*>(dest) + first_write_max_size,
                    data_, size - first_write_max_size);
    }
}

void buffer::resize(std::size_t to_fit) {
    std::size_t current_size = size();
    if (max_size - to_fit <= current_size) {
        throw buffer_exception("Maximum capacity exceeded");
    }

    std::size_t needed = current_size + to_fit;
    std::size_t new_capacity = capacity_;
    while (new_capacity < needed) {
        if (new_capacity > (max_size >> 1)) {
            // We cannot double the capacity again or we will overflow.
            // Just give exactly what is needed.
            new_capacity = needed;
        }
        new_capacity <<= 1;
    }

    std::uint8_t* new_data = new std::uint8_t[new_capacity];
    read_into(new_data, current_size);

    data_ = new_data;
    capacity_ = new_capacity;
    read_ = 0;
    write_ = 0;
    full_ = false;
    advance_write(current_size);
}

std::uint8_t buffer::get() {
    if (empty()) {
        throw buffer_exception("Cannot read an empty buffer");
    }

    std::uint8_t next = *(data_ + read_);
    advance_read(1);
    return next;
}

std::vector<std::uint8_t> buffer::get_many(std::size_t amount) {
    if (amount > size()) {
        throw buffer_exception("Buffer overflow");
    }

    std::vector<std::uint8_t> output(amount);

    read_into(output.data(), amount);
    advance_read(amount);

    return output;
}

std::vector<std::uint8_t> buffer::get_until(const std::string& delim) {
    std::vector<std::uint8_t> output;
    std::size_t delim_pos = 0;
    while (!empty()) {
        std::uint8_t next = get();
        if (next == delim[delim_pos]) {
            // Matched the next character of the delimiter.
            ++delim_pos;
            if (delim_pos == delim.length()) {
                // Matched the entire delimiter.
                return output;
            }
        } else {
            // Did not match the next character of the delimiter.
            if (delim_pos > 0) {
                // We matched a part of the delimiter earlier, but those
                // characters were not added to the buffer. Add them here.
                std::copy(delim.begin(), std::next(delim.begin(), delim_pos),
                          std::back_inserter(output));
                delim_pos = 0;
            }
            output.push_back(next);
        }
    }

    // Reached end of buffer without matching delimiter.
    return output;
}

std::uint8_t* buffer::reserve(std::size_t size) {
    if (size == 0) {
        throw buffer_exception("Cannot reserve buffer of zero size");
    }

    // Have enough immediately ready.
    std::size_t space_to_end = space_remaining_until_end();
    if (space_to_end >= size) {
        return data_ + write_;
    }

    // Have enough in the buffer, but it's circled around.
    if (space_remaining() >= size) {
        shift();
        return data_ + write_;
    }

    // Buffer is not big enough!
    resize(size);
    return data_ + write_;
}

void buffer::consume(std::size_t amount) {
    if (amount > size()) {
        throw buffer_exception("Buffer overflow");
    }
    advance_read(amount);
}

void buffer::commit(std::size_t size) { advance_write(size); }

void buffer::shift() {
    // Data is full and already shifted.
    // Nothing to do here, so we exit early to avoid memory operations.
    if (full_ && read_ == 0) {
        return;
    }

    if (write_ > read_) {
        // Data is not circled around, so a simple move works.
        std::size_t size = write_ - read_;
        std::memmove(data_, data_ + read_, size);
        read_ = 0;
        write_ = size;
    } else {
        // Data is circled around, so must be careful not to overwrite and lose
        // data.

        /*
                w   r
            0 1 2 3 4 5
            c d 0 0 a b
            capacity = 6

            temp
            0 1
            c d

            first_write_size = 6 - 4 = 2
            After first move:
                w   r
            0 1 2 3 4 5
            a b 0 0 a b

            After second move:
                w   r
            0 1 2 3 4 5
            a b c d a b

            Update pointers:
            r       w
            0 1 2 3 4 5
            a b c d a b
        */

        // Store the last write_ bytes (number of bytes circled around) in a
        // temporary memory location.
        // We know write_ is not 0 because of the check at the beginning of the
        // function.

        std::uint8_t* temp = new std::uint8_t[write_];
        std::memmove(temp, data_, write_);

        // Move the beginning of the data to the actual beginning of the buffer.
        std::size_t first_write_size = capacity_ - read_;
        std::memmove(data_, data_ + read_, first_write_size);

        // Move the rest of the data in.
        std::memmove(data_ + first_write_size, temp, write_);

        // Update pointers.
        std::size_t data_size = size();
        read_ = 0;
        write_ = data_size;
    }
}

std::vector<buffer_view> buffer::view() const {
    if (write_ >= read_) {
        return {{data_ + read_, size()}};
    } else {
        return {{data_ + read_, capacity_ - read_}, {data_, write_}};
    }
}

std::string buffer::to_string() {
    std::vector<std::uint8_t> data = get_many(size());
    return {data.begin(), data.end()};
}

void buffer::copy_into(const util::buffer& buffer) {
    reserve(buffer.size());
    auto views = buffer.view();
    for (auto& view : views) {
        put(view.data, view.size);
    }
}

}  // namespace util