#include "EventHandler.h"

namespace engine {

std::unordered_map<ecs::Entity, std::vector<detail::EventId>> EventHandler::callers;
std::unordered_map<detail::EventId, detail::EventFunction> EventHandler::events;

} // namespace engine