
#ifndef MMBKPP_WRAP_UVW_PIPE_H_INCLUDED
#define MMBKPP_WRAP_UVW_PIPE_H_INCLUDED

#include <uvw/pipe.h>
#include "handle.h"
#include "stream.h"

#ifndef MMBKPP_WRAP_UVW_3_0_DISABLED
namespace uvw {
    
    using PipeHandle = pipe_handle;

}

#endif

#endif // !MMBKPP_WRAP_UVW_PIPE_H_INCLUDED
