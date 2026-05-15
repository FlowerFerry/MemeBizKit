
#ifndef MMBKPP_CHRONO_PASSIVE_TIMER_HPP_INCLUDED
#define MMBKPP_CHRONO_PASSIVE_TIMER_HPP_INCLUDED

#include <mego/util/std/time.h>
#include <mego/predef/symbol/likely.h>

#include <thread>
#include <memory>
#include <vector>
#include <chrono>
#include <set>
#include <functional>

#include <megopp/util/scope_cleanup.h>

namespace mmbkpp {
namespace chrono {


	class ticker;

	class passive_timer
	{
		friend class ticker;
	public:
        typedef bool callback_t(passive_timer*, void*);

		static const intptr_t invalid_due_time = INTPTR_MAX;
		static const intptr_t max_due_time = INTPTR_MAX - 1;

		passive_timer() noexcept :
			//ticker_(nullptr),
			isStart_(false),
			isOnce_(false),
			interval_(0),
			count_(0),
			lastTs_(0),
			userdata_(nullptr),
			cb_(nullptr)
		{}

		passive_timer(const std::weak_ptr<ticker>& _ticker) noexcept :
			ticker_(_ticker),
			isStart_(false),
			isOnce_(false),
			interval_(0),
			count_(0),
			lastTs_(0),
			userdata_(nullptr),
			cb_(nullptr)
		{}

        passive_timer(const passive_timer& _other) noexcept;
        passive_timer(passive_timer&& _other) noexcept;

		virtual ~passive_timer();

        passive_timer& operator=(const passive_timer& _other) noexcept;
        passive_timer& operator=(passive_timer&& _other) noexcept;

        virtual inline void set_ticker(const std::weak_ptr<ticker>& _ticker) noexcept { ticker_ = _ticker; }

		inline constexpr void on(callback_t* _cb, void* _u) noexcept
		{
			cb_ = _cb; userdata_ = _u;
		};

        inline constexpr size_t interval() const noexcept { return interval_; }
		
		inline constexpr void set_interval(size_t _ms) noexcept { interval_ = _ms; }

		inline void start(mgu_timestamp_t _curr) noexcept;

		inline void start_once(mgu_timestamp_t _curr) noexcept;

		inline constexpr bool is_start() const noexcept { return isStart_; }
		
        inline constexpr bool is_once() const noexcept { return isOnce_; }

		inline void cancel() noexcept;

		inline void restart(mgu_timestamp_t _curr) noexcept;
		
		inline bool timing_notcall(mgu_timestamp_t _curr)
		{
			if (interval_ < 0 || !isStart_)
				return true;

			if (MG_SYM__UNLIKELY(_curr < lastTs_)) 
			{
				lastTs_ = _curr;
				return true;
			}

			if (MG_SYM__UNLIKELY(count_ < 0))
				count_ = 0;

			count_ += (_curr - lastTs_);
            lastTs_ = _curr;
			return false;
		}
			
		inline bool timing(mgu_timestamp_t _curr, bool* _isDie = nullptr)
		{
			if (timing_notcall(_curr))
				return false;
			
			if (count_ >= interval_)
			{
                if (interval_ > 0 && count_ <= 2 * interval_) 
				{
					count_ %= interval_;
                } else {
                    count_ = 0;
                }

				if (isOnce_) {
					isStart_ = false;
				}
				if (cb_) {
					bool isAlive = cb_(this, userdata_);
					if (_isDie)
						*_isDie = !isAlive;
				}
				return true;
			}

			return false;
		}

		inline intptr_t due_in(mgu_timestamp_t _curr) const noexcept
		{
			if (!is_start())
				return max_due_time;

			if (MG_SYM__UNLIKELY(_curr < lastTs_))
			{
				return 0;
			}

			auto count = count_ + (_curr - lastTs_);
			return interval_ - count;
		}

	private:
		
		std::weak_ptr<ticker> ticker_;
		bool isStart_;
		bool isOnce_;
		intptr_t interval_;
		int64_t count_;
		mgu_timestamp_t lastTs_;
		
