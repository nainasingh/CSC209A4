#include "lists.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int asprintf(char **strp, const char *fmt, ...);
//int snprintf(char * restrict s, size_t n, const char * restrict format, ...);
void free_memory(Poll *poll);

/* a wrapper function for malloc so we don't have to do this each time. */
void *Malloc(int size) {
    void *result;
    if ((result = malloc(size)) == NULL) {
        perror("malloc");
        exit(1);
    }
    return result;
}

/* Create a poll with this name and num_slots. 
 * Insert it into the list of polls whose head is pointed to by *head_ptr_add
 * Return 0 if successful, 1 if a poll by this name already exists in this list.
 */
int create_poll(char *name, char **slot_labels, int num_slots, 
                Poll **head_ptr_add) {

    // can't have duplicate named polls so first check if this poll exists
    if (find_poll(name, *head_ptr_add) != NULL) {
        return 1;
    }
    Poll *new_poll = Malloc(sizeof(struct poll));
    new_poll->num_slots = num_slots;

    // copy name in and ensure it is a string
    strncpy(new_poll->name, name, 31);
    new_poll->name[31] = '\0';

    // create the array of slot_labels and malloc space for each label
    new_poll->slot_labels = Malloc(sizeof(char *) * num_slots);
    int i;
    for (i=0; i < num_slots; i++) {
        new_poll->slot_labels[i] = Malloc(strlen(slot_labels[i])+1);
        strcpy(new_poll->slot_labels[i], slot_labels[i]);
    }

    // put new poll at tail of list so its next is NULL explicitly
    new_poll->next = NULL;
    // if this is first item to add it goes at front
    if (*head_ptr_add == NULL) {
        *head_ptr_add = new_poll;
    } else {
        // traverse the list to find the tail and add our new node
        Poll *prev = *head_ptr_add;
        while (prev->next != NULL) {
            prev = prev->next;
        }
        prev->next = new_poll;
    }
    return 0;
}


/* Return a pointer to the poll with this name in this list starting with head.
  Return NULL if there is no such poll.
 */
Poll *find_poll(char *name, Poll *head) {
    while (head != NULL) {
        if (!strcmp(head->name, name)) {
            return head;
        }
        head = head->next;
    }
    // if we get this far then we didn't find a poll with this name
    return NULL;
}



/* delete the poll by the name poll_name from the list at *head_ptr_add.
   Update the head of the list as appropriate and free all dynamically 
   allocated memory no longer used 
   Return 0 if successful and 1 if poll by this name is not found in list. 
*/
int delete_poll(char *poll_name, Poll **head_ptr_add) {

    Poll *poll_to_delete; 
    if ((poll_to_delete = find_poll(poll_name, *head_ptr_add)) == NULL) {
        return 1;
    }
    // need to find pointer to previous poll or to know if first
    Poll *prev = *head_ptr_add;
    // head must point to at least one poll or this function would have
    // already returned after unsuccessfully finding the poll
    if (!strcmp(prev->name, poll_name)) {
        // poll to delete is at the head of the list so go around it
        *head_ptr_add = prev->next;
    } else {
        while (prev->next != NULL) {
            if (!strcmp(prev->next->name, poll_name)) {
                break;
            }
            prev = prev->next;
        }
    }
    // now we have a pointer to poll_to_delete and to prev
    prev->next = poll_to_delete->next;
    // free the memory in poll_to_delete
    free_memory(poll_to_delete);
    return 0;
}

/* do all the freeing for a single poll */
void free_memory(Poll *poll) {
    // clean up participants
    Participant *cur = poll->participants;
    Participant *next;
    while (cur != NULL) {
        if (cur->comment != NULL) {
            free(cur->comment);
        }
        if (cur->availability != NULL) {
            free(cur->availability);
        }
        next = cur->next;
        free(cur);
        cur = next;
    }

    // clean up slot labels
    int i;
    for (i=0; i < poll->num_slots; i++) {
        free(poll->slot_labels[i]);
    }
    free(poll->slot_labels);
    free(poll);
}
    
/* Add a participant with this part_name to the participant list for the poll
   with this poll_name in the list at head_pt. Duplicate participant names
   are not allowed. Set the availability of this participant to avail.
   Return: 0 on success 
      1 for poll does not exist with this name
      2 for participant by this name already in this poll
      3 for availibility string is wrong length. Particpant not added.
*/
int add_participant(char *part_name, char *poll_name, Poll *head_ptr, 
                    char *avail) {
    Poll *poll;
    if ((poll = find_poll(poll_name, head_ptr)) == NULL) {
        return 1;
    }
    Participant *part;
    if ((part = find_part(part_name, poll)) != NULL) {
        return 2;
    }
    if (strlen(avail) != poll->num_slots) {
        return 3;
    }


    // create the participant to add
    Participant *new_part = Malloc(sizeof(struct participant));
    // copy name in and ensure it is a string
    strncpy(new_part->name, part_name, 31);
    new_part->name[31] = '\0';

    // set comment to NULL so we we can know if we need to free 
    // allocated memory when we delete the participant
    new_part->comment = NULL;

    // set availability for this new participant
    new_part->availability = Malloc(poll->num_slots + 1);
    // strcpy is safe because we checked the lengths
    strcpy(new_part->availability, avail);

    // insert this participant at the head of the participant list for this poll
    new_part->next = poll->participants;
    poll->participants = new_part;
    return 0;
}

