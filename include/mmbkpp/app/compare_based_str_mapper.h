
#ifndef MMBKPP_APP_COMPARE_BASED_STR_MAPPER_H_INCLUDED
#define MMBKPP_APP_COMPARE_BASED_STR_MAPPER_H_INCLUDED

#include <tuple>
#include <vector>
#include <memory>

namespace mmbkpp {
namespace app {

namespace cbsm {

struct cond
{
    virtual ~cond() = default;
    virtual bool match(double _v) const = 0;
};

struct cond_one_contrast : public cond
{
    virtual ~cond_one_contrast() = default;
    virtual double contrast_value() const = 0;
};

struct cond_equal : public cond_one_contrast
{
    cond_equal(double _v) 
        : v_(_v) 
    {}

    inline bool match(double _v) const override { return v_ == _v; }
    inline double contrast_value() const override { return v_; }

private:
    double v_;
};

struct cond_not_equal : public cond_one_contrast
{
    cond_not_equal(double _v) 
        : v_(_v) 
    {}

    inline bool match(double _v) const override { return v_ != _v; }
    inline double contrast_value() const override { return v_; }

private:
    double v_;
};

struct cond_less_than : public cond_one_contrast
{
    cond_less_than(double _v) 
        : v_(_v) 
    {}

    inline bool match(double _v) const override { return _v < v_; }
    inline double contrast_value() const override { return v_; }

private:
    double v_;
};

struct cond_less_than_or_equal : public cond_one_contrast
{
    cond_less_than_or_equal(double _v) 
        : v_(_v) 
    {}

    inline bool match(double _v) const override { return _v <= v_; }
    inline double contrast_value() const override { return v_; }

private:
    double v_;
};

struct cond_greater_than : public cond_one_contrast
{
    cond_greater_than(double _v) 
        : v_(_v) 
    {}

    inline bool match(double _v) const override { return _v > v_; } 
    inline double contrast_value() const override { return v_; }

private:
    double v_;
};

struct cond_greater_than_or_equal : public cond_one_contrast
{
    cond_greater_than_or_equal(double _v) 
        : v_(_v) 
    {}

    inline bool match(double _v) const override { return _v >= v_; }
    inline double contrast_value() const override { return v_; }

private:
    double v_;  
};

struct cond_range : public cond
{
    cond_range(std::unique_ptr<cond_one_contrast> _lep, std::unique_ptr<cond_one_contrast> _rep)
        : lep_(std::move(_lep))
        , rep_(std::move(_rep))
    {
        if (lep_->contrast_value() > rep_->contrast_value())
            std::swap(lep_, rep_);
    }

    inline bool match(double _v) const override
    {
        return lep_->match(_v) && rep_->match(_v);
    }

private:
    std::unique_ptr<cond_one_contrast> lep_;
    std::unique_ptr<cond_one_contrast> rep_;
};

}

template <typename T>
struct cmp_based_str_mapper
{

    inline void push_back(std::unique_ptr<cbsm::cond> _cond, const T& _value)
    {
        m_.emplace_back(std::move(_cond), _value);
    }

    inline const T* find(double _v) const
    {
        for (const auto& [cond, value] : m_)
        {
            if (cond->match(_v))
                return &value;
        }
        return nullptr;
    }

    inline void clear()
    {
        m_.clear();
    }

private:
    std::vector< std::tuple<std::unique_ptr<cbsm::cond>, T> > m_;
};

}    
}

#endif // !MMBKPP_APP_COMPARE_BASED_STR_MAPPER_H_INCLUDED