        void* userdata_;
        callback_t* cb_;
	};

	class ticker
	{
	public:
		ticker():
            locked_(false)
        {}
		
		inline bool locked() const noexcept { return locked_; }

		void accept(
				passive_timer* _timer, mgu_timestamp_t _curr) noexcept;

		void remove(passive_timer* _timer) noexcept 
		{
            if (locked()) {
                wait_removes_.insert(_timer);
                return;
            }
			
			auto tm_it = std::find(timers_.begin(), timers_.end(), _timer);
			if (tm_it != timers_.end()) {
				timers_.erase(tm_it);
			}

			auto wa_it = std::find(wait_accepts_.begin(), wait_accepts_.end(), _timer);
			if (wa_it != wait_accepts_.end()) {
				wait_accepts_.erase(wa_it);
			}
        }
		
		inline bool wheel_timing(mgu_timestamp_t _curr);

		inline intptr_t due_in(mgu_timestamp_t _curr) const noexcept
		{
			if (timers_.empty())
				return 0;
			
			return timers_.front()->due_in(_curr);
		}

	private:

		inline bool
			remove_and_iteration(std::vector<passive_timer*>::iterator& _it)
		{
			auto rit = wait_removes_.find(*_it);
			if (rit != wait_removes_.end())
			{
				wait_removes_.erase(rit);
				_it = timers_.erase(_it);
				return true;
			}
			
            return false;
		}

		//inline std::vector<passive_timer*>::iterator
		//	internal_accept(
		//		passive_timer* _timer, mgu_timestamp_t _curr) noexcept;

		std::vector<passive_timer*> timers_;
        std::vector<passive_timer*> wait_accepts_;
        std::set<passive_timer*> wait_removes_;
        bool locked_;
	};
	using ticker_ptr = std::shared_ptr<ticker>;

	struct passive_function_timer : public passive_timer
	{
		passive_function_timer() noexcept 
		{
			passive_timer::on(__on_passive_function_timer, this);
		}

		passive_function_timer(const std::weak_ptr<ticker>& _ticker) noexcept 
			: passive_timer(_ticker)
		{
			passive_timer::on(__on_passive_function_timer, this);
		}

		passive_function_timer(const passive_function_timer& _other) 
			: passive_timer(_other), fn_(_other.fn_)
		{
			passive_timer::on(__on_passive_function_timer, this);
		}

		passive_function_timer(passive_function_timer&& _other) noexcept 
			: passive_timer(std::move(_other)), fn_(std::move(_other.fn_))
		{
			passive_timer::on(__on_passive_function_timer, this);
		}

		passive_function_timer& operator=(const passive_function_timer& _other) 
		{
			if (this != &_other) {
				passive_timer::operator=(_other);
				fn_ = _other.fn_;
				passive_timer::on(__on_passive_function_timer, this);
			}
			return *this;
		}

		passive_function_timer& operator=(passive_function_timer&& _other) noexcept 
		{
			if (this != &_other) {
				passive_timer::operator=(std::move(_other));
				fn_ = std::move(_other.fn_);
				passive_timer::on(__on_passive_function_timer, this);
			}
			return *this;
		}

		inline void on(const std::function<bool(passive_function_timer*)>& _fn)
		{
			fn_ = _fn;
		};

	private:

		static inline bool __on_passive_function_timer(passive_timer* _timer, void* _u)
		{
			auto self = reinterpret_cast<passive_function_timer*>(_u);
			if (self->fn_) {
				return self->fn_(self);
			}
			return true;
		}

		std::function<bool(passive_function_timer*)> fn_;
	}; 

