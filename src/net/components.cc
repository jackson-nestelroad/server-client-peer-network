#include "components.h"

namespace net {

Components::Components(const program::Options& options)
    : options(options),
      // TODO: Make this an option.
      thread_pool(8),
      temp_file_service(options.temp_directory) {}

}  // namespace net