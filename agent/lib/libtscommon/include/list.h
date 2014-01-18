/*
 * list.h
 *
 *  Created on: 09.12.2012
 *      Author: myaut
 *
 *  Based on lists implementation for Linux kernel.
 */

#ifndef TS_LIST_H
#define TS_LIST_H

#include <defs.h>

#include <stdarg.h>
#include <stdio.h>

/**
 * @module Double linked list
 *
 * Simple double linked list implementation.
 *
 * Some of the internal functions ("__xxx") are useful when
 * manipulating whole lists rather than single entries, as
 * sometimes we already know the next/prev entries and we can
 * generate better code by using them directly rather than
 * using the generic single-entry routines.
 *
 * @note Implementation is taken from Linux Kernel
 */

#define LISTNAMELEN		24

typedef struct list_node {
	struct list_node *next, *prev;
} list_node_t;

typedef struct list_head {
	list_node_t l_head;

	char l_name[LISTNAMELEN];
} list_head_t;

#define LIST_POISON1 	(list_node_t*) 0xDEADCAFE
#define LIST_POISON2 	(list_node_t*) 0xCAFEDEAD

STATIC_INLINE void list_node_init(list_node_t *list)
{
	list->next = list;
	list->prev = list;
}

STATIC_INLINE void list_head_init(list_head_t *list, const char* namefmt, ...)
{
	va_list va;

	list_node_init(&list->l_head);

	if(namefmt != NULL) {
		va_start(va, namefmt);
		vsnprintf(list->l_name, LISTNAMELEN, namefmt, va);
		va_end(va);
	}
}

STATIC_INLINE list_node_t* list_head_node(list_head_t* head) {
	return &head->l_head;
}

#if 0
STATIC_INLINE void list_head_copy(list_head_t *dst, const list_head_t *src) {
	if(list_empty(src)) {
		list_node_init(&dst->l_head);
	}
	else {
		dst->l_head.prev = src->l_head.prev;
		dst->l_head.next = src->l_head.next;
	}

	strcpy(dst->l_name, src->l_name);
}
#endif

/**
 * Insert a new entry between two known consecutive entries.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
STATIC_INLINE void __list_add(list_node_t *node,
			      list_node_t *prev,
			      list_node_t *next)
{
	next->prev = node;
	node->next = next;
	node->prev = prev;
	prev->next = node;
}

/**
 * list_add - add a new entry
 * @param new  new entry to be added
 * @param head  list head to add it after
 *
 * Insert a new entry after the specified head.
 * This is good for implementing stacks.
 */
STATIC_INLINE void list_add(list_node_t *node, list_head_t *head)
{
	__list_add(node, &head->l_head, head->l_head.next);
}


/**
 * list_add_tail - add a new entry
 * @param node  new entry to be added
 * @param head  list head to add it before
 *
 * Insert a new entry before the specified head.
 * This is useful for implementing queues.
 */
STATIC_INLINE void list_add_tail(list_node_t *node, list_head_t *head)
{
	__list_add(node, head->l_head.prev, &head->l_head);
}

/**
 * list_insert - insert entry after another
 * @param node  new entry to be added
 * @param iter  node after which it has to be added
 */
STATIC_INLINE void list_insert(list_node_t *node, list_node_t *iter)
{
	__list_add(node, iter->prev, iter->next);
}


/*
 * Delete a list entry by making the prev/next entries
 * point to each other.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
STATIC_INLINE void __list_del(list_node_t * prev, list_node_t * next)
{
	next->prev = prev;
	prev->next = next;
}

/**
 * list_del - deletes entry from list.
 * @param entry  the element to delete from the list.
 * Note: list_empty() on entry does not return true after this, the entry is
 * in an undefined state.
 */
STATIC_INLINE void __list_del_entry(list_node_t *entry)
{
	__list_del(entry->prev, entry->next);
}