	//! @brief Timer interval calculator (Intervalometer) - Based on Cron-aligned mode.
	//! 
	//! This class calculates the wait time (in milliseconds) required for the next timer trigger.
	//! It adopts a "natural time alignment (Cron-style)" strategy, prioritizing strict alignment 
	//! with real-world wall-clock boundaries (top of the minute, hour, or day) rather than 
	//! a simple "current time + absolute interval" approach.
	//! 
	//! [Core Alignment Rules (Anchor & Range)]
	//! The system automatically selects the alignment anchor and range based on the configured 
	//! interval (sec_interval_):
	//!   1. Interval > 1 hour   : Anchored to the "natural day (00:00:00)" with a 24-hour range.
	//!   2. Interval > 1 minute : Anchored to the "natural hour (XX:00:00)" with a 1-hour range.
	//!   3. Interval <= 1 minute: Anchored to the "natural minute (XX:XX:00)" with a 1-minute range.
	//! 
	//! @note [Boundary Truncation Feature]
	//! When the configured interval cannot evenly divide its range (e.g., a 7-minute interval 
	//! within a 60-minute range), a "truncation jump" occurs when crossing the range boundary, 
	//! forcing alignment to the start of the next range. 
	//! This behavior is identical to Linux Crontab (e.g., `*/7 * * * *`).
	//! 
	//! @example Boundary Truncation Example (sec_interval_ = 420 seconds / 7 minutes)
	//! Assuming a start time of 10:00:00:
	//!   - 10:00:00 (Hour alignment)
	//!   - 10:07:00 (7-minute interval)
	//!   - 10:14:00 (7-minute interval)
	//!   - ...
	//!   - 10:49:00 (7-minute interval)
	//!   - 10:56:00 (7-minute interval)
	//!   - 11:00:00 (⚠️ Boundary truncation triggered, interval becomes 4 mins, forced alignment to the next hour)
	//!   - 11:07:00 (7-minute interval resumes)
	//! 
	//! @note [Offset Feature]
	//! Allows adding a fixed offset (sec_offset_) to the aligned anchor points.
	//! @example Offset Example (sec_interval_ = 300s / 5 mins, sec_offset_ = 60s / 1 min)
	//! Trigger times will be: 10:01:00 -> 10:06:00 -> 10:11:00 ...
	//! 
	//! @note [Debounce Tolerance]
	//! Internal calculations include a slight tolerance (e.g., 50ms) to prevent "duplicate 
	//! triggers within the same cycle" caused by the OS timer waking up a few milliseconds early.
	struct intervalometer : protected passive_timer
	{
		typedef bool callback_t(intervalometer*, void*);
		
		intervalometer():
			intervalometer_cb_(nullptr),
			intervalometer_userdata_(nullptr),
			is_stop_(true),
			sec_interval_(1),
			sec_offset_(0)
		{}
		
		intervalometer(const std::weak_ptr<ticker>& _ticker) noexcept: 
			intervalometer_cb_(nullptr),
			intervalometer_userdata_(nullptr),
			passive_timer(_ticker),
			is_stop_(true),
			sec_interval_(1),
			sec_offset_(0)
		{}

		intervalometer(const intervalometer& _other) noexcept :
			passive_timer(_other),
			intervalometer_cb_(_other.intervalometer_cb_),
			intervalometer_userdata_(_other.intervalometer_userdata_),
			is_stop_(_other.is_stop_),
			sec_interval_(_other.sec_interval_),
			sec_offset_(_other.sec_offset_)
		{
			passive_timer::on(on_intervalometer, this);
			
		}

		intervalometer(intervalometer&& _other) noexcept :
			passive_timer(std::move(_other)),
			intervalometer_cb_(std::move(_other.intervalometer_cb_)),
			intervalometer_userdata_(std::move(_other.intervalometer_userdata_)),
			is_stop_(_other.is_stop_),
			sec_interval_(_other.sec_interval_),
			sec_offset_(_other.sec_offset_)
		{
			passive_timer::on(on_intervalometer, this);
			_other.is_stop_ = true;
			_other.intervalometer_cb_ = nullptr;
		}

		intervalometer& operator=(const intervalometer& _other) noexcept
		{
			if (this != &_other) {
				passive_timer::operator=(_other);
				intervalometer_cb_ = _other.intervalometer_cb_;
				intervalometer_userdata_ = _other.intervalometer_userdata_;
				is_stop_ = _other.is_stop_;
				sec_interval_ = _other.sec_interval_;
				sec_offset_ = _other.sec_offset_;
				passive_timer::on(on_intervalometer, this);
			}
			return *this;
		}

