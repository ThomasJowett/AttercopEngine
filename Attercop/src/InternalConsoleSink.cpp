#include "InternalConsoleSink.hpp"

namespace atcp {
uint16_t InternalConsole::s_MessageBufferCapacity = 256;
uint16_t InternalConsole::s_MessageBufferSize = 0;
uint16_t InternalConsole::s_MessageBufferBegin = 0;
std::vector<InternalConsole::Message> InternalConsole::s_MessageBuffer(s_MessageBufferCapacity);
}
