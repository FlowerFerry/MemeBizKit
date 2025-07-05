
#ifndef MMBKPP_WRAP_UVW_SUBPROCESS_H_INCLUDED
#define MMBKPP_WRAP_UVW_SUBPROCESS_H_INCLUDED

#include <uvw/loop.h>
#include <uvw/pipe.h>
#include <uvw/async.h>
#include <uvw/process.h>

#include <vector>
#include <optional>
#include <functional>

namespace mmbkpp {
namespace wrap {
namespace uvw {

class subprocess : public std::enable_shared_from_this<subprocess>
{
public:
    using data_callback  = std::function<void(::uvw::data_event  &, subprocess &)>;
    using error_callback = std::function<void(::uvw::error_event &, subprocess &)>;
    using exit_callback  = std::function<void(::uvw::exit_event  &, subprocess &)>;
    using close_callback = std::function<void(::uvw::close_event &, subprocess &)>;

    enum class state {
        STOPPED,
        STOPPING,
        STARTING,
        RUNNING
    };

    subprocess(::uvw::loop::token _token, std::shared_ptr<::uvw::loop> _ref)
        : loop_{ std::move(_ref) }
    {
    }

    ~subprocess()
    {}

    int leak_if(int _err) noexcept {
        if(_err == 0) {
            self_ptr_ = this->shared_from_this();
        }

        return _err;
    }

    void self_reset() noexcept {
        self_ptr_.reset();
    }

    int init()
    {
        async_ = loop_->resource<::uvw::async_handle>();
        async_->on<::uvw::close_event>([this](auto &_event, auto &_handle) {
            __on_async_close(_handle);
        });
        async_->on<::uvw::error_event>([this](auto &_event, auto &_handle) {
            __on_async_error(_event, _handle);
        });
        return leak_if(0);
    }
    
    template<typename _E>
    subprocess &on(std::function<void(_E &, subprocess &)> _cb) noexcept
    {
        if constexpr (std::is_same_v<_E, ::uvw::data_event>) {
            stdout_cb_ = std::move(_cb);
        } 
        else if constexpr (std::is_same_v<_E, ::uvw::error_event>) {
            error_cb_ = std::move(_cb);
        } 
        else if constexpr (std::is_same_v<_E, ::uvw::exit_event>) {
            exit_cb_ = std::move(_cb);
        } 
        else if constexpr (std::is_same_v<_E, ::uvw::close_event>) {
            close_cb_ = std::move(_cb);
        }
        return *this;
    }

    void disable_stdio_inheritance() noexcept
    {
        disable_stdio_inheritance_ = true;
    }
    
    void ready_write_stdin()
    {
        ready_write_stdin_ = true;
    }

    void ready_read_stderr(const data_callback& _cb)
    {
        stderr_cb_ = _cb;
    }

    int kill(int _signum)
    {
        if (state() != state::RUNNING || state() != state::STARTING) {
            return UV_EALREADY;
        }
        
        if (!proc_) {
            return UV_EBADF;
        }
        state_ = state::STOPPING;
        return proc_->kill(_signum);
    }

    int pid() const noexcept
    {
        if (!proc_) {
            return UV_EBADF;
        }
        return proc_->pid();
    }

    state state() const noexcept
    {
        return state_;
    }

    subprocess &cwd(const std::string &_path) noexcept
    {
        cwd_ = _path;
        return *this;
    }

    subprocess &flags(::uvw::process_handle::process_flags _flags) noexcept
    {
        flags_ = _flags;
        return *this;
    }

    subprocess &uid(::uvw::uid_type _id) noexcept
    {
        uid_ = _id;
        return *this;
    }

    subprocess &gid(::uvw::gid_type _id) noexcept
    {
        gid_ = _id;
        return *this;
    }

