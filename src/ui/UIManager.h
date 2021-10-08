#pragma once

namespace ptgn {

namespace interfaces {

class UIManager {

};

} // namespace interface

namespace services {

interfaces::UIManager& GetUIManager();

} // namespace services

} // namespace ptgn