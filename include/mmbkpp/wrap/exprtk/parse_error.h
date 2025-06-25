
#ifndef MMBKPP_WRAP_EXPRTK_PARSE_ERROR_H_INCLUDED
#define MMBKPP_WRAP_EXPRTK_PARSE_ERROR_H_INCLUDED

#include <mmbkpp/wrap/exprtk/error_code.h>
#include <mmbkpp/wrap/exprtk/lexer_token.h>
#include <memepp/string.hpp>

namespace mmbkpp {
namespace wrap {
namespace exprtk {

struct parse_error
{
    enum class mode {
         unknown   = 0,
         syntax    = 1,
         token     = 2,
         numeric   = 4,
         symtab    = 5,
         lexer     = 6,
         synthesis = 7,
         helper    = 8,
         parser    = 9
    };

    inline static memepp::string to_string(mode _mode)
    {
        switch (_mode) {
            case mode::unknown : return ("Unknown Error");
            case mode::syntax  : return ("Syntax Error" );
            case mode::token   : return ("Token Error"  );
            case mode::numeric : return ("Numeric Error");
            case mode::symtab  : return ("Symbol Error" );
            case mode::lexer   : return ("Lexer Error"  );
            case mode::helper  : return ("Helper Error" );
            case mode::parser  : return ("Parser Error" );
            default            : return ("Unknown Error");
        }
    }

    error_code ec = error_code::ok;
    int src_ec    = 0;
    mode mode     = mode::unknown;
    lexer_token token;
    memepp::string diagnostic;
    memepp::string src_location;
    memepp::string error_line;
    size_t line_no   = 0;
    size_t column_no = 0;
};

}    
}
}

#endif // !MMBKPP_WRAP_EXPRTK_PARSE_ERROR_H_INCLUDED