    int spawn(const char *_file, char **_args, char **_envs = nullptr)
    {
        if (state() != state::STOPPED) {
            return UV_EALREADY;
        }

        if (proc_) {
            return UV_EALREADY;
        }

        state_ = state::STARTING;

        __create_proc();

        auto result = proc_->spawn(_file, _args, _envs);
        if (result == 0) {
            if (out_pipe_) {
                out_pipe_->read();
            }
            if (err_pipe_) {
                err_pipe_->read();
            }

            state_ = state::RUNNING;
        }
        else {
            if (out_pipe_) {
                out_pipe_->stop();
            }

            if (err_pipe_) {
                err_pipe_->stop();
            }

            proc_->close();
            proc_.reset();
            
            if (in_pipe_) {
                in_pipe_->close();
                in_pipe_.reset();
            }

            if (out_pipe_) {
                out_pipe_->close();
                out_pipe_.reset();
            }

            if (err_pipe_) {
                err_pipe_->close();
                err_pipe_.reset();
            }

            state_ = state::STOPPED;
        }
        return result;
    }

    int spawn(const std::string &_file, const std::vector<std::string> &_args = {}, const std::vector<std::string> &_envs = {})
    {
        static char empty_str[] = { '\0' };

        std::vector<char *> c_args;
        for (const auto &arg : _args) {
            c_args.push_back(const_cast<char *>(arg.c_str()));
        }
        if (c_args.empty()) {
            c_args.push_back(empty_str);
        }
        c_args.push_back(nullptr);

        std::vector<char *> c_envs;
        for (const auto &e : _envs) {
            c_envs.push_back(const_cast<char *>(e.c_str()));
        }
        if (!c_envs.empty()) {
            c_envs.push_back(nullptr);
        }

        return spawn(_file.c_str(), c_args.data(), c_envs.empty() ? nullptr : c_envs.data());
    }

    template<typename _ArgIt, typename _EnvIt>
    int spawn(const std::string &_file, _ArgIt _arg_begin, _ArgIt _arg_end, _EnvIt _env_begin, _EnvIt _env_end)
    {
        static char empty_str[] = { '\0' };

        std::vector<char *> c_args;
        for (auto it = _arg_begin; it != _arg_end; ++it) {
            c_args.push_back(const_cast<char *>(it->c_str()));
        }
        if (c_args.empty()) {
            c_args.push_back(empty_str);
        }
        c_args.push_back(nullptr);

        std::vector<char *> c_envs;
        for (auto it = _env_begin; it != _env_end; ++it) {
            c_envs.push_back(const_cast<char *>(it->c_str()));
        }
        if (!c_envs.empty()) {
            c_envs.push_back(nullptr);
        }

        return spawn(_file.c_str(), c_args.data(), c_envs.empty() ? nullptr : c_envs.data());
    }

    template<typename Deleter>
    int write(std::unique_ptr<char[], Deleter> _data, unsigned int _len)
    {
        if (!in_pipe_) {
            return UV_EBADF;
        }
        return in_pipe_->write(std::move(_data), _len);
    }

    int write(char *data, unsigned int len)
    {
        if (!in_pipe_) {
            return UV_EBADF;
        }
        return in_pipe_->write(data, len);
    }

    bool closing() const noexcept {
        return ( in_pipe_ &&  in_pipe_->closing()) ||
               (out_pipe_ && out_pipe_->closing()) ||
               (err_pipe_ && err_pipe_->closing()) ||
               (async_ && async_->closing()) ||
               (proc_  &&  proc_->closing());
    }
    
    void close() noexcept {
        if (async_) {
            async_->close();
        }

    }

private:
    void __create_in_pipe ()
    {
        if (in_pipe_) {
            return; // Already created
        }

        in_pipe_ = loop_->resource<::uvw::pipe_handle>();
        in_pipe_->on<::uvw::error_event>([this](auto &_event, auto &_handle) 
        {
            __on_in_pipe_error(_event, _handle);
        });

        in_pipe_->on<::uvw::close_event>([this](auto &_event, auto &_handle) 
        {
            __on_in_pipe_close(_handle);
        });
    }