STATIC_INLINE void list_del(list_node_t *entry)
{
	__list_del(entry->prev, entry->next);

	entry->next = entry;
	entry->prev = entry;
}

/**
 * list_replace - replace old entry by new one
 * @param old the element to be replaced
 * @param new the new element to insert
 *
 * If @old was empty, it will be overwritten.
 */
STATIC_INLINE void list_replace(list_node_t *old,
				list_node_t *node)
{
	node->next = old->next;
	node->next->prev = node;
	node->prev = old->prev;
	node->prev->next = node;
}

STATIC_INLINE void list_replace_init(list_node_t *old,
					list_node_t *node)
{
	list_replace(old, node);
	list_node_init(old);
}

/**
 * list_del_init - deletes entry from list and reinitialize it.
 * @param entry  the element to delete from the list.
 */
STATIC_INLINE void list_del_init(list_node_t *entry)
{
	__list_del_entry(entry);
	list_node_init(entry);
}

/**
 * list_move - delete from one list and add as another's head
 * @param list  the entry to move
 * @param head  the head that will precede our entry
 */
STATIC_INLINE void list_move(list_node_t *list, list_head_t *head)
{
	__list_del_entry(list);
	list_add(list, head);
}

/**
 * list_move_tail - delete from one list and add as another's tail
 * @param list  the entry to move
 * @param head  the head that will follow our entry
 */
STATIC_INLINE void list_move_tail(list_node_t *list,
		list_head_t *head)
{
	__list_del_entry(list);
	list_add_tail(list, head);
}

/**
 * list_is_last - tests whether @list is the last entry in list @head
 * @param list  the entry to test
 * @param head  the head of the list
 */
STATIC_INLINE int list_is_last(const list_node_t *list,
				const list_head_t *head)
{
	return list->next == &(head->l_head);
}

/**
 * list_empty - tests whether a list is empty
 * @param head  the list to test.
 */
STATIC_INLINE int list_empty(const list_head_t *head)
{
	return head->l_head.next == &head->l_head;
}

/**
 * list_empty_careful - tests whether a list is empty and not being modified
 * @param head  the list to test
 *
 * Description:
 * tests whether a list is empty _and_ checks that no other CPU might be
 * in the process of modifying either member (next or prev)
 *
 * NOTE: using list_empty_careful() without synchronization
 * can only be safe if the only activity that can happen
 * to the list entry is list_del_init(). Eg. it cannot be used
 * if another CPU could re-list_add() it.
 */
STATIC_INLINE int list_empty_careful(const list_node_t *head)
{
	list_node_t *next = head->next;
	return (next == head) && (next == head->prev);
}

/**
 * list_node_alone	- tests if node is not inserted into list
 * @param head  the list to test.
 */
STATIC_INLINE int list_node_alone(const list_node_t *node)
{
	return node->next == node && node->prev == node;
}

/**
 * list_rotate_left - rotate the list to the left
 * @param head  the head of the list
 */
STATIC_INLINE void list_rotate_left(list_head_t *head)
{
	list_node_t *first;

	if (!list_empty(head)) {
		first = head->l_head.next;
		list_move_tail(first, head);
	}
}

/**
 * list_is_singular - tests whether a list has just one entry.
 * @param head  the list to test.
 */
STATIC_INLINE int list_is_singular(const list_head_t *head)
{
	return !list_empty(head) && (head->l_head.next == head->l_head.prev);
}

STATIC_INLINE void __list_cut_position(list_head_t *list,
		list_head_t *head, list_node_t *entry)
{
	list_node_t *new_first = entry->next;

	list_head_init(list, "%s#", head->l_name);

	list->l_head.next = head->l_head.next;
	list->l_head.next->prev = &list->l_head;
	list->l_head.prev = entry;

	entry->next = &list->l_head;
	head->l_head.next = new_first;

	new_first->prev = &head->l_head;
}

