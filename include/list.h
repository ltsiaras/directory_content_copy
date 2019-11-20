#ifndef __LIST_H__
#define __LIST_H__

#include "list_type.h"

typedef struct info *info_p;

typedef struct list_node *list_node_p;

typedef struct info
{   int size;	
	list_node_p head;
} info;

typedef struct list_node
{
	data_t data;
	list_node_p next;
	list_node_p previous;
}list_node;

info_p LIST_create();		//Initializes list
void LIST_destroy(info_p *linfo);		//Destroys list

int LIST_empty(info_p linfo);		//Checks if list is empty
void LIST_content(const info_p  linfo, list_node_p p, data_t *proc, int *error);	//Displays content of p node
void LIST_traverse(const info_p  linfo);		//traverses whole list and prints the elements to file

void LIST_insert(info_p * const linfo, data_t proc, list_node_p new_node, int *error);		//Inserts new list element
void insert_begin(info_p * const linfo, data_t proc, int *error);		//Inserts at beginning
void insert_after(info_p * const linfo, data_t proc, list_node_p cur_node, int *error);	//Inserts after node cur_node
void LIST_delete(info_p * const linfo, list_node_p * const p, int *error);	//Deletes p node

void LIST_next(const info_p linfo, list_node_p * const p, int *error);	//Shows next node
void LIST_previous(const info_p linfo, list_node_p * const p, int *error);	//Shows previous node
void LIST_first(const info_p linfo, list_node_p * const first, int *error);	//Shows first node
void LIST_last(const info_p linfo, list_node_p * const last, int *error);	//Shows last node

pid_t LIST_search_jobID(const info_p linfo, int job_id, data_t *proc, list_node_p *p, int *found);	//Searches list for jobID
void LIST_search_pid(const info_p  linfo, int pid, data_t *proc, list_node_p *p, int *found);

#endif
