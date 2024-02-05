#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "project2.h"


#define WAIT 0
#define SEND_MESSAGE 1
#define ACK 1
#define NACK 0
#define SEQ0 0
#define SEQ1 1
#define MESSAGE_TIMEOUT 100

struct queue { // struct for linked list logic
    struct msg message;
    struct queue* next;
};

char empty_msg[MESSAGE_LENGTH] = {'\0'}; 
int state;
int curr_sequenceA;
int curr_sequenceB;
struct pkt* curr_package;
struct queue *head; 
struct queue *tail;


 
/* ***************************************************************************
 ALTERNATING BIT AND GO-BACK-N NETWORK EMULATOR: VERSION 1.1  J.F.Kurose

   This code should be used for unidirectional or bidirectional
   data transfer protocols from A to B and B to A.
   Network properties:
   - one way network delay averages five time units (longer if there
     are other messages in the channel for GBN), but can be larger
   - packets can be corrupted (either the header or the data portion)
     or lost, according to user-defined probabilities
   - packets may be delivered out of order.

   Compile as gcc -g project2.c student2.c -o p2
**********************************************************************/



/********* STUDENTS WRITE THE NEXT SEVEN ROUTINES *********/
/* 
 * The routines you will write are detailed below. As noted above, 
 * such procedures in real-life would be part of the operating system, 
 * and would be called by other procedures in the operating system.  
 * All these routines are in layer 4.
 */

/* 
 * A_output(message), where message is a structure of type msg, containing 
 * data to be sent to the B-side. This routine will be called whenever the 
 * upper layer at the sending side (A) has a message to send. It is the job 
 * of your protocol to insure that the data in such a message is delivered 
 * in-order, and correctly, to the receiving side upper layer.
 */
void A_output(struct msg message) {
  if (state == WAIT) {
    addToQueue(message);
  }
  else {
    sendMessage(AEntity, &message);
  }
}


/* sendMessage takes a sender entity and message struct, then creates 
 * a packet to send from the input entity to the other entity
 */
void sendMessage(int entity, struct msg* message) {
  if (message->data != NULL) {
    int sum = calculateChecksum(message->data, ACK, curr_sequenceA);
    curr_package = makePacket(curr_sequenceA, ACK, sum, message->data);
    tolayer3(entity, *curr_package);
    startTimer(entity, MESSAGE_TIMEOUT);
    state = WAIT;
  }
  
}


// takes the current sequence number and switches it to the other number (1->0, 0->1)
int switchSeq(int old) {
  if (old == SEQ0) {
    return SEQ1;
  } else if (old == SEQ1) {
    return SEQ0;
  } 
}

// make packet takes in all the data fields for a packet and returns a newly created packet pointer 
struct pkt* makePacket(int seqnum, int acknum, int checksum, char* message_content) {
  struct pkt* p = malloc(sizeof(struct pkt));
  p->seqnum = seqnum;
  p->acknum = acknum;
  p->checksum = checksum;
  memcpy(p->payload, message_content, MESSAGE_LENGTH);
  return p;
}

// returns the checksum value given payload, acknum, and seqnum
int calculateChecksum(char* vdata, int acknum, int seqnum){
        int i, checksum = 0;


        for(i = 0; i < MESSAGE_LENGTH; i++){
          checksum += (int)(vdata[i]) * i;
        }

        checksum += acknum * 21;
        checksum += seqnum * 22;

        return checksum;
}



/*
 * Just like A_output, but residing on the B side.  USED only when the 
 * implementation is bi-directional.
 */
void B_output(struct msg message)  {

}

/* 
 * A_input(packet), where packet is a structure of type pkt. This routine 
 * will be called whenever a packet sent from the B-side (i.e., as a result 
 * of a tolayer3() being done by a B-side procedure) arrives at the A-side. 
 * packet is the (possibly corrupted) packet sent from the B-side.
 */
void A_input(struct pkt packet) {
  
  int ans = packet.acknum;
  stopTimer(AEntity);
  
  // the response is either NACK or corrupted
  if (ans == NACK || corrupt(packet)) {
    if (curr_package)
    {
      resendLast(AEntity);
    } 
  }
  else if (ans == ACK) { // recived an ACK, proceed with next message
    state = SEND_MESSAGE;
    free(curr_package);
    //curr_package = NULL;
    curr_sequenceA = switchSeq(curr_sequenceA);
    struct msg *incoming = nextInLine();
    sendMessage(AEntity, incoming);
    free(incoming);
  }
  
}

