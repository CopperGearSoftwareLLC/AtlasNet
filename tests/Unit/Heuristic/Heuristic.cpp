
#include "DebugView.hpp"
#include "atlasnet/core/geometry/AABB.hpp"
#include <gtest/gtest.h>
#include "KDTree.hpp"
TEST(HEURISTIC, DebugView)
{
  DebugView view(DebugView::Scene3DSettings{.SceneScale = 5.0f});
  view.on_update(
          [](DebugView::Context& ctx)
          {
            std::cerr << "Total time: " << ctx.totalTime << std::endl;
            ctx.timer("testtimer", 2.3f)
                .on_repeating_active(
                    [&]()
                    {
                      std::cerr << "Drawing box at time " << ctx.totalTime
                                << std::endl;
                      AtlasNet::AABB3f box({-1.0f, -1.0f, -1.0f},
                                           {1.0f, 1.0f, 1.0f});
                      AtlasNet::AABB3f box2({-0.2, 0.2, 0.1}, {0.2, 0.4, 0.3});
                      ctx.DrawAABB(box, DebugView::DrawMode::Wireframe, BLUE);
                      ctx.DrawAABB(box2, DebugView::DrawMode::Wireframe, RED);
                    });

            ctx.timer("shutdown", 5.0f)
                .on_first_active([&]() { ctx.Shutdown(); });
            // Your update logic here, using ctx.deltaTime and ctx.totalTime
          })
      .run();
  EXPECT_TRUE(true);
}
int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}