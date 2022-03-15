#include <gmock/gmock.h>
#include "Worker.hpp"

//1000000000 je vterina

#define TICK_LEN_SHORT 100
#define TICK_LEN 1000000
#define TICK_LEN_LONG 100000000000

using namespace chronos;

//app_duration tick_length(TICK_LEN_SHORT);
app_duration tick_length(TICK_LEN);
app_duration tick_length_long(TICK_LEN_LONG);

class TestChronos : public Chronos {
 public:
    int ticks = 0;

    TestChronos(app_duration tick_l = tick_length) : Chronos(tick_l) {};

    void tick() override {
      ticks++;
      if (get_remaining_time().count() < 0)
        std::cout << "Remaining: " << get_remaining_time().count() << " < 0  - increase TICK_LEN\n";
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

class TestWorkerPassiveSlave : public Worker {
 public:
    int ticks = 0;

    void main() override {
      while (ready()) {
        ticks++;
        sleep_until();
      }
    }
};

class TestWorkerPassiveSlaveZero : public Worker {
 public:
    int ticks = 0;

    void main() override {
      while (ready()) {
        ticks++;
        sleep_until(0);
      }
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
    TestChronos &parent;

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

class TestChronosTime : public Chronos {
 public:
    int ticks = 0;

    TestChronosTime(app_time p_max, app_duration tick_l = tick_length) : Chronos(tick_l, p_max) {};

    void tick() override {
      ticks++;
      EXPECT_EQ(ticks, get_time());
    }
};

class TestWorkerTime : public Worker {
 public:
    int counter = 0;
    int max_tick;
    Chronos &parent;

    TestWorkerTime(int max_tick, Chronos &pparent) : max_tick(max_tick), parent(pparent) {};

    void main() override {
      for (int i = 1; i <= max_tick * 2; i++) {
        EXPECT_EQ(std::min(i, max_tick + 1), parent.get_time());
        sleep_until();
        counter++;
      }
    }
};

TEST(Chronos, TimesGoesOn) {
  app_time g_max_time = 10;
  TestChronosTime god(g_max_time);
  TestWorkerTime w1(g_max_time, god);
  workers_list workers = {&w1};
  god.run(workers);
  EXPECT_EQ(god.ticks, g_max_time + 1);
  god.wait();
  EXPECT_EQ(w1.counter, 2 * g_max_time);
}

TEST(Chronos, TestWorkerPassiveSlave) {
  app_time g_max_time = 10;
  TestChronosTime god(g_max_time);
  TestWorkerPassiveSlave w1;
  workers_list workers = {&w1};
  god.run(workers);
  EXPECT_EQ(god.ticks, g_max_time + 1);
  EXPECT_NO_FATAL_FAILURE(god.wait());
  EXPECT_EQ(w1.ticks, g_max_time + 1);
}

TEST(Chronos, TestWorkerPassiveSlaveZero) {
  app_time g_max_time = 10;
  TestChronosTime god(g_max_time);
  TestWorkerPassiveSlaveZero w1;
  workers_list workers = {&w1};
  god.run(workers);
  EXPECT_EQ(god.ticks, g_max_time + 1);
  EXPECT_NO_FATAL_FAILURE(god.wait());
  EXPECT_EQ(w1.ticks, g_max_time + 1);
}

class TestChronosTimeAsync : public Chronos {
 public:
    int ticks = 0;

    TestChronosTimeAsync(app_time p_max, app_duration tick_l = tick_length_long) : Chronos(tick_l, p_max) {};

    void tick() override {
      ticks++;
      EXPECT_EQ(ticks, get_time());
    }

    int get_int(int val) {
      return async([val]() {
          return val + 5;
      });
    }
};

class TestWorkerAsyncCallEnd : public Worker
{
 public:
    int counter = 0;
    TestChronosTimeAsync &parent;

    TestWorkerAsyncCallEnd(TestChronosTimeAsync &pparent) : parent(pparent) {};

    void main() override {
      while (ready()) {
        counter++;
        std::cout << "main " << counter << " " << parent.get_time() << "\n";
        int ret = parent.get_int(counter);
        std::cout << "async " << counter << " " << parent.get_time() << " " << ret << "\n";
        EXPECT_EQ(ret, counter + 5);
        sleep_until();
      }
      std::cout << "END " << counter << " " << parent.get_time() << "\n";
    }
};

TEST(Chronos, TestWorkerAsyncCallEnd) {
  app_time g_max_time = 10;
  TestChronosTimeAsync god(g_max_time);
  TestWorkerAsyncCallEnd w1(god);
  workers_list workers = {&w1};
  god.run(workers);
  EXPECT_EQ(god.ticks, g_max_time + 1);
  EXPECT_NO_FATAL_FAILURE(god.wait());
  EXPECT_EQ(w1.counter, g_max_time + 1);
}


int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}