// returns true if packet is corrupt, returns false if packet is not corrupt
int corrupt(struct pkt packet){
  int checksum = calculateChecksum(packet.payload, packet.acknum, packet.seqnum);
  if(packet.checksum == checksum) {
    return FALSE;
  }
  return TRUE;
}

// retransmits a packet if there was a NACK, corruption, or timeout
void resendLast(int entity)
{
  if (curr_package)
  {
    tolayer3(entity, *curr_package);
    state = WAIT;
    startTimer(entity, MESSAGE_TIMEOUT);
  }
}

/*
 * A_timerinterrupt()  This routine will be called when A's timer expires 
 * (thus generating a timer interrupt). You'll probably want to use this 
 * routine to control the retransmission of packets. See starttimer() 
 * and stoptimer() in the writeup for how the timer is started and stopped.
 */
void A_timerinterrupt() {
  //printf("--------PACKET WAS LOST---------SENDING ANOTHER\n");
  resendLast(AEntity);
  startTimer(AEntity, MESSAGE_TIMEOUT);
}  

/* The following routine will be called once (only) before any other    */
/* entity A routines are called. You can use it to do any initialization */
void A_init() {
  state = SEND_MESSAGE;
  curr_sequenceA = SEQ0;
}


/* 
 * Note that with simplex transfer from A-to-B, there is no routine  B_output() 
 */

/*
 * B_input(packet),where packet is a structure of type pkt. This routine 
 * will be called whenever a packet sent from the A-side (i.e., as a result 
 * of a tolayer3() being done by a A-side procedure) arrives at the B-side. 
 * packet is the (possibly corrupted) packet sent from the A-side.
 */
void B_input(struct pkt packet) {


  // make sure incoming packet is not corrupt
  if (!corrupt(packet)) {
      struct msg recvData;
      strncpy(recvData.data, packet.payload, MESSAGE_LENGTH);
      if (packet.seqnum == curr_sequenceB) { //make sure there is no duplicate before sending to upper layer
        sendACK();
        tolayer5(BEntity, recvData);
        curr_sequenceB = switchSeq(curr_sequenceB);
      } else {
        sendACK();
      }     
  } else {
    sendNACK();
  }
}

// creates and sends a ACK message to A entity only
void sendACK() {
  struct pkt* ack = makePacket(curr_sequenceB, ACK, calculateChecksum(empty_msg, ACK, curr_sequenceB), empty_msg);
  tolayer3(BEntity, *ack); 
}

//creates and sends a NACK message to A entity only
void sendNACK() {
  struct pkt* nack = makePacket(curr_sequenceB, NACK, calculateChecksum(empty_msg, NACK, curr_sequenceB), empty_msg);
  tolayer3(BEntity, *nack);
}

/*
 * B_timerinterrupt()  This routine will be called when B's timer expires 
 * (thus generating a timer interrupt). You'll probably want to use this 
 * routine to control the retransmission of packets. See starttimer() 
 * and stoptimer() in the writeup for how the timer is started and stopped.
 */
void  B_timerinterrupt() {
// not used
}

/* 
 * The following routine will be called once (only) before any other   
 * entity B routines are called. You can use it to do any initialization 
 */
void B_init() {
  curr_sequenceB = SEQ0; //making B side sequence 0 to match A
}

// addToQueue takes a message struct and adds it to the head of the linked list
void addToQueue(struct msg m) {
  struct queue *new_message = malloc(sizeof(struct queue));
  memcpy(&new_message->message, &m, sizeof(struct msg));
  new_message->next = NULL;
  if (head == NULL) {
    head = new_message;
    tail = new_message;
    
  } else {
    tail->next = new_message;
    tail = new_message;
  }
}

// nextInLine returns the current head of the linked list and makes the next in line the new head
struct msg* nextInLine(){
  if (head == NULL) {
    return NULL;
  }
  struct queue *old_head = head;
  struct msg *head_message = &old_head->message;
  head = head->next;
  return head_message;
}







