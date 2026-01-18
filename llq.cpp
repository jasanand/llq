// STD libs
#include <iostream>
#include <syncstream>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>

#include "utils.h"

// decide which queue to use
//using TASK_QUEUE = queue::Queue<TradePtr>;
using TASK_QUEUE = queue::CircularQueue<TradePtr>;

template <typename T>
class Thread
{
public:
   Thread(const std::string& id):jthread_(), id_(id) {}
   Thread(const Thread&) = delete;
   Thread(Thread&&) = delete;
   Thread operator=(const Thread&) = delete;
   Thread operator=(Thread&&) = delete;

   bool run()
   {
      if (jthread_)
      {
         std::cerr << "Thread is already running!!" << std::endl;
         return false;
      }

      jthread_.reset(new std::jthread([&](std::stop_token stop) 
                     { static_cast<T*>(this)->runImpl(stop); }));
      return true;
   }

   void stop()
   {
      if (jthread_)
      {
         jthread_->request_stop();
         // maybe wait for a bit...
         //std::this_thread::sleep_for(std::chrono::seconds(3));
         join();
      }
   }

   void join()
   {
      if (jthread_)
      {
         jthread_->join();
         jthread_.reset();
      }
   }

   // join in destructor
   ~Thread()
   {
      join();
   }

protected:
   std::unique_ptr<std::jthread> jthread_;
   std::string id_;
};

// Dummy producer to produce trades.
class Exchange : public Thread<Exchange>
{
public:
   Exchange(const std::string& ric, double priceLow, int volumeLow, double vola, TASK_QUEUE& queue, int max)
   : Thread<Exchange>(ric), 
     ric_(ric), 
     rand_price_(priceLow, priceLow*(1.0+vola)), 
     rand_volume_(volumeLow, volumeLow*(1.0+vola)), 
     queue_(queue),
     max_(max)
   {
   }

private:
   friend class Thread<Exchange>;
   void runImpl(std::stop_token stop)
   {
      for (int i = 1; i <= this->max_ && !stop.stop_requested(); ++i)
      //for (int i = 1; i <= this->max_; ++i)
      {
         TradePtr trade = Traits::create(ric_,rand_price_(),rand_volume_());
         // note in case we are using unique_ptr we lose ownership after passing it to produce
         queue_.enqueue(trade);
      }
#ifndef BENCHMARK_LLQ
      std::osyncstream (std::cout) << "Exchange Thread: " << ric_ << ", Total Produced: " << this->max_ << std::endl;
#endif
   }

private:
   std::string ric_;
   Rand<double> rand_price_;
   Rand<int> rand_volume_;
   TASK_QUEUE& queue_;
   int max_;
};

// Dummy consumer to consume trades.
class Strategy: public Thread<Strategy>
{
public:
   Strategy(const std::string& strategy_id, TASK_QUEUE& queue, int max)
   : Thread<Strategy>(strategy_id), 
     strategy_id_(strategy_id), queue_(queue), max_(max)
   {
   }

private:
   friend class Thread<Strategy>;

   // Consumes trades/market data from exchanges
   void runImpl(std::stop_token stop)
   {
      int counter = 0;
      while(!stop.stop_requested())
      //while(true)
      {
         TradePtr trade;
         if (queue_.dequeue(trade))
         {
            ++counter;

#ifndef BENCHMARK_LLQ
            //std::osyncstream (std::cout) << "Strategy: " << strategy_id_
            //<< ", Ric: " << trade->ric_ << ", Price: " << trade->price_ <<  std::endl;
#endif

            // not required if std::unique_ptr
            if constexpr(!Traits::is_unique_ptr(trade))
            {
               Traits::destroy(trade);
            }

         }
         // in live mode we keep spinning
         if (counter >= this->max_)
            break;
      }

#ifndef BENCHMARK_LLQ
      std::osyncstream (std::cout) << "Strategy: " << strategy_id_ << ", Total Processed: " << counter << std::endl;
#endif
   }

private:

   std::string strategy_id_;
   TASK_QUEUE& queue_;
   int max_;
};

void run_main()
{
   // Set 1 TASK_QUEUE
   //TASK_QUEUE task_queue_1; // unbounded queue, 
   TASK_QUEUE task_queue_1(1000000); // bounded 1000000 circular queue, 

   // Set 2 TASK_QUEUE
   //TASK_QUEUE task_queue_2; // unbounded queue
   TASK_QUEUE task_queue_2(1000000); // bounded 1000000 circular queue

   {
      double vola = 0.10; // 10%

      // Set 1 Exchanges and Strategy sink
      Exchange exchange1 {"MSFTO.O", 490.0, 10000, vola, task_queue_1, 2000000};
      Exchange exchange2 {"AAPL.OQ", 230.0, 15000, vola, task_queue_1, 1000000};
      Strategy strategy1 {"S1", task_queue_1, 3000000};

      // Set 2 Exchanges and Strategy sink
      Exchange exchange3 {"NVDA.O" , 174.8, 20000, vola, task_queue_2, 1000000};
      Exchange exchange4 {"META.O" , 724.5, 21000, vola, task_queue_2, 1000000};
      Strategy strategy2 {"S2", task_queue_2, 2000000};

      exchange1.run();
      exchange2.run();
      strategy1.run();

      exchange3.run();
      exchange4.run();
      strategy2.run();

      //std::this_thread::sleep_for(std::chrono::seconds(3));

   } // all threads join on destruction, no need for explicit join
}

#ifndef BENCHMARK_LLQ

int main()
{
   run_main();
   return 0;
}

#else

#include <benchmark/benchmark.h>
static void BENCHMARK_llq(benchmark::State& state)
{
   for (auto _ : state)
   {
      run_main();
   }
}

BENCHMARK(BENCHMARK_llq)->Iterations(1)->Repetitions(5)->Unit(benchmark::kMillisecond);
BENCHMARK_MAIN();

#endif
