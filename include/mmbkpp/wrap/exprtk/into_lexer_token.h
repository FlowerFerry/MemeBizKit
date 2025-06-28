
#ifndef MMBKPP_WRAP_EXPRTK_INTO_LEXER_TOKEN_H_INCLUDED
#define MMBKPP_WRAP_EXPRTK_INTO_LEXER_TOKEN_H_INCLUDED

#include <exprtk.hpp>
#include <mmbkpp/wrap/exprtk/lexer_token.h>
#include <memepp/convert/std/string.hpp>

namespace mmbkpp {
namespace wrap {
namespace exprtk {

    inline lexer_token into_lexer_token(const ::exprtk::lexer::token& _token)
    {
        lexer_token result;
        result.type = static_cast<enum lexer_token::type>(_token.type);
        result.value = mm_from(_token.value);
        result.position = _token.position;
        return result;
    }

} // namespace exprtk
} // namespace wrap
} // namespace mmbkpp

#endif // !MMBKPP_WRAP_EXPRTK_INTO_LEXER_TOKEN_H_INCLUDED
