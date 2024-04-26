
#ifndef MMBKPP_WRAP_UVW_ASYNC_FWD_H_INCLUDED
#define MMBKPP_WRAP_UVW_ASYNC_FWD_H_INCLUDED


#ifdef MMBKPP_WRAP_UVW_3_0_AFTER_DISABLED
namespace uvw {
    class AsyncHandle;
}
#else
namespace uvw {
    class async_handle;
    using AsyncHandle = async_handle;
}
#endif

#endif // !MMBKPP_WRAP_UVW_ASYNC_FWD_H_INCLUDED
