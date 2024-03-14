
#ifndef MMBKPP_WRAP_OBFY_INSTR_H_INCLUDED
#define MMBKPP_WRAP_OBFY_INSTR_H_INCLUDED

#include <obfy/instr.h>

#define OBF_V(x) V(x)
#define OBF_N(x) N(x)

#define OBF_RETURN(x) RETURN(x)
#define OBF_BREAK(x) BREAK(x)
#define OBF_CONTINUE(x) CONTINUE(x)

#define OBF_IF(x) IF(x)
#define OBF_ELSE(x) ELSE(x)
#define OBF_ENDIF(x) ENDIF(x)

#define OBF_FOR(x) FOR(x)
#define OBF_ENDFOR(x) ENDFOR(x)

#define OBF_WHILE(x) WHILE(x)
#define OBF_ENDWHILE(x) ENDWHILE(x)

#define OBF_REPEAT(x) REPEAT(x)
#define OBF_AS_LONG_AS(x) AS_LONG_AS(x)

#define OBF_CASE(x) CASE(x)
#define OBF_ENDCASE(x) ENDCASE(x)

#define OBF_WHEN(x) WHEN(x)
#define OBF_DO(x) DO(x)
#define OBF_DONE(x) DONE(x)
#define OBF_OR(x) OR(x)
#define OBF_DEFAULT(x) DEFAULT(x)

#undef V
#undef N
#undef RETURN
#undef BREAK
#undef CONTINUE

#undef IF
#undef ELSE
#undef ENDIF

#undef FOR
#undef ENDFOR

#undef WHILE
#undef ENDWHILE

#undef REPEAT
#undef AS_LONG_AS

#undef CASE
#undef ENDCASE

#undef WHEN
#undef DO
#undef DONE
#undef OR
#undef DEFAULT

#endif // !MMBKPP_WRAP_OBFY_INSTR_H_INCLUDED
