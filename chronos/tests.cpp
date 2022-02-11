#include <gmock/gmock.h>

#include "Worker.hpp"
#include "Chronos.hpp"

//1000000000 je vterina

#define TICK_LEN 1000000

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
          std::this_thread::sleep_for(counter * tick_length);
        }
    };

    class TestWorkerPassive : public Worker {
     public:
        int counter;

        TestWorkerPassive(Chronos &main, int tick_count) : counter(tick_count), Worker(main) {};

        void main() override {
          for (int i = 0; i < counter; i++)
            sleep_until();
        }
    };

    class MockPassive : public TestWorkerPassive {
     public:
        MockPassive(Chronos &main, int tick_count) : TestWorkerPassive(main, tick_count) {};
        MOCK_METHOD(void, main, (), (override));
    };


    TEST(Chronos, EmptyChronos) {
      TestChronos god;
      god.run();
      EXPECT_EQ(god.ticks, 0);
    }

    TEST(Chronos, SinglePassive) {
      TestChronos god;
      TestWorkerPassive w1(god, 100);
      god.run();
      EXPECT_EQ(god.ticks, 101);
    }

    TEST(Chronos, SingleActive) {
      TestChronos god;
      TestWorkerActive w1(god, 100);
      god.run();
      //20% time error
      EXPECT_GT(god.ticks, 80);
      EXPECT_LT(god.ticks, 120);
    }


    TEST(Chronos, SingleActiveMock) {
      TestChronos god;
      MockPassive w1(god, 100);
      EXPECT_CALL(w1, main());
      god.run();
    }

//    EXPECT_EQ(f.Bar(input_filepath, output_filepath), 0);


}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}