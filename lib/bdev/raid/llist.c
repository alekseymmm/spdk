#include <stdatomic.h>

#include "vbdev_common.h"
#include "compiler.h"
#include "llist.h"

/**
 * llist_add_batch - add several linked entries in batch
 * @new_first:	first entry in batch to be added
 * @new_last:	last entry in batch to be added
 * @head:	the head for your lock-less list
 *
 * Return whether list is empty before adding.
 */
bool llist_add_batch(struct llist_node *new_first, struct llist_node *new_last,
		     struct llist_head *head)
{
	struct llist_node *first;

	do {
		new_last->next = first = ACCESS_ONCE(head->first);
	} while (__sync_val_compare_and_swap(&head->first, first, new_first) != first);
	//} while (cmpxchg(&head->first, first, new_first) != first);

	return !first;
}

/**
 * llist_del_first - delete the first entry of lock-less list
 * @head:	the head for your lock-less list
 *
 * If list is empty, return NULL, otherwise, return the first entry
 * deleted, this is the newest added one.
 *
 * Only one llist_del_first user can be used simultaneously with
 * multiple llist_add users without lock.  Because otherwise
 * llist_del_first, llist_add, llist_add (or llist_del_all, llist_add,
 * llist_add) sequence in another user may change @head->first->next,
 * but keep @head->first.  If multiple consumers are needed, please
 * use llist_del_all or use lock between consumers.
 */
struct llist_node *llist_del_first(struct llist_head *head)
{
	struct llist_node *entry, *old_entry, *next;

	//entry = smp_load_acquire(&head->first);
	entry = atomic_load(&head->first);
	for (;;) {
		if (entry == NULL)
			return NULL;
		old_entry = entry;
		next = READ_ONCE(entry->next);
		//entry = cmpxchg(&head->first, old_entry, next);
		entry = __sync_val_compare_and_swap(&head->first, old_entry, next);
		if (entry == old_entry)
			break;
	}

	return entry;
}
