#include "SceneView.hpp"
#include "raylib.h" /* 
#include <variant>
int SceneView::run()
{

  const int screenWidth = 800;
  const int screenHeight = 450;

  InitWindow(screenWidth, screenHeight,
             "raylib [core] example - 3d camera mode");

  // Define the camera to look into our 3d world
  Camera3D camera = {0};
  camera.position = (Vector3){0.0f, 10.0f, 10.0f}; // Camera position
  camera.target = (Vector3){0.0f, 0.0f, 0.0f};     // Camera looking at point
  camera.up =
      (Vector3){0.0f, 1.0f, 0.0f}; // Camera up vector (rotation towards target)
  camera.fovy = 45.0f;             // Camera field-of-view Y
  camera.projection = CAMERA_PERSPECTIVE; // Camera mode type

  Vector3 cubePosition = {0.0f, 0.0f, 0.0f};

  SetTargetFPS(60); // Set our game to run at 60 frames-per-second
  //--------------------------------------------------------------------------------------

  // Main game loop
  while (!WindowShouldClose()) // Detect window close button or ESC key
  {
    // Update
    //----------------------------------------------------------------------------------
    // TODO: Update your variables here
    //----------------------------------------------------------------------------------

    // Draw
    //----------------------------------------------------------------------------------
    BeginDrawing();

    ClearBackground(RAYWHITE);

    BeginMode3D(camera);

    DrawCube(cubePosition, 2.0f, 2.0f, 2.0f, RED);
    DrawCubeWires(cubePosition, 2.0f, 2.0f, 2.0f, MAROON);

    DrawGrid(10, 1.0f);

    EndMode3D();

    DrawText("Welcome to the third dimension!", 10, 40, 20, DARKGRAY);

    DrawFPS(10, 10);

    EndDrawing();
    //----------------------------------------------------------------------------------
  }

  // De-Initialization
  //--------------------------------------------------------------------------------------
  CloseWindow(); // Close window and OpenGL context
  //--------------------------------------------------------------------------------------

  return 0;
}
 */
