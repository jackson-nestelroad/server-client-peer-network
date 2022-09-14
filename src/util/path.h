#ifndef UTIL_PATH_
#define UTIL_PATH_

#include <string>
#include <system_error>

namespace util {
namespace fs {

/**
 * @brief Path for a file or directory in a hierarchical file system.
 *
 */
class path {
   public:
    static constexpr char preferred_separator = '/';
    static constexpr std::size_t max_path_size = 4096;

    using value_type = std::string::value_type;

    path();
    path(const std::string& str);
    template <typename InputIterator>
    path(InputIterator begin, InputIterator end) : path() {
        assign(std::string{begin, end});
    }
    path(const path& other) = default;
    path& operator=(const path& rhs) = default;
    path& operator=(const std::string& str);
    path(path&& rhs) noexcept = default;
    path& operator=(path&& rhs) = default;

    static path current_path(std::error_code& ec);

    /**
     * @brief Converts the path to normal form.
     *
     * @return path
     */
    path lexically_normal() const;

    /**
     * @brief Creates the relative offset between this path and the given path.
     *
     * @param base
     * @return path
     */
    path lexically_relative(const path& base) const;

    path root_path() const;
    path root_name() const;
    path root_directory() const;
    path relative_path() const;
    path parent_path() const;
    path filename() const;
    path stem() const;
    path extension() const;

    bool has_root_path() const;
    bool has_root_name() const;
    bool has_root_directory() const;
    bool has_relative_path() const;
    bool has_parent_path() const;
    bool has_filename() const;
    bool has_stem() const;
    bool has_extension() const;

    bool is_absolute() const;
    bool is_relative() const;

    bool empty() const;

    int compare(const path& rhs) const;

    bool operator==(const path& rhs) const;
    bool operator!=(const path& rhs) const;
    bool operator==(const std::string& str) const;
    bool operator!=(const std::string& str) const;
    path& operator/=(const path& rhs);
    path& operator/=(const std::string& str);

    path& append(const std::string& str);
    path& append(const path& add);

    path& assign(const path& replacement);
    path& assign(const std::string& str);

    template <typename InputIterator>
    path& append(InputIterator first, InputIterator last) {
        return append(std::string{first, last});
    }

    std::string native() const;
    std::string string() const;

    void clear();
    path& remove_filename();
    path& replace_filename(const path& replacement);
    path& remove_trailing_separator();

    class iterator;
    using const_iterator = iterator;

    iterator begin() const;
    iterator end() const;

   private:
   private:
    void process_assigned_path();

    std::size_t root_path_length() const;
    std::size_t root_name_length() const;

    std::size_t prefix_length_ = 0;
    std::string path_;
};

path operator/(const path& lhs, const path& rhs);
path operator/(const path& lhs, const std::string& rhs);

/**
 * @brief Iterator for names in a hierarchical file system path.
 *
 */
class path::iterator {
   private:
    using internal_iterator = std::string::const_iterator;

   public:
    using value_type = const path;
    using difference_type = std::ptrdiff_t;
    using pointer = const path*;
    using reference = const path&;
    using iterator_category = std::bidirectional_iterator_tag;

    iterator();
    iterator(const path& path, const internal_iterator& pos);
    iterator& operator++();
    iterator operator++(int);
    iterator& operator--();
    iterator operator--(int);
    bool operator==(const iterator& rhs) const;
    bool operator!=(const iterator& rhs) const;
    reference operator*() const;
    pointer operator->() const;

   private:
    internal_iterator increment(const internal_iterator& pos) const;
    internal_iterator decrement(const internal_iterator& pos) const;
    void update_current();

    internal_iterator first_;
    internal_iterator last_;
    internal_iterator prefix_;
    internal_iterator root_;
    internal_iterator iter_;
    path current_;

    friend class path;
};

}  // namespace fs
}  // namespace util

#endif  // UTIL_PATH_