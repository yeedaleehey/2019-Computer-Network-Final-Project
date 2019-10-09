#include <sys/types.h>
#include <sys/wait.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#define MSS 1024
int sockfd;
int BufferSize = 32768;
int now_seq = 0;
int FileNum = 0;
short server_port;
short client_port = 9993;
char server_ip[20];
char buffer[32768];
char packet_buffer[1048];
char FileName[10][20];
struct sockaddr_in server_info, client_info;
socklen_t addrlen = sizeof(client_info);

typedef struct{
    short source_port;
    short dest_port;
    int seq_num;
    int ack_num;
    short head_len:4, not_use:6, u:1, ack:1, p:1, r:1, syn:1, fin:1;
    short recv_window;
    short checksum;
    short urg_data_ptr;
    int option;
    char data[MSS]; // 8 byte
}Packet; //packet size = 24 + sizeof(data)


int send_packet(int sockfd, Packet packet, int data_size){
    char pk[1024];
    int packet_size = 24 + data_size;
    memset(pk, 0, 128 * sizeof(char));
    memcpy(pk, &packet, packet_size);
    sendto(sockfd, pk, packet_size, 0, (struct sockaddr *)&server_info, sizeof(server_info));
    //printf("%d %d %d", packet.syn, packet.seq_num, packet.ack_num);
    return packet_size;
}

int receive_packet(Packet *packet){
    int packet_len = recvfrom(sockfd, packet_buffer, 10240, 0, (struct sockaddr *)&server_info, &addrlen);
    if(packet_len == -1) return 0; //timeout
    memcpy(packet, packet_buffer, packet_len);
    return packet_len;
}

int three_way_handshake(){
    //sent syn
    int packet_len = 0;
    Packet packet, rcv_packet;
    memset(&packet, 0, sizeof(packet));
    packet.syn = 1;
    packet.source_port = client_port;
    packet.dest_port = server_port;
    packet.seq_num = now_seq = (short)(rand()%10000)+1;
    send_packet(sockfd, packet, 0);
    printf("Send a packet(SYN) to %s : %d\n",inet_ntoa(server_info.sin_addr), ntohs(server_info.sin_port));
    //receive syn/ack
    packet_len = receive_packet(&rcv_packet);
    if(rcv_packet.syn && rcv_packet.ack && rcv_packet.ack_num == now_seq + 1){
        printf("Receive a packet(SYN) from %s : %hu\n", inet_ntoa(server_info.sin_addr), ntohs(server_info.sin_port));
        now_seq += 1;
        server_port = rcv_packet.source_port;

        //create a socket
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if(sockfd == -1) printf("Fail to create a socket.\n");
        //set server info
        bzero(&server_info, sizeof(server_info)); //set struct content to zero
        server_info.sin_family = PF_INET; //IPv4
        server_info.sin_addr.s_addr = inet_addr(server_ip);
        server_info.sin_port = htons(server_port);

    }else{
        printf("handshake erorr.\n");
        return -1;
    }
    printf("\tReceive a packet (seq_num = %d, ack_num = %d)\n", rcv_packet.seq_num, rcv_packet.ack_num);
    //send ack
    memset(&packet, 0, sizeof(packet));
    packet.source_port = client_port;
    packet.dest_port = server_port;
    packet.ack = 1;
    packet.seq_num = now_seq;
    packet.ack_num = rcv_packet.seq_num + 1;
    send_packet(sockfd, packet, 0);
    printf("Send a packet(ACK) to %s : %d\n",inet_ntoa(server_info.sin_addr), ntohs(server_info.sin_port));
    printf("=====Complete the three-way handshake=====\n");
    return 1;
}

int receiving_file(char *filename){
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000; // 500 ms
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    FILE *fp = fopen(filename, "w");
    Packet data_packet, ack_packet;
    memset(&data_packet, 0, sizeof(data_packet));
    memset(&ack_packet, 0, sizeof(ack_packet));
    int seq_num = rand()%10000 + 1;
    int rcv_seq = 1;
    int recv_count = 0;
    //send file name
    strcpy(data_packet.data, filename);
    send_packet(sockfd, data_packet, strlen(filename) * sizeof(char));
    //receive file
    printf("Receive %s from %s : %d\n", filename, inet_ntoa(server_info.sin_addr), ntohs(server_info.sin_port));
    while(1){
        int data_len = receive_packet(&data_packet) - 24;
        if(data_len < 0){
            //0 - 24 = -24, timeout
            printf("***** Timeout !! *****\n");
            ack_packet.ack = 1;
            ack_packet.recv_window = BufferSize;
            ack_packet.source_port = client_port;
            ack_packet.dest_port = server_port;
            ack_packet.seq_num = data_packet.ack_num;
            ack_packet.ack_num = rcv_seq;
            send_packet(sockfd, ack_packet, 0);
            recv_count = 0;
        }else if(data_len == 0 && data_packet.fin){
            fwrite(buffer, sizeof(char), (32768 - BufferSize), fp);
            BufferSize = 32768;
            break;
        }else{
            //received packet
            recv_count++;
            if(rcv_seq == -1) rcv_seq = data_packet.seq_num ;
            if(data_packet.seq_num == rcv_seq){ //only write continuous packet
                //printf("buffer size: %d ", BufferSize);
                if(data_len >= BufferSize){
                    //printf("buffer full ");
                    //buffer full, flush
                    fwrite(buffer, sizeof(char), (32768 - BufferSize), fp);
                    BufferSize = 32768;
                }
                memcpy(buffer + (32768 - BufferSize), data_packet.data, data_len);
                BufferSize -= data_len;
                rcv_seq += data_len;
                if(recv_count == 2){
                    ack_packet.ack = 1;
                    ack_packet.recv_window = BufferSize;
                    ack_packet.source_port = client_port;
                    ack_packet.dest_port = server_port;
                    ack_packet.seq_num = data_packet.ack_num;
                    ack_packet.ack_num = rcv_seq;
                    send_packet(sockfd, ack_packet, 0);
                    recv_count = 0;
                }
            }
            printf("\tReceive a packet (seq_num = %d, ack_num = %d)\n", data_packet.seq_num, data_packet.ack_num);
        }
    }
    return 1;
}

int myconnect(char *server_ip, int port){
    //create a socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd == -1) printf("Fail to create a socket.\n");
    //set server info
    bzero(&server_info, sizeof(server_info)); //set struct content to zero
    server_info.sin_family = PF_INET; //IPv4
    server_info.sin_addr.s_addr = inet_addr(server_ip);
    server_info.sin_port = htons(port);
    //------start three-way handshake------
    three_way_handshake();
    //------receive file 1-------
    for(int i = 0 ; i < FileNum; i++){
        receiving_file(FileName[i]);
    }
    Packet fin;
    memset(&fin, 0, sizeof(fin));
    fin.source_port = client_port;
    fin.dest_port = server_port;
    fin.fin = 1;
    send_packet(sockfd, fin, 0);
    return sockfd;
}

int main(int argc, char *argv[]){
    srand(time(NULL));
    strcpy(server_ip, argv[1]);
    server_port = atoi(argv[2]);
    FileNum = argc - 4;
    for(int i = 0; i < FileNum ; i++)strcpy(FileName[i], argv[i+4]);
    sockfd = myconnect(server_ip, server_port);
    return 0;
}
