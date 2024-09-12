#ifndef ROCKSDB_MACRO_H
#define ROCKSDB_MACRO_H

#define FOREACH0_0(f)
#define FOREACH0_1(f)  FOREACH0_0(f);  f(0);
#define FOREACH0_2(f)  FOREACH0_1(f);  f(1);
#define FOREACH0_3(f)  FOREACH0_2(f);  f(2);
#define FOREACH0_4(f)  FOREACH0_3(f);  f(3);
#define FOREACH0_5(f)  FOREACH0_4(f);  f(4);
#define FOREACH0_6(f)  FOREACH0_5(f);  f(5);
#define FOREACH0_7(f)  FOREACH0_6(f);  f(6);
#define FOREACH0_8(f)  FOREACH0_7(f);  f(7);
#define FOREACH0_9(f)  FOREACH0_8(f);  f(8);
#define FOREACH0_10(f) FOREACH0_9(f);  f(9);
#define FOREACH0_11(f) FOREACH0_10(f); f(10);
#define FOREACH0_12(f) FOREACH0_11(f); f(11);
#define FOREACH0_13(f) FOREACH0_12(f); f(12);
#define FOREACH0_14(f) FOREACH0_13(f); f(13);
#define FOREACH0_15(f) FOREACH0_14(f); f(14);
#define FOREACH0_16(f) FOREACH0_15(f); f(15);
#define FOREACH0_17(f) FOREACH0_16(f); f(16);
#define FOREACH0_18(f) FOREACH0_17(f); f(17);
#define FOREACH0_19(f) FOREACH0_18(f); f(18);
#define FOREACH0_20(f) FOREACH0_19(f); f(19);
#define FOREACH0_21(f) FOREACH0_20(f); f(20);
#define FOREACH0_22(f) FOREACH0_21(f); f(21);
#define FOREACH0_23(f) FOREACH0_22(f); f(22);
#define FOREACH0_24(f) FOREACH0_23(f); f(23);
#define FOREACH0_25(f) FOREACH0_24(f); f(24);
#define FOREACH0_26(f) FOREACH0_25(f); f(25);
#define FOREACH0_27(f) FOREACH0_26(f); f(26);
#define FOREACH0_28(f) FOREACH0_27(f); f(27);
#define FOREACH0_29(f) FOREACH0_28(f); f(28);
#define FOREACH0_30(f) FOREACH0_29(f); f(29);
#define FOREACH0_31(f) FOREACH0_30(f); f(30);
#define FOREACH0_32(f) FOREACH0_31(f); f(31);

#define EXPAND(x) x
#define CONCAT(a, b) a##b
#define FOREACH0(f, n) EXPAND(CONCAT(FOREACH0_, n)(f))

#endif //ROCKSDB_MACRO_H
