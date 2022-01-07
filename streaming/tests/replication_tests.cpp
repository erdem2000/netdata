
#include <gtest/gtest.h>
extern "C" {
  //#include "../replication.h"
}


TEST(ReplicationTests, BasicAssertions) {

  EXPECT_STRNE("hello", "hello");

  EXPECT_EQ(7 * 6, 42);
}

TEST(ReplicationTests, BasicAssertions2) {

  EXPECT_STRNE("hello", "world");

  EXPECT_EQ(7 * 6, 42);
}