    void __create_out_pipe()
    {
        if (out_pipe_) {
            return; // Already created
        }

        out_pipe_ = loop_->resource<::uvw::pipe_handle>();
        out_pipe_->on<::uvw::data_event>([this](auto &_event, auto &_handle) 
        {
            __on_out_pipe_data(_event, _handle);
        });

        out_pipe_->on<::uvw::error_event>([this](auto &_event, auto &_handle) 
        {
            __on_out_pipe_error(_event, _handle);
        });

        out_pipe_->on<::uvw::close_event>([this](auto &_event, auto &_handle) 
        {
            __on_out_pipe_close(_handle);
        });

        out_pipe_->on<::uvw::end_event>([this](auto &_event, auto &_handle) 
        {
            __on_out_pipe_end(_handle);
        });
    }

    void __create_err_pipe()
    {
        if (err_pipe_) {
            return; // Already created
        }

        err_pipe_ = loop_->resource<::uvw::pipe_handle>();
        err_pipe_->on<::uvw::data_event>([this](auto &_event, auto &_handle) 
        {
            __on_err_pipe_data(_event, _handle);
        });

        err_pipe_->on<::uvw::error_event>([this](auto &_event, auto &_handle) 
        {
            __on_err_pipe_error(_event, _handle);
        });

        err_pipe_->on<::uvw::close_event>([this](auto &_event, auto &_handle) 
        {
            __on_err_pipe_close(_handle);
        });

        err_pipe_->on<::uvw::end_event>([this](auto &_event, auto &_handle) 
        {
            __on_err_pipe_end(_handle);
        });
    }

    void __create_proc()
    {
        if (proc_) {
            return; // Already created
        }

        if (ready_write_stdin_) {
            __create_in_pipe();
        }
        if (stdout_cb_) {
            __create_out_pipe();
        }
        if (stderr_cb_) {
            __create_err_pipe();
        }

        proc_ = loop_->resource<::uvw::process_handle>();
        proc_->on<::uvw::error_event>([this](auto &_event, auto &_handle) 
        {
            __on_proc_error(_event, _handle);
        });

        proc_->on<::uvw::exit_event>([this](auto &_event, auto &_handle) 
        {
            __on_proc_exit(_event, _handle);
        });

        proc_->on<::uvw::close_event>([this](auto &_event, auto &_handle) 
        {
            __on_proc_close(_handle);
        });

        proc_->cwd(cwd_);
        proc_->flags(flags_);
        proc_->uid(uid_);
        proc_->gid(gid_);
        proc_->disable_stdio_inheritance();

        if (in_pipe_) {
            proc_->stdio(*in_pipe_, 
                ::uvw::process_handle::stdio_flags::CREATE_PIPE | 
                ::uvw::process_handle::stdio_flags::READABLE_PIPE);
        }

        if (out_pipe_) {
            proc_->stdio(*out_pipe_, 
                ::uvw::process_handle::stdio_flags::CREATE_PIPE | 
                ::uvw::process_handle::stdio_flags::WRITABLE_PIPE);
        }

        if (err_pipe_) {
            proc_->stdio(*err_pipe_, 
                ::uvw::process_handle::stdio_flags::CREATE_PIPE | 
                ::uvw::process_handle::stdio_flags::WRITABLE_PIPE);
        }
    }

    void __on_async_error(::uvw::error_event &_event, ::uvw::async_handle &_handle)
    {
        if (error_cb_) {
            error_cb_(_event, *this);
        }
    }

    void __on_async_close(::uvw::async_handle &_handle)
    {
        async_.reset();

        if (proc_) {
            if (should_kill_process_when_closed_) {
                proc_->kill(SIGKILL);
            }
            proc_->close();
        }

        __on_close();
    }

    void __on_proc_error(::uvw::error_event &_event, ::uvw::process_handle &_handle)
    {
        if (error_cb_) {
            error_cb_(_event, *this);
        }
    }

    void __on_proc_exit(::uvw::exit_event &_event, ::uvw::process_handle &_handle)
    {
        if (proc_.get() != &_handle) {
            return;
        }
        state_ = state::STOPPING;
        exit_event_ = _event;
        proc_->close();
    }

    void __on_proc_close(::uvw::process_handle &_handle)
    {
        if (proc_.get() != &_handle) {
            return;
        }
        proc_.reset();

        if (in_pipe_) {
            in_pipe_->close();
        }
        if (out_pipe_) {
            out_pipe_->stop();
            out_pipe_->close();
        }
        if (err_pipe_) {
            err_pipe_->stop();
            err_pipe_->close();
        }

        __on_exit();
        __on_close();
    }

