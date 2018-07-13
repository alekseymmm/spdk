/*
 * bitops.h
 *
 *  Created on: 13 июл. 2018 г.
 *      Author: root
 */

#ifndef LIB_BDEV_RAID_BITOPS_H_
#define LIB_BDEV_RAID_BITOPS_H_

static inline bool constant_test_bit(int nr, const void *addr)
{
	const u32 *p = (const u32 *)addr;
	return ((1UL << (nr & 31)) & (p[nr >> 5])) != 0;
}
static inline bool variable_test_bit(int nr, const void *addr)
{
	bool v;
	const u32 *p = (const u32 *)addr;

	asm("btl %2,%1; setc %0" : "=qm" (v) : "m" (*p), "Ir" (nr));
	return v;
}

#define test_bit(nr,addr) \
(__builtin_constant_p(nr) ? \
 constant_test_bit((nr),(addr)) : \
 variable_test_bit((nr),(addr)))

static inline void set_bit(int nr, void *addr)
{
	asm("btsl %1,%0" : "+m" (*(u32 *)addr) : "Ir" (nr));
}

#endif /* LIB_BDEV_RAID_BITOPS_H_ */
