#pragma once

#include "atlasnet/core/geometry/AABB.hpp"
#include "atlasnet/core/geometry/Vec.hpp"
#include "raylib.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>

class DebugView
{

  struct TimerState
  {
    bool wasActiveLastFrame = false;
  };

public:
  enum class CameraMode
  {
    Perspective,
    Orthographic
  };
  enum class CameraPosition
  {
    Top,
    Bottom,
    Left,
    Right,
    Front,
    Back,
    IsometricNW,
    IsometricNE,
    IsometricSW,
    IsometricSE
  };
  struct Scene3DSettings
  {

    float SceneScale = 2.5f;
    CameraMode cameraMode = CameraMode::Perspective;
    CameraPosition cameraPosition = CameraPosition::IsometricNE;
  };
  struct Scene2DSettings
  {
    float SceneScale = 2.5f;
  };
  enum class DrawMode
  {
    Wireframe,
    Solid
  };
  class TimerHandle
  {
    DebugView& view;
    TimerState& state;
    bool active;

  public:
    TimerHandle(DebugView& v, TimerState& s, bool isActive)
        : view(v), state(s), active(isActive)
    {
    }

    TimerHandle& on_first_active(const std::function<void()>& func)
    {
      if (active && !state.wasActiveLastFrame)
      {
        func();
      }
      return *this;
    }

    TimerHandle& on_repeating_active(const std::function<void()>& func)
    {
      if (active)
      {
        func();
      }
      return *this;
    }

    TimerHandle& on_inactive(const std::function<void()>& func)
    {
      if (!active)
      {
        func();
      }
      return *this;
    }

    ~TimerHandle()
    {
      state.wasActiveLastFrame = active;
    }
  };

  class Context
  {
    std::variant<Camera3D, Camera2D> camera;
    DebugView& view;
    bool _ShouldShutdown = false;

  public:
    float deltaTime = 0.0f;
    float totalTime = 0.0f;

    Context(DebugView& v, float dt, float tt,
            std::variant<Camera3D, Camera2D> cam)
        : view(v), deltaTime(dt), totalTime(tt), camera(cam)
    {
    }

    TimerHandle timer(const std::string& id, float startTime)
    {
      auto& state = view.timers[id];
      bool active = totalTime >= startTime;
      return TimerHandle(view, state, active);
    }

    TimerHandle timer(const std::string& id, float startTime, float duration)
    {
      auto& state = view.timers[id];
      bool active =
          (totalTime >= startTime) && (totalTime < startTime + duration);
      return TimerHandle(view, state, active);
    }
    void Shutdown()
    {
      _ShouldShutdown = true;
    }
    bool ShouldShutdown() const
    {
      return _ShouldShutdown;
    }
    void DrawAABB(AtlasNet::AABB3f box, DrawMode mode = DrawMode::Solid,
                  Color color = RED)
    {
      DrawAABB<3, float>(box, mode, color);
    }
    void DrawAABB(AtlasNet::AABB2f box, DrawMode mode = DrawMode::Solid,
                  Color color = RED)
    {
      DrawAABB<2, float>(box, mode, color);
    }
    template <size_t Dim, typename Type>
    void DrawAABB(AtlasNet::AABB<Dim, Type> box,
                  DrawMode mode = DrawMode::Solid, Color color = RED)
    {
      if constexpr (Dim == 3)
      {
        Vector3 center = {(box.min[0] + box.max[0]) / 2.0f,
                          (box.min[1] + box.max[1]) / 2.0f,
                          (box.min[2] + box.max[2]) / 2.0f};
        Vector3 size = {box.max[0] - box.min[0], box.max[1] - box.min[1],
                        box.max[2] - box.min[2]};
        if (mode == DrawMode::Solid)
          DrawCube(center, size.x, size.y, size.z, color);
        else
        {
          DrawCubeWires(center, size.x, size.y, size.z, color);
          for (int i = 0; i < 8; i++)
          {
            // draw a sphere at each corner
            Vector3 corner = {i & 1 ? box.max[0] : box.min[0],
                              i & 2 ? box.max[1] : box.min[1],
                              i & 4 ? box.max[2] : box.min[2]};
            DrawSphere(corner, 0.05f, color);
          }
        }
      }
      else if constexpr (Dim == 2)
      {
        Vector2 center = {(box.min[0] + box.max[0]) / 2.0f,
                          (box.min[1] + box.max[1]) / 2.0f};
        Vector2 size = {box.max[0] - box.min[0], box.max[1] - box.min[1]};
        if (mode == DrawMode::Solid)
          DrawRectangleV(center, size, color);
        else
          DrawRectangleLines(center.x - size.x / 2, center.y - size.y / 2,
                             size.x, size.y, color);
      }
    }
    vec2 world_to_screen(const vec3& worldPos)
    {
      Vector2 screenPos = GetWorldToScreen({worldPos.x, worldPos.y, worldPos.z},
                                           std::get<Camera3D>(camera));
      return {screenPos.x, screenPos.y};
    }
  };

  DebugView& on_update(std::function<void(Context&)> func)
  {
    updateFunc = std::move(func);
    return *this;
  }
  DebugView& on_update_post_3d(std::function<void(Context&)> func)
  {
    UpdatePost3DFunc = std::move(func);
    return *this;
  }

  DebugView(Scene2DSettings settings) : settings_(std::move(settings)) {}
  DebugView(Scene3DSettings settings) : settings_(std::move(settings)) {}
  int run();

private:
  void _run_2d_scene();
  void _run_3d_scene();
  std::variant<Scene3DSettings, Scene2DSettings> settings_;

  std::function<void(Context&)> updateFunc, UpdatePost3DFunc;
  std::unordered_map<std::string, TimerState> timers;
  float totalTime_ = 0.0f;
  const int screenWidth = 1280;
  const int screenHeight = 1280;
};