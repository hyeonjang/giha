module giha;

#define GIHA_WORKGROUP_SIZE 256
#define GIHA_SUBGROUP_SIZE 32

public static const uint WORKGROUP_SIZE = GIHA_WORKGROUP_SIZE;
public static const uint SUBGROUP_SIZE = GIHA_SUBGROUP_SIZE;

__include "giha/geometry/quantity";
__include "giha/geometry/dart";
__include "giha/geometry/geometry";

__include "giha/utills/codec.slang";
__include "giha/utills/digit.slang";
__include "giha/utills/hash.slang";