		intervalometer& operator=(intervalometer&& _other) noexcept
		{
			if (this != &_other) {
				passive_timer::operator=(std::move(_other));
				intervalometer_cb_ = std::move(_other.intervalometer_cb_);
				intervalometer_userdata_ = std::move(_other.intervalometer_userdata_);
				is_stop_ = _other.is_stop_;
				sec_interval_ = _other.sec_interval_;
				sec_offset_ = _other.sec_offset_;
				passive_timer::on(on_intervalometer, this);

				_other.is_stop_ = true;
				_other.intervalometer_cb_ = nullptr;
			}
			return *this;
		}

		inline void set_repeat(int _sec_interval, int _sec_offset = 0) noexcept
		{
    		if (_sec_interval < 1)
        		return;
			sec_interval_ = _sec_interval;
			
    		if (_sec_offset < 0)
        		return;
    		if (_sec_offset >= _sec_interval)
        		_sec_offset %= _sec_interval;
			sec_offset_ = _sec_offset;
		}

		inline constexpr void on(callback_t* _cb, void* _u) noexcept
		{
			intervalometer_cb_ = _cb; intervalometer_userdata_ = _u;
		};

		inline void start(mgu_timestamp_t _curr) noexcept
		{
			if (is_start())
				return;
			
			is_stop_ = false;
			passive_timer::on(on_intervalometer, this);
			passive_timer::set_interval(__calc_next_ms_interval(_curr));
			passive_timer::start_once(_curr);
		}

		inline void stop() noexcept
		{
			is_stop_ = true;
			passive_timer::cancel();
		}

		inline bool is_start() const noexcept
		{
			return !is_stop_;
		}

        inline bool timing(mgu_timestamp_t _curr, bool* _isDie = nullptr) noexcept
        {
            return passive_timer::timing(_curr, _isDie);
        }

		static inline bool on_intervalometer(
			passive_timer* _timer, void* _u) noexcept
		{
			auto self = reinterpret_cast<intervalometer*>(_u);
			if (self->is_stop_)
				return false;
			
			return self->__on_intervalometer();
		}
	private:
	    /**
		 * @brief Calculates the milliseconds to wait until the next trigger.
		 * @param _curr The current absolute timestamp (usually in milliseconds).
		 * @return int The number of milliseconds until the next trigger. Returns -1 if configuration is invalid.
		 */
		inline int __calc_next_ms_interval(mgu_timestamp_t _curr) const noexcept
		{
			auto sec = std::chrono::seconds(sec_interval_ + sec_offset_);
			if (sec > std::chrono::hours(24))
			{
				return -1;
			}

			std::chrono::seconds range;
			mgu_timestamp_t start_ts;

			if (sec > std::chrono::hours(1))
			{
				start_ts = mgu_timestamp_round_to_day(_curr, mgu_round_down);
				range = std::chrono::hours(24);
			}
			else if (sec > std::chrono::minutes(1))
			{
                start_ts = mgu_timestamp_round_to_hour(_curr, 1, mgu_round_down);
				range = std::chrono::hours(1);
			}
			else {
                start_ts = mgu_timestamp_round_to_minute(_curr, 1, mgu_round_down);
				range = std::chrono::minutes(1);
			}

			auto cumulative = _curr - start_ts;
			auto offset_ms = sec_offset_ * 1000;
			auto interval_ms = sec_interval_ * 1000;

			if (sec_offset_ && cumulative < offset_ms)
				return int(offset_ms - cumulative);

			auto cumulative_next = cumulative - offset_ms;
			cumulative_next = ((cumulative_next + 50) / interval_ms) + 1;
			cumulative_next =   cumulative_next * interval_ms + offset_ms;
			
			auto range_msec = std::chrono::duration_cast<std::chrono::milliseconds>(range).count();
			if (cumulative_next >= range_msec)
			{
				if (range_msec - cumulative <= 50) {
					int next_point = (offset_ms > 0) ? offset_ms : interval_ms;
					return int(range_msec - cumulative + next_point);
				}
				return int(range_msec - cumulative);
			}
			
			return int(cumulative_next - cumulative);
		}

