
#ifndef MMBKPP_WRAP_UVW_FS_H_INCLUDED
#define MMBKPP_WRAP_UVW_FS_H_INCLUDED

#include <uvw/fs.h>

#ifndef MMBKPP_WRAP_UVW_3_0_DISABLED
namespace uvw {
    using FsReq   = fs_req;
    using FsEvent = fs_event;
    
    using FileReq = file_req;
}
#endif

#endif // !MMBKPP_WRAP_UVW_FS_H_INCLUDED
