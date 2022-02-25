#include <gmock/gmock.h>

#include "Worker.hpp"

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

        int get_int(int val) {
          return async([val]() {
            std::cout << "Evaled lambda";
            return val + 5;
          });
        }

        std::string get_string(std::string s1, std::string s2) {
          return async([s1, s2]() {
              return "string: " + s1 + s2;
          });
        }
    };


    class TestWorkerActive : public Worker {
     public:
        int counter;

        TestWorkerActive(int tick_count) : counter(tick_count) {};

        void main() override {
          std::this_thread::sleep_for(counter * tick_length);
        }
    };

    class TestWorkerPassive : public Worker {
     public:
        int counter;

        TestWorkerPassive(int tick_count) : counter(tick_count) {};

        void main() override {
          for (int i = 0; i < counter; i++)
            sleep_until();
        }
    };

    class TestWorkerCrash : public Worker {
     public:
        void main() override {
          throw "Something bad happened";
        }
    };

    class TestWorkerAsync : public Worker {
     public:
        int num;
        TestChronos & parent;
        TestWorkerAsync(TestChronos &main) : parent(main) {};
        void main() override {
          num = 0;
          num = parent.get_int(5);
        }
    };


    class MockPassive : public TestWorkerPassive {
     public:
        MockPassive(int tick_count) : TestWorkerPassive(tick_count) {};
        MOCK_METHOD(void, main, (), (override));
    };


    TEST(Chronos, EmptyChronos) {
      TestChronos god;
      workers_list workers;
      god.run(workers);
      EXPECT_EQ(god.ticks, 0);
    }

    TEST(Chronos, SinglePassive) {
      TestChronos god;
      TestWorkerPassive w1(100);
      workers_list workers = {&w1};
      god.run(workers);
      EXPECT_EQ(god.ticks, 101);
    }

    TEST(Chronos, SingleActive) {
      TestChronos god;
      TestWorkerActive w1(100);
      workers_list workers = {&w1};
      god.run(workers);
      //20% time error
      EXPECT_GT(god.ticks, 80);
      EXPECT_LT(god.ticks, 120);
    }


    TEST(Chronos, SingleActiveMock) {
      TestChronos god;
      MockPassive w1(100);
      workers_list workers = {&w1};
      EXPECT_CALL(w1, main());
      god.run(workers);
    }

    TEST(Chronos, TestWorkerCrash) {
      TestChronos god;
      TestWorkerCrash w1;
      workers_list workers = {&w1};
      ASSERT_NO_THROW(god.run(workers));
    }

    TEST(Chronos, AsyncChronos) {
      TestChronos god;
      TestWorkerAsync w1(god);
      workers_list workers = {&w1};
      god.run(workers);
      EXPECT_EQ(w1.num, 10);
    }


//    EXPECT_EQ(f.Bar(input_filepath, output_filepath), 0);


}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}