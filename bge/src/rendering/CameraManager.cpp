#include "rendering/CameraManager.h"

namespace bge
{

uint32 CameraManager::AddPerspective(Vec4i32 viewport, float fovDeg,
                                     float nearVal, float farVal)
{
  m_Projection.emplace_back(GenPerspectiveMat(
      ToRadians(fovDeg), (float)viewport[2] / (float)viewport[3], nearVal,
      farVal));

  m_View.emplace_back(1.0f);
  m_Viewport.emplace_back(std::move(viewport));
  m_FovDegrees.push_back(fovDeg);
  m_Near.push_back(nearVal);
  m_Far.push_back(farVal);
  m_IsOrtho.emplace_back(false);

  return m_Projection.size() - 1;
}

uint32 CameraManager::AddOrthographic(Vec4i32 viewport, float nearVal,
                                      float farVal)
{
  m_Projection.emplace_back(GenOrthoMat((float)viewport[0], (float)viewport[2],
                                        (float)viewport[1], (float)viewport[3],
                                        nearVal, farVal));
  m_View.emplace_back(1.0f);
  m_Viewport.emplace_back(std::move(viewport));
  m_FovDegrees.push_back(0.0f);
  m_Near.push_back(nearVal);
  m_Far.push_back(farVal);
  m_IsOrtho.emplace_back(true);

  return m_Projection.size() - 1;
}

} // namespace bge