/* Add a comment from the participant with this part_name to the poll with
   this poll_name. Return values:
      0 success
      1 no poll by this name
      2 no participant by this name for this poll
 */
int add_comment(char *part_name, char *poll_name, char *comment, 
                Poll *head_ptr) {
    Poll *poll;
    if ((poll = find_poll(poll_name, head_ptr)) == NULL) {
        return 1;
    }
    Participant *part;
    if ((part = find_part(part_name, poll)) == NULL) {
        return 2;
    }
    // set or replace comment
    // if comment was previously set, then space needs to be freed
    if (part->comment != NULL) {
        free(part->comment);
    }
    part->comment = Malloc(strlen(comment) + 1);
    strcpy(part->comment, comment);
    return 0;
}

/* Add availabilty for the participant with this part_name to the poll with
 * this poll_name. Return values:
 *    0 success
 *    1 no poll by this name
 *    2 no participant by this name for this poll
 *    3 avail string is incorrect size for this poll 
 */
int update_availability(char *part_name, char *poll_name, char *avail, 
          Poll *head_ptr) {
    Poll *poll;
    if ((poll = find_poll(poll_name, head_ptr)) == NULL) {
        return 1;
    }
    Participant *part;
    if ((part = find_part(part_name, poll)) == NULL) {
        return 2;
    }
    if (strlen(avail) != poll->num_slots) {
        return 3;
    }
    /* strcpy is safe because we allocated the right amount earlier */
    strcpy(part->availability, avail);
    return 0;
}


/* Return pointer to participant with this name from this poll or
   NULL if no such participant exists.
 */
Participant *find_part(char *name, Poll *poll) {
    Participant *current = poll->participants;
    while (current != NULL) {
        if (!strcmp(current->name, name)) {
            return current;
        }
        current = current->next;
    }
    // if we get this far then we didn't find a participant with this name
    return NULL;
}
    
/* 
 *  Print poll names one per line 
 */
char * print_polls(Poll *head) {
    char * message = NULL; 
    int messageSize = 0; 
    Poll *current = head;
    while (current != NULL) {
        printf("%s\n",current->name);
        //for newline character
        messageSize =  messageSize + snprintf(NULL, 0, "%s\n", current->name) + 1;
        current = current->next;
    }
    message = Malloc(sizeof(char) * (messageSize + 2));
    current = head; 
    while (current != NULL) {
        //snprintf(message, strlen(current->name) + 1, "%s\n", current->name);
        strncat(message, current->name, strlen(current->name)); 
        strncat(message, "\n", strlen("\n")); 
        current = current->next;
    }
    message[strlen(message)] = '\0'; 
    return message;
}


/* For the poll by the name poll_name from the list at head_ptr,
 * print the name, number of slots and each label and each participant.
 * For each participant, print name and availability.
 * Return 0 if successful and 1 if poll by this name is not found in list. 
 */
char * print_poll_info(char *poll_name, Poll *head_ptr) {
    char * message = NULL; 
    int messageSize = 0; 
    Poll *poll; 
    if ((poll = find_poll(poll_name, head_ptr)) == NULL) {
        return message;
    }
    
    // start by printing the poll name
    printf("%s\n", poll_name);
    messageSize =  messageSize + snprintf(NULL, 0, "%s\n", poll_name) + 1;


    // add the slot labels each on a separate line
    int label;
    for (label=0; label < poll->num_slots; label++) {
        printf("  Meeting time:%s\r\n", poll->slot_labels[label]);
        messageSize =  messageSize + snprintf(NULL, 0, "  Meeting time:%s\r\n", poll->slot_labels[label]) + 1;
    }

    // then each participant
    Participant *current = poll->participants;
    while (current != NULL) {
        printf("%s:  %s\n", current->name, current->availability);
        messageSize =  messageSize + snprintf(NULL, 0, "%s:  %s\n",current->name, current->availability) + 1;
        if (current->comment != NULL) {
            printf("  Comment: %s\n", current->comment);
            messageSize =  messageSize + snprintf(NULL, 0, "  Comment: %s\n",current->comment) + 1;
        }
        current = current->next;
    }
    message = Malloc(sizeof(char) * (messageSize + 2)); 
    strncat(message, poll_name, strlen(poll_name));
    strncat(message, "\n", 1);
    char *line = "\r\n";

    for (label=0; label < poll->num_slots; label++) {
        strncat(message,"  Meeting time:", 16 );
        strncat(message,poll->slot_labels[label], strlen(poll->slot_labels[label])); 
        strncat(message, line, strlen(line));
    }
    current = poll->participants; 
    while (current != NULL) {
        strncat(message, current->name, strlen(current->name));
        strncat(message, ":  ", 3);
        strncat(message, current->availability, strlen(current->availability)); 
        strncat(message, "\n", 1); 
        if (current->comment != NULL) {
            strncat(message,"  Comment: ", 11);
            strncat(message, current->comment, strlen(current->comment)); 
            strncat(message, "\n", 1); 
        }
        current = current->next;
    }
    message[strlen(message)] = '\0'; 
    return message;
}

