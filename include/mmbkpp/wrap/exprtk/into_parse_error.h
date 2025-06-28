
#ifndef MMBKPP_WRAP_EXPRTK_INTO_PARSE_ERROR_H_INCLUDED
#define MMBKPP_WRAP_EXPRTK_INTO_PARSE_ERROR_H_INCLUDED

#include <exprtk.hpp>
#include <mmbkpp/wrap/exprtk/parse_error.h>
#include <mmbkpp/wrap/exprtk/into_lexer_token.h>
#include <mmbkpp/wrap/exprtk/into_error_code.h>

namespace mmbkpp {
namespace wrap {
namespace exprtk {

    inline parse_error into_parse_error(const ::exprtk::parser_error::type& _error)
    {
        parse_error result;
        result.token = into_lexer_token(_error.token);
        result.mode = static_cast<enum parse_error::mode>(_error.mode);
        result.diagnostic = mm_from(_error.diagnostic);
        result.src_location = mm_from(_error.src_location);
        result.error_line = mm_from(_error.error_line);
        result.line_no = _error.line_no;
        result.column_no = _error.column_no;

        auto pos = result.diagnostic.find("ERR");
        if (pos != result.diagnostic.npos)
        {
            result.src_ec = std::atoi(result.diagnostic.data() + pos + 3);
            result.ec = into_error_code(result.src_ec, result.token.type);
        }
        else {
            result.src_ec = -1;
            result.ec = error_code::unknown;
        }

        return result;
    }

}
}
}

#endif // !MMBKPP_WRAP_EXPRTK_INTO_PARSE_ERROR_H_INCLUDED