/**
 * list_cut_position - cut a list into two
 * @param list  a new list to add all removed entries
 * @param head  a list with entries
 * @param entry  an entry within head, could be the head itself
 *	and if so we won't cut the list
 *
 * This helper moves the initial part of @head, up to and
 * including @entry, from @head to @list. You should
 * pass on @entry an element you know is on @head. @list
 * should be an empty list or a list you do not care about
 * losing its data.
 *
 */
STATIC_INLINE void list_cut_position(list_head_t *list,
		list_head_t *head, list_node_t *entry)
{
	if (list_empty(head))
		return;

	if (list_is_singular(head) &&
		(head->l_head.next != entry && &head->l_head != entry))
		return;

	if (entry == &head->l_head)
		list_head_init(list, head->l_name);
	else
		__list_cut_position(list, head, entry);
}

STATIC_INLINE void __list_splice(const list_head_t *list,
				 list_node_t *prev,
				 list_node_t *next)
{
	list_node_t *first = list->l_head.next;
	list_node_t *last = list->l_head.prev;

	first->prev = prev;
	prev->next = first;

	last->next = next;
	next->prev = last;
}

/**
 * list_splice - join two lists, this is designed for stacks
 * @param list  the new list to add.
 * @param head  the place to add it in the first list.
 */
STATIC_INLINE void list_splice(const list_head_t *list,
				list_node_t *node)
{
	if (!list_empty(list))
		__list_splice(list, node, node->next);
}

/**
 * list_splice_tail - join two lists, each list being a queue
 * @param list  the new list to add.
 * @param head  the place to add it in the first list.
 */
STATIC_INLINE void list_splice_tail(list_head_t *list,
				list_node_t *node)
{
	if (!list_empty(list))
		__list_splice(list, node->prev, node);
}

/**
 * list_splice_init - join two lists and reinitialise the emptied list.
 * @param list  the new list to add.
 * @param head  the place to add it in the first list.
 *
 * The list at @list is reinitialised
 */
STATIC_INLINE void list_splice_init(list_head_t *list,
		list_node_t *node)
{
	if (!list_empty(list)) {
		__list_splice(list, node, node->next);
		list_head_init(list, NULL);
	}
}

/**
 * list_splice_tail_init - join two lists and reinitialise the emptied list
 * @param list  the new list to add.
 * @param head  the place to add it in the first list.
 *
 * Each of the lists is a queue.
 * The list at @list is reinitialised
 */
STATIC_INLINE void list_splice_tail_init(list_head_t *list,
		list_node_t *node)
{
	if (!list_empty(list)) {
		__list_splice(list, node->prev, node);
		list_head_init(list, NULL);
	}
}

/**
 * list_entry - get the struct for this entry
 * @param ptr 	the &struct list_head pointer.
 * @param type 	the type of the struct this is embedded in.
 * @param member 	the name of the list_struct within the struct.
 */
#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)

/**
 * list_first_entry - get the first element from a list
 * @param ptr 	the list head to take the element from.
 * @param type 	the type of the struct this is embedded in.
 * @param member 	the name of the list_struct within the struct.
 *
 * Note, that list is expected to be not empty.
 */
#define list_first_entry(type, head, member) \
	list_entry((head)->l_head.next, type, member)

/**
* list_last_entry - get the last element from a list
* @param head         the list head to take the element from.
* @param type         the type of the struct this is embedded in.
* @param member         the name of the list_struct within the struct.
*
* Note, that list is expected to be not empty.
*/
#define list_last_entry(type, head, member) \
        list_entry((head)->l_head.prev, type, member)

/**
 * list_for_each	-	iterate over a list
 * @param pos 	the &struct list_head to use as a loop cursor.
 * @param head 	the head for your list.
 */
#define list_for_each(pos, head) \
	for (pos = (head)->l_head.next; pos != &(head)->l_head; pos = pos->next)

