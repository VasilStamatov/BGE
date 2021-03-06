#pragma once

#include "Task.h"
#include "core/Common.h"

#include <array>
#include <mutex>

namespace bge
{

constexpr uint32 c_NumberOfTasks = 1024u;
constexpr uint32 c_Mask = c_NumberOfTasks - 1;

/**
 * A work stealing queue/dequeue is responsible for storing tasks for a
 * specific thread in a ring buffer, and allowing that thread to pop and push
 * tasks from the back of the queue and also enable the ability to steal tasks
 * from the front of the queue so other threads can take on some work if
 * they're empty. These operations must be threadsafe.
 */
class WorkStealingQueue
{
public:
  WorkStealingQueue();

  /**
   * Push a task to the back
   * @param task pointer to task to push back
   */
  void Push(Task* task); ///< push a task to the back

  /**
   * Pop a task from the back
   * @return pointer to popped task
   */
  Task* Pop();

  /**
   * Steal a task from the front
   * @return pointer to stolen task
   */
  Task* Steal();

private:
  Task* m_Tasks[c_NumberOfTasks]; ///< ring buffer of tasks
  std::mutex m_Mutex;             ///< mutex to keep the operation threadsafe
  uint32 m_Bottom;                ///< "index" to the back task
  uint32 m_Top;                   ///< "index" to the front task
};

} // namespace bge