#pragma once

#include "core/Common.h"
#include "events/Event.h"

#include <memory>
#include <string>

namespace bge
{

/**
 * Window Creation data
 */
struct WindowData
{
  std::string m_Title;
  uint32 m_Width;
  uint32 m_Height;
  bool m_VSync;

  WindowData(std::string title = "BGE Engine", uint32 width = 1280,
             uint32 height = 720, bool vsync = false)
      : m_Title(std::move(title))
      , m_Width(width)
      , m_Height(height)
      , m_VSync(vsync)
  {
  }
};

/**
 * The game window class
 */
class Window
{
  using EventCallbackFn = std::function<void(Event&)>;

public:
  Window();
  ~Window();

  DELETE_COPY_AND_ASSIGN(Window);

  /**
   * Create/Start the window
   * @param data window creation data
   */
  void Create(const WindowData& data);

  /**
   * Destroy the window
   */
  void Destroy();

  // Tick called every frame
  void OnTick();

  // Setters
  void SetEventCallback(const EventCallbackFn& callback);
  void SetVSync(bool enabled);
  void SetCursor(bool enabled);

  // Getters
  bool IsVSync() const;
  uint32 GetWidth() const;
  uint32 GetHeight() const;
  void* GetNativeWindow() const;

private:
  class Impl;

  std::unique_ptr<Impl> m_impl; /**< Private implementation */
};

} // namespace bge