/**
 * __list_for_each	-	iterate over a list
 * @param pos 	the &struct list_head to use as a loop cursor.
 * @param head 	the head for your list.
 *
 * This variant doesn't differ from list_for_each() any more.
 * We don't do prefetching in either case.
 */
#define __list_for_each(pos, head) \
	for (pos = (head)->l_head.next; pos != &(head)->l_head; pos = pos->next)

/**
 * list_for_each_prev	-	iterate over a list backwards
 * @param pos 	the &struct list_head to use as a loop cursor.
 * @param head 	the head for your list.
 */
#define list_for_each_prev(pos, head) \
	for (pos = (head)->l_head.prev; pos != &(head)->l_head; pos = pos->prev)

/**
 * list_for_each_safe - iterate over a list safe against removal of list entry
 * @param pos 	the &struct list_head to use as a loop cursor.
 * @param n 		another &struct list_head to use as temporary storage
 * @param head 	the head for your list.
 */
#define list_for_each_safe(pos, n, head) \
	for (pos = (head)->l_head.next, n = pos->next; \
		pos != &(head)->l_head); \
		pos = n, n = pos->next)

/**
 * iterate over a list backwards safe against removal of list entry
 * @param pos 	the &struct list_head to use as a loop cursor.
 * @param n 		another &struct list_head to use as temporary storage
 * @param head 	the head for your list.
 */
#define list_for_each_prev_safe(pos, n, head) \
	for (pos = (head)->l_head.prev, n = pos->prev; \
	     pos != &(head)->l_head; \
	     pos = n, n = pos->prev)

/**
 * list_for_each_entry	-	iterate over list of given type
 * @param pos 	the type * to use as a loop cursor.
 * @param head 	the head for your list.
 * @param member 	the name of the list_struct within the struct.
 */
#define list_for_each_entry(type, pos, head, member)				\
	for (pos = list_entry((head)->l_head.next, type, member);	\
	     &pos->member != &(head)->l_head; 	\
	     pos = list_entry(pos->member.next, type, member))

/**
 * list_for_each_entry_reverse - iterate backwards over list of given type.
 * @param pos 	the type * to use as a loop cursor.
 * @param head 	the head for your list.
 * @param member 	the name of the list_struct within the struct.
 */
#define list_for_each_entry_reverse(type, pos, head, member)			\
	for (pos = list_entry((head)->l_head.prev, type, member);	\
	     &pos->member != &(head)->l_head; 	\
	     pos = list_entry(pos->member.prev, type, member))

/**
 * list_prepare_entry - prepare a pos entry for use in list_for_each_entry_continue()
 * @param pos 	the type * to use as a start point
 * @param head 	the head of the list
 * @param member 	the name of the list_struct within the struct.
 *
 * Prepares a pos entry for use as a start point in list_for_each_entry_continue().
 */
#define list_prepare_entry(type, pos, head, member) \
	((pos) ? : list_entry(head, type, member))

/**
 * list_for_each_entry_continue - continue iteration over list of given type
 * @param pos 	the type * to use as a loop cursor.
 * @param head 	the head for your list.
 * @param member 	the name of the list_struct within the struct.
 *
 * Continue to iterate over list of given type, continuing after
 * the current position.
 */
#define list_for_each_entry_continue(type, pos, head, member) 		\
	for (pos = list_entry(pos->member.next, type, member);	\
	     &pos->member != &(head)->l_head;	\
	     pos = list_entry(pos->member.next, type, member))

/**
 * list_for_each_entry_continue_reverse - iterate backwards from the given point
 * @param pos 	the type * to use as a loop cursor.
 * @param head 	the head for your list.
 * @param member 	the name of the list_struct within the struct.
 *
 * Start to iterate over list of given type backwards, continuing after
 * the current position.
 */
#define list_for_each_entry_continue_reverse(type, pos, head, member)		\
	for (pos = list_entry(pos->member.prev, type, member);	\
	     &pos->member != &(head)->l_head;	\
	     pos = list_entry(pos->member.prev, type, member))