    void __on_in_pipe_error(::uvw::error_event &_event, ::uvw::pipe_handle &_handle)
    {
        if (error_cb_) {
            error_cb_(_event, *this);
        }
    }

    void __on_in_pipe_close(::uvw::pipe_handle &_handle)
    {
        if (in_pipe_.get() != &_handle) {
            return;
        }

        in_pipe_.reset();
        
        __on_exit();
        __on_close();
    }

    void __on_out_pipe_data(::uvw::data_event &_event, ::uvw::pipe_handle &_handle)
    {
        if (stdout_cb_) {
            stdout_cb_(_event, *this);
        }
    }

    void __on_out_pipe_error(::uvw::error_event &_event, ::uvw::pipe_handle &_handle)
    {
        if (error_cb_) {
            error_cb_(_event, *this);
        }
    }

    void __on_out_pipe_close(::uvw::pipe_handle &_handle)
    {
        if (out_pipe_.get() != &_handle) {
            return;
        }
        out_pipe_.reset();
        
        __on_exit();
        __on_close();
    }

    void __on_out_pipe_end(::uvw::pipe_handle &_handle)
    {
        if (out_pipe_) {
            out_pipe_->stop();
            out_pipe_->close();
        }
    }

    void __on_err_pipe_data(::uvw::data_event &_event, ::uvw::pipe_handle &_handle)
    {
        if (stderr_cb_) {
            stderr_cb_(_event, *this);
            return;
        }
    }

    void __on_err_pipe_error(::uvw::error_event &_event, ::uvw::pipe_handle &_handle)
    {
        if (error_cb_) {
            error_cb_(_event, *this);
        }
    }

    void __on_err_pipe_close(::uvw::pipe_handle &_handle)
    {
        if (err_pipe_.get() != &_handle) {
            return;
        }
        err_pipe_.reset();
        
        __on_exit();
        __on_close();
    }

    void __on_err_pipe_end(::uvw::pipe_handle &_handle)
    {
        if (err_pipe_) {
            err_pipe_->stop();
            err_pipe_->close();
        }

    }

    void __on_close()
    {
        if (async_)
            return;
        if (in_pipe_ || out_pipe_ || err_pipe_)
            return;
        if (proc_)
            return;

        auto self_ptr = this->shared_from_this();
        self_reset();
        if (close_cb_) {
            close_cb_(::uvw::close_event{}, *this);
        }
    }
    
    void __on_exit()
    {
        if (!exit_event_.has_value())
            return;
        if (in_pipe_ || out_pipe_ || err_pipe_)
            return;
        if (proc_)
            return;

        if (exit_cb_) {
            exit_cb_(exit_event_.value(), *this);
        }
        exit_event_.reset();
        state_ = state::STOPPED;
    }

    std::shared_ptr<void> self_ptr_;
    std::shared_ptr<::uvw::loop> loop_;
    std::shared_ptr<::uvw::async_handle> async_;
    std::shared_ptr<::uvw::process_handle> proc_;
    std::shared_ptr<::uvw::pipe_handle> in_pipe_;
    std::shared_ptr<::uvw::pipe_handle> out_pipe_;
    std::shared_ptr<::uvw::pipe_handle> err_pipe_;
    data_callback  stdout_cb_;
    data_callback  stderr_cb_;
    error_callback error_cb_;
    exit_callback  exit_cb_;
    close_callback close_cb_;    
    std::string cwd_;
    enum class state state_ = state::STOPPED;
    ::uvw::process_handle::process_flags flags_ = ::uvw::process_handle::process_flags::_UVW_ENUM;
    std::optional<::uvw::exit_event> exit_event_;
    ::uvw::uid_type uid_ = {0};
    ::uvw::gid_type gid_ = {0};
    bool disable_stdio_inheritance_ = false;
    bool ready_write_stdin_ = false;
    bool should_kill_process_when_closed_ = true;
};

}
}
}

#endif // !MMBKPP_WRAP_UVW_SUBPROCESS_H_INCLUDED

