#include "util/Timer.h"

namespace bge
{

Timer::Timer()
    : m_Start(TimePoint::clock::now())
{
}

void Timer::Renew() { m_Start = TimePoint::clock::now(); }

float Timer::GetElapsedMilli() const
{
  using Milli = std::chrono::duration<float, std::milli>;

  return std::chrono::duration_cast<Milli>((TimePoint::clock::now() - m_Start))
      .count();
}

float Timer::GetElapsedSeconds() const { return GetElapsedMilli() / 1000.0f; }

} // namespace bge