		inline bool __on_intervalometer() noexcept
		{
			if (intervalometer_cb_) {
				auto b = intervalometer_cb_(this, intervalometer_userdata_);
				if (b) {
                    auto ts = mgu_timestamp_get();
                    passive_timer::set_interval(__calc_next_ms_interval(ts));
					passive_timer::start_once(ts);
				}
				return b;
			}
			return true;
		}

		callback_t* intervalometer_cb_;
		void* intervalometer_userdata_;
		int is_stop_;

		/** 
		 * @brief The configured trigger interval (in seconds).
		 * Determines the alignment range (minute-level, hour-level, or day-level).
		 */
		int sec_interval_;

		/** 
		 * @brief The trigger offset (in seconds).
		 * The number of seconds to delay the trigger after the aligned time point.
		 */
		int sec_offset_;
	};

	inline passive_timer::passive_timer(const passive_timer& _other) noexcept :
        ticker_(_other.ticker_),
        isStart_(_other.isStart_),
        isOnce_(_other.isOnce_),
        interval_(_other.interval_),
        count_(_other.count_),
        lastTs_(_other.lastTs_),
        userdata_(_other.userdata_),
        cb_(_other.cb_)
    {
        if (isStart_) {
            auto ticker = ticker_.lock();
            if (ticker) ticker->accept(this, lastTs_);
        }
    }

    inline passive_timer& passive_timer::operator=(const passive_timer& _other) noexcept
    {
        if (this != &_other) {
            cancel();

            ticker_ = _other.ticker_;
            isStart_ = _other.isStart_;
            isOnce_ = _other.isOnce_;
            interval_ = _other.interval_;
            count_ = _other.count_;
            lastTs_ = _other.lastTs_;
            userdata_ = _other.userdata_;
            cb_ = _other.cb_;

            if (isStart_) {
                auto ticker = ticker_.lock();
                if (ticker) ticker->accept(this, lastTs_);
            }
        }
        return *this;
    }

    inline passive_timer::passive_timer(passive_timer&& _other) noexcept :
        ticker_(std::move(_other.ticker_)),
        isStart_(_other.isStart_),
        isOnce_(_other.isOnce_),
        interval_(_other.interval_),
        count_(_other.count_),
        lastTs_(_other.lastTs_),
        userdata_(_other.userdata_),
        cb_(_other.cb_)
    {
        if (isStart_) {
            auto ticker = ticker_.lock();
            if (ticker) {
                ticker->remove(&_other);
                ticker->accept(this, lastTs_);
            }
        }

		_other.isStart_ = false;
        _other.ticker_.reset(); 
    }

    inline passive_timer& passive_timer::operator=(passive_timer&& _other) noexcept
    {
        if (this != &_other) {
            cancel(); 

            ticker_ = std::move(_other.ticker_);
            isStart_ = _other.isStart_;
            isOnce_ = _other.isOnce_;
            interval_ = _other.interval_;
            count_ = _other.count_;
            lastTs_ = _other.lastTs_;
            userdata_ = _other.userdata_;
            cb_ = _other.cb_;

            if (isStart_) {
                auto ticker = ticker_.lock();
                if (ticker) {
                    ticker->remove(&_other);
                    ticker->accept(this, lastTs_);
                }
            }
            _other.isStart_ = false;
            _other.ticker_.reset();
        }
        return *this;
    }

	inline passive_timer::~passive_timer()
	{
		auto ticker = ticker_.lock();
		if (ticker)
			ticker->remove(this);
	}

	inline void passive_timer::start(mgu_timestamp_t _curr) noexcept
	{
		count_ = 0;
		isStart_ = true;
		isOnce_ = false;
		lastTs_ = _curr;

		auto ticker = ticker_.lock();
		if (ticker)
			ticker->accept(this, _curr);
	}

