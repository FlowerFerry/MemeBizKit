
#ifndef MMBKPP_WRAP_UVW_PIPE_FWD_H_INCLUDED
#define MMBKPP_WRAP_UVW_PIPE_FWD_H_INCLUDED

#include <mmbkpp/wrap/uvw/stream_fwd.h>

#ifndef MMBKPP_WRAP_UVW_3_0_DISABLED
namespace uvw {
    
    class PipeHandle;
}
#else
namespace uvw {
    
    class pipe_handle;
    
    using PipeHandle = pipe_handle;
    
}
#endif

#endif // !MMBKPP_WRAP_UVW_PIPE_FWD_H_INCLUDED
