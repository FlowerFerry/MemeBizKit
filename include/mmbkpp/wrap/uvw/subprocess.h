
#ifndef MMBKPP_WRAP_UVW_SUBPROCESS_H_INCLUDED
#define MMBKPP_WRAP_UVW_SUBPROCESS_H_INCLUDED

#include <uvw/process.h>
#include <uvw/loop.h>
#include <uvw/pipe.h>

#include <vector>
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

    subprocess(::uvw::loop::token _token, std::shared_ptr<::uvw::loop> _ref)
        : proc_hdl_(std::make_shared<::uvw::process_handle>(_token, std::move(_ref))) 
    {
        proc_hdl_->on<::uvw::error_event>([this](auto &_event, auto &_handle) {
            if (error_cb_) {
                error_cb_(_event, *this);
            }
        });

        proc_hdl_->on<::uvw::exit_event>([this](auto &_event, auto &_handle) {
            if (exit_cb_) {
                exit_cb_(_event, *this);
            }
        });

        proc_hdl_->on<::uvw::close_event>([this](auto &_event, auto &_handle) {
            if (close_cb_) {
                close_cb_(_event, *this);
            }

            proc_hdl_.reset();
            if (!in_pipe_ && !out_pipe_ && !err_pipe_) 
            {
                // If all pipes are not set, reset the subprocess instance
                // to allow it to be reused.
                self_reset();
            } 
        });
    }

    ~subprocess()
    {}

    int init()
    {
        return proc_hdl_->init();
    }
    
    int leak_if(int _err) noexcept {
        if(_err == 0) {
            self_ptr_ = this->shared_from_this();
        }

        return _err;
    }

    void self_reset() noexcept {
        self_ptr_.reset();
    }

    template<typename _E>
    subprocess &on(std::function<void(_E &, subprocess &)> _cb) noexcept
    {
        if constexpr (std::is_same_v<_E, ::uvw::data_event>) {
            data_cb_  = std::move(_cb);
            ready_read_stdout();
        } 
        else if constexpr (std::is_same_v<_E, ::uvw::error_event>) {
            error_cb_ = std::move(_cb);
        } 
        else if constexpr (std::is_same_v<_E, ::uvw::exit_event>) {
            exit_cb_  = std::move(_cb);
        } 
        else if constexpr (std::is_same_v<_E, ::uvw::close_event>) {
            close_cb_ = std::move(_cb);
        }
        return *this;
    }

    void disable_stdio_inheritance() noexcept
    {
        proc_hdl_->disable_stdio_inheritance();
    }
    
    void ready_write_stdin()
    {
        if (!in_pipe_) {
            in_pipe_ = proc_hdl_->parent().resource<::uvw::pipe_handle>();
            in_pipe_->on<::uvw::error_event>([this](auto &_event, auto &_handle) {
                if (error_cb_) {
                    error_cb_(_event, *this);
                }
            });

            in_pipe_->on<::uvw::close_event>([this](auto &_event, auto &_handle) {
                if (close_cb_) {
                    close_cb_(_event, *this);
                }
                
                in_pipe_.reset();
                if (!out_pipe_ && !err_pipe_ && !proc_hdl_) 
                    self_reset();
                
            });

            proc_hdl_->stdio(*in_pipe_, 
                ::uvw::process_handle::stdio_flags::CREATE_PIPE | 
                ::uvw::process_handle::stdio_flags::READABLE_PIPE);
        }
    }

    void ready_read_stdout()
    {
        if (!out_pipe_) {
            out_pipe_ = proc_hdl_->parent().resource<::uvw::pipe_handle>();
            out_pipe_->on<::uvw::data_event>([this](auto &_event, auto &_handle) {
                if (stdout_cb_) {
                    stdout_cb_(_event, *this);
                }
            });

            out_pipe_->on<::uvw::error_event>([this](auto &_event, auto &_handle) {
                if (error_cb_) {
                    error_cb_(_event, *this);
                }
            });

            out_pipe_->on<::uvw::close_event>([this](auto &_event, auto &_handle) {
                if (close_cb_) {
                    close_cb_(_event, *this);
                }
                
                out_pipe_.reset();
                if (!in_pipe_ && !err_pipe_ && !proc_hdl_) 
                    self_reset();
            });

            proc_hdl_->stdio(*out_pipe_, 
                ::uvw::process_handle::stdio_flags::CREATE_PIPE | 
                ::uvw::process_handle::stdio_flags::WRITABLE_PIPE);
        }
    }

    void ready_read_stderr(const data_callback& _cb)
    {
        stderr_cb_ = _cb;

        if (!err_pipe_) {
            
            err_pipe_ = proc_hdl_->parent().resource<::uvw::pipe_handle>();
            err_pipe_->on<::uvw::data_event>([this](auto &_event, auto &_handle) {
                if (stderr_cb_) {
                    stderr_cb_(_event, *this);
                    return;
                }
                if (stdout_cb_) {
                    stdout_cb_(_event, *this);
                }
            });

            err_pipe_->on<::uvw::error_event>([this](auto &_event, auto &_handle) {
                if (error_cb_) {
                    error_cb_(_event, *this);
                }
            });
            
            err_pipe_->on<::uvw::close_event>([this](auto &_event, auto &_handle) {
                if (close_cb_) {
                    close_cb_(_event, *this);
                }
                
                err_pipe_.reset();
                if (!in_pipe_ && !out_pipe_ && !proc_hdl_) 
                    self_reset();
            });

            proc_hdl_->stdio(*err_pipe_, 
                ::uvw::process_handle::stdio_flags::CREATE_PIPE | 
                ::uvw::process_handle::stdio_flags::WRITABLE_PIPE);
        }
    }

    int kill(int _signum)
    {
        return proc_hdl_->kill(_signum);
    }

    int pid() noexcept
    {
        return proc_hdl_->pid();
    }

    subprocess &cwd(const std::string &_path) noexcept
    {
        proc_hdl_->cwd(_path);
        return *this;
    }

    subprocess &flags(::uvw::process_handle::process_flags _flags) noexcept
    {
        proc_hdl_->flags(_flags);
        return *this;
    }

    subprocess &uid(::uvw::uid_type _id) noexcept
    {
        proc_hdl_->uid(_id);
        return *this;
    }

    subprocess &gid(::uvw::gid_type _id) noexcept
    {
        proc_hdl_->gid(_id);
        return *this;
    }

    int spawn(const char *_file, char **_args, char **_envs = nullptr)
    {
        if (stdout_cb_ && !err_pipe_) {
            ready_read_stderr(nullptr);
        }

        return leak_if(proc_hdl_->spawn(_file, _args, _envs));
    }

    int spawn(const std::string &_file, const std::vector<std::string> &_args = {}, const std::vector<std::string> &_envs = {})
    {
        std::vector<char *> c_args;
        for (const auto &arg : _args) {
            c_args.push_back(const_cast<char *>(arg.c_str()));
        }
        c_args.push_back(nullptr);

        std::vector<char *> c_env;
        for (const auto &e : _envs) {
            c_env.push_back(const_cast<char *>(e.c_str()));
        }
        c_env.push_back(nullptr);

        return spawn(_file.c_str(), c_args.data(), c_env.data());
    }

    template<typename _ArgIt, typename _EnvIt>
    int spawn(const std::string &_file, _ArgIt _arg_begin, _ArgIt _arg_end, _EnvIt _env_begin, _EnvIt _env_end)
    {
        std::vector<char *> c_args;
        for (auto it = _arg_begin; it != _arg_end; ++it) {
            c_args.push_back(const_cast<char *>(it->c_str()));
        }
        c_args.push_back(nullptr);

        std::vector<char *> c_envs;
        for (auto it = _env_begin; it != _env_end; ++it) {
            c_envs.push_back(const_cast<char *>(it->c_str()));
        }
        c_envs.push_back(nullptr);

        return spawn(_file.c_str(), c_args.data(), c_envs.data());
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
               (proc_hdl_ && proc_hdl_->closing());
    }
    
    void close() noexcept {
        if (in_pipe_) {
            in_pipe_->close();
        }
        if (out_pipe_) {
            out_pipe_->close();
        }
        if (err_pipe_) {
            err_pipe_->close();
        }
        if (proc_hdl_) {
            proc_hdl_->close();
        }
    }

private:
    std::shared_ptr<void> self_ptr_;
    std::shared_ptr<::uvw::process_handle> proc_hdl_;
    std::shared_ptr<::uvw::pipe_handle> in_pipe_;
    std::shared_ptr<::uvw::pipe_handle> out_pipe_;
    std::shared_ptr<::uvw::pipe_handle> err_pipe_;
    data_callback  stdout_cb_;
    data_callback  stderr_cb_;
    error_callback error_cb_;
    exit_callback  exit_cb_;
    close_callback close_cb_;
};

}
}
}

#endif // !MMBKPP_WRAP_UVW_SUBPROCESS_H_INCLUDED

