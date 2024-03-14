
#ifndef MMBKPP_WRAP_OBFY_INSTR_H_INCLUDED
#define MMBKPP_WRAP_OBFY_INSTR_H_INCLUDED

#include <obfy/instr.h>


#if defined OBF_DEBUG

#define OBF_BEGIN
#define OBF_END

#define OBF_V(x) x
#define OBF_N(x) x

#define OBF_RETURN(x) return x;

#define OBF_BREAK break;
#define OBF_CONTINUE continue;

#define OBF_IF(x) if(x) {
#define OBF_ELSE } else {
#define OBF_ENDIF }

#define OBF_FOR(init,cond,inc) for(init;cond;inc) {
#define OBF_ENDFOR }

#define OBF_WHILE(x) while(x) {
#define OBF_ENDWHILE }

#define OBF_REPEAT   do {
#define OBF_AS_LONG_AS(x) } while ((x));

#define OBF_CASE(a) switch (a) {
#define OBF_ENDCASE }
#define OBF_WHEN(c) case c:
#define OBF_DO {
#define OBF_DONE }
#define OBF_OR
#define OBF_DEFAULT default:



#else
#define OBF_JOIN(a,b) a##b
#define OBF_N(a) (obf::Num<decltype(a), obf::MetaRandom<__COUNTER__, 4096>::value ^ a>().get() ^ obf::MetaRandom<__COUNTER__ - 1, 4096>::value)
#define OBF_DEFINE_EXTRA(N,implementer) template <typename T> struct extra_chooser<T,N> { using type = implementer<T>; }
OBF_DEFINE_EXTRA(0, extra_xor);
OBF_DEFINE_EXTRA(1, extra_substraction);
OBF_DEFINE_EXTRA(2, extra_addition);
#define OBF_V(a) ([&](){obf::extra_chooser<std::remove_reference<decltype(a)>::type, obf::MetaRandom<__COUNTER__, \
            MAX_BOGUS_IMPLEMENTATIONS>::value >::type OBF_JOIN(_ec_,__COUNTER__)(a);\
            return obf::stream_helper();}() << a)

#define OBF_FOR(init,cond,inc) { std::shared_ptr<obf::base_rvholder> __rvlocal; obf::for_wrapper( [&](){(init); return __crv; },\
           [&]()->bool{return (cond); }, \
           [&](){inc;return __crv;}).set_body( [&]() {
#define OBF_ENDFOR return __crv;}).run(); }

#define OBF_END return __crv;}).run(); }

#define OBF_IF(x) {std::shared_ptr<obf::base_rvholder> __rvlocal; obf::if_wrapper(( [&]()->bool{ return (x); })).set_then( [&]() {
#define OBF_ELSE return __crv;}).set_else( [&]() {
#define OBF_ENDIF END

#define OBF_WHILE(x) {std::shared_ptr<obf::base_rvholder> __rvlocal; obf::while_wrapper([&]()->bool{ return (x); }).set_body( [&]() {
#define OBF_ENDWHILE END

#define OBF_BREAK __crv = obf::next_step::ns_break; throw __crv;
#define OBF_CONTINUE __crv = obf::next_step::ns_continue; throw __crv;

#define OBF_RETURN(x) __rvlocal.reset(new obf::rvholder<std::remove_reference<decltype(x)>::type>(x,x));  throw __rvlocal;

#define OBF_REPEAT { std::shared_ptr<obf::base_rvholder> __rvlocal; obf::repeat_wrapper().set_body( [&]() {
#define OBF_AS_LONG_AS(x) return __crv;}).set_condition([&]()->bool{ return ( (x) ); }).run(); }

#define OBF_OBF_BEGIN try { obf::next_step __crv = obf::next_step::ns_done; std::shared_ptr<obf::base_rvholder> __rvlocal; (void)__crv;
#define OBF_OBF_END } catch(std::shared_ptr<obf::base_rvholder>& r) { return *r; } catch (...) {throw;}

#define OBF_CASE(a) try { std::shared_ptr<obf::base_rvholder> __rvlocal;\
                auto __avholder = a; obf::case_wrapper<std::remove_reference<decltype(a)>::type>(a).
#define OBF_ENDCASE run(); } catch(obf::next_step&) {}
#define OBF_WHEN(c) add_entry(obf::branch<std::remove_reference<decltype(__avholder)>::type>\
                ( [&,__avholder]() -> std::remove_reference<decltype(__avholder)>::type \
                { std::remove_reference<decltype(__avholder)>::type __c = (c); return __c;} )).
#define OBF_DO add_entry( obf::body([&](){
#define OBF_DONE return obf::next_step::ns_continue;})).
#define OBF_OR join().
#define OBF_DEFAULT add_default(obf::body([&](){

#endif

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
