#include "EventHandler.h"

namespace engine {

std::unordered_map<ecs::Entity, std::vector<EventHandler::EventId>> EventHandler::callers_;
std::unordered_map<EventHandler::EventId, internal::EventFunction> EventHandler::events_;

} // namespace engine