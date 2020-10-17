#include "EventHandler.h"

namespace engine {

std::unordered_map<ecs::Entity, std::vector<EventId>> EventHandler::callers;
std::unordered_map<EventId, EventFunction> EventHandler::events;

} // namespace engine