/**
 * list_for_each_entry_from - iterate over list of given type from the current point
 * @param pos 	the type * to use as a loop cursor.
 * @param head 	the head for your list.
 * @param member 	the name of the list_struct within the struct.
 *
 * Iterate over list of given type, continuing from current position.
 */
#define list_for_each_entry_from(type, pos, head, member) 			\
	for (; &pos->member != &(head)->l_head;	\
	     pos = list_entry(pos->member.next, type, member))

/**
 * list_for_each_entry_safe - iterate over list of given type safe against removal of list entry
 * @param pos 	the type * to use as a loop cursor.
 * @param n 		another type * to use as temporary storage
 * @param head 	the head for your list.
 * @param member 	the name of the list_struct within the struct.
 */
#define list_for_each_entry_safe(type, pos, n, head, member)			\
	for (pos = list_entry((head)->l_head.next, type, member),	\
		n = list_entry(pos->member.next, type, member);	\
	     &pos->member != &(head)->l_head; 					\
	     pos = n, n = list_entry(n->member.next, type, member))

/**
 * list_for_each_entry_safe_continue - continue list iteration safe against removal
 * @param pos 	the type * to use as a loop cursor.
 * @param n 		another type * to use as temporary storage
 * @param head 	the head for your list.
 * @param member 	the name of the list_struct within the struct.
 *
 * Iterate over list of given type, continuing after current point,
 * safe against removal of list entry.
 */
#define list_for_each_entry_safe_continue(type, pos, n, head, member) 		\
	for (pos = list_entry(pos->member.next, type, member), 		\
		n = list_entry(pos->member.next, type, member);		\
	     &pos->member != &(head)->l_head;						\
	     pos = n, n = list_entry(n->member.next, type, member))

/**
 * list_for_each_entry_safe_from - iterate over list from current point safe against removal
 * @param pos 	the type * to use as a loop cursor.
 * @param n 		another type * to use as temporary storage
 * @param head 	the head for your list.
 * @param member 	the name of the list_struct within the struct.
 *
 * Iterate over list of given type from current point, safe against
 * removal of list entry.
 */
#define list_for_each_entry_safe_from(type, pos, n, head, member) 			\
	for (n = list_entry(pos->member.next, type, member);		\
	     &pos->member != &(head)->l_head;								\
	     pos = n, n = list_entry(n->member.next, type, member))

/**
 * list_for_each_entry_safe_reverse - iterate backwards over list safe against removal
 * @param pos 	the type * to use as a loop cursor.
 * @param n 		another type * to use as temporary storage
 * @param head 	the head for your list.
 * @param member 	the name of the list_struct within the struct.
 *
 * Iterate backwards over list of given type, safe against removal
 * of list entry.
 */
#define list_for_each_entry_safe_reverse(type, pos, n, head, member)			\
	for (pos = list_entry((head)->l_head.prev, type, member),	\
		n = list_entry(pos->member.prev, type, member);			\
	     &pos->member != &(head)->l_head; 								\
	     pos = n, n = list_entry(n->member.prev, type, member))

/**
 * list_safe_reset_next - reset a stale list_for_each_entry_safe loop
 * @param pos 	the loop cursor used in the list_for_each_entry_safe loop
 * @param n 		temporary storage used in list_for_each_entry_safe
 * @param member 	the name of the list_struct within the struct.
 *
 * list_safe_reset_next is not safe to use in general if the list may be
 * modified concurrently (eg. the lock is dropped in the loop body). An
 * exception to this is if the cursor element (pos) is pinned in the list,
 * and list_safe_reset_next is called after re-taking the lock and before
 * completing the current iteration of the loop body.
 */
#define list_safe_reset_next(type, pos, n, member)				\
	n = list_entry(pos->member.next, type, member)

#endif
