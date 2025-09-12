#pragma once

class God {
   public:
    God();
    ~God();

    bool spawnPartition(int32 id, int32 port);
    bool cleanupPartitions();

   private:
};