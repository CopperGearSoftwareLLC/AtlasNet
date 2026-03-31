#pragma once
#include "atlasnet/core/serialize/ByteWriter.hpp"
namespace AtlasNet
{
  class IMessage
  {
  };
} // namespace AtlasNet
// user-facing field syntax
#define ATLASNET_MESSAGE_DATA(Type, Name) (Type, Name)

// ----- internal helpers -----

#define ATLASNET_EXPAND(x) x
#define ATLASNET_CAT(a, b) ATLASNET_CAT_I(a, b)
#define ATLASNET_CAT_I(a, b) a##b

// count args, up to 8 fields here
#define ATLASNET_NARGS(...) ATLASNET_NARGS_I(__VA_ARGS__,8,7,6,5,4,3,2,1,0)
#define ATLASNET_NARGS_I(_1,_2,_3,_4,_5,_6,_7,_8,N,...) N

// apply macro to each argument
#define ATLASNET_FOR_EACH(M, ...) \
  ATLASNET_EXPAND(ATLASNET_CAT(ATLASNET_FOR_EACH_, ATLASNET_NARGS(__VA_ARGS__))(M, __VA_ARGS__))

#define ATLASNET_FOR_EACH_1(M, a1) M(a1)
#define ATLASNET_FOR_EACH_2(M, a1, a2) M(a1) M(a2)
#define ATLASNET_FOR_EACH_3(M, a1, a2, a3) M(a1) M(a2) M(a3)
#define ATLASNET_FOR_EACH_4(M, a1, a2, a3, a4) M(a1) M(a2) M(a3) M(a4)
#define ATLASNET_FOR_EACH_5(M, a1, a2, a3, a4, a5) M(a1) M(a2) M(a3) M(a4) M(a5)
#define ATLASNET_FOR_EACH_6(M, a1, a2, a3, a4, a5, a6) M(a1) M(a2) M(a3) M(a4) M(a5) M(a6)
#define ATLASNET_FOR_EACH_7(M, a1, a2, a3, a4, a5, a6, a7) M(a1) M(a2) M(a3) M(a4) M(a5) M(a6) M(a7)
#define ATLASNET_FOR_EACH_8(M, a1, a2, a3, a4, a5, a6, a7, a8) M(a1) M(a2) M(a3) M(a4) M(a5) M(a6) M(a7) M(a8)

// unwrap (Type, Name)
#define ATLASNET_DECLARE_FIELD(Field) ATLASNET_DECLARE_FIELD_I Field
#define ATLASNET_DECLARE_FIELD_I(Type, Name) Type Name;

#define ATLASNET_SERIALIZE_FIELD(Field) ATLASNET_SERIALIZE_FIELD_I Field
#define ATLASNET_SERIALIZE_FIELD_I(Type, Name) archive(Name);

// final message macro
#define ATLASNET_MESSAGE(Name, ...)                                  \
  struct Name : AtlasNet::IMessage {                                 \
    ATLASNET_FOR_EACH(ATLASNET_DECLARE_FIELD, __VA_ARGS__)           \
                                                                     \
    template <typename Ar>                                           \
    void Serialize(Ar& archive) {                                    \
      ATLASNET_FOR_EACH(ATLASNET_SERIALIZE_FIELD, __VA_ARGS__)       \
    }                                                                \
  };


  
ATLASNET_MESSAGE(
    testmessage, ATLASNET_MESSAGE_DATA(int, test), ATLASNET_MESSAGE_DATA(float, test2)
);


void test()
{
    testmessage msg;
    msg.test = 42;
    msg.test2 = 3.14f;
    ByteWriter bw;
    msg.Serialize(bw);
}