#include <gmock/gmock.h>

#include "Worker.hpp"
#include "Chronos.hpp"

#define TICK_LEN 1000

namespace chronos {

    app_duration tick_length(TICK_LEN);

    class TestChronos : public Chronos {
     public:
        int ticks = 0;

        TestChronos() : Chronos(tick_length) {};

        void tick() override {
          ticks++;
        }
    };


    class TestWorkerActive : public Worker {
     public:
        int counter;

        TestWorkerActive(Chronos &main, int tick_count) : counter(tick_count), Worker(main) {};

        void main() override {
          std::this_thread::sleep_for(counter * TICK_LEN * tick_length);
        }
    };

    class TestWorkerPassive : public Worker {
     public:
        int counter;

        TestWorkerPassive(Chronos &main, int tick_count) : counter(tick_count), Worker(main) {};

        void main() override {
          for (int i = 0; i < counter; i++)
            sleep_until(1);
        }
    };

    class MockPassive : public TestWorkerPassive {
     public:
        MockPassive(Chronos &main, int tick_count):TestWorkerPassive(main,tick_count){};
        MOCK_METHOD(void, main, (), (override));
    };


    TEST(Chronos, EmptyChr) {
      TestChronos god;
      god.run();
      EXPECT_EQ(god.ticks, 0);
    }

    TEST(Chronos, SingleActive) {
      TestChronos god;
      MockPassive w1(god, 100);
      EXPECT_CALL(w1, main());
      god.run();
      EXPECT_EQ(god.ticks, 100);
    }

//    EXPECT_EQ(f.Bar(input_filepath, output_filepath), 0);


}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}