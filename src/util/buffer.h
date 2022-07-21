#ifndef UTIL_BUFFER_
#define UTIL_BUFFER_

#include <cstdint>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace util {

/**
 * @brief Exception for buffer operations.
 *
 */
class buffer_exception : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};

/**
 * @brief Simple structure for viewing a buffer.
 *
 */
struct buffer_view {
    std::uint8_t* data;
    std::size_t size;
};

namespace buffer_detail {

template <typename T, typename = void>
struct is_iterator {
    static constexpr bool value = false;
};

template <typename T>
struct is_iterator<
    T, typename std::enable_if<!std::is_same<
           typename std::iterator_traits<T>::value_type, void>::value>::type> {
    static constexpr bool value = true;
};

}  // namespace buffer_detail

/**
 * @brief Circular buffer that can be dynamically resized as more data comes in.
 *
 */
class buffer {
   public:
    /**
     * @brief Maximum size the buffer can take.
     *
     */
    static constexpr std::size_t max_size =
        std::numeric_limits<std::size_t>::max();

    /**
     * @brief Default size of a buffer.
     *
     */
    static constexpr std::size_t default_size = 1024;

    buffer(std::size_t size = default_size);
    buffer(const void* data, std::size_t size);
    buffer(std::string&& data);
    buffer(std::vector<std::uint8_t>&& data);
    ~buffer();
    buffer(const buffer& other);
    buffer& operator=(const buffer& rhs);
    buffer(buffer&& other) noexcept;
    buffer& operator=(buffer&& rhs) noexcept;

    /**
     * @brief Resets the buffer.
     *
     */
    void reset();

    /**
     * @brief Checks if the buffer is full.
     *
     * @return true
     * @return false
     */
    bool full() const;

    /**
     * @brief Checks if the buffer is empty.
     *
     * @return true
     * @return false
     */
    bool empty() const;

    /**
     * @brief Returns the capacity of the buffer.
     *
     * @return std::size_t
     */
    std::size_t capacity() const;

    /**
     * @brief Returns the size of existing data in the buffer that has not been
     * read out.
     *
     * @return std::size_t
     */
    std::size_t size() const;

    /**
     * @brief Writes a single byte to the buffer.
     *
     * @param byte New byte
     * @param allow_resize Allow the buffer to dynamically resize?
     */
    void put(std::uint8_t byte, bool allow_resize = false);

    /**
     * @brief Writes new data into the buffer.
     *
     * @param data Data source
     * @param size Number of bytes to write
     * @param allow_resize Allow the buffer to dynamically resize?
     */
    void put(const void* data, std::size_t size, bool allow_resize = false);

    /**
     * @brief Writes new data into the buffer from an iterator range.
     *
     * @tparam It Iterator type
     * @param begin Beginning of range
     * @param end End of range
     * @param allow_resize Allow the buffer to dynamically resize?
     */
    template <typename It,
              typename std::enable_if<buffer_detail::is_iterator<It>::value,
                                      int>::type = 0>
    void put_iter(It begin, It end, bool allow_resize = false) {
        std::size_t distance = std::distance(begin, end);
        if (distance > space_remaining()) {
            if (allow_resize) {
                resize(distance);
            } else {
                throw buffer_exception("Buffer overflow");
            }
        }

        std::size_t first_write_max_size = space_remaining_until_end();
        if (first_write_max_size >= distance) {
            // We can fit this data without any circling back.
            std::copy(begin, end, data_ + write_);
        } else {
            // Write to the end of the buffer, then circle back and write the
            // rest.
            It mid = std::next(begin, first_write_max_size);
            std::copy(begin, mid, data_ + write_);
            std::copy(mid, end, data_);
        }

        advance_write(distance);
    }

    /**
     * @brief Moves another buffer into this buffer.
     *
     * Data is read out of the given buffer and placed in this buffer.
     *
     * @param other Other buffer
     * @param allow_resize Allow the buffer to dynamically resize?
     */
    void move_buffer(util::buffer& other, bool allow_resize);

    /**
     * @brief Reads the next byte.
     *
     * @return std::uint8_t
     */
    std::uint8_t get();

    /**
     * @brief Get the next bytes from the buffer.
     *
     * @param amount
     * @return std::vector<std::uint8_t>
     */
    std::vector<std::uint8_t> get_many(std::size_t amount);

    /**
     * @brief Get the next bytes from the buffer until the given delimiter is
     * matched.
     *
     * The delimiter is extracted from the buffer but not returned to the
     * caller.
     *
     * If the end of the buffer is reached before the delimiter is matched, the
     * entire buffer is read and returned. If the buffer is empty, an empty
     * vector will be returned.
     *
     * @param delim
     * @return std::vector<std::uint8_t>
     */
    std::vector<std::uint8_t> get_until(const std::string& delim);

    /**
     * @brief Reserves a contiguous space in the buffer that of the specified
     * size.
     *
     * Use `commit` to commit the section to the buffer.
     *
     * @param size Number of bytes to reserve
     * @return std::uint8_t* Pointer to reserved buffer
     */
    std::uint8_t* reserve(std::size_t size);

    /**
     * @brief Consumes a given number of bytes from the buffer by advancing the
     * read pointer.
     *
     * @param size
     */
    void consume(std::size_t amount);

    /**
     * @brief Commits a given number of bytes to the buffer by advancing the
     * write pointer.
     *
     * Should be used after a buffer obtained from `reserve` is filled.
     *
     * @param size
     */
    void commit(std::size_t size);

    /**
     * @brief Shifts the buffer to the beginning of the memory space.
     *
     */
    void shift();

    /**
     * @brief Creates a vector of `buffer_view`s into the circular buffer.
     *
     * Guaranteed to return only 1 or 2 buffers.
     *
     * @return std::vector<buffer_view>
     */
    std::vector<buffer_view> view() const;

    /**
     * @brief Reads the buffer into a string format.
     *
     * @return std::string
     */
    std::string to_string();

    /**
     * @brief Copies the given buffer into this one.
     *
     * @param other
     */
    void copy_into(const util::buffer& other);

   private:
    std::size_t space_remaining() const;
    std::size_t space_remaining_until_end() const;
    void advance_write(std::size_t by);
    void advance_read(std::size_t by);
    void read_into(void* dest, std::size_t size);
    void resize(std::size_t to_fit);

    std::uint8_t* data_;
    std::size_t capacity_;
    std::size_t read_;
    std::size_t write_;
    bool full_;
};
}  // namespace util

#endif  // UTIL_BUFFER_