	inline void passive_timer::start_once(mgu_timestamp_t _curr) noexcept
	{
		count_ = 0;
		isStart_ = true;
		isOnce_ = true;
		lastTs_ = _curr;

		auto ticker = ticker_.lock();
		if (ticker)
			ticker->accept(this, _curr);
	}

	inline void passive_timer::cancel() noexcept
	{
		count_ = 0;
		isStart_ = false;
		// isOnce_ = false;

		auto ticker = ticker_.lock();
		if (ticker)
            ticker->remove(this);
	}

	inline void passive_timer::restart(mgu_timestamp_t _curr) noexcept
	{
		count_ = 0; lastTs_ = _curr;

		auto ticker = ticker_.lock();
		if (ticker)
			ticker->accept(this, _curr);
	}

	inline void 
		ticker::accept(
			passive_timer* _timer, mgu_timestamp_t _curr) noexcept
	{
		if (locked()) {
			auto wa_it = std::find(wait_accepts_.begin(), wait_accepts_.end(), _timer);
			if (wa_it == wait_accepts_.end()) {
				wait_accepts_.push_back(_timer);
			}
			wait_removes_.erase(_timer);
			return;
		}

		if (timers_.empty()) {
			timers_.insert(timers_.end(), _timer);
			return;
		}

		for (auto it = timers_.begin(); it != timers_.end();)
		{
			if (*it == _timer) {
				it = timers_.erase(it);
			}
            else {
				if (remove_and_iteration(it))
					continue;

                // (*it)->timing_notcall(_curr);
				++it;
			}
		}

		auto due_time = _timer->due_in(_curr);
		for (auto it = timers_.begin(); it != timers_.end();)
		{
			if (due_time < (*it)->due_in(_curr))
			{
				timers_.insert(it, _timer);
				return;
			} 
			
			++it;
		}
		timers_.insert(timers_.end(), _timer);
	}
	
	inline bool ticker::wheel_timing(mgu_timestamp_t _curr)
	{
		auto ts = _curr;
		bool hasCall = false;
		
		auto accepts_cleanup = megopp::util::scope_cleanup__create([&]
		{
            for (auto t : wait_removes_) {
                remove(t);
            }
            wait_removes_.clear();

			if (!wait_accepts_.empty()) {
				for (auto it = wait_accepts_.begin(); it != wait_accepts_.end(); ++it)
				{
					accept(*it, ts);
				}
				wait_accepts_.clear();
			}
		});

        locked_ = true;
		MEGOPP_UTIL__ON_SCOPE_CLEANUP([&] { locked_ = false; });

        for (auto it = timers_.begin(); it != timers_.end();)
        {
			if (remove_and_iteration(it))
				continue;

			bool isDie = false;
    		bool triggered = false;

			try {
				triggered = (*it)->timing(ts, &isDie);
			} catch (...) {
				
                auto backup = *it;
				timers_.erase(it);
				if (isDie  && !backup->is_once())
					backup->isStart_ = false;
				if (!isDie && !backup->is_once())
				{
					auto wa_it = std::find(wait_accepts_.begin(), wait_accepts_.end(), backup);
					if (wa_it == wait_accepts_.end()) {
						wait_accepts_.push_back(backup);
					}
				}
				throw;
			}

            if (triggered) {
				if (remove_and_iteration(it)) 
				{
					ts = mgu_timestamp_get();
					hasCall = true;
					continue;
				}

                auto backup = *it;
				it = timers_.erase(it);
				if (isDie  && !backup->is_once())
					backup->isStart_ = false;
				if (!isDie && !backup->is_once()) 
				{
					auto wa_it = std::find(wait_accepts_.begin(), wait_accepts_.end(), backup);
					if (wa_it == wait_accepts_.end()) {
						wait_accepts_.push_back(backup);
					}
				}

				ts = mgu_timestamp_get();
				hasCall = true;
            }
			else {
				++it;
			}
        }

		std::this_thread::yield();
		return hasCall;
	}

};
};

#endif // !MMBKPP_CHRONO_PASSIVE_TIMER_HPP_INCLUDED
