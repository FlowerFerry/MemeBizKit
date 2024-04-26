
#ifndef MMBKPP_WRAP_UVW_LOOP_FWD_H_INCLUDED
#define MMBKPP_WRAP_UVW_LOOP_FWD_H_INCLUDED

namespace uvw {
#ifdef MMBKPP_WRAP_UVW_3_0_AFTER_DISABLED
	class Loop;
#else
	class loop;
	using Loop = loop;
#endif
};

#endif // !MMBKPP_WRAP_UVW_LOOP_FWD_H_INCLUDED
