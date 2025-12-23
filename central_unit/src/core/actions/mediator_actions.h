#pragma once
#include "actions.h"

#include <string_view>
#include <memory>

namespace SmartHome {
    class Actions;

    class MediatorActions {
    public:

        /**
         * @brief
         *
         * @note Message must be in JSON RPC format with `params` field formated as follows:
         *       \code{.json}
         *       {
         *         "target": "mediator",
         *         "method_params": [
         *           {"module_id": <uint>}, // Object with a module id, signalizes that command's target is a module.
         *                                  // TODO implement execute command for mediator process.
         *           <string>,  // String representing an action to execute
         *           <optional> // Optional argument of type depending on what is expected by action
         *         ]
         *       }
         *       \endcode
         */
        static ba::awaitable<API::ApiResponse> mediatorExecuteHandler(
            const std::shared_ptr<Actions::CommandMetadata> &commandMetadata);
    };
}
