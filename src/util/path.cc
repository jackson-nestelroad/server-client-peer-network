#include "path.h"

#include <unistd.h>
#include <util/strings.h>

#include <memory>

namespace util {
namespace fs {

decltype(path::preferred_separator) constexpr path::preferred_separator;
decltype(path::max_path_size) constexpr path::max_path_size;

path::path() : prefix_length_(0) {}

path::path(const std::string& str) : path() { assign(str); }

path path::current_path(std::error_code& ec) {
    ec.clear();
    std::size_t pathlen = static_cast<std::size_t>(
        std::max(int(::pathconf(".", _PC_PATH_MAX)), int(max_path_size)));
    std::unique_ptr<char[]> buffer(new char[pathlen + 1]);
    if (::getcwd(buffer.get(), pathlen) == nullptr) {
        ec = std::error_code(errno, std::system_category());
        return path();
    }
    return path(buffer.get());
}

path& path::operator=(const std::string& str) {
    return this->operator=(path(str));
}

void path::process_assigned_path() {
    auto start = prefix_length_;
    if (path_.length() > prefix_length_ + 2 &&
        path_[prefix_length_] == preferred_separator &&
        path_[prefix_length_ + 1] == preferred_separator &&
        path_[prefix_length_ + 2] != preferred_separator) {
        // Path begins with two slashes only, so it is a network name.
        //
        // Do not remove them!
        start = prefix_length_ + 2;
    }

    // Remove duplicate slashes.
    auto unique = std::unique(
        path_.begin() + static_cast<std::string::difference_type>(start),
        path_.end(), [](path::value_type lhs, path::value_type rhs) {
            return lhs == rhs && lhs == preferred_separator;
        });
    path_.erase(unique, path_.end());
}

path path::lexically_normal() const {
    path normalized;
    bool last_dot_dot = false;
    for (auto name : *this) {
        if (name == ".") {
            // Ignore dots.
            normalized /= "";
            continue;
        } else if (name == ".." && !normalized.empty()) {
            auto root = root_path();
            if (normalized == root) {
                // Can't go farther back than the root.
                continue;
            } else if (*(--normalized.end()) != "..") {
                // Remove previous file name, and potential slash.
                if (normalized.path_.back() == preferred_separator) {
                    normalized.path_.pop_back();
                }
                normalized.remove_filename();
                continue;
            }
        }

        if (!(name.empty() && last_dot_dot)) {
            // Add a name only if the next entry does not cancel it out.
            normalized /= name;
        }
        last_dot_dot = name == "..";
    }

    // Empty normal form is a dot.
    if (normalized.empty()) {
        normalized = ".";
    }

    return normalized;
}

path path::lexically_relative(const path& base) const {
    if (root_name() != base.root_name() ||
        is_absolute() != base.is_absolute() ||
        (!has_root_directory() && base.has_root_directory())) {
        // No way to determine relation.
        return path();
    }

    // Increment a and b while they are equal.
    auto me = begin();
    auto them = base.begin();
    while (me != end() && me != base.end() && *me == *them) {
        ++me;
        ++them;
    }

    if (me == end() && them == base.end()) {
        // Reached end of both paths at the same time, so it's the same path!
        return path(".");
    }

    // Count the difference.
    int count = 0;
    for (auto it = them; it != base.end(); ++it) {
        auto& name = *it;
        if (name != "" && name != "." && name != "..") {
            ++count;
        } else if (name == "..") {
            --count;
        }
    }
    if (count < 0) {
        // I am backwards compared to the other path.
        return path();
    }

    path result;
    for (int i = 0; i < count; ++i) {
        result /= "..";
    }
    for (auto it = me; it != end(); ++it) {
        auto& name = *it;
        result /= name;
    }
    return result;
}

bool path::empty() const { return path_.empty(); }

int path::compare(const path& rhs) const {
    // First, compare root names.
    auto rnl1 = root_name_length();
    auto rnl2 = rhs.root_name_length();
    auto rnc = path_.compare(0, rnl1, rhs.path_, 0, std::min(rnl1, rnl2));
    if (rnc) {
        return rnc;
    }

    // Next, compare root directories.
    bool hrd1 = has_root_directory();
    bool hrd2 = rhs.has_root_directory();
    if (hrd1 != hrd2) {
        return hrd1 ? 1 : -1;
    }

    if (hrd1) {
        ++rnl1;
        ++rnl2;
    }

    // Compare entries.
    auto it1 = path_.begin() + static_cast<int>(rnl1);
    auto it2 = rhs.path_.begin() + static_cast<int>(rnl2);
    while (it1 != path_.end() && it2 != rhs.path_.end() && *it1 == *it2) {
        ++it1;
        ++it2;
    }

    if (it1 == path_.end()) {
        return it2 == rhs.path_.end() ? 0 : -1;
    }
    if (it2 == path_.end()) {
        return 1;
    }

    // Does one end in a separator?
    if (*it1 == preferred_separator) {
        return -1;
    }
    if (*it2 == preferred_separator) {
        return 1;
    }
    return *it1 < *it2 ? -1 : 1;
}

bool path::operator==(const path& rhs) const { return compare(rhs) == 0; }

bool path::operator!=(const path& rhs) const { return compare(rhs) != 0; }

bool path::operator==(const std::string& str) const {
    return compare(path(str)) == 0;
}
bool path::operator!=(const std::string& str) const {
    return compare(path(str)) != 0;
}

path operator/(const path& lhs, const path& rhs) {
    path merged(lhs);
    merged /= rhs;
    return merged;
}

path operator/(const path& lhs, const std::string& rhs) {
    path merged(lhs);
    merged /= path(rhs);
    return merged;
}

path& path::operator/=(const path& rhs) {
    if (rhs.empty()) {
        // Assigning an empty path.
        if (!path_.empty() &&
            path_[path_.length() - 1] != preferred_separator &&
            path_[path_.length() - 1] != ':') {
            // End of our path needs a separator.
            path_ += preferred_separator;
        }
        return *this;
    }
    if ((rhs.is_absolute() &&
         (path_ != root_name().path_ || rhs.path_ != "/")) ||
        (rhs.has_root_name() && rhs.root_name() != root_name())) {
        // Different root names.
        assign(rhs);
        return *this;
    }
    if (rhs.has_root_directory()) {
        // Root directory overwrites previous path.
        assign(root_name());
    } else if ((!has_root_directory() && is_absolute()) || has_filename()) {
        // Our path does not already end in a separator.
        path_ += preferred_separator;
    }

    auto it = rhs.begin();
    if (rhs.has_root_name()) {
        // Skip root name.
        ++it;
    }

    // Add each entry in the path.
    bool first = true;
    while (it != rhs.end()) {
        if (!first && !(!path_.empty() &&
                        path_[path_.length() - 1] == preferred_separator)) {
            path_ += preferred_separator;
        }
        first = false;
        path_ += (*it++).native();
    }
    return *this;
}

path& path::operator/=(const std::string& str) { return append(str); }

path& path::append(const std::string& str) {
    return this->operator/=(path(str));
}

path& path::append(const path& add) { return this->operator/=(add); }

path& path::assign(const path& replacement) {
    path_ = replacement.path_;
    prefix_length_ = replacement.prefix_length_;
    return *this;
}

path& path::assign(const std::string& str) {
    path_ = str;
    process_assigned_path();
    return *this;
}

path path::root_path() const {
    return path(root_name().string() + root_directory().string());
}

path path::root_name() const {
    return path(path_.substr(prefix_length_, root_name_length()));
}

path path::root_directory() const {
    if (has_root_directory()) {
        static const path root_dir(std::string(1, preferred_separator));
        return root_dir;
    }
    return path();
}

path path::relative_path() const {
    return path(path_.substr(std::min(root_path_length(), path_.length())));
}

path path::parent_path() const {
    std::size_t rpl = root_path_length();
    if (path_.length() > rpl) {
        // Path is longer than the root path.
        if (empty()) {
            return path();
        } else {
            auto it = end().decrement(path_.end());
            // Do not remove the root path.
            if (it > path_.begin() + static_cast<int>(rpl) &&
                *it != preferred_separator) {
                --it;
            }
            return path(path_.begin(), it);
        }
    } else {
        // Path is not longer than the root path, so nothing to remove.
        return *this;
    }
}

path path::filename() const {
    return !has_relative_path() ? path() : path(*--end());
}

path path::stem() const {
    auto file = filename().native();
    if (file != "." && file != "..") {
        // This is an actual file.
        auto pos = file.rfind('.');
        if (pos != std::string::npos && pos > 0) {
            // There is a file extension to remove.
            return path(file.substr(0, pos));
        }
    }
    return path(file);
}

path path::extension() const {
    if (has_relative_path()) {
        auto file = filename().native();
        auto pos = file.rfind('.');
        if (pos != std::string::npos && pos > 0) {
            // There is a file extension to return.
            return path(file.substr(pos));
        }
    }
    return path();
}

bool path::has_root_path() const {
    return has_root_name() || has_root_directory();
}

bool path::has_root_name() const { return root_name_length() > 0; }

bool path::has_root_directory() const {
    auto root_length = prefix_length_ + root_name_length();
    return path_.length() > root_length &&
           path_[root_length] == preferred_separator;
}

bool path::has_relative_path() const {
    return root_path_length() < path_.length();
}

bool path::has_parent_path() const { return !parent_path().empty(); }

bool path::has_filename() const {
    return has_relative_path() && !filename().empty();
}

bool path::has_stem() const { return !stem().empty(); }

bool path::has_extension() const { return !extension().empty(); }

bool path::is_absolute() const { return has_root_directory(); }

bool path::is_relative() const { return !is_absolute(); }

std::size_t path::root_path_length() const {
    return prefix_length_ + root_name_length() + (has_root_directory() ? 1 : 0);
}

std::size_t path::root_name_length() const { return 0; }

std::string path::native() const { return path_; }

std::string path::string() const { return native(); }

void path::clear() { path_.clear(); }

path& path::remove_filename() {
    if (has_filename()) {
        path_.erase(path_.size() - filename().path_.size());
    }
    return *this;
}

path& path::replace_filename(const path& replacement) {
    remove_filename();
    return append(replacement);
}

path& path::remove_trailing_separator() {
    if (path_.length() > 0 && path_.back() == preferred_separator) {
        path_.pop_back();
    }
    return *this;
}

path::iterator::iterator() {}

path::iterator::iterator(const path& p, const internal_iterator& pos)
    : first_(p.path_.begin()),
      last_(p.path_.end()),
      prefix_(first_ +
              static_cast<std::string::difference_type>(p.prefix_length_)),
      root_(p.has_root_directory()
                ? first_ + static_cast<std::string::difference_type>(
                               p.prefix_length_ + p.root_name_length())
                : last_),
      iter_(pos) {
    if (pos != last_) {
        update_current();
    }
}

path::iterator::internal_iterator path::iterator::increment(
    const internal_iterator& pos) const {
    internal_iterator it = pos;
    bool from_start = it == first_ || it == prefix_;
    if (it != last_) {
        if (from_start && it == first_ && prefix_ > first_) {
            // If we are at the first_ iterator, we should work with the prefix
            // first.
            it = prefix_;
        } else if (*it++ == preferred_separator) {
            // We can only sit on a separator if it is a network name or a root.
            if (it != last_ && *it == preferred_separator) {
                if (from_start &&
                    !(it + 1 != last_ && (*it + 1) == preferred_separator)) {
                    // Leading double slash detected, treat this unit and the
                    // following unit as one until another slash is detected.
                    it = std::find(it, last_, preferred_separator);
                } else {
                    // Skip redundant slashes.
                    while (it != last_ && *it == preferred_separator) {
                        ++it;
                    }
                }
            }
        } else if (from_start && it != last_ && *it == ':') {
            // Count colon in root name.
            ++it;
        } else {
            // Find next separator.
            it = std::find(it, last_, preferred_separator);
        }
    }
    return it;
}

path::iterator::internal_iterator path::iterator::decrement(
    const internal_iterator& pos) const {
    internal_iterator it = pos;
    if (it != first_) {
        --it;
        // If we are now at the root slash or trailing slash, we are done.
        if (it != root_ && (pos != last_ || *it != preferred_separator)) {
            // If not, look for previous slash.
            it = std::find(std::reverse_iterator<internal_iterator>(it),
                           std::reverse_iterator<internal_iterator>(first_),
                           preferred_separator)
                     .base();
        }

        // Now we must check if this is a network name.
        if (it - first_ == 2 && *first_ == preferred_separator &&
            *(first_ + 1) == preferred_separator) {
            it -= 2;
        }
    }
    return it;
}

void path::iterator::update_current() {
    if ((iter_ == last_) ||
        (iter_ != first_ && iter_ != last_ &&
         (*iter_ == preferred_separator && iter_ != root_) &&
         (iter_ + 1 == last_))) {
        // We are at the end or reaching the end.
        current_.clear();
    } else {
        current_.assign(std::string{iter_, increment(iter_)});
    }
}

path::iterator& path::iterator::operator++() {
    iter_ = increment(iter_);
    // Increment until we reach the end, are not at the root, on a separator,
    // and not the last character.
    while (iter_ != last_ && iter_ != root_ && *iter_ == preferred_separator &&
           (iter_ + 1) != last_) {
        ++iter_;
    }
    update_current();
    return *this;
}

path::iterator path::iterator::operator++(int) {
    auto it = *this;
    ++(*this);
    return it;
}

path::iterator& path::iterator::operator--() {
    iter_ = decrement(iter_);
    update_current();
    return *this;
}

path::iterator path::iterator::operator--(int) {
    auto it = *this;
    --(*this);
    return it;
}

bool path::iterator::operator==(const iterator& rhs) const {
    return iter_ == rhs.iter_;
}

bool path::iterator::operator!=(const iterator& rhs) const {
    return iter_ != rhs.iter_;
}

path::iterator::reference path::iterator::operator*() const { return current_; }

path::iterator::pointer path::iterator::operator->() const { return &current_; }

path::iterator path::begin() const { return iterator(*this, path_.begin()); }
path::iterator path::end() const { return iterator(*this, path_.end()); }

}  // namespace fs
}  // namespace util