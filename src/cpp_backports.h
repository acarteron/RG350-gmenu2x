#ifndef __CPP_BACKPORTS__
#define __CPP_BACKPORTS__

// Backports of C++ 17 stuff to C++ 14.

namespace stdx {
template <typename _Tp>
constexpr const _Tp& clamp(const _Tp& __val, const _Tp& __lo, const _Tp& __hi) {
  return (__val < __lo) ? __lo : (__hi < __val) ? __hi : __val;
}

template <typename _Tp, typename _Compare>
constexpr const _Tp& clamp(const _Tp& __val, const _Tp& __lo, const _Tp& __hi,
                           _Compare __comp) {
  return __comp(__val, __lo) ? __lo : __comp(__hi, __val) ? __hi : __val;
}
}  // namespace std

#endif  // __CPP_BACKPORTS__