int SceneView::run()
{

  InitWindow(screenWidth, screenHeight,
             "raylib [core] example - 3d camera mode");
  SetTargetFPS(60); // Set our game to run at 60 frames-per-second
  // Define the camera to look into our 3d world

  if (std::holds_alternative<Scene3DSettings>(settings_))
  {
    _run_3d_scene();
  }
  else
  {
    _run_2d_scene();
  }

  return 0;
}
void SceneView::_run_2d_scene()
{
  Camera2D camera = {0};
  Scene2DSettings settings = std::get<Scene2DSettings>(settings_);

  camera.target = {0.0f, 0.0f};
  camera.offset = {screenWidth * 0.5f, screenHeight * 0.5f};
  camera.zoom = screenWidth / (settings.SceneScale);
  camera.rotation = 0.0f;

  float dt = 1.0f / 60.0f;

  while (!WindowShouldClose())
  {
    dt = GetFrameTime();

    // -----------------------------
    // Camera mouse controls
    // -----------------------------

    // Zoom toward mouse position
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f)
    {
      Vector2 mouseScreen = GetMousePosition();
      Vector2 mouseWorldBeforeZoom = GetScreenToWorld2D(mouseScreen, camera);

      // Smooth multiplicative zoom
      float zoomFactor = 1.0f + wheel * 0.1f;
      if (zoomFactor < 0.1f)
        zoomFactor = 0.1f;

      camera.zoom *= zoomFactor;

      // Clamp zoom
      if (camera.zoom < 0.01f)
        camera.zoom = 0.01f;
      if (camera.zoom > 1000.0f)
        camera.zoom = 1000.0f;

      // Keep the world point under the mouse fixed while zooming
      Vector2 mouseWorldAfterZoom = GetScreenToWorld2D(mouseScreen, camera);
      camera.target.x += mouseWorldBeforeZoom.x - mouseWorldAfterZoom.x;
      camera.target.y += mouseWorldBeforeZoom.y - mouseWorldAfterZoom.y;
    }

    // Pan with middle mouse button
    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE))
    {
      Vector2 delta = GetMouseDelta();

      camera.target.x -= delta.x / camera.zoom;
      camera.target.y -= delta.y / camera.zoom;
    }

    // Optional: pan with right mouse button too
    // if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
    // {
    //   Vector2 delta = GetMouseDelta();
    //   camera.target.x -= delta.x / camera.zoom;
    //   camera.target.y -= delta.y / camera.zoom;
    // }

    BeginDrawing();
    ClearBackground(RAYWHITE);

    BeginMode2D(camera);

    totalTime_ += dt;
    // Draw axes
    DrawLineEx({0.0f, 0.0f}, {settings.SceneScale, 0.0f}, 1.0f, RED);
    DrawLineEx({0.0f, 0.0f}, {0.0f, settings.SceneScale}, 1.0f, GREEN);
    if (updateFunc)
    {
      Context ctx(*this, dt, totalTime_, camera);
      updateFunc(ctx);
      if (ctx.ShouldShutdown())
      {
        EndMode2D();
        EndDrawing();
        break;
      }
    }

    EndMode2D();

    DrawFPS(10, 10);

    EndDrawing();
  }

  CloseWindow();
}
void SceneView::_run_3d_scene()
{
  Camera3D camera = {0};
  Scene3DSettings settings = std::get<Scene3DSettings>(settings_);
  const float CameraDistance = settings.SceneScale;
  switch (settings.cameraPosition)
  {

  case CameraPosition::Top:
    camera.position = (Vector3){0.0f, CameraDistance, 0.0f};
    camera.up = (Vector3){0.0f, 0.0f, -1.0f};
    break;
  case CameraPosition::Bottom:
    camera.position = (Vector3){0.0f, -CameraDistance, 0.0f};
    camera.up = (Vector3){0.0f, 0.0f, 1.0f};
    break;
  case CameraPosition::Left:
    camera.position = (Vector3){-CameraDistance, 0.0f, 0.0f};
    camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    break;
  case CameraPosition::Right:
    camera.position = (Vector3){CameraDistance, 0.0f, 0.0f};
    camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    break;
  case CameraPosition::Front:
    camera.position = (Vector3){0.0f, 0.0f, CameraDistance};
    camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    break;
  case CameraPosition::Back:
    camera.position = (Vector3){0.0f, 0.0f, -CameraDistance};
    camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    break;
  case CameraPosition::IsometricNW:
    camera.position =
        (Vector3){-CameraDistance, CameraDistance, CameraDistance};
    camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    break;
  case CameraPosition::IsometricNE:
    camera.position = (Vector3){CameraDistance, CameraDistance, CameraDistance};
    camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    break;
  case CameraPosition::IsometricSW:
    camera.position =
        (Vector3){-CameraDistance, CameraDistance, -CameraDistance};
    camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    break;
  case CameraPosition::IsometricSE:
    camera.position =
        (Vector3){CameraDistance, CameraDistance, -CameraDistance};
    camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    break;
  }
  camera.target = (Vector3){0.0f, 0.0f, 0.0f}; // Camera looking at point
  camera.fovy = 45.0f;                         // Camera field-of-view Y
  camera.projection = (settings.cameraMode == CameraMode::Perspective)
                          ? CAMERA_PERSPECTIVE
                          : CAMERA_ORTHOGRAPHIC; // Camera mode type

  float dt = 1.0f / 60.0f;
  while (!WindowShouldClose()) // Detect window close button or ESC key
  {
    // Update
    //----------------------------------------------------------------------------------
    // TODO: Update your variables here
    //----------------------------------------------------------------------------------

    // Draw
    //----------------------------------------------------------------------------------
    BeginDrawing();

    ClearBackground(RAYWHITE);
    UpdateCamera(&camera, CAMERA_THIRD_PERSON);
    BeginMode3D(camera);
    totalTime_ += dt;
    float mid_scale = settings.SceneScale * 0.5f;
    float axis_width = settings.SceneScale * 0.001f;
    DrawCubeV(Vector3(mid_scale, 0, 0),
              Vector3(settings.SceneScale, axis_width, axis_width), RED);
    DrawCubeV(Vector3(0, mid_scale, 0),
              Vector3(axis_width, settings.SceneScale, axis_width), GREEN);
    DrawCubeV(Vector3(0, 0, mid_scale),
              Vector3(axis_width, axis_width, settings.SceneScale), BLUE);
    // grid spacing is base 10 magnitude of scenee scale.
    float spacing =
        std::pow(10.0f, std::floor(std::log10(settings.SceneScale)) - 1);

    DrawGrid(10, spacing);
    if (updateFunc)
    {
      Context ctx(*this, dt, totalTime_, camera);
      updateFunc(ctx);
      if (ctx.ShouldShutdown())
      {
        break;
      }
    }

    EndMode3D();

    // Draw the axis
    if (UpdatePost3DFunc)
    {
      Context ctx(*this, dt, totalTime_, camera);
      UpdatePost3DFunc(ctx);
      if (ctx.ShouldShutdown())
      {
        break;
      }
    }
    DrawFPS(10, 10);

    EndDrawing();
    //----------------------------------------------------------------------------------
  }

  // De-Initialization
  //--------------------------------------------------------------------------------------
  CloseWindow(); // Close window and OpenGL context
  //------------------------------------------------------------
  // Fake